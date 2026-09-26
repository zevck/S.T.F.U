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

#include "PrismaUIMenu.h"
#include "EditorID.h"
#include "Config.h"
#include "SettingsPersistence.h"
#include "TopicResponseExtractor.h"
#include <spdlog/spdlog.h>
#include <sstream>
#include <iomanip>
#include "PrismaUIMenuJson.h"

using namespace PrismaUIMenuDetail;

void PrismaUIMenu::OnAddToWhitelist(const char* data)
{
    if (!data) {
        spdlog::error("[PrismaUIMenu::OnAddToWhitelist] Null data received");
        return;
    }
    
    spdlog::info("[PrismaUIMenu::OnAddToWhitelist] Received data");
    
    try {
        auto* db = DialogueDB::GetDatabase();
        if (!db) {
            spdlog::error("[PrismaUIMenu::OnAddToWhitelist] Database not available");
            return;
        }
        
        // Parse JSON data: {"entries":[{...}],"notes":"..."}
        std::string jsonStr(data);
        
        // Extract optional notes
        auto extractOptionalString = [&jsonStr](const std::string& key) -> std::string {
            std::string searchKey = "\"" + key + "\":\"";
            size_t keyPos = jsonStr.find(searchKey);
            if (keyPos == std::string::npos) return "";
            size_t valueStart = keyPos + searchKey.length();
            size_t valueEnd = jsonStr.find("\"", valueStart);
            if (valueEnd == std::string::npos) return "";
            return jsonStr.substr(valueStart, valueEnd - valueStart);
        };
        
        std::string userNotes = extractOptionalString("notes");
        spdlog::info("[PrismaUIMenu::OnAddToWhitelist] Notes: '{}'", userNotes);
        
        // Helper to extract JSON string value
        auto extractString = [](const std::string& json, const std::string& key, size_t startPos = 0) -> std::string {
            std::string searchKey = "\"" + key + "\":\"";
            size_t keyPos = json.find(searchKey, startPos);
            if (keyPos == std::string::npos) return "";
            size_t valueStart = keyPos + searchKey.length();
            size_t valueEnd = json.find("\"", valueStart);
            if (valueEnd == std::string::npos) return "";
            return json.substr(valueStart, valueEnd - valueStart);
        };
        
        // Helper to extract JSON array
        auto extractArray = [](const std::string& json, const std::string& key, size_t startPos = 0) -> std::string {
            std::string searchKey = "\"" + key + "\":[";
            size_t keyPos = json.find(searchKey, startPos);
            if (keyPos == std::string::npos) return "[]";
            size_t arrayStart = keyPos + searchKey.length() - 1;
            size_t arrayEnd = json.find("]", arrayStart);
            if (arrayEnd == std::string::npos) return "[]";
            return json.substr(arrayStart, arrayEnd - arrayStart + 1);
        };
        
        // Find all entry objects
        std::vector<DialogueDB::BlacklistEntry> entriesToAdd;
        size_t searchPos = 0;
        
        while (true) {
            // Find next topicEditorID field
            size_t topicPos = jsonStr.find("\"topicEditorID\":\"", searchPos);
            if (topicPos == std::string::npos) break;
            
            // Find the start of this object (search backwards for {)
            size_t objStart = jsonStr.rfind("{", topicPos);
            if (objStart == std::string::npos) break;
            
            // Find the end of this object
            size_t objEnd = jsonStr.find("},", topicPos);
            if (objEnd == std::string::npos) {
                objEnd = jsonStr.find("}]", topicPos);
                if (objEnd == std::string::npos) break;
            }
            
            // Extract this entry's JSON substring
            std::string entryStr = jsonStr.substr(objStart, objEnd - objStart + 1);
            
            // Parse entry fields
            DialogueDB::BlacklistEntry entry;
            
            // Check if this is a scene
            bool isScene = (entryStr.find("\"isScene\":true") != std::string::npos);
            bool isBardSong = (entryStr.find("\"isBardSong\":true") != std::string::npos);
            
            entry.targetType = (isScene || isBardSong) ? DialogueDB::BlacklistTarget::Scene : DialogueDB::BlacklistTarget::Topic;
            
            // Extract EditorIDs
            if (isScene || isBardSong) {
                entry.targetEditorID = extractString(entryStr, "sceneEditorID");
                entry.targetFormID = 0;
                entry.questEditorID = extractString(entryStr, "questEditorID");
            } else {
                entry.targetEditorID = extractString(entryStr, "topicEditorID");
                entry.questEditorID = extractString(entryStr, "questEditorID");
                
                // Extract FormID for topics
                std::string formIDStr = extractString(entryStr, "topicFormID");
                entry.targetFormID = 0;
                if (!formIDStr.empty()) {
                    try {
                        entry.targetFormID = std::stoul(formIDStr, nullptr, 16);
                    } catch (...) {
                        spdlog::warn("[PrismaUIMenu::OnAddToWhitelist] Failed to parse FormID: {}", formIDStr);
                    }
                }
            }
            
            entry.sourcePlugin = extractString(entryStr, "sourcePlugin");
            entry.subtypeName = extractString(entryStr, "subtypeName");
            
            // Extract and serialize allResponses array to responseText
            std::string allResponsesArray = extractArray(entryStr, "allResponses");
            entry.responseText = allResponsesArray;
            
            // Parse actor filters - handles both "0x01A6A4" and "01A6A4" formats
            auto parseActorFormIDs = [&entryStr]() -> std::vector<uint32_t> {
                std::vector<uint32_t> formIDs;
                size_t arrayStart = entryStr.find("\"actorFilterFormIDs\":[");
                if (arrayStart == std::string::npos) return formIDs;
                
                size_t pos = entryStr.find("[", arrayStart);
                if (pos == std::string::npos) return formIDs;
                pos++;
                
                while (pos < entryStr.length()) {
                    while (pos < entryStr.length() && (entryStr[pos] == ' ' || entryStr[pos] == ',')) pos++;
                    if (pos >= entryStr.length() || entryStr[pos] == ']') break;
                    if (entryStr[pos] != '\"') break;
                    pos++;
                    
                    size_t hexEnd = entryStr.find("\"", pos);
                    if (hexEnd == std::string::npos) break;
                    
                    std::string hexStr = entryStr.substr(pos, hexEnd - pos);
                    if (hexStr.length() >= 2 && hexStr[0] == '0' && (hexStr[1] == 'x' || hexStr[1] == 'X')) {
                        hexStr = hexStr.substr(2);
                    }
                    
                    try {
                        if (!hexStr.empty()) {
                            uint32_t formID = std::stoul(hexStr, nullptr, 16);
                            formIDs.push_back(formID);
                        }
                    } catch (...) { }
                    
                    pos = hexEnd + 1;
                }
                return formIDs;
            };
            
            auto parseActorNames = [&entryStr]() -> std::vector<std::string> {
                std::vector<std::string> names;
                size_t arrayStart = entryStr.find("\"actorFilterNames\":[");
                if (arrayStart == std::string::npos) return names;
                size_t pos = entryStr.find("\"", arrayStart + 20);
                while (pos != std::string::npos && pos < entryStr.size()) {
                    if (entryStr[pos] != '\"') break;
                    pos++;
                    size_t nameEnd = entryStr.find("\"", pos);
                    if (nameEnd == std::string::npos) break;
                    names.push_back(entryStr.substr(pos, nameEnd - pos));
                    pos = entryStr.find("\",\"", nameEnd);
                    if (pos == std::string::npos) break;
                    pos += 3;
                }
                return names;
            };
            
            entry.actorFilterFormIDs = parseActorFormIDs();
            entry.actorFilterNames = parseActorNames();
            
            // Set notes from user input
            entry.notes = userNotes;
            
            // Set whitelist-specific fields
            entry.addedTimestamp = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count();
            
            // Whitelist entries use the filterCategory to specify their type
            entry.filterCategory = entry.subtypeName.empty() ? "Whitelist" : entry.subtypeName;
            
            entriesToAdd.push_back(entry);
            
            // Move to next entry
            searchPos = objEnd + 1;
        }
        
        spdlog::info("[PrismaUIMenu::OnAddToWhitelist] Parsed {} entries from JSON", entriesToAdd.size());
        
        // Add entries to whitelist and remove from blacklist if present
        int addedCount = 0;
        for (const auto& entry : entriesToAdd) {
            // Check if this entry exists in blacklist and remove it
            if (entry.targetType == DialogueDB::BlacklistTarget::Scene && !entry.targetEditorID.empty()) {
                // Scene entry - find by scene EditorID
                auto blacklist = db->GetBlacklist();
                for (const auto& blEntry : blacklist) {
                    if (blEntry.targetType == DialogueDB::BlacklistTarget::Scene &&
                        blEntry.targetEditorID == entry.targetEditorID) {
                        spdlog::info("[PrismaUIMenu::OnAddToWhitelist] Removing scene '{}' from blacklist", entry.targetEditorID);
                        db->RemoveFromBlacklist(blEntry.id);
                        break;
                    }
                }
            } else if (entry.targetType == DialogueDB::BlacklistTarget::Topic) {
                // Topic entry - find by FormID or EditorID
                int64_t blacklistId = db->GetBlacklistEntryId(entry.targetFormID, entry.targetEditorID);
                if (blacklistId > 0) {
                    spdlog::info("[PrismaUIMenu::OnAddToWhitelist] Removing topic FormID=0x{:08X} EditorID='{}' from blacklist", 
                        entry.targetFormID, entry.targetEditorID);
                    db->RemoveFromBlacklist(blacklistId);
                }
            }
            
            // Add to whitelist
            if (db->AddToWhitelist(entry)) {
                addedCount++;
            }
        }
        
        spdlog::info("[PrismaUIMenu::OnAddToWhitelist] Successfully added {} of {} entries", 
            addedCount, entriesToAdd.size());
        
        // Refresh UI
        SendWhitelistData();
        SendBlacklistData();
        SendHistoryData();
        
    } catch (const std::exception& e) {
        spdlog::error("[PrismaUIMenu::OnAddToWhitelist] Exception: {}", e.what());
    }
}

