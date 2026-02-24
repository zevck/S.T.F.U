#include "PopulateTopicInfoHook.h"
#include "Config.h"
#include "DialogueDatabase.h"
#include "ConstructResponseHook.h"
#include "TopicResponseExtractor.h"
#include "SceneHook.h"
#include "STFUMenu.h"
#include "../include/PCH.h"
#include "../external/Detours/src/detours.h"
#include <RE/B/BGSSceneActionDialogue.h>
#include <chrono>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <algorithm>
#include <vector>

// Forward declaration for force-logging check
namespace DialogueItemCtorHook {
    bool WasBlockedWithNullptr(uint32_t speakerFormID, const std::string& responseText);
}

namespace PopulateTopicInfoHook
{
    // Correlation with DialogueItem::Ctor: track (speaker + topicInfo) constructions
    // This distinguishes actual dialogue playback from menu evaluation
    struct DialogueConstruct {
        uint32_t speakerFormID;
        uint32_t topicInfoFormID;
        int64_t timestamp;
    };
    
    static std::vector<DialogueConstruct> g_recentConstructs;
    static std::mutex g_constructMutex;
    
    // Cooldown system: track recently logged dialogue to prevent duplicates
    struct DialogueKey {
        uint32_t speakerFormID;
        uint32_t topicInfoFormID;
        
        bool operator==(const DialogueKey& other) const {
            return speakerFormID == other.speakerFormID && topicInfoFormID == other.topicInfoFormID;
        }
    };
    
    struct DialogueKeyHash {
        std::size_t operator()(const DialogueKey& k) const {
            return std::hash<uint32_t>()(k.speakerFormID) ^ (std::hash<uint32_t>()(k.topicInfoFormID) << 1);
        }
    };
    
    // Text-based deduplication: track (speaker + response text) to catch duplicate responses
    struct TextDialogueKey {
        uint32_t speakerFormID;
        std::string responseText;
        
        bool operator==(const TextDialogueKey& other) const {
            return speakerFormID == other.speakerFormID && responseText == other.responseText;
        }
    };
    
    struct TextDialogueKeyHash {
        std::size_t operator()(const TextDialogueKey& k) const {
            return std::hash<uint32_t>()(k.speakerFormID) ^ std::hash<std::string>()(k.responseText);
        }
    };
    
    static std::unordered_map<DialogueKey, int64_t, DialogueKeyHash> g_recentlyLogged;
    static std::unordered_map<TextDialogueKey, int64_t, TextDialogueKeyHash> g_recentlyLoggedByText;
    static std::mutex g_loggedMutex;
    static int64_t g_lastCleanupTime = 0;
    static constexpr int64_t CLEANUP_INTERVAL_MS = 30000; // Clean up every 30 seconds    
    // Check if this speaker+text was recently logged (for external deduplication)
    bool WasRecentlyLogged(uint32_t speakerFormID, const std::string& responseText, int64_t withinMs = 5000)
    {
        std::lock_guard<std::mutex> lock(g_loggedMutex);
        
        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        TextDialogueKey key{speakerFormID, responseText};
        auto it = g_recentlyLoggedByText.find(key);
        if (it != g_recentlyLoggedByText.end()) {
            int64_t timeSince = now - it->second;
            if (timeSince < withinMs) {
                return true;
            }
        }
        return false;
    }
    // Function signature for PopulateTopicInfo
    // This is called for ALL dialogue types: combat barks, idle chatter, greetings, menu dialogue, etc.
    typedef int64_t(WINAPI* PopulateTopicInfoType)(
        int64_t a_1, 
        RE::TESTopic* a_topic, 
        RE::TESTopicInfo* a_topicInfo, 
        RE::Character* a_speaker, 
        RE::TESTopicInfo::ResponseData* a_responseData
    );

    static PopulateTopicInfoType _OriginalPopulateTopicInfo = nullptr;

