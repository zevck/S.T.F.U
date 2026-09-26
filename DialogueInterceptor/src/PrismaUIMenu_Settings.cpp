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

void PrismaUIMenu::OnToggleSubtypeFilter(const char* data)
{
    if (!data) {
        spdlog::error("[PrismaUIMenu::OnToggleSubtypeFilter] Null data received");
        return;
    }
    
    try {
        spdlog::info("[PrismaUIMenu::OnToggleSubtypeFilter] Received data: {}", data);
        
        std::string jsonStr(data);
        
        // Parse JSON to extract topicSubtype
        // Expected format: {"topicSubtype": 5}
        size_t subtypePos = jsonStr.find("\"topicSubtype\"");
        if (subtypePos == std::string::npos) {
            spdlog::error("[PrismaUIMenu::OnToggleSubtypeFilter] 'topicSubtype' field not found in JSON");
            return;
        }
        
        // Find the colon after "topicSubtype"
        size_t colonPos = jsonStr.find(":", subtypePos);
        if (colonPos == std::string::npos) {
            spdlog::error("[PrismaUIMenu::OnToggleSubtypeFilter] Malformed JSON - no colon after topicSubtype");
            return;
        }
        
        // Extract the numeric value
        size_t numStart = colonPos + 1;
        while (numStart < jsonStr.length() && (jsonStr[numStart] == ' ' || jsonStr[numStart] == '\t')) {
            numStart++;
        }
        
        size_t numEnd = numStart;
        while (numEnd < jsonStr.length() && jsonStr[numEnd] >= '0' && jsonStr[numEnd] <= '9') {
            numEnd++;
        }
        
        if (numStart >= numEnd) {
            spdlog::error("[PrismaUIMenu::OnToggleSubtypeFilter] Could not parse topicSubtype value");
            return;
        }
        
        std::string subtypeStr = jsonStr.substr(numStart, numEnd - numStart);
        uint16_t topicSubtype = static_cast<uint16_t>(std::stoi(subtypeStr));
        
        spdlog::info("[PrismaUIMenu::OnToggleSubtypeFilter] Toggling subtype filter for subtype {}", topicSubtype);
        
        // Call Config function to toggle the subtype filter
        bool success = Config::ToggleSubtypeFilter(topicSubtype);
        
        if (success) {
            spdlog::info("[PrismaUIMenu::OnToggleSubtypeFilter] Successfully toggled subtype {} filter", topicSubtype);
            
            // Send updated settings to UI
            SendSettingsData();
            
            // Refresh history to show updated statuses
            std::string historyJSON = SerializeHistoryToJSON();
            if (prismaUI_ && view_) {
                std::string jsCall = BuildSKSEUpdateScript("updateHistory", historyJSON);
                prismaUI_->Invoke(view_, jsCall.c_str());
            }
        } else {
            spdlog::warn("[PrismaUIMenu::OnToggleSubtypeFilter] Failed to toggle subtype {} filter (no MCM global?)", topicSubtype);
        }
        
    } catch (const std::exception& e) {
        spdlog::error("[PrismaUIMenu::OnToggleSubtypeFilter] Exception: {}", e.what());
    }
}

