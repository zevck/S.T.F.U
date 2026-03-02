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
        spdlog::info("[SCENE BLOCKER] Patching scenes...");

        auto* db = DialogueDB::GetDatabase();
        if (!db) {
            spdlog::error("[SCENE BLOCKER] Database not available!");
            return;
        }

        // Bulk-load both lists once — eliminates all per-scene SQLite calls
        auto blacklistCache = db->GetBlacklist();
        auto whitelistCache = db->GetWhitelist();

        // Build O(1) lookup sets
        std::unordered_set<std::string> whitelistedSceneIDs;
        std::unordered_set<std::string> whitelistedTopicIDs;
        for (const auto& e : whitelistCache) {
            if (e.targetEditorID.empty()) continue;
            if (e.targetType == DialogueDB::BlacklistTarget::Scene)
                whitelistedSceneIDs.insert(e.targetEditorID);
            else if (e.targetType == DialogueDB::BlacklistTarget::Topic)
                whitelistedTopicIDs.insert(e.targetEditorID);
        }

        std::unordered_set<std::string> hardBlockedSceneIDs;
        std::unordered_set<std::string> hardBlockedTopicIDs;
        for (const auto& e : blacklistCache) {
            if (e.blockType != DialogueDB::BlockType::Hard || e.targetEditorID.empty()) continue;
            if (e.targetType == DialogueDB::BlacklistTarget::Scene)
                hardBlockedSceneIDs.insert(e.targetEditorID);
            else if (e.targetType == DialogueDB::BlacklistTarget::Topic)
                hardBlockedTopicIDs.insert(e.targetEditorID);
        }

        int blockedSceneCount = 0;
        int blockedPhaseCount = 0;

        // Helper: apply blocking condition to all phases of a scene
        auto patchScene = [&](RE::BGSScene* scene, RE::TESGlobal* global) {
            RemoveConditionsFromScene(scene);
            for (auto* phase : scene->phases) {
                if (!phase) continue;
                if (auto* cond = CreateGlobalDisabledCondition(global)) {
                    cond->next = phase->startConditions.head;
                    cond->data.flags.isOR = false;
                    phase->startConditions.head = cond;
                    blockedPhaseCount++;
                }
            }
            blockedSceneCount++;
        };

        auto* scenesGlobal  = Config::GetScenesGlobal();
        auto* bardSongsGlobal = Config::GetBardSongsGlobal();

        // Pass 1: Hard-blocked scenes by editorID — direct lookup, no full scan needed
        if (scenesGlobal) {
            for (const auto& editorID : hardBlockedSceneIDs) {
                if (whitelistedSceneIDs.count(editorID)) continue;
                if (auto* scene = RE::TESForm::LookupByEditorID<RE::BGSScene>(editorID)) {
                    patchScene(scene, scenesGlobal);
                    spdlog::debug("[SCENE BLOCKER] Patched Hard-blocked scene: {}", editorID);
                } else {
                    spdlog::warn("[SCENE BLOCKER] Hard-blocked scene not found in form table: {}", editorID);
                }
            }
        }

        // Pass 2: Bard songs (3 known quests) — direct quest lookup, iterate only their scenes
        if (bardSongsGlobal) {
            for (const auto& questEditorID : Config::GetBardSongQuestsList()) {
                auto* quest = RE::TESForm::LookupByEditorID<RE::TESQuest>(questEditorID);
                if (!quest) {
                    spdlog::warn("[SCENE BLOCKER] Bard song quest not found: {}", questEditorID);
                    continue;
                }
                // Walk all registered scenes and patch any whose parentQuest matches
                // (BGSScene::parentQuest is the link; no direct quest->scenes collection in CommonLibSSE)
                auto* dataHandler = RE::TESDataHandler::GetSingleton();
                if (!dataHandler) continue;
                for (auto* scene : dataHandler->GetFormArray<RE::BGSScene>()) {
                    if (!scene || scene->parentQuest != quest) continue;
                    const char* sceneEditorID = scene->GetFormEditorID();
                    std::string sceneIDStr = sceneEditorID ? sceneEditorID : "";
                    if (!sceneIDStr.empty() && whitelistedSceneIDs.count(sceneIDStr)) continue;
                    patchScene(scene, bardSongsGlobal);
                    spdlog::debug("[SCENE BLOCKER] Patched bard song scene: {}", sceneIDStr);
                }
            }
        }

        // Pass 3: Topic-blocked scenes — full scan only if there are Hard topic blocks
        // (checking all scene actions against the hardBlockedTopicIDs hash set)
        if (!hardBlockedTopicIDs.empty() && scenesGlobal) {
            auto* dataHandler = RE::TESDataHandler::GetSingleton();
            if (dataHandler) {
                for (auto* scene : dataHandler->GetFormArray<RE::BGSScene>()) {
                    if (!scene) continue;
                    const char* sceneEditorID = scene->GetFormEditorID();
                    std::string sceneIDStr = sceneEditorID ? sceneEditorID : "";

                    // Skip already patched (Pass 1) or whitelisted scenes
                    if (!sceneIDStr.empty() && (hardBlockedSceneIDs.count(sceneIDStr) || whitelistedSceneIDs.count(sceneIDStr)))
                        continue;

                    bool shouldPatch = false;
                    for (auto* action : scene->actions) {
                        if (!action || action->GetType() != RE::BGSSceneAction::Type::kDialogue) continue;
                        auto* da = static_cast<RE::BGSSceneActionDialogue*>(action);
                        if (!da || !da->topic) continue;
                        const char* topicEditorID = da->topic->GetFormEditorID();
                        if (!topicEditorID) continue;
                        std::string topicIDStr = topicEditorID;

                        if (whitelistedTopicIDs.count(topicIDStr)) {
                            shouldPatch = false;
                            break;  // Whitelist overrides
                        }
                        if (hardBlockedTopicIDs.count(topicIDStr)) {
                            shouldPatch = true;
                            // Don't break — keep checking remaining actions for whitelist overrides
                        }
                    }

                    if (shouldPatch) {
                        patchScene(scene, scenesGlobal);
                        spdlog::debug("[SCENE BLOCKER] Patched topic-blocked scene: {}", sceneIDStr);
                    }
                }
            }
        }

        spdlog::info("[SCENE BLOCKER] Patching complete — {} scenes ({} phases) patched",
            blockedSceneCount, blockedPhaseCount);
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
            // Direct lookup — no full form array scan needed
            auto* targetScene = RE::TESForm::LookupByEditorID<RE::BGSScene>(sceneEditorID);
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
