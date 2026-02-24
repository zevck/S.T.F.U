#include "TopicResponseExtractor.h"
#include <RE/B/BGSSceneActionDialogue.h>
#include <spdlog/spdlog.h>
#include <unordered_set>

namespace TopicResponseExtractor
{
    // Call the game's GetResponseList function (not exposed in CommonLibSSE-NG but exists in game)
    // RELOCATION_ID(25083, 25626) - from CommonLibSSE
    using GetResponseListFunc = RE::TESTopicInfo::ResponseData*(*)(RE::TESTopicInfo*, RE::TESTopicInfo::ResponseData**);
    
    RE::TESTopicInfo::ResponseData* GetResponseList(RE::TESTopicInfo* topicInfo)
    {
        if (!topicInfo) return nullptr;
        
        // Call game function to get response list
        // This loads the responses from the plugin file into memory
        static REL::Relocation<GetResponseListFunc> func{ RELOCATION_ID(25083, 25626) };
        
        RE::TESTopicInfo::ResponseData* responseList = nullptr;
        func(topicInfo, &responseList);  // Function populates responseList via pointer
        return responseList;  // Return the populated list, not the function's return value
    }
    
    std::vector<std::string> ExtractResponsesFromTopicInfo(RE::TESTopicInfo* topicInfo)
    {
        std::vector<std::string> responses;
        
        if (!topicInfo) return responses;
        
        try {
            // Get the response data chain from the TopicInfo
            auto* responseData = GetResponseList(topicInfo);
            
            if (!responseData) {
                return responses;
            }
            
            // Walk the response chain and collect all text
            while (responseData) {
                if (responseData->responseText.c_str() && responseData->responseText.length() > 0) {
                    std::string text = responseData->responseText.c_str();
                    responses.emplace_back(text);
                }
                responseData = responseData->next;
            }
            
        } catch (const std::exception& e) {
            spdlog::warn("[ResponseExtractor] Exception extracting responses from TopicInfo 0x{:08X}: {}", 
                topicInfo->GetFormID(), e.what());
        }
        
        return responses;
    }
    
    // Internal helper: Extract all responses from a TESTopic pointer
    std::vector<std::string> ExtractAllResponsesFromTopicPtr(RE::TESTopic* topic, const std::string& identifier)
    {
        std::vector<std::string> allResponses;
        
        if (!topic) {
            spdlog::warn("[ResponseExtractor] Null topic pointer for {}", identifier);
            return allResponses;
        }
        
        // Force-load the topic data from file if TopicInfos not yet loaded
        if (topic->numTopicInfos == 0 && topic->topicInfos == nullptr) {
            auto* file = topic->GetFile(0);
            if (file) {
                spdlog::debug("[ResponseExtractor] Topic {} has 0 TopicInfos, calling Load() to force load from {}", 
                    identifier, file->GetFilename());
                topic->Load(file);
            } else {
                spdlog::warn("[ResponseExtractor] Topic {} has no file pointer, cannot force load", identifier);
            }
        }
        
        spdlog::info("[ResponseExtractor] Extracting responses for topic: {} ({} TopicInfos)", 
            identifier, topic->numTopicInfos);
        
        // Loop through all TopicInfo entries in this topic
        if (topic->topicInfos && topic->numTopicInfos > 0) {
            for (std::uint32_t i = 0; i < topic->numTopicInfos; i++) {
                auto* topicInfo = topic->topicInfos[i];
                if (!topicInfo) continue;
                
                // Extract responses from this TopicInfo
                auto responses = ExtractResponsesFromTopicInfo(topicInfo);
                
                // Add all responses to our list
                for (const auto& response : responses) {
                    if (!response.empty()) {
                        allResponses.push_back(response);
                    }
                }
            }
        }
        
        spdlog::info("[ResponseExtractor] Extracted {} total responses for topic {}", 
            allResponses.size(), identifier);
        
        return allResponses;
    }
    
