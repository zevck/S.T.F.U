#pragma once

#include "../include/PCH.h"
#include <string>
#include <unordered_set>
#include <unordered_map>

namespace Config
{
    // Whitelist configuration (STFU_Whitelist.yaml) - never block these
    struct WhitelistSettings
    {
        std::unordered_set<uint32_t> topicFormIDs;
        std::unordered_set<std::string> topicEditorIDs;
        std::unordered_set<uint32_t> questFormIDs;
        std::unordered_set<std::string> questEditorIDs;
        std::unordered_set<uint16_t> subtypes;
    };
    
    // Blacklist configuration (STFU_Blacklist.yaml) - blocks audio/subtitles
    struct BlacklistSettings
    {
        std::unordered_set<uint32_t> topicFormIDs;
        std::unordered_set<std::string> topicEditorIDs;
        std::unordered_set<uint32_t> questFormIDs;
        std::unordered_set<std::string> questEditorIDs;
        std::unordered_set<uint16_t> subtypes;
        RE::TESGlobal* toggleGlobal = nullptr;  // Master toggle from STFU.esp
    };
    
    // SkyrimNet filter configuration (STFU_SkyrimNetFilter.yaml) - blocks logging only
    struct SkyrimNetFilterSettings
    {
        std::unordered_set<uint32_t> topicFormIDs;
        std::unordered_set<std::string> topicEditorIDs;
        std::unordered_set<uint32_t> questFormIDs;
        std::unordered_set<std::string> questEditorIDs;
        std::unordered_set<uint16_t> subtypes;
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
        WhitelistSettings whitelist;
        BlacklistSettings blacklist;
        SkyrimNetFilterSettings skyrimNetFilter;
        MCMSettings mcm;
        HardcodedScenes hardcodedScenes;
        uint32_t menuHotkey = 0xD2;  // Default: Insert key (DirectInput scancode)
    };

    // Blocking decision cache for performance optimization
    struct BlockingDecision
    {
        bool blockAudio;
        bool blockSubtitles;
        bool blockSkyrimNet;
        int64_t cacheTime;  // Timestamp when cached
    };

    // Load configuration from STFU files
    void Load();
    
    // Clear the blocking decision cache (call when config reloads or MCM changes)
    void ClearCache();

    // Get current settings
    const Settings& GetSettings();
    
    // Get accurate subtype for topic (uses correction map if available, falls back to DATA.subtype)
    uint16_t GetAccurateSubtype(RE::TESTopic* topic);
    
    // Get human-readable name for subtype ID
    std::string GetSubtypeName(uint16_t subtype);
    
    // Get the subtype name-to-ID map for INI operations
    const std::unordered_map<std::string, uint16_t>& GetSubtypeNameMap();

    // Check if dialogue audio/subtitles should be blocked (STFU_Blacklist.yaml + MCM subtypes)
    bool ShouldBlockDialogue(RE::TESQuest* quest, RE::TESTopic* topic, const char* speakerName, const char* responseText);
    
    // Check individual blocking options (checks database granular flags + YAML + MCM)
    bool ShouldBlockAudio(RE::TESQuest* quest, RE::TESTopic* topic, const char* speakerName, const char* responseText);
    bool ShouldBlockSubtitles(RE::TESQuest* quest, RE::TESTopic* topic, const char* speakerName, const char* responseText);
    
    // Check if dialogue should be filtered from SkyrimNet logging (STFU_SkyrimNetFilter.yaml)
    bool ShouldBlockSkyrimNet(RE::TESQuest* quest, RE::TESTopic* topic, const char* speakerName);
    
    // Invalidate cache for a specific topic (call after adding/updating blacklist entries)
    void InvalidateTopicCache(uint32_t topicFormID);
    
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
    
    // Get list of hardcoded scenes for database import
    std::vector<std::string> GetHardcodedScenesList();
    std::vector<std::string> GetBardSongQuestsList();
    std::vector<std::string> GetFollowerCommentaryScenesList();
    
    // Import YAML blacklist into database
    void ImportYAMLToDatabase();
    
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
            if (form && form->GetFormEditorID()) {
                if (strcmp(form->GetFormEditorID(), editorID) == 0) {
                    return form;
                }
            }
        }
        
        return nullptr;
    }
}