// ===== Whitelist Functions =====

void PrismaUIMenu::SendWhitelistData()
{
    if (!initialized_ || !prismaUI_ || !prismaUI_->IsValid(view_)) {
        spdlog::warn("[PrismaUIMenu::SendWhitelistData] Not initialized or invalid view");
        return;
    }
    
    try {
        std::string jsonData = SerializeWhitelistToJSON();
        spdlog::info("[PrismaUIMenu] Sending {} bytes of whitelist data", jsonData.size());
        
        std::string jsCode = BuildSKSEUpdateScript("updateWhitelist", jsonData);
        prismaUI_->Invoke(view_, jsCode.c_str());
        
        spdlog::debug("[PrismaUIMenu] Successfully invoked updateWhitelist");
    } catch (const std::exception& e) {
        spdlog::error("[PrismaUIMenu] Failed to send whitelist data: {}", e.what());
    }
}

std::string PrismaUIMenu::SerializeWhitelistToJSON()
{
    auto* db = DialogueDB::GetDatabase();
    if (!db) {
        return "[]";
    }
    
    // Get all whitelist entries (reuses BlacklistEntry structure)
    auto whitelist = db->GetWhitelist();
    
    std::ostringstream json;
    json << "[";
    
    bool first = true;
    for (const auto& entry : whitelist) {
        if (!first) json << ",";
        first = false;
        
        json << "{";
        json << "\"id\":" << entry.id << ",";
        
        // Add target type
        std::string targetTypeStr;
        switch (entry.targetType) {
            case DialogueDB::BlacklistTarget::Topic: targetTypeStr = "Topic"; break;
            case DialogueDB::BlacklistTarget::Quest: targetTypeStr = "Quest"; break;
            case DialogueDB::BlacklistTarget::Subtype: targetTypeStr = "Subtype"; break;
            case DialogueDB::BlacklistTarget::Scene: targetTypeStr = "Scene"; break;
            case DialogueDB::BlacklistTarget::Plugin: targetTypeStr = "Plugin"; break;
            case DialogueDB::BlacklistTarget::Actor: targetTypeStr = "Actor"; break;
            case DialogueDB::BlacklistTarget::Faction: targetTypeStr = "Faction"; break;
            default: targetTypeStr = "Unknown"; break;
        }
        json << "\"targetType\":\"" << targetTypeStr << "\",";
        
        // Whitelist entries show as "Allowed" instead of block type
        json << "\"blockType\":\"Allowed\",";
        
        // Emit FormID as "0x" + 8 fixed hex chars. See the corresponding comment
        // in the history serializer for why the previous "leading zero" logic
        // was broken.
        if (entry.targetFormID != 0) {
            char formIDHex[16];
            sprintf_s(formIDHex, "0x%08X", entry.targetFormID);
            json << "\"topicFormID\":\"" << formIDHex << "\",";
        } else {
            json << "\"topicFormID\":null,";
        }
        
        json << "\"topicEditorID\":\"" << escapeJSON(entry.targetEditorID) << "\",";
        json << "\"questEditorID\":\"" << escapeJSON(entry.questEditorID) << "\",";
        json << "\"sourcePlugin\":\"" << escapeJSON(entry.sourcePlugin) << "\",";
        
        // Use questEditorID or targetEditorID as quest name
        json << "\"questName\":\"" << escapeJSON(entry.questEditorID) << "\",";
        
        json << "\"filterCategory\":\"" << escapeJSON(entry.filterCategory) << "\",";
        json << "\"note\":\"" << escapeJSON(entry.notes) << "\",";
        json << "\"dateAdded\":" << entry.addedTimestamp << ",";
        
        // Add responseText (already JSON-encoded array of responses)
        json << "\"responseText\":\"" << escapeJSON(entry.responseText) << "\",";
        
        // Add actor filters (FormIDs as hex strings, names as string array)
        json << "\"actorFilterFormIDs\":[";
        for (size_t i = 0; i < entry.actorFilterFormIDs.size(); ++i) {
            if (i > 0) json << ",";
            char formIDHex[16];
            sprintf_s(formIDHex, "%08X", entry.actorFilterFormIDs[i]);
            json << "\"0x" << formIDHex << "\"";
        }
        json << "],";
        
        json << "\"actorFilterNames\":[";
        for (size_t i = 0; i < entry.actorFilterNames.size(); ++i) {
            if (i > 0) json << ",";
            json << "\"" << escapeJSON(entry.actorFilterNames[i]) << "\"";
        }
        json << "],";
        
        // Add faction filters
        json << "\"factionFilterEditorIDs\":[";
        for (size_t i = 0; i < entry.factionFilterEditorIDs.size(); ++i) {
            if (i > 0) json << ",";
            json << "\"" << escapeJSON(entry.factionFilterEditorIDs[i]) << "\"";
        }
        json << "]";
        
        json << "}";
    }
    
    json << "]";
    
    return json.str();
}

