#include "SceneMonitor.h"
#include "Config.h"
#include "DialogueDatabase.h"
#include <spdlog/spdlog.h>

namespace SceneMonitor
{
    static std::vector<RE::TESQuest*> g_bardQuests;
    static bool g_initialized = false;
    
    void Initialize()
    {
        try {
            spdlog::info("[SceneMonitor] Starting initialization...");
            g_bardQuests.clear();
            
            // Register the three bard song quests by EditorID
            auto dataHandler = RE::TESDataHandler::GetSingleton();
            if (!dataHandler) {
                spdlog::error("[SceneMonitor] TESDataHandler not available!");
                return;
            }
            
            spdlog::debug("[SceneMonitor] DataHandler valid, looking up bard quests...");
            if (dataHandler) {
                // BardSongs
                try {
                    if (auto quest = Config::SafeLookupForm<RE::TESQuest>("BardSongs")) {
                        g_bardQuests.push_back(quest);
                        spdlog::info("SceneMonitor: Registered BardSongs quest");
                    } else {
                        spdlog::warn("SceneMonitor: Failed to find BardSongs quest");
                    }
                }
                catch (const std::exception& e) {
                    spdlog::error("[SceneMonitor] Exception looking up BardSongs: {}", e.what());
                }
                catch (...) {
                    spdlog::error("[SceneMonitor] Unknown exception looking up BardSongs");
                }
                
                // BardSongsInstrumental
                try {
                    if (auto quest = Config::SafeLookupForm<RE::TESQuest>("BardSongsInstrumental")) {
                        g_bardQuests.push_back(quest);
                        spdlog::info("SceneMonitor: Registered BardSongsInstrumental quest");
                    } else {
                        spdlog::warn("SceneMonitor: Failed to find BardSongsInstrumental quest");
                    }
                }
                catch (const std::exception& e) {
                    spdlog::error("[SceneMonitor] Exception looking up BardSongsInstrumental: {}", e.what());
                }
                catch (...) {
                    spdlog::error("[SceneMonitor] Unknown exception looking up BardSongsInstrumental");
                }
                
                // BardAudienceQuest
                try {
                    if (auto quest = Config::SafeLookupForm<RE::TESQuest>("BardAudienceQuest")) {
                        g_bardQuests.push_back(quest);
                        spdlog::info("SceneMonitor: Registered BardAudienceQuest");
                    } else {
                        spdlog::warn("SceneMonitor: Failed to find BardAudienceQuest");
                    }
                }
                catch (const std::exception& e) {
                    spdlog::error("[SceneMonitor] Exception looking up BardAudienceQuest: {}", e.what());
                }
                catch (...) {
                    spdlog::error("[SceneMonitor] Unknown exception looking up BardAudienceQuest");
                }
                
                spdlog::info("SceneMonitor: Initialized with {} bard quests", g_bardQuests.size());
                
                // Preemptively add all bard song scenes to the blacklist
                auto db = DialogueDB::GetDatabase();
                if (db) {
                    int scenesAdded = 0;
                    int scenesSkipped = 0;
                    int totalScenesFound = 0;
                    for (auto* quest : g_bardQuests) {
                        if (!quest) continue;
                        
                        try {
                            // UNSAFE: GetFormEditorID() can crash during early init - log FormID instead
                            spdlog::debug("[SceneMonitor] Processing quest FormID: 0x{:08X}", quest->GetFormID());
                            
                            auto& scenesArray = quest->scenes;
                            if (scenesArray.empty()) {
                                spdlog::debug("[SceneMonitor] Quest 0x{:08X} has no scenes, skipping", quest->GetFormID());
                                continue;
                            }
                            
                            spdlog::debug("[SceneMonitor] Quest 0x{:08X} has {} scenes", quest->GetFormID(), scenesArray.size());
                            
                            for (auto* scene : scenesArray) {
                                if (!scene) continue;
                                
                                try {
                                    // Get EditorID - pointer may be unstable during init
                                    const char* sceneEditorID_unsafe = nullptr;
                                    try {
                                        sceneEditorID_unsafe = scene->GetFormEditorID();
                                    } catch (...) {
                                        // Skip scenes with inaccessible EditorIDs during init
                                        continue;
                                    }
                                    if (!sceneEditorID_unsafe || strlen(sceneEditorID_unsafe) == 0) {
                                        continue;
                                    }
                                    
                                    // CRITICAL: Immediately copy to safe string to avoid crashes from unstable pointer
                                    std::string sceneEditorID(sceneEditorID_unsafe);
                                    
                                    totalScenesFound++;
                                    
                                    // Check if already in blacklist
                                    bool alreadyExists = false;
                                    try {
                                        auto existingBlacklist = db->GetBlacklist();
                                        for (const auto& entry : existingBlacklist) {
                                            try {
                                                if (entry.targetType == DialogueDB::BlacklistTarget::Scene &&
                                                    !entry.targetEditorID.empty() &&
                                                    entry.targetEditorID == sceneEditorID) {
                                                    alreadyExists = true;
                                                    break;
                                                }
                                            }
                                            catch (...) {
                                                // Corrupted blacklist entry - skip it
                                                continue;
                                            }
                                        }
                                    }
                                    catch (const std::exception& e) {
                                        spdlog::error("[SceneMonitor] Exception checking blacklist: {}", e.what());
                                        // Assume not exists, continue with add
                                    }
                                    
                                    if (alreadyExists) {
                                        scenesSkipped++;
                                        continue;
                                    }
                                    
                                    // Add scene to blacklist with BardSongs category
                                    DialogueDB::BlacklistEntry entry;
                                    entry.targetType = DialogueDB::BlacklistTarget::Scene;
                                    entry.targetFormID = 0;
                                    entry.targetEditorID = sceneEditorID;
                                    entry.blockType = DialogueDB::BlockType::Hard;
                                    entry.addedTimestamp = std::chrono::duration_cast<std::chrono::seconds>(
                                        std::chrono::system_clock::now().time_since_epoch()
                                    ).count();
                                    entry.notes = "Auto-added by SceneMonitor";
                                    entry.responseText = "";
                                    entry.subtype = 14;  // Scene subtype
                                    entry.subtypeName = "Scene";
                                    entry.filterCategory = "BardSongs";
                                    entry.sourcePlugin = "Skyrim.esm";
                                    entry.blockAudio = true;
                                    entry.blockSubtitles = true;
                                    entry.blockSkyrimNet = true;
                                    
                                    // Skip response extraction during initialization (incompatible with po3 CommonLibSSE)
                                    if (db->AddToBlacklist(entry, true)) {
                                        scenesAdded++;
                                        spdlog::debug("[SceneMonitor] Auto-added bard scene 0x{:08X} to blacklist", scene->GetFormID());
                                    } else {
                                        spdlog::warn("SceneMonitor: Failed to add bard scene 0x{:08X} to blacklist", scene->GetFormID());
                                    }
                                }
                                catch (const std::exception& e) {
                                    spdlog::error("[SceneMonitor] Exception processing scene 0x{:08X}: {}", scene->GetFormID(), e.what());
                                }
                                catch (...) {
                                    spdlog::error("[SceneMonitor] Unknown exception processing scene 0x{:08X}", scene->GetFormID());
                                }
                            }
                        }
                        catch (const std::exception& e) {
                            spdlog::error("[SceneMonitor] Exception processing quest 0x{:08X}: {}", quest->GetFormID(), e.what());
                        }
                        catch (...) {
                            spdlog::error("[SceneMonitor] Unknown exception processing quest 0x{:08X}", quest->GetFormID());
                        }
                    }
                    
                    spdlog::info("SceneMonitor: Found {} total scenes, added {}, skipped {} (already existed)", totalScenesFound, scenesAdded, scenesSkipped);
                }
            }
            
            g_initialized = true;
            spdlog::info("[SceneMonitor] Initialization complete");
        }
        catch (const std::exception& e) {
            spdlog::error("[SceneMonitor] FATAL: Exception during initialization: {}", e.what());
            spdlog::error("[SceneMonitor] Scene monitoring will not function!");
        }
        catch (...) {
            spdlog::error("[SceneMonitor] FATAL: Unknown exception during initialization!");
            spdlog::error("[SceneMonitor] Scene monitoring will not function!");
        }
    }
    
    // Note: Update() function removed - it was dead code never called anywhere.
    // Scenes are now blocked preventively via PatchScenes() using conditions,
    // eliminating the need for runtime monitoring to stop already-playing scenes.
    
    void RegisterBardQuest(RE::TESQuest* quest)
    {
        if (quest && std::find(g_bardQuests.begin(), g_bardQuests.end(), quest) == g_bardQuests.end()) {
            g_bardQuests.push_back(quest);
        }
    }
}
