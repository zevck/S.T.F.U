#include "SceneHook.h"
#include "Config.h"
#include "DialogueDatabase.h"
#include "../include/PCH.h"
#include <spdlog/spdlog.h>

namespace SceneHook
{
    // Create a condition that checks if a global == 0 (disabled)
    // When the MCM toggle is OFF (0), this condition will be TRUE, blocking the scene
    // When the MCM toggle is ON (1), this condition will be FALSE, allowing the scene
    static RE::TESConditionItem* CreateGlobalDisabledCondition(RE::TESGlobal* global)
    {
        if (!global) {
            spdlog::error("[SCENE BLOCKER] Cannot create condition - global is null!");
            return nullptr;
        }
        
        // Allocate a condition item
        auto* condition = static_cast<RE::TESConditionItem*>(RE::malloc(sizeof(RE::TESConditionItem)));
        if (!condition) return nullptr;
        
        // Initialize to zeros
        memset(condition, 0, sizeof(RE::TESConditionItem));
        
        // Set up: GetGlobalValue(global) == 0
        condition->data.functionData.function = static_cast<RE::FUNCTION_DATA::FunctionID>(74); // GetGlobalValue
        condition->data.functionData.params[0] = global; // Pass the global as parameter
        condition->data.comparisonValue.f = 0.0f;  // Compare against 0 (not a global, just float)
        condition->data.flags.opCode = RE::CONDITION_ITEM_DATA::OpCode::kEqualTo;  // ==
        condition->data.flags.global = false;  // comparisonValue is a float, not a global pointer
        condition->data.object = RE::CONDITIONITEMOBJECT::kSelf;  // Run on self (the scene/phase)
        
        condition->next = nullptr;
        
        return condition;
    }
    
    // Forward declaration
    static void RemoveConditionsFromScene(RE::BGSScene* scene);
    
    void Install()
    {
        spdlog::info("========================================");
        spdlog::info("Scene blocker ready");
        spdlog::info("========================================");
    }
    
