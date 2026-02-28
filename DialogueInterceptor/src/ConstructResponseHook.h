#pragma once

#include <string>

namespace RE {
    class TESObjectREFR;
    using FormID = std::uint32_t;
}

namespace ConstructResponseHook
{
    void Install();
    void SetEarlyBlockFlag(bool shouldBlock);  // Called by PopulateTopicInfo before ConstructResponse runs
    void SetBlockingDecision(bool shouldSoftBlock, bool shouldBlockSkyrimNet);  // Single evaluation point
    void ClearBlockingDecision();  // Clear when new dialogue detected
    bool GetCachedSoftBlock();  // Get cached soft block decision for duplicates
    bool IsDuplicateDialogue(RE::FormID topicInfoFormID, RE::TESObjectREFR* speaker);  // Check if duplicate within 5 seconds
}
