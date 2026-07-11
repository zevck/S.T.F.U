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
