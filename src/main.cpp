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

#include "../include/PCH.h"
#include "EditorID.h"
#include "Logger.h"
#include "DialogueDatabase.h"
#include "ConstructResponseHook.h"
#include "PopulateTopicInfoHook.h"
#include "SceneHook.h"
#include "SceneMonitor.h"
#include "Config.h"
#include "TopicResponseExtractor.h"
#include "PrismaUIMenu.h"
#include "SettingsPersistence.h"
#include "PapyrusInterface.h"
#include <detours.h>
#include <algorithm>
#include <string>
#include <unordered_set>

// Watch for the "Loading Menu" opening — at that point the engine has already stopped all
// running scenes, so it is safe to patch conditions. Patching on open (not close) ensures
// conditions are in place before the new cell's scenes are allowed to start.
class LoadingMenuSink : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
{
public:
    static LoadingMenuSink* GetSingleton()
    {
        static LoadingMenuSink instance;
        return &instance;
    }

    RE::BSEventNotifyControl ProcessEvent(
        const RE::MenuOpenCloseEvent* a_event,
        RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
    {
        if (a_event && a_event->opening && a_event->menuName == "Loading Menu") {
            SceneHook::PatchDeferredScenes();
        }
        return RE::BSEventNotifyControl::kContinue;
    }
};

// DialogueItem::Ctor hook - records constructions for history correlation
namespace DialogueItemCtorHook
{
    using DialogueItemCtor_t = RE::DialogueItem* (*)(RE::DialogueItem*, RE::TESQuest*, RE::TESTopic*, RE::TESTopicInfo*, RE::Actor*);
    static DialogueItemCtor_t _DialogueItemCtor = nullptr;

    // (The previous SkyrimNet caller-identity gate was reverted: SkyrimNet also
    // detours DialogueItem::Ctor via MinHook, so every real-playback call passes
    // through SkyrimNet.dll on its way to STFU's hook. _ReturnAddress() would
    // then point inside SkyrimNet.dll on every real line, causing STFU to skip
    // RecordDialogueConstruct and lose all history entries while SkyrimNet is
    // loaded. The dialogue-extractor noise this guard tried to filter is a
    // smaller problem than missing real history. To re-introduce filtering, ask
    // the SkyrimNet author to expose IsVoiceSampleExtractionMode() via an
    // extern "C" wrapper so we can resolve it with GetProcAddress.)
    
    RE::DialogueItem* Hook_DialogueItemCtor(RE::DialogueItem* a_this, RE::TESQuest* a_quest, RE::TESTopic* a_topic, RE::TESTopicInfo* a_topicInfo, RE::Actor* a_speaker)
    {
        // Log entry for all dialogue constructions
        if (a_topic && a_topicInfo && a_speaker) {
            const char* topicEditorID = STFU::GetEditorID(a_topic);
            const char* speakerName = a_speaker->GetName();
            uint16_t subtype = Config::GetAccurateSubtype(a_topic);
            spdlog::debug("[CTOR ENTRY] Speaker: {}, Topic: {}, TopicInfo: 0x{:08X}, Subtype: {}",
                speakerName ? speakerName : "Unknown",
                topicEditorID ? topicEditorID : "(none)",
                a_topicInfo->GetFormID(),
                Config::GetSubtypeName(subtype));
        }
        
        // Record this construction for correlation with PopulateTopicInfo
        if (a_speaker && a_topicInfo) {
            uint32_t speakerID = a_speaker->GetFormID();
            uint32_t topicInfoID = a_topicInfo->GetFormID();
            uint32_t topicID = a_topic ? a_topic->GetFormID() : 0;
            const char* speakerName = a_speaker->GetName();

            spdlog::trace("[CTOR RECORD] Recording construct: speaker={} (0x{:08X}), topicInfo=0x{:08X}, topic=0x{:08X}",
                speakerName ? speakerName : "Unknown", speakerID, topicInfoID, topicID);
            PopulateTopicInfoHook::RecordDialogueConstruct(speakerID, topicInfoID, topicID);
        }
        
        // Call original Dialogue Item constructor
        spdlog::trace("[CTOR ALLOW] Creating DialogueItem normally");
        return _DialogueItemCtor(a_this, a_quest, a_topic, a_topicInfo, a_speaker);
    }
    
