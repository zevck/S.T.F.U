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
#include <vector>
#include <string>

namespace TopicResponseExtractor
{
    // Structure to hold extracted topic information
    struct DialogueTopicInfo {
        uint32_t formID;
        std::string editorID;
        std::string sourcePlugin;
    };

    // Extract ALL response text strings from a topic (from all TopicInfo entries)
    // This reads the response data directly from the loaded forms
    std::vector<std::string> ExtractAllResponsesForTopic(const std::string& topicEditorID);
    
    // Extract ALL response text strings from a topic by FormID (for topics without EditorIDs)
    std::vector<std::string> ExtractAllResponsesForTopic(uint32_t topicFormID);
    
    // Extract ALL response text strings from a scene
    std::vector<std::string> ExtractAllResponsesForScene(const std::string& sceneEditorID);
    
    // Extract responses from a single TopicInfo
    std::vector<std::string> ExtractResponsesFromTopicInfo(RE::TESTopicInfo* topicInfo);
    
    // Extract all dialogue action topics from a scene (for soft blocking)
    std::vector<DialogueTopicInfo> ExtractDialogueTopicsFromScene(const std::string& sceneEditorID);
}
