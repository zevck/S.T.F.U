#include "../include/PCH.h"
#include "Logger.h"
#include "DialogueHook.h"
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
#include "../external/Detours/src/detours.h"
#include "../include/version.h"
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

// Forward declare function for PopulateTopicInfo to check if dialogue should be force-logged
namespace DialogueItemCtorHook {
    bool WasBlockedWithNullptr(uint32_t speakerFormID, const std::string& responseText);
    void Install();
}

namespace PopulateTopicInfoHook {
    bool WasRecentlyLogged(uint32_t speakerFormID, const std::string& responseText, int64_t withinMs);
}

// DialogueItem::Ctor hook - blocks SkyrimNet logging only
namespace DialogueItemCtorHook
{
    using DialogueItemCtor_t = RE::DialogueItem* (*)(RE::DialogueItem*, RE::TESQuest*, RE::TESTopic*, RE::TESTopicInfo*, RE::Actor*);
    static DialogueItemCtor_t _DialogueItemCtor = nullptr;
    
    // Track dialogues where we returned nullptr so PopulateTopicInfo can log them
    struct NullptrDialogueKey {
        uint32_t speakerFormID;
        std::string responseText;
        int64_t timestamp;
        
        bool operator==(const NullptrDialogueKey& other) const {
            return speakerFormID == other.speakerFormID && responseText == other.responseText;
        }
    };
    
    struct NullptrDialogueKeyHash {
        size_t operator()(const NullptrDialogueKey& key) const {
            return std::hash<uint32_t>()(key.speakerFormID) ^ std::hash<std::string>()(key.responseText);
        }
    };
    
    static std::unordered_set<NullptrDialogueKey, NullptrDialogueKeyHash> g_nullptrLoggedDialogues;
    static std::mutex g_nullptrMutex;    
    
