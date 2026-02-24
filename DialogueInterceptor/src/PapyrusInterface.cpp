#include "PapyrusInterface.h"
#include "Config.h"
#include "STFUMenu.h"
#include "SettingsPersistence.h"
#include <thread>

namespace PapyrusInterface
{
    void ImportHardcodedScenes(RE::StaticFunctionTag*)
    {
        spdlog::info("[PapyrusInterface] ImportHardcodedScenes called from MCM");
        
        // Run import on background thread to avoid blocking MCM
        std::thread([](){ 
            STFUMenu::ReimportHardcodedScenes(); 
        }).detach();
    }
    
    void ImportFromYAML(RE::StaticFunctionTag*)
    {
        spdlog::info("[PapyrusInterface] ImportFromYAML called from MCM");
        
        // Run import on background thread to avoid blocking MCM
        std::thread([](){ 
            Config::ImportYAMLToDatabase(); 
        }).detach();
    }
    
    int32_t GetMenuHotkey(RE::StaticFunctionTag*)
    {
        const auto& settings = Config::GetSettings();
        return static_cast<int32_t>(settings.menuHotkey);
    }
    
    void SetMenuHotkey(RE::StaticFunctionTag*, int32_t scancode)
    {
        spdlog::info("[PapyrusInterface] SetMenuHotkey called with scancode: 0x{:X}", scancode);
        
        // Validate hotkey
        if (!STFUMenu::IsValidMenuHotkey(static_cast<uint32_t>(scancode))) {
            spdlog::warn("[PapyrusInterface] Invalid hotkey scancode: 0x{:X}", scancode);
            return;
        }
        
        // Update settings
        auto& settings = const_cast<Config::Settings&>(Config::GetSettings());
        settings.menuHotkey = static_cast<uint32_t>(scancode);
        
        // Save to persistent storage
        SettingsPersistence::SaveSettings();
        
        spdlog::info("[PapyrusInterface] Menu hotkey updated to: 0x{:X}", scancode);
    }
    
    void SaveSettings(RE::StaticFunctionTag*)
    {
        spdlog::info("[PapyrusInterface] SaveSettings called from MCM");
        SettingsPersistence::SaveSettings();
    }
    
    int32_t ClearBlacklist(RE::StaticFunctionTag*)
    {
        spdlog::info("[PapyrusInterface] ClearBlacklist called from MCM");
        
        auto db = DialogueDB::GetDatabase();
        if (!db) {
            spdlog::error("[PapyrusInterface] Database not available");
            return 0;
        }
        
        int count = db->ClearBlacklist();
        spdlog::info("[PapyrusInterface] Cleared {} blacklist entries", count);
        return static_cast<int32_t>(count);
    }
    
    int32_t ClearWhitelist(RE::StaticFunctionTag*)
    {
        spdlog::info("[PapyrusInterface] ClearWhitelist called from MCM");
        
        auto db = DialogueDB::GetDatabase();
        if (!db) {
            spdlog::error("[PapyrusInterface] Database not available");
            return 0;
        }
        
        int count = db->ClearWhitelist();
        spdlog::info("[PapyrusInterface] Cleared {} whitelist entries", count);
        return static_cast<int32_t>(count);
    }
    
    bool RegisterFunctions(RE::BSScript::IVirtualMachine* vm)
    {
        if (!vm) {
            spdlog::error("[PapyrusInterface] Virtual machine is null");
            return false;
        }
        
        vm->RegisterFunction("ImportHardcodedScenes", "STFU_MCM", ImportHardcodedScenes);
        vm->RegisterFunction("ImportFromYAML", "STFU_MCM", ImportFromYAML);
        vm->RegisterFunction("GetMenuHotkey", "STFU_MCM", GetMenuHotkey);
        vm->RegisterFunction("SetMenuHotkey", "STFU_MCM", SetMenuHotkey);
        vm->RegisterFunction("SaveSettings", "STFU_MCM", SaveSettings);
        vm->RegisterFunction("ClearBlacklist", "STFU_MCM", ClearBlacklist);
        vm->RegisterFunction("ClearWhitelist", "STFU_MCM", ClearWhitelist);
        
        spdlog::info("[PapyrusInterface] Registered all Papyrus functions for STFU_MCM");
        return true;
    }
}