    std::vector<std::string> ExtractAllResponsesForTopic(const std::string& topicEditorID)
    {
        std::vector<std::string> allResponses;
        
        if (topicEditorID.empty()) {
            return allResponses;
        }
        
        // Look up the topic by EditorID
        auto* topic = RE::TESForm::LookupByEditorID<RE::TESTopic>(topicEditorID);
        if (!topic) {
            spdlog::warn("[ResponseExtractor] Topic not found: {}", topicEditorID);
            return allResponses;
        }
        
        return ExtractAllResponsesFromTopicPtr(topic, topicEditorID);
    }
    
    std::vector<std::string> ExtractAllResponsesForTopic(uint32_t topicFormID)
    {
        std::vector<std::string> allResponses;
        
        if (topicFormID == 0) {
            return allResponses;
        }
        
        // Look up the topic by FormID
        auto* form = RE::TESForm::LookupByID(topicFormID);
        if (!form) {
            spdlog::warn("[ResponseExtractor] Form not found for FormID: 0x{:08X}", topicFormID);
            return allResponses;
        }
        
        auto* topic = form->As<RE::TESTopic>();
        if (!topic) {
            spdlog::warn("[ResponseExtractor] Form 0x{:08X} is not a TESTopic", topicFormID);
            return allResponses;
        }
        
        char identifierBuf[64];
        sprintf_s(identifierBuf, "FormID 0x%08X", topicFormID);
        std::string identifier = identifierBuf;
        return ExtractAllResponsesFromTopicPtr(topic, identifier);
    }
    
    std::vector<std::string> ExtractAllResponsesForScene(const std::string& sceneEditorID)
    {
        std::vector<std::string> allResponses;
        
        if (sceneEditorID.empty()) {
            return allResponses;
        }
        
        // Look up the scene by EditorID
        auto* scene = RE::TESForm::LookupByEditorID<RE::BGSScene>(sceneEditorID);
        if (!scene) {
            spdlog::warn("[ResponseExtractor] Scene not found: {}", sceneEditorID);
            return allResponses;
        }
        
        spdlog::info("[ResponseExtractor] Scene found: {} - has {} actions", 
            sceneEditorID, scene->actions.size());
        
        // Loop through all scene actions
        int dialogueActionCount = 0;
        for (auto* action : scene->actions) {
            if (!action) {
                spdlog::debug("[ResponseExtractor] Null action in scene {}", sceneEditorID);
                continue;
            }
            
            auto actionType = action->GetType();
            spdlog::debug("[ResponseExtractor] Scene {} - Action type: {}", 
                sceneEditorID, static_cast<int>(actionType));
            
            if (actionType != RE::BGSSceneAction::Type::kDialogue) {
                continue;
            }
            
            dialogueActionCount++;
            
            // Cast to dialogue action
            auto* dialogueAction = static_cast<RE::BGSSceneActionDialogue*>(action);
            if (!dialogueAction) {
                spdlog::warn("[ResponseExtractor] Failed to cast to dialogue action for scene {}", sceneEditorID);
                continue;
            }
            
            if (!dialogueAction->topic) {
                spdlog::debug("[ResponseExtractor] Dialogue action has no topic in scene {}", sceneEditorID);
                continue;
            }
            
            // Get the topic associated with this scene action
            auto* topic = dialogueAction->topic;
            
            // Force-load the topic data from file if TopicInfos not yet loaded
            if (topic->numTopicInfos == 0 && topic->topicInfos == nullptr) {
                auto* file = topic->GetFile(0);
                if (file) {
                    spdlog::debug("[ResponseExtractor] Topic has 0 TopicInfos, calling Load() to force load from {}", 
                        file->GetFilename());
                    topic->Load(file);
                } else {
                    spdlog::warn("[ResponseExtractor] Topic has no file pointer, cannot force load");
                }
            }
            
            spdlog::debug("[ResponseExtractor] Scene {} - Dialogue action has topic with {} TopicInfos", 
                sceneEditorID, topic->numTopicInfos);
            
            // Loop through all TopicInfo entries in this topic
            if (topic->topicInfos && topic->numTopicInfos > 0) {
                for (std::uint32_t i = 0; i < topic->numTopicInfos; i++) {
                    auto* topicInfo = topic->topicInfos[i];
                    if (!topicInfo) {
                        spdlog::debug("[ResponseExtractor] TopicInfo {} is null", i);
                        continue;
                    }
                    
                    spdlog::debug("[ResponseExtractor] Processing TopicInfo {} (FormID: 0x{:08X})", i, topicInfo->GetFormID());
                    
                    // Extract responses from this TopicInfo
                    auto responses = ExtractResponsesFromTopicInfo(topicInfo);
                    spdlog::debug("[ResponseExtractor] TopicInfo {} extracted {} responses", i, responses.size());
                    
                    // Add all responses to our list
                    for (const auto& response : responses) {
                        if (!response.empty()) {
                            allResponses.push_back(response);
                        }
                    }
                }
            } else {
                spdlog::debug("[ResponseExtractor] Topic has no TopicInfos or null array after InitItemImpl");
            }
        }
        
        spdlog::info("[ResponseExtractor] Scene {} - Found {} dialogue actions, extracted {} total responses", 
            sceneEditorID, dialogueActionCount, allResponses.size());
        
        return allResponses;
    }
    
