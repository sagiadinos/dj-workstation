// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "library/library_service.hpp"

#include "analysis/track_analysis.hpp"
#include "library/missing_files.hpp"
#include "library/playlist_files.hpp"
#include "library/utf8_path.hpp"

#include <algorithm>
#include <system_error>
#include <utility>

namespace djw::core::library
{
    std::expected<LibraryService::Connection, std::string>
    LibraryService::connect(const std::filesystem::path &database_file)
    {
        auto tracks = TrackStore::open(database_file);
        if (!tracks)
        {
            return std::unexpected{tracks.error()};
        }
        // The playlists use the track store's connection, which stays in place when the store moves
        auto playlists = PlaylistStore::open(tracks->connection());
        if (!playlists)
        {
            return std::unexpected{playlists.error()};
        }
        return Connection{.tracks = std::move(*tracks), .playlists = std::move(*playlists)};
    }

    std::expected<std::unique_ptr<LibraryService>, std::string>
    LibraryService::open(const std::filesystem::path &database_file, TagReader read_tags, TagWriter write_tags)
    {
        auto reader = connect(database_file);
        if (!reader)
        {
            return std::unexpected{reader.error()};
        }
        auto writer = connect(database_file);
        if (!writer)
        {
            return std::unexpected{writer.error()};
        }
        return std::unique_ptr<LibraryService>{
            new LibraryService{std::move(*reader), std::move(*writer), std::move(read_tags), std::move(write_tags)}};
    }

    LibraryService::LibraryService(Connection reader, Connection writer, TagReader read_tags, TagWriter write_tags)
        : reader{std::move(reader)}, writer{std::move(writer)}, read_tags{std::move(read_tags)},
          write_tags{std::move(write_tags)}
    {
        tracks_total  = this->reader.tracks.trackCount().value_or(0);
        missing_total = this->reader.tracks.missingCount().value_or(0);
        thread          = std::thread{[this] { run(); }};
        analysis_thread = std::thread{[this] { analysisLoop(); }};
    }

    LibraryService::~LibraryService()
    {
        stopping.store(true);
        {
            const std::scoped_lock lock{mutex};
            scan_requested = false;
        }
        wake.notify_all();
        {
            // Lock and unlock, so an analysis thread between its check and its wait cannot miss the notification
            const std::scoped_lock lock{analysis_mutex};
        }
        analysis_wake.notify_all();
        analysis_thread.join(); // before the library thread: its results are jobs for that thread
        thread.join();
    }

    void LibraryService::queue(Job job)
    {
        {
            const std::scoped_lock lock{mutex};
            jobs.push_back(std::move(job));
        }
        wake.notify_all();
    }

    std::expected<void, std::string> LibraryService::addFolder(const std::filesystem::path &folder)
    {
        std::error_code error;
        const auto      absolute = std::filesystem::canonical(folder, error);
        if (error || !std::filesystem::is_directory(absolute, error))
        {
            return std::unexpected{"not a folder: " + toUtf8(folder)};
        }
        if (auto added = reader.tracks.addFolder(absolute); !added)
        {
            return added;
        }
        rescan();
        return {};
    }

    void LibraryService::rescan()
    {
        {
            const std::scoped_lock lock{mutex};
            scan_requested = true;
        }
        wake.notify_all();
    }

    void LibraryService::updateTags(std::int64_t track_id, api::LibraryTagChange change)
    {
        queue([this, track_id, change = std::move(change)] { return writeTags(track_id, change); });
    }

    void LibraryService::setCuePoint(std::int64_t track_id, std::optional<double> seconds)
    {
        queue([this, track_id, seconds] { return writer.tracks.setCue(track_id, seconds); });
    }

    void LibraryService::requestAnalysis(AnalysisRequest request, bool in_front)
    {
        {
            const std::scoped_lock lock{analysis_mutex};
            const auto             queued = std::ranges::find(analysis_queue, request.track_id, &AnalysisRequest::track_id);
            if (queued != analysis_queue.end())
            {
                if (in_front && queued != analysis_queue.begin())
                {
                    std::rotate(analysis_queue.begin(), queued, queued + 1);
                }
                return;
            }
            if (analysis_running == request.track_id)
            {
                return;
            }
            if (in_front)
            {
                analysis_queue.push_front(std::move(request));
            }
            else
            {
                analysis_queue.push_back(std::move(request));
            }
        }
        analysis_wake.notify_all();
    }

