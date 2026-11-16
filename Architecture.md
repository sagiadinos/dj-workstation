# Architecture and Development Guidelines

Rules for everyone working on this repository.

## Project

djw (DJ-Workstation) is a lean, touch-first DJ application in C++23. It is a complete rewrite of the original Win32 DJ-Workstation, developed from 1998 to 2002. No code from the original is reused.

**`FEATURES.md` is the specification.** Read it before starting any task. Do not implement features that are not listed there, and never implement anything from its non-goals list. If a task seems to require something outside the spec, ask first.

**What comes next is decided by the user.** FEATURES.md marks every feature as done or planned; there are no milestones. Do not start a planned feature unless asked.

## Hard rules

### Architecture
- `core/` must never include anything from `player-ui/` or SDL. The core builds and runs headless.
- The frontends in `player-ui/` talk to the core only through `core/api/`: setup (adding decks, starting audio) through the `api::Core` interface, while running only through the command queue and state snapshots. The music library through the `api::Library` interface (`api::openLibrary`), its saved playlists through `api::Playlists` (`Library::playlists()`). No direct calls into engine or library internals.
- Platform-specific code stays behind an interface (e.g. `audio_io::AudioDevice`, with miniaudio as the implementation), never scattered with `#ifdef` across the core.

### Audio thread
The audio callback and everything it calls must be real-time safe:
- no heap allocation or deallocation (`new`, `delete`, `malloc`, growing `std::vector`, `std::string`, `std::function` with captures)
- no locks (`std::mutex`, condition variables), only atomics and lock-free queues
- no I/O, no logging, no syscalls
- no exceptions
- bounded execution time

Mark audio-thread functions with a comment `// [audio-thread]` so they are easy to audit. When in doubt whether something is RT-safe, it is not.

### Track loading
- A loaded track is decoded completely into memory as interleaved stereo float32 at the engine sample rate, so cue, hot cues, seeking and reverse playback (scratch) work at any position without latency. The waveform overview uses the same samples.
- Decoding runs on the decoder thread in chunks of a few seconds, from the start to the end. Playback can start as soon as the first chunk is ready. A seek into a part that is not decoded yet makes the decoder decode that part first; only then a short gap is possible.
- The audio thread reads the decoded chunks directly. Chunks are published to it lock-free and stay unchanged while the track is loaded; they are freed outside the audio thread after the track is unloaded.
- Memory per track: about 23 MB per minute at 48 kHz (a 5-minute track about 115 MB).
- The size held in memory has a configurable limit, as in VirtualDJ. Tracks above it are streamed from disk through a ring buffer (`decode::StreamingSource`); every seek there (also a jump on the waveform, turning keylock on or off, the scrub loop) has a gap of some 50 ms while the stream refills, and reverse playback is limited to what is buffered. The default limit is 10 minutes of track length (about 230 MB), so DJ tracks including most extended versions are held in memory; longer ones (mixes) are streamed. If the memory cannot be had the track is streamed as well.
- Implemented in `decode::MemoryTrack` and `decode::StreamingSource`; `decode::makeTrack` chooses between them.
- While a track in memory is decoded, the decoder thread also fills its fine waveform (`api::TrackPeaks`: the peak of every 256 frames in three bands, bass below 200 Hz, mids 300 to 2500 Hz, highs above 4 kHz, each 0 to 255) for the beat waveforms; the frontend gets it with `Core::trackPeaks`. A streamed track has none.

### Dependencies
- Only use libraries listed in FEATURES.md section 6.
- Ask before adding any new dependency.
- Dependencies are declared via CMake `FetchContent` with pinned versions (tag or commit hash) and `FIND_PACKAGE_ARGS`: an installed system or vcpkg package is used if found, otherwise the pinned version is downloaded. Exception: libraries without a CMake package config (miniaudio, SQLite) are always downloaded and built from their sources by our own CMake target; a download by URL is pinned by its SHA-256 hash. Single-header libraries and assets such as the UI font (`third_party/fonts/`) may be vendored in `third_party/` with their license file.
- Every dependency must be compatible with AGPLv3.

### Licensing
- Project license: AGPL-3.0-or-later.
- Every new source file starts with:
  ```cpp
  // SPDX-License-Identifier: AGPL-3.0-or-later
  // Copyright (C) 2026 Niko Sagiadinos
  ```
- Files taken from third parties (e.g. xwax timecoder) keep their original headers and license unchanged.

## Design principles

- Single responsibility: one class has one reason to change. If a description needs "and", split it.
- Dependencies on the outside world (clock, audio device, file system, MIDI, database) are interfaces passed in through the constructor. This keeps modules testable with fakes and replaceable per platform.
- Depend on interfaces, not on concrete implementations, across module boundaries. Inside a module, concrete types are fine.
- Composition over inheritance. Inheritance only for implementing an interface.
- Keep interfaces small. A consumer must not depend on methods it does not use.
- No duplicated logic: extract it when it appears a second time. Code that only looks alike but changes for different reasons stays separate.
- Exception: the audio path. Real-time safety and performance come before abstraction there, so no virtual calls per sample and no indirection inside inner loops.

