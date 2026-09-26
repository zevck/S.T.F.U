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

void PrismaUIMenu::OnRequestBlacklist(const char* data)
{
    spdlog::trace("[PrismaUIMenu] Blacklist data requested");
    SendBlacklistData();
}

void PrismaUIMenu::OnDeleteBlacklistEntry(const char* data)
{
    if (!data) {
        spdlog::warn("[PrismaUIMenu::OnDeleteBlacklistEntry] Null data");
        return;
    }
    
    try {
        int64_t entryId = std::stoll(data);
        spdlog::info("[PrismaUIMenu] Deleting blacklist entry: {}", entryId);
        
        auto* db = DialogueDB::GetDatabase();
        if (db && db->RemoveFromBlacklist(entryId)) {
            spdlog::info("[PrismaUIMenu] Successfully deleted blacklist entry {}", entryId);
            SendBlacklistData();  // Refresh the data
        } else {
            spdlog::error("[PrismaUIMenu] Failed to delete blacklist entry {}", entryId);
        }
    } catch (const std::exception& e) {
        spdlog::error("[PrismaUIMenu::OnDeleteBlacklistEntry] Exception: {}", e.what());
    }
}

void PrismaUIMenu::OnDeleteBlacklistBatch(const char* data)
{
    if (!data) {
        spdlog::warn("[PrismaUIMenu::OnDeleteBlacklistBatch] Null data");
        return;
    }
    
    try {
        // Parse comma-separated IDs
        std::vector<int64_t> ids;
        std::string dataStr(data);
        std::istringstream ss(dataStr);
        std::string token;
        
        while (std::getline(ss, token, ',')) {
            if (!token.empty()) {
                ids.push_back(std::stoll(token));
            }
        }
        
        spdlog::info("[PrismaUIMenu] Deleting {} blacklist entries", ids.size());
        
        auto* db = DialogueDB::GetDatabase();
        if (db) {
            int removedCount = db->RemoveFromBlacklistBatch(ids);
            spdlog::info("[PrismaUIMenu] Successfully deleted {} blacklist entries", removedCount);
            SendBlacklistData();  // Refresh the data
        }
    } catch (const std::exception& e) {
        spdlog::error("[PrismaUIMenu::OnDeleteBlacklistBatch] Exception: {}", e.what());
    }
}

void PrismaUIMenu::OnClearBlacklist(const char* data)
{
    spdlog::info("[PrismaUIMenu] Clearing all blacklist entries");
    
    try {
        auto* db = DialogueDB::GetDatabase();
        if (db) {
            int removedCount = db->ClearBlacklist();
            spdlog::info("[PrismaUIMenu] Successfully cleared {} blacklist entries", removedCount);
            SendBlacklistData();  // Refresh the data
        }
    } catch (const std::exception& e) {
        spdlog::error("[PrismaUIMenu::OnClearBlacklist] Exception: {}", e.what());
    }
}

void PrismaUIMenu::OnRefreshBlacklist(const char* data)
{
    spdlog::info("[PrismaUIMenu] Refreshing blacklist data");
    SendBlacklistData();
}

