#pragma once

#include <string>
#include <cstdint>

namespace SceneHook
{
    // Initialize the scene blocker (called at kDataLoaded)
    void Install();
    
    // Patch blocked scenes by adding blocking conditions to phases
    // Called at kDataLoaded. Safe to call multiple times.
    void PatchScenes();
    
    // Patch only scenes that were deferred because they were playing when the block was added.
    // Called at kPostLoadGame - load screens stop all scenes so it is safe to patch them now.
    void PatchDeferredScenes();
    
    // Update scene conditions at runtime when blacklist changes
    // blockType: 1=Soft (no scene condition), 2=Hard (add scene condition), 3=SkyrimNet (no scene condition)
    void UpdateSceneConditions(const std::string& sceneEditorID, uint8_t blockType);
    void UpdateSceneConditionsForTopic(const std::string& topicEditorID, uint8_t blockType);
}
