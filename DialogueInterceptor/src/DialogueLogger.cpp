#include "../include/DialogueLogger.h"
#include "../include/PCH.h"
#include <chrono>
#include <iomanip>
#include <sstream>
#include <filesystem>

namespace DialogueLogger
{
    static std::ofstream logFile_;
    static std::mutex logMutex_;
    static bool enabled_ = true;
    static std::string logPath_;
    static const size_t MAX_FILE_SIZE = 10 * 1024 * 1024;  // 10 MB
    
    std::string FormatTimestamp(int64_t timestamp)
    {
        // Use current time instead of passed timestamp (which may be in milliseconds)
        auto now = std::chrono::system_clock::now();
        auto tt = std::chrono::system_clock::to_time_t(now);
        
        std::tm tm;
        localtime_s(&tm, &tt);
        
        std::ostringstream oss;
        oss << std::put_time(&tm, "%H:%M:%S");
        return oss.str();
    }
    
    void RotateLogIfNeeded()
    {
        if (!logFile_.is_open()) return;
        
        // Check file size
        auto pos = logFile_.tellp();
        if (pos < MAX_FILE_SIZE) return;
        
        // Close current file
        logFile_.close();
        
        // Rename old log
        std::string oldPath = logPath_ + ".old";
        try {
            if (std::filesystem::exists(oldPath)) {
                std::filesystem::remove(oldPath);
            }
            std::filesystem::rename(logPath_, oldPath);
        } catch (...) {
            spdlog::warn("[DialogueLogger] Failed to rotate log file");
        }
        
        // Reopen new file
        logFile_.open(logPath_, std::ios::out | std::ios::app);
        if (logFile_.is_open()) {
            logFile_ << "\n";  // Just a blank line for rotated files
            logFile_.flush();
        }
    }
    
    void Initialize()
    {
        std::lock_guard<std::mutex> lock(logMutex_);
        
        // Determine log path - same directory as YAML configs
        wchar_t buffer[MAX_PATH];
        GetModuleFileNameW(nullptr, buffer, MAX_PATH);
        std::filesystem::path exePath(buffer);
        auto logDir = exePath.parent_path() / "Data" / "SKSE" / "Plugins" / "STFU";
        
        try {
            std::filesystem::create_directories(logDir);
            logPath_ = (logDir / "STFU_DialogueLog.txt").string();
        } catch (...) {
            spdlog::error("[DialogueLogger] Failed to create log directory");
            enabled_ = false;
            return;
        }
        
        // Open log file (append mode)
        logFile_.open(logPath_, std::ios::out | std::ios::app);
        
        if (!logFile_.is_open()) {
            spdlog::error("[DialogueLogger] Failed to open log file: {}", logPath_);
            enabled_ = false;
            return;
        }
        
        logFile_.flush();
        spdlog::info("[DialogueLogger] Initialized. Log file: {}", logPath_);
    }
    
    void Shutdown()
    {
        std::lock_guard<std::mutex> lock(logMutex_);
        if (logFile_.is_open()) {
            logFile_.close();
        }
    }
    
    bool IsEnabled()
    {
        return enabled_;
    }
    
    void SetEnabled(bool enabled)
    {
        enabled_ = enabled;
    }
    
    void LogEntry(
        int64_t timestamp,
        const std::string& status,
        const std::string& subtypeName,
        const std::string& questEditorID,
        uint32_t topicFormID,
        const std::string& topicEditorID,
        const std::string& speakerName,
        const std::string& responseText,
        const std::string& sourcePlugin,
        bool skyrimNetBlockable)
    {
        if (!enabled_) {
            spdlog::warn("[DialogueLogger] LogEntry called but logger is disabled");
            return;
        }
        
        std::lock_guard<std::mutex> lock(logMutex_);
        
        if (!logFile_.is_open()) {
            spdlog::error("[DialogueLogger] LogEntry called but log file is not open");
            return;
        }
        
        spdlog::trace("[DialogueLogger] Logging dialogue: {}", speakerName);
        
        // Check if rotation is needed
        RotateLogIfNeeded();
        
        // Format timestamp
        std::string timeStr = FormatTimestamp(timestamp);
        
        // Truncate dialogue if too long
        std::string truncatedText = responseText;
        if (truncatedText.length() > 100) {
            truncatedText = truncatedText.substr(0, 97) + "...";
        }
        
        // Format FormID to match YAML format (with one leading zero)
        char formIDHex[16];
        sprintf_s(formIDHex, "%08X", topicFormID);
        
        // Strip leading zeros but keep at least one digit, then add one leading zero
        const char* stripped = formIDHex;
        while (*stripped == '0' && *(stripped + 1) != '\0') {
            stripped++;
        }
        
        char formIDStr[16];
        sprintf_s(formIDStr, "0%s", stripped);  // Add one leading zero
        
        // Line 1: Human-readable context
        // [HH:MM:SS] [STATUS] [Subtype] [Quest] [FormID] [Menu] Speaker: dialogue
        logFile_ << "[" << timeStr << "] "
                 << "[" << status << "] "
                 << "[" << subtypeName << "] "
                 << "[" << (questEditorID.empty() ? "NoQuest" : questEditorID) << "] "
                 << "[" << formIDStr << "]";
        
        // Add [Menu] indicator if SkyrimNet blockable
        if (skyrimNetBlockable) {
            logFile_ << " [Menu]";
        }
        
        logFile_ << " " << speakerName << ": " << truncatedText << "\n";
        
        // Line 2: Copy/paste ready YAML entry (no suffix, just EditorID or FormID)
        if (!topicEditorID.empty()) {
            logFile_ << "  - " << topicEditorID << "\n";
        } else {
            logFile_ << "  - " << formIDStr << "\n";
        }
        
        logFile_.flush();
        spdlog::trace("[DialogueLogger] Successfully logged dialogue entry to {}", logPath_);
    }
}