void PrismaUIMenu::OnUpdateBlacklistEntry(const char* data)
{
    if (!data) {
        spdlog::warn("[PrismaUIMenu::OnUpdateBlacklistEntry] Null data");
        return;
    }
    
    try {
        // Parse JSON: {"id":123,"blockType":"Soft","filterCategory":"Blacklist","notes":"Comment"}
        spdlog::info("[PrismaUIMenu] Updating blacklist entry, data: {}", data);
        
        std::string jsonStr(data);
        
        // Simple JSON parsing (hacky but sufficient for our controlled data)
        auto getValue = [&jsonStr](const std::string& key) -> std::string {
            std::string searchKey = "\"" + key + "\":";
            size_t pos = jsonStr.find(searchKey);
            if (pos == std::string::npos) return "";
            
            pos += searchKey.length();
            // Skip whitespace
            while (pos < jsonStr.length() && (jsonStr[pos] == ' ' || jsonStr[pos] == '\t')) pos++;
            
            if (pos >= jsonStr.length()) return "";
            
            // Check if it's a string (starts with quote)
            if (jsonStr[pos] == '"') {
                pos++; // Skip opening quote
                size_t endPos = jsonStr.find('"', pos);
                if (endPos == std::string::npos) return "";
                return jsonStr.substr(pos, endPos - pos);
            } else {
                // It's a number or other value
                size_t endPos = jsonStr.find_first_of(",}", pos);
                if (endPos == std::string::npos) endPos = jsonStr.length();
                return jsonStr.substr(pos, endPos - pos);
            }
        };
        
        int64_t entryId = std::stoll(getValue("id"));
        std::string blockTypeStr = getValue("blockType");
        std::string filterCategory = getValue("filterCategory");
        std::string notes = getValue("notes");
        
        // Unescape notes (\n -> newline)
        size_t escapePos = 0;
        while ((escapePos = notes.find("\\n", escapePos)) != std::string::npos) {
            notes.replace(escapePos, 2, "\n");
            escapePos += 1;
        }
        
        spdlog::info("[PrismaUIMenu] Parsed: id={}, blockType={}, filterCategory={}, notes={}",
                    entryId, blockTypeStr, filterCategory, notes);
        
        auto* db = DialogueDB::GetDatabase();
        if (!db) {
            spdlog::error("[PrismaUIMenu] Database not available");
            return;
        }
        
        // Get the existing entry
        auto allEntries = db->GetBlacklist();
        DialogueDB::BlacklistEntry* existingEntry = nullptr;
        for (auto& entry : allEntries) {
            if (entry.id == entryId) {
                existingEntry = &entry;
                break;
            }
        }
        
        if (!existingEntry) {
            spdlog::error("[PrismaUIMenu] Blacklist entry {} not found", entryId);
            return;
        }
        
        // Update the fields
        if (blockTypeStr == "Soft") {
            existingEntry->blockType = DialogueDB::BlockType::Soft;
            existingEntry->blockAudio = true;
            existingEntry->blockSubtitles = true;
            existingEntry->blockSkyrimNet = true;
        } else if (blockTypeStr == "Hard") {
            existingEntry->blockType = DialogueDB::BlockType::Hard;
            existingEntry->blockAudio = true;
            existingEntry->blockSubtitles = true;
            existingEntry->blockSkyrimNet = true;
        } else if (blockTypeStr == "SkyrimNet") {
            existingEntry->blockType = DialogueDB::BlockType::SkyrimNet;
            existingEntry->blockAudio = false;
            existingEntry->blockSubtitles = false;
            existingEntry->blockSkyrimNet = true;
        }
        
        existingEntry->filterCategory = filterCategory;
        existingEntry->notes = notes;
        
        // Parse actor filters from JSON if present
        auto parseActorFormIDsFromUpdate = [&jsonStr]() -> std::vector<uint32_t> {
            std::vector<uint32_t> formIDs;
            // Search string is 25 chars: "actorFilterFormIDs":["0x
            size_t arrayStart = jsonStr.find("\"actorFilterFormIDs\":[\"0x");
            if (arrayStart == std::string::npos) return formIDs;
            size_t pos = arrayStart + 25;  // points past the '0x' to the hex digits
            while (pos < jsonStr.size()) {
                size_t hexEnd = jsonStr.find("\"", pos);
                if (hexEnd == std::string::npos) break;
                std::string hexStr = jsonStr.substr(pos, hexEnd - pos);
                try {
                    uint32_t formID = std::stoul(hexStr, nullptr, 16);
                    formIDs.push_back(formID);
                } catch (...) { }
                pos = jsonStr.find("\"0x", hexEnd);
                if (pos == std::string::npos) break;
                pos += 3;
            }
            return formIDs;
        };
        
        auto parseActorNamesFromUpdate = [&jsonStr]() -> std::vector<std::string> {
            std::vector<std::string> names;
            size_t arrayStart = jsonStr.find("\"actorFilterNames\":[");
            if (arrayStart == std::string::npos) return names;
            // Early exit for empty array
            if (arrayStart + 20 < jsonStr.size() && jsonStr[arrayStart + 20] == ']') return names;
            size_t pos = jsonStr.find("\"", arrayStart + 20);
            while (pos != std::string::npos && pos < jsonStr.size()) {
                if (jsonStr[pos] != '\"') break;
                pos++;
                size_t nameEnd = jsonStr.find("\"", pos);
                if (nameEnd == std::string::npos) break;
                names.push_back(jsonStr.substr(pos, nameEnd - pos));
                pos = jsonStr.find("\",\"", nameEnd);
                if (pos == std::string::npos) break;
                pos += 3;
            }
            return names;
        };
        
        existingEntry->actorFilterFormIDs = parseActorFormIDsFromUpdate();
        existingEntry->actorFilterNames = parseActorNamesFromUpdate();

        // Parse faction EditorIDs from JSON array: "factionFilterEditorIDs":["FactionA","FactionB"]
        auto parseFactionEditorIDsFromUpdate = [&jsonStr]() -> std::vector<std::string> {
            std::vector<std::string> ids;
            size_t arrayStart = jsonStr.find("\"factionFilterEditorIDs\":");
            if (arrayStart == std::string::npos) return ids;
            size_t bracketPos = jsonStr.find('[', arrayStart);
            if (bracketPos == std::string::npos) return ids;
            size_t pos = bracketPos + 1;
            while (pos < jsonStr.size()) {
                // Skip whitespace
                while (pos < jsonStr.size() && (jsonStr[pos] == ' ' || jsonStr[pos] == '\t')) pos++;
                if (pos >= jsonStr.size() || jsonStr[pos] == ']') break;
                if (jsonStr[pos] != '"') { pos++; continue; }
                pos++; // skip opening quote
                size_t end = jsonStr.find('"', pos);
                if (end == std::string::npos) break;
                ids.push_back(jsonStr.substr(pos, end - pos));
                pos = end + 1;
                // Skip to next element or end
                while (pos < jsonStr.size() && jsonStr[pos] != ',' && jsonStr[pos] != ']') pos++;
                if (pos < jsonStr.size() && jsonStr[pos] == ',') pos++;
            }
            return ids;
        };
        existingEntry->factionFilterEditorIDs = parseFactionEditorIDsFromUpdate();

        // Save to database (AddToBlacklist handles both insert and update)
        if (db->AddToBlacklist(*existingEntry)) {
            spdlog::info("[PrismaUIMenu] Successfully updated blacklist entry {}", entryId);
            // TODO: Invalidate cache - need to expose Config::InvalidateTopicCache
            SendBlacklistData();  // Refresh the UI
        } else {
            spdlog::error("[PrismaUIMenu] Failed to update blacklist entry {}", entryId);
        }
    } catch (const std::exception& e) {
        spdlog::error("[PrismaUIMenu::OnUpdateBlacklistEntry] Exception: {}", e.what());
    }
}

