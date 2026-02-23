#include "../include/PCH.h"
#include "Logger.h"
#include "DialogueHook.h"
#include "Config.h"
<<<<<<< Updated upstream
=======
#include "STFUMenu.h"
#include "PrismaUIMenu.h"
#include "TopicResponseExtractor.h"
#include "SettingsPersistence.h"
#include "PapyrusInterface.h"
#include "../external/Detours/src/detours.h"
>>>>>>> Stashed changes
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
        
        // Check subtype blocking (use corrected subtypes for vanilla topics)
        uint16_t subtype = Config::GetAccurateSubtype(a_topic);
        if (settings.blacklistedSubtypes.find(subtype) != settings.blacklistedSubtypes.end()) {
            return true;
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
        // Call original function using member function pointer syntax
        if (_OriginalInitiateDialogue) {
            (a_this->*_OriginalInitiateDialogue)(a_target, a_loc1, a_loc2);
        }
    }
    
    RE::DialogueItem* Hook_DialogueItemCtor(RE::DialogueItem* a_this, RE::TESQuest* a_quest, RE::TESTopic* a_topic, RE::TESTopicInfo* a_topicInfo, RE::Actor* a_speaker)
    {
        if (ShouldBlockDialogue(a_quest, a_topic, nullptr)) {
            const char* speakerName = a_speaker ? a_speaker->GetName() : "Unknown";
            const char* topicEditorID = a_topic ? a_topic->GetFormEditorID() : "(none)";
            uint16_t subtype = a_topic ? static_cast<uint16_t>(a_topic->data.subtype.get()) : 0;
            
            spdlog::info("Dialogue blocked: {} - {} (subtype: {})", speakerName, topicEditorID, subtype);
            return nullptr;  // Blocks SkyrimNet logging, scripts still execute
        }
        
        return _DialogueItemCtor(a_this, a_quest, a_topic, a_topicInfo, a_speaker);
    }
}

namespace
{
<<<<<<< Updated upstream
=======
    // SKSEMenuFramework input event callback for menu hotkey
    // Lifetime: Created during kDataLoaded, automatically cleaned up by unique_ptr on DLL unload
    // SKSEMenuFramework maintains weak references, so no explicit unregistration needed
    static std::unique_ptr<SKSEMenuFramework::Model::InputEvent> g_inputHandler;
    
    bool __stdcall HandleInputEvent(RE::InputEvent* a_event)
    {
        if (!a_event) {
            return false;
        }
        
        for (auto event = a_event; event; event = event->next) {
            if (event->eventType != RE::INPUT_EVENT_TYPE::kButton) {
                continue;
            }
            
            const auto button = static_cast<RE::ButtonEvent*>(event);
            if (!button->IsDown()) {
                continue;
            }
            
            const auto keyCode = button->GetIDCode();
            
            // If in hotkey capture mode, prioritize capturing the key
            if (STFUMenu::IsCapturingHotkey()) {
                if (STFUMenu::CaptureHotkey(keyCode)) {
                    return true;  // Consume the event
                }
            }
            
            // F10 - hardcoded test key for legacy ImGui menu (for comparison during PrismaUI migration)
            if (keyCode == 0x44) {  // F10
                spdlog::debug("[HOTKEY] F10 detected - toggling ImGui menu (test/comparison)");
                STFUMenu::Toggle();
                return true;  // Consume the event
            }
            
            // Check for configured menu hotkey (when not in capture mode)
            const auto& settings = Config::GetSettings();
            if (keyCode == settings.menuHotkey) {
                spdlog::debug("[HOTKEY] Menu hotkey (0x{:X}) detected - toggling PrismaUI menu", keyCode);
                PrismaUIMenu::Toggle();
                return true;  // Consume the event
            }
            
            // Note: We intentionally DON'T consume keyboard events while typing
            // ImGui handles this automatically via WantCaptureKeyboard
            // Consuming events here would block mouse wheel from reaching ImGui
        }
        
        // Always return false to let ImGui receive all input including mouse wheel
        return false;
    }
    
    // Event sink for cell loading - reapplies scene conditions on loading screen transitions
    class CellLoadEventHandler : public RE::BSTEventSink<RE::TESCellFullyLoadedEvent>
    {
    public:
        static CellLoadEventHandler* GetSingleton()
        {
            static CellLoadEventHandler singleton;
            return &singleton;
        }
        
        RE::BSEventNotifyControl ProcessEvent(const RE::TESCellFullyLoadedEvent* a_event, 
                                              RE::BSTEventSource<RE::TESCellFullyLoadedEvent>*)
        {
            if (!a_event || !a_event->cell) {
                return RE::BSEventNotifyControl::kContinue;
            }
            
            // Conditions persist through cell changes - no patching needed
            return RE::BSEventNotifyControl::kContinue;
            
            return RE::BSEventNotifyControl::kContinue;
        }
        
