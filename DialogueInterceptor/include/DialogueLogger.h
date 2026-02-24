#pragma once

#include <string>
#include <fstream>
#include <mutex>
#include <cstdint>

namespace DialogueLogger
{
    // Log a dialogue entry to the text file in copy/paste-ready format
    // Format:
    //   [HH:MM:SS] [STATUS] [Subtype] [Quest] [FormID] Speaker: dialogue text
    //     - FormID
    void LogEntry(
        int64_t timestamp,
        const std::string& status,          // "ALLOWED", "SOFT BLOCK", "HARD BLOCK", etc.
        const std::string& subtypeName,     // "Hello", "Attack", etc.
        const std::string& questEditorID,   // Quest name
        uint32_t topicFormID,               // Topic FormID
        const std::string& topicEditorID,   // Topic EditorID (optional)
        const std::string& speakerName,     // Speaker name
        const std::string& responseText,    // Dialogue text
        const std::string& sourcePlugin,    // Plugin name
        bool skyrimNetBlockable             // True if menu dialogue (SkyrimNet compatible)
    );
    
    // Initialize the logger (opens file, writes header)
    void Initialize();
    
    // Close the log file
    void Shutdown();
    
    // Check if logger is enabled
    bool IsEnabled();
    
    // Set whether logging is enabled
    void SetEnabled(bool enabled);
}