void PrismaUIMenu::OnImportScenes(const char* data)
{
    spdlog::info("[PrismaUIMenu::OnImportScenes] Import hardcoded scenes requested");
    
    try {
        // Import ambient scenes, bard songs, and follower commentary
        auto* db = DialogueDB::GetDatabase();
        if (!db) {
            spdlog::error("[PrismaUIMenu::OnImportScenes] Database not available");
            std::string jsCode = "window.showToast('Failed to import scenes: Database not available', 'error')";
            prismaUI_->Invoke(view_, jsCode.c_str());
            return;
        }
        
        auto scenesList = Config::GetHardcodedScenesList();
        db->ImportHardcodedScenes(scenesList, "Scene");
        spdlog::info("[PrismaUIMenu::OnImportScenes] Imported {} ambient scenes", scenesList.size());
        
        auto bardSongs = Config::GetBardSongQuestsList();
        std::vector<std::string> bardScenes(bardSongs.begin(), bardSongs.end());
        db->ImportHardcodedScenes(bardScenes, "BardSongs");
        spdlog::info("[PrismaUIMenu::OnImportScenes] Imported {} bard song quests", bardScenes.size());
        
        auto followerScenes = Config::GetFollowerCommentaryScenesList();
        db->ImportHardcodedScenes(followerScenes, "FollowerCommentary");
        spdlog::info("[PrismaUIMenu::OnImportScenes] Imported {} follower commentary scenes", followerScenes.size());
        
        spdlog::info("[PrismaUIMenu::OnImportScenes] Scene import completed successfully");
        
        // Show success toast
        std::string jsCode = "window.showToast('Scenes imported successfully', 'success')";
        prismaUI_->Invoke(view_, jsCode.c_str());
        
        // Refresh blacklist UI to show newly imported scenes
        SendBlacklistData();
        SendHistoryData();
        
    } catch (const std::exception& e) {
        spdlog::error("[PrismaUIMenu::OnImportScenes] Exception: {}", e.what());
        std::string jsCode = "window.showToast('Failed to import scenes', 'error')";
        prismaUI_->Invoke(view_, jsCode.c_str());
    }
}

void PrismaUIMenu::OnImportYAML(const char* data)
{
    spdlog::info("[PrismaUIMenu::OnImportYAML] Import from YAML requested");
    
    try {
        auto* db = DialogueDB::GetDatabase();
        if (!db) {
            spdlog::error("[PrismaUIMenu::OnImportYAML] Database not available");
            std::string jsCode = "window.showToast('Failed to import YAML: Database not available', 'error')";
            prismaUI_->Invoke(view_, jsCode.c_str());
            return;
        }
        
        // Check if YAML files exist
        wchar_t buffer[MAX_PATH];
        GetModuleFileNameW(nullptr, buffer, MAX_PATH);
        std::filesystem::path exePath(buffer);
        std::filesystem::path yamlDir = exePath.parent_path() / "Data" / "SKSE" / "Plugins" / "STFU" / "import";
        std::filesystem::path blacklistPath = yamlDir / "STFU_Blacklist.yaml";
        std::filesystem::path whitelistPath = yamlDir / "STFU_Whitelist.yaml";
        
        bool blacklistExists = std::filesystem::exists(blacklistPath);
        bool whitelistExists = std::filesystem::exists(whitelistPath);
        
        if (!blacklistExists && !whitelistExists) {
            spdlog::error("[PrismaUIMenu::OnImportYAML] No YAML files found at {}", yamlDir.string());
            std::string jsCode = "window.showToast('Failed to import YAML: No configuration files found', 'error')";
            prismaUI_->Invoke(view_, jsCode.c_str());
            return;
        }
        
        // Call the existing Config function that handles YAML import
        Config::ImportYAMLToDatabase();
        
        spdlog::info("[PrismaUIMenu::OnImportYAML] YAML import completed successfully");
        
        // Show success toast with details
        std::string message = "YAML config imported successfully";
        if (blacklistExists && whitelistExists) {
            message = "Blacklist and Whitelist imported successfully";
        } else if (blacklistExists) {
            message = "Blacklist imported successfully (no Whitelist found)";
        } else {
            message = "Whitelist imported successfully (no Blacklist found)";
        }
        
        std::string jsCode = "window.showToast('" + message + "', 'success')";
        prismaUI_->Invoke(view_, jsCode.c_str());
        
        // Refresh all UI data to show newly imported entries
        SendBlacklistData();
        SendWhitelistData();
        SendHistoryData();
        
    } catch (const std::exception& e) {
        spdlog::error("[PrismaUIMenu::OnImportYAML] Exception: {}", e.what());
        std::string jsCode = "window.showToast('Failed to import YAML config', 'error')";
        prismaUI_->Invoke(view_, jsCode.c_str());
    }
}