    // Our hook function
    int64_t WINAPI Hook_PopulateTopicInfo(
        int64_t a_1,
        RE::TESTopic* a_topic,
        RE::TESTopicInfo* a_topicInfo,
        RE::Character* a_speaker,
        RE::TESTopicInfo::ResponseData* a_responseData)
    {
        const char* speakerName = a_speaker ? a_speaker->GetName() : nullptr;
        
        // Extract response text from topicInfo DIRECTLY - it's a static record, always available
        // This gets the response chain (multiple fragments concatenated with spaces)
        std::string fullResponseText;
        if (a_topicInfo) {
            auto responses = TopicResponseExtractor::ExtractResponsesFromTopicInfo(a_topicInfo);
            // Concatenate all responses in the chain (usually just one, but some dialogue has multiple fragments)
            for (size_t i = 0; i < responses.size(); ++i) {
                if (i > 0) fullResponseText += " ";
                fullResponseText += responses[i];
            }
        }
        const char* responseText = !fullResponseText.empty() ? fullResponseText.c_str() : nullptr;
        
        spdlog::info("[POPULATE EXTRACTED] Speaker: {}, Text from topicInfo: '{}'",
            speakerName ? speakerName : "Unknown",
            responseText ? responseText : "(NULL/EMPTY)");
        
        // Clear state only if not a duplicate (different TopicInfo/Speaker OR >5 seconds)
        RE::FormID currentFormID = a_topicInfo ? a_topicInfo->GetFormID() : 0;
        bool isDuplicate = ConstructResponseHook::IsDuplicateDialogue(currentFormID, a_speaker);
        
        if (isDuplicate) {
            spdlog::debug("[POPULATE] Duplicate detected (FormID: {:#x}, Speaker: {}), skipping all processing", 
                currentFormID, speakerName ? speakerName : "Unknown");
            // Call original without any modifications - skip all blocking logic
            return _OriginalPopulateTopicInfo(a_1, a_topic, a_topicInfo, a_speaker, a_responseData);
        }
        
        // New dialogue - clear state and process normally
        spdlog::debug("[POPULATE] New dialogue detected (FormID: {:#x}, Speaker: {}), clearing state", 
            currentFormID, speakerName ? speakerName : "Unknown");
        ConstructResponseHook::SetShouldBlockSubtitles(false);
        
        // Check for HARD BLOCKING first (complete prevention, not just silencing)
        if (a_topic && a_speaker) {
            RE::TESQuest* quest = a_topic->ownerQuest;
            uint32_t topicFormID = a_topic->GetFormID();
            const char* topicEditorID = a_topic->GetFormEditorID();
            std::string topicEditorIDStr = topicEditorID ? topicEditorID : "";
            uint16_t subtype = Config::GetAccurateSubtype(a_topic);
            
            // Get quest info for ESL-safe matching
            std::string questEditorIDStr = "";
            if (quest) {
                const char* questEdID = quest->GetFormEditorID();
                questEditorIDStr = questEdID ? questEdID : "";
            }
            const char* topicSourcePlugin = a_topic->GetFile(0) ? a_topic->GetFile(0)->fileName : nullptr;
            std::string topicSourcePluginStr = topicSourcePlugin ? topicSourcePlugin : "";
            uint32_t localFormID = topicFormID & 0xFFF;  // Extract last 3 hex digits (stable part)
            
            // Check if this topic/quest is hard-blocked in the database
            bool isHardBlocked = false;
            auto db = DialogueDB::GetDatabase();
            if (db) {
                // Check blacklist for hard block type
                auto blacklistCache = db->GetBlacklist();
                
                // Check topic hard block with ESL-safe matching
                auto topicEntry = std::find_if(blacklistCache.begin(), blacklistCache.end(),
                    [topicFormID, &topicEditorIDStr, &questEditorIDStr, &topicSourcePluginStr, localFormID](const DialogueDB::BlacklistEntry& e) {
                        if (e.targetType != DialogueDB::BlacklistTarget::Topic || e.blockType != DialogueDB::BlockType::Hard) {
                            return false;
                        }
                        
                        // ESL-safe match: quest + plugin + local FormID
                        if (!e.questEditorID.empty() && !e.sourcePlugin.empty() && 
                            e.questEditorID == questEditorIDStr && e.sourcePlugin == topicSourcePluginStr &&
                            (e.targetFormID & 0xFFF) == localFormID) {
                            return true;
                        }
                        
                        // EditorID match (works for all plugins with EditorIDs)
                        if (!e.targetEditorID.empty() && e.targetEditorID == topicEditorIDStr) {
                            return true;
                        }
                        
                        // Full FormID match (fallback for topics without EditorIDs)
                        if (e.targetFormID == topicFormID && topicFormID != 0) {
                            return true;
                        }
                        
                        return false;
                    });
                
                if (topicEntry != blacklistCache.end()) {
                    isHardBlocked = true;
                } else if (quest) {
                    // Check quest hard block
                    uint32_t questFormID = quest->GetFormID();
                    const char* questEditorID = quest->GetFormEditorID();
                    std::string questEditorIDStr = questEditorID ? questEditorID : "";
                    
                    auto questEntry = std::find_if(blacklistCache.begin(), blacklistCache.end(),
                        [questFormID, &questEditorIDStr](const DialogueDB::BlacklistEntry& e) {
                            return e.targetType == DialogueDB::BlacklistTarget::Quest &&
                                   (e.targetFormID == questFormID || e.targetEditorID == questEditorIDStr) &&
                                   e.blockType == DialogueDB::BlockType::Hard;
                        });
                    
                    if (questEntry != blacklistCache.end()) {
                        isHardBlocked = true;
                    }
                }
            }
            
            // HARD BLOCK: Log to history and return early to prevent dialogue completely
            if (isHardBlocked) {
                // Skip logging if no text (even for hard blocks)
                std::string trimmedText = fullResponseText;
                trimmedText.erase(0, trimmedText.find_first_not_of(" \t\n\r"));
                trimmedText.erase(trimmedText.find_last_not_of(" \t\n\r") + 1);
                
                if (trimmedText.empty()) {
                    spdlog::info("[HARD BLOCK] Skipping log - no text: {} (subtype: {})",
                        topicEditorIDStr, Config::GetSubtypeName(subtype));
                    return 0;
                }
                
                // Filter breathing/grunt sounds from history even when hard blocked
                if (Config::ShouldFilterFromHistory(responseText, subtype, a_topic)) {
                    spdlog::info("[HARD BLOCK] Filtered from history: {} (subtype: {})", 
                        topicEditorIDStr, subtype);
                    return 0;  // Block without logging
                }
                
                spdlog::info("[HARD BLOCK] Speaker: {} - Topic: {} (subtype: {}, FormID: 0x{:08X})",
                    speakerName ? speakerName : "Unknown",
                    topicEditorIDStr,
                    Config::GetSubtypeName(subtype),
                    topicFormID);
                
                // Log to history database before blocking
                DialogueDB::DialogueEntry entry;
                auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()
                ).count();
                entry.timestamp = now / 1000;
                
                entry.speakerName = speakerName ? speakerName : "Unknown";
                entry.speakerFormID = a_speaker->GetFormID();
                entry.speakerBaseFormID = a_speaker->GetActorBase() ? a_speaker->GetActorBase()->GetFormID() : 0;
                
                entry.topicEditorID = topicEditorIDStr;
                entry.topicFormID = topicFormID;
                entry.topicSubtype = subtype;
                entry.topicSubtypeName = Config::GetSubtypeName(subtype);
                
                entry.questEditorID = quest && quest->GetFormEditorID() ? quest->GetFormEditorID() : "";
                entry.questFormID = quest ? quest->GetFormID() : 0;
                entry.questName = quest && quest->GetName() ? quest->GetName() : "";
                
                // Debug logging for questEditorID
                if (entry.questEditorID.empty() && entry.questName.empty()) {
                    spdlog::warn("[PopulateTopicInfoHook] Quest info missing for topic '{}' (FormID: 0x{:08X}) - quest pointer: {}", 
                        topicEditorIDStr, topicFormID, quest ? "valid" : "null");
                } else {
                    spdlog::debug("[PopulateTopicInfoHook] Topic '{}' -> Quest: '{}' (EditorID: '{}', Name: '{}')", 
                        topicEditorIDStr, entry.questEditorID, entry.questEditorID, entry.questName);
                }
                
                entry.sceneEditorID = "";
                entry.topicInfoFormID = a_topicInfo ? a_topicInfo->GetFormID() : 0;
                
                // Use actual response text that was extracted at the beginning
                entry.responseText = fullResponseText;
                
                // Extract all responses for this topic
                if (!topicEditorIDStr.empty()) {
                    entry.allResponses = TopicResponseExtractor::ExtractAllResponsesForTopic(topicEditorIDStr);
                } else if (topicFormID > 0) {
                    entry.allResponses = TopicResponseExtractor::ExtractAllResponsesForTopic(topicFormID);
                }
                if (entry.allResponses.empty() && !fullResponseText.empty()) {
                    entry.allResponses.push_back(fullResponseText);
                }
                
                entry.voiceFilepath = "";
                entry.blockedStatus = DialogueDB::BlockedStatus::HardBlock;
                entry.isScene = (subtype == 14);
                entry.isBardSong = Config::IsBardSongQuest(quest);
                entry.isHardcodedScene = Config::IsHardcodedAmbientScene(a_topic);
                entry.skyrimNetBlockable = false;
                
                if (a_topic) {
                    auto* file = a_topic->GetFile(0);
                    entry.topicSourcePlugin = file ? file->GetFilename() : "";
                }
                if (a_topicInfo) {
                    auto* file = a_topicInfo->GetFile(0);
                    entry.sourcePlugin = file ? file->GetFilename() : "";
                }
                
                // Distance check: Don't log dialogue from NPCs that are too far away (>5000 units)
                auto player = RE::PlayerCharacter::GetSingleton();
                if (player && a_speaker) {
                    float distance = player->GetPosition().GetDistance(a_speaker->GetPosition());
                    if (distance > 5000.0f) {
                        spdlog::debug("[HARD BLOCK] Speaker '{}' too far away ({:.1f} units), skipping dialogue log but still blocking", 
                            speakerName ? speakerName : "Unknown", distance);
                        // Still return 0 to block the dialogue, just don't log it
                        return 0;
                    }
                }
                
                DialogueDB::GetDatabase()->LogDialogue(entry);
                
                // Return 0 to prevent dialogue from proceeding
                // This stops the entire dialogue event - no turning, no animations, nothing
                return 0;
            }
        }
        
