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
#include "SettingsPersistence.h"
#include <spdlog/spdlog.h>
#include <unordered_map>

namespace Config
{
    Settings g_settings;
    
    // No cache - database lookups are fast enough and avoid cache invalidation issues
    
    // Auto-generated vanilla Skyrim subtype corrections
    // Subtype corrections map - cleared for testing
    // If topics need corrections, add them here as: {formID, correctSubtype}
    // Infrastructure kept for future use
    static std::unordered_map<uint32_t, uint16_t> VanillaSubtypeCorrections = {
        // Empty - using raw DATA.subtype values
    };
    
    // Map user-friendly subtype names to enum values
    std::unordered_map<std::string, uint16_t> SubtypeNameMap = {
        {"Custom", 0}, {"ForceGreet", 1}, {"Rumors", 2}, {"Intimidate", 4}, {"Flatter", 5},
        {"Bribe", 6}, {"AskGift", 7}, {"Gift", 8}, {"AskFavor", 9}, {"Favor", 10},
        {"ShowRelationships", 11}, {"Follow", 12}, {"Reject", 13}, {"Scene", 14}, {"Show", 15},
        {"Agree", 16}, {"Refuse", 17}, {"ExitFavorState", 18}, {"MoralRefusal", 19},
        {"Attack", 26}, {"PowerAttack", 27}, {"Bash", 28}, {"Hit", 29}, {"Flee", 30},
        {"Bleedout", 31}, {"AvoidThreat", 32}, {"Death", 33}, {"Block", 35}, {"Taunt", 36},
        {"AllyKilled", 37}, {"Steal", 38}, {"Yield", 39}, {"AcceptYield", 40},
        {"PickpocketCombat", 41}, {"Assault", 42}, {"Murder", 43}, {"AssaultNPC", 44},
        {"MurderNPC", 45}, {"PickpocketNPC", 46}, {"StealFromNPC", 47},
        {"TrespassAgainstNPC", 48}, {"Trespass", 49}, {"WereTransformCrime", 50},
        {"VoicePowerStartShort", 51}, {"VoicePowerStartLong", 52},
        {"VoicePowerEndShort", 53}, {"VoicePowerEndLong", 54},
        {"AlertIdle", 55}, {"LostIdle", 56}, {"NormalToAlert", 57},
        {"AlertToCombat", 58}, {"NormalToCombat", 59}, {"AlertToNormal", 60},
        {"CombatToNormal", 61}, {"CombatToLost", 62}, {"LostToNormal", 63},
        {"LostToCombat", 64}, {"DetectFriendDie", 65}, {"ServiceRefusal", 66},
        {"Repair", 67}, {"Travel", 68}, {"Training", 69}, {"BarterExit", 70},
        {"RepairExit", 71}, {"Recharge", 72}, {"RechargeExit", 73}, {"TrainingExit", 74},
        {"ObserveCombat", 75}, {"NoticeCorpse", 76}, {"TimeToGo", 77}, {"Goodbye", 78},
        {"Hello", 79}, {"SwingMeleeWeapon", 80}, {"ShootBow", 81}, {"ZKeyObject", 82},
        {"Jump", 83}, {"KnockOverObject", 84}, {"DestroyObject", 85},
        {"StandOnFurniture", 86}, {"LockedObject", 87}, {"PickpocketTopic", 88},
        {"PursueIdleTopic", 89}, {"SharedInfo", 90}, {"PlayerCastProjectileSpell", 91},
        {"PlayerCastSelfSpell", 92}, {"PlayerShout", 93}, {"Idle", 94},
        {"EnterSprintBreath", 95}, {"EnterBowZoomBreath", 96}, {"ExitBowZoomBreath", 97},
        {"ActorCollideWithActor", 98}, {"PlayerInIronSights", 99},
        {"OutOfBreath", 100}, {"CombatGrunt", 101}, {"LeaveWaterBreath", 102}
    };