void PrismaUIMenu::SendBlacklistData()
{
    if (!initialized_ || !prismaUI_ || !prismaUI_->IsValid(view_)) {
        spdlog::warn("[PrismaUIMenu::SendBlacklistData] Not initialized or invalid view");
        return;
    }
    
    try {
        std::string jsonData = SerializeBlacklistToJSON();
        spdlog::info("[PrismaUIMenu] Sending {} bytes of blacklist data", jsonData.size());
        
        std::string jsCode = BuildSKSEUpdateScript("updateBlacklist", jsonData);
        prismaUI_->Invoke(view_, jsCode.c_str());
        
        spdlog::debug("[PrismaUIMenu] Successfully invoked updateBlacklist");
    } catch (const std::exception& e) {
        spdlog::error("[PrismaUIMenu] Failed to send blacklist data: {}", e.what());
    }
}

std::string PrismaUIMenu::SerializeBlacklistToJSON()
{
    auto* db = DialogueDB::GetDatabase();
    if (!db) {
        return "[]";
    }
    
    // Get all blacklist entries
    auto blacklist = db->GetBlacklist();
    
    std::ostringstream json;
    json << "[";
    
    bool first = true;
    for (const auto& entry : blacklist) {
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
        
        // Add block type
        std::string blockTypeStr;
        switch (entry.blockType) {
            case DialogueDB::BlockType::Soft: blockTypeStr = "Soft Block"; break;
            case DialogueDB::BlockType::Hard: blockTypeStr = "Hard Block"; break;
            case DialogueDB::BlockType::SkyrimNet: blockTypeStr = "SkyrimNet Block"; break;
            default: blockTypeStr = "Unknown"; break;
        }
        json << "\"blockType\":\"" << blockTypeStr << "\",";
        
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
        
        // Add responseText (already  JSON-encoded array of responses)
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
        json << "]";
        
        json << "}";
    }
    
    json << "]";
    
    return json.str();
}

