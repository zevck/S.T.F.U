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

#include "Config.h"
#include "ConfigInternal.h"
#include "EditorID.h"
#include "DialogueDatabase.h"
#include <spdlog/spdlog.h>
#include <yaml-cpp/yaml.h>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <vector>

namespace Config
{
    static std::string GetWhitelistPath()
    {
        static std::string path;
        if (path.empty()) {
            wchar_t buffer[MAX_PATH];
            GetModuleFileNameW(nullptr, buffer, MAX_PATH);
            std::filesystem::path exePath(buffer);
            path = (exePath.parent_path() / "Data" / "SKSE" / "Plugins" / "STFU" / "import" / "STFU_Whitelist.yaml").string();
        }
        return path;
    }
    
    static std::string GetBlacklistPath()
    {
        static std::string path;
        if (path.empty()) {
            wchar_t buffer[MAX_PATH];
            GetModuleFileNameW(nullptr, buffer, MAX_PATH);
            std::filesystem::path exePath(buffer);
            path = (exePath.parent_path() / "Data" / "SKSE" / "Plugins" / "STFU" / "import" / "STFU_Blacklist.yaml").string();
        }
        return path;
    }
    
    static std::string GetSubtypeOverridesPath()
    {
        static std::string path;
        if (path.empty()) {
            wchar_t buffer[MAX_PATH];
            GetModuleFileNameW(nullptr, buffer, MAX_PATH);
            std::filesystem::path exePath(buffer);
            path = (exePath.parent_path() / "Data" / "SKSE" / "Plugins" / "STFU" / "import" / "STFU_SubtypeOverrides.yaml").string();
        }
        return path;
    }

    void GenerateDefaultYAMLs()
    {
        // Create STFU directory if it doesn't exist
        wchar_t buffer[MAX_PATH];
        GetModuleFileNameW(nullptr, buffer, MAX_PATH);
        std::filesystem::path exePath(buffer);
        std::filesystem::path configDir = exePath.parent_path() / "Data" / "SKSE" / "Plugins" / "STFU" / "import";
        
        try {
            std::filesystem::create_directories(configDir);
        } catch (const std::exception& e) {
            spdlog::error("[Config] Failed to create config directory: {}", e.what());
            return;
        }
        
        // Generate Blacklist YAML if it doesn't exist
        std::string blacklistPath = GetBlacklistPath();
        if (!std::filesystem::exists(blacklistPath)) {
            try {
                std::ofstream file(blacklistPath);
                file << R"(# ============================================================================
#                        STFU Blacklist Configuration
# ============================================================================
# Topics that will be SOFT BLOCKED (audio/subtitles silenced) when MCM toggle is ON
# After making changes, use "Import from YAML" in MCM Settings page
#
# IDENTIFIER FORMATS:
#   - EditorID (recommended): TopicEditorID
#   - FormKey: 0x012345:PluginName.esp
#   - Quote special characters: "[Topic]:Name" for YAML special chars
#
# TIP: Check Data/SKSE/Plugins/STFU/STFU_DialogueLog.txt
#      for dialogue that occurs during gameplay. Copy/paste entries directly!
#      [Menu] tag indicates menu dialogue (can be used with SkyrimNet)
# ============================================================================

topics:
  # Add dialogue topics to soft block (silence audio/subtitles)
  # Examples:
  # - WICastMagicNonHostileSpellStealthTopic
  # - AnnoyingDialogueTopic
  
scenes:
  # Block entire scenes (HARD BLOCK - may break quests!)
  # Examples:
  # - WhiterunMikaelSongScene
  # - WICraftItem01Scene
  
quests:
  # Block all dialogue topics referenced by a quest
  # Example:
  # - DA07MuseumScenes
)";
                file.close();
                spdlog::info("[Config] Generated default STFU_Blacklist.yaml");
            } catch (const std::exception& e) {
                spdlog::error("[Config] Failed to generate Blacklist YAML: {}", e.what());
            }
        }
        
