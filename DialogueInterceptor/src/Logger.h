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
        
        path /= L"My Games\\Skyrim Special Edition\\SKSE\\DialogueInterceptor.log";
        
        // Create directories if needed
        std::filesystem::create_directories(path.parent_path());
        
        return path;
    }

    inline void Setup()
    {
        auto path = GetLogPath();
        auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path.string(), true);

        auto log = std::make_shared<spdlog::logger>("global log", std::move(sink));
        log->set_level(spdlog::level::info);
        log->flush_on(spdlog::level::info);

        spdlog::set_default_logger(std::move(log));
        spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");

        spdlog::info("DialogueInterceptor v{} initialized", PLUGIN_VERSION);
    }
}