void PrismaUIMenu::OnAddToBlacklist(const char* data)
{
    if (!data) {
        spdlog::error("[PrismaUIMenu::OnAddToBlacklist] Null data received");
        return;
    }
    
    spdlog::info("[PrismaUIMenu::OnAddToBlacklist] Received data");
    
    try {
        auto* db = DialogueDB::GetDatabase();
        if (!db) {
            spdlog::error("[PrismaUIMenu::OnAddToBlacklist] Database not available");
            return;
        }
        
        // Parse JSON data: {"entries":[{...}],"blockType":"Soft|Hard|SkyrimNet"}
        std::string jsonStr(data);
        
        // Extract block type
        size_t blockTypePos = jsonStr.find("\"blockType\":\"");
        if (blockTypePos == std::string::npos) {
            spdlog::error("[PrismaUIMenu::OnAddToBlacklist] blockType not found");
            return;
        }
        
        size_t blockTypeStart = blockTypePos + 13;
        size_t blockTypeEnd = jsonStr.find("\"", blockTypeStart);
        std::string blockTypeStr = jsonStr.substr(blockTypeStart, blockTypeEnd - blockTypeStart);
        
        DialogueDB::BlockType blockType;
        if (blockTypeStr == "Hard") {
            blockType = DialogueDB::BlockType::Hard;
        } else if (blockTypeStr == "SkyrimNet") {
            blockType = DialogueDB::BlockType::SkyrimNet;
        } else {
            blockType = DialogueDB::BlockType::Soft;
        }
        
        spdlog::info("[PrismaUIMenu::OnAddToBlacklist] Block type: {}", blockTypeStr);
        
        // Extract optional notes and filterCategory
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
        std::string userFilterCategory = extractOptionalString("filterCategory");
        
        spdlog::info("[PrismaUIMenu::OnAddToBlacklist] Notes: '{}', FilterCategory: '{}'", userNotes, userFilterCategory);
        
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
        
        // Find all entry objects by looking for topicEditorID fields (each entry has one)
        std::vector<DialogueDB::BlacklistEntry> entriesToAdd;
        size_t searchPos = 0;
        
        while (true) {
            // Find next topicEditorID field
            size_t topicPos = jsonStr.find("\"topicEditorID\":\"", searchPos);
            if (topicPos == std::string::npos) break;
            
            // Find the start of this object (search backwards for {)
            size_t objStart = jsonStr.rfind("{", topicPos);
            if (objStart == std::string::npos) break;
            
            // Find the end of this object (find matching })
            // This should work because we're looking for the } that closes this specific entry
            size_t objEnd = jsonStr.find("},", topicPos);
            if (objEnd == std::string::npos) {
                // Might be the last entry, check for }]
                objEnd = jsonStr.find("}]", topicPos);
                if (objEnd == std::string::npos) break;
            }
            
            // Extract this entry's JSON substring
            std::string entryStr = jsonStr.substr(objStart, objEnd - objStart + 1);
            
            // Parse entry fields (matching STFUMenu's logic)
            DialogueDB::BlacklistEntry entry;
            
            // Check if this is a scene (matching STFUMenu line 1012)
            bool isScene = (entryStr.find("\"isScene\":true") != std::string::npos);
            bool isBardSong = (entryStr.find("\"isBardSong\":true") != std::string::npos);
            
            // Set target type based on scene status (matching STFUMenu line 1012)
            entry.targetType = (isScene || isBardSong) ? DialogueDB::BlacklistTarget::Scene : DialogueDB::BlacklistTarget::Topic;
            
            // Extract EditorIDs (scenes use sceneEditorID, topics use topicEditorID)
            if (isScene || isBardSong) {
                entry.targetEditorID = extractString(entryStr, "sceneEditorID");
                entry.targetFormID = 0;  // Scenes use EditorID, not FormID
                entry.questEditorID = "";  // Scenes don't have quest context
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
                        spdlog::warn("[PrismaUIMenu::OnAddToBlacklist] Failed to parse FormID: {}", formIDStr);
                    }
                }
            }
            
            entry.sourcePlugin = extractString(entryStr, "sourcePlugin");
            entry.subtypeName = extractString(entryStr, "subtypeName");
            
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
            
            // Set fields (matching STFUMenu logic from lines 1032-1049)
            entry.blockType = blockType;
            entry.addedTimestamp = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count();
            
            // Set granular blocking (matching STFUMenu logic)
            if (blockType == DialogueDB::BlockType::SkyrimNet) {
                entry.blockAudio = false;
                entry.blockSubtitles = false;
                entry.blockSkyrimNet = true;
                entry.filterCategory = "SkyrimNet";
            } else {
                entry.blockAudio = true;
                entry.blockSubtitles = true;
                entry.blockSkyrimNet = (blockType == DialogueDB::BlockType::Soft);
                // Use user-provided filter category if available, otherwise default to Blacklist
                if (!userFilterCategory.empty()) {
                    entry.filterCategory = userFilterCategory;
                } else {
                    entry.filterCategory = "Blacklist";
                }
            }
            
            entriesToAdd.push_back(entry);
            
            // Move to next entry
            searchPos = objEnd + 1;
        }
        
        spdlog::info("[PrismaUIMenu::OnAddToBlacklist] Parsed {} entries from JSON", entriesToAdd.size());
        
        // Use batch add for efficiency (same as STFUMenu)
        int addedCount = db->AddToBlacklistBatch(entriesToAdd, false);
        
        spdlog::info("[PrismaUIMenu::OnAddToBlacklist] Successfully added {} of {} entries", 
            addedCount, entriesToAdd.size());
        
        // Refresh UI (same as STFUMenu)
        SendBlacklistData();
        SendWhitelistData();
        SendHistoryData();
        
    } catch (const std::exception& e) {
        spdlog::error("[PrismaUIMenu::OnAddToBlacklist] Exception: {}", e.what());
    }
}