void PrismaUIMenu::OnSetCombatGruntsBlocked(const char* data)
{
    spdlog::info("[PrismaUIMenu::OnSetCombatGruntsBlocked] Request received");
    
    if (!data || data[0] == '\0') {
        spdlog::error("[PrismaUIMenu::OnSetCombatGruntsBlocked] Null or empty data received");
        return;
    }
    
    try {
        spdlog::info("[PrismaUIMenu::OnSetCombatGruntsBlocked] Received data: {}", data);
        
        // Parse JSON: {"blocked":true} or {"blocked":false}
        std::string dataStr(data);
        auto blockedPos = dataStr.find("\"blocked\"");
        if (blockedPos == std::string::npos) {
            spdlog::error("[PrismaUIMenu::OnSetCombatGruntsBlocked] 'blocked' field not found in JSON");
            return;
        }
        
        // Find the boolean value after "blocked":
        auto colonPos = dataStr.find(':', blockedPos);
        if (colonPos == std::string::npos) {
            spdlog::error("[PrismaUIMenu::OnSetCombatGruntsBlocked] Malformed JSON - no colon after blocked");
            return;
        }
        
        // Skip whitespace and find true/false
        size_t valueStart = colonPos + 1;
        while (valueStart < dataStr.length() && (dataStr[valueStart] == ' ' || dataStr[valueStart] == '\t')) {
            valueStart++;
        }
        
        bool blocked = false;
        if (dataStr.substr(valueStart, 4) == "true") {
            blocked = true;
        } else if (dataStr.substr(valueStart, 5) == "false") {
            blocked = false;
        } else {
            spdlog::error("[PrismaUIMenu::OnSetCombatGruntsBlocked] Could not parse blocked value");
            return;
        }
        
        spdlog::info("[PrismaUIMenu::OnSetCombatGruntsBlocked] Setting combat grunts blocked to: {}", blocked);
        
        // Get the PreserveGrunts global (inverted logic: 1=preserve/block, 0=filter/allow)
        const auto& settings = Config::GetSettings();
        if (settings.mcm.preserveGruntsGlobal) {
            settings.mcm.preserveGruntsGlobal->value = blocked ? 1.0f : 0.0f;
            spdlog::info("[PrismaUIMenu::OnSetCombatGruntsBlocked] Set STFU_PreserveGrunts global to {}", 
                settings.mcm.preserveGruntsGlobal->value);
            
            // Save to INI
            SettingsPersistence::SaveSettings();
            
            // Clear cache so new setting takes effect immediately
            Config::ClearCache();
            
            // Send updated settings to UI
            SendSettingsData();
            
        } else {
            spdlog::warn("[PrismaUIMenu::OnSetCombatGruntsBlocked] PreserveGrunts global not found");
        }
        
    } catch (const std::exception& e) {
        spdlog::error("[PrismaUIMenu::OnSetCombatGruntsBlocked] Exception: {}", e.what());
    }
}