    void Install()
    {
        REL::Relocation<std::uintptr_t> target{ REL::VariantID(34413, 35220, 0x572FD0) };
        _DialogueItemCtor = reinterpret_cast<DialogueItemCtor_t>(target.address());
        
        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
        DetourAttach(&(PVOID&)_DialogueItemCtor, (PBYTE)&Hook_DialogueItemCtor);
        
        if (DetourTransactionCommit() != NO_ERROR) {
            spdlog::error("Failed to install DialogueItem::Ctor hook!");
        } else {
            spdlog::info("DialogueItem::Ctor hook installed (Detours - early execution)");
        }
    }
    

}

namespace
{
    // Input event sink for the PrismaUI menu hotkey
    class InputEventSink : public RE::BSTEventSink<RE::InputEvent*>
    {
    public:
        static InputEventSink* GetSingleton()
        {
            static InputEventSink singleton;
            return &singleton;
        }

        RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* a_event,
                                              RE::BSTEventSource<RE::InputEvent*>*) override
        {
            if (!a_event) return RE::BSEventNotifyControl::kContinue;
            for (auto* event = *a_event; event; event = event->next) {
                if (event->eventType != RE::INPUT_EVENT_TYPE::kButton) continue;
                const auto* button = static_cast<const RE::ButtonEvent*>(event);
                if (button->GetDevice() != RE::INPUT_DEVICE::kKeyboard) continue;
                if (!button->IsDown()) continue;
                const auto keyCode = button->GetIDCode();
                const auto& settings = Config::GetSettings();
                if (keyCode == settings.menuHotkey) {
                    spdlog::debug("[HOTKEY] Menu hotkey (0x{:X}) detected - toggling menu", keyCode);
                    PrismaUIMenu::Toggle();
                }
            }
            return RE::BSEventNotifyControl::kContinue;
        }

    private:
        InputEventSink() = default;
        InputEventSink(const InputEventSink&) = delete;
        InputEventSink(InputEventSink&&) = delete;
        InputEventSink& operator=(const InputEventSink&) = delete;
        InputEventSink& operator=(InputEventSink&&) = delete;
    };
    
