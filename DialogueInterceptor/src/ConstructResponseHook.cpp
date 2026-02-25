#include "ConstructResponseHook.h"
#include "PopulateTopicInfoHook.h"
#include "Config.h"
#include "DialogueDatabase.h"
#include "STFUMenu.h"
#include "../include/PCH.h"
#include <spdlog/spdlog.h>
#include <cstring>
#include <chrono>

namespace ConstructResponseHook
{
    // Track if current dialogue should be blocked (for SetSubtitle hook)
    thread_local static bool g_shouldBlockCurrent = false;
    
    // Track current dialogue to detect duplicates (same TopicInfo + Speaker within 5 seconds)
    thread_local static RE::FormID g_lastTopicInfoFormID = 0;
    thread_local static RE::TESObjectREFR* g_lastSpeaker = nullptr;
    thread_local static std::chrono::steady_clock::time_point g_lastDialogueTime;

    // Function signature for SetSubtitle
    using SetSubtitleType = char*(*)(RE::DialogueResponse* a_response, char* a_text, int32_t a_unk);
    static REL::Relocation<SetSubtitleType> _SetSubtitle;

    // Hook SetSubtitle to remove subtitles for blocked dialogue
    char* Hook_SetSubtitle(RE::DialogueResponse* a_response, char* a_text, int32_t a_unk)
    {
        if (g_shouldBlockCurrent) {
            // For soft blocking, block subtitles
            spdlog::trace("[SetSubtitle] Blocking subtitle (soft block)");
            static char empty[] = "";
            return _SetSubtitle(a_response, empty, a_unk);
        }
        
        // Normal dialogue - let subtitles work naturally
        return _SetSubtitle(a_response, a_text, a_unk);
    }

    // Allow PopulateTopicInfo to set the blocking flag early
    void SetShouldBlockSubtitles(bool shouldBlock)
    {
        g_shouldBlockCurrent = shouldBlock;
        if (shouldBlock) {
            spdlog::trace("[ConstructResponse] Flag set early by PopulateTopicInfo");
        }
    }
    
    // Check if this is a duplicate dialogue (same TopicInfo + Speaker within 5 seconds)
    bool IsDuplicateDialogue(RE::FormID topicInfoFormID, RE::TESObjectREFR* speaker)
    {
        // First call ever - not a duplicate
        if (g_lastTopicInfoFormID == 0 || g_lastSpeaker == nullptr) {
            g_lastTopicInfoFormID = topicInfoFormID;
            g_lastSpeaker = speaker;
            g_lastDialogueTime = std::chrono::steady_clock::now();
            return false;
        }
        
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - g_lastDialogueTime).count();
        
        // Duplicate if: same TopicInfo + same Speaker + within 5 seconds
        bool isDuplicate = (topicInfoFormID == g_lastTopicInfoFormID) && 
                          (speaker == g_lastSpeaker) && 
                          (elapsed < 5000);
        
        if (!isDuplicate) {
            // Different dialogue - update trackers
            g_lastTopicInfoFormID = topicInfoFormID;
            g_lastSpeaker = speaker;
            g_lastDialogueTime = now;
        }
        
