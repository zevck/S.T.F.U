#pragma once

#include "../include/PCH.h"
#include <string>
#include <unordered_set>

namespace Config
{
    // Configuration loaded from STFU files
    struct Settings
    {
        // Blacklisted dialogue topics (always block)
        std::unordered_set<uint32_t> blacklistedFormIDs;
        std::unordered_set<std::string> blacklistedEditorIDs;
        
        // Blacklisted quests (block all topics from these quests)
        std::unordered_set<uint32_t> blacklistedQuestFormIDs;
        std::unordered_set<std::string> blacklistedQuestEditorIDs;

        // Whitelisted dialogue (never block)
        std::unordered_set<uint32_t> whitelistedFormIDs;
        std::unordered_set<std::string> whitelistedEditorIDs;

        // Disabled subtypes from STFU_Config.ini
        std::unordered_set<std::string> disabledSubtypes;

        // Configuration flags
        bool vanillaOnly = false;
        bool safeMode = false;
        
        // Global toggle (from STFU.esp)
        RE::TESGlobal* toggleGlobal = nullptr;
    };

    // Load configuration from STFU files
    void Load();

    // Get current settings
    const Settings& GetSettings();
}
