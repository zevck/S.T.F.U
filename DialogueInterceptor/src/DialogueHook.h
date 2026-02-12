#pragma once

#include "../include/PCH.h"

namespace DialogueHook
{
    // Initialize dialogue hooks
    void Install();

    // Update polling (call each frame)
    void Update();

    // Check if a specific dialogue should be blocked
    bool ShouldBlockDialogue(const char* editorID, uint32_t formID);
}