    RE::DialogueItem* Hook_DialogueItemCtor(RE::DialogueItem* a_this, RE::TESQuest* a_quest, RE::TESTopic* a_topic, RE::TESTopicInfo* a_topicInfo, RE::Actor* a_speaker)
    {
        // Log entry for all dialogue constructions
        if (a_topic && a_topicInfo && a_speaker) {
            const char* topicEditorID = a_topic->GetFormEditorID();
            const char* speakerName = a_speaker->GetName();
            uint16_t subtype = Config::GetAccurateSubtype(a_topic);
            spdlog::debug("[CTOR ENTRY] Speaker: {}, Topic: {}, TopicInfo: 0x{:08X}, Subtype: {}",
                speakerName ? speakerName : "Unknown",
                topicEditorID ? topicEditorID : "(none)",
                a_topicInfo->GetFormID(),
                Config::GetSubtypeName(subtype));
        }
        
        // If SkyrimNet is loaded AND this is MENU dialogue AND should be SkyrimNet-blocked, return nullptr
        // This prevents SkyrimNet from seeing this dialogue while allowing subtitles to work normally
        // NOTE: Only for MENU dialogue - background/ambient dialogue uses ConstructResponse blocking
        if (Config::IsSkyrimNetLoaded() && a_topic && a_topicInfo && a_speaker) {
            const char* topicEditorID = a_topic->GetFormEditorID();
            const char* speakerName = a_speaker->GetName();
            uint16_t subtype = Config::GetAccurateSubtype(a_topic);
            
            // Check if this is menu dialogue (player actively talking to NPC)
            bool isMenuDialogue = false;
            auto* ui = RE::UI::GetSingleton();
            if (ui && ui->IsMenuOpen(RE::DialogueMenu::MENU_NAME)) {
                auto* menuTopicManager = RE::MenuTopicManager::GetSingleton();
                if (menuTopicManager && a_speaker) {
                    auto speakerHandle = a_speaker->GetHandle();
                    isMenuDialogue = (speakerHandle == menuTopicManager->speaker);
                }
            }
            
            // Only process blocking for menu dialogue - background dialogue uses ConstructResponse
            if (isMenuDialogue) {
                // Extract response text for checking block status
                std::string responseText;
                if (a_topicInfo) {
                    auto responses = TopicResponseExtractor::ExtractResponsesFromTopicInfo(a_topicInfo);
                    for (size_t i = 0; i < responses.size(); ++i) {
                        if (i > 0) responseText += " ";
                        responseText += responses[i];
                    }
                }
                
                // Check block status
                bool shouldBlockSkyrimNet = Config::ShouldBlockSkyrimNet(a_quest, a_topic, speakerName);
                bool shouldSoftBlock = Config::ShouldSoftBlock(a_quest, a_topic, speakerName, responseText.c_str());
                
                // Return nullptr for SkyrimNet-only blocked menu dialogue (not soft-blocked)
                // Soft-blocked menu dialogue has responseText cleared in PopulateTopicInfo
                if (shouldBlockSkyrimNet && !shouldSoftBlock) {
                    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count();
                    
                    // Check if already logged (either manually or by PopulateTopicInfo)
                    bool alreadyLogged = false;
                    
                    // First check PopulateTopicInfo's recent logs
                    if (PopulateTopicInfoHook::WasRecentlyLogged(a_speaker->GetFormID(), responseText, 5000)) {
                        alreadyLogged = true;
                        spdlog::debug("[CTOR NULLPTR] DUPLICATE - PopulateTopicInfo already logged this, skipping: text='{}'",
                            responseText.substr(0, 50));
                    }
                    
                    // Also check our own manual logs
                    if (!alreadyLogged) {
                        std::lock_guard<std::mutex> lock(g_nullptrMutex);
                        
                        // Clean up old entries first (>5 seconds)
                        for (auto it = g_nullptrLoggedDialogues.begin(); it != g_nullptrLoggedDialogues.end();) {
                            if (now - it->timestamp > 5000) {
                                it = g_nullptrLoggedDialogues.erase(it);
                            } else {
                                ++it;
                            }
                        }
                        
                        // Check if this speaker+text combo was already manually logged
                        NullptrDialogueKey searchKey{ a_speaker->GetFormID(), responseText, 0 };
                        for (const auto& key : g_nullptrLoggedDialogues) {
                            if (key.speakerFormID == searchKey.speakerFormID && key.responseText == searchKey.responseText) {
                                int64_t timeSince = now - key.timestamp;
                                if (timeSince < 5000) { // Within last 5 seconds
                                    alreadyLogged = true;
                                    spdlog::debug("[CTOR NULLPTR] DUPLICATE - Already manually logged {}ms ago, skipping: text='{}'",
                                        timeSince, responseText.substr(0, 50));
                                    break;
                                }
                            }
                        }
                    }
                    
                    if (!alreadyLogged) {
                        // Skip if no text content
                        std::string trimmedText = responseText;
                        trimmedText.erase(0, trimmedText.find_first_not_of(" \t\n\r"));
                        trimmedText.erase(trimmedText.find_last_not_of(" \t\n\r") + 1);
                        
                        if (trimmedText.empty()) {
                            spdlog::debug("[CTOR NULLPTR] Skipping log - no text for TopicInfo 0x{:08X}",
                                a_topicInfo->GetFormID());
                            return nullptr;
                        }
                        
                        // Manually log to database BEFORE returning nullptr (PopulateTopicInfo won't log it)
                        spdlog::debug("[CTOR NULLPTR] Manually logging to database before returning nullptr");
                        
                        DialogueDB::DialogueEntry entry;
                        entry.timestamp = now / 1000;
                        
                        entry.speakerName = speakerName ? speakerName : "";
                        entry.speakerFormID = a_speaker->GetFormID();
                        entry.speakerBaseFormID = a_speaker->GetActorBase() ? a_speaker->GetActorBase()->GetFormID() : 0;
                        
                        entry.topicEditorID = topicEditorID ? topicEditorID : "";
                        entry.topicFormID = a_topic->GetFormID();
                        entry.topicSubtype = subtype;
                        entry.topicSubtypeName = Config::GetSubtypeName(subtype);
                        
                        entry.questEditorID = a_quest && a_quest->GetFormEditorID() ? a_quest->GetFormEditorID() : "";
                        entry.questFormID = a_quest ? a_quest->GetFormID() : 0;
                        entry.questName = a_quest && a_quest->GetName() ? a_quest->GetName() : "";
                        
                        auto* topicInfoFile = a_topicInfo->GetFile(0);
                        entry.sourcePlugin = topicInfoFile ? std::string(topicInfoFile->GetFilename()) : "";
                        
                        auto* topicFile = a_topic->GetFile(0);
                        entry.topicSourcePlugin = topicFile ? std::string(topicFile->GetFilename()) : "";
                        
                        entry.topicInfoFormID = a_topicInfo->GetFormID();
                        entry.responseText = responseText;
                        
                        // Extract all responses
                        if (!entry.topicEditorID.empty()) {
                            entry.allResponses = TopicResponseExtractor::ExtractAllResponsesForTopic(entry.topicEditorID);
                        } else if (entry.topicFormID > 0) {
                            entry.allResponses = TopicResponseExtractor::ExtractAllResponsesForTopic(entry.topicFormID);
                        }
                        if (entry.allResponses.empty() && !responseText.empty()) {
                            entry.allResponses.push_back(responseText);
                        }
                        
                        entry.voiceFilepath = "";
                        entry.blockedStatus = DialogueDB::BlockedStatus::SoftBlock;
                        entry.isScene = false;
                        entry.isBardSong = Config::IsBardSongQuest(a_quest);
                        entry.isHardcodedScene = Config::IsHardcodedAmbientScene(a_topic);
                        entry.skyrimNetBlockable = true;
                        entry.sceneEditorID = "";
                        
                        // Log to database
                        DialogueDB::GetDatabase()->LogDialogue(entry);
                        spdlog::debug("[CTOR NULLPTR] Logged to database: speaker=0x{:08X}, topicInfo=0x{:08X}, text='{}'",
                            entry.speakerFormID, entry.topicInfoFormID, responseText.substr(0, 50));
                        
                        // Track that this was manually logged (using speaker + responseText as key)
                        {
                            std::lock_guard<std::mutex> lock(g_nullptrMutex);
                            NullptrDialogueKey key{ a_speaker->GetFormID(), responseText, now };
                            g_nullptrLoggedDialogues.insert(key);
                        }
                    }
                    
                    spdlog::debug("[CTOR NULLPTR] Returning nullptr to block dialogue from SkyrimNet: Topic='{}', Speaker='{}'", 
                        topicEditorID ? topicEditorID : "(none)", 
                        speakerName ? speakerName : "Unknown");
                    
                    return nullptr;
                }
                else if (shouldBlockSkyrimNet && shouldSoftBlock) {
                    // This dialogue is SOFT-BLOCKED - it shouldn't have reached DialogueItem::Ctor at all!
                    // PopulateTopicInfo should have cleared responseText, preventing construction
                    spdlog::warn("[CTOR SOFT-BLOCKED] Soft-blocked dialogue reached Ctor (shouldn't happen): Topic='{}', Speaker='{}', Text='{}'",
                        topicEditorID ? topicEditorID : "(none)", 
                        speakerName ? speakerName : "Unknown",
                        responseText.substr(0, 50));
                    // Don't return nullptr - let it construct normally (ConstructResponse will handle blocking)
                }
            } // End isMenuDialogue check
        }
        
        // Record this construction for correlation with PopulateTopicInfo
        if (a_speaker && a_topicInfo) {
            uint32_t speakerID = a_speaker->GetFormID();
            uint32_t topicInfoID = a_topicInfo->GetFormID();
            const char* speakerName = a_speaker->GetName();
            
            spdlog::trace("[CTOR RECORD] Recording construct: speaker={} (0x{:08X}), topicInfo=0x{:08X}", 
                speakerName ? speakerName : "Unknown", speakerID, topicInfoID);
            PopulateTopicInfoHook::RecordDialogueConstruct(speakerID, topicInfoID);
        }
        
        // Call original Dialogue Item constructor
        spdlog::trace("[CTOR ALLOW] Creating DialogueItem normally");
        return _DialogueItemCtor(a_this, a_quest, a_topic, a_topicInfo, a_speaker);
    }
    