void PrismaUIMenu::OnRequestWhitelist(const char* data)
{
    spdlog::trace("[PrismaUIMenu] Whitelist data requested");
    SendWhitelistData();
}

void PrismaUIMenu::OnRemoveFromWhitelist(const char* data)
{
    if (!data) {
        spdlog::error("[PrismaUIMenu::OnRemoveFromWhitelist] Null data received");
        return;
    }
    
    spdlog::info("[PrismaUIMenu::OnRemoveFromWhitelist] Received: {}", data);
    
    try {
        auto* db = DialogueDB::GetDatabase();
        if (!db) {
            spdlog::error("[PrismaUIMenu::OnRemoveFromWhitelist] Database not available");
            return;
        }
        
        // Parse JSON: {"id":123}
        std::string jsonStr(data);
        
        // Extract id
        size_t idPos = jsonStr.find("\"id\":");
        if (idPos == std::string::npos) {
            spdlog::error("[PrismaUIMenu::OnRemoveFromWhitelist] 'id' field not found");
            return;
        }
        
        size_t valueStart = idPos + 5;
        while (valueStart < jsonStr.length() && (jsonStr[valueStart] == ' ' || jsonStr[valueStart] == '\t')) {
            valueStart++;
        }
        
        size_t valueEnd = jsonStr.find_first_of(",}", valueStart);
        std::string idStr = jsonStr.substr(valueStart, valueEnd - valueStart);
        int64_t whitelistId = std::stoll(idStr);
        
        spdlog::info("[PrismaUIMenu::OnRemoveFromWhitelist] Removing entry ID: {}", whitelistId);
        
        if (db->RemoveFromWhitelist(whitelistId)) {
            spdlog::info("[PrismaUIMenu::OnRemoveFromWhitelist] Successfully removed entry ID: {}", whitelistId);
            
            // Refresh displays
            SendWhitelistData();
            SendHistoryData();
        } else {
            spdlog::error("[PrismaUIMenu::OnRemoveFromWhitelist] Failed to remove entry ID: {}", whitelistId);
        }
        
    } catch (const std::exception& e) {
        spdlog::error("[PrismaUIMenu::OnRemoveFromWhitelist] Exception: {}", e.what());
    }
}