    std::vector<DialogueTopicInfo> ExtractDialogueTopicsFromScene(const std::string& sceneEditorID)
    {
        std::vector<DialogueTopicInfo> topics;
        
        if (sceneEditorID.empty()) {
            return topics;
        }
        
        // Look up the scene by EditorID
        auto* scene = RE::TESForm::LookupByEditorID<RE::BGSScene>(sceneEditorID);
        if (!scene) {
            spdlog::warn("[TopicExtractor] Scene not found: {}", sceneEditorID);
            return topics;
        }
        
        spdlog::info("[TopicExtractor] Extracting dialogue topics from scene: {} - has {} actions", 
            sceneEditorID, scene->actions.size());
        
        // Loop through all scene actions
        int dialogueActionCount = 0;
        for (auto* action : scene->actions) {
            if (!action) continue;
            
            auto actionType = action->GetType();
            if (actionType != RE::BGSSceneAction::Type::kDialogue) {
                continue;
            }
            
            dialogueActionCount++;
            
            // Cast to dialogue action
            auto* dialogueAction = static_cast<RE::BGSSceneActionDialogue*>(action);
            if (!dialogueAction || !dialogueAction->topic) {
                continue;
            }
            
            // Get the topic associated with this scene action
            auto* topic = dialogueAction->topic;
            
            DialogueTopicInfo info;
            info.formID = topic->GetFormID();
            info.editorID = topic->GetFormEditorID() ? topic->GetFormEditorID() : "";
            
            // Get source plugin
            auto* file = topic->GetFile(0);
            if (file) {
                info.sourcePlugin = file->GetFilename();
            } else {
                info.sourcePlugin = "";
            }
            
            // Check for duplicates (some scenes may reference the same topic multiple times)
            bool isDuplicate = false;
            for (const auto& existing : topics) {
                if (existing.formID == info.formID && existing.editorID == info.editorID) {
                    isDuplicate = true;
                    break;
                }
            }
            
            if (!isDuplicate) {
                topics.push_back(info);
                spdlog::debug("[TopicExtractor] Found topic: {} (FormID: 0x{:08X}, Plugin: {})", 
                    info.editorID, info.formID, info.sourcePlugin);
            }
        }
        
        spdlog::info("[TopicExtractor] Scene {} - Found {} unique dialogue topics from {} dialogue actions", 
            sceneEditorID, topics.size(), dialogueActionCount);
        
        return topics;
    }
}
