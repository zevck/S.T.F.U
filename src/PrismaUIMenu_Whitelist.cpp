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

        int64_t entryId = std::stoll(ExtractJsonValue(jsonStr, "id"));
        std::string notes = ExtractJsonValue(jsonStr, "notes");

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
        existingEntry->actorFilterFormIDs = ParseHexFormIDs(ExtractJsonStringArray(jsonStr, "actorFilterFormIDs"));
        existingEntry->actorFilterNames = ExtractJsonStringArray(jsonStr, "actorFilterNames");
        existingEntry->factionFilterEditorIDs = ExtractJsonStringArray(jsonStr, "factionFilterEditorIDs");

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
