#pragma once

#include <string>

namespace RE {
    class TESObjectREFR;
    using FormID = std::uint32_t;
}

namespace ConstructResponseHook
{
    void Install();
    void SetShouldBlockSubtitles(bool shouldBlock);
    bool IsDuplicateDialogue(RE::FormID topicInfoFormID, RE::TESObjectREFR* speaker);  // Check if duplicate within 5 seconds
}