        // Clear NPC response text BEFORE calling original to block SkyrimNet
        // Note: Topic fullName (player's dialogue choice) is cleared in DialogueItem::Ctor hook
        if (a_topic && a_responseData && !fullResponseText.empty()) {
            RE::TESQuest* quest = a_topic->ownerQuest;
            bool shouldBlockAudio = Config::ShouldBlockAudio(quest, a_topic, speakerName, responseText);
            bool shouldBlockSubtitles = Config::ShouldBlockSubtitles(quest, a_topic, speakerName, responseText);
            bool shouldBlockSkyrimNet = false;
            if (STFUMenu::IsSkyrimNetLoaded()) {
                shouldBlockSkyrimNet = Config::ShouldBlockSkyrimNet(quest, a_topic, speakerName);
            }
            
            // Check if this is menu dialogue (not ambient/background dialogue)
            bool isMenuDialogue = false;
            auto* ui = RE::UI::GetSingleton();
            if (ui && ui->IsMenuOpen(RE::DialogueMenu::MENU_NAME)) {
                auto* menuTopicManager = RE::MenuTopicManager::GetSingleton();
                if (menuTopicManager && a_speaker) {
                    auto speakerHandle = a_speaker->GetHandle();
                    isMenuDialogue = (speakerHandle == menuTopicManager->speaker);
                }
            }
            
            // SOFT BLOCKING: Block audio + subtitles + SkyrimNet
            if (shouldBlockAudio && shouldBlockSubtitles) {
                spdlog::info("[POPULATE] Clearing NPC response text for SOFT-BLOCKED dialogue: '{}'", 
                    responseText ? responseText : "(none)");
                // Clear response text - this prevents subtitles AND audio
                a_responseData->responseText = "";
                ConstructResponseHook::SetShouldBlockSubtitles(true);
            }
            // SKYRIMNET-ONLY BLOCKING: Do nothing here
            // DialogueItem::Ctor will return nullptr to block from SkyrimNet
            // Subtitles and audio work naturally
        }
        