## Engine and audio

- Layers: `audio_io` (the device), `engine` (decks and mixer), `decode` (tracks, see Track loading), `runtime` (`RuntimeCore`, the implementation of `api::Core`, wires them together). The frontend only sees `api::Core`.
- Threads:
  - Audio thread: the device calls `Engine::render` through `AudioCallback`. Everything there is marked `// [audio-thread]`.
  - Control thread: the frontend's thread, calling `api::Core` (add or remove decks, load tracks). It publishes a new deck list or track to the audio thread through an atomic pointer; the audio thread picks it up at the start of its next render.
  - Decoder thread: decodes the loaded tracks, see Track loading.
- Commands go in through one SPSC queue per producer (`Engine::addCommandQueue`, only before rendering starts); `render` takes at most one queue capacity per call, so a producer cannot stall it. At present the frontend is the only producer: controller events are carried out by the frontend like its own buttons. State comes out once per render through a triple buffer per deck (`api::DeckSnapshot`) and one for the mixer (`api::MixerSnapshot`); decks have no fixed upper limit.
- Freeing: the audio thread never frees anything and never waits. What it no longer sees (an old deck list, an unloaded track, a removed deck) is retired to the `Reclaimer` and freed by the control thread once a later render has started; `RuntimeCore` does that on every call.
- Signal path per render, in blocks of at most `max_block_frames`:
  1. `Deck`: transport (play, pause with a short fade, CUE like a CDJ, seek, scrub loop, scratch). A scratch reads the track at the hand's speed itself, backwards too, with the same Hermite interpolation, in blocks of 256 frames from a window allocated with the deck; it needs a source that can be read anywhere at once (`AudioSource::randomAccess`, a `MemoryTrack`), so streamed tracks cannot be scratched. The track goes through the `Resampler` (varispeed, 4-point Hermite, bit-exact at speed 1.0) or with keylock through a `TimeStretcher`. That is an interface with one implementation per library (SoundTouch, Signalsmith Stretch), made by `makeTimeStretcher` from `CoreConfig::time_stretch` (Settings > Sound, keylock quality Standard or High); Signalsmith Stretch is built in with `DJW_WITH_SIGNALSMITH_STRETCH` (on by default), SoundTouch always. Each sizes its buffers in its constructor so it does not allocate while rendering, and reports its position from what it was fed minus what it still holds; the stretcher tests run for every library built in.
  2. `MixerChannel`: meters the deck before the fader, applies channel volume and the crossfader gain of its side, adds the signal to the master sum and, with headphone cue on, before the fader to the cue sum. Fader changes are ramped over one block (`GainRamp`), so they do not click.
  3. `Crossfader`: constant power curve (-3 dB per side at the center), momentary A/B acts as the end of a side without moving the fader.
  4. `MasterBus`: master volume, `Limiter` (no look-ahead, so no added latency; the output never goes beyond full scale), master meter.
  5. `HeadphoneBus` (only with 4 or more output channels): blends the cue sum with the master, headphone volume, its own limiter.
  6. `routeOutput`: master on channels 1/2, headphones on 3/4, silence on any further channel.
- Meters (`PeakMeter`): a new peak shows at once and falls back by 20 dB in 1.7 s, so a frontend that reads less often than blocks come misses none.
- Device (`MiniaudioDevice`): the system's output or the first whose name contains the configured text, at its own sample rate; miniaudio converts to the hardware's format. With headphones it is opened with its own channel count, a device with fewer than 4 channels falls back to stereo (`MixerSnapshot::headphones_available`). A Null backend runs in real time without sound, for tests and headless runs. The engine also renders without a device, which the offline render tests use.

## Player windows

