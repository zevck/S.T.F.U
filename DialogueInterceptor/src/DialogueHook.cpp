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
