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

#include "../include/PCH.h"
#include "EditorID.h"
#include <string>
#include <unordered_set>
#include <unordered_map>

namespace Config
{
    // Blacklist master toggle (entries themselves live in the database)
    struct BlacklistSettings
    {
        RE::TESGlobal* toggleGlobal = nullptr;  // Master toggle from STFU.esp
    };
    
    // MCM globals for subtype toggles (from STFU.esp)
    struct MCMSettings
    {
        std::unordered_map<uint16_t, RE::TESGlobal*> subtypeGlobals;
        RE::TESGlobal* blockScenesGlobal = nullptr;  // Runtime global for scene blocking
        RE::TESGlobal* blockBardSongsGlobal = nullptr;  // Runtime global for bard song blocking
        RE::TESGlobal* blockFollowerCommentaryGlobal = nullptr;  // Runtime global for follower commentary blocking
        RE::TESGlobal* preserveGruntsGlobal = nullptr;  // Runtime global for grunt preservation (0=filter, 1=preserve)
    };
    
    // Hardcoded ambient scenes from patcher (vanilla + DLC scenes that should be blockable)
    struct HardcodedScenes
    {
        std::unordered_set<std::string> topicEditorIDs;
    };

    struct Settings
    {
        BlacklistSettings blacklist;
        MCMSettings mcm;
        HardcodedScenes hardcodedScenes;
        uint32_t menuHotkey = 0xD2;  // Default: Insert key (DirectInput scancode)
    };

    // Load configuration from STFU files
    void Load();

    // Get current settings
    const Settings& GetSettings();
    
    // Get accurate subtype for topic (uses correction map if available, falls back to DATA.subtype)
    uint16_t GetAccurateSubtype(RE::TESTopic* topic);
    
    // Get human-readable name for subtype ID
    std::string GetSubtypeName(uint16_t subtype);
    
    // Get the subtype name-to-ID map for INI operations
    const std::unordered_map<std::string, uint16_t>& GetSubtypeNameMap();

    // Check if dialogue should be soft-blocked (silences BOTH audio AND subtitles)
    // Checks database entries + YAML + MCM subtypes. Optional actor filtering.
    bool ShouldSoftBlock(RE::TESQuest* quest, RE::TESTopic* topic, const char* speakerName, const char* responseText, uint32_t speakerFormID = 0, RE::TESObjectREFR* speakerRef = nullptr);
    
    // Check if dialogue is blocked by MCM subtype filter specifically (for UI status display)
    bool IsFilteredByMCM(uint32_t topicFormID, uint16_t topicSubtype);
    
    // Check if a subtype has a toggle that exists but is disabled
    bool HasDisabledSubtypeToggle(uint16_t topicSubtype);
    
    // Toggle a subtype's MCM filter setting (flip between enabled/disabled)
    bool ToggleSubtypeFilter(uint16_t topicSubtype);
    
    // Check if a filterCategory's toggle is enabled (for database blacklist entries)
    bool IsFilterCategoryEnabled(const std::string& filterCategory);
    
    // Check if scenes (subtype 14) should be stopped entirely
    bool ShouldBlockScenes();
    
    // Check if bard songs should be blocked
    bool ShouldBlockBardSongs();
    
    // Check if a quest is a bard song quest
    bool IsBardSongQuest(RE::TESQuest* quest);
    
    // Check if dialogue should be filtered from history log (grunts, EnterSprintBreath)
    bool ShouldFilterFromHistory(const char* responseText, uint16_t subtype, RE::TESTopic* topic);
    
    // Check if a topic is in the hardcoded ambient scenes list
    bool IsHardcodedAmbientScene(RE::TESTopic* topic);
    
    // Check if a scene is in the hardcoded ambient scenes list (by scene editor ID)
    bool IsHardcodedAmbientScene(RE::BGSScene* scene);
    
    // Get MCM global for STFU_Scenes toggle
    RE::TESGlobal* GetScenesGlobal();

    // Get MCM global for STFU_BardSongs toggle
    RE::TESGlobal* GetBardSongsGlobal();

    // Get MCM global for STFU_Blacklist toggle (gates user-added blacklist entries)
    RE::TESGlobal* GetBlacklistGlobal();

    // Get MCM global for STFU_FollowerCommentary toggle
    RE::TESGlobal* GetFollowerCommentaryGlobal();

    // Pick the toggle global that should gate a scene hard-block, based on the
    // blacklist entry's filterCategory ("Blacklist" -> STFU_Blacklist, "Scene" ->
    // STFU_Scenes, "FollowerCommentary" -> STFU_FollowerCommentary, "BardSongs" ->
    // STFU_BardSongs). Falls back to STFU_Scenes for unknown categories.
    RE::TESGlobal* GetSceneGateGlobalForCategory(const std::string& filterCategory);
    
    // Get list of hardcoded scenes for database import
    std::vector<std::string> GetHardcodedScenesList();
    std::vector<std::string> GetBardSongQuestsList();
    std::vector<std::string> GetFollowerCommentaryScenesList();
    
    // Import YAML blacklist into database
    int ImportYAMLToDatabase();

    // Parse form identifier (supports 0x format, FormKey format, or EditorID)
    // Returns pair of (formID, editorID) - formID is 0 if EditorID, editorID is empty if FormID
    std::pair<uint32_t, std::string> ParseFormIdentifier(const std::string& value);
    
    // Template helper to safely lookup a form by EditorID
    template <typename T>
    T* SafeLookupForm(const char* editorID)
    {
        if (!editorID) return nullptr;
        
        auto* dataHandler = RE::TESDataHandler::GetSingleton();
        if (!dataHandler) return nullptr;
        
        for (auto& form : dataHandler->GetFormArray<T>()) {
            if (form && STFU::GetEditorID(form)) {
                if (strcmp(STFU::GetEditorID(form), editorID) == 0) {
                    return form;
                }
            }
        }
        
        return nullptr;
    }
}