void PrismaUIMenu::OnRemoveFromBlacklist(const char* data)
{
    if (!data) {
        spdlog::error("[PrismaUIMenu::OnRemoveFromBlacklist] Null data received");
        return;
    }
    
    spdlog::info("[PrismaUIMenu::OnRemoveFromBlacklist] Received: {}", data);
    
    try {
        auto* db = DialogueDB::GetDatabase();
        if (!db) {
            spdlog::error("[PrismaUIMenu::OnRemoveFromBlacklist] Database not available");
            return;
        }
        
        // Parse JSON: {"topicFormID":"...", "topicEditorID":"...", "sceneEditorID":"...", "isScene":bool}
        std::string jsonStr(data);
        
        // Helper to extract JSON string value
        auto extractString = [](const std::string& json, const std::string& key) -> std::string {
            std::string searchKey = "\"" + key + "\":\"";
            size_t keyPos = json.find(searchKey);
            if (keyPos == std::string::npos) return "";
            size_t valueStart = keyPos + searchKey.length();
            size_t valueEnd = json.find("\"", valueStart);
            if (valueEnd == std::string::npos) return "";
            return json.substr(valueStart, valueEnd - valueStart);
        };
        
        // Extract fields
        std::string topicFormIDStr = extractString(jsonStr, "topicFormID");
        std::string topicEditorID = extractString(jsonStr, "topicEditorID");
        std::string sceneEditorID = extractString(jsonStr, "sceneEditorID");
        bool isScene = (jsonStr.find("\"isScene\":true") != std::string::npos);
        
        spdlog::info("[PrismaUIMenu::OnRemoveFromBlacklist] Looking for: topicEditorID='{}', topicFormIDStr='{}', sceneEditorID='{}', isScene={}", 
            topicEditorID, topicFormIDStr, sceneEditorID, isScene);
        
        // Find the blacklist entry
        int64_t blacklistId = -1;
        
        if (isScene && !sceneEditorID.empty()) {
            // Look for scene entry
            auto blacklist = db->GetBlacklist();
            for (const auto& entry : blacklist) {
                if (entry.targetType == DialogueDB::BlacklistTarget::Scene &&
                    entry.targetEditorID == sceneEditorID) {
                    blacklistId = entry.id;
                    break;
                }
            }
        } else if (!topicEditorID.empty() || !topicFormIDStr.empty()) {
            // Look for topic entry - parse FormID
            uint32_t topicFormID = 0;
            if (!topicFormIDStr.empty()) {
                try {
                    topicFormID = std::stoul(topicFormIDStr, nullptr, 16);
                    spdlog::info("[PrismaUIMenu::OnRemoveFromBlacklist] Parsed topicFormID: 0x{:08X}", topicFormID);
                } catch (const std::exception& e) {
                    spdlog::warn("[PrismaUIMenu::OnRemoveFromBlacklist] Failed to parse FormID '{}': {}", topicFormIDStr, e.what());
                }
            }
            
            spdlog::info("[PrismaUIMenu::OnRemoveFromBlacklist] Calling GetBlacklistEntryId with FormID=0x{:08X}, EditorID='{}'", topicFormID, topicEditorID);
            blacklistId = db->GetBlacklistEntryId(topicFormID, topicEditorID);
            spdlog::info("[PrismaUIMenu::OnRemoveFromBlacklist] GetBlacklistEntryId returned: {}", blacklistId);
        } else {
            spdlog::warn("[PrismaUIMenu::OnRemoveFromBlacklist] No valid identifiers provided (topicEditorID and topicFormIDStr both empty)");
        }
        
        if (blacklistId > 0) {
            if (db->RemoveFromBlacklist(blacklistId)) {
                spdlog::info("[PrismaUIMenu::OnRemoveFromBlacklist] Successfully removed entry ID: {}", blacklistId);
                
                // Refresh displays
                SendBlacklistData();
                SendHistoryData();
            } else {
                spdlog::error("[PrismaUIMenu::OnRemoveFromBlacklist] Failed to remove entry ID: {}", blacklistId);
            }
        } else {
            spdlog::warn("[PrismaUIMenu::OnRemoveFromBlacklist] Entry not found in blacklist");
        }
        
    } catch (const std::exception& e) {
        spdlog::error("[PrismaUIMenu::OnRemoveFromBlacklist] Exception: {}", e.what());
    }
}