    void MessageHandler(SKSE::MessagingInterface::Message* a_msg)
    {
        switch (a_msg->type) {
        case SKSE::MessagingInterface::kDataLoaded:
            {
                // Load configuration first
                Config::Load();
                
                // Initialize dialogue database in Data/SKSE/Plugins/STFU/ for MO2 compatibility
                // MO2 will virtualize this path and redirect to overwrite folder on first creation
                wchar_t buffer[MAX_PATH];
                GetModuleFileNameW(nullptr, buffer, MAX_PATH);
                std::filesystem::path exePath(buffer);
                auto dataPath = exePath.parent_path() / "Data" / "SKSE" / "Plugins" / "STFU";
                std::filesystem::create_directories(dataPath);
                std::filesystem::create_directories(dataPath / "data");
                auto dbPath = (dataPath / "data" / "dialogue.db").string();
                
                spdlog::info("Initializing database at: {}", dbPath);
                
                if (!DialogueDB::GetDatabase()->Initialize(dbPath)) {
                    spdlog::error("Failed to initialize dialogue database!");
                } else {
                    // One-shot import gated by a persistent meta flag rather than a
                    // row count. The old count-based check (HasScenesImported) would
                    // re-import every scene if the user ever cleared the blacklist or
                    // removed every scene — flag-based gating keeps user curation.
                    // Explicit re-imports via MCM / Prisma UI buttons still work.
                    auto* db = DialogueDB::GetDatabase();
                    if (!db->GetMetaFlag("hardcoded_scenes_initialized")) {
                        spdlog::info("Importing hardcoded scenes (first-run)...");
                        auto scenesList = Config::GetHardcodedScenesList();
                        db->ImportHardcodedScenes(scenesList);

                        spdlog::info("Importing follower commentary scenes (first-run)...");
                        auto followerScenes = Config::GetFollowerCommentaryScenesList();
                        db->ImportHardcodedScenes(followerScenes, "FollowerCommentary");

                        db->SetMetaFlag("hardcoded_scenes_initialized", true);
                    } else {
                        spdlog::info("Hardcoded scenes already initialized, skipping auto-import");
                    }
                    
                    // Load persistent settings from database
                    spdlog::info("Loading persistent settings...");
                    SettingsPersistence::LoadSettings();
                    
                    // Initialize SceneMonitor to register bard song scenes
                    spdlog::info("Initializing SceneMonitor...");
                    SceneMonitor::Initialize();
                }
                
                // Allocate trampoline memory for hooks: SKSE's branch pool if available, otherwise
                // our own block near the game module (SKSE::AllocTrampoline dropped that fallback
                // in CommonLib v9 and silently allocates nothing without a TrampolineInterface)
                {
                    constexpr std::size_t trampolineSize = 1 << 8;  // 256 bytes
                    auto& trampoline = SKSE::GetTrampoline();
                    void* mem = nullptr;
                    if (const auto* intfc = SKSE::GetTrampolineInterface()) {
                        mem = intfc->AllocateFromBranchPool(trampolineSize);
                    }
                    if (mem) {
                        trampoline.set_trampoline(mem, trampolineSize);
                    } else {
                        trampoline.create(trampolineSize);
                    }
                }
                
                // Install PopulateTopicInfo hook (blocks dialogue selection at source - earliest interception)
                PopulateTopicInfoHook::Install();
                
                // Install Scene hook (blocks entire scenes from starting - both dialogue and packages)
                SceneHook::Install();
                
                // Install ConstructResponse hook (blocks audio/subtitles at execution level)
                ConstructResponseHook::Install();
                
                // Install DialogueItem::Ctor hook using Detours (better hook priority than SKSE trampoline)
                DialogueItemCtorHook::Install();
                
                // Initialize PrismaUI menu (must be done after database init)
                PrismaUIMenu::Initialize();
                if (auto* inputManager = RE::BSInputDeviceManager::GetSingleton()) {
                    inputManager->AddEventSink(InputEventSink::GetSingleton());
                    spdlog::info("Registered menu hotkey handler");
                } else {
                    spdlog::error("Failed to get BSInputDeviceManager - menu hotkey will not work!");
                }
                
                // Watch for the Loading Menu opening: fires on every loading screen (doors, fast
                // travel, save loads), unlike TESLoadGameEvent, which only fires on save loads.
                if (auto* ui = RE::UI::GetSingleton()) {
                    ui->AddEventSink(LoadingMenuSink::GetSingleton());
                    spdlog::info("[MAIN] Registered LoadingMenuSink for deferred scene patching");
                }

                // Patch scenes immediately after data is loaded to prevent early scenes from starting unblocked
                spdlog::info("Patching scenes...");
                SceneHook::PatchScenes();
                
                spdlog::info("STFU initialized successfully");
            }
            break;
            
        case SKSE::MessagingInterface::kPostLoadGame:
            {
                spdlog::info("[MAIN] kPostLoadGame event fired - reloading settings from INI");
                // Reload settings from INI after save game loads (save game overwrites global values)
                SettingsPersistence::LoadSettings();
                // Flush database queue periodically
                DialogueDB::GetDatabase()->FlushQueue();
                // Deferred scene patching is handled by LoadingMenuSink (Loading Menu opening).
            }
            break;
            
        case SKSE::MessagingInterface::kNewGame:
            {
                spdlog::info("[MAIN] kNewGame event fired - loading settings from INI");
                // Load settings from INI for new game
                SettingsPersistence::LoadSettings();
            }
            break;
            
        case SKSE::MessagingInterface::kPreLoadGame:
            break;
        }
    }
}

SKSE_PLUGIN_LOAD(const SKSE::LoadInterface* a_skse)
{
    Logger::Setup();

    // Pass a_log=false to keep SKSE::Init from wiping out our logger with its
    // own file sink (also truncate=true) and its compile-time-locked level.
    // Without this, Logger::Setup's pattern and debug-flag detection are
    // silently overridden and the log is always info-level in Release builds.
    SKSE::Init(a_skse, /*a_log=*/false);

    auto messaging = SKSE::GetMessagingInterface();
    if (!messaging->RegisterListener(MessageHandler)) {
        spdlog::error("Failed to register messaging listener!");
        return false;
    }
// Register Papyrus functions for MCM
    auto papyrus = SKSE::GetPapyrusInterface();
    if (papyrus) {
        papyrus->Register(PapyrusInterface::RegisterFunctions);
        spdlog::info("Registered Papyrus interface for STFU_MCM");
    } else {
        spdlog::warn("Failed to get Papyrus interface - MCM functions will not be available");
    }

    
    return true;
}