    std::size_t LibraryService::analysisPending() const
    {
        const std::scoped_lock lock{analysis_mutex};
        return analysis_queue.size() + (analysis_running != 0 ? 1 : 0);
    }

    void LibraryService::analyze(std::int64_t track_id)
    {
        const auto analyzed = reader.tracks.isAnalyzed(track_id);
        if (!analyzed || *analyzed)
        {
            return;
        }
        const auto path = reader.tracks.trackPath(track_id);
        if (path && *path)
        {
            requestAnalysis({track_id, **path}, true);
        }
    }

    void LibraryService::analyzeAll()
    {
        const auto files = reader.tracks.unanalyzed();
        if (!files)
        {
            return;
        }
        for (const StoredFile &file : *files)
        {
            requestAnalysis({file.track_id, file.path}, false);
        }
    }

    void LibraryService::rescanTrack(std::int64_t track_id)
    {
        queue(
            [this, track_id]() -> std::expected<void, std::string>
            {
                const auto path = writer.tracks.trackPath(track_id);
                if (!path)
                {
                    return std::unexpected{path.error()};
                }
                if (!*path)
                {
                    return std::unexpected{"cannot scan again: track " + std::to_string(track_id) +
                                           " is no longer in the library"};
                }
                const auto stamp = fileStamp(**path);
                if (!stamp)
                {
                    return std::unexpected{stamp.error()};
                }
                if (auto stored = writer.tracks.store(**path, *stamp, read_tags(**path)); !stored)
                {
                    return stored;
                }
                requestAnalysis({track_id, **path}, true);
                return {};
            });
    }

    std::vector<std::uint8_t> LibraryService::waveform(std::int64_t track_id) const
    {
        return reader.tracks.waveform(track_id).value_or(std::vector<std::uint8_t>{});
    }