void PrismaUIMenu::OnUpdateWhitelistEntryAdvanced(const char* data)
{
    if (!data) {
        spdlog::warn("[PrismaUIMenu::OnUpdateWhitelistEntryAdvanced] Null data");
        return;
    }

    try {
        spdlog::info("[PrismaUIMenu] Updating whitelist entry (advanced), data: {}", data);
        std::string jsonStr(data);

        auto getValue = [&jsonStr](const std::string& key) -> std::string {
            std::string searchKey = "\"" + key + "\":";
            size_t pos = jsonStr.find(searchKey);
            if (pos == std::string::npos) return "";
            pos += searchKey.length();
            while (pos < jsonStr.length() && (jsonStr[pos] == ' ' || jsonStr[pos] == '\t')) pos++;
            if (pos >= jsonStr.length()) return "";
            if (jsonStr[pos] == '"') {
                pos++;
                size_t endPos = jsonStr.find('"', pos);
                if (endPos == std::string::npos) return "";
                return jsonStr.substr(pos, endPos - pos);
            } else {
                size_t endPos = jsonStr.find_first_of(",}", pos);
                if (endPos == std::string::npos) endPos = jsonStr.length();
                return jsonStr.substr(pos, endPos - pos);
            }
        };

        int64_t entryId = std::stoll(getValue("id"));
        std::string notes = getValue("notes");

        // Unescape notes
        size_t escapePos = 0;
        while ((escapePos = notes.find("\\n", escapePos)) != std::string::npos) {
            notes.replace(escapePos, 2, "\n");
            escapePos += 1;
        }

        // Parse actorFilterFormIDs: ["0x...", ...]
        auto parseFormIDs = [&jsonStr]() -> std::vector<uint32_t> {
            std::vector<uint32_t> formIDs;
            size_t arrayStart = jsonStr.find("\"actorFilterFormIDs\":[\"0x");
            if (arrayStart == std::string::npos) return formIDs;
            size_t pos = arrayStart + 24;
            while (pos < jsonStr.size()) {
                size_t hexEnd = jsonStr.find('"', pos);
                if (hexEnd == std::string::npos) break;
                std::string hexStr = jsonStr.substr(pos, hexEnd - pos);
                try { formIDs.push_back(std::stoul(hexStr, nullptr, 16)); } catch (...) {}
                pos = jsonStr.find("\"0x", hexEnd);
                if (pos == std::string::npos) break;
                pos += 3;
            }
            return formIDs;
        };

        // Parse actorFilterNames
        auto parseNames = [&jsonStr]() -> std::vector<std::string> {
            std::vector<std::string> names;
            size_t arrayStart = jsonStr.find("\"actorFilterNames\":");
            if (arrayStart == std::string::npos) return names;
            size_t pos = jsonStr.find('"', arrayStart + 20);
            while (pos != std::string::npos && pos < jsonStr.size()) {
                if (jsonStr[pos] != '"') break;
                pos++;
                size_t nameEnd = jsonStr.find('"', pos);
                if (nameEnd == std::string::npos) break;
                names.push_back(jsonStr.substr(pos, nameEnd - pos));
                pos = jsonStr.find("\",\"", nameEnd);
                if (pos == std::string::npos) break;
                pos += 3;
            }
            return names;
        };

        // Parse factionFilterEditorIDs
        auto parseFactions = [&jsonStr]() -> std::vector<std::string> {
            std::vector<std::string> ids;
            size_t arrayStart = jsonStr.find("\"factionFilterEditorIDs\":");
            if (arrayStart == std::string::npos) return ids;
            size_t bracketPos = jsonStr.find('[', arrayStart);
            if (bracketPos == std::string::npos) return ids;
            size_t pos = bracketPos + 1;
            while (pos < jsonStr.size()) {
                while (pos < jsonStr.size() && (jsonStr[pos] == ' ' || jsonStr[pos] == '\t')) pos++;
                if (pos >= jsonStr.size() || jsonStr[pos] == ']') break;
                if (jsonStr[pos] != '"') { pos++; continue; }
                pos++;
                size_t end = jsonStr.find('"', pos);
                if (end == std::string::npos) break;
                ids.push_back(jsonStr.substr(pos, end - pos));
                pos = end + 1;
                while (pos < jsonStr.size() && jsonStr[pos] != ',' && jsonStr[pos] != ']') pos++;
                if (pos < jsonStr.size() && jsonStr[pos] == ',') pos++;
            }
            return ids;
        };

        auto* db = DialogueDB::GetDatabase();
        if (!db) {
            spdlog::error("[PrismaUIMenu] Database not available");
            return;
        }

        auto allEntries = db->GetWhitelist();
        DialogueDB::BlacklistEntry* existingEntry = nullptr;
        for (auto& entry : allEntries) {
            if (entry.id == entryId) {
                existingEntry = &entry;
                break;
            }
        }

        if (!existingEntry) {
            spdlog::error("[PrismaUIMenu] Whitelist entry {} not found", entryId);
            return;
        }

        existingEntry->notes = notes;
        existingEntry->actorFilterFormIDs = parseFormIDs();
        existingEntry->actorFilterNames = parseNames();
        existingEntry->factionFilterEditorIDs = parseFactions();

        if (db->AddToWhitelist(*existingEntry)) {
            spdlog::info("[PrismaUIMenu] Successfully updated whitelist entry (advanced) {}", entryId);
            SendWhitelistData();
        } else {
            spdlog::error("[PrismaUIMenu] Failed to update whitelist entry (advanced) {}", entryId);
        }
    } catch (const std::exception& e) {
        spdlog::error("[PrismaUIMenu::OnUpdateWhitelistEntryAdvanced] Exception: {}", e.what());
    }
}