    // Map subtype values to MCM global EditorIDs
    static std::unordered_map<uint16_t, std::string> SubtypeGlobalMap = {
        // Combat subtypes
        {26, "STFU_Attack"}, {27, "STFU_PowerAttack"}, {28, "STFU_Bash"}, {29, "STFU_Hit"},
        {30, "STFU_Flee"}, {31, "STFU_Bleedout"}, {32, "STFU_AvoidThreat"}, {33, "STFU_Death"},
        {35, "STFU_Block"}, {36, "STFU_Taunt"}, {37, "STFU_AllyKilled"},
        {39, "STFU_Yield"}, {40, "STFU_AcceptYield"}, {41, "STFU_PickpocketCombat"},
        {55, "STFU_AlertIdle"}, {56, "STFU_LostIdle"}, {57, "STFU_NormalToAlert"},
        {58, "STFU_AlertToCombat"}, {59, "STFU_NormalToCombat"}, {60, "STFU_AlertToNormal"},
        {61, "STFU_CombatToNormal"}, {62, "STFU_CombatToLost"}, {63, "STFU_LostToNormal"},
        {64, "STFU_LostToCombat"}, {65, "STFU_DetectFriendDie"}, {75, "STFU_ObserveCombat"},
        
        // Non-combat subtypes
        {43, "STFU_Murder"}, {45, "STFU_MurderNC"}, {42, "STFU_Assault"}, {44, "STFU_AssaultNC"},
        {98, "STFU_ActorCollideWithActor"}, {70, "STFU_BarterExit"}, {85, "STFU_DestroyObject"},
        {78, "STFU_Goodbye"}, {79, "STFU_Hello"}, {84, "STFU_KnockOverObject"},
        {76, "STFU_NoticeCorpse"}, {88, "STFU_PickpocketTopic"}, {46, "STFU_PickpocketNC"},
        {87, "STFU_LockedObject"}, {13, "STFU_Refuse"}, {19, "STFU_MoralRefusal"},
        {18, "STFU_ExitFavorState"}, {16, "STFU_Agree"}, {15, "STFU_Show"},
        {49, "STFU_Trespass"}, {77, "STFU_TimeToGo"}, {94, "STFU_Idle"},
        {47, "STFU_StealFromNC"}, {48, "STFU_TrespassAgainstNC"},
        {93, "STFU_PlayerShout"}, {99, "STFU_PlayerInIronSights"},
        {91, "STFU_PlayerCastProjectileSpell"}, {92, "STFU_PlayerCastSelfSpell"},
        {38, "STFU_Steal"}, {80, "STFU_SwingMeleeWeapon"}, {81, "STFU_ShootBow"},
        {82, "STFU_ZKeyObject"}, {86, "STFU_StandOnFurniture"}, {74, "STFU_TrainingExit"},
        {53, "STFU_VoicePowerEndLong"}, {52, "STFU_VoicePowerEndShort"},
        {54, "STFU_VoicePowerStartLong"}, {51, "STFU_VoicePowerStartShort"},
        {50, "STFU_WerewolfTransformCrime"}, {89, "STFU_PursueIdleTopic"}
    };