        // Call original function (SkyrimNet sees empty, we'll show manual subtitle)
        int64_t result = _OriginalPopulateTopicInfo(a_1, a_topic, a_topicInfo, a_speaker, a_responseData);
        
        // Log dialogue to database
        if (a_topic && a_speaker) {
            // Skip logging if TopicInfo has no text content (but still allow blocking above)
            // Trim whitespace to check for whitespace-only strings
            std::string trimmedText = fullResponseText;
            trimmedText.erase(0, trimmedText.find_first_not_of(" \t\n\r"));
            trimmedText.erase(trimmedText.find_last_not_of(" \t\n\r") + 1);
            
            if (trimmedText.empty()) {
                spdlog::trace("[POPULATE] Skipping database log for TopicInfo with no text (0x{:08X})",
                    a_topicInfo ? a_topicInfo->GetFormID() : 0);
                return result;
            }
            
            const char* speakerName = a_speaker->GetName();
            const char* topicEditorID = a_topic->GetFormEditorID();
            RE::TESQuest* quest = a_topic->ownerQuest;
            
            uint32_t topicFormID = a_topic->GetFormID();
            uint32_t speakerFormID = a_speaker->GetFormID();
            
            auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count();
            
            // Filter grunts and breathing sounds from history log
            // Determine subtype for filtering
            uint16_t subtype = Config::GetAccurateSubtype(a_topic);
            
            if (Config::ShouldFilterFromHistory(responseText, subtype, a_topic)) {
                // Silently skip grunts and sprint breathing (don't log, don't spam)
                spdlog::trace("[POPULATE FILTERED] Subtype {} filtered from history",
                    Config::GetSubtypeName(subtype));
                // Already called original, just return
                return result;
            }
            
            // CORRELATION: Only log if DialogueItem::Ctor also fired for this (speaker + topicInfo)
            // DialogueItem::Ctor fires for actual dialogue playback, not menu evaluation
            // This filters out menu spam while preserving all legitimate dialogue
            uint32_t topicInfoFormID = a_topicInfo ? a_topicInfo->GetFormID() : 0;
            bool wasConstructed = false;
            
            spdlog::trace("[POPULATE] Checking correlation for speaker={} (0x{:08X}), topicInfo=0x{:08X}, topic={}", 
                speakerName ? speakerName : "Unknown", speakerFormID, topicInfoFormID,
                topicEditorID ? topicEditorID : "(none)");
            
            {
                std::lock_guard<std::mutex> lock(g_constructMutex);
                
                spdlog::trace("[POPULATE] Current constructs in queue: {}", g_recentConstructs.size());
                
                for (auto it = g_recentConstructs.begin(); it != g_recentConstructs.end();) {
                    int64_t timeSince = now - it->timestamp;
                    if (timeSince > 1000) {
                        // Remove old entries (>1 second)
                        it = g_recentConstructs.erase(it);
                    } else {
                        // Check if this construct matches our dialogue (both speaker AND topicInfo)
                        spdlog::trace("[POPULATE] Comparing with construct: speaker=0x{:08X}, topicInfo=0x{:08X}, age={}ms", 
                            it->speakerFormID, it->topicInfoFormID, timeSince);
                        
                        if (it->speakerFormID == speakerFormID && it->topicInfoFormID == topicInfoFormID && timeSince < 50) {
                            wasConstructed = true;
                            spdlog::info("[POPULATE] MATCH FOUND! Logging dialogue.");
                            // Remove this construct (single-use to prevent multi-response duplication)
                            it = g_recentConstructs.erase(it);
                            break;
                        } else {
                            ++it;
                        }
                    }
                }
            }
            
            // FIRST: Check if DialogueItem::Ctor manually logged this (when returning nullptr)
            // We check this BEFORE wasConstructed to catch cases where correlation might succeed
            bool wasManuallyLogged = DialogueItemCtorHook::WasBlockedWithNullptr(speakerFormID, fullResponseText);
            if (wasManuallyLogged) {
                spdlog::info("[POPULATE] MANUALLY LOGGED - DialogueItem::Ctor already logged this before returning nullptr, skipping duplicate");
                return result;
            }
            
            if (!wasConstructed) {
                // No matching DialogueItem::Ctor - likely menu evaluation, skip logging
                spdlog::info("[POPULATE] NO MATCH - Skipping logging (likely menu evaluation)");
                return result;
            }
            
            // Cooldown: Skip if same (speaker + topicInfo) was just logged within 60 seconds
            DialogueKey key{speakerFormID, topicInfoFormID};
            const int64_t COOLDOWN_DURATION = 60000; // 60 seconds
            
            spdlog::trace("[POPULATE COOLDOWN CHECK] Speaker: {} (0x{:08X}), TopicInfo: 0x{:08X}, Subtype: {}",
                speakerName ? speakerName : "Unknown", speakerFormID, topicInfoFormID, Config::GetSubtypeName(subtype));
            
            {
                std::lock_guard<std::mutex> lock(g_loggedMutex);
                auto it = g_recentlyLogged.find(key);
                if (it != g_recentlyLogged.end()) {
                    int64_t timeSinceLast = now - it->second;
                    if (timeSinceLast < COOLDOWN_DURATION) {
                        // Same dialogue logged within cooldown period - skip duplicate
                        spdlog::debug("[COOLDOWN SKIP] {} - TopicInfo: 0x{:08X}, Subtype: {} ({}ms since last)",
                            speakerName ? speakerName : "Unknown", topicInfoFormID, Config::GetSubtypeName(subtype), timeSinceLast);
                        // Already called original, just return
                        return result;
                    }
                }
                g_recentlyLogged[key] = now;
                
                spdlog::trace("[COOLDOWN PASSED] {} - TopicInfo: 0x{:08X}, Subtype: {} - Will log to database",
                    speakerName ? speakerName : "Unknown", topicInfoFormID, Config::GetSubtypeName(subtype));
                
                // Periodic cleanup: only run every CLEANUP_INTERVAL_MS to avoid O(n) iteration on every dialogue
                // This significantly improves performance during dialogue-heavy gameplay
                if (now - g_lastCleanupTime > CLEANUP_INTERVAL_MS) {
                    g_lastCleanupTime = now;
                    int removedCount = 0;
                    constexpr int64_t CLEANUP_THRESHOLD_MS = 120000; // 2 minutes (2x cooldown duration)
                    for (auto iter = g_recentlyLogged.begin(); iter != g_recentlyLogged.end();) {
                        if (now - iter->second > CLEANUP_THRESHOLD_MS) {
                            iter = g_recentlyLogged.erase(iter);
                            removedCount++;
                        } else {
                            ++iter;
                        }
                    }
                    if (removedCount > 0) {
                        spdlog::debug("[COOLDOWN] Periodic cleanup removed {} expired entries ({} remaining)",
                            removedCount, g_recentlyLogged.size());
                    }
                }
            }
            
            // Determine blocking status with granular control
            DialogueDB::BlockedStatus blockedStatus = DialogueDB::BlockedStatus::Normal;
            
            // Check granular blocking flags
            bool shouldBlockAudio = Config::ShouldBlockAudio(quest, a_topic, speakerName, responseText);
            bool shouldBlockSubtitles = Config::ShouldBlockSubtitles(quest, a_topic, speakerName, responseText);
            bool shouldBlockSkyrimNet = false;
            if (STFUMenu::IsSkyrimNetLoaded()) {
                shouldBlockSkyrimNet = Config::ShouldBlockSkyrimNet(quest, a_topic, speakerName);
            }
            
            // For scenes (subtype 14), check if scene blocking is actually enabled
            // If scene blocking is disabled, hardcoded scenes should show as Normal even if they would be blocked
            bool isScene = (subtype == 14);
            bool scenesEnabled = Config::ShouldBlockScenes();
            bool isHardcodedScene = Config::IsHardcodedAmbientScene(a_topic);
            
            if (isScene && isHardcodedScene && !scenesEnabled) {
                // Hardcoded scene with scene blocking disabled - don't mark as blocked
                shouldBlockAudio = false;
                shouldBlockSubtitles = false;
            }
            
            // Determine status for logging:
            // - If audio or subtitles blocked: SoftBlock
            // - Else if SkyrimNet blocked: SkyrimNetBlock
            // - Else: Normal
            if (shouldBlockAudio || shouldBlockSubtitles) {
                blockedStatus = DialogueDB::BlockedStatus::SoftBlock;
            } else if (shouldBlockSkyrimNet) {
                blockedStatus = DialogueDB::BlockedStatus::SkyrimNetBlock;
            }
            
            // Create database entry
            DialogueDB::DialogueEntry entry;
            entry.timestamp = now / 1000;  // Convert milliseconds to seconds for database
            
            entry.speakerName = speakerName ? speakerName : "";
            entry.speakerFormID = speakerFormID;
            entry.speakerBaseFormID = a_speaker->GetActorBase() ? a_speaker->GetActorBase()->GetFormID() : 0;
            
            entry.topicEditorID = topicEditorID ? topicEditorID : "";
            entry.topicFormID = topicFormID;
            entry.topicSubtype = subtype;
            entry.topicSubtypeName = Config::GetSubtypeName(subtype);
            
            entry.questEditorID = quest && quest->GetFormEditorID() ? quest->GetFormEditorID() : "";
            entry.questFormID = quest ? quest->GetFormID() : 0;
            entry.questName = quest && quest->GetName() ? quest->GetName() : "";
            
            // Debug logging for questEditorID
            if (entry.questEditorID.empty() && entry.questName.empty()) {
                spdlog::warn("[PopulateTopicInfoHook] Quest info missing for dialogue - topic: '{}' (FormID: 0x{:08X}), quest pointer: {}", 
                    entry.topicEditorID, topicFormID, quest ? "valid" : "null");
            } else {
                spdlog::debug("[PopulateTopicInfoHook] Logging dialogue - Topic: '{}', Quest EditorID: '{}', Quest Name: '{}'", 
                    entry.topicEditorID, entry.questEditorID, entry.questName);
            }
            
            // Get source plugin from topicInfo (specific response that played)
            if (a_topicInfo) {
                auto* file = a_topicInfo->GetFile(0);
                entry.sourcePlugin = file ? file->GetFilename() : "";
            } else {
                entry.sourcePlugin = "";
            }
            
            // Get source plugin from topic (container - used when blacklisting)
            if (a_topic) {
                auto* file = a_topic->GetFile(0);
                entry.topicSourcePlugin = file ? file->GetFilename() : "";
            } else {
                entry.topicSourcePlugin = "";
            }
            
            entry.topicInfoFormID = a_topicInfo ? a_topicInfo->GetFormID() : 0;
            
            // Response text was already extracted at the top of the function (fullResponseText)
            // reuse that captured text here
            entry.responseText = fullResponseText;
            
            // Set isScene flag BEFORE response extraction
            entry.isScene = (subtype == 14);
            entry.isBardSong = Config::IsBardSongQuest(quest);
            entry.isHardcodedScene = Config::IsHardcodedAmbientScene(a_topic);
            
            // For scene dialogue, find and capture the parent scene's EditorID BEFORE extraction
            if (entry.isScene && quest && a_topic) {
                auto& scenesArray = quest->scenes;
                
                for (auto* scene : scenesArray) {
                    if (!scene) continue;
                    
                    // Check if this scene contains our topic
                    bool sceneContainsTopic = false;
                    for (auto* action : scene->actions) {
                        if (!action) continue;
                        if (action->GetType() == RE::BGSSceneAction::Type::kDialogue) {
                            auto* dialogueAction = static_cast<RE::BGSSceneActionDialogue*>(action);
                            if (dialogueAction && dialogueAction->topic == a_topic) {
                                sceneContainsTopic = true;
                                break;
                            }
                        }
                    }
                    
                    if (sceneContainsTopic) {
                        // Found the scene - capture its EditorID
                        const char* sceneEditorID = scene->GetFormEditorID();
                        entry.sceneEditorID = sceneEditorID ? sceneEditorID : "";
                        break;
                    }
                }
            }
            
            // Extract all responses for this topic/scene
            if (entry.isScene && !entry.sceneEditorID.empty()) {
                // Scene dialogue - extract all responses from the scene
                spdlog::info("[POPULATE EXTRACT] Extracting all responses for scene: {}", entry.sceneEditorID);
                entry.allResponses = TopicResponseExtractor::ExtractAllResponsesForScene(entry.sceneEditorID);
                spdlog::info("[POPULATE EXTRACT] Scene extraction returned {} responses", entry.allResponses.size());
            } else if (!entry.topicEditorID.empty()) {
                // Regular topic dialogue with EditorID - extract by EditorID
                spdlog::info("[POPULATE EXTRACT] Extracting all responses for topic EditorID: {}", entry.topicEditorID);
                entry.allResponses = TopicResponseExtractor::ExtractAllResponsesForTopic(entry.topicEditorID);
                spdlog::info("[POPULATE EXTRACT] Topic EditorID extraction returned {} responses", entry.allResponses.size());
            } else if (entry.topicFormID > 0) {
                // Regular topic with FormID only (no EditorID) - extract by FormID
                spdlog::info("[POPULATE EXTRACT] Extracting all responses for topic FormID: 0x{:08X}", entry.topicFormID);
                entry.allResponses = TopicResponseExtractor::ExtractAllResponsesForTopic(entry.topicFormID);
                spdlog::info("[POPULATE EXTRACT] Topic FormID extraction returned {} responses", entry.allResponses.size());
            }
            
            // If extraction failed or returned empty, fall back to just the current response
            if (entry.allResponses.empty() && !fullResponseText.empty()) {
                spdlog::info("[POPULATE EXTRACT] Extraction returned empty, falling back to current response");
                entry.allResponses.push_back(fullResponseText);
            }
            
            spdlog::debug("[DIALOGUE] {}: \"{}\" (subtype: {}, status: {}, responses: {})", 
                speakerName ? speakerName : "Unknown",
                entry.responseText.empty() ? "(no text)" : entry.responseText.substr(0, 100).c_str(),
                entry.topicSubtype,
                static_cast<int>(blockedStatus),
                entry.allResponses.size());
            
            entry.voiceFilepath = "";
            entry.blockedStatus = blockedStatus;
            
            // SkyrimNet blockable: Check if this TopicInfo is in MenuTopicManager's dialogueList
            // Only menu-based dialogue choices can be blocked from SkyrimNet without subtitle issues
            entry.skyrimNetBlockable = false;
            auto* menuTopicManager = RE::MenuTopicManager::GetSingleton();
            if (menuTopicManager && menuTopicManager->dialogueList && a_topicInfo) {
                // Walk the dialogueList to see if this TopicInfo appears in any dialogue entry
                for (auto it = menuTopicManager->dialogueList->begin(); it != menuTopicManager->dialogueList->end(); ++it) {
                    auto* dialogue = *it;
                    if (dialogue && dialogue->parentTopicInfo == a_topicInfo) {
                        entry.skyrimNetBlockable = true;
                        spdlog::debug("[POPULATE] TopicInfo 0x{:08X} found in MenuTopicManager dialogueList - SkyrimNet blockable",
                            a_topicInfo->GetFormID());
                        break;
                    }
                }
            }
            
            // Text-based deduplication: Skip if same speaker said same text within 5 seconds
            TextDialogueKey textKey{speakerFormID, entry.responseText};
            const int64_t TEXT_COOLDOWN_DURATION = 5000; // 5 seconds
            
            {
                std::lock_guard<std::mutex> lock(g_loggedMutex);
                auto it = g_recentlyLoggedByText.find(textKey);
                if (it != g_recentlyLoggedByText.end()) {
                    int64_t timeSinceLast = now - it->second;
                    if (timeSinceLast < TEXT_COOLDOWN_DURATION) {
                        spdlog::info("[POPULATE] TEXT DUPLICATE SKIP - Same speaker said same text {}ms ago, skipping", timeSinceLast);
                        return result;
                    }
                }
                g_recentlyLoggedByText[textKey] = now;
                
                // Clean up old text-based entries (>10 seconds)
                for (auto iter = g_recentlyLoggedByText.begin(); iter != g_recentlyLoggedByText.end();) {
                    if (now - iter->second > 10000) {
                        iter = g_recentlyLoggedByText.erase(iter);
                    } else {
                        ++iter;
                    }
                }
            }
            
            // Log to dialogue history
            spdlog::debug("[DIALOGUE] {}: \"{}\" (subtype: {}, status: {}, length: {})", 
                speakerName ? speakerName : "Unknown",
                entry.responseText.empty() ? "(no text)" : entry.responseText.substr(0, 100).c_str(),
                entry.topicSubtype,
                static_cast<int>(blockedStatus),
                entry.responseText.length());
            
            // Distance check: Don't log dialogue from NPCs that are too far away (>5000 units)
            auto player = RE::PlayerCharacter::GetSingleton();
            if (player && a_speaker) {
                float distance = player->GetPosition().GetDistance(a_speaker->GetPosition());
                if (distance > 5000.0f) {
                    spdlog::debug("[DIALOGUE] Speaker '{}' too far away ({:.1f} units), skipping dialogue log", 
                        speakerName ? speakerName : "Unknown", distance);
                    return result;
                }
            }
            
            spdlog::trace("[DATABASE WRITE] About to log dialogue - TopicInfo: 0x{:08X}, Subtype: {}, BlockedStatus: {}, SkyrimNetBlockable: {}",
                topicInfoFormID, Config::GetSubtypeName(subtype), static_cast<int>(blockedStatus), entry.skyrimNetBlockable);
            
            DialogueDB::GetDatabase()->LogDialogue(entry);
            
            spdlog::trace("[DATABASE WRITE] LogDialogue() call completed for TopicInfo: 0x{:08X}", topicInfoFormID);
            
            // Runtime enrichment: Update blacklist entries with all captured responses
            if (entry.isScene && !entry.sceneEditorID.empty() && !entry.allResponses.empty()) {
                DialogueDB::GetDatabase()->EnrichBlacklistEntryAtRuntime(
                    DialogueDB::BlacklistTarget::Scene,
                    entry.sceneEditorID,
                    entry.allResponses
                );
            } else if (!entry.isScene && !entry.topicEditorID.empty() && !entry.allResponses.empty()) {
                // Runtime enrichment for individual topics
                DialogueDB::GetDatabase()->EnrichBlacklistEntryAtRuntime(
                    DialogueDB::BlacklistTarget::Topic,
                    entry.topicEditorID,
                    entry.allResponses
                );
            }
        }
        