    void PatchScenes()
    {
        spdlog::info("Patching scenes...");
        
        auto* dataHandler = RE::TESDataHandler::GetSingleton();
        if (!dataHandler) {
            spdlog::error("[SCENE BLOCKER] Failed to get TESDataHandler!");
            return;
        }
        
        // Cache the blacklist to check block types
        auto db = DialogueDB::GetDatabase();
        std::vector<DialogueDB::BlacklistEntry> blacklistCache;
        if (db) {
            blacklistCache = db->GetBlacklist();
        }
        
        int blockedSceneCount = 0;
        int blockedPhaseCount = 0;
        int totalScenesChecked = 0;
        int bardSongScenes = 0;
        int ambientScenes = 0;
        
        // Iterate directly over all scenes instead of through quests
        for (auto* scene : dataHandler->GetFormArray<RE::BGSScene>()) {
                if (!scene) continue;
                totalScenesChecked++;
                
                // Determine which global should control this scene
                RE::TESGlobal* controllingGlobal = nullptr;
                bool isBardSong = Config::IsBardSongQuest(scene->parentQuest);
                
                if (isBardSong) {
                    // Bard songs are controlled by STFU_BardSongs
                    controllingGlobal = Config::GetBardSongsGlobal();
                } else {
                    // TESTING: Only block scenes that are in database blacklist
                    // Hardcoded scenes are NOT automatically blocked unless also in blacklist
                    bool hasBlockedContent = false;
                    
                    // Check if scene itself is in database (whitelist OR blacklist)
                    const char* sceneEditorID = scene->GetFormEditorID();
                    if (sceneEditorID) {
                        auto db = DialogueDB::GetDatabase();
                        if (db) {
                            std::string sceneEditorIDStr = sceneEditorID;
                            
                            // Debug log for Carlotta scene
                            if (sceneEditorIDStr.find("CarlottaNazeem") != std::string::npos) {
                                spdlog::info("[SCENE BLOCKER] Checking scene: {}", sceneEditorIDStr);
                            }
                            
                            // HIGHEST PRIORITY: Check whitelist - whitelisted scenes are NEVER blocked
                            if (db->IsWhitelisted(DialogueDB::BlacklistTarget::Scene, 0, std::string(sceneEditorID))) {
                                hasBlockedContent = false;
                                
                                if (sceneEditorIDStr.find("CarlottaNazeem") != std::string::npos) {
                                    spdlog::info("[SCENE BLOCKER]   Scene is WHITELISTED - skipping");
                                }
                                // Skip further checks for this scene
                                continue;
                            }
                            
                            // Check blacklist - only add phase conditions for HARD blocks
                            // Soft blocks (silence only) and SkyrimNet blocks (logging only) don't need conditions
                            auto blacklistEntry = std::find_if(blacklistCache.begin(), blacklistCache.end(),
                                [&sceneEditorIDStr](const DialogueDB::BlacklistEntry& entry) {
                                    return entry.targetType == DialogueDB::BlacklistTarget::Scene &&
                                           entry.targetEditorID == sceneEditorIDStr;
                                });
                            
                            if (blacklistEntry != blacklistCache.end()) {
                                // Only set hasBlockedContent for HARD blocks (scene prevention via phase conditions)
                                // Soft blocks (silence) and SkyrimNet blocks (logging) don't need phase conditions
                                if (blacklistEntry->blockType == DialogueDB::BlockType::Hard) {
                                    hasBlockedContent = true;
                                    
                                    if (sceneEditorIDStr.find("CarlottaNazeem") != std::string::npos) {
                                        spdlog::info("[SCENE BLOCKER]   Scene is HARD BLOCKED (ID: {}) - will add phase conditions", blacklistEntry->id);
                                    }
                                } else {
                                    if (sceneEditorIDStr.find("CarlottaNazeem") != std::string::npos) {
                                        const char* blockTypeName = blacklistEntry->blockType == DialogueDB::BlockType::Soft ? "SOFT" : "SKYRIMNET";
                                        spdlog::info("[SCENE BLOCKER]   Scene is {} BLOCKED (ID: {}) - NOT adding phase conditions", blockTypeName, blacklistEntry->id);
                                    }
                                }
                            } else if (sceneEditorIDStr.find("CarlottaNazeem") != std::string::npos) {
                                spdlog::info("[SCENE BLOCKER]   Scene NOT found in blacklist");
                            }
                        }
                    }
                    
                    // Check topics within the scene for database whitelist/blacklist
                    if (!hasBlockedContent) {
                        for (auto* action : scene->actions) {
                            if (!action || action->GetType() != RE::BGSSceneAction::Type::kDialogue) continue;
                            auto* dialogueAction = static_cast<RE::BGSSceneActionDialogue*>(action);
                            if (!dialogueAction || !dialogueAction->topic) continue;
                            
                            uint32_t topicFormID = dialogueAction->topic->GetFormID();
                            const char* topicEditorID = dialogueAction->topic->GetFormEditorID();
                            std::string editorIDStr = topicEditorID ? topicEditorID : "";
                            
                            // Get quest info for ESL-safe matching
                            std::string questEditorIDStr = "";
                            if (dialogueAction->topic->ownerQuest) {
                                const char* questEdID = dialogueAction->topic->ownerQuest->GetFormEditorID();
                                questEditorIDStr = questEdID ? questEdID : "";
                            }
                            const char* topicSourcePlugin = dialogueAction->topic->GetFile(0) ? dialogueAction->topic->GetFile(0)->fileName : nullptr;
                            std::string topicSourcePluginStr = topicSourcePlugin ? topicSourcePlugin : "";
                            uint32_t localFormID = topicFormID & 0xFFF;  // Extract last 3 hex digits (stable part)
                            
                            auto db = DialogueDB::GetDatabase();
                            if (db) {
                                // HIGHEST PRIORITY: If ANY topic in scene is whitelisted, don't block entire scene
                                if (db->IsWhitelisted(DialogueDB::BlacklistTarget::Topic, topicFormID, editorIDStr)) {
                                    hasBlockedContent = false;
                                    break;  // Whitelist overrides everything
                                }
                                
                                // Check blacklist - only add phase conditions for HARD blocks
                                // Soft blocks (silence only) and SkyrimNet blocks (logging only) don't need conditions
                                auto topicEntry = std::find_if(blacklistCache.begin(), blacklistCache.end(),
                                    [&editorIDStr, topicFormID, &questEditorIDStr, &topicSourcePluginStr, localFormID](const DialogueDB::BlacklistEntry& entry) {
                                        if (entry.targetType != DialogueDB::BlacklistTarget::Topic) {
                                            return false;
                                        }
                                        
                                        // ESL-safe match: quest + plugin + local FormID
                                        if (!entry.questEditorID.empty() && !entry.sourcePlugin.empty() &&
                                            entry.questEditorID == questEditorIDStr && entry.sourcePlugin == topicSourcePluginStr &&
                                            (entry.targetFormID & 0xFFF) == localFormID) {
                                            return true;
                                        }
                                        
                                        // EditorID match
                                        if (entry.targetEditorID == editorIDStr && !editorIDStr.empty()) {
                                            return true;
                                        }
                                        
                                        // Full FormID match
                                        if (entry.targetFormID == topicFormID && topicFormID != 0) {
                                            return true;
                                        }
                                        
                                        return false;
                                    });
                                
                                if (topicEntry != blacklistCache.end() && 
                                    topicEntry->blockType == DialogueDB::BlockType::Hard) {
                                    hasBlockedContent = true;
                                    break;
                                }
                            }
                        }
                    }
                    
                    if (hasBlockedContent) {
                        // User-blacklisted content is controlled by STFU_Scenes
                        controllingGlobal = Config::GetScenesGlobal();
                    }
                }
                
                // If we found a controlling global, add the condition
                if (controllingGlobal) {
                    // Remove any existing blocking conditions first to avoid duplicates
                    RemoveConditionsFromScene(scene);
                    
                    blockedSceneCount++;
                    if (isBardSong) {
                        bardSongScenes++;
                    } else {
                        ambientScenes++;
                    }
                    
                    // Log the scene we're about to patch (only for specific scenes for debugging)
                    const char* sceneEditorID = scene->GetFormEditorID();
                    std::string sceneEditorIDStr = sceneEditorID ? sceneEditorID : "";
                    if (sceneEditorIDStr.find("CarlottaNazeem") != std::string::npos) {
                        spdlog::info("[SCENE BLOCKER] Patching scene: {} with {} phases", sceneEditorIDStr, scene->phases.size());
                    }
                    
                    // Add blocking condition to ALL phases (prepend to any existing conditions)
                    for (auto* phase : scene->phases) {
                        if (!phase) continue;
                        
                        // Create the condition
                        auto* newCondition = CreateGlobalDisabledCondition(controllingGlobal);
                        if (newCondition) {
                            // Prepend to existing conditions (AND logic)
                            newCondition->next = phase->startConditions.head;
                            newCondition->data.flags.isOR = false;  // AND with existing conditions
                            phase->startConditions.head = newCondition;
                            blockedPhaseCount++;
                            
                            // Log for debugging carlotta scene
                            if (sceneEditorIDStr.find("CarlottaNazeem") != std::string::npos) {
                                spdlog::info("[SCENE BLOCKER]   Added condition to phase - global value is currently: {}", controllingGlobal->value);
                            }
                        }
                    }
                }
            }
            
            spdlog::debug("[SCENE BLOCKER] Scanned {} scenes", totalScenesChecked);
            
            if (blockedSceneCount > 0) {
                spdlog::info("Patched {} scenes ({} phases): {} bard songs, {} ambient",
                    blockedSceneCount, blockedPhaseCount, bardSongScenes, ambientScenes);
            } else {
                spdlog::warn("[SCENE BLOCKER] No scenes were blocked!");
            }
            
            spdlog::info("[SCENE BLOCKER] Scene patching completed");
    }
    
