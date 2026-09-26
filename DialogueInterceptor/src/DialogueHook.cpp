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

#include "DialogueHook.h"
#include "Config.h"

namespace DialogueHook
{
    // No hooks needed - just log from event handler
    
    // ========================================
    // Helper Functions
    // ========================================
    
    bool ShouldBlockDialogue(const char* editorID, uint32_t formID)
    {
        // Phase 2: Log everything, block nothing (yet)
        if (editorID && editorID[0]) {
            spdlog::info("[DIALOGUE CHECK] EditorID: {} | FormID: 0x{:08X}", editorID, formID);
        } else {
            spdlog::info("[DIALOGUE CHECK] FormID: 0x{:08X} (no EditorID)", formID);
        }
        
        // Phase 3 will check STFU configuration here
        return false;
    }

    // ========================================
    // Installation
    // ========================================
    
    void Install()
    {
        spdlog::info("========================================");
        spdlog::info("No hooks installed - hooks don't fire for NPCs");
        spdlog::info("Will rely on TopicInfo events in SkyrimNet instead");
        spdlog::info("========================================");
    }
    
    // Update function no longer needed with real hooks
    void Update()
    {
        // Empty - hooks will fire automatically
    }
}