        // Check if we should block this dialogue
        if (a_topic && a_topicInfo) {
            RE::TESQuest* quest = a_topic->ownerQuest;
            const char* speakerName = a_speaker ? a_speaker->GetName() : nullptr;

            uint16_t subtype = static_cast<uint16_t>(a_topic->data.subtype.get());

            // Check for scenes (subtype 14)
            if (subtype == 14) {
                const char* topicEditorID = a_topic->GetFormEditorID();
                const char* questEditorID = quest ? quest->GetFormEditorID() : nullptr;
                
                // Check if this scene should be blocked
                bool isBardSong = Config::IsBardSongQuest(quest);
                bool bardSongsEnabled = Config::ShouldBlockBardSongs();
                bool isHardcoded = Config::IsHardcodedAmbientScene(a_topic);
                
                // Check granular scene blocking from unified blacklist (target_type=4)
                // Need to find the scene EditorID, not the topic EditorID
                auto db = DialogueDB::GetDatabase();
                bool shouldBlockSceneAudio = false;
                bool shouldBlockSceneSubtitles = false;
                std::string sceneEditorID;  // Declare in wider scope for safety net
                
                if (db && quest && a_topic) {
                    // Find the scene that contains this topic
                    auto& scenesArray = quest->scenes;
                    
                    for (auto* scene : scenesArray) {
                        if (!scene) continue;
                        for (auto* action : scene->actions) {
                            if (!action || action->GetType() != RE::BGSSceneAction::Type::kDialogue) continue;
                            auto* dialogueAction = static_cast<RE::BGSSceneActionDialogue*>(action);
                            if (dialogueAction && dialogueAction->topic == a_topic) {
                                const char* scnEditorID = scene->GetFormEditorID();
                                sceneEditorID = scnEditorID ? scnEditorID : "";
                                break;
                            }
                        }
                        if (!sceneEditorID.empty()) break;
                    }
                    
                    if (!sceneEditorID.empty()) {
                        shouldBlockSceneAudio = db->ShouldBlockAudio(0, sceneEditorID);
                        shouldBlockSceneSubtitles = db->ShouldBlockSubtitles(0, sceneEditorID);
                    }
                }
                
                bool scenesEnabled = Config::ShouldBlockScenes();
                
                // Check if scene is HARD blocked (scene prevention via phase conditions)
                // Soft blocks should NOT trigger safety net - they're supposed to start and get silenced
                bool isHardBlocked = false;
                if (db && !sceneEditorID.empty()) {
                    // Get the blacklist cache to check block type
                    auto blacklistCache = db->GetBlacklist();
                    auto entry = std::find_if(blacklistCache.begin(), blacklistCache.end(),
                        [&sceneEditorID](const DialogueDB::BlacklistEntry& e) {
                            return e.targetType == DialogueDB::BlacklistTarget::Scene &&
                                   e.targetEditorID == sceneEditorID;
                        });
                    
                    if (entry != blacklistCache.end()) {
                        isHardBlocked = (entry->blockType == DialogueDB::BlockType::Hard);
                    }
                }
                
                // TESTING: Disabled hardcoded scene blocking - only block database entries
                // Safety net ONLY triggers for Hard-blocked scenes (scene prevention)
                // Soft-blocked scenes are supposed to start - they just get silenced during playback
                bool shouldTriggerSafetyNet = false;
                if (isBardSong && bardSongsEnabled) {
                    shouldTriggerSafetyNet = true;
                }
                if (isHardBlocked && scenesEnabled) {
                    shouldTriggerSafetyNet = true;
                }
                
                if (shouldTriggerSafetyNet) {
                    // SAFETY NET: Hard-blocked scene started despite phase conditions
                    // This should not happen - log error for debugging
                    if (!sceneEditorID.empty()) {
                        spdlog::error("[POPULATE SCENE SAFETY] Hard-blocked scene '{}' started despite conditions! "
                                   "Quest: {} - This should be investigated",
                                   sceneEditorID,
                                   questEditorID ? questEditorID : "(unknown)");
                    }
                    
                    // Let scene continue - audio/subtitles blocked by ConstructResponse
                    // Don't remove conditions (would persist in saves causing reload loops)
                }
            }

            // Check granular blocking for logging (ConstructResponse will actually apply the blocks)
            bool shouldBlockAudio = Config::ShouldBlockAudio(quest, a_topic, speakerName, responseText);
            bool shouldBlockSubtitles = Config::ShouldBlockSubtitles(quest, a_topic, speakerName, responseText);
            
            // Set the flag early so SetSubtitle hook can block the first call (even before ConstructResponse runs)
            ConstructResponseHook::SetShouldBlockSubtitles(shouldBlockSubtitles);
            
            if (shouldBlockAudio || shouldBlockSubtitles) {
                uint16_t subtype = static_cast<uint16_t>(a_topic->data.subtype.get());
                const char* topicEditorID = a_topic->GetFormEditorID();
                uint32_t topicFormID = a_topic->GetFormID();

                spdlog::debug("[POPULATE SILENCE] Speaker: {} - Topic: {} (subtype: {}, FormID: 0x{:08X}, blockAudio: {}, blockSubtitles: {})",
                    speakerName ? speakerName : "Unknown",
                    topicEditorID ? topicEditorID : "(none)",
                    subtype,
                    topicFormID,
                    shouldBlockAudio,
                    shouldBlockSubtitles);

                // Don't block - let it through to ConstructResponse hook which will silence it
                // This allows scripts to execute while removing audio/subtitles/animations
            }
        }