    // Helper to remove all conditions from a scene's phases
    static void RemoveConditionsFromScene(RE::BGSScene* scene)
    {
        if (!scene) return;
        
        auto* scenesGlobal = Config::GetScenesGlobal();
        auto* bardSongsGlobal = Config::GetBardSongsGlobal();
        
        for (auto* phase : scene->phases) {
            if (!phase || !phase->startConditions.head) continue;
            
            // Remove all conditions that reference our blocking globals
            RE::TESConditionItem* prev = nullptr;
            RE::TESConditionItem* current = phase->startConditions.head;
            
            while (current) {
                RE::TESConditionItem* next = current->next;
                
                // Check if this condition references one of our blocking globals
                bool isBlockingCondition = false;
                if (current->data.functionData.function == static_cast<RE::FUNCTION_DATA::FunctionID>(74)) {
                    auto* condGlobal = static_cast<RE::TESGlobal*>(current->data.functionData.params[0]);
                    if (condGlobal == scenesGlobal || condGlobal == bardSongsGlobal) {
                        isBlockingCondition = true;
                    }
                }
                
                if (isBlockingCondition) {
                    // Remove this condition
                    if (prev) {
                        prev->next = next;
                    } else {
                        phase->startConditions.head = next;
                    }
                    RE::free(current);
                    current = next;
                } else {
                    prev = current;
                    current = next;
                }
            }
        }
    }
    