void PrismaUIMenu::OnRemoveWhitelistBatch(const char* data)
{
    if (!data) {
        spdlog::error("[PrismaUIMenu::OnRemoveWhitelistBatch] Null data received");
        return;
    }
    
    try {
        spdlog::info("[PrismaUIMenu::OnRemoveWhitelistBatch] Received data: {}", data);
        
        std::string jsonStr(data);
        std::vector<int64_t> entryIds;
        
        // Parse JSON to extract ids array
        // Expected format: {"ids": [1, 2, 3, ...]}
        size_t idsPos = jsonStr.find("\"ids\"");
        if (idsPos == std::string::npos) {
            spdlog::error("[PrismaUIMenu::OnRemoveWhitelistBatch] 'ids' field not found in JSON");
            return;
        }
        
        // Find the array start bracket
        size_t arrayStart = jsonStr.find("[", idsPos);
        if (arrayStart == std::string::npos) {
            spdlog::error("[PrismaUIMenu::OnRemoveWhitelistBatch] Array start bracket not found");
            return;
        }
        
        // Find the array end bracket
        size_t arrayEnd = jsonStr.find("]", arrayStart);
        if (arrayEnd == std::string::npos) {
            spdlog::error("[PrismaUIMenu::OnRemoveWhitelistBatch] Array end bracket not found");
            return;
        }
        
        // Extract array content
        std::string arrayContent = jsonStr.substr(arrayStart + 1, arrayEnd - arrayStart - 1);
        
        // Parse comma-separated values
        size_t pos = 0;
        while (pos < arrayContent.length()) {
            // Skip whitespace
            while (pos < arrayContent.length() && (arrayContent[pos] == ' ' || arrayContent[pos] == '\t' || arrayContent[pos] == '\n')) {
                pos++;
            }
            
            // Find next number
            size_t numStart = pos;
            while (pos < arrayContent.length() && arrayContent[pos] >= '0' && arrayContent[pos] <= '9') {
                pos++;
            }
            
            if (pos > numStart) {
                std::string numStr = arrayContent.substr(numStart, pos - numStart);
                entryIds.push_back(std::stoll(numStr));
            }
            
            // Skip comma
            if (pos < arrayContent.length() && arrayContent[pos] == ',') {
                pos++;
            }
        }
        
        spdlog::info("[PrismaUIMenu::OnRemoveWhitelistBatch] Parsed {} IDs", entryIds.size());
        
        auto* db = DialogueDB::GetDatabase();
        if (!db) {
            spdlog::error("[PrismaUIMenu::OnRemoveWhitelistBatch] Database not available");
            return;
        }
        
        // Remove each entry
        int removedCount = 0;
        for (int64_t id : entryIds) {
            if (db->RemoveFromWhitelist(id)) {
                spdlog::info("[PrismaUIMenu::OnRemoveWhitelistBatch] Removed entry ID: {}", id);
                removedCount++;
            } else {
                spdlog::warn("[PrismaUIMenu::OnRemoveWhitelistBatch] Failed to remove entry ID: {}", id);
            }
        }
        
        spdlog::info("[PrismaUIMenu::OnRemoveWhitelistBatch] Successfully removed {} of {} entries",
                    removedCount, entryIds.size());
        
        // Refresh UI
        SendWhitelistData();
        SendHistoryData();
        
    } catch (const std::exception& e) {
        spdlog::error("[PrismaUIMenu::OnRemoveWhitelistBatch] Exception: {}", e.what());
    }
}