        // Return result from original (already called at the top)
        return result;
    }

    void Install()
    {
        // Get the function address
        REL::Relocation<std::uintptr_t> target{ RELOCATION_ID(34429, 35249) };
        _OriginalPopulateTopicInfo = reinterpret_cast<PopulateTopicInfoType>(target.address());

        // Install the hook using Microsoft Detours
        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
        DetourAttach(&(PVOID&)_OriginalPopulateTopicInfo, (PBYTE)&Hook_PopulateTopicInfo);
        
        if (DetourTransactionCommit() != NO_ERROR) {
            spdlog::error("Failed to install PopulateTopicInfo hook!");
            return;
        }

        spdlog::info("========================================");
        spdlog::info("PopulateTopicInfo hook installed successfully");
        spdlog::info("This hook SILENCES dialogue (scripts still run):");
        spdlog::info("  - Removes audio (no sound plays)");
        spdlog::info("  - Removes subtitles (no text shown)");
        spdlog::info("  - Removes animations (no lipsync)");
        spdlog::info("  - Scripts still execute (follower commands work!)");
        spdlog::info("========================================");
    }
    
    void RecordDialogueConstruct(uint32_t speakerFormID, uint32_t topicInfoFormID)
    {
        std::lock_guard<std::mutex> lock(g_constructMutex);
        
        int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        g_recentConstructs.push_back({speakerFormID, topicInfoFormID, now});
        
        // Log for diagnostics
        spdlog::trace("[DIALOGUE_CTOR] Speaker: 0x{:08X} | TopicInfo: 0x{:08X} | Time: {}",
            speakerFormID, topicInfoFormID, now);
    }
    
    void RecordConstructResponse(uint32_t topicInfoFormID)
    {
        // No longer needed - we directly check menu state via UI::IsMenuOpen
        // This function is kept for compatibility but does nothing
    }
    
    bool IsTopicInMenuCache(uint32_t topicInfoFormID)
    {
        // Not needed with dialogue speaker tracking
        return false;
    }
}