void PrismaUIMenu::OnSetFollowerCommentaryEnabled(const char* data)
{
    spdlog::info("[PrismaUIMenu::OnSetFollowerCommentaryEnabled] Request received");
    
    if (!data || data[0] == '\0') {
        spdlog::error("[PrismaUIMenu::OnSetFollowerCommentaryEnabled] Null or empty data received");
        return;
    }
    
    try {
        spdlog::info("[PrismaUIMenu::OnSetFollowerCommentaryEnabled] Received data: {}", data);
        
        // Parse JSON: {"enabled":true} or {"enabled":false}
        std::string dataStr(data);
        auto enabledPos = dataStr.find("\"enabled\"");
        if (enabledPos == std::string::npos) {
            spdlog::error("[PrismaUIMenu::OnSetFollowerCommentaryEnabled] 'enabled' field not found in JSON");
            return;
        }
        
        // Find the boolean value after "enabled":
        auto colonPos = dataStr.find(':', enabledPos);
        if (colonPos == std::string::npos) {
            spdlog::error("[PrismaUIMenu::OnSetFollowerCommentaryEnabled] Malformed JSON - no colon after enabled");
            return;
        }
        
        // Skip whitespace and find true/false
        size_t valueStart = colonPos + 1;
        while (valueStart < dataStr.length() && (dataStr[valueStart] == ' ' || dataStr[valueStart] == '\t')) {
            valueStart++;
        }
        
        bool enabled = false;
        if (dataStr.substr(valueStart, 4) == "true") {
            enabled = true;
        } else if (dataStr.substr(valueStart, 5) == "false") {
            enabled = false;
        } else {
            spdlog::error("[PrismaUIMenu::OnSetFollowerCommentaryEnabled] Could not parse enabled value");
            return;
        }
        
        spdlog::info("[PrismaUIMenu::OnSetFollowerCommentaryEnabled] Setting follower commentary enabled to: {}", enabled);
        
        // Get the BlockFollowerCommentary global (inverted: 1=block/disabled, 0=allow/enabled)
        const auto& settings = Config::GetSettings();
        if (settings.mcm.blockFollowerCommentaryGlobal) {
            settings.mcm.blockFollowerCommentaryGlobal->value = enabled ? 0.0f : 1.0f;
            spdlog::info("[PrismaUIMenu::OnSetFollowerCommentaryEnabled] Set STFU_BlockFollowerCommentary global to {}", 
                settings.mcm.blockFollowerCommentaryGlobal->value);
            
            // Save to INI
            SettingsPersistence::SaveSettings();
            
            // Clear cache so new setting takes effect immediately
            Config::ClearCache();
            
            // Send updated settings to UI
            SendSettingsData();
            
        } else {
            spdlog::warn("[PrismaUIMenu::OnSetFollowerCommentaryEnabled] BlockFollowerCommentary global not found");
        }
        
    } catch (const std::exception& e) {
        spdlog::error("[PrismaUIMenu::OnSetFollowerCommentaryEnabled] Exception: {}", e.what());
    }
}

void PrismaUIMenu::OnSetBlacklistEnabled(const char* data)
{
    spdlog::info("[PrismaUIMenu::OnSetBlacklistEnabled] Request received");
    
    if (!data || data[0] == '\0') {
        spdlog::error("[PrismaUIMenu::OnSetBlacklistEnabled] Null or empty data received");
        return;
    }
    
    try {
        spdlog::info("[PrismaUIMenu::OnSetBlacklistEnabled] Received data: {}", data);
        
        // Parse JSON: {"enabled":true} or {"enabled":false}
        std::string dataStr(data);
        auto enabledPos = dataStr.find("\"enabled\"");
        if (enabledPos == std::string::npos) {
            spdlog::error("[PrismaUIMenu::OnSetBlacklistEnabled] 'enabled' field not found in JSON");
            return;
        }
        
        auto colonPos = dataStr.find(':', enabledPos);
        if (colonPos == std::string::npos) {
            spdlog::error("[PrismaUIMenu::OnSetBlacklistEnabled] Malformed JSON - no colon after enabled");
            return;
        }
        
        size_t valueStart = colonPos + 1;
        while (valueStart < dataStr.length() && (dataStr[valueStart] == ' ' || dataStr[valueStart] == '\t')) {
            valueStart++;
        }
        
        bool enabled = false;
        if (dataStr.substr(valueStart, 4) == "true") {
            enabled = true;
        } else if (dataStr.substr(valueStart, 5) == "false") {
            enabled = false;
        } else {
            spdlog::error("[PrismaUIMenu::OnSetBlacklistEnabled] Could not parse enabled value");
            return;
        }
        
        spdlog::info("[PrismaUIMenu::OnSetBlacklistEnabled] Setting blacklist enabled to: {}", enabled);
        
        const auto& settings = Config::GetSettings();
        if (settings.blacklist.toggleGlobal) {
            settings.blacklist.toggleGlobal->value = enabled ? 1.0f : 0.0f;
            spdlog::info("[PrismaUIMenu::OnSetBlacklistEnabled] Set blacklist toggle global to {}", 
                settings.blacklist.toggleGlobal->value);
            
            SettingsPersistence::SaveSettings();
            Config::ClearCache();
            SendSettingsData();
            
        } else {
            spdlog::warn("[PrismaUIMenu::OnSetBlacklistEnabled] Blacklist toggle global not found");
        }
        
    } catch (const std::exception& e) {
        spdlog::error("[PrismaUIMenu::OnSetBlacklistEnabled] Exception: {}", e.what());
    }
}