- A view (`player-ui/sdl/view.hpp`: player, library, crossfader, playlist) lays itself out in the rectangle it is given and never assumes it starts at the window's top left corner. Tests check this for every view (`test_view_host.cpp`).
- A window is a host: `SdlWindow` shows the views of its `ViewHost`, each in its own area, without overlap. Pointer input goes to the widgets of all its views, the wheel and the right mouse button to the view under the pointer, typed text to the view a pointer last went down on.
- A widget may carry a short help (`Widget::setTip`): `ViewHost` shows it in a bubble near the pointer once the mouse has rested on the widget for half a second, and hides it on a press or when the mouse leaves the window. Touch has no resting pointer, so there the help never shows; a widget must make sense without it.
- Two window modes, switched with View > One-window mode and taking effect at the next start; the layout file keeps the mode and a layout for each mode. Without a saved layout the program starts in the one window:
  - Separate windows: every element has its own window (one view filling its host).
  - One window (MDI): one host on the main display, fullscreen, maximized or a window of its own size, with title bar and close button (View > Fullscreen, Ctrl+F); it opens as the layout saved it, and without a saved layout as a window 927 points high (big enough for a library whose menus open in full) and as wide as the row of players needs when it takes half of that height (`defaultHostSize`). Every element is a part of it in a `PaneFrame` (title bar, close button for players, crossfader, playlist and beat waveforms, a discreet border of 1 point so neighbouring parts can be told apart). `arrangeMdi` places the parts automatically and without overlap: players in a row with the crossfader after player 1, the library below, the playlist to its right; with the library below, the row takes at most half of the window (`mdi_row_share`), the library the other half. Horizontal beat waveforms are a strip across the top above the players (at most a third of the window); vertical ones stand between player 1 and player 2 above the crossfader, the two together as high as a player, the crossfader in its own height flush with the bottom of the players; the library starts right below the row, as without them. The row shrinks as a whole when it does not fit. The default (no saved layout, and Layout > Reset, the last entry of the Layout menu, set apart by a line) is two players with the vertical beat waveforms between them and the crossfader below, the playlist closed (`defaultMdiLayout`; in separate windows `defaultLayout` puts the windows the same way, the library below the players). Reset keeps the players open and the window as it is; in the one window it drops the places set by hand. Closing the host ends the program.
  - Layout > Edit layout: the parts are locked and dimmed, a guide every 20 points is shown; a part is dragged anywhere to move it or at its lower right corner to resize it. An edge within 12 points of an edge of another part or of the window goes there (touching it or in line with it), otherwise it snaps to a grid of 5 points; always inside the window. The part touched last (at the start the first, Tab for the next) has a thicker frame: the cursor keys move it by 1 point, with Shift by 10, and with Ctrl resize it at its lower right corner, without snapping; a step that would leave the window, overlap another part or go below the smallest size is not taken and at least 120 x 100 points (`ViewHost` arranging, `snapMove`, `snapResize`). A part let go over another goes back. Done or Escape ends it. From the first move on the parts keep their places (`MdiPlaces`, for the window size they were made in and scaled with the window, saved with Layout > Save layout in the `mdi` lines of the layout file); a part opened later takes the first free place on the grid (`freePlace`), smaller if needed; a closed crossfader, playlist or beat waveforms keep their place and gives way if it is taken when it opens again. Layout > Arrange automatically goes back to `arrangeMdi`.
  - The menu bar is in the library, in both modes alike. A title is as wide as its text plus 14 points on each side, at least 64 points so a finger finds it (`MenuBar::min_title_width`); the widths are measured when the bar is drawn. Titles that do not fit together are cut, the widest first, so the short ones stay whole. An entry that is on shows a check mark at its start; an entry can have a line above it that sets it apart (`MenuItem::separated`, File > Quit).
  - The menus, the context menu and the drop-down lists of a part stay within that part.