        // Generate Whitelist YAML if it doesn't exist
        std::string whitelistPath = GetWhitelistPath();
        if (!std::filesystem::exists(whitelistPath)) {
            try {
                std::ofstream file(whitelistPath);
                file << R"(# ============================================================================
#                        STFU Whitelist Configuration
# ============================================================================
# Topics that will NEVER be blocked, even if they match blacklist rules
# Use this to protect important dialogue from being blocked
# After making changes, use "Import from YAML" in MCM Settings page
#
# IDENTIFIER FORMATS:
#   - EditorID (recommended): TopicEditorID
#   - FormKey: 0x012345:PluginName.esp
#   - Quote special characters: "[Topic]:Name" for YAML special chars
#
# USE CASES:
#   - Quest-critical dialogue that shouldn't be blocked
#   - Dialogue that breaks if silenced (house purchases, etc.)
#   - Override broad blacklist rules for specific exceptions
# ============================================================================

topics:
  # Add dialogue topics to protect here
  # Examples:
  # - ImportantQuestTopic
  # - 0x012345:MyMod.esp
  
plugins:
  # Protect ALL dialogue from specific plugins
  # Example:
  # - "ImportantQuestMod.esp"
  
scenes:
  # Protect specific scenes from being blocked
  # Example:
  # - CriticalQuestScene
  
quests:
  # Protect all dialogue from specific quests
  # Examples:
  # - DLC2MQ05
  # - ImportantQuestWithDialogue
)";
                file.close();
                spdlog::info("[Config] Generated default STFU_Whitelist.yaml");
            } catch (const std::exception& e) {
                spdlog::error("[Config] Failed to generate Whitelist YAML: {}", e.what());
            }
        }
        
        // Generate Subtype Overrides YAML if it doesn't exist
        std::string overridesPath = GetSubtypeOverridesPath();
        if (!std::filesystem::exists(overridesPath)) {
            try {
                std::ofstream file(overridesPath);
                file << R"(# ============================================================================
#                   STFU Subtype Overrides Configuration
# ============================================================================
# Manually correct miscategorized dialogue subtypes
# Use this when a topic has the wrong subtype in the game data
# After making changes, use "Import from YAML" in MCM Settings page
#
# FORMAT: TopicIdentifier: NewSubtype
#
# IDENTIFIER FORMATS:
#   - EditorID (recommended): TopicEditorID: Subtype
#   - FormKey: 0x012345:PluginName.esp: Subtype
#
# COMMON SUBTYPES:
#   Hello, Goodbye, Idle, Combat, Detection, Service, Favor,
#   ForceGreet, Scene, Misc, Attack, PowerAttack, Death
#
# USE CASES:
#   - Combat dialogue miscategorized as Idle
#   - Scene dialogue miscategorized as Hello
#   - Background chatter miscategorized as important dialogue
# ============================================================================

overrides:
  # Add subtype corrections here
  # Format: TopicIdentifier: CorrectSubtype
  # Example:
  # DLC2PillarBlockingTopic: Idle
)";
                file.close();
                spdlog::info("[Config] Generated default STFU_SubtypeOverrides.yaml");
            } catch (const std::exception& e) {
                spdlog::error("[Config] Failed to generate Subtype Overrides YAML: {}", e.what());
            }
        }
    }
    
    // Helper function to parse form identifier (FormKey, 0x format, or EditorID)
    // Returns pair: {formID, editorID}
    static std::pair<uint32_t, std::string> ParseFormIdentifierInternal(const std::string& value)
    {
        // Check for FormKey format: "FormID:PluginName" (e.g., "04C2D2:Skyrim.esm")
        size_t colonPos = value.find(':');
        if (colonPos != std::string::npos && colonPos > 0) {
            try {
                std::string formIDStr = value.substr(0, colonPos);
                std::string pluginName = value.substr(colonPos + 1);
                
                // Parse the form ID (hex string)
                uint32_t localFormID = std::stoul(formIDStr, nullptr, 16);
                
                // Look up plugin to get load order
                auto* dataHandler = RE::TESDataHandler::GetSingleton();
                if (dataHandler) {
                    auto modIndexOpt = dataHandler->GetModIndex(pluginName.c_str());
                    if (modIndexOpt.has_value()) {
                        uint8_t modIndex = modIndexOpt.value();
                        // Construct full form ID: (modIndex << 24) | localFormID
                        uint32_t fullFormID = (static_cast<uint32_t>(modIndex) << 24) | (localFormID & 0x00FFFFFF);
                        return {fullFormID, ""};
                    } else {
                        spdlog::warn("[Config] Plugin not found for FormKey: {} (plugin: {})", value, pluginName);
                    }
                }
            } catch (const std::exception& e) {
                spdlog::error("[Config] Failed to parse FormKey: {} (error: {})", value, e.what());
            }
        }
        
        // Check for 0x format: "0xFormID"
        if (value.length() > 2 && (value.substr(0, 2) == "0x" || value.substr(0, 2) == "0X")) {
            std::string hexPart = value.substr(2);
            // Strip leading zeros so over-padded strings (e.g. an extra "0" prepended
            // upstream) don't push past 8 hex chars and overflow uint32_t.
            size_t firstNonZero = hexPart.find_first_not_of('0');
            if (firstNonZero == std::string::npos) {
                return {0, ""};  // all zeros => FormID 0
            }
            hexPart.erase(0, firstNonZero);

            if (hexPart.length() <= 8) {
                bool isAllHex = std::all_of(hexPart.begin(), hexPart.end(),
                    [](char c) { return std::isxdigit(static_cast<unsigned char>(c)) != 0; });
                if (isAllHex) {
                    try {
                        // Use stoull to safely cover any width on the way to uint32_t.
                        uint64_t parsed = std::stoull(hexPart, nullptr, 16);
                        if (parsed <= 0xFFFFFFFFull) {
                            return {static_cast<uint32_t>(parsed), ""};
                        }
                    } catch (const std::exception& e) {
                        spdlog::error("[Config] Failed to parse hex FormID: {} (error: {})", value, e.what());
                    }
                }
            }
        }

        // Check if it's a hex string without "0x" prefix (e.g., "02707A" or "ABC12").
        // Strip leading zeros first so an over-padded 9-char "067400023" still parses
        // as the underlying 8-char FormID 0x67400023.
        if (!value.empty()) {
            bool isAllHex = std::all_of(value.begin(), value.end(),
                [](char c) { return std::isxdigit(static_cast<unsigned char>(c)) != 0; });

            if (isAllHex) {
                size_t firstNonZero = value.find_first_not_of('0');
                std::string stripped = (firstNonZero == std::string::npos)
                    ? std::string("0")
                    : value.substr(firstNonZero);

                if (stripped.length() <= 8) {
                    try {
                        uint64_t parsed = std::stoull(stripped, nullptr, 16);
                        if (parsed <= 0xFFFFFFFFull) {
                            uint32_t formID = static_cast<uint32_t>(parsed);
                            spdlog::info("[Config] Parsed bare hex string as FormID: {} -> 0x{:08X}", value, formID);
                            return {formID, ""};
                        }
                    } catch (const std::exception& e) {
                        spdlog::error("[Config] Failed to parse bare hex FormID: {} (error: {})", value, e.what());
                    }
                }
            }
        }

        // Otherwise treat as EditorID
        return {0, value};
    }

    static void ImportSubtypeOverrides()
    {
        std::string overridesPath = GetSubtypeOverridesPath();
        
        if (!std::filesystem::exists(overridesPath)) {
            spdlog::debug("[Config] Subtype overrides YAML not found: {}", overridesPath);
            return;
        }

        auto* db = DialogueDB::GetDatabase();
        if (!db) {
            spdlog::error("[Config] Database not available for subtype overrides import");
            return;
        }

        try {
            spdlog::info("[Config] Importing from STFU_SubtypeOverrides.yaml...");
            YAML::Node config = YAML::LoadFile(overridesPath);
            
            if (!config["overrides"] || !config["overrides"].IsMap()) {
                spdlog::warn("[Config] No 'overrides' section found in STFU_SubtypeOverrides.yaml");
                return;
            }

            int importedCount = 0;
            int failedCount = 0;

            for (const auto& entry : config["overrides"]) {
                std::string topicIdentifier = entry.first.as<std::string>();
                std::string subtypeName = entry.second.as<std::string>();

                // Look up subtype ID from name
                auto subtypeIt = SubtypeNameMap.find(subtypeName);
                if (subtypeIt == SubtypeNameMap.end()) {
                    spdlog::warn("[Config] Unknown subtype name '{}' for topic '{}'", subtypeName, topicIdentifier);
                    failedCount++;
                    continue;
                }
                uint16_t subtypeId = subtypeIt->second;

                // Parse the topic identifier (FormKey or EditorID)
                auto [formID, editorID] = ParseFormIdentifierInternal(topicIdentifier);

                // Create blacklist entry with the subtype category
                DialogueDB::BlacklistEntry blacklistEntry;
                blacklistEntry.targetType = DialogueDB::BlacklistTarget::Topic;
                blacklistEntry.targetFormID = formID;
                blacklistEntry.targetEditorID = editorID;
                blacklistEntry.filterCategory = subtypeName;  // Use subtype name as filter category
                blacklistEntry.blockType = DialogueDB::BlockType::Soft;
                blacklistEntry.notes = "Subtype override from YAML: " + subtypeName;
                blacklistEntry.subtype = subtypeId;
                blacklistEntry.subtypeName = subtypeName;

                if (db->AddToBlacklist(blacklistEntry, false)) {
                    importedCount++;
                    spdlog::debug("[Config] Added subtype override: {} -> {} (ID: {})", topicIdentifier, subtypeName, subtypeId);
                } else {
                    failedCount++;
                    spdlog::warn("[Config] Failed to add subtype override: {} -> {}", topicIdentifier, subtypeName);
                }
            }

            spdlog::info("[Config] Subtype overrides import complete: {} successful, {} failed", importedCount, failedCount);

        } catch (const YAML::Exception& e) {
            spdlog::error("[Config] Subtype overrides YAML parse error: {}", e.what());
        }
    }

    int ImportYAMLToDatabase()
    {
        spdlog::info("[Config] Starting YAML import to database...");
        
        auto* db = DialogueDB::GetDatabase();
        if (!db) {
            spdlog::error("[Config] Database not available for YAML import");
            return 0;
        }
        
        int blacklistTopics = 0, blacklistScenes = 0, blacklistQuests = 0;
        int whitelistTopics = 0, whitelistScenes = 0, whitelistQuests = 0, whitelistPlugins = 0;
        
        // Import from Blacklist YAML
        std::string blacklistPath = GetBlacklistPath();
        if (std::filesystem::exists(blacklistPath)) {
            try {
                spdlog::info("[Config] Importing from STFU_Blacklist.yaml...");
                YAML::Node config = YAML::LoadFile(blacklistPath);
                
                // Import topics
                if (config["topics"] && config["topics"].IsSequence()) {
                    for (const auto& entry : config["topics"]) {
                        if (entry.IsScalar()) {
                            std::string value = entry.as<std::string>();
                            auto [formID, editorID] = ParseFormIdentifierInternal(value);
                            
                            DialogueDB::BlacklistEntry blacklistEntry;
                            blacklistEntry.targetType = DialogueDB::BlacklistTarget::Topic;
                            blacklistEntry.targetFormID = formID;
                            blacklistEntry.targetEditorID = editorID;
                            blacklistEntry.filterCategory = "Blacklist";
                            blacklistEntry.blockType = DialogueDB::BlockType::Soft;
                            blacklistEntry.notes = "Imported from Blacklist YAML";
                            
                            if (db->AddToBlacklist(blacklistEntry, false)) {
                                blacklistTopics++;
                            }
                        }
                    }
                }
                
                // Import scenes
                if (config["scenes"] && config["scenes"].IsSequence()) {
                    for (const auto& entry : config["scenes"]) {
                        if (entry.IsScalar()) {
                            std::string sceneEditorID = entry.as<std::string>();
                            
                            DialogueDB::BlacklistEntry blacklistEntry;
                            blacklistEntry.targetType = DialogueDB::BlacklistTarget::Scene;
                            blacklistEntry.targetFormID = 0;
                            blacklistEntry.targetEditorID = sceneEditorID;
                            blacklistEntry.filterCategory = "Blacklist";
                            blacklistEntry.blockType = DialogueDB::BlockType::Hard;
                            blacklistEntry.notes = "Imported from Blacklist YAML";
                            blacklistEntry.subtype = 14;
                            blacklistEntry.subtypeName = "Scene";
                            
                            if (db->AddToBlacklist(blacklistEntry, false)) {
                                blacklistScenes++;
                            }
                        }
                    }
                }
                
                // Import quests (extract ALL topics from them)
                if (config["quests"] && config["quests"].IsSequence()) {
                    for (const auto& entry : config["quests"]) {
                        if (entry.IsScalar()) {
                            std::string questEditorID = entry.as<std::string>();
                            
                            auto* quest = SafeLookupForm<RE::TESQuest>(questEditorID.c_str());
                            if (!quest) {
                                spdlog::warn("[Config] Quest not found: {}", questEditorID);
                                continue;
                            }
                            
                            // Import from regular topics arrays (SceneDialogue + Combat + Favors + Detection + Service + Miscellaneous)
                            for (int dialogueType = 0; dialogueType < (RE::DIALOGUE_TYPES::kTotal - RE::DIALOGUE_TYPES::kBranchedTotal); ++dialogueType) {
                                auto& topicsArray = quest->topics[dialogueType];
                                for (auto* topic : topicsArray) {
                                    if (!topic) continue;

                                    uint32_t topicFormID = topic->GetFormID();
                                    const char* topicEditorID = STFU::GetEditorID(topic);

                                    DialogueDB::BlacklistEntry blacklistEntry;
                                    blacklistEntry.targetType = DialogueDB::BlacklistTarget::Topic;
                                    blacklistEntry.targetFormID = topicFormID;
                                    blacklistEntry.targetEditorID = topicEditorID ? topicEditorID : "";
                                    blacklistEntry.filterCategory = "Blacklist";
                                    blacklistEntry.blockType = DialogueDB::BlockType::Soft;
                                    blacklistEntry.notes = "Imported from Blacklist YAML (quest: " + questEditorID + ")";

                                    if (db->AddToBlacklist(blacklistEntry, false)) {
                                        blacklistQuests++;
                                    }
                                }
                            }

                            // Also import from branched dialogue (PlayerDialogue + CommandDialogue)
                            for (int branchType = 0; branchType < RE::DIALOGUE_TYPES::kBranchedTotal; ++branchType) {
                                auto& branchMap = quest->branchedDialogue[branchType];
                                for (auto& [branch, topicsArray] : branchMap) {
                                    if (!topicsArray) continue;
                                    for (auto* topic : *topicsArray) {
                                        if (!topic) continue;

                                        uint32_t topicFormID = topic->GetFormID();
                                        const char* topicEditorID = STFU::GetEditorID(topic);

                                        DialogueDB::BlacklistEntry blacklistEntry;
                                        blacklistEntry.targetType = DialogueDB::BlacklistTarget::Topic;
                                        blacklistEntry.targetFormID = topicFormID;
                                        blacklistEntry.targetEditorID = topicEditorID ? topicEditorID : "";
                                        blacklistEntry.filterCategory = "Blacklist";
                                        blacklistEntry.blockType = DialogueDB::BlockType::Soft;
                                        blacklistEntry.notes = "Imported from Blacklist YAML (quest: " + questEditorID + ", branched)";

                                        if (db->AddToBlacklist(blacklistEntry, false)) {
                                            blacklistQuests++;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                
                spdlog::info("[Config] Blacklist YAML import: {} topics, {} scenes, {} quest topics", 
                    blacklistTopics, blacklistScenes, blacklistQuests);
                    
            } catch (const YAML::Exception& e) {
                spdlog::error("[Config] Blacklist YAML import error: {}", e.what());
            }
        } else {
            spdlog::warn("[Config] Blacklist YAML not found: {}", blacklistPath);
        }
        
        // Import from Whitelist YAML
        std::string whitelistPath = GetWhitelistPath();
        if (std::filesystem::exists(whitelistPath)) {
            try {
                spdlog::info("[Config] Importing from STFU_Whitelist.yaml...");
                YAML::Node config = YAML::LoadFile(whitelistPath);
                
                // Import topics
                if (config["topics"] && config["topics"].IsSequence()) {
                    for (const auto& entry : config["topics"]) {
                        if (entry.IsScalar()) {
                            std::string value = entry.as<std::string>();
                            auto [formID, editorID] = ParseFormIdentifierInternal(value);
                            
                            DialogueDB::BlacklistEntry whitelistEntry;
                            whitelistEntry.targetType = DialogueDB::BlacklistTarget::Topic;
                            whitelistEntry.targetFormID = formID;
                            whitelistEntry.targetEditorID = editorID;
                            whitelistEntry.filterCategory = "Whitelist";
                            whitelistEntry.blockType = DialogueDB::BlockType::Soft;
                            whitelistEntry.notes = "Imported from Whitelist YAML";
                            
                            if (db->AddToWhitelist(whitelistEntry)) {
                                whitelistTopics++;
                            }
                        }
                    }
                }
                
                // Import scenes
                if (config["scenes"] && config["scenes"].IsSequence()) {
                    for (const auto& entry : config["scenes"]) {
                        if (entry.IsScalar()) {
                            std::string sceneEditorID = entry.as<std::string>();
                            
                            DialogueDB::BlacklistEntry whitelistEntry;
                            whitelistEntry.targetType = DialogueDB::BlacklistTarget::Scene;
                            whitelistEntry.targetFormID = 0;
                            whitelistEntry.targetEditorID = sceneEditorID;
                            whitelistEntry.filterCategory = "Whitelist";
                            whitelistEntry.blockType = DialogueDB::BlockType::Hard;
                            whitelistEntry.notes = "Imported from Whitelist YAML";
                            whitelistEntry.subtype = 14;
                            whitelistEntry.subtypeName = "Scene";
                            
                            if (db->AddToWhitelist(whitelistEntry)) {
                                whitelistScenes++;
                            }
                        }
                    }
                }
                
                // Import quests (extract ALL topics from them)
                if (config["quests"] && config["quests"].IsSequence()) {
                    for (const auto& entry : config["quests"]) {
                        if (entry.IsScalar()) {
                            std::string questEditorID = entry.as<std::string>();
                            
                            auto* quest = SafeLookupForm<RE::TESQuest>(questEditorID.c_str());
                            if (!quest) {
                                spdlog::warn("[Config] Quest not found: {}", questEditorID);
                                continue;
                            }
                            
                            // Import from regular topics arrays (SceneDialogue + Combat + Favors + Detection + Service + Miscellaneous)
                            for (int dialogueType = 0; dialogueType < (RE::DIALOGUE_TYPES::kTotal - RE::DIALOGUE_TYPES::kBranchedTotal); ++dialogueType) {
                                auto& topicsArray = quest->topics[dialogueType];
                                for (auto* topic : topicsArray) {
                                    if (!topic) continue;

                                    uint32_t topicFormID = topic->GetFormID();
                                    const char* topicEditorID = STFU::GetEditorID(topic);

                                    DialogueDB::BlacklistEntry whitelistEntry;
                                    whitelistEntry.targetType = DialogueDB::BlacklistTarget::Topic;
                                    whitelistEntry.targetFormID = topicFormID;
                                    whitelistEntry.targetEditorID = topicEditorID ? topicEditorID : "";
                                    whitelistEntry.filterCategory = "Whitelist";
                                    whitelistEntry.blockType = DialogueDB::BlockType::Soft;
                                    whitelistEntry.notes = "Imported from Whitelist YAML (quest: " + questEditorID + ")";

                                    if (db->AddToWhitelist(whitelistEntry)) {
                                        whitelistQuests++;
                                    }
                                }
                            }

                            // Also import from branched dialogue (PlayerDialogue + CommandDialogue)
                            for (int branchType = 0; branchType < RE::DIALOGUE_TYPES::kBranchedTotal; ++branchType) {
                                auto& branchMap = quest->branchedDialogue[branchType];
                                for (auto& [branch, topicsArray] : branchMap) {
                                    if (!topicsArray) continue;
                                    for (auto* topic : *topicsArray) {
                                        if (!topic) continue;

                                        uint32_t topicFormID = topic->GetFormID();
                                        const char* topicEditorID = STFU::GetEditorID(topic);

                                        DialogueDB::BlacklistEntry whitelistEntry;
                                        whitelistEntry.targetType = DialogueDB::BlacklistTarget::Topic;
                                        whitelistEntry.targetFormID = topicFormID;
                                        whitelistEntry.targetEditorID = topicEditorID ? topicEditorID : "";
                                        whitelistEntry.filterCategory = "Whitelist";
                                        whitelistEntry.blockType = DialogueDB::BlockType::Soft;
                                        whitelistEntry.notes = "Imported from Whitelist YAML (quest: " + questEditorID + ", branched)";

                                        if (db->AddToWhitelist(whitelistEntry)) {
                                            whitelistQuests++;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                
                // Import plugins
                if (config["plugins"] && config["plugins"].IsSequence()) {
                    for (const auto& entry : config["plugins"]) {
                        if (entry.IsScalar()) {
                            std::string pluginName = entry.as<std::string>();
                            
                            DialogueDB::BlacklistEntry whitelistEntry;
                            whitelistEntry.targetType = DialogueDB::BlacklistTarget::Plugin;
                            whitelistEntry.targetFormID = 0;
                            whitelistEntry.targetEditorID = pluginName;  // Plugin name stored in EditorID field
                            whitelistEntry.filterCategory = "Whitelist";
                            whitelistEntry.blockType = DialogueDB::BlockType::Soft;
                            whitelistEntry.notes = "Imported from Whitelist YAML - all dialogue from this plugin allowed";
                            whitelistEntry.sourcePlugin = pluginName;
                            
                            if (db->AddToWhitelist(whitelistEntry)) {
                                whitelistPlugins++;
                            }
                        }
                    }
                }
                
                spdlog::info("[Config] Whitelist YAML import: {} topics, {} scenes, {} quest topics, {} plugins", 
                    whitelistTopics, whitelistScenes, whitelistQuests, whitelistPlugins);
                    
            } catch (const YAML::Exception& e) {
                spdlog::error("[Config] Whitelist YAML import error: {}", e.what());
            }
        } else {
            spdlog::warn("[Config] Whitelist YAML not found: {}", whitelistPath);
        }
        
        // Import subtype overrides
        ImportSubtypeOverrides();
        
        int totalBlacklist = blacklistTopics + blacklistScenes + blacklistQuests;
        int totalWhitelist = whitelistTopics + whitelistScenes + whitelistQuests + whitelistPlugins;
        spdlog::info("[Config] YAML import complete - Blacklist: {} entries | Whitelist: {} entries",
            totalBlacklist, totalWhitelist);
        return totalBlacklist + totalWhitelist;
    }
    // Public wrapper for ParseFormIdentifier
    std::pair<uint32_t, std::string> ParseFormIdentifier(const std::string& value)
    {
        return ParseFormIdentifierInternal(value);
    }
}  // namespace Config
