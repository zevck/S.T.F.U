#include "AddTopicHook.h"
#include "Config.h"
#include "DialogueDatabase.h"
#include "TopicResponseExtractor.h"
#include "../include/PCH.h"
#include <spdlog/spdlog.h>
#include <chrono>

namespace AddTopicHook
{
    // AddTopic function from RE namespace
    // Returns int64_t, takes MenuTopicManager*, TESTopic*, and two int64 params
    using AddTopicType = int64_t(*)(RE::MenuTopicManager*, RE::TESTopic*, int64_t, int64_t);
    static REL::Relocation<AddTopicType> _AddTopic;

    // Our hook - prevents blocked topics from being added to dialogue menu
    int64_t Hook_AddTopic(RE::MenuTopicManager* a_this, RE::TESTopic* a_topic, int64_t a_3, int64_t a_4)
    {
        if (!a_topic) {
            return _AddTopic(a_this, a_topic, a_3, a_4);
        }

        // Check if this topic should be blocked
        RE::TESQuest* quest = a_topic->ownerQuest;
        
        // Get speaker for context (if available)
        const char* speakerName = nullptr;
        if (a_this && a_this->speaker.get()) {
            auto speaker = a_this->speaker.get().get();
            if (speaker) {
                speakerName = speaker->GetName();
            }
        }

        if (Config::ShouldBlockDialogue(quest, a_topic, speakerName, nullptr)) {
            const char* topicEditorID = a_topic->GetFormEditorID();
            uint16_t subtype = static_cast<uint16_t>(a_topic->data.subtype.get());
            uint32_t topicFormID = a_topic->GetFormID();
            // Extract response text to check if it's empty
            std::vector<std::string> extractedResponses;
            if (topicEditorID && topicEditorID[0]) {
                extractedResponses = TopicResponseExtractor::ExtractAllResponsesForTopic(std::string(topicEditorID));
            } else if (topicFormID > 0) {
                extractedResponses = TopicResponseExtractor::ExtractAllResponsesForTopic(topicFormID);
            }
            
            // Check if we have any actual text
            bool hasText = false;
            for (const auto& resp : extractedResponses) {
                std::string trimmed = resp;
                trimmed.erase(0, trimmed.find_first_not_of(" \t\n\r"));
                trimmed.erase(trimmed.find_last_not_of(" \t\n\r") + 1);
                if (!trimmed.empty()) {
                    hasText = true;
                    break;
                }
            }
            
            if (!hasText) {
                spdlog::info("[HARD BLOCK] Skipping log - no text: {} (subtype: {})",
                    topicEditorID ? topicEditorID : "(none)", Config::GetSubtypeName(subtype));
                return 0;
            }
            
            // Filter breathing/grunt sounds from history even when hard blocked
            if (Config::ShouldFilterFromHistory(nullptr, subtype, a_topic)) {
                spdlog::info("[HARD BLOCK] Filtered from history: {} (subtype: {})", 
                    topicEditorID ? topicEditorID : "(none)", subtype);
                return 0;  // Block without logging
            }

            spdlog::info("[HARD BLOCK] Speaker: {} - Topic: {} (subtype: {}, FormID: 0x{:08X})",
                speakerName ? speakerName : "Unknown",
                topicEditorID ? topicEditorID : "(none)",
                subtype,
                topicFormID);

            // Log to history database before blocking
            DialogueDB::DialogueEntry entry;
            auto now = std::chrono::system_clock::now();
            entry.timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
            
            entry.speakerName = speakerName ? speakerName : "Unknown";
            entry.speakerFormID = 0;  // Not available in AddTopicHook
            entry.speakerBaseFormID = 0;  // Not available in AddTopicHook
            
            entry.topicEditorID = topicEditorID ? topicEditorID : "";
            entry.topicFormID = topicFormID;
            entry.topicSubtype = subtype;
            entry.topicSubtypeName = Config::GetSubtypeName(subtype);
            
            entry.questEditorID = quest && quest->GetFormEditorID() ? quest->GetFormEditorID() : "";
            entry.questFormID = quest ? quest->GetFormID() : 0;
            entry.questName = quest && quest->GetName() ? quest->GetName() : "";
            
            entry.sceneEditorID = "";  // Scene info not available here
            entry.topicInfoFormID = 0;  // Not available yet
            
            // Extract response text from topic (even though it won't be spoken)
            if (topicEditorID && topicEditorID[0]) {
                entry.allResponses = TopicResponseExtractor::ExtractAllResponsesForTopic(std::string(topicEditorID));
            } else if (topicFormID > 0) {
                entry.allResponses = TopicResponseExtractor::ExtractAllResponsesForTopic(topicFormID);
            }
            
            // Use first response as main text, or show generic message if no text
            if (!entry.allResponses.empty()) {
                entry.responseText = entry.allResponses[0];
            } else {
                entry.responseText = "(no response text available)";
            }
            
            entry.voiceFilepath = "";
            entry.blockedStatus = DialogueDB::BlockedStatus::HardBlock;
            entry.isScene = (subtype == 14);
            entry.isBardSong = Config::IsBardSongQuest(quest);
            entry.isHardcodedScene = Config::IsHardcodedAmbientScene(a_topic);
            
            DialogueDB::GetDatabase()->LogDialogue(entry);

            // Return 0 to "consume" the topic - prevents it from being added to dialogue menu
            // This means the NPC never enters conversation state for this topic
            return 0;
        }

        // Not blocked - add normally
        return _AddTopic(a_this, a_topic, a_3, a_4);
    }

    void Install()
    {
        auto& trampoline = SKSE::GetTrampoline();

        // Hook AddTopic at two call sites (same as DDR)
        // Call site 1: RELOCATION_ID(34460, 35287)
        _AddTopic = trampoline.write_call<5>(
            REL::Relocation<std::uintptr_t>{ RELOCATION_ID(34460, 35287) }.address() + REL::Relocate(0xFA, 0x154),
            Hook_AddTopic
        );

        // Call site 2: RELOCATION_ID(34477, 35304)
        trampoline.write_call<5>(
            REL::Relocation<std::uintptr_t>{ RELOCATION_ID(34477, 35304) }.address() + REL::Relocate(0x79, 0x6C),
            Hook_AddTopic
        );

        spdlog::info("========================================");
        spdlog::info("AddTopic hook installed successfully");
        spdlog::info("Prevents blocked topics from appearing in menu:");
        spdlog::info("  - Topic never added to dialogue list");
        spdlog::info("  - NPC never enters conversation state");
        spdlog::info("  - No idle animations triggered");
        spdlog::info("  - Equivalent to patcher condition blocking");
        spdlog::info("========================================");
    }
}