- Still open for the MDI mode: scrolling when the parts do not fit. Window positions are not used for it, since Wayland gives clients none.
- While a player plays nothing closes that would stop it: neither that player (window or close button) nor the main window (library window, or the one window), and a quit (Alt+F4, Cmd+Q) is ignored (`SdlApp::anyPlaying`). The one way out then is File > Quit, which asks first (`ConfirmDialog`, Cancel the default); the entry shows no key, since the keys do nothing while a track plays.
- Text sizes: a player scales its text with its window, the time on its display fills its field. `SdlText` keeps fonts per text scale (steps of 0.05, up to 8), so the parts of one window can draw at different sizes without rebuilding fonts; the `ViewHost` sets the scale back to 1 before each part.
- Level meters: the core holds PPM-like peaks per deck (before the channel fader) and for the master in the snapshots; `look::meter` draws them as LED segments, in the groove of a player's channel fader and above the crossfader.
- Settings > Devices opens `DevicesDialog`, a modal dialog over the library window (in the MDI mode the one window), added to its `ViewHost` always on top with a widget that swallows every touch outside it. It sets the audio output (Automatic or a device by name), stereo or four channels and the latency, kept in `<pref>/settings`; `main` reads that file and creates the core with it, so these take effect at the next start. It also sets which mapping each controller's MIDI port uses; that applies at once (`api::Controllers::connect`).
- Colors: every widget asks `look::colors()` for its colors when it draws, a `look::Palette` with the roles that differ between the dark look and the daylight look (surfaces, text, fields, lists, menus, frames). View > Daylight colors switches between the two fixed palettes (`look::setDaylight`) for all windows at once. What signals something (play, CUE, meters) and the black displays with what they show keep their colors in both. A test holds every palette to WCAG contrast for text on what it is drawn on.
- Settings > Sound opens `SoundDialog` the same way. It sets the keylock quality, Standard (SoundTouch) or High (Signalsmith Stretch), offering only what is built in, kept in the same file and taking effect at the next start.
- Pitch: ±10 % (`api::max_pitch_percent`, the engine clamps to it) in steps of 0.02 % as on a CDJ (`api::pitch_step_percent`), shown with two decimals. As on a turntable, the pitch fader pulled down is faster (`DeckView::pitchOf`: the bottom, fader value 0, is +10 %; the top −10 %; a detent at 0 so a finger finds it). A controller's `deck.pitch` keeps its meaning, its highest value is the fastest, and sets the pitch exactly, past the detent, so its fine steps count; a fader built the other way round gets `invert = true` in its mapping, as the Starlight's has.
- The beat waveforms (`BeatView`, View > Beat waveforms) show the players on crossfader sides A and B, each in a lane with a fixed playhead in the middle and 6 s of real time (the pitch taken into account), from the core's `TrackPeaks` in three colours. A button in its upper right corner turns the lanes (its picture shows how they will stand); the layout keeps whether they are open, where and how they stand.
- Controllers (`core/control`, behind `api/controllers.hpp`): RtMidi delivers MIDI messages on its own threads; `Controller` turns them into `api::ControlEvent`s through a TOML mapping (`mappings/` next to the program, then `<pref>/mappings`), `ControllerHub` queues them under a lock, and the frontend takes them once per frame and carries them out like its own buttons and faders (`SdlApp::handleControl`). Ports are known by their name without the numbers ALSA adds, so an assignment survives a replug. A mapping may name its controller's sound card (`audio`): with the output on Automatic, `main` connects the controllers first and opens that card with four channels. `importMixxx` turns a Mixxx mapping's direct bindings into ours (its script cannot be taken over); Settings > Devices saves the result in the user's mapping folder and connects again. Lights: once per frame the frontend hands every player's state (`api::DeckLights`, CUE already blinking) to `setLights`; the hub sends a mapping's `[[light]]` messages to the MIDI output of the same name, only when one changes, and switches them off when it closes. Not yet: hotplug, scripts.
- Library list (`TrackList`, `track_columns`): rows 40 points high, the library's text (list, header, search, filters, status) at 0.85 of the common size (`LibraryView::text_scale`), its buttons and menus in the common size. Columns: artist, title, genre, year, BPM and duration, each with a fixed share of the width (25, 26, 15, 10, 14 and 10 %, `columnAreas`); the filter fields, the header and the rows use the same shares, so they line up. The BPM column is a little wider than year and duration for its two decimals ("127.96").
- Player labels: the controls keep the DJ's English words in every language (Load, Time, KEY, CUE, Pitch, BPM, Vol, Head, Cue-Load); their help bubbles are translated (`player.tip.*`), the state line (loaded, playing) as well. The headphone cue button is a gray button with a round amber LED in it, lit while the cue is on (`TextButton::setLed`, `look::led`); the LED is drawn the same in the dark and the daylight look.
- Keyboard: Tab and Shift+Tab go round the search field and the filters of the library; the cursor keys and Page Up/Down move the selection in the library and the playlist (also while a search field has the focus; Up and Down with Shift, Ctrl or both move it a row as well, `TrackList::navigate`), Enter loads it. The player keys all need the left Alt (Option on the Mac; not with Ctrl, the system key or AltGr, `PlayerKey`): Alt+P plays and pauses, Alt+C acts as CUE while held and Alt+L loads (not while it plays); Alt with the cursor keys left and right steps back and ahead by one scrub loop (60 ms), with Shift by 10 seconds, like a touch on the waveform there (a playing deck jumps, a standing one scrubs); Alt with up and down sets the pitch a step of 0.02 % higher or lower, with Shift 0.1 % (held exactly, past the fader's detent at 0, until the fader is touched). Text typed with the left Alt held is dropped (Option+letter types a sign on the Mac). A character typed while the focused view takes no typing, e.g. a player, goes into the library's search, which takes the focus, its window raised (`strayCharacter`, `SdlApp::typedStray`). Keys go to the view a pointer last went down on; in the one window it has a dark blue line around it, just enough to see it.

## Languages

- `player-ui/sdl/i18n`: the texts of the user interface in English, German, French, Spanish, Greek and Russian, compiled into the program as one catalog per language (`catalog_<code>.cpp`): a list of key and message, e.g. `{"menu.file.quit", "Quit"}`. Keys are named after where the text appears.
- Messages use a subset of ICU MessageFormat (`formatMessage`): `{name}` for an argument and `{n, plural, one {# track} other {# tracks}}` with the plural forms of the CLDR rules per language (`pluralCategory`; Russian has one, few and many). The full ICU library is not used.
- `Translator::tr` looks a key up in the current language, then in English; a key found nowhere comes back as the key itself, so the mistake shows. Views ask for their texts on every frame, so a new language takes effect at once.
- The language: Settings > Language, kept in the settings file; without one the first of the system's languages that djw has, else English.
- Tests check every catalog against the English one: the same keys, once each, no others; every message formats with the arguments of the English one; plural messages have the forms their language needs. A new text therefore goes into all six catalogs in the same change.
- A new language: an entry in `Language` and `languages`, its code and name, its plural rule, a catalog file and its case in `catalog()`.

## Settings and layout files

- Two text files in the user's data folder (`SDL_GetPrefPath`, e.g. `~/.local/share/djw/` on Linux), one entry per line, starting with a header with a version (`djw-settings 1`, `djw-layout 2`).
- `settings` (`parseSettings`, `formatSettings`): buffer size, output device, stereo or master and headphones, the mapping per controller port, the language, the keylock quality (High by default), the daylight colors. Written by Settings > Devices, Settings > Sound, Settings > Language and View > Daylight colors. Read once by `main` before the core is created, so the device settings take effect at the next start. What cannot be read (no file, an unknown line, e.g. from a later version, a buffer size not offered) keeps its default, so the program always starts.
- `layout` (`parseLayout`, `formatLayout`): the window mode for the next start, then the separate windows (`player`, `library`, `mixer`, `playlist` lines, and `beats` with the beat waveforms' orientation) and the one window (`mdi` lines: players with their crossfader side and options such as Cue-Load and remaining time, fullscreen, window size, which tools are open (`mixer`, `playlist`, `beats`), the hand-arranged places with the window size they were made for). Each mode keeps its own layout. Written by Layout > Save layout and by switching the window mode (then only the mode changes). Named layouts are files of the same format (`.djwlayout`), by default in `layouts/` next to `layout`: Layout > Save layout as… writes the current layout there, Layout > Load layout… reads one and writes it as `layout`, so the next start opens with it; it is shown at once when it is for the current window mode, otherwise from the next start. A file that cannot be read as a whole (other version, damaged, sizes that make no sense) is ignored and the default layout used, so a broken file never opens odd windows; a version 1 file is read as the separate windows; a file without `beats` lines (from before the beat waveforms) has them closed and horizontal.

## Library

- `core/library` behind `api/library.hpp` and `api/playlists.hpp`, opened with `api::openLibrary`. One SQLite file, `library.db` in the user's data folder (e.g. `~/.local/share/djw/library.db`).
- Threads (`LibraryService`): two connections to the file in WAL mode, so reading never waits for writing. The frontend's thread reads (search, playlists). The library thread scans and does every write: tag edits, cue points, removals and playlist changes are jobs in one queue, run in order, during a scan between two of its files. Scan requests during a scan are merged into one more scan; every scan ends with the dead link check (`checkMissingFiles`). The analysis (tempo, overview waveform) runs on a third thread and hands its result to the queue as a job; a track the user waits for goes to the front.
- First start: while the library has no music folder, the library view opens the folder dialog at its first update, as File > Add music folder does; after a cancel it asks again at the next start, nothing is stored.
- Scan (`scanFolders`): the folders recursively, only audio files by extension; tags are read only for new files and files whose modification time or size changed. Tags come from TagLib (`readTags`); an MP3 with only an ID3v1 tag (no ID3v2, no APE) takes its genre from the comment, if there is one. Committed in batches, so a search sees the progress.
- Tables:
  - `folders`: the folders the library covers.
  - `tracks`: one row per file with tags, file stamp, the missing flag, cue point, tempo and whether it was analyzed. Besides the tags it keeps them folded (`foldText`: lower case, without accents for Latin script) in `*_key` columns, for the filters, wildcard words and sorting.
  - `tracks_search`: the full text index (FTS5) over artist, title, album, genre and file name. It keeps no copy of the text (`content = 'tracks'`), triggers keep it in step with `tracks`. SQLite adds its own tables for it (`tracks_search_data`, `_idx`, `_config`); the code never uses them directly. It keeps no column sizes (`columnsize = 0`), since those only serve ranking by relevance.
  - `waveforms`: the overview waveform per track (its loudness), apart from `tracks` so reading tracks never loads it.
  - `playlists`, `playlist_entries`: saved playlists with dense positions; a change of the order rewrites a playlist's entries in one transaction.
- Search (`buildSearch`): one SELECT over `tracks`. Plain words of the search text go to FTS5 as prefix terms, all must match; words with `*` or `?` and the column filters are GLOB patterns on the folded columns. The search only filters: the order is always the chosen column, empty values last, ties by artist and title. Results are never ranked by relevance, so the list keeps its order while typing.
- Schema version in `PRAGMA user_version` (`TrackStore::schema_version`). Until 1.0 there are no migrations: the schema is created in its final form, a database of any other version is refused with the advice to delete it. The same holds for the settings and layout files: a file of another version or with a line missing falls back to the defaults.

## Analysis

- `core/analysis`: `analyzeFile` decodes the whole file and finds the tempo and the overview waveform in one pass. It takes seconds per track, so it runs on the library's analysis thread (see Library), never on the audio or the frontend's thread.
- Tempo (`BpmAnalyzer`): QM-DSP, onset detection function and beat tracking. The tracker gives the beats on its grid of detection steps only (about 11 ms), so a single beat interval is a whole number of steps and the median of them would be off by up to 1.3 % (at 128 BPM 126.1 or 129.2); the tempo is the slope of a straight line through the positions of the longest run of regular beats (least squares, intervals within 15 % of the median), exact to a few hundredths of a BPM; with fewer than 8 regular beats the median. The result is folded into 70 to 180 BPM by halving or doubling, since beat trackers often find half or double what a DJ would count; no tempo if the track is too short or has no beat. The player shows it with the pitch taken into account and two decimals, so the number follows a pitch step of 0.02 %; the library's column has two as well.
- Waveform (`WaveformBuilder`): the mean square of every small block while decoding, folded into a fixed number of columns at the end (the RMS of the column's blocks in dB, 0 for -60 dBFS or quieter to 255 for full scale), so the length need not be known in advance. The loudness, not the peak: a track mastered loud has peaks near full scale everywhere, its loudness still shows intro, breakdown and drop. Stored in the `waveforms` table. The player (`WaveformBar`) pulls it apart: the loudest column at the top, the bottom at the quietest but for the quietest 5 % of the columns, at least 12 dB below the top.
- When it runs: a track loaded into a player that is not analyzed yet (at the front of the queue, the user waits for it), the library's context menu for one track (scan again, then analyze), and Analyze all tracks (at the back). An analyzed track is not analyzed again; a changed file loses its analysis, a tag edit keeps it.
- Closing the library ends a running analysis early; nothing is stored then. A file that cannot be decoded is marked analyzed without tempo or waveform, so it is not tried again and again.
- Not part of it: the first audible sound for Cue-Load, which `api::Core::loadTrack` looks for in the first 30 seconds of the loaded track.

## Code conventions

- C++23, no compiler extensions. Minimum toolchains: GCC 13, Clang 17 (libc++ 17), Apple Clang 15 (Xcode 15), MSVC 19.38 (VS 2022 17.8).
- Warnings: `-Wall -Wextra -Wpedantic` (MSVC: `/W4`), warnings are errors in CI.
- Prefer `std::span`, `std::array`, `std::unique_ptr`. No raw owning pointers.
- No exceptions across the core API boundary. Return `std::expected`.
- Allowed C++23 library features: `std::expected`, `std::to_underlying`, `std::unreachable`, `std::byteswap`, `std::ranges` additions supported by all minimum toolchains.
- Do not use: C++ modules, `std::flat_map`, `std::mdspan`, `std::generator`, `std::stacktrace`, `std::print` (uneven standard library support).
- Error type in `std::expected` is `std::string` or a dedicated error type.

### Naming
- Types (classes, structs, enums, aliases): `PascalCase`. Enumerators: `PascalCase`, use `enum class`.
- Functions and methods: `camelCase`.
- Variables, parameters and members: `snake_case`. Members have no prefix (no `m_`).
- Constants: `constexpr` in `snake_case`, no `UPPER_CASE`. `UPPER_CASE` only for macros.
- Files: `snake_case` with `.hpp` and `.cpp`, e.g. `deck_transport.hpp`.
- Namespaces follow the directory: `djw::core::engine`, `djw::player_ui::sdl`, ... (`-` in directory names becomes `_`). File-local helpers go into an anonymous namespace.

### Headers
- Include guards instead of `#pragma once`, built from the path: `DJW_CORE_ENGINE_DECK_HPP`, closed with `#endif // DJW_CORE_ENGINE_DECK_HPP`.
- Includes in this order, separated by a blank line: own header (in `.cpp`), project headers with path relative to the module root (`"engine/deck.hpp"`), then standard and third-party headers in `<>`.

### Formatting
Enforced by `.clang-format`. The clang-format version is pinned (see Static analysis and CI), because the output differs between versions:
- Based on LLVM style, 4 spaces indentation, no tabs, column limit 120.
- Allman braces (opening brace on its own line), also for namespaces, classes and functions.
- Namespace contents and access modifiers are indented; members below `public:`/`private:` are indented again.
- Consecutive assignments and declarations are aligned.
- Short functions on one line only when defined inline in the class. No single-line `if`.
- `&` and `*` bind to the name: `const Deck &deck`, `char *buffer`.

### Documentation
- Classes get a `/** ... */` block explaining purpose and behavior.
- Methods get a `///` line when the name alone is not enough.
- Comments explain why, not what.

### Structure
- Keep files small and focused. One class per file pair as a rule.
- No commented-out code in commits.

## Build

```bash
cmake --preset linux-debug
cmake --build --preset linux-debug
ctest --preset linux-debug
```

Presets (`CMakePresets.json`): `linux-debug`, `linux-release`, `linux-arm64-release`, `windows-debug`, `windows-release`, `macos-debug`, `macos-release`. Sanitizer presets: `linux-asan` (ASan + LeakSanitizer + UBSan) and `linux-tsan` (TSan), each with configure, build and test preset; the test presets set the sanitizer options (leak detection on, abort on the first error). Android, iOS and Web presets come with those platforms. `scripts/build-linux.sh` builds the Linux release binary.

Build options:
- `DJW_PLAYER_SDL` (default ON): build the SDL3 player in `src/player-ui/sdl`
- `DJW_BUILD_TESTS` (default ON in debug)
- `DJW_WITH_FFMPEG` (default OFF)
- `DJW_WITH_DVS` (default OFF; DVS is planned)
- `DJW_WARNINGS_AS_ERRORS` (default OFF, ON in CI)
- `DJW_SANITIZE`: empty, `address` (ASan + LeakSanitizer + UBSan) or `thread` (TSan). Applies to the whole build including fetched dependencies.

## Tests

Tests live in `tests/<module>`, one doctest executable per module, so every module can be built and tested alone. All tests run with `ctest`.

### Unit tests
- Core logic gets unit tests (doctest).
- No mocking framework. Use hand-written fakes (`FakeClock`, `FakeAudioDevice`, `FakeMidiInput`, `MemoryFileSystem`, ...) and check the end state, not call sequences that break with every internal change.

### Integration tests
- Engine changes need an offline render test: run a command sequence through the command queue against the engine without an audio device, render to a buffer, compare against a golden file.
- Golden comparison uses a per-sample tolerance (maximum absolute deviation on normalized float samples, default `1e-5`, overridable per test), not bit-exact comparison, because float results differ between x86-64/ARM and between compilers.
- Golden files are generated on the reference platform (Linux x86-64, GCC) and committed together with the change that alters the output.
- Library tests run against a real SQLite database in a temporary directory.
- Controller mapping tests feed fake MIDI events through a TOML mapping and check the resulting commands.

### Architecture tests
- Target separation: core targets must not link SDL or any UI target. This is the hard boundary.
- An include check runs as a CTest test (CMake script, no extra dependency): `core/` must not include `player-ui/` or SDL headers, `player-ui/` may include from the core only `core/api/`.
- Real-time safety: in test builds `malloc`/`new` are replaced by counting versions. A test drives the audio callback with a typical command sequence and requires zero allocations inside it. Locks in the audio path are covered by TSan.

### Mutation tests
- They check that the tests actually detect changed behavior, not just execute the code. `tools/mutate.py` (Python, no dependency) changes one spot of a source file at a time (comparison, arithmetic and logical operators, constants, negations, conditions, removed statements), rebuilds, runs the tests of that module and lists the mutants no test noticed. Each surviving mutant is either a missing test or code that does not matter.
- Run it on a separate checkout (`git worktree`) with its own build folder, never on the working tree; `--shard i/n` splits the work over several checkouts. The usage is in the script's header.
- Scope for now: `src/core/`. A mutant that cannot matter (e.g. a log text) is marked in the source with a `mutation: skip` comment on its line.
- `tools/mutate.py` is not part of CI (too slow); run it before a release.
- In CI the compiler-based tool Mull runs once a month and by hand (`.github/workflows/mutation.yml`, config in `mull.yml`): the core is built with clang-19 and the Mull plugin, which builds all mutants into the test programs, so nothing is rebuilt per mutant. Only lines covered by a test are mutated; the runs of all test programs are combined, so a mutant counts as killed when any test kills it.
- Not mutated: `src/core/audio_io/miniaudio_device.cpp`, a thin adapter on the audio hardware whose settings only show on a real device (excluded in `mull.yml`). Single lines are excluded with `// mull-ignore` or `// mull-ignore-next` (`// mull-off` ... `// mull-on` for a block), for one mutator with `: <mutator>` appended.
- Phantoms: Mull reports some mutants as survived without switching them on, so the tests run unchanged and pass. This happens to removed calls whose arguments have a destructor (`cxx_remove_void_call` on a lambda passed as `std::function`, a struct with a path), to some assignments from such a call, and to mutants in inline functions defined in headers. A phantom is marked with `// mull-ignore-next: <mutator>` on the line before, or `// mull-ignore-next` for every mutator of the line, only after checking by hand that a test fails with the change.
- Mutants that cannot change the behavior stay unmarked: values set again before they are used, a read of one element past the end that is overwritten afterwards, the tuning values of the QM-DSP beat detection. Error paths of the library are tested with the SQL fault of `sqlite_database.hpp` (`testing::failSql`, in the tests the guard `SqlFault`).
- The report with the surviving mutants is attached to the run. The badge on the branch `badges` shows Mull's mutation score, which counts timeouts as not killed; the summary of the run also lists killed, survived and timed-out mutants.

### Memory leaks
- Avoid leaks by construction: every resource has an owner (RAII). Heap memory through `std::unique_ptr` or containers, C library handles (miniaudio, SQLite, SDL, ...) in a small RAII wrapper with a deleter, never with manual `free`/`close` calls spread over the code.
- Leaks are checked, not assumed: LeakSanitizer runs with every ASan test run (`ASAN_OPTIONS=detect_leaks=1` set explicitly in the test environment). A reported leak fails the test.
- LeakSanitizer is only reliable on Linux (macOS on Apple Silicon and MSVC do not support it), so the Linux ASan run in CI is the authoritative leak check.
- Leaks inside third-party libraries are suppressed in a suppression file with a comment naming the library and the reason, never by disabling the check. The same holds for data races TSan reports inside third-party code (`tests/sanitizers/tsan.supp`, e.g. SQLite's lock-free WAL index header) and for undefined behavior UBSan finds there (`tests/sanitizers/ubsan.supp`, e.g. TagLib writing an empty block through a null pointer).
- Long-running paths (deck load/unload, adding/removing decks, library scans) get tests that repeat the operation many times, so a leak per operation shows up even if it is small.

### General
- Run tests before declaring a task done, including the sanitizer runs (ASan with LeakSanitizer, UBSan, TSan for threading code) before a release.
- Debug builds must be able to run with ASan, UBSan and TSan.

## Static analysis and CI

- `.clang-tidy` in the repository root with `bugprone-*`, `modernize-*`, `performance-*`, `cppcoreguidelines-*`, `readability-identifier-naming` (enforces the naming rules above) and selected `misc-*` checks. Noisy checks are disabled there with a reason, not suppressed in code.
- clang-tidy runs on the sources in `src/`, not on `tests/` (the doctest macros trigger many checks), `third_party/` or fetched dependencies. Locally optional, in CI warnings are errors.
- clang-format and clang-tidy are pinned to one version in `.github/lint-requirements.txt` and installed with pip, so local runs and CI give the same result. Use the same version locally: `python3 -m venv .venv-lint && .venv-lint/bin/pip install -r .github/lint-requirements.txt`.
- Duplicated code is caught in review, there is no separate duplicate detection tool.
- CI (`.github/workflows/ci.yml`) runs for every push to `main` and every pull request, for now on Linux x86_64 only:
  - build and all tests with `linux-asan` (ASan + LeakSanitizer + UBSan) and `linux-tsan`, with warnings as errors and GCC 13 (the minimum)
  - Clang with libc++ is not in the CI; it is built on the Mac
  - clang-format check and clang-tidy
  - line coverage of `src/` with gcovr; the report is attached to the run. On `main` the coverage badge (an SVG made by `.github/scripts/coverage_badge.py`, no external service) is committed to the branch `badges`, from which the README shows it
- `tools/ci-local.sh` runs the same jobs before a commit, in a container (`tools/ci-local/Dockerfile`) with the Ubuntu, compilers and tools of the CI, on the files a commit would hold, with the dependencies fetched as in the CI (a TagLib installed on the machine lays out its headers differently). The four jobs run side by side; `tools/ci-local.sh lint` runs one of them. Tests wait for what other threads do with a deadline, not for a number of frames, and anything that goes by the clock takes a clock the test sets: under TSan on a busy runner frames and threads are slow.
- Mutation tests with Mull run in their own workflow (`.github/workflows/mutation.yml`), once a month, see Tests: Mutation tests
- The CI does not start the SDL player, so leaks in system graphics drivers (seen locally with Mesa/EGL) do not show up there. A LeakSanitizer suppression file is added once a CI job runs the player.

## Workflow

- Small, focused commits. One logical change per commit.
- Commit messages in English, imperative mood (`Add deck command queue`, not `Added ...`).
- When a feature from FEATURES.md is completed, reference it in the commit or PR, so the spec can be tracked.
- If the spec is ambiguous or contradictory, ask instead of guessing.
- Do not refactor unrelated code while working on a task.

## Target hardware for testing

- Reference controller: Hercules DJControl Starlight (class-compliant USB MIDI + 4-channel audio: master on 1/2, headphones on 3/4).
- Performance baseline: Raspberry Pi 4 (4 GB). If something is too slow there, it is too slow.