void PrismaUIMenu::OnSetSkyrimNetEnabled(const char* data)
{
    spdlog::info("[PrismaUIMenu::OnSetSkyrimNetEnabled] Request received");
    
    if (!data || data[0] == '\0') {
        spdlog::error("[PrismaUIMenu::OnSetSkyrimNetEnabled] Null or empty data received");
        return;
    }
    
    try {
        spdlog::info("[PrismaUIMenu::OnSetSkyrimNetEnabled] Received data: {}", data);
        
        std::string dataStr(data);
        auto enabledPos = dataStr.find("\"enabled\"");
        if (enabledPos == std::string::npos) {
            spdlog::error("[PrismaUIMenu::OnSetSkyrimNetEnabled] 'enabled' field not found in JSON");
            return;
        }
        
        auto colonPos = dataStr.find(':', enabledPos);
        if (colonPos == std::string::npos) {
            spdlog::error("[PrismaUIMenu::OnSetSkyrimNetEnabled] Malformed JSON - no colon after enabled");
            return;
        }
        
        size_t valueStart = colonPos + 1;
        while (valueStart < dataStr.length() && (dataStr[valueStart] == ' ' || dataStr[valueStart] == '\t')) {
            valueStart++;
        }
        
        bool enabled = false;
        if (dataStr.substr(valueStart, 4) == "true") {
            enabled = true;
        } else if (dataStr.substr(valueStart, 5) == "false") {
            enabled = false;
        } else {
            spdlog::error("[PrismaUIMenu::OnSetSkyrimNetEnabled] Could not parse enabled value");
            return;
        }
        
        spdlog::info("[PrismaUIMenu::OnSetSkyrimNetEnabled] Setting SkyrimNet enabled to: {}", enabled);
        
        const auto& settings = Config::GetSettings();
        if (settings.skyrimNetFilter.toggleGlobal) {
            settings.skyrimNetFilter.toggleGlobal->value = enabled ? 1.0f : 0.0f;
            spdlog::info("[PrismaUIMenu::OnSetSkyrimNetEnabled] Set SkyrimNet toggle global to {}", 
                settings.skyrimNetFilter.toggleGlobal->value);
            
            SettingsPersistence::SaveSettings();
            Config::ClearCache();
            SendSettingsData();
            
        } else {
            spdlog::warn("[PrismaUIMenu::OnSetSkyrimNetEnabled] SkyrimNet toggle global not found");
        }
        
    } catch (const std::exception& e) {
        spdlog::error("[PrismaUIMenu::OnSetSkyrimNetEnabled] Exception: {}", e.what());
    }
}

