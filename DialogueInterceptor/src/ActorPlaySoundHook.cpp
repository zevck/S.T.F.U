#include "../include/PCH.h"
#include "ActorPlaySoundHook.h"
#include "Config.h"
#include <spdlog/spdlog.h>
#include <atomic>

// Extern from main.cpp
extern std::atomic<RE::FormID> g_lastSpeakerFormID;
extern std::atomic<bool> g_expectingDialogueAudio;

namespace ActorPlaySoundHook
{
    // Original function pointer
    using PlayASound_t = void(*)(RE::Actor*, RE::BSSoundHandle&, RE::FormID, bool, std::uint32_t);
    REL::Relocation<PlayASound_t> _OriginalPlayASound;

    bool IsDialogueSpeaker(RE::Actor* a_actor)
    {
        if (!a_actor) return false;
        
        // Get MenuTopicManager singleton
        auto menuTopicManager = RE::MenuTopicManager::GetSingleton();
        if (!menuTopicManager) return false;
        
        // Check if dialogue menu is open
        if (!menuTopicManager->speaker) return false;
        
        // Check if this actor is the current speaker
        auto speaker = menuTopicManager->speaker.get();
        if (!speaker || speaker->formID != a_actor->formID) return false;
        
        // Check if there's an active topic
        return menuTopicManager->currentTopicInfo != nullptr;
    }

    void Hook_PlayASound(RE::Actor* a_this, RE::BSSoundHandle& a_result, RE::FormID a_formID, bool a_arg3, std::uint32_t a_flags)
    {
        // LOG ABSOLUTELY EVERYTHING - no filters
        const char* actorName = a_this ? a_this->GetName() : nullptr;
        spdlog::info("[HOOK] Actor::PlayASound - Actor: {} (ptr: {:p}), FormID: 0x{:08X}, arg3: {}, flags: 0x{:08X}",
                     actorName ? actorName : "<null>", (void*)a_this, a_formID, a_arg3, a_flags);
        
        // Check if this is our tracked speaker
        bool isTracked = g_expectingDialogueAudio && a_this && a_this->GetFormID() == g_lastSpeakerFormID;
        if (isTracked) {
            spdlog::info("  -> THIS IS THE TRACKED DIALOGUE SPEAKER!");
            
            // Check if this is dialogue audio via MenuTopicManager
            bool isDialogue = IsDialogueSpeaker(a_this);
            spdlog::info("  MenuTopicManager isDialogue check: {}", isDialogue);
            
            if (isDialogue) {
                auto menuTopicManager = RE::MenuTopicManager::GetSingleton();
                auto topicInfo = menuTopicManager ? menuTopicManager->currentTopicInfo : nullptr;
                
                bool shouldBlock = false;
                if (topicInfo) {
                    auto topic = topicInfo->parentTopic;
                    auto quest = topic ? topic->ownerQuest : nullptr;
                    shouldBlock = Config::ShouldBlockDialogue(quest, topic, actorName, nullptr);
                }
                
                if (shouldBlock) {
                    spdlog::info("  -> BLOCKING DIALOGUE AUDIO - Invalidating sound handle");
                    a_result.soundID = RE::BSSoundHandle::kInvalidID;
                    a_result.state = RE::BSSoundHandle::AssumedState::kStopped;
                    return;
                }
            }
        }
        
        // Call original function
        _OriginalPlayASound(a_this, a_result, a_formID, a_arg3, a_flags);
    }

    void Install()
    {
        spdlog::info("Installing Actor::PlayASound hook...");
        
        // Actor::PlayASound: RELOCATION_ID(36730, 37743)
        REL::Relocation<std::uintptr_t> PlayASoundAddr{ RELOCATION_ID(36730, 37743) };
        
        auto& trampoline = SKSE::GetTrampoline();
        
        _OriginalPlayASound = trampoline.write_branch<5>(PlayASoundAddr.address(), Hook_PlayASound);
        
        spdlog::info("✓ Actor::PlayASound hook installed at 0x{:X}", PlayASoundAddr.address());
    }
}