    // Check if a dialogue was blocked (nullptr returned) by DialogueItem::Ctor
    bool WasBlockedWithNullptr(uint32_t speakerFormID, const std::string& responseText)
    {
        std::lock_guard<std::mutex> lock(g_nullptrMutex);
        
        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        // Check if this dialogue was manually logged by DialogueItem::Ctor (speaker + responseText match)
        for (auto it = g_nullptrLoggedDialogues.begin(); it != g_nullptrLoggedDialogues.end();) {
            // Clean up old entries (older than 5 seconds)
            if (now - it->timestamp > 5000) {
                it = g_nullptrLoggedDialogues.erase(it);
                continue;
            }
            
            // Check if this matches (speaker + responseText)
            if (it->speakerFormID == speakerFormID && it->responseText == responseText) {
                int64_t timeSince = now - it->timestamp;
                if (timeSince < 5000) {
                    spdlog::debug("[CTOR NULLPTR] Found nullptr-blocked dialogue {}ms ago, PopulateTopicInfo should skip", timeSince);
                    return true; // Don't erase - multiple PopulateTopicInfo calls need to see it
                }
            }
            ++it;
        }
        
        return false; // Not found in recent logs
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
                
                // Initialize PrismaUI menu (must be done after database init)
                PrismaUIMenu::Initialize();
                if (auto* inputManager = RE::BSInputDeviceManager::GetSingleton()) {
                    inputManager->AddEventSink(InputEventSink::GetSingleton());
                    spdlog::info("Registered menu hotkey handler");
                } else {
                    spdlog::error("Failed to get BSInputDeviceManager - menu hotkey will not work!");
                }
                
                // Register a menu-event sink that watches for the Loading Menu closing.
                // This fires after every loading screen (door transitions, fast travel, save loads)
                // and is more reliable than TESLoadGameEvent which only fires on save loads.
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
                // Note: deferred scene patching is handled by LoadingMenuSink (fires when
                // the loading screen closes), so no explicit PatchDeferredScenes() call needed here.
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

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Load(const SKSE::LoadInterface* a_skse)
{
    Logger::Setup();

    SKSE::Init(a_skse);

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