void PrismaUIMenu::OnSetScenesEnabled(const char* data)
{
    spdlog::info("[PrismaUIMenu::OnSetScenesEnabled] Request received");
    
    if (!data || data[0] == '\0') {
        spdlog::error("[PrismaUIMenu::OnSetScenesEnabled] Null or empty data received");
        return;
    }
    
    try {
        spdlog::info("[PrismaUIMenu::OnSetScenesEnabled] Received data: {}", data);
        
        std::string dataStr(data);
        auto enabledPos = dataStr.find("\"enabled\"");
        if (enabledPos == std::string::npos) {
            spdlog::error("[PrismaUIMenu::OnSetScenesEnabled] 'enabled' field not found in JSON");
            return;
        }
        
        auto colonPos = dataStr.find(':', enabledPos);
        if (colonPos == std::string::npos) {
            spdlog::error("[PrismaUIMenu::OnSetScenesEnabled] Malformed JSON - no colon after enabled");
            return;
        }
        
        size_t valueStart = colonPos + 1;
        while (valueStart < dataStr.length() && (dataStr[valueStart] == ' ' || dataStr[valueStart] == '\t')) {
            valueStart++;
        }
        
        bool enabled = false;
        if (dataStr.substr(valueStart, 4) == "true") {
            enabled = true;
        } else if (dataStr.substr(valueStart, 5) == "false") {
            enabled = false;
        } else {
            spdlog::error("[PrismaUIMenu::OnSetScenesEnabled] Could not parse enabled value");
            return;
        }
        
        spdlog::info("[PrismaUIMenu::OnSetScenesEnabled] Setting scenes enabled to: {}", enabled);
        
        const auto& settings = Config::GetSettings();
        if (settings.mcm.blockScenesGlobal) {
            settings.mcm.blockScenesGlobal->value = enabled ? 1.0f : 0.0f;
            spdlog::info("[PrismaUIMenu::OnSetScenesEnabled] Set blockScenes global to {}", 
                settings.mcm.blockScenesGlobal->value);
            
            SettingsPersistence::SaveSettings();
            Config::ClearCache();
            SendSettingsData();
            
        } else {
            spdlog::warn("[PrismaUIMenu::OnSetScenesEnabled] BlockScenes global not found");
        }
        
    } catch (const std::exception& e) {
        spdlog::error("[PrismaUIMenu::OnSetScenesEnabled] Exception: {}", e.what());
    }
}

void PrismaUIMenu::OnSetBardSongsEnabled(const char* data)
{
    spdlog::info("[PrismaUIMenu::OnSetBardSongsEnabled] Request received");
    
    if (!data || data[0] == '\0') {
        spdlog::error("[PrismaUIMenu::OnSetBardSongsEnabled] Null or empty data received");
        return;
    }
    
    try {
        spdlog::info("[PrismaUIMenu::OnSetBardSongsEnabled] Received data: {}", data);
        
        std::string dataStr(data);
        auto enabledPos = dataStr.find("\"enabled\"");
        if (enabledPos == std::string::npos) {
            spdlog::error("[PrismaUIMenu::OnSetBardSongsEnabled] 'enabled' field not found in JSON");
            return;
        }
        
        auto colonPos = dataStr.find(':', enabledPos);
        if (colonPos == std::string::npos) {
            spdlog::error("[PrismaUIMenu::OnSetBardSongsEnabled] Malformed JSON - no colon after enabled");
            return;
        }
        
        size_t valueStart = colonPos + 1;
        while (valueStart < dataStr.length() && (dataStr[valueStart] == ' ' || dataStr[valueStart] == '\t')) {
            valueStart++;
        }
        
        bool enabled = false;
        if (dataStr.substr(valueStart, 4) == "true") {
            enabled = true;
        } else if (dataStr.substr(valueStart, 5) == "false") {
            enabled = false;
        } else {
            spdlog::error("[PrismaUIMenu::OnSetBardSongsEnabled] Could not parse enabled value");
            return;
        }
        
        spdlog::info("[PrismaUIMenu::OnSetBardSongsEnabled] Setting bard songs enabled to: {}", enabled);
        
        const auto& settings = Config::GetSettings();
        if (settings.mcm.blockBardSongsGlobal) {
            settings.mcm.blockBardSongsGlobal->value = enabled ? 1.0f : 0.0f;
            spdlog::info("[PrismaUIMenu::OnSetBardSongsEnabled] Set blockBardSongs global to {}", 
                settings.mcm.blockBardSongsGlobal->value);
            
            SettingsPersistence::SaveSettings();
            Config::ClearCache();
            SendSettingsData();
            
        } else {
            spdlog::warn("[PrismaUIMenu::OnSetBardSongsEnabled] BlockBardSongs global not found");
        }
        
    } catch (const std::exception& e) {
        spdlog::error("[PrismaUIMenu::OnSetBardSongsEnabled] Exception: {}", e.what());
    }
}

