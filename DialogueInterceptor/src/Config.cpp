#include "Config.h"
#include "SettingsPersistence.h"
#include "DialogueDatabase.h"
#include "PopulateTopicInfoHook.h"
#include <spdlog/spdlog.h>
#include <yaml-cpp/yaml.h>
#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <algorithm>
#include <vector>

namespace Config
{
    static Settings g_settings;
    
    // No cache - database lookups are fast enough and avoid cache invalidation issues
    
    // Auto-generated vanilla Skyrim subtype corrections
    // Subtype corrections map - cleared for testing
    // If topics need corrections, add them here as: {formID, correctSubtype}
    // Infrastructure kept for future use
    static std::unordered_map<uint32_t, uint16_t> VanillaSubtypeCorrections = {
        // Empty - using raw DATA.subtype values
    };
    
    // Map user-friendly subtype names to enum values
    static std::unordered_map<std::string, uint16_t> SubtypeNameMap = {
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

    static std::string GetWhitelistPath()
    {
        static std::string path;
        if (path.empty()) {
            wchar_t buffer[MAX_PATH];
            GetModuleFileNameW(nullptr, buffer, MAX_PATH);
            std::filesystem::path exePath(buffer);
            path = (exePath.parent_path() / "Data" / "SKSE" / "Plugins" / "STFU" / "STFU_Whitelist.yaml").string();
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
            path = (exePath.parent_path() / "Data" / "SKSE" / "Plugins" / "STFU" / "STFU_Blacklist.yaml").string();
        }
        return path;
    }
    
    static std::string GetSkyrimNetFilterPath()
    {
        static std::string path;
        if (path.empty()) {
            wchar_t buffer[MAX_PATH];
            GetModuleFileNameW(nullptr, buffer, MAX_PATH);
            std::filesystem::path exePath(buffer);
            path = (exePath.parent_path() / "Data" / "SKSE" / "Plugins" / "STFU" / "STFU_SkyrimNetFilter.yaml").string();
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
            path = (exePath.parent_path() / "Data" / "SKSE" / "Plugins" / "STFU" / "STFU_SubtypeOverrides.yaml").string();
        }
        return path;
    }

    static void GenerateDefaultYAMLs()
    {
        // Create STFU directory if it doesn't exist
        wchar_t buffer[MAX_PATH];
        GetModuleFileNameW(nullptr, buffer, MAX_PATH);
        std::filesystem::path exePath(buffer);
        std::filesystem::path configDir = exePath.parent_path() / "Data" / "SKSE" / "Plugins" / "STFU";
        
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
  
plugins:
  # Block ALL dialogue from specific plugins (use with extreme caution!)
  # Example:
  # - "AnnoyingMod.esp"
  
scenes:
  # Block entire scenes (HARD BLOCK - may break quests!)
  # Examples:
  # - WhiterunMikaelSongScene
  # - WICraftItem01Scene
  
quests:
  # Block all dialogue topics referenced by a quest
  # Example:
  # - DA07MuseumScenes
  
quest_patterns:
  # Use * wildcard to match multiple quests with similar names
  # EditorIDs only
  # Example:
  # - "DialogueGeneric*"
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
  
quest_patterns:
  # Use * wildcard to protect multiple quests
  # EditorIDs only
  # Example:
  # - "MainQuest*"
)";
                file.close();
                spdlog::info("[Config] Generated default STFU_Whitelist.yaml");
            } catch (const std::exception& e) {
                spdlog::error("[Config] Failed to generate Whitelist YAML: {}", e.what());
            }
        }
        
        // Generate SkyrimNet Filter YAML if it doesn't exist
        std::string skyrimNetPath = GetSkyrimNetFilterPath();
        if (!std::filesystem::exists(skyrimNetPath)) {
            try {
                std::ofstream file(skyrimNetPath);
                file << R"(# ============================================================================
#                    STFU SkyrimNet Filter Configuration
# ============================================================================
# Blocks dialogue from being logged to SkyrimNet event history
# Does NOT affect audio, subtitles, or scripts - purely for filtering logs
# Only works with MENU DIALOGUE (check [Menu] tag in dialogue log)
# After making changes, use "Import from YAML" in MCM Settings page
#
# IDENTIFIER FORMATS:
#   - EditorID (recommended): TopicEditorID
#   - FormKey: 0x012345 or 0x012345:PluginName.esp
#
# COMMON USE CASES:
#   - Follower commands (trade, wait, follow, dismiss)
#   - Service dialogue (merchants, innkeepers, trainers)
#   - Repetitive greetings (Hello subtype)
#   - Collision/environmental dialogue
# ============================================================================

topics:
  # Filter specific dialogue topics from SkyrimNet logs
  # Examples:
  # - DialogueFollowerDismissTopic  # Follower commands
  # - 0x07F6BB                      # OfferServiceTopic
  
quests:
  # Filter all dialogue from specific quests
  # Examples:
  # - nwsFollowerController  # NFF follower management
  # - sosQuest               # Simple Outfit System
  
subtypes:
  # Filter entire dialogue subtypes from logs
  # WARNING: This can filter a LOT of dialogue!
  # Common options: Hello, Idle, Combat, Detection
  # Examples:
  # - Hello              # All greeting dialogue
  # - KnockOverObject    # Environmental reactions
)";
                file.close();
                spdlog::info("[Config] Generated default STFU_SkyrimNetFilter.yaml");
            } catch (const std::exception& e) {
                spdlog::error("[Config] Failed to generate SkyrimNet Filter YAML: {}", e.what());
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

    static void LoadWhitelist()
    {
        std::string configPath = GetWhitelistPath();
        
        if (!std::filesystem::exists(configPath)) {
            return;
        }

        try {
            YAML::Node config = YAML::LoadFile(configPath);
            
            // Load whitelisted topics
            if (config["topics"] && config["topics"].IsSequence()) {
                for (const auto& entry : config["topics"]) {
                    if (entry.IsScalar()) {
                        std::string value = entry.as<std::string>();
                        
                        if (value.substr(0, 2) == "0x" || value.substr(0, 2) == "0X") {
                            uint32_t formID = std::stoul(value, nullptr, 16);
                            g_settings.whitelist.topicFormIDs.insert(formID);
                        } else {
                            g_settings.whitelist.topicEditorIDs.insert(value);
                        }
                    }
                }
            }
            
            // Load whitelisted quests
            if (config["quests"] && config["quests"].IsSequence()) {
                for (const auto& entry : config["quests"]) {
                    if (entry.IsScalar()) {
                        std::string value = entry.as<std::string>();
                        
                        if (value.substr(0, 2) == "0x" || value.substr(0, 2) == "0X") {
                            uint32_t formID = std::stoul(value, nullptr, 16);
                            g_settings.whitelist.questFormIDs.insert(formID);
                        } else {
                            g_settings.whitelist.questEditorIDs.insert(value);
                        }
                    }
                }
            }
            
            // Load whitelisted subtypes
            if (config["subtypes"] && config["subtypes"].IsSequence()) {
                for (const auto& entry : config["subtypes"]) {
                    if (entry.IsScalar()) {
                        std::string subtypeName = entry.as<std::string>();
                        
                        auto it = SubtypeNameMap.find(subtypeName);
                        if (it != SubtypeNameMap.end()) {
                            g_settings.whitelist.subtypes.insert(it->second);
                        } else {
                            spdlog::warn("Unknown subtype name in Whitelist: {}", subtypeName);
                        }
                    }
                }
            }
            
            int totalTopics = g_settings.whitelist.topicFormIDs.size() + g_settings.whitelist.topicEditorIDs.size();
            int totalQuests = g_settings.whitelist.questFormIDs.size() + g_settings.whitelist.questEditorIDs.size();
            int totalSubtypes = g_settings.whitelist.subtypes.size();
            
            if (totalTopics > 0 || totalQuests > 0 || totalSubtypes > 0) {
                spdlog::info("Loaded Whitelist (never block): {} topics, {} quests, {} subtypes", 
                    totalTopics, totalQuests, totalSubtypes);
            }
        } catch (const YAML::Exception& e) {
            spdlog::error("Whitelist YAML parse error: {}", e.what());
        }
    }
    
    static void LoadBlacklist()
    {
        std::string configPath = GetBlacklistPath();
        
        if (!std::filesystem::exists(configPath)) {
            return;
        }

        try {
            YAML::Node config = YAML::LoadFile(configPath);
            
            // Load blocked topics
            if (config["topics"] && config["topics"].IsSequence()) {
                for (const auto& entry : config["topics"]) {
                    if (entry.IsScalar()) {
                        std::string value = entry.as<std::string>();
                        
                        if (value.substr(0, 2) == "0x" || value.substr(0, 2) == "0X") {
                            uint32_t formID = std::stoul(value, nullptr, 16);
                            g_settings.blacklist.topicFormIDs.insert(formID);
                        } else {
                            g_settings.blacklist.topicEditorIDs.insert(value);
                        }
                    }
                }
            }
            
            // Load blocked quests
            if (config["quests"] && config["quests"].IsSequence()) {
                for (const auto& entry : config["quests"]) {
                    if (entry.IsScalar()) {
                        std::string value = entry.as<std::string>();
                        
                        if (value.substr(0, 2) == "0x" || value.substr(0, 2) == "0X") {
                            uint32_t formID = std::stoul(value, nullptr, 16);
                            g_settings.blacklist.questFormIDs.insert(formID);
                        } else {
                            g_settings.blacklist.questEditorIDs.insert(value);
                        }
                    }
                }
            }
            
            // DISABLED: Subtype blocking is now handled by MCM globals (soft block only)
            // YAML blacklist subtypes caused hard blocking which prevented logging
            // Subtypes should never be hard blocked - only soft blocked via MCM
            /*
            if (config["subtypes"] && config["subtypes"].IsSequence()) {
                for (const auto& entry : config["subtypes"]) {
                    if (entry.IsScalar()) {
                        std::string subtypeName = entry.as<std::string>();
                        
                        auto it = SubtypeNameMap.find(subtypeName);
                        if (it != SubtypeNameMap.end()) {
                            g_settings.blacklist.subtypes.insert(it->second);
                        } else {
                            spdlog::warn("Unknown subtype name in Blacklist: {}", subtypeName);
                        }
                    }
                }
            }
            */
            
            int totalTopics = g_settings.blacklist.topicFormIDs.size() + g_settings.blacklist.topicEditorIDs.size();
            int totalQuests = g_settings.blacklist.questFormIDs.size() + g_settings.blacklist.questEditorIDs.size();
            
            if (totalTopics > 0 || totalQuests > 0) {
                spdlog::info("Loaded Blacklist YAML (hard block): {} topics, {} quests", 
                    totalTopics, totalQuests);
            }
        } catch (const YAML::Exception& e) {
            spdlog::error("Blacklist YAML parse error: {}", e.what());
        }
    }
    
    static void LoadSkyrimNetFilter()
    {
        std::string configPath = GetSkyrimNetFilterPath();
        
        if (!std::filesystem::exists(configPath)) {
            return;
        }

        try {
            YAML::Node config = YAML::LoadFile(configPath);
            
            // Load blocked topics
            if (config["topics"] && config["topics"].IsSequence()) {
                for (const auto& entry : config["topics"]) {
                    if (entry.IsScalar()) {
                        std::string value = entry.as<std::string>();
                        
                        if (value.substr(0, 2) == "0x" || value.substr(0, 2) == "0X") {
                            uint32_t formID = std::stoul(value, nullptr, 16);
                            g_settings.skyrimNetFilter.topicFormIDs.insert(formID);
                        } else {
                            g_settings.skyrimNetFilter.topicEditorIDs.insert(value);
                        }
                    }
                }
            }
            
            // Load blocked quests
            if (config["quests"] && config["quests"].IsSequence()) {
                for (const auto& entry : config["quests"]) {
                    if (entry.IsScalar()) {
                        std::string value = entry.as<std::string>();
                        
                        if (value.substr(0, 2) == "0x" || value.substr(0, 2) == "0X") {
                            uint32_t formID = std::stoul(value, nullptr, 16);
                            g_settings.skyrimNetFilter.questFormIDs.insert(formID);
                        } else {
                            g_settings.skyrimNetFilter.questEditorIDs.insert(value);
                        }
                    }
                }
            }
            
            // Load blocked subtypes
            if (config["subtypes"] && config["subtypes"].IsSequence()) {
                for (const auto& entry : config["subtypes"]) {
                    if (entry.IsScalar()) {
                        std::string subtypeName = entry.as<std::string>();
                        
                        auto it = SubtypeNameMap.find(subtypeName);
                        if (it != SubtypeNameMap.end()) {
                            g_settings.skyrimNetFilter.subtypes.insert(it->second);
                        } else {
                            spdlog::warn("Unknown subtype name in SkyrimNetFilter: {}", subtypeName);
                        }
                    }
                }
            }
            
            int totalTopics = g_settings.skyrimNetFilter.topicFormIDs.size() + g_settings.skyrimNetFilter.topicEditorIDs.size();
            int totalQuests = g_settings.skyrimNetFilter.questFormIDs.size() + g_settings.skyrimNetFilter.questEditorIDs.size();
            int totalSubtypes = g_settings.skyrimNetFilter.subtypes.size();
            
            if (totalTopics > 0 || totalQuests > 0 || totalSubtypes > 0) {
                spdlog::info("Loaded SkyrimNetFilter (logging only): {} topics, {} quests, {} subtypes", 
                    totalTopics, totalQuests, totalSubtypes);
            }
        } catch (const YAML::Exception& e) {
            spdlog::error("SkyrimNetFilter YAML parse error: {}", e.what());
        }
    }

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
    
    static void InitializeHardcodedScenes()
    {
        // Hardcoded ambient scene topic/scene EditorIDs from Synthesis patcher
        // These are vanilla + DLC ambient scenes that should be blockable when STFU_Scenes is enabled
        // Complete list of ~900 scene EditorIDs - checked against topic->GetFormEditorID()
        // NOTE: All stored in lowercase for case-insensitive matching
        std::vector<std::string> sceneNames = {
            "AddvarHouse2", "AddvarHouseScene1", "AncanoMirabelleScene01", "AngasMillCommonHouseScene01", "AngasMillCommonHouseScene02",
            "ApothecaryScene", "ArgonianScene1", "ArnielBrelynaScene01", "ArnielEnthirScene01", "ArnielEnthirScene02",
            "ArnielEnthirSceneDragon01", "ArnielNiryaScene01", "ArnielNiryaScene02", "AtWork", "BardsLunch",
            "BetridBoliMarketScene1", "BetridBoliMarketScene2", "BetridKerahMarketScene2", "BetridKerahScene1", "BirnaEnthirScene01",
            "BirnaEnthirScene02", "BlacksmithConversation", "BrelynaOnmundDormScene01", "BrelynaOnmundDormScene2", "BrinasHouseScene01",
            "BrinasHouseScene02", "BrylingFalkTryst", "BrylingScene1", "BrylingScene2", "BuyingARound",
            "CaravanScene1Scene", "CaravanScene2Scene", "CaravanScene3Scene", "CaravanScene4Scene", "CaravanScene5Scene",
            "CaravanScene6Scene", "CaravanScene7Scene", "CaravanScene8Scene", "CidhnaMinePrisoner01Scene", "CidhnaMinePrisoner03",
            "CidhnaMinePrisonerScene02", "CidhnaMinePrisonerScene04", "ColetteDrevisScene01", "CollegeGHallScene06", "CommanderScene1",
            "CommanderScene2", "CourtScene2", "DamphallMine01SceneQuestDialogue", "DawnstarIntroBrinaScene",
            "DGScene04", "DGSceneSpecial01", "DialogueBrandyMugFarmScene1", "DialogueBrandyMugFarmScene2", "DialogueBrandyMugFarmScene3",
            "DialogueCidhnaMineBlathlocGrisvar02Scene", "DialogueCompanionsAelaNjadaScene1", "DialogueCompanionsAelaNjadaScene2", "DialogueCompanionsFarkasAthisScene1", "DialogueCompanionsFarkasAthisScene2",
            "DialogueCompanionsFarkasTorvarScene1", "DialogueCompanionsFarkasTorvarScene2", "DialogueCompanionsKodlakAelaScene1", "DialogueCompanionsKodlakAelaScene2", "DialogueCompanionsKodlakFarkasScene1",
            "DialogueCompanionsKodlakFarkasScene2", "DialogueCompanionsKodlakSkjorScene1", "DialogueCompanionsKodlakSkjorScene2", "DialogueCompanionsKodlakTorvarScene1", "DialogueCompanionsKodlakTorvarScene2",
            "DialogueCompanionsRiaVilkasScene1", "DialogueCompanionsRiaVilkasScene2", "DialogueCompanionsRiaVilkasScene3", "DialogueCompanionsSkjorAelaScene1", "DialogueCompanionsSkjorAelaScene2",
            "DialogueCompanionsSkjorNjadaScene1", "DialogueCompanionsSkjorNjadaScene2", "DialogueCompanionsTorvarAthisScene1", "DialogueCompanionsTorvarAthisScene2", "DialogueDarkwaterCrossingAnnekeVernerScene1",
            "DialogueDarkwaterCrossingAnnekeVernerScene2", "DialogueDarkwaterCrossingAnnekeVernerScene3", "DialogueDarkwaterCrossingHrefna1", "DialogueDarkwaterCrossingHrefna2", "DialogueDarkwaterCrossingHrefna4",
            "DialogueDarkwaterCrossingInn4", "DialogueDarkwaterCrossingInn5", "DialogueDawnstarWindpeakInnScene04View", "DialogueDawnstarWindpeakInnScene05View", "DialogueDawnstarWindpeakInnScene06View",
            "DialogueDawnstarWindpeakInnScene07View", "DialogueDawnstarWindpeakInnScene08View", "DialogueDawnstarWindpeakInnScene09View", "DialogueDushnikhYalBlacksmithingScene01View", "DialogueDushnikhYalBlacksmithingScene02View",
            "DialogueDushnikhYalBlacksmithingScene03View", "DialogueDushnikhYalExterior01Scene", "DialogueDushnikhYalExterior02Scene", "DialogueDushnikhYalLonghouse01Scene", "DialogueDushnikhYalLonghouse02Scene",
            "DialogueDushnikhYalLonghouse03Scene", "DialogueDushnikhYalMine01Scene", "DialogueDushnikhYalMine02Scene", "DialogueGenericSceneDog01Scene", "DialogueHlaaluFarmScene1",
            "DialogueHlaaluFarmScene2", "DialogueHlaaluFarmScene3", "DialogueHollyfrostFarmTulvurHilleviScene1", "DialogueHollyfrostFarmTulvurHilleviScene2", "DialogueHollyfrostFarmTulvurTorstenScene1",
            "DialogueHollyfrostFarmTulvurTorstenScene2", "DialogueIvarsteadFellstarFarmScene01SCN", "DialogueIvarsteadFellstarFarmScene02SCN", "DialogueIvarsteadFellstarFarmScene03SCN", "DialogueIvarsteadInnScene01SCN",
            "DialogueIvarsteadInnScene02SCN", "DialogueIvarsteadInnScene03SCN", "DialogueIvarsteadInnScene04SCN", "DialogueIvarsteadInnScene05SCN", "DialogueIvarsteadInnScene06SCN",
            "DialogueIvarsteadKlimmekHouseScene01SCN", "DialogueIvarsteadKlimmekHouseScene02SCN", "DialogueIvarsteadTembasMillScene01SCN", "DialogueIvarsteadTembasMillScene02SCN", "DialogueKarthwastenMinersBarracksScene01View",
            "DialogueKarthwastenMinersBarracksScene02view", "DialogueKarthwastenMinersBarracksScene03View", "DialogueKarthwastenMinersBarracksScene04View", "DialogueKarthwastenMinersBarracksScene05View", "DialogueKarthwastenMinersBarracksScene06View",
            "DialogueKarthwastenMinersBarracksScene07View", "DialogueKarthwastenMinersBarracksScene08View", "DialogueKarthwastenMinersHouseScene03View", "DialogueKarthwastenOutsideScene01View", "DialogueKarthwastenOutsideScene02View",
            "DialogueKarthwastenSanuarachMineScene04View", "DialogueKarthwastenT01EnmonsHouseScene02View", "DialogueKarthwastenT01EnmonsHouseScene03View", "DialogueKarthwastenT01EnmonsHouseScene04View", "DialogueKolskeggrMineHouseScene02View",
            "DialogueKolskeggrMineScene01View", "DialogueKolskeggrMineScene02View", "DialogueKolskeggrMineScene03View", "DialogueKynesgroveEskilGala01TGCoKGScene", "DialogueKynesgroveEskilGala02TGCoKGScene",
            "DialogueKynesgroveEskilGala03TGCoKGScene", "DialogueKynesgroveEskilGala04TGCoKGScene", "DialoguekynesgroveEskilPilgrim01Scene01TGCoKGScene01", "DialogueKynesgroveGannaGemmaScene1", "DialogueKynesgroveGannaGemmaScene2",
            "DialogueKynesgroveGannaGemmaScene3", "DialogueKynesgroveGannaGemmaScene4", "DialogueKynesgroveGannaGemmaScene5", "DialogueKynesgroveGannaGemmaScene6", "DialogueKynesgroveKjeldDravyneaMineScene1",
            "DialogueKynesgroveKjeldDravyneaMineScene2", "DialogueKynesgroveKjeldDravyneaMineScene3", "DialogueKynesgroveKjeldYoungerIddraScene1", "DialogueKynesgroveKjeldYoungerIddraScene2", "DialogueKynesgroveKjeldYoungerIddraScene3",
            "DialogueKynesgroveKjeldYoungerKjeldScene1", "DialogueKynesgroveKjeldYoungerKjeldScene2", "DialogueKynesgroveKjeldYoungerKjeldScene3", "DialogueKynesgrovePilgrim01Pilgrim0201TGCoKGScene01", "DialogueKynesgrovePilgrim01Pilgrim0202TGCoKScene02",
            "DialogueKynesgrovePilgrim02Eskil01TGCoKGScene01", "DialogueKynesgroveRoggiDravyneaMineScene1", "DialogueKynesgroveRoggiDravyneaMineScene2", "DialogueKynesgroveRoggiIddraScene1", "DialogueKynesgroveRoggiIddraScene2",
            "DialogueKynesgroveRoggiIddraScene3", "DialogueKynesgroveRoggiKjeldInnScene1", "DialogueKynesgroveRoggiKjeldInnScene2", "DialogueKynesgroveRoggiKjeldInnScene3", "DialogueKynesgroveRoggiKjeldScene1",
            "DialogueKynesgroveRoggiKjeldScene2", "DialogueKynesgroveRoggiKjeldScene3", "DialogueLeftHandMineDaighresHouse01Scene", "DialogueLeftHandMineDaighresHouse02View", "DialogueLeftHandMineDaighreSkaggi02Scene",
            "DialogueLeftHandMineIntroSceneView", "DialogueLeftHandMineMinersBarracks01Scene", "DialogueLeftHandMineMinersBarracks02View", "DialogueLeftHandMineScene04View", "DialogueMarkarthArnleifandSonsScene05View",
            "DialogueMarkarthArnleifandSonsScene06View", "DialogueMarkarthBlacksmithScene01View", "DialogueMarkarthDragonsKeepScene01View", "DialogueMarkarthDragonsKeepScene02View", "DialogueMarkarthDragonsMarketScene01View",
            "DialogueMarkarthDragonsRiversideScene01View", "DialogueMarkarthGenericScene01View", "DialogueMarkarthGenericScene02View", "DialogueMarkarthGenericScene03View", "DialogueMarkarthHagsCureSceneMuiriBothela03Scene",
            "DialogueMarkarthHagsCureSceneMuiriBothela04Scene", "DialogueMarkarthIntroArnleifandSonsSceneView", "DialogueMarkarthIntroBlacksmithSceneView", "DialogueMarkarthIntroSmelterSceneView", "DialogueMarkarthKeepIntroCourtSceneView",
            "DialogueMarkarthPostAttackScene01View", "DialogueMarkarthRiversideScene01View", "DialogueMarkarthRiversideScene02View", "DialogueMarkarthRiversideScene03View", "DialogueMarkarthRiversideScene04View",
            "DialogueMarkarthRiversideScene07View", "DialogueMarkarthRiversideScene08View", "DialogueMarkarthRiversideScene09View", "DialogueMarkarthRiversideScene10View", "DialogueMarkarthRiversideScene11View",
            "DialogueMarkarthRiversideScene12View", "DialogueMarkarthSilverFishInnScene16View", "DialogueMarkarthSilverFishInnScene17View", "DialogueMarkarthStablesBanningCedranScene01View", "DialogueMarkarthStablesBanningCedranScene02View",
            "DialogueMorKhazgurExterior02Scene", "DialogueMorKhazgurHuntingScene02View", "DialogueMorKhazgurLonghouse01Scene", "DialogueMorKhazgurLonghouse02Scene", "DialogueNarzulburBolarYatulScene1",
            "DialogueNarzulburBolarYatulScene2", "DialogueNarzulburBolarYatulScene3", "DialogueNarzulburBolarYatulScene4", "DialogueNarzulburMauhulakhBolarScene1", "DialogueNarzulburMauhulakhBolarScene2",
            "DialogueNarzulburMauhulakhDushnamubScene1", "DialogueNarzulburMauhulakhDushnamubScene2", "DialogueNarzulburMauhulakhDushnamubScene3", "DialogueNarzulburMauhulakhUrogScene1", "DialogueNarzulburMauhulakhUrogScene2",
            "DialogueNarzulburMauhulakhUrogScene3", "DialogueNarzulburMauhulakhYatulScene1", "DialogueNarzulburMauhulakhYatulScene2", "DialogueNarzulburMulGadbaScene1", "DialogueNarzulburMulGadbaScene2",
            "DialogueNarzulburMulGadbaScene3", "DialogueNarzulburMulGadbaScene4", "DialogueNarzulburUrogYatulScene1", "DialogueNarzulburUrogYatulScene2", "DialogueNarzulburUrogYatulScene3",
            "DialogueOldHroldanHangedManInnScene05View", "DialogueOldHroldanHangedManInnScene06View", "DialogueRiftenGrandPlazaScene019",
            "DialogueRiftenSS02Scene", "DialogueRiverwoodSceneParts", "DialogueSalviusFarmSceneAView", "DialogueSalviusFarmSceneDView",
            "DialogueSalviusSceneCView", "DialogueSceneSigridAlvor", "DialogueSceneSigridEmbry", "DialogueSceneSigridHilde", "DialogueShorsStoneGatheringScene01SCN",
            "DialogueShorsStoneGatheringScene02SCN", "DialogueShorsStoneGatheringScene03SCN", "DialogueShorsStoneGatheringScene04SCN", "DialogueShorsStoneGatheringScene05SCN", "DialogueShorsStoneGatheringScene06SCN",
            "DialogueSolitudePalaceScene10ElisifBolgeir", "DialogueSolitudePalaceScene5FalkErikur", "DialogueSolitudePalaceScene6BrylingFalk", "DialogueSolitudePalaceScene7FalkBrylingSybille", "DialogueSolitudePalaceScene8ErikurFalkElisif",
            "DialogueSolitudePalaceScene9ElisifFalkBolgeir", "DialogueSoljundsMineHouseScene01View", "DialogueSoljundsMineHouseScene02View", "DialogueSoljundsMineHouseScene03View", "DialogueSoljundsMineHouseScene04View",
            "DialogueSoljundsMineScene01view", "DialogueSoljundsMineScene02view", "DialogueWhiterunAnoriathBrenuinScene1Scene", "DialogueWhiterunAnoriathOlfinaScene1Scene", "DialogueWhiterunAnoriathYsoldaScene1Scene",
            "DialogueWhiterunCarlottaBrenuinScene1Scene", "DialogueWhiterunCarlottaNazeemScene1View", "DialogueWhiterunCarlottaOlfinaScene1Scene", "DialogueWhiterunCarlottaYsoldaScene1Scene", "DialogueWhiterunDagnyFrothar1Scene",
            "DialogueWhiterunFraliaYsoldaScene1View", "DialogueWhiterunHrongarBalgruuf1Scene", "DialogueWhiterunHrongarProventus1Scene", "DialogueWhiterunHrongarProventus2Scene", "DialogueWhiterunIrilethBalgruuf1Scene",
            "DialogueWhiterunIrilethProventus1Scene", "DialogueWhiterunNazeemYsoldaScene1View", "DialogueWhiterunOlfinaYsoldaScene1View", "DialogueWhiterunProventusBalgruuf1Scene", "DialogueWhiterunSceneVignarBalgruuf1",
            "DialogueWhiterunTempleCastOnSoldier", "DialogueWhiterunTempleFarmerScene", "DialogueWhiterunTempleHealFarmerScene", "DialogueWhiterunTempleHealSoldierScene", "DialogueWhiterunTempleSoldierScene",
            "DialogueWhiterunTempleTalkFarmerScene", "DialogueWhiterunTempleTalkSoldierScene", "DialogueWhiterunYsoldaBrenuinScene1Scene", "DialogueWindhelmCandlehearthEldaCalixtoScene7", "DialogueWindhelmCandlehearthEldaLonelyGaleScene6",
            "DialogueWindhelmCandlehearthHallScene1", "DialogueWindhelmCandlehearthHallScene2", "DialogueWindhelmCandlehearthHallScene3", "DialogueWindhelmEldaLonelyGaleScene1", "DialogueWindhelmMarketSceneBrunwulfAvalScene",
            "DialogueWindhelmMarketSceneJovaNiranyeScene", "DialogueWindhelmMarketSceneLonelyGaleNiranyeScene", "DialogueWindhelmMarketSceneNilsineHilleviScene", "DialogueWindhelmMarketSceneTorbjornHilleviScene", "DialogueWindhelmMarketSceneTorbjornNiranyeScene",
            "DialogueWindhelmMarketSceneTorstenHilleviScene", "DialogueWindhelmMarketSceneTorstenNiranyeScene", "DialogueWindhelmMarketSceneTovaAvalScene", "DialogueWindhelmNurelionQuintusScene1", "DialogueWindhelmNurelionQuintusScene2",
            "DialogueWindhelmNurelionQuintusScene3", "DialogueWindhelmPalaceUlfricGormlaithScene9", "DialogueWindhelmRevynNiranyeScene1", "DialogueWindhelmUlfricGormlaithScene1", "DialogueWindhelmUlfricTorstenScene1",
            "DialogueWindhelmViolaBothersLonelyGaleScene1", "DialogueWindhelmViolaBothersLonelyGaleScene2", "DialogueWindhelmViolaBothersLonelyGaleScene3", "DialogueWindhelmViolaBothersLonelyGaleScene4", "DialogueWinterholdBirnasHouseScene1",
            "DialogueWinterholdBirnasHouseScene2", "DialogueWinterholdInitScene", "DialogueWinterholdInnInitialScene", "DLC1HunterBaseScene1", "DLC1HunterBaseScene10",
            "DLC1HunterBaseScene2", "DLC1HunterBaseScene3", "DLC1HunterBaseScene4", "DLC1HunterBaseScene5", "DLC1HunterBaseScene6",
            "DLC1HunterBaseScene7", "DLC1HunterBaseScene8", "DLC1HunterBaseScene9", "DLC1VampireBaseScene01", "DLC1VampireBaseScene02",
            "DLC1VampireBaseScene03", "DLC1VampireBaseScene04", "DLC1VampireBaseScene05", "DLC1VampireBaseScene06", "DLC2DrovasElyneaScene01",
            "DLC2DrovasElyneaScene02", "DLC2DrovasNelothScene01", "DLC2DrovasNelothScene02", "DLC2DrovasTalvasScene01", "DLC2DrovasTalvasScene02",
            "DLC2MHBujoldElmusReclaimScene01Scene", "DLC2MHBujoldElmusScene01Scene", "DLC2MHBujoldHilundReclaimScene01Scene", "DLC2MHBujoldHilundScene01", "DLC2MHBujoldKuvarReclaimScene01Scene",
            "DLC2MHBujoldKuvarReclaimScene02Scene", "DLC2MHBujoldKuvarReclaimScene03Scene", "DLC2MHBujoldKuvarScene01Scene", "DLC2MHBujoldKuvarScene02Scene", "DLC2MHBujoldKuvarScene03Scene",
            "DLC2MHElmusKuvarReclaimScene01Scene", "DLC2MHElmusKuvarScene01Scene", "DLC2MHElmusKuvarScene02Scene", "DLC2MHKuvarHilundReclaimScene01SCene", "DLC2MHKuvarHilundScene01Scene",
            "DLC2MQ02FreaTempleScene", "DLC2RRAlorHouseScene001", "DLC2RRAlorHouseScene002", "DLC2RRAnyLocScene001", "DLC2RRAnyLocScene0016",
            "DLC2RRAnyLocScene002", "DLC2RRAnyLocScene003", "DLC2RRAnyLocScene004", "DLC2RRAnyLocScene005", "DLC2RRAnyLocScene006",
            "DLC2RRAnyLocScene007", "DLC2RRAnyLocScene008", "DLC2RRAnyLocScene009", "DLC2RRAnyLocScene010", "DLC2RRAnyLocScene011",
            "DLC2RRAnyLocScene012", "DLC2RRAnyLocScene013", "DLC2RRAnyLocScene014", "DLC2RRAnyLocScene015", "DLC2RRAnyLocScene017",
            "DLC2RRAnyLocScene018", "DLC2RRAnyLocScene019", "DLC2RRAnyLocScene020", "DLC2RRAnyLocScene021", "DLC2RRArrivalGoScene",
            "DLC2RRBeggarBralsaScene01", "DLC2RRCresciusHouseScene001", "DLC2RRCresciusHouseScene002", "DLC2RREstablishingScene00", "DLC2RRIenthFarmScene002",
            "DLC2RRMorvaynManorScene001", "DLC2RRMorvaynManorScene002", "DLC2RRMorvaynManorScene003", "DLC2RRMorvaynManorScene004", "DLC2RRMorvaynManorScene005",
            "DLC2RRMorvaynManorScene006", "DLC2RRNetchScene001", "DLC2RRNetchScene002", "DLC2RRNetchScene003", "DLC2RRNetchScene004",
            "DLC2RRNetchScene005", "DLC2RRNetchScene006", "DLC2RRNetchScene007", "DLC2RRNetchScene008", "DLC2RRNetchScene009",
            "DLC2RRNetchScene010", "DLC2RROthrelothPreachingScene00", "DLC2RRSeverinManorScene001", "DLC2RRSeverinManorScene002", "DLC2RRSeverinManorScene003",
            "DLC2RRTempleScene001", "DLC2RRTempleScene002", "DLC2RRTempleScene003", "DLC2SVBaldorMorwenScene01Scene", "DLC2SVBaldorMorwenScene02Scene",
            "DLC2SVBaldorMorwenScene03Scene", "DLC2SVDeorWulfScene01Scene", "DLC2SVDeorWulfScene02Scene", "DLC2SVDeorWulfScene03Scene", "DLC2SVDeorYrsaScene01Scene",
            "DLC2SVDeorYrsaScene02Scene", "DLC2SVEdlaNikulasScene01Scene", "DLC2SVMorwenTharstanScene01Scene", "DLC2SVOslafAetaScene01Scene", "DLC2SVOslafFinnaScene01Scene",
            "DLC2SVOslafFinnaScene02Scene", "DLC2SVOslafYrsaScene01Scene", "DLC2SVTharstanFanariScene01Scene", "DLC2SVTharstanFanariScene02Scene", "DLC2TelMithrynNelothTalvas01",
            "DLC2TelMithrynNelothTalvas02", "DLC2TelMithrynNelothTalvas03", "DLC2TelMithrynNelothTalvas04", "DLC2VaronaElyneaScene01", "DLC2VaronaElyneaScene02",
            "DLC2VaronaNelothScene01", "DLC2VaronaNelothScene02", "DLC2VaronaTalvasScene01", "DLC2VaronaTalvasScene02", "DLC2VaronaUlvesScene01",
            "DLC2VaronaUlvesScene02", "DragonBridgeFarmScene01", "DragonBridgeFarmScene02", "DragonBridgeHorgeirsHouseScene01", "DragonBridgeMillSceneHorgeirLodvar",
            "DragonBridgeMillSceneLodvarOlda", "DragonBridgeScene01", "DragonBridgeTavernSceneFaidaJuli", "DragonBridgeTavernSceneJuliFaida",
            "DrinkingContest", "DryGoodsCamillaLucanScene", "EasternMineScene01", "EasternMineScene02", "EndonKerahMarketScene1",
            "EndonKerahMarketScene3", "ErikurFamilyScene", "ErikurFamilyScene2", "ErikurMelaranScene", "FalkElisifAboutPower",
            "FalkreathCemeteryScene01", "FalkreathCemeteryScene02", "FalkreathCorpselightFarmScene01", "FalkreathDeadMansDrinkScene01", "FalkreathDeadMansDrinkScene02",
            "FalkreathDeadMansDrinkScene03", "FalkreathDeadMansDrinkScene04", "FalkreathDeadMansDrinkScene05", "FalkreathDeadMansDrinkScene06", "FalkreathDeadMansDrinkScene07",
            "FalkreathDeadMansDrinkScene08", "FalkreathDeadMansDrinkScene09", "FalkreathDeadMansDrinkScene10", "FalkreathDeadMansDrinkScene11", "FalkreathDengeirsHallScene01",
            "FalkreathDengeirsHallScene02", "FalkreathDengeirsHallScene03", "FalkreathGrayPineGoodsScene01", "FalkreathHouseofArkayScene01", "FalkreathHouseofArkayScene02",
            "FalkreathHouseofArkayScene03", "FalkreathJarlsLonghouseScene01", "FalkreathJarlsLonghouseScene02", "FalkreathJarlsLonghouseScene03", "FalkSybille",
            "FaraldaEnthirScene01", "FletcherScene", "FletcherScene2", "GretaLookingForWork", "HeartwoodMillScene01",
            "HeartwoodMillScene02", "HighmoonHallScene01", "HighmoonHallScene02", "HighmoonHallScene03", "HighmoonHallScene04",
            "HighmoonHallScene05", "HighmoonHallScene06", "HighmoonHallScene07", "IvarsteadSSScene", "JailerScene2",
            "JailScene1", "JornAiaScene", "JzargoOnmundDormScene01", "KatlaFamilyScene", "KidsPlaying",
            "LoreiusFarmOutsideScene01", "LoreiusFarmOutsideScene02",
            "MarkarthArnleifandsonsScene01", "MarkarthArnleifandSonsScene02", "MarkarthEndonsHouseScene01", "MarkarthEndonsHouseScene02", "MarkarthHagsCureScene01",
            "MarkarthHagsCureScene02", "MarkarthKeepScene01", "MarkarthKeepScene02", "MarkarthKeepScene03", "MarkarthKeepScene04",
            "MarkarthKeepScene05", "MarkarthKeepScene06", "MarkarthKeepScene08", "MarkarthKeepScene10", "MarkarthKeepScene10DUPLICATE001",
            "MarkarthSilverFishhInnScene13", "MarkarthSilverFishInnScene01", "MarkarthSilverFishInnScene02", "MarkarthSilverFishInnScene03", "MarkarthSilverFishInnScene04",
            "MarkarthSilverFishInnScene05", "MarkarthSilverFishInnScene06", "MarkarthSilverFishInnScene12", "MarkarthSilverFishInnScene14", "MarkarthTreasuryHouseScene1",
            "MarkarthTreasuryHouseScene10", "MarkarthTreasuryHouseScene2", "MarkarthTreasuryHouseScene3", "MarkarthTreasuryHouseScene6", "MarkarthTreasuryHouseScene7",
            "MarkarthTreasuryHouseScene8", "MarkarthWarrensScene01", "MarkarthWarrensScene03", "MarkarthWarrensScene04", "MarkarthWizardsTowerScene01",
            "MarkarthWizardsTowerScene02", "MerryfairFarmScene02", "MerryfairScene01", "MjollsHouseScene01",
            "MoorsideScene05", "MoorsideScene06", "MorthalAlvasHouseScene1", "MorthalFalionsHouseScene1", "MorthalFalionsHouseScene2",
            "MorthalFalionsHouseScene3", "MorthalFalionsHouseScene4", "MorthalMoorsideScene07", "MorthalMoorsideScene08",
            "MorthalMoorsideScene1", "MorthalMoorsideScene2", "MorthalMoorsideScene3", "MorthalMoorsideScene4", "MorthalThaumaturgistHutScene01",
            "MorthalThaumaturgistsHutScene2", "MorthalThaumaturgistsHutScene3", "NosterBegging", "PalaceScene1", "RaimentsScene",
            "RDS01Scene", "RDSScene04", "RDSScene05", "RiftenBBManorScene01", "RiftenBBManorScene02",
            "RiftenBBMeaderyScene01", "RiftenBBMeaderyScene02", "RiftenBBMeaderyScene03", "RiftenBeeAndBarbScene01", "RiftenBeeAndBarbScene02",
            "RiftenBeeAndBarbScene03", "RiftenBeeAndBarbScene04", "RiftenBeeAndBarbScene05", "RiftenBeeAndBarbScene06", "RiftenBeeAndBarbScene07",
            "RiftenBeeAndBarbScene08", "RiftenBeeAndBarbScene09", "RiftenBeeAndBarbScene10", "RiftenBeeAndBarbScene11", "RiftenBeeAndBarbScene12",
            "RiftenBeeandBarbScene13", "RiftenBeeAndBarbScene14", "RiftenBeeAndBarbScene15", "RiftenBeeAndBarbScene16", "RiftenBeeAndBarbScene17",
            "RiftenBeeAndBarbScene18", "RiftenBeeAndBarbScene19", "RiftenBeeandBarbScene20", "RiftenBeggarEddaScene", "RiftenBeggarSnilfScene",
            "RiftenElgrimsElixirsScene01", "RiftenElgrimsElixirsScene02", "RiftenElgrimsElixirsScene03", "RiftenElgrimsElixirsScene4", "RiftenFisheryScene01",
            "RiftenFisheryScene02", "RiftenFisheryScene03", "RiftenFisheryScene04", "RiftenGrandPlazaScene01", "RiftenGrandPlazaScene02",
            "RiftenGrandPlazaScene03", "RiftenGrandPlazaScene04", "RiftenGrandPlazaScene05", "RiftenGrandPlazaScene06", "RiftenGrandPlazaScene07",
            "RiftenGrandPlazaScene08", "RiftenGrandPlazaScene09", "RiftenGrandPlazaScene10", "RiftenGrandPlazaScene11", "RiftenGrandPlazaScene12",
            "RiftenGrandPlazaScene13", "RiftenGrandPlazaScene14", "RiftenGrandPlazaScene15", "RiftenGrandPlazaScene16", "RiftenGrandPlazaScene17",
            "RiftenGrandPlazaScene18", "RiftenGrandPlazaScene20", "RiftenGrandPlazaScene21", "RiftenGrandPlazaScene22", "RiftenGrandPlazaScene23",
            "RiftenGrandPlazaScene24", "RiftenGrandPlazaScene25", "RiftenGrandPlazaScene26", "RiftenGrandPlazaScene27", "RiftenGrandPlazaScene28",
            "RiftenGrandPlazaScene29", "RiftenGrandPlazaScene30", "RiftenGrandPlazaScene31", "RiftenGrandPlazaScene32", "RiftenGrandPlazaScene33",
            "RiftenGrandPlazaScene34", "RiftenGrandPlazaScene35", "RiftenGrandPlazaScene36", "RiftenGrandPlazaScene37", "RiftenGrandPlazaScene38",
            "RiftenGrandPlazaScene39", "RiftenGrandPlazaScene40", "RiftenGrandPlazaScene41", "RiftenGrandPlazaScene42", "RiftenGrandPlazaScene43",
            "RiftenHaelgasBunkhouseScene01", "RiftenHaelgasBunkhouseScene02", "RiftenHaelgasBunkhouseScene03", "RiftenHaelgasBunkhouseScene04", "RiftenHaelgasBunkhouseScene05",
            "RiftenHaelgasBunkhouseScene06", "RiftenHaelgasBunkhouseScene07", "RiftenHaelgasBunkhouseScene08", "RiftenHaelgasBunkhouseScene09", "RiftenHaelgasBunkhouseScene10",
            "RiftenHaelgasBunkhouseScene11", "RiftenHaelgasBunkhouseScene12", "RiftenHonorhallScene01", "RiftenKeepScene01",
            "RiftenKeepScene01Alternate", "RiftenKeepScene02", "RiftenKeepScene02Alternate", "RiftenKeepScene03", "RiftenKeepScene03Alternate",
            "RiftenKeepScene04", "RiftenKeepScene04Alternate", "RiftenKeepScene05", "RiftenKeepScene05Alternate", "RiftenKeepScene06",
            "RiftenKeepScene06Alternate", "RiftenKeepScene07", "RiftenKeepScene07Alternate", "RiftenKeepScene08", "RiftenKeepScene08Alternate01",
            "RiftenKeepScene09", "RiftenKeepScene10", "RiftenKeepScene11", "RiftenMjollHouseScene02", "RiftenPawnedPrawnScene01",
            "RiftenPawnedPrawnScene02", "RiftenPawnedPrawnScene03", "RiftenRaggedFlagon05Scene", "RiftenRaggedFlagonScene01", "RiftenRaggedFlagonScene02",
            "RiftenRaggedFlagonScene03", "RiftenRaggedFlagonScene04", "RiftenRaggedFlagonScene06", "RiftenRaggedFlagonScene07", "RiftenRaggedFlagonScene08",
            "RiftenRaggedFlagonScene09", "RiftenRaggedFlagonScene10", "RiftenRaggedFlagonScene11", "RiftenRaggedFlagonScene12", "RiftenSnowShodHouseScene01",
            "RiftenSnowShodHouseScene02", "RiftenSnowShodHouseScene03", "RiftenTempleofMaraScene01", "RiftenTempleofMaraScene02", "RiverwoodAlvorDortheScene1",
            "RiverwoodAlvorDortheScene2", "RiverwoodDortheFrodnarPlayScene", "RiverwoodFrodnarHodScene1", "RiverwoodIntroSceneHildeSoldierScene", "RiverwoodSceneA",
            "RiverwoodSceneEmbrySoldierScene", "RiverwoodSceneGerdurHod", "RiverwoodSceneSigridAlvorCabbage", "RiverwoodSceneSoldierHildeScene", "RiverwoodSigridDortheScene1",
            "RiverwoodSleepingGiantOrderDrinks", "RiverwoodSleepingGiantScene3", "RiverwoodSleepingGiantScene4", "RiverwoodSleepingGiantScene5", "RiverwoodSleepingGiantScene6",
            "Romance", "RoriksteadBritteSisselScene1", "RoriksteadEnnisReldithScene1", "RoriksteadSisselRouaneScene1", "RustleifSmithyScene01",
            "RustleifSmithyScene02", "SalviusFarmSceneAView", "SanHouse2", "SanHouseScene1", "SarethiFarmScene01",
            "SarethiFarmScene02", "SarethiFarmScene03", "SavosMirabelleScene01", "SavosMirabelleScene02", "SavosMirabelleScene03",
            "SavosMirabelleScene04", "SkyHavenTempleConversation02", "SkyHavenTempleConversation03", "SkyHavenTempleConversation04", "SkyHavenTempleConversationScene",
            "SleepingGiantDelphineOrgnarScene", "SleepingGiantDelphineRaevildScene", "SleepingGiantScene7", "SleepingGiantScene8", "SnowShodFarmScene01",
            "SnowShodFarmScene02", "SolitudeBardScene4", "SolitudeBardScene5", "SolitudeBardScene6", "SolitudeBardsCollegeClassGiraudScene1",
            "SolitudeBardsCollegeClassGiraudScene2", "SolitudeBardsCollegeClassIngeScene1", "SolitudeBardsCollegeClassIngeScene2", "SolitudeBardsCollegeClassPanteaScene1", "SolitudeBardsCollegeClassPanteaScene2",
            "SolitudeMarketplaceAiaEvetteScene1", "SolitudeMarketplaceAtAfAlanAdvarScene1", "SolitudeMarketplaceAtAfAlanEvetteScene1", "SolitudeMarketplaceAtAfAlanJalaScene1", "SolitudeMarketplaceIlldiAdvarScene1",
            "SolitudeMarketplaceIlldiEvetteScene1", "SolitudeMarketplaceIlldiJalaScene1", "SolitudeMarketplaceJawananAdvarScene1", "SolitudeMarketplaceJawananEvetteScene1", "SolitudeMarketplaceJawananJalaScene",
            "SolitudeMarketplaceJornAdvarScene1", "SolitudeMarketplaceJornJalaScene1", "SolitudeMarketplaceRorlundAdvarScene1", "SolitudeMarketplaceRorlundEvetteScene1", "SolitudeMarketplaceRorlundJalaScene1",
            "SolitudeMarketplaceSilanaAdvarScene1", "SolitudeMarketplaceSilanaEvetteScene1", "SolitudeMarketplaceSilanaJalaScene1", "SolitudeMarketplaceSorexAdvarScene1", "SolitudeMarketplaceSorexEvetteScene1",
            "SolitudeMarketplaceSorexJalaScene1", "SolitudeMarketplaceXanderAngelineScene1", "SolitudeMarketplaceXanderSaymaScene1", "SolitudeMarketplaceXanderTaarieScene1", "SolitudeSawmillScene",
            "SolitudeStreetOdarJawananScene1", "SolitudeStreetOdarJornScene1", "SolitudeStreetUnaJornScene1", "SolitudeStreetUnaLisetteScene1", "SolitudeStreetVivienneLisetteScene",
            "SolitudeStreetVivienneLisetteScene2", "SolitudeStreetVivienneLisetteScene3", "StonehillsGesturJesperScene1", "StonehillsGesturSwanhvirScene1", "StonehillsGesturTeebaEiScene1",
            "StonehillsMineGesturTalibScene1", "StudentTeacher", "TeachersScene", "TempleScene1", "TempleScene2",
            "TempleScene3", "UragColetteScene01", "UragColetteScene02", "UragDrevisScene02", "UragNiryaScene01",
            "UragNiryaScene02", "UragSergiusScene01", "ViniusFamilyScene", "ViniusFamilyScene2", "WarehouseWorkScene",
            "WesternMineScene01", "WesternMineScene02", "WhiteHallImperialScene01", "WhiteHallImperialScene02",
            "WhiteHallImperialScene03", "WhiteHallImperialScene04", "WhiteHallImperialScene05", "WhiteHallSonsScene01", "WhiteHallSonsScene02",
            "WhiteHallSonsScene03", "WhiteHallSonsScene04", "WhiteHallSonsScene05", "WhiteHallsonsScene06",
            "WhiterunAnoriathElrindirScene1", "WhiterunAnoriathElrindirScene2", "WhiterunAnoriathElrindirScene3",
            "WhiterunBanneredMareScene1", "WhiterunBanneredMareScene2", "WhiterunBanneredMareScene3", "WhiterunBraithAmrenScene1", "WhiterunBraithAmrenScene2",
            "WhiterunBraithLarsScene1", "WhiterunBraithLarsScene2", "WhiterunBraithLarsScene3", "WhiterunBraithSaffirScene1", "WhiterunBraithSaffirScene2",
            "WhiterunCivilWarArgueScene", "WhiterunDragonsreachScene1",
            "WhiterunHeimskrPreachScene", "WhiterunHouseBattleBornScene1", "WhiterunHouseBattleBornScene2", "WhiterunHouseGrayManeScene1", "WhiterunHouseGrayManeScene2",
            "WhiterunMarketplaceScene1", "WhiterunMarketplaceScene2", "WhiterunMarketplaceScene3", "WhiterunMarketplaceScene4a",
            "WhiterunOlfinaJonScene1", "WhiterunOlfinaJonScene2", "WhiterunOlfinaJonScene3",
            "WhiterunStablesScene1", "WhiterunStablesScene2", "WhiterunStablesScene3", "WhiterunStablesScene4", "WhiterunTempleOfKynarethScene1",
            "WhiterunVignarBrillScene1", "WhiterunVignarBrillScene2", "WhiterunVignarBrillScene3", "WIGreetingScene",
            "WindhelmDialogueAmbaryScoutsManyMarshesScene1", "WindhelmDialogueAmbarysScoutsManyMarshesScene2", "WindhelmDialogueAmbarySuvarisScene1", "WindhelmDialogueAmbarySuvarisScene2",
            "WindhelmDialogueEldaCalixtoScene1", "WindhelmDialogueEldaJoraScene1", "WindhelmDialogueEldaJoraScene2", "WindhelmDialogueUlfricGormlaith2", "WindhelmDialogueUlfricJorleifScene1",
            "WindhelmDialogueUlfricJorleifScene2", "WindhelmDialogueUlfricLonelyGaleScene1", "WindhelmDialogueUlfricLonelyGaleScene2",
            "WindhelmOengulHermirScene1", "WindhelmOengulHermirScene2", "WindhelmOengulHermirScene3", "WindhelmUlfricJorleifScene3", "WindpeakInnScene02",
            "WindpeakInnScene03", "WinterholdInnScene01", "WinterholdInnScene02", "WinterholdInnScene03", "WinterholdInnScene04",
            "WinterholdInnScene05", "WinterholdInnScene06", "WinterholdInnScene07", "WinterholdInnScene08", "WinterholdJarlsLonghouseScene01",
            "WinterholdJarlsLonghouseScene02", "WinterholdJarlsLonghouseScene03", "WinterholdKorirsHouseScene01", "WinterholdKorirsHouseScene02", "WinterholdKorirsHouseScene03",
            "WIRentRoomWalkToScene", "WITavernGreeting", "WITavernPlayerSits"
        };
        
        // Store scene names with proper casing
        for (const auto& name : sceneNames) {
            g_settings.hardcodedScenes.topicEditorIDs.insert(name);
        }
        
        spdlog::info("Initialized {} hardcoded ambient scenes", g_settings.hardcodedScenes.topicEditorIDs.size());
    }

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
        
        // Helper to look up global by EditorID - iterate all globals for robust lookup
        auto lookupGlobal = [dataHandler](const char* editorID) -> RE::TESGlobal* {
            for (auto* global : dataHandler->GetFormArray<RE::TESGlobal>()) {
                if (global) {
                    const char* formEditorID = global->GetFormEditorID();
                    if (formEditorID && _stricmp(formEditorID, editorID) == 0) {
                        spdlog::debug("Found global '{}' with value {}", editorID, global->value);
                        return global;
                    }
                }
            }
            spdlog::warn("Global '{}' not found in loaded ESPs", editorID);
            return nullptr;
        };
        
        // Look up master toggles
        g_settings.blacklist.toggleGlobal = lookupGlobal("STFU_Blacklist");
        g_settings.skyrimNetFilter.toggleGlobal = lookupGlobal("STFU_SkyrimNetFilter");
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
        
        spdlog::info("Loaded {} subtype globals from STFU.esp (6 master toggles + {} subtypes)", 
            6 + foundGlobals, foundGlobals);
        
        spdlog::info("Config loaded successfully");
    }
    
    void ClearCache()
    {
        // Cache removed - database queries are fast enough
        spdlog::debug("[Config] Cache removal complete - using direct database queries");
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
        // Build reverse lookup map (ID -> Name)
        static std::unordered_map<uint16_t, std::string> reverseMap;
        if (reverseMap.empty()) {
            for (const auto& [name, id] : SubtypeNameMap) {
                reverseMap[id] = name;
            }
        }
        
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

    bool ShouldBlockDialogue(RE::TESQuest* quest, RE::TESTopic* topic, const char* speakerName, const char* responseText)
    {
        if (!topic) {
            return false;
        }

        // Get accurate subtype for this topic
        uint16_t subtype = GetAccurateSubtype(topic);
        uint32_t topicFormID = topic->GetFormID();
        const char* topicEditorID = topic->GetFormEditorID();
        std::string editorIDStr = topicEditorID ? topicEditorID : "";
        
        // HIGHEST PRIORITY: Check plugin whitelist - if plugin is whitelisted, allow ALL dialogue from it
        auto db = DialogueDB::GetDatabase();
        if (db && topic) {
            auto* file = topic->GetFile(0);
            if (file && file->fileName) {
                std::string pluginName = file->fileName;
                if (db->IsPluginWhitelisted(pluginName)) {
                    return false;
                }
            }
        }
        
        // SECOND PRIORITY: Check database whitelist - whitelisted entries are NEVER blocked
        if (db) {
            // Check if topic is whitelisted
            if (db->IsWhitelisted(DialogueDB::BlacklistTarget::Topic, topicFormID, editorIDStr)) {
                return false;
            }
            // Check if quest is whitelisted
            if (quest) {
                uint32_t questFormID = quest->GetFormID();
                const char* questEditorID = quest->GetFormEditorID();
                std::string questEditorIDStr = questEditorID ? questEditorID : "";
                if (db->IsWhitelisted(DialogueDB::BlacklistTarget::Quest, questFormID, questEditorIDStr)) {
                    return false;
                }
            }
        }
        
        // THIRD PRIORITY: Check PreserveGrunts - if it's a grunt and PreserveGrunts == 0, allow it through
        if (responseText && g_settings.mcm.preserveGruntsGlobal) {
            if (ShouldFilterFromHistory(responseText, subtype, topic)) {
                // This is a grunt - check PreserveGrunts global
                if (g_settings.mcm.preserveGruntsGlobal->value < 0.5f) {
                    // PreserveGrunts == 0 means "don't preserve" = "allow through" (don't block)
                    return false;
                }
                // PreserveGrunts == 1 means "preserve" = continue with normal blocking logic
            }
        }
        
        // NOTE: YAML whitelist/blacklist removed - database is single source of truth
        // Use "Import from YAML" button to load YAML entries into database

        // NOTE: MCM subtype globals are NOT checked here!
        // This function is used for HARD BLOCKING (preventing dialogue from happening)
        // MCM subtype toggles should only SOFT BLOCK (silence audio/subtitles)
        // See ShouldSoftBlock for soft blocking with MCM globals

        // All blocking now comes from database - no YAML runtime checks
        // Database entries are populated via "Import from YAML" button
        return false;  // Not blocked
    }
    
    bool IsFilteredByMCM(uint32_t topicFormID, uint16_t topicSubtype)
    {
        // Check if this subtype has an MCM global and it's enabled
        auto subtypeGlobalIt = g_settings.mcm.subtypeGlobals.find(topicSubtype);
        if (subtypeGlobalIt != g_settings.mcm.subtypeGlobals.end() && subtypeGlobalIt->second) {
            // MCM global exists - 1.0 = block, 0.0 = allow
            if (subtypeGlobalIt->second->value >= 0.5f) {
                return true;  // Filtered by MCM
            }
        }
        return false;
    }
    
    bool HasDisabledSubtypeToggle(uint16_t topicSubtype)
    {
        // Check if this subtype has an MCM global but it's disabled
        auto subtypeGlobalIt = g_settings.mcm.subtypeGlobals.find(topicSubtype);
        if (subtypeGlobalIt != g_settings.mcm.subtypeGlobals.end() && subtypeGlobalIt->second) {
            // MCM global exists - check if it's disabled
            if (subtypeGlobalIt->second->value < 0.5f) {
                return true;  // Has toggle but it's disabled
            }
        }
        return false;
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
        
        // Clear the cache so decisions are re-evaluated with the new setting
        ClearCache();
        
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
        
        // Special case: "SkyrimNet" uses SkyrimNet filter toggle
        if (filterCategory == "SkyrimNet") {
            return g_settings.skyrimNetFilter.toggleGlobal && g_settings.skyrimNetFilter.toggleGlobal->value >= 0.5f;
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
    
    bool ShouldBlockSkyrimNet(RE::TESQuest* quest, RE::TESTopic* topic, const char* speakerName)
    {
        if (!topic) {
            return false;
        }

        uint32_t topicFormID = topic->GetFormID();
        
        // Call ShouldSoftBlock to compute blocking status, then re-query database for SkyrimNet flag
        ShouldSoftBlock(quest, topic, speakerName, nullptr);
        
        auto db = DialogueDB::GetDatabase();
        if (!db) return false;
        
        const char* topicEditorID = topic->GetFormEditorID();
        std::string editorIDStr = topicEditorID ? topicEditorID : "";
        
        return db->ShouldBlockSkyrimNet(topicFormID, editorIDStr);
    }
    
    void InvalidateTopicCache(uint32_t topicFormID)
    {
        // Cache removed - no invalidation needed
        spdlog::debug("[Config] Cache invalidation called for topic FormID: 0x{:08X} (no-op)", topicFormID);
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
    
    bool IsBardSongQuest(RE::TESQuest* quest)
    {
        if (!quest) {
            return false;
        }
        
        const char* questEditorID = quest->GetFormEditorID();
        if (!questEditorID) {
            return false;
        }
        
        // Check hardcoded list (only 3 quests)
        static const std::unordered_set<std::string> bardSongQuests = {
            "BardSongs",
            "BardSongsInstrumental",
            "MS05BardSongs"
        };
        
        return bardSongQuests.count(questEditorID) > 0;
    }
    
    bool ShouldFilterFromHistory(const char* responseText, uint16_t subtype, RE::TESTopic* topic)
    {
        // Filter EnterSprintBreath (95), OutOfBreath (100), and LeaveWaterBreath (102)
        if (subtype == 95 || subtype == 100 || subtype == 102) {
            return true;
        }
        
        // Filter combat grunts by response text
        if (!responseText || responseText[0] == '\0') {
            return false;
        }
        
        std::string text = responseText;
        // Trim whitespace
        text.erase(0, text.find_first_not_of(" \t\n\r"));
        text.erase(text.find_last_not_of(" \t\n\r") + 1);
        
        // Hardcoded grunt list from vanilla Skyrim (from DialogueGeneric and unique NPC voices)
        static const std::unordered_set<std::string> knownGrunts = {
            // From DialogueGeneric
            "Agh!", "Oof!", "Nargh!", "Argh!", "Weergh!", "Yeagh!", "Yearrgh!", "Hyargh!",
            "Rrarggh!", "Grrargh!", "Aggghh!", "Nyyarrggh!", "Hsssss!", "Agh...", "Ugh...",
            "Hunh...", "Gah...", "Nuh...", "Gah!", "Unf!", "Grrh!", "Nnh!",
            "Hhyyaarargghhhh!", "Rrrraaaaarrggghhhh!", "Yyyaaaarrgghh!", "Aaaayyyaarrrrgghh!",
            "Nnyyyaarrgghh!", "Hunh!", "Gah!", "Yah!", "Rargh!",
            
            // From unique NPC voices
            "Aghh..", "Nuhn!", "Aggh!", "Arrggh!", "Wagh!", "Unh!", "Yeeeaarrggh!",
            "Hhyyaarrgghh!", "Nnnnyyaarrgghh!", "Agggh!", "Grrarrgh!", "Naggh!", "Yeeaaaggh!", "Yiiee!", "Nyargh!",
            
            // Additional grunts
            "Grrragh!", "Gaaaah!", "Aaaaah!", "Nuh!", "Yeeeaaahhh!", "Ahhhhh.....",
            "Hhhhuuuuuhhh....", "Aiiiieee!", "Ahhhhhhhh!", "Eyyaargh!", "Grraaagghh!",
            "Raarrgh!", "Huuragh!", "Grrrarrggh!", "Raaarr!", "Raaarrgh!", "Grragh!",
            "Arugh!", "Yaaaargh!", "Ffffeeeargh!", "Rlyehhhgh1", "Heyargh!", "Grrr!",
            "Uf!", "Egh!", "Yergh!"
        };
        
        // Case-insensitive lookup
        for (const auto& grunt : knownGrunts) {
            if (text.size() == grunt.size() && 
                std::equal(text.begin(), text.end(), grunt.begin(),
                [](char a, char b) { return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b)); })) {
                return true;
            }
        }
        
        return false;
    }
    
    bool IsHardcodedAmbientScene(RE::TESTopic* topic)
    {
        if (!topic) {
            return false;
        }
        
        const char* topicEditorID = topic->GetFormEditorID();
        if (!topicEditorID) {
            return false;
        }
        
        // For scene topics, need to find the scene EditorID to check blacklist
        // (user blacklists by scene EditorID, not topic EditorID)
        auto quest = topic->ownerQuest;
        if (quest) {
            auto& scenesArray = quest->scenes;
            
            for (auto* scene : scenesArray) {
                if (!scene) continue;
                for (auto* action : scene->actions) {
                    if (!action || action->GetType() != RE::BGSSceneAction::Type::kDialogue) continue;
                    auto* dialogueAction = static_cast<RE::BGSSceneActionDialogue*>(action);
                    if (dialogueAction && dialogueAction->topic == topic) {
                        const char* sceneEditorID = scene->GetFormEditorID();
                        if (sceneEditorID) {
                            // Actor filtering not available at this level (no TESTopicInfo)
                            if (DialogueDB::GetDatabase()->ShouldSoftBlock(0, sceneEditorID)) {
                                return true;
                            }
                        }
                        break;
                    }
                }
            }
        }
        
        // Fallback to in-memory hardcoded list (case-sensitive, database handles most lookups)
        bool found = g_settings.hardcodedScenes.topicEditorIDs.count(topicEditorID) > 0;
        
        if (found || g_settings.hardcodedScenes.topicEditorIDs.empty()) {
            spdlog::info("[HARDCODED CHECK] Topic '{}' - Found: {}, List size: {}", 
                topicEditorID, found, g_settings.hardcodedScenes.topicEditorIDs.size());
        }
        
        return found;
    }
    
    bool IsHardcodedAmbientScene(RE::BGSScene* scene)
    {
        if (!scene) {
            return false;
        }
        
        const char* sceneEditorID = scene->GetFormEditorID();
        if (!sceneEditorID) {
            return false;
        }
        
        // Check unified blacklist (scenes have target_type=4)
        // Note: Actor filtering not applicable for scene-level checks
        if (DialogueDB::GetDatabase()->ShouldSoftBlock(0, sceneEditorID)) {
            return true;
        }
        
        // Fallback to in-memory hardcoded list (case-sensitive, database handles most lookups)
        return g_settings.hardcodedScenes.topicEditorIDs.count(sceneEditorID) > 0;
    }
    
    RE::TESGlobal* GetScenesGlobal()
    {
        return g_settings.mcm.blockScenesGlobal;
    }
    
    RE::TESGlobal* GetBardSongsGlobal()
    {
        return g_settings.mcm.blockBardSongsGlobal;
    }
    
    std::vector<std::string> GetHardcodedScenesList()
    {
        // Return scene names from the in-memory set (now stored with correct casing)
        std::vector<std::string> scenes;
        for (const auto& scene : g_settings.hardcodedScenes.topicEditorIDs) {
            scenes.push_back(scene);
        }
        return scenes;
    }
    
    std::vector<std::string> GetBardSongQuestsList()
    {
        return {"BardSongs", "BardSongsInstrumental", "MS05BardSongs"};
    }
    
    std::vector<std::string> GetFollowerCommentaryScenesList()
    {
        return {"WIFollowerChatter01Scene", "WIFollowerChatter02Scene", "WIFollowerChatter03Scene"};
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
            try {
                uint32_t formID = std::stoul(value, nullptr, 16);
                return {formID, ""};
            } catch (const std::exception& e) {
                spdlog::error("[Config] Failed to parse hex FormID: {} (error: {})", value, e.what());
            }
        }
        
        // Check if it's a hex string without "0x" prefix (e.g., "02707A" or "ABC12")
        // FormIDs are typically 6-8 hex digits, but we'll accept 1-8 for flexibility
        if (!value.empty() && value.length() <= 8) {
            bool isAllHex = true;
            for (char c : value) {
                if (!std::isxdigit(static_cast<unsigned char>(c))) {
                    isAllHex = false;
                    break;
                }
            }
            
            if (isAllHex) {
                try {
                    uint32_t formID = std::stoul(value, nullptr, 16);
                    spdlog::info("[Config] Parsed bare hex string as FormID: {} -> 0x{:08X}", value, formID);
                    return {formID, ""};
                } catch (const std::exception& e) {
                    spdlog::error("[Config] Failed to parse bare hex FormID: {} (error: {})", value, e.what());
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

    static void ImportSkyrimNetFilter()
    {
        std::string filterPath = GetSkyrimNetFilterPath();
        
        if (!std::filesystem::exists(filterPath)) {
            spdlog::debug("[Config] SkyrimNet filter YAML not found: {}", filterPath);
            return;
        }

        auto* db = DialogueDB::GetDatabase();
        if (!db) {
            spdlog::error("[Config] Database not available for SkyrimNet filter import");
            return;
        }

        try {
            spdlog::info("[Config] Importing from STFU_SkyrimNetFilter.yaml...");
            YAML::Node config = YAML::LoadFile(filterPath);
            
            int importedTopics = 0;
            int importedQuestTopics = 0;

            // Import topics
            if (config["topics"] && config["topics"].IsSequence()) {
                for (const auto& entry : config["topics"]) {
                    if (entry.IsScalar()) {
                        std::string value = entry.as<std::string>();
                        auto [formID, editorID] = ParseFormIdentifierInternal(value);

                        DialogueDB::BlacklistEntry filterEntry;
                        filterEntry.targetType = DialogueDB::BlacklistTarget::Topic;
                        filterEntry.targetFormID = formID;
                        filterEntry.targetEditorID = editorID;
                        filterEntry.filterCategory = "SkyrimNet Filter";
                        filterEntry.blockType = DialogueDB::BlockType::SkyrimNet;
                        filterEntry.blockSkyrimNet = true;
                        filterEntry.notes = "Imported from SkyrimNet Filter YAML";

                        if (db->AddToBlacklist(filterEntry, false)) {
                            importedTopics++;
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
                        // kTotal - kBranchedTotal = 8 - 2 = 6 arrays
                        for (int dialogueType = 0; dialogueType < (RE::DIALOGUE_TYPES::kTotal - RE::DIALOGUE_TYPES::kBranchedTotal); ++dialogueType) {
                            auto& topicsArray = quest->topics[dialogueType];
                            for (auto* topic : topicsArray) {
                                if (!topic) continue;

                                uint32_t topicFormID = topic->GetFormID();
                                const char* topicEditorID = topic->GetFormEditorID();

                                DialogueDB::BlacklistEntry filterEntry;
                                filterEntry.targetType = DialogueDB::BlacklistTarget::Topic;
                                filterEntry.targetFormID = topicFormID;
                                filterEntry.targetEditorID = topicEditorID ? topicEditorID : "";
                                filterEntry.filterCategory = "SkyrimNet Filter";
                                filterEntry.blockType = DialogueDB::BlockType::SkyrimNet;
                                filterEntry.blockSkyrimNet = true;

                                if (db->AddToBlacklist(filterEntry, false)) {
                                    importedQuestTopics++;
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
                                    const char* topicEditorID = topic->GetFormEditorID();

                                    DialogueDB::BlacklistEntry filterEntry;
                                    filterEntry.targetType = DialogueDB::BlacklistTarget::Topic;
                                    filterEntry.targetFormID = topicFormID;
                                    filterEntry.targetEditorID = topicEditorID ? topicEditorID : "";
                                    filterEntry.filterCategory = "SkyrimNet Filter";
                                    filterEntry.blockType = DialogueDB::BlockType::SkyrimNet;
                                    filterEntry.blockSkyrimNet = true;

                                    if (db->AddToBlacklist(filterEntry, false)) {
                                        importedQuestTopics++;
                                    }
                                }
                            }
                        }
                    }
                }
            }

            spdlog::info("[Config] SkyrimNet filter import complete: {} direct topics, {} quest topics", 
                importedTopics, importedQuestTopics);

        } catch (const YAML::Exception& e) {
            spdlog::error("[Config] SkyrimNet filter YAML parse error: {}", e.what());
        }
    }
    
    void ImportYAMLToDatabase()
    {
        spdlog::info("[Config] Starting YAML import to database...");
        
        auto* db = DialogueDB::GetDatabase();
        if (!db) {
            spdlog::error("[Config] Database not available for YAML import");
            return;
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
                                    const char* topicEditorID = topic->GetFormEditorID();

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
                                        const char* topicEditorID = topic->GetFormEditorID();

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
                                    const char* topicEditorID = topic->GetFormEditorID();

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
                                        const char* topicEditorID = topic->GetFormEditorID();

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
        
        // Import SkyrimNet filter
        ImportSkyrimNetFilter();
        
        spdlog::info("[Config] YAML import complete - Blacklist: {} entries | Whitelist: {} entries",
            blacklistTopics + blacklistScenes + blacklistQuests,
            whitelistTopics + whitelistScenes + whitelistQuests + whitelistPlugins);
    }
}

// Public Config functions
namespace Config
{
    // Public wrapper for ParseFormIdentifier
    std::pair<uint32_t, std::string> ParseFormIdentifier(const std::string& value)
    {
        return ParseFormIdentifierInternal(value);
    }
    
    // Check if dialogue should be soft-blocked (audio + subtitles together)
    // Checks: Database blacklist + YAML + MCM subtype filters
    bool ShouldSoftBlock(RE::TESQuest* quest, RE::TESTopic* topic, const char* speakerName, const char* responseText, uint32_t speakerFormID)
    {
        if (!topic) return false;
        
        // Get topic identifiers
        uint32_t topicFormID = topic->GetFormID();
        const char* topicEditorID = topic->GetFormEditorID();
        std::string editorIDStr = topicEditorID ? topicEditorID : "";
        
        // Check database for blacklist entry
        auto* db = DialogueDB::GetDatabase();
        if (db) {
            if (db->ShouldSoftBlock(topicFormID, editorIDStr, speakerFormID, speakerName ? speakerName : "")) {
                return true;
            }
        }
        
        // Check MCM subtype filter
        uint16_t subtype = GetAccurateSubtype(topic);
        auto it = g_settings.mcm.subtypeGlobals.find(subtype);
        if (it != g_settings.mcm.subtypeGlobals.end() && it->second) {
            float value = it->second->value;
            if (value > 0.0f) {
                return true;  // Subtype is filtered
            }
        }
        
        // Check YAML blacklist (quest/topic based)
        if (quest) {
            uint32_t questFormID = quest->GetFormID();
            const char* questEditorID = quest->GetFormEditorID();
            
            // Check quest blacklist
            if (questFormID != 0 && g_settings.blacklist.questFormIDs.count(questFormID)) {
                return true;
            }
            if (questEditorID && g_settings.blacklist.questEditorIDs.count(questEditorID)) {
                return true;
            }
        }
        
        // Check topic blacklist
        if (topicFormID != 0 && g_settings.blacklist.topicFormIDs.count(topicFormID)) {
            return true;
        }
        if (!editorIDStr.empty() && g_settings.blacklist.topicEditorIDs.count(editorIDStr)) {
            return true;
        }
        
        return false;
    }}  // namespace Config