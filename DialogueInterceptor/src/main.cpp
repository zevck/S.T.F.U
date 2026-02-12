#include "../include/PCH.h"
#include "Logger.h"
#include "DialogueHook.h"
#include "Config.h"
#include "../include/version.h"

// Hook Actor::InitiateDialogue - Attempt to block at dialogue initiation point
namespace
{
    using InitiateDialogue_t = void (RE::Actor::*)(RE::Actor*, RE::PackageLocation*, RE::PackageLocation*);
    InitiateDialogue_t _OriginalInitiateDialogue = nullptr;
    
    using DialogueItemCtor_t = RE::DialogueItem* (*)(RE::DialogueItem*, RE::TESQuest*, RE::TESTopic*, RE::TESTopicInfo*, RE::Actor*);
    REL::Relocation<DialogueItemCtor_t> _DialogueItemCtor;
    
    bool ShouldBlockDialogue(RE::TESQuest* a_quest, RE::TESTopic* a_topic, const char* speakerName)
    {
        if (!a_topic) return false;
        
        const auto& settings = Config::GetSettings();
        
        // Check if globally enabled (if toggle exists, check its value)
        if (settings.toggleGlobal) {
            float globalValue = settings.toggleGlobal->value;
            if (globalValue == 0.0f) {
                // Toggle is OFF - don't block anything
                return false;
            }
        }
        // If no toggle global exists, default to enabled (always block)
        
        // Check quest-level blocking first (blocks all topics from this quest)
        if (a_quest) {
            uint32_t questFormID = a_quest->GetFormID();
            if (settings.blacklistedQuestFormIDs.find(questFormID) != settings.blacklistedQuestFormIDs.end()) {
                return true;
            }
            
            const char* questEditorID = a_quest->GetFormEditorID();
            if (questEditorID && settings.blacklistedQuestEditorIDs.find(questEditorID) != settings.blacklistedQuestEditorIDs.end()) {
                return true;
            }
        }
        
        // Check topic FormID
        uint32_t topicFormID = a_topic->GetFormID();
        if (settings.blacklistedFormIDs.find(topicFormID) != settings.blacklistedFormIDs.end()) {
            return true;
        }
        
        // Check topic EditorID
        const char* topicEditorID = a_topic->GetFormEditorID();
        if (topicEditorID && settings.blacklistedEditorIDs.find(topicEditorID) != settings.blacklistedEditorIDs.end()) {
            return true;
        }
        
        return false;
    }
    
    void Hook_InitiateDialogue(RE::Actor* a_this, RE::Actor* a_target, RE::PackageLocation* a_loc1, RE::PackageLocation* a_loc2)
    {
        spdlog::info("========================================");
        spdlog::info("[HOOK] Actor::InitiateDialogue");
        
        const char* initiatorName = a_this ? a_this->GetName() : "Unknown";
        const char* targetName = a_target ? a_target->GetName() : "Unknown";
        
        spdlog::info("[INITIATOR] {}", initiatorName);
        spdlog::info("[TARGET] {}", targetName);
        
        // For now, let all dialogue through to see what gets logged
        // In future, check if we should block based on actor or other criteria
        spdlog::info("[STATUS] Allowing dialogue to proceed");
        spdlog::info("========================================");
        
        // Call original function using member function pointer syntax
        if (_OriginalInitiateDialogue) {
            (a_this->*_OriginalInitiateDialogue)(a_target, a_loc1, a_loc2);
        }
    }
    
