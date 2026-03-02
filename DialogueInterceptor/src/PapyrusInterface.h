#pragma once

#include "../include/PCH.h"

namespace PapyrusInterface
{
    // Import hardcoded scenes (ambient scenes, bard songs, follower commentary)
    void ImportHardcodedScenes(RE::StaticFunctionTag*);
    
    // Import from YAML files (Blacklist, Whitelist, SubtypeOverrides, SkyrimNetFilter)
    // Returns total number of entries imported
    int32_t ImportFromYAML(RE::StaticFunctionTag*);
    
    // Get current menu hotkey scancode
    int32_t GetMenuHotkey(RE::StaticFunctionTag*);
    
    // Set menu hotkey scancode
    void SetMenuHotkey(RE::StaticFunctionTag*, int32_t scancode);
    
    // Save settings to INI file
    void SaveSettings(RE::StaticFunctionTag*);
    
    // Clear all blacklist entries (returns count removed)
    int32_t ClearBlacklist(RE::StaticFunctionTag*);
    
    // Clear all whitelist entries (returns count removed)
    int32_t ClearWhitelist(RE::StaticFunctionTag*);
    
    // Register all Papyrus native functions
    bool RegisterFunctions(RE::BSScript::IVirtualMachine* vm);
}