    private:
        CellLoadEventHandler() = default;
        CellLoadEventHandler(const CellLoadEventHandler&) = delete;
        CellLoadEventHandler(CellLoadEventHandler&&) = delete;
        CellLoadEventHandler& operator=(const CellLoadEventHandler&) = delete;
        CellLoadEventHandler& operator=(CellLoadEventHandler&&) = delete;
    };
    
>>>>>>> Stashed changes
    void MessageHandler(SKSE::MessagingInterface::Message* a_msg)
    {
        switch (a_msg->type) {
        case SKSE::MessagingInterface::kDataLoaded:
            {
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
                } catch (const std::exception& e) {
                    spdlog::error("Failed to install InitiateDialogue hook: {}", e.what());
                }
                
                // Hook #2: DialogueItem::Ctor - Fallback for logging prevention
                REL::Relocation<std::uintptr_t> target{ RELOCATION_ID(34413, 35220) };
                _DialogueItemCtor = trampoline.write_branch<5>(target.address(), Hook_DialogueItemCtor);
                
                Config::Load();
<<<<<<< Updated upstream
=======
                
                // Check if SkyrimNet is loaded
                bool skyrimNetLoaded = false;
                auto* dataHandler = RE::TESDataHandler::GetSingleton();
                if (dataHandler) {
                    for (auto* file : dataHandler->files) {
                        if (file) {
                            auto filenameSV = file->GetFilename();
                            if (filenameSV.empty()) continue;
                            std::string filename(filenameSV);
                            std::transform(filename.begin(), filename.end(), filename.begin(), ::tolower);
                            if (filename == "skyrimnet.esp") {
                                skyrimNetLoaded = true;
                                break;
                            }
                        }
                    }
                }
                
                if (skyrimNetLoaded) {
                    spdlog::info("[SKYRIMNET] SkyrimNet.esp detected - will use nullptr blocking for menu dialogue");
                } else {
                    spdlog::info("[SKYRIMNET] SkyrimNet.esp not found - nullptr blocking disabled");
                }
                
                // Initialize dialogue database in Data/SKSE/Plugins/STFU/ for MO2 compatibility
                // MO2 will virtualize this path and redirect to overwrite folder on first creation
                wchar_t buffer[MAX_PATH];
                GetModuleFileNameW(nullptr, buffer, MAX_PATH);
                std::filesystem::path exePath(buffer);
                auto dataPath = exePath.parent_path() / "Data" / "SKSE" / "Plugins" / "STFU";
                std::filesystem::create_directories(dataPath);
                auto dbPath = (dataPath / "dialogue.db").string();
                
                spdlog::info("Initializing database at: {}", dbPath);
                
                if (!DialogueDB::GetDatabase()->Initialize(dbPath)) {
                    spdlog::error("Failed to initialize dialogue database!");
                } else {
                    // Import hardcoded scenes into database (one-time operation)
                    if (!DialogueDB::GetDatabase()->HasScenesImported()) {
                        spdlog::info("Importing hardcoded scenes...");
                        auto scenesList = Config::GetHardcodedScenesList();
                        DialogueDB::GetDatabase()->ImportHardcodedScenes(scenesList);
                        
                        // Import follower commentary scenes with FollowerCommentary filter category
                        spdlog::info("Importing follower commentary scenes...");
                        auto followerScenes = Config::GetFollowerCommentaryScenesList();
                        DialogueDB::GetDatabase()->ImportHardcodedScenes(followerScenes, "FollowerCommentary");
                    } else {
                        spdlog::info("Scenes already imported, skipping scene import");
                    }
                    
                    // Load persistent settings from database
                    spdlog::info("Loading persistent settings...");
                    SettingsPersistence::LoadSettings();
                    
                    // Initialize SceneMonitor to register bard song scenes
                    spdlog::info("Initializing SceneMonitor...");
                    SceneMonitor::Initialize();
                }
                
               // Allocate trampoline memory for hooks
                SKSE::AllocTrampoline(1 << 8);  // 256 bytes
                
                // Install PopulateTopicInfo hook (blocks dialogue selection at source - earliest interception)
                PopulateTopicInfoHook::Install();
                
                // Install Scene hook (blocks entire scenes from starting - both dialogue and packages)
                SceneHook::Install();
                
                // Install ConstructResponse hook (blocks audio/subtitles at execution level)
                ConstructResponseHook::Install();
                
                // Install DialogueItem::Ctor hook using Detours (better hook priority than SKSE trampoline)
                DialogueItemCtorHook::Install();
                
                // Initialize STFU menus (must be done after database init)
                STFUMenu::Initialize();          // ImGui menu (legacy, will be removed)
                PrismaUIMenu::Initialize();      // PrismaUI menu (new)
                
                // Register input handler using SKSEMenuFramework
                if (!SKSEMenuFramework::IsInstalled()) {
                    spdlog::error("SKSEMenuFramework is not installed!");
                } else {
                    g_inputHandler = std::make_unique<SKSEMenuFramework::Model::InputEvent>(HandleInputEvent);
                    spdlog::info("Registered menu hotkey (Insert key)");
                }
                
                // Cell load event handler not needed - conditions persist through cell changes
                
                // Patch scenes immediately after data is loaded to prevent early scenes from starting unblocked
                spdlog::info("Patching scenes...");
                SceneHook::PatchScenes();
                
                spdlog::info("STFU initialized successfully");
>>>>>>> Stashed changes
            }
            break;
        case SKSE::MessagingInterface::kPostLoadGame:
        case SKSE::MessagingInterface::kNewGame:
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

    SKSE::Init(a_skse);

    auto messaging = SKSE::GetMessagingInterface();
    if (!messaging->RegisterListener(MessageHandler)) {
        spdlog::error("Failed to register messaging listener!");
        return false;
    }

    return true;
}