    void LibraryService::analysisLoop() // analysis thread
    {
        while (true)
        {
            AnalysisRequest request;
            {
                std::unique_lock lock{analysis_mutex};
                analysis_wake.wait(lock, [this] { return !analysis_queue.empty() || stopping.load(); });
                if (stopping.load())
                {
                    return;
                }
                request          = std::move(analysis_queue.front());
                analysis_queue.pop_front();
                analysis_running = request.track_id;
            }
            // 48 kHz: the rate the engine plays at; the analysis does not depend on it
            auto analysis = analysis::analyzeFile(request.path, 48'000, stopping);
            if (analysis)
            {
                queue([this, id = request.track_id, result = std::move(*analysis)]
                      { return writer.tracks.setAnalysis(id, result.bpm, result.waveform); });
            }
            else if (!stopping.load())
            {
                // A file that cannot be decoded is not tried again and again: marked analyzed, without tempo
                queue([this, id = request.track_id] { return writer.tracks.setAnalysis(id, std::nullopt, {}); });
            }
            const std::scoped_lock lock{analysis_mutex};
            analysis_running = 0;
        }
    }

    void LibraryService::removeMissing()
    {
        queue([this] { return writer.tracks.removeMissing(); });
    }

    std::vector<std::filesystem::path> LibraryService::folders() const
    {
        return reader.tracks.folders().value_or(std::vector<std::filesystem::path>{});
    }

    api::LibraryStatus LibraryService::status() const
    {
        const std::scoped_lock lock{mutex};
        return {
            .scanning       = scanning || scan_requested,
            .files_found    = counters.files_found.load(std::memory_order_relaxed),
            .files_updated  = counters.files_updated.load(std::memory_order_relaxed),
            .tracks_total   = tracks_total,
            .missing_total  = missing_total,
            .pending_writes = jobs.size() + jobs_running,
            .analysis_pending = analysisPending(),
            .revision       = revision,
            .scan_error     = scan_error,
            .write_error    = write_error,
            .last_import    = last_import,
        };
    }

    std::expected<std::vector<api::LibraryTrack>, std::string>
    LibraryService::search(const api::LibraryQuery &query) const
    {
        return reader.tracks.search(query);
    }

    std::expected<std::vector<api::Playlist>, std::string> LibraryService::list() const
    {
        return reader.playlists.list();
    }

    std::expected<std::vector<api::LibraryTrack>, std::string> LibraryService::tracks(std::int64_t playlist_id) const
    {
        return reader.playlists.tracks(playlist_id);
    }

    void LibraryService::create(std::string name)
    {
        queue(
            [this, name = std::move(name)]() -> std::expected<void, std::string>
            {
                const auto taken = writer.playlists.idOf(name);
                if (!taken)
                {
                    return std::unexpected{taken.error()};
                }
                if (*taken)
                {
                    return std::unexpected{"a playlist named " + name + " exists already"};
                }
                const auto created = writer.playlists.create(name);
                if (!created)
                {
                    return std::unexpected{created.error()};
                }
                return {};
            });
    }

    void LibraryService::rename(std::int64_t playlist_id, std::string name)
    {
        queue(
            [this, playlist_id, name = std::move(name)]() -> std::expected<void, std::string>
            {
                const auto taken = writer.playlists.idOf(name);
                if (!taken)
                {
                    return std::unexpected{taken.error()};
                }
                if (*taken && **taken != playlist_id)
                {
                    return std::unexpected{"a playlist named " + name + " exists already"};
                }
                return writer.playlists.rename(playlist_id, name);
            });
    }

    void LibraryService::remove(std::int64_t playlist_id)
    {
        queue([this, playlist_id] { return writer.playlists.remove(playlist_id); });
    }

    void LibraryService::changeEntries(std::int64_t                                     playlist_id,
                                       std::function<bool(std::vector<std::int64_t> &)> change)
    {
        queue(
            [this, playlist_id, change = std::move(change)]() -> std::expected<void, std::string>
            {
                auto entries = writer.playlists.entries(playlist_id);
                if (!entries)
                {
                    return std::unexpected{entries.error()};
                }
                if (!change(*entries))
                {
                    return {}; // nothing to change, e.g. an index past the end
                }
                return writer.playlists.setEntries(playlist_id, *entries);
            });
    }

    void LibraryService::add(std::int64_t playlist_id, std::vector<std::int64_t> track_ids)
    {
        changeEntries(playlist_id,
                      [track_ids = std::move(track_ids)](std::vector<std::int64_t> &entries)
                      {
                          entries.insert(entries.end(), track_ids.begin(), track_ids.end());
                          return !track_ids.empty();
                      });
    }

    void LibraryService::removeEntry(std::int64_t playlist_id, std::size_t index)
    {
        changeEntries(playlist_id,
                      [index](std::vector<std::int64_t> &entries)
                      {
                          if (index >= entries.size())
                          {
                              return false;
                          }
                          entries.erase(entries.begin() + static_cast<std::ptrdiff_t>(index));
                          return true;
                      });
    }

    void LibraryService::moveEntry(std::int64_t playlist_id, std::size_t from, std::size_t to)
    {
        changeEntries(playlist_id,
                      [from, to](std::vector<std::int64_t> &entries)
                      {
                          if (from >= entries.size() || to >= entries.size() || from == to)
                          {
                              return false;
                          }
                          const auto first = entries.begin();
                          if (from < to)
                          {
                              std::rotate(first + static_cast<std::ptrdiff_t>(from),
                                          first + static_cast<std::ptrdiff_t>(from) + 1,
                                          first + static_cast<std::ptrdiff_t>(to) + 1);
                          }
                          else
                          {
                              std::rotate(first + static_cast<std::ptrdiff_t>(to),
                                          first + static_cast<std::ptrdiff_t>(from),
                                          first + static_cast<std::ptrdiff_t>(from) + 1);
                          }
                          return true;
                      });
    }

    void LibraryService::importM3u(std::filesystem::path file)
    {
        queue(
            [this, file = std::move(file)]() -> std::expected<void, std::string>
            {
                auto imported = importM3uFile(file, writer.tracks, writer.playlists, read_tags);
                if (!imported)
                {
                    return std::unexpected{imported.error()};
                }
                const std::scoped_lock lock{mutex};
                last_import = std::move(*imported);
                return {};
            });
    }

    void LibraryService::exportM3u(std::int64_t playlist_id, std::filesystem::path file)
    {
        queue([this, playlist_id, file = std::move(file)]
              { return exportM3uFile(playlist_id, file, writer.playlists); });
    }

    void LibraryService::run() // library thread
    {
        while (true)
        {
            bool scan_now = false;
            {
                std::unique_lock lock{mutex};
                wake.wait(lock, [this] { return scan_requested || !jobs.empty() || stopping.load(); });
                scan_now       = scan_requested && !stopping.load();
                scan_requested = false;
                if (scan_now)
                {
                    scanning = true;
                    scan_error.clear();
                }
            }
            runJobs();
            if (stopping.load())
            {
                return; // the jobs queued until now are done, a scan is not started any more
            }
            if (scan_now)
            {
                scan();
            }
        }
    }

    void LibraryService::scan() // library thread
    {
        counters.files_found.store(0);
        counters.files_updated.store(0);

        std::string error;
        if (auto folders = writer.tracks.folders(); !folders)
        {
            error = folders.error();
        }
        else
        {
            error =
                scanFolders(*folders, writer.tracks, read_tags, counters, stopping, [this] { runJobsDuringScan(); });
        }
        if (std::string missing_error = checkMissingFiles(writer.tracks, stopping); error.empty())
        {
            error = std::move(missing_error);
        }
        updateCounts();

        const std::scoped_lock lock{mutex};
        scanning   = false;
        scan_error = std::move(error);
        ++revision;
    }

    void LibraryService::updateCounts() // library thread
    {
        const auto tracks  = writer.tracks.trackCount();
        const auto missing = writer.tracks.missingCount();

        const std::scoped_lock lock{mutex};
        tracks_total  = tracks.value_or(tracks_total);
        missing_total = missing.value_or(missing_total);
    }

    void LibraryService::runJobsDuringScan() // library thread
    {
        {
            const std::scoped_lock lock{mutex};
            if (jobs.empty())
            {
                return;
            }
        }
        // Committed first, so a change is visible to readers as soon as the revision says so
        auto committed = writer.tracks.commitTransaction();
        runJobs();
        if (committed)
        {
            committed = writer.tracks.beginTransaction();
        }
        if (!committed)
        {
            const std::scoped_lock lock{mutex};
            write_error = committed.error();
        }
    }

    void LibraryService::runJobs() // library thread
    {
        while (true)
        {
            Job job;
            {
                const std::scoped_lock lock{mutex};
                if (jobs.empty())
                {
                    return;
                }
                job = std::move(jobs.front());
                jobs.pop_front();
                jobs_running = 1;
            }
            const auto done = job();
            if (done)
            {
                updateCounts();
            }

            const std::scoped_lock lock{mutex};
            jobs_running = 0;
            if (done)
            {
                write_error.clear();
                ++revision;
            }
            else
            {
                write_error = done.error();
            }
        }
    }

    std::expected<void, std::string> LibraryService::writeTags(std::int64_t                 track_id,
                                                               const api::LibraryTagChange &change) // library thread
    {
        const auto path = writer.tracks.trackPath(track_id);
        if (!path)
        {
            return std::unexpected{path.error()};
        }
        if (!*path)
        {
            return std::unexpected{"cannot write tags: track " + std::to_string(track_id) +
                                   " is no longer in the library"};
        }
        if (auto written = write_tags(**path, change); !written)
        {
            return written;
        }
        // Read back, so the database holds what is in the file now, with the new stamp of the changed file
        const auto stamp = fileStamp(**path);
        if (!stamp)
        {
            return std::unexpected{stamp.error()};
        }
        // Only tags changed, the audio and so its analysis stay
        return writer.tracks.store(**path, *stamp, read_tags(**path), true);
    }
} // namespace djw::core::library