    void UpdateSceneConditions(const std::string& sceneEditorID, uint8_t blockType)
    {
        auto* taskInterface = SKSE::GetTaskInterface();
        if (!taskInterface) {
            spdlog::error("[SCENE UPDATE] Failed to get SKSE task interface!");
            return;
        }
        
        taskInterface->AddTask([sceneEditorID, blockType]() {
            auto* dataHandler = RE::TESDataHandler::GetSingleton();
            if (!dataHandler) return;
            
            // Find the scene by EditorID
            RE::BGSScene* targetScene = nullptr;
            for (auto* scene : dataHandler->GetFormArray<RE::BGSScene>()) {
                if (!scene) continue;
                const char* editorID = scene->GetFormEditorID();
                if (editorID && sceneEditorID == editorID) {
                    targetScene = scene;
                    break;
                }
            }
            
            if (!targetScene) {
                spdlog::warn("[SCENE UPDATE] Scene not found: {}", sceneEditorID);
                return;
            }
            
            // BlockType: 1=Soft, 2=Hard, 3=SkyrimNet
            // Only Hard blocks (2) should prevent scenes from starting
            const uint8_t HARD_BLOCK = 2;
            
            if (blockType == HARD_BLOCK) {
                // SAFETY CHECK: Don't add conditions to running scenes - this would softlock NPCs
                if (targetScene->isPlaying) {
                    spdlog::warn("[SCENE UPDATE] Scene {} is currently running! Skipping condition addition to prevent NPC softlock", 
                        sceneEditorID);
                    return;
                }
                
                // Add blocking condition for Hard blocks
                auto* controllingGlobal = Config::GetScenesGlobal();
                if (!controllingGlobal) {
                    spdlog::error("[SCENE UPDATE] STFU_Scenes global not found!");
                    return;
                }
                
                int phasesPatched = 0;
                for (auto* phase : targetScene->phases) {
                    if (!phase) continue;
                    
                    auto* newCondition = CreateGlobalDisabledCondition(controllingGlobal);
                    if (newCondition) {
                        newCondition->next = phase->startConditions.head;
                        newCondition->data.flags.isOR = false;
                        phase->startConditions.head = newCondition;
                        phasesPatched++;
                    }
                }
                
                spdlog::info("[SCENE UPDATE] Added blocking conditions to scene {} ({} phases) - Hard block", 
                    sceneEditorID, phasesPatched);
            } else {
                // Remove blocking conditions for Soft/SkyrimNet blocks
                // Safe to do even if scene is running - just removes preventive conditions
                RemoveConditionsFromScene(targetScene);
                spdlog::info("[SCENE UPDATE] Removed blocking conditions from scene {} - Soft/SkyrimNet block", 
                    sceneEditorID);
            }
        });
    }
    
