#pragma once

#include <string>
#include <cstdint>

namespace SceneHook
{
    // Initialize the scene blocker (called at kDataLoaded)
    void Install();
    
    // Patch blocked scenes by adding blocking conditions to phases
    // Called at kDataLoaded, kPostLoadGame, and interior cell loads
    // Safe to call multiple times - removes old conditions before adding new ones
    void PatchScenes();
    
    // Update scene conditions at runtime when blacklist changes
    // blockType: 1=Soft (no scene condition), 2=Hard (add scene condition), 3=SkyrimNet (no scene condition)
    void UpdateSceneConditions(const std::string& sceneEditorID, uint8_t blockType);
    void UpdateSceneConditionsForTopic(const std::string& topicEditorID, uint8_t blockType);
}
