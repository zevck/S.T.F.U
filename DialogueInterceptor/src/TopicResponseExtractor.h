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