void PrismaUIMenu::SendSettingsData()
{
    if (!initialized_ || !prismaUI_ || !prismaUI_->IsValid(view_)) {
        spdlog::warn("[PrismaUIMenu::SendSettingsData] Not initialized or invalid view");
        return;
    }
    
    try {
        const auto& settings = Config::GetSettings();
        
        // Build JSON with all current global values
        std::ostringstream json;
        json << "{";
        
        // Master controls
        json << "\"blacklistEnabled\":" << (settings.blacklist.toggleGlobal && settings.blacklist.toggleGlobal->value >= 0.5f ? "true" : "false") << ",";
        json << "\"skyrimNetEnabled\":" << (settings.skyrimNetFilter.toggleGlobal && settings.skyrimNetFilter.toggleGlobal->value >= 0.5f ? "true" : "false") << ",";
        json << "\"scenesEnabled\":" << (settings.mcm.blockScenesGlobal && settings.mcm.blockScenesGlobal->value >= 0.5f ? "true" : "false") << ",";
        json << "\"bardSongsEnabled\":" << (settings.mcm.blockBardSongsGlobal && settings.mcm.blockBardSongsGlobal->value >= 0.5f ? "true" : "false") << ",";
        
        // Follower Commentary and Combat Grunts (note inverted logic)
        json << "\"followerCommentaryEnabled\":" << (settings.mcm.blockFollowerCommentaryGlobal && settings.mcm.blockFollowerCommentaryGlobal->value < 0.5f ? "true" : "false") << ",";
        json << "\"combatGruntsBlocked\":" << (settings.mcm.preserveGruntsGlobal && settings.mcm.preserveGruntsGlobal->value >= 0.5f ? "true" : "false") << ",";
        
        // Subtype toggles
        json << "\"subtypes\":{";
        bool first = true;
        for (const auto& [subtypeId, global] : settings.mcm.subtypeGlobals) {
            if (!global) continue;
            
            if (!first) json << ",";
            first = false;
            
            json << "\"" << subtypeId << "\":" << (global->value >= 0.5f ? "true" : "false");
        }
        json << "}";
        
        json << "}";
        
        std::string jsonData = json.str();
        spdlog::info("[PrismaUIMenu] Sending settings data: {}", jsonData);
        
        // Escape for JS
        std::string escapedJSON;
        for (char c : jsonData) {
            switch (c) {
                case '\'': escapedJSON += "\\'"; break;
                case '\\': escapedJSON += "\\\\"; break;
                case '\n': escapedJSON += "\\n"; break;
                case '\r': escapedJSON += "\\r"; break;
                case '\t': escapedJSON += "\\t"; break;
                default: escapedJSON += c; break;
            }
        }
        
        std::string jsCode = "window.SKSE_API?.call('updateSettings', '" + escapedJSON + "')";
        prismaUI_->Invoke(view_, jsCode.c_str());
        
        spdlog::info("[PrismaUIMenu] Successfully invoked updateSettings");
    } catch (const std::exception& e) {
        spdlog::error("[PrismaUIMenu] Failed to send settings data: {}", e.what());
    }
}

void PrismaUIMenu::OnRequestSettings(const char* data)
{
    spdlog::trace("[PrismaUIMenu] Settings data requested");
    SendSettingsData();
}