        return isDuplicate;
    }

    // Function signature for ConstructResponse
    using ConstructResponseType = bool(*)(
        RE::TESTopicInfo::ResponseData* a_response,
        char* a_filePath,
        RE::BGSVoiceType* a_voiceType,
        RE::TESTopic* a_topic,
        RE::TESTopicInfo* a_topicInfo
    );
    
    static REL::Relocation<ConstructResponseType> _ConstructResponse;

    // Our hook function
    bool Hook_ConstructResponse(
        RE::TESTopicInfo::ResponseData* a_response,
        char* a_filePath,
        RE::BGSVoiceType* a_voiceType,
        RE::TESTopic* a_topic,
        RE::TESTopicInfo* a_topicInfo)
    {
        // Log entry to track if this hook fires when DialogueItem::Ctor returns nullptr
        if (a_topic && a_topicInfo) {
            const char* topicEditorID = a_topic->GetFormEditorID();
            uint16_t subtype = Config::GetAccurateSubtype(a_topic);
            spdlog::info("[CONSTRUCT ENTRY] Topic: {}, TopicInfo: 0x{:08X}, Subtype: {}",
                topicEditorID ? topicEditorID : "(none)",
                a_topicInfo->GetFormID(),
                Config::GetSubtypeName(subtype));
        }
        
        // Track that ConstructResponse was called for this topic (for SkyrimNet blockable detection)
        if (a_topicInfo) {
            PopulateTopicInfoHook::RecordConstructResponse(a_topicInfo->GetFormID());
        }
        
        g_shouldBlockCurrent = false;
        
        // Pre-check if we should block BEFORE calling original
        bool shouldBlockAudio = false;
        bool shouldBlockSubtitles = false;
        bool shouldBlockSkyrimNet = false;
        
        if (a_topic) {
            RE::TESQuest* quest = a_topic->ownerQuest;
            uint16_t subtype = Config::GetAccurateSubtype(a_topic);
            
            const char* topicEditorID = a_topic->GetFormEditorID();
            uint32_t topicFormID = a_topic->GetFormID();
            
            spdlog::trace("[ConstructResponse] Checking blocking for topic: {} (FormID: 0x{:08X}, subtype: {})", 
                topicEditorID ? topicEditorID : "(none)", topicFormID, subtype);
            
            if (subtype == 14) {
                // Scene dialogue - check whitelist first, then unified blacklist
                auto db = DialogueDB::GetDatabase();
                if (db && topicEditorID) {
                    std::string editorIDStr = topicEditorID;
                    
                    // HIGHEST PRIORITY: Check whitelist - never block whitelisted scenes
                    if (db->IsWhitelisted(DialogueDB::BlacklistTarget::Scene, 0, editorIDStr)) {
                        shouldBlockAudio = false;
                        shouldBlockSubtitles = false;
                        spdlog::trace("[ConstructResponse] Scene dialogue whitelisted - never block");
                    } else {
                        // Retrieve speaker context if available
                        uint32_t actorFormID = 0;
                        std::string actorName = "";
                        auto speakerContext = PopulateTopicInfoHook::GetSpeakerContext(a_topicInfo);
                        if (speakerContext.has_value()) {
                            actorFormID = speakerContext->first;
                            actorName = speakerContext->second;
                            spdlog::trace("[ConstructResponse] Retrieved speaker context: {} (FormID: 0x{:08X})", 
                                actorName, actorFormID);
                        }
                        
                        shouldBlockAudio = db->ShouldBlockAudio(0, editorIDStr, actorFormID, actorName);
                        shouldBlockSubtitles = db->ShouldBlockSubtitles(0, editorIDStr, actorFormID, actorName);
                        spdlog::trace("[ConstructResponse] Scene dialogue: audio={}, subtitles={}", shouldBlockAudio, shouldBlockSubtitles);
                    }
                }
            } else {
                // Regular dialogue - retrieve speaker context and pass to Config functions
                uint32_t actorFormID = 0;
                std::string actorName = "";
                auto speakerContext = PopulateTopicInfoHook::GetSpeakerContext(a_topicInfo);
                if (speakerContext.has_value()) {
                    actorFormID = speakerContext->first;
                    actorName = speakerContext->second;
                }
                
                const char* speakerNamePtr = !actorName.empty() ? actorName.c_str() : nullptr;
                shouldBlockAudio = Config::ShouldBlockAudio(quest, a_topic, speakerNamePtr, nullptr, actorFormID);
                shouldBlockSubtitles = Config::ShouldBlockSubtitles(quest, a_topic, speakerNamePtr, nullptr, actorFormID);
                if (STFUMenu::IsSkyrimNetLoaded()) {
                    shouldBlockSkyrimNet = Config::ShouldBlockSkyrimNet(quest, a_topic, speakerNamePtr);
                }
                spdlog::trace("[ConstructResponse] Regular dialogue: audio={}, subtitles={}, skyrimNet={}", 
                    shouldBlockAudio, shouldBlockSubtitles, shouldBlockSkyrimNet);
            }
            
            // Set flag BEFORE calling original so SetSubtitle hook can use it
            if (shouldBlockSubtitles || shouldBlockAudio) {
                // Soft blocking: block subtitles in SetSubtitle
                g_shouldBlockCurrent = true;
                spdlog::trace("[ConstructResponse] Setting g_shouldBlockCurrent = true (soft block)");
            } else {
                g_shouldBlockCurrent = false;
            }
        }

        // Check for scene hard blocking (subtype 14)
        if (a_topic) {
            uint16_t subtype = Config::GetAccurateSubtype(a_topic);
            
            if (subtype == 14) {
                // Scene detected - check if we should block it
                const char* topicEditorID = a_topic->GetFormEditorID();
                const char* questEditorID = a_topic->ownerQuest ? a_topic->ownerQuest->GetFormEditorID() : nullptr;
                RE::TESQuest* quest = a_topic->ownerQuest;
                bool isHardcoded = Config::IsHardcodedAmbientScene(a_topic);
                bool isBardSong = Config::IsBardSongQuest(quest);
                bool scenesEnabled = Config::ShouldBlockScenes();
                bool bardSongsEnabled = Config::ShouldBlockBardSongs();
                
                // Check granular scene blocking flags from database
                // For scenes, check unified blacklist (target_type=4, formID=0)
                // Need to find the scene EditorID, not the topic EditorID
                auto db = DialogueDB::GetDatabase();
                bool shouldBlockSceneAudio = false;
                bool shouldBlockSceneSubtitles = false;
                
                if (db && quest && a_topic) {
                    // Find the scene that contains this topic
                    std::string sceneEditorID;
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
                
                // Full scene blocking (stops the scene entirely) for hard blocks or if both audio and subtitles are blocked
                // TESTING: Disabled hardcoded scene blocking - only block database entries
                bool shouldHardBlockScene = false;
                
                if ((shouldBlockSceneAudio && shouldBlockSceneSubtitles) && scenesEnabled) {
                    shouldHardBlockScene = true;
                    spdlog::debug("[SCENE BLOCK] Blocking blacklisted scene: {} (quest: {})",
                        topicEditorID ? topicEditorID : "(none)",
                        questEditorID ? questEditorID : "(unknown)");
                }
                if (isBardSong && bardSongsEnabled) {
                    shouldHardBlockScene = true;
                    spdlog::debug("[SCENE BLOCK] Blocking bard song: {} (quest: {})",
                        topicEditorID ? topicEditorID : "(none)",
                        questEditorID ? questEditorID : "(unknown)");
                }
                
                // Don't log scenes to database - ConstructResponse is called constantly
                // for scenes even when not playing (causing Heimskr spam)
                // Scenes aren't useful for the menu anyway since we can't interact with them mid-playback
                
                if (shouldHardBlockScene) {
                    // Find and stop the scene that contains this topic
                    if (quest) {
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
                            
                            if (sceneContainsTopic && scene->isPlaying) {
                                scene->isPlaying = false;
                            }
                        }
                    }
                    
                    return false;  // Don't call original, scene dialogue won't construct
                }
                
                // If only partial blocking (e.g., audio but not subtitles), continue to soft block below
                // This allows granular control of scene audio/subtitles without stopping the scene
            }
        }

        // Call original first
        bool result = _ConstructResponse(a_response, a_filePath, a_voiceType, a_topic, a_topicInfo);
        
        if (!result || !a_filePath || !a_response) {
            return result;
        }

        // Apply blocking that was determined before calling original
        if (a_topic && (shouldBlockAudio || shouldBlockSubtitles || shouldBlockSkyrimNet)) {
            const char* topicEditorID = a_topic->GetFormEditorID();
            uint16_t subtype = Config::GetAccurateSubtype(a_topic);
            
            // SOFT BLOCKING: Audio + subtitles
            if (shouldBlockAudio && shouldBlockSubtitles) {
                spdlog::debug("[SOFT BLOCK] Silencing audio + subtitles for topic: {} (subtype: {})", 
                    topicEditorID ? topicEditorID : "(none)", subtype);
                
                // Block audio - clear file path
                *a_filePath = '\0';
                
                // Block subtitles - set flag for SetSubtitle hook
                g_shouldBlockCurrent = true;
                
                // Clear ResponseData text and animation fields to prevent jerky turns
                auto* response = a_response;
                while (response) {
                    response->responseText = "";
                    response->speakerIdle = nullptr;
                    response->listenerIdle = nullptr;
                    response->emotionType = RE::TESTopicInfo::ResponseData::EmotionType::kNeutral;
                    response->emotionValue = 0;
                    response->flags = RE::TESTopicInfo::ResponseData::Flag::kNone;
                    response = response->next;
                }
            }
            // SKYRIMNET-ONLY BLOCKING: Let everything work naturally, text will be cleared in DialogueItem::Ctor
            else if (shouldBlockSkyrimNet && !shouldBlockAudio && !shouldBlockSubtitles) {
                spdlog::info("[SKYRIMNET-ONLY BLOCK] Allowing natural audio/subtitles/animations, will clear text in Ctor for topic: {} (subtype: {})",
                    topicEditorID ? topicEditorID : "(none)", subtype);
                
                // DON'T clear anything here - let DialogueResponse objects be created with full text
                // SetSubtitle will work naturally
                // DialogueResponse.text will be cleared in DialogueItem::Ctor after subtitles are set
            }
            // Audio-only or subtitle-only blocking
            else {
                if (shouldBlockAudio) {
                    spdlog::debug("[SOFT BLOCK] Silencing audio only for topic: {} (subtype: {})", 
                        topicEditorID ? topicEditorID : "(none)", subtype);
                    *a_filePath = '\0';
                }
                if (shouldBlockSubtitles) {
                    spdlog::debug("[SOFT BLOCK] Silencing subtitles only for topic: {} (subtype: {})", 
                        topicEditorID ? topicEditorID : "(none)", subtype);
                    g_shouldBlockCurrent = true;
                    if (a_topicInfo) {
                        a_topicInfo->data.flags.reset(RE::TOPIC_INFO_DATA::TOPIC_INFO_FLAGS::kForceSubtitle);
                    }
                    auto* response = a_response;
                    while (response) {
                        response->responseText = "";
                        response->speakerIdle = nullptr;
                        response->listenerIdle = nullptr;
                        response->emotionType = RE::TESTopicInfo::ResponseData::EmotionType::kNeutral;
                        response->emotionValue = 0;
                        response->flags = RE::TESTopicInfo::ResponseData::Flag::kNone;
                        response = response->next;
                    }
                }
            }
        }

        return result;
    }

    void Install()
    {
        auto& trampoline = SKSE::GetTrampoline();
        
        REL::Relocation<std::uintptr_t> target{ RELOCATION_ID(34429, 35249) };
        
        // Hook SetSubtitle at call site
        _SetSubtitle = trampoline.write_call<5>(
            target.address() + REL::Relocate(0x61, 0x61),
            Hook_SetSubtitle
        );
        
        spdlog::info("ConstructResponseHook: SetSubtitle hook installed");
        
        _ConstructResponse = trampoline.write_call<5>(
            target.address() + REL::Relocate(0xDE, 0xDE),
            Hook_ConstructResponse
        );
        
        spdlog::info("ConstructResponse + SetSubtitle hooks installed");
    }
}