    void Load()
    {
        // Generate default YAML templates if they don't exist (first-time setup)
        GenerateDefaultYAMLs();
        
        // YAMLs are now only loaded via "Import from YAML" button
        // Do NOT load YAMLs at startup - database is the single source of truth
        InitializeHardcodedScenes();
        
        // Look up TESGlobals from STFU.esp
        // These are used by scene phase conditions to dynamically block scenes
        spdlog::info("Looking up TESGlobals from STFU.esp...");
        
        auto* dataHandler = RE::TESDataHandler::GetSingleton();
        if (!dataHandler) {
            spdlog::error("TESDataHandler not available!");
            return;
        }
        
        // Helper to look up global by EditorID
        // Use LookupByEditorID which goes through the editor ID map directly,
        // as GetFormEditorID() may return null in CommonLibVR 4.5.0
        auto lookupGlobal = [](const char* editorID) -> RE::TESGlobal* {
            auto* global = RE::TESForm::LookupByEditorID<RE::TESGlobal>(editorID);
            if (global) {
                spdlog::debug("Found global '{}' with value {}", editorID, global->value);
                return global;
            }
            spdlog::warn("Global '{}' not found in loaded ESPs", editorID);
            return nullptr;
        };
        
        // Look up master toggles
        g_settings.blacklist.toggleGlobal = lookupGlobal("STFU_Blacklist");
        g_settings.mcm.blockScenesGlobal = lookupGlobal("STFU_Scenes");
        g_settings.mcm.blockBardSongsGlobal = lookupGlobal("STFU_BardSongs");
        g_settings.mcm.blockFollowerCommentaryGlobal = lookupGlobal("STFU_FollowerCommentary");
        g_settings.mcm.preserveGruntsGlobal = lookupGlobal("STFU_PreserveGrunts");
        
        // Look up subtype globals
        int foundGlobals = 0;
        for (const auto& [subtype, globalName] : SubtypeGlobalMap) {
            auto* global = lookupGlobal(globalName.c_str());
            if (global) {
                g_settings.mcm.subtypeGlobals[subtype] = global;
                foundGlobals++;
            }
        }
        
        spdlog::info("Loaded {} of {} subtype globals from STFU.esp", foundGlobals, SubtypeGlobalMap.size());
        
        spdlog::info("Config loaded successfully");
    }
    
    const Settings& GetSettings()
    {
        return g_settings;
    }
    
    uint16_t GetAccurateSubtype(RE::TESTopic* topic)
    {
        if (!topic) {
            return 0;
        }
        
        uint32_t formID = topic->GetFormID();
        auto it = VanillaSubtypeCorrections.find(formID);
        if (it != VanillaSubtypeCorrections.end()) {
            // Return corrected subtype from SNAM
            return it->second;
        }
        
        // Fall back to DATA subtype for topics not in correction map
        return static_cast<uint16_t>(topic->data.subtype.get());
    }

    std::string GetSubtypeName(uint16_t subtype)
    {
        // Build reverse lookup map (ID -> Name) exactly once. The fill runs inside
        // the initializer so C++'s thread-safe static-init guard serializes it:
        // concurrent callers block until the first finishes, then only ever read.
        // The old "declare empty, fill if empty()" form left the fill unsynchronized
        // — several dialogue hooks run on BSJobs worker threads at once (e.g. the
        // first frame after a save load), so they could fill the map simultaneously
        // and corrupt it, freezing or crashing the game.
        static const std::unordered_map<uint16_t, std::string> reverseMap = [] {
            std::unordered_map<uint16_t, std::string> map;
            for (const auto& [name, id] : SubtypeNameMap) {
                map[id] = name;
            }
            return map;
        }();

        auto it = reverseMap.find(subtype);
        if (it != reverseMap.end()) {
            return it->second;
        }
        
        // Unknown subtype - return numeric string
        return "Unknown_" + std::to_string(subtype);
    }
    
    const std::unordered_map<std::string, uint16_t>& GetSubtypeNameMap()
    {
        return SubtypeNameMap;
    }
    
