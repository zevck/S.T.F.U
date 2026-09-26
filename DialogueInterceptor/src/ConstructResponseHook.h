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

namespace RE {
    class TESObjectREFR;
    using FormID = std::uint32_t;
}

namespace ConstructResponseHook
{
    void Install();
    void SetEarlyBlockFlag(bool shouldBlock);  // Called by PopulateTopicInfo before ConstructResponse runs
    void SetBlockingDecision(bool shouldSoftBlock, bool shouldBlockSkyrimNet, RE::FormID topicInfoFormID);  // Single evaluation point
    void ClearBlockingDecision();  // Clear when new dialogue detected
    bool GetCachedSoftBlock();  // Get cached soft block decision for duplicates
    bool IsDuplicateDialogue(RE::FormID topicInfoFormID, RE::TESObjectREFR* speaker);  // Check if duplicate within 5 seconds
}
