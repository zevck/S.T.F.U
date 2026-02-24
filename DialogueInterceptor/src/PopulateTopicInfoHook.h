#pragma once

#include <cstdint>

namespace PopulateTopicInfoHook
{
    // Install the PopulateTopicInfo hook (called for ALL dialogue including combat barks)
    void Install();
    
    // Called by DialogueItem::Ctor to record when dialogue is actually being constructed for playback
    void RecordDialogueConstruct(uint32_t speakerFormID, uint32_t topicInfoFormID);
    
    // Called by ConstructResponse hook to record topics that can be executed
    void RecordConstructResponse(uint32_t topicInfoFormID);
    
    // Check if a topic is in the menu cache (for database read fallback)
    bool IsTopicInMenuCache(uint32_t topicInfoFormID);
}
