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
    // Called when the Loading Menu opens (LoadingMenuSink) - the engine has stopped all scenes by then.
    void PatchDeferredScenes();
    
    // Update scene conditions at runtime when blacklist changes
    // blockType: 1=Soft (no scene condition), 2=Hard (add scene condition), 3=SkyrimNet (no scene condition)
    void UpdateSceneConditions(const std::string& sceneEditorID, uint8_t blockType);
    void UpdateSceneConditionsForTopic(const std::string& topicEditorID, uint8_t blockType);
}