    bool ToggleSubtypeFilter(uint16_t topicSubtype)
    {
        // Find the subtype's global variable
        auto subtypeGlobalIt = g_settings.mcm.subtypeGlobals.find(topicSubtype);
        if (subtypeGlobalIt == g_settings.mcm.subtypeGlobals.end() || !subtypeGlobalIt->second) {
            spdlog::warn("[Config::ToggleSubtypeFilter] Subtype {} has no MCM toggle global", topicSubtype);
            return false;  // No global for this subtype
        }
        
        RE::TESGlobal* global = subtypeGlobalIt->second;
        
        // Toggle the value (flip between 0.0 and 1.0)
        bool wasEnabled = global->value >= 0.5f;
        global->value = wasEnabled ? 0.0f : 1.0f;
        
        spdlog::info("[Config::ToggleSubtypeFilter] Toggled subtype {} filter from {} to {}", 
            topicSubtype, wasEnabled ? "enabled" : "disabled", !wasEnabled ? "enabled" : "disabled");
        
        // Save to INI so setting persists
        SettingsPersistence::SaveSettings();
        
        return true;
    }
    
    bool IsFilterCategoryEnabled(const std::string& filterCategory)
    {
        // Special case: "Blacklist" uses blacklist toggle
        if (filterCategory == "Blacklist") {
            return g_settings.blacklist.toggleGlobal && g_settings.blacklist.toggleGlobal->value >= 0.5f;
        }
        
        // Special case: "Scene" uses scene blocking toggle
        if (filterCategory == "Scene") {
            return g_settings.mcm.blockScenesGlobal && g_settings.mcm.blockScenesGlobal->value >= 0.5f;
        }
        
        // Special case: "BardSongs" uses bard songs toggle
        if (filterCategory == "BardSongs") {
            return g_settings.mcm.blockBardSongsGlobal && g_settings.mcm.blockBardSongsGlobal->value >= 0.5f;
        }
        
        // Special case: "FollowerCommentary" uses follower commentary toggle
        if (filterCategory == "FollowerCommentary") {
            return g_settings.mcm.blockFollowerCommentaryGlobal && g_settings.mcm.blockFollowerCommentaryGlobal->value >= 0.5f;
        }
        
        // Otherwise, map category name to subtype ID
        // SubtypeGlobalMap is: {subtypeID, "STFU_SubtypeName"}
        // We need to check if category matches the subtype name part (after "STFU_")
        for (const auto& [subtypeID, globalName] : SubtypeGlobalMap) {
            // Extract subtype name from "STFU_Hello" -> "Hello"
            std::string subtypeName = globalName;
            if (subtypeName.find("STFU_") == 0) {
                subtypeName = subtypeName.substr(5);  // Remove "STFU_" prefix
            }
            
            if (subtypeName == filterCategory) {
                // Found matching subtype, check if its global is enabled
                auto it = g_settings.mcm.subtypeGlobals.find(subtypeID);
                if (it != g_settings.mcm.subtypeGlobals.end() && it->second) {
                    return it->second->value >= 0.5f;
                }
                return false;  // Global not initialized
            }
        }
        
        // If filterCategory doesn't match anything, default to false (not enabled)
        return false;
    }
    
    bool ShouldBlockScenes()
    {
        // Check if STFU_Scenes MCM toggle is enabled
        if (g_settings.mcm.blockScenesGlobal) {
            return g_settings.mcm.blockScenesGlobal->value >= 0.5f;
        }
        return false;  // Default: don't block scenes
    }
    
    bool ShouldBlockBardSongs()
    {
        // Check if STFU_BardSongs MCM toggle is enabled
        if (g_settings.mcm.blockBardSongsGlobal) {
            return g_settings.mcm.blockBardSongsGlobal->value >= 0.5f;
        }
        return false;  // Default: don't block bard songs
    }
    
    RE::TESGlobal* GetScenesGlobal()
    {
        return g_settings.mcm.blockScenesGlobal;
    }
    
    RE::TESGlobal* GetBardSongsGlobal()
    {
        return g_settings.mcm.blockBardSongsGlobal;
    }

    RE::TESGlobal* GetBlacklistGlobal()
    {
        return g_settings.blacklist.toggleGlobal;
    }

    RE::TESGlobal* GetFollowerCommentaryGlobal()
    {
        return g_settings.mcm.blockFollowerCommentaryGlobal;
    }
}  // namespace Config