    RE::DialogueItem* Hook_DialogueItemCtor(RE::DialogueItem* a_this, RE::TESQuest* a_quest, RE::TESTopic* a_topic, RE::TESTopicInfo* a_topicInfo, RE::Actor* a_speaker)
    {
        spdlog::info("========================================");
        spdlog::info("[HOOK] DialogueItem::Ctor");
        
        const char* speakerName = a_speaker ? a_speaker->GetName() : "Unknown";
        uint32_t questFormID = a_quest ? a_quest->GetFormID() : 0;
        const char* questEditorID = a_quest ? a_quest->GetFormEditorID() : "(none)";
        uint32_t topicFormID = a_topic ? a_topic->GetFormID() : 0;
        const char* topicEditorID = a_topic ? a_topic->GetFormEditorID() : "(none)";
        uint32_t responseFormID = a_topicInfo ? a_topicInfo->GetFormID() : 0;
        
        spdlog::info("[SPEAKER] {}", speakerName);
        spdlog::info("[QUEST] FormID: 0x{:08X}, EditorID: {}", questFormID, questEditorID ? questEditorID : "(none)");
        spdlog::info("[TOPIC] FormID: 0x{:08X}, EditorID: {}", topicFormID, topicEditorID ? topicEditorID : "(none)");
        spdlog::info("[RESPONSE] FormID: 0x{:08X}", responseFormID);
        
        if (ShouldBlockDialogue(a_quest, a_topic, speakerName)) {
            spdlog::warn("[BLOCKED] SkyrimNet logging prevented (follower/merchant topic)");
            spdlog::info("========================================");
            return nullptr;  // Blocks SkyrimNet logging, scripts still execute
        }
        
        spdlog::info("========================================");
        return _DialogueItemCtor(a_this, a_quest, a_topic, a_topicInfo, a_speaker);
    }
}

namespace
{
    void MessageHandler(SKSE::MessagingInterface::Message* a_msg)
    {
        switch (a_msg->type) {
        case SKSE::MessagingInterface::kDataLoaded:
            {
                spdlog::info("Data loaded - Installing hooks");
                
                SKSE::AllocTrampoline(1 << 7);
                auto& trampoline = SKSE::GetTrampoline();
                
                // Hook #1: Actor::InitiateDialogue (virtual 0xD8) - Dialogue start point
                try {
                    REL::Relocation<std::uintptr_t> actorVtbl{ RE::VTABLE_Actor[0] };
                    auto originalAddr = actorVtbl.write_vfunc(0xD8, reinterpret_cast<std::uintptr_t>(&Hook_InitiateDialogue));
                    
                    // Convert uintptr_t to member function pointer via union
                    union {
                        std::uintptr_t addr;
                        InitiateDialogue_t func;
                    } converter;
                    converter.addr = originalAddr;
                    _OriginalInitiateDialogue = converter.func;
                    
                    spdlog::info("[HOOK 1] Actor::InitiateDialogue installed (virtual 0xD8)");
                } catch (const std::exception& e) {
                    spdlog::error("[HOOK 1] Failed to install InitiateDialogue hook: {}", e.what());
                }
                
                // Hook #2: DialogueItem::Ctor - Fallback for logging prevention
                REL::Relocation<std::uintptr_t> target{ RELOCATION_ID(34413, 35220) };
                _DialogueItemCtor = trampoline.write_branch<5>(target.address(), Hook_DialogueItemCtor);
                spdlog::info("[HOOK 2] DialogueItem::Ctor installed (fallback)");
                
                spdlog::info("[STATUS] Testing dialogue initiation hook");
                
                Config::Load();
                spdlog::info("Configuration loaded");
            }
            break;
        case SKSE::MessagingInterface::kPostLoadGame:
            spdlog::info("Game loaded");
            break;
        case SKSE::MessagingInterface::kNewGame:
            spdlog::info("New game started");
            break;
        }
    }
}

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Query(const SKSE::QueryInterface* a_skse, SKSE::PluginInfo* a_info)
{
    a_info->infoVersion = SKSE::PluginInfo::kVersion;
    a_info->name = PLUGIN_NAME;
    a_info->version = 1;

    if (a_skse->IsEditor()) {
        return false;
    }

    const auto ver = a_skse->RuntimeVersion();
    if (ver < SKSE::RUNTIME_SSE_1_5_39) {
        return false;
    }

    return true;
}

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Load(const SKSE::LoadInterface* a_skse)
{
    Logger::Setup();

    spdlog::info("========================================");
    spdlog::info("{} v{}", PLUGIN_NAME, PLUGIN_VERSION);
    spdlog::info("{}", PLUGIN_DESCRIPTION);
    spdlog::info("By {}", PLUGIN_AUTHOR);
    spdlog::info("========================================");

    SKSE::Init(a_skse);

    auto messaging = SKSE::GetMessagingInterface();
    if (!messaging->RegisterListener(MessageHandler)) {
        spdlog::error("Failed to register messaging listener!");
        return false;
    }

    spdlog::info("Plugin loaded successfully");
    return true;
}
