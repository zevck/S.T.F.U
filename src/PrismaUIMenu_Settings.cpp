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
        
        auto bardScenes = Config::GetBardSongScenesList();
        db->ImportHardcodedScenes(bardScenes, "BardSongs");
        spdlog::info("[PrismaUIMenu::OnImportScenes] Imported {} bard song scenes", bardScenes.size());
        
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
        
        int failedFiles = 0;
        const int imported = Config::ImportYAMLToDatabase(&failedFiles);

        if (failedFiles > 0) {
            spdlog::warn("[PrismaUIMenu::OnImportYAML] {} YAML file(s) failed to parse", failedFiles);
            prismaUI_->Invoke(view_, BuildToastScript(std::format(
                "{} YAML file(s) could not be read (see STFU.log); imported {} entries from the rest", failedFiles, imported), "error").c_str());
        } else {
            spdlog::info("[PrismaUIMenu::OnImportYAML] YAML import completed successfully");
            prismaUI_->Invoke(view_, BuildToastScript(std::format("Imported {} entries from YAML", imported), "success").c_str());
        }
        
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

namespace
{
    // Handles a settings-toggle request from the UI: parses {"<field>": true|false},
    // writes it to the toggle's global, saves settings and pushes the new state back
    // to the UI. Every toggle global is 1 when its blocking is on.
    void SetToggleFromUI(const char* handler, const char* data, const char* field,
                         RE::TESGlobal* global)
    {
        spdlog::info("[PrismaUIMenu::{}] Request received", handler);

        if (!data || data[0] == '\0') {
            spdlog::error("[PrismaUIMenu::{}] Null or empty data received", handler);
            return;
        }

        try {
            spdlog::info("[PrismaUIMenu::{}] Received data: {}", handler, data);

            std::string dataStr(data);
            auto fieldPos = dataStr.find(std::string("\"") + field + "\"");
            if (fieldPos == std::string::npos) {
                spdlog::error("[PrismaUIMenu::{}] '{}' field not found in JSON", handler, field);
                return;
            }

            auto colonPos = dataStr.find(':', fieldPos);
            if (colonPos == std::string::npos) {
                spdlog::error("[PrismaUIMenu::{}] Malformed JSON - no colon after {}", handler, field);
                return;
            }

            size_t valueStart = colonPos + 1;
            while (valueStart < dataStr.length() && (dataStr[valueStart] == ' ' || dataStr[valueStart] == '\t')) {
                valueStart++;
            }

            bool value = false;
            if (dataStr.substr(valueStart, 4) == "true") {
                value = true;
            } else if (dataStr.substr(valueStart, 5) == "false") {
                value = false;
            } else {
                spdlog::error("[PrismaUIMenu::{}] Could not parse {} value", handler, field);
                return;
            }

            spdlog::info("[PrismaUIMenu::{}] Setting {} to: {}", handler, field, value);

            if (!global) {
                spdlog::warn("[PrismaUIMenu::{}] Toggle global not found", handler);
                return;
            }

            global->value = value ? 1.0f : 0.0f;
            const char* globalID = STFU::GetEditorID(global);
            spdlog::info("[PrismaUIMenu::{}] Set {} global to {}", handler, globalID ? globalID : "?", global->value);

            SettingsPersistence::SaveSettings();
            PrismaUIMenu::SendSettingsData();

        } catch (const std::exception& e) {
            spdlog::error("[PrismaUIMenu::{}] Exception: {}", handler, e.what());
        }
    }
}

// Combat grunts: STFU_PreserveGrunts is 1 when grunts are blocked
void PrismaUIMenu::OnSetCombatGruntsBlocked(const char* data)
{
    SetToggleFromUI("OnSetCombatGruntsBlocked", data, "blocked", Config::GetSettings().mcm.preserveGruntsGlobal);
}

// Follower commentary: "enabled" means blocking is on (STFU_FollowerCommentary = 1)
void PrismaUIMenu::OnSetFollowerCommentaryEnabled(const char* data)
{
    SetToggleFromUI("OnSetFollowerCommentaryEnabled", data, "enabled", Config::GetSettings().mcm.blockFollowerCommentaryGlobal);
}

void PrismaUIMenu::OnSetBlacklistEnabled(const char* data)
{
    SetToggleFromUI("OnSetBlacklistEnabled", data, "enabled", Config::GetSettings().blacklist.toggleGlobal);
}

void PrismaUIMenu::OnSetScenesEnabled(const char* data)
{
    SetToggleFromUI("OnSetScenesEnabled", data, "enabled", Config::GetSettings().mcm.blockScenesGlobal);
}

void PrismaUIMenu::OnSetBardSongsEnabled(const char* data)
{
    SetToggleFromUI("OnSetBardSongsEnabled", data, "enabled", Config::GetSettings().mcm.blockBardSongsGlobal);
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
        json << "\"scenesEnabled\":" << (settings.mcm.blockScenesGlobal && settings.mcm.blockScenesGlobal->value >= 0.5f ? "true" : "false") << ",";
        json << "\"bardSongsEnabled\":" << (settings.mcm.blockBardSongsGlobal && settings.mcm.blockBardSongsGlobal->value >= 0.5f ? "true" : "false") << ",";
        
        json << "\"followerCommentaryEnabled\":" << (settings.mcm.blockFollowerCommentaryGlobal && settings.mcm.blockFollowerCommentaryGlobal->value >= 0.5f ? "true" : "false") << ",";
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
