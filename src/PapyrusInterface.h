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

#pragma once

#include "../include/PCH.h"

namespace PapyrusInterface
{
    // Import hardcoded scenes (ambient scenes, bard songs, follower commentary)
    void ImportHardcodedScenes(RE::StaticFunctionTag*);
    
    // Import from YAML files (Blacklist, Whitelist, SubtypeOverrides)
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