    void UpdateSceneConditionsForTopic(const std::string& topicEditorID, uint8_t blockType)
    {
        auto* taskInterface = SKSE::GetTaskInterface();
        if (!taskInterface) {
            spdlog::error("[SCENE UPDATE] Failed to get SKSE task interface!");
            return;
        }
        
        taskInterface->AddTask([topicEditorID, blockType]() {
            auto* dataHandler = RE::TESDataHandler::GetSingleton();
            if (!dataHandler) return;
            
            // Find all scenes that contain this topic
            std::vector<RE::BGSScene*> affectedScenes;
            
            for (auto* scene : dataHandler->GetFormArray<RE::BGSScene>()) {
                if (!scene) continue;
                
                // Check if this scene contains the topic
                for (auto* action : scene->actions) {
                    if (!action || action->GetType() != RE::BGSSceneAction::Type::kDialogue) continue;
                    auto* dialogueAction = static_cast<RE::BGSSceneActionDialogue*>(action);
                    if (!dialogueAction || !dialogueAction->topic) continue;
                    
                    const char* topicEditorIDPtr = dialogueAction->topic->GetFormEditorID();
                    if (topicEditorIDPtr && topicEditorID == topicEditorIDPtr) {
                        affectedScenes.push_back(scene);
                        break;
                    }
                }
            }
            
            if (affectedScenes.empty()) {
                spdlog::debug("[SCENE UPDATE] No scenes found containing topic: {}", topicEditorID);
                return;
            }
            
            // BlockType: 1=Soft, 2=Hard, 3=SkyrimNet
            const uint8_t HARD_BLOCK = 2;
            
            auto* controllingGlobal = Config::GetScenesGlobal();
            if (!controllingGlobal) {
                spdlog::error("[SCENE UPDATE] STFU_Scenes global not found!");
                return;
            }
            
            int totalPhasesPatched = 0;
            int skippedRunningScenes = 0;
            for (auto* scene : affectedScenes) {
                const char* sceneEditorID = scene->GetFormEditorID();
                
                if (blockType == HARD_BLOCK) {
                    // SAFETY CHECK: Don't add conditions to running scenes - this would softlock NPCs
                    if (scene->isPlaying) {
                        spdlog::warn("[SCENE UPDATE] Scene {} is currently running! Skipping condition addition to prevent NPC softlock", 
                            sceneEditorID ? sceneEditorID : "(unknown)");
                        skippedRunningScenes++;
                        continue;
                    }
                    
                    // Add blocking condition for Hard blocks
                    int phasesPatched = 0;
                    for (auto* phase : scene->phases) {
                        if (!phase) continue;
                        
                        auto* newCondition = CreateGlobalDisabledCondition(controllingGlobal);
                        if (newCondition) {
                            newCondition->next = phase->startConditions.head;
                            newCondition->data.flags.isOR = false;
                            phase->startConditions.head = newCondition;
                            phasesPatched++;
                            totalPhasesPatched++;
                        }
                    }
                } else {
                    // Remove blocking conditions for Soft/SkyrimNet blocks
                    // Safe to do even if scene is running - just removes preventive conditions
                    RemoveConditionsFromScene(scene);
                }
            }
            
            if (blockType == HARD_BLOCK) {
                spdlog::info("[SCENE UPDATE] Topic {} - hard blocked {} scenes ({} phases, {} running scenes skipped)", 
                    topicEditorID, affectedScenes.size() - skippedRunningScenes, totalPhasesPatched, skippedRunningScenes);
            } else {
                spdlog::info("[SCENE UPDATE] Topic {} - soft/skyrimnet blocked {} scenes (scene conditions removed)", 
                    topicEditorID, affectedScenes.size());
            }
        });
    }
}