void PrismaUIMenu::OnMoveToBlacklist(const char* data)
{
    if (!data) {
        spdlog::error("[PrismaUIMenu::OnMoveToBlacklist] Null data received");
        return;
    }
    
    spdlog::info("[PrismaUIMenu::OnMoveToBlacklist] Received: {}", data);
    
    try {
        auto* db = DialogueDB::GetDatabase();
        if (!db) {
            spdlog::error("[PrismaUIMenu::OnMoveToBlacklist] Database not available");
            return;
        }
        
        // Parse JSON: {"id":123}
        std::string jsonStr(data);
        
        // Extract id
        size_t idPos = jsonStr.find("\"id\":");
        if (idPos == std::string::npos) {
            spdlog::error("[PrismaUIMenu::OnMoveToBlacklist] 'id' field not found");
            return;
        }
        
        size_t valueStart = idPos + 5;
        while (valueStart < jsonStr.length() && (jsonStr[valueStart] == ' ' || jsonStr[valueStart] == '\t')) {
            valueStart++;
        }
        
        size_t valueEnd = jsonStr.find_first_of(",}", valueStart);
        std::string idStr = jsonStr.substr(valueStart, valueEnd - valueStart);
        int64_t whitelistId = std::stoll(idStr);
        
        spdlog::info("[PrismaUIMenu::OnMoveToBlacklist] Moving whitelist entry ID {} to blacklist", whitelistId);
        
        // Find the whitelist entry
        auto whitelist = db->GetWhitelist();
        DialogueDB::BlacklistEntry* entryToMove = nullptr;
        for (auto& entry : whitelist) {
            if (entry.id == whitelistId) {
                entryToMove = &entry;
                break;
            }
        }
        
        if (!entryToMove) {
            spdlog::error("[PrismaUIMenu::OnMoveToBlacklist] Whitelist entry {} not found", whitelistId);
            return;
        }
        
        // Create blacklist entry from whitelist entry
        DialogueDB::BlacklistEntry blacklistEntry = *entryToMove;
        blacklistEntry.id = 0; // Reset ID for new entry
        blacklistEntry.blockType = DialogueDB::BlockType::Soft; // Default to soft block
        blacklistEntry.blockAudio = true;
        blacklistEntry.blockSubtitles = true;
        blacklistEntry.blockSkyrimNet = true;
        blacklistEntry.addedTimestamp = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
        
        // Add to blacklist
        bool addSuccess = db->AddToBlacklist(blacklistEntry);
        
        if (addSuccess) {
            // Remove from whitelist
            bool removeSuccess = db->RemoveFromWhitelist(whitelistId);
            
            if (removeSuccess) {
                spdlog::info("[PrismaUIMenu::OnMoveToBlacklist] Successfully moved entry to blacklist");
                
                // Refresh both lists
                SendWhitelistData();
                SendBlacklistData();
                SendHistoryData();
            } else {
                spdlog::error("[PrismaUIMenu::OnMoveToBlacklist] Failed to remove from whitelist after adding to blacklist");
            }
        } else {
            spdlog::error("[PrismaUIMenu::OnMoveToBlacklist] Failed to add to blacklist");
        }
        
    } catch (const std::exception& e) {
        spdlog::error("[PrismaUIMenu::OnMoveToBlacklist] Exception: {}", e.what());
    }
}
