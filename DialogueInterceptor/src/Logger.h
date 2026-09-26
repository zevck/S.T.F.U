/*
 * STFU - a Skyrim SKSE plugin for silencing and filtering NPC dialogue.
 * Copyright (C) 2026 Zevick
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include "../include/PCH.h"
#include "../include/version.h"

namespace Logger
{
    inline std::filesystem::path GetLogPath()
    {
        wchar_t* docPath = nullptr;
        SHGetKnownFolderPath(FOLDERID_Documents, 0, nullptr, &docPath);
        std::filesystem::path path(docPath);
        CoTaskMemFree(docPath);
        
        path /= L"My Games\\Skyrim Special Edition\\SKSE\\STFU.log";
        
        // Create directories if needed
        std::filesystem::create_directories(path.parent_path());
        
        return path;
    }

    // Debug logging is opt-in via a marker file next to STFU.log. Drop
    // "stfu_debug.flag" (any contents) alongside the log file and restart the
    // game; STFU will log at debug level for that run. Delete the flag to
    // return to info level. Kept as a filesystem toggle (not an INI key) so
    // it works even when the settings load path is broken.
    inline bool IsDebugLoggingEnabled()
    {
        auto flagPath = GetLogPath().parent_path() / L"stfu_debug.flag";
        std::error_code ec;
        return std::filesystem::exists(flagPath, ec);
    }

    inline void Setup()
    {
        auto path = GetLogPath();
        auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path.string(), true);

        auto log = std::make_shared<spdlog::logger>("global log", std::move(sink));

        const bool debugEnabled = IsDebugLoggingEnabled();
        const auto level = debugEnabled ? spdlog::level::debug : spdlog::level::info;
        log->set_level(level);
        log->flush_on(level);

        spdlog::set_default_logger(std::move(log));
        spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");

        spdlog::info("STFU loaded (debug logging: {})", debugEnabled ? "on" : "off");
    }
}
