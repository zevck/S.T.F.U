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
}
