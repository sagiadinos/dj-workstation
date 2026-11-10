// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "api/library.hpp"
#include "library/library_service.hpp"
#include "library/tag_reader.hpp"
#include "library/utf8_path.hpp"

#include <system_error>

namespace djw::core::api
{
    std::expected<std::unique_ptr<Library>, std::string> openLibrary(const LibraryConfig &config)
    {
        std::error_code error;
        if (const auto folder = config.database_file.parent_path(); !folder.empty())
        {
            std::filesystem::create_directories(folder, error);
            if (error)
            {
                return std::unexpected{"cannot create " + library::toUtf8(folder) + ": " + error.message()};
            }
        }
        auto service = library::LibraryService::open(config.database_file, &library::readTags);
        if (!service)
        {
            return std::unexpected{service.error()};
        }
        return std::move(*service);
    }
} // namespace djw::core::api
