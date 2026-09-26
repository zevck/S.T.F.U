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

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

// Forward declaration
namespace RE {
    class TESTopicInfo;
}

namespace PopulateTopicInfoHook
{
    // Install the PopulateTopicInfo hook (called for ALL dialogue including combat barks)
    void Install();
    
    // Called by DialogueItem::Ctor to record when dialogue is actually being constructed for playback.
    // topicFormID is the parent Topic's FormID; used to detect the follower-command
    // candidate-burst pattern (multiple TopicInfos of one Topic ctor'd within a
    // few ms so the engine can pick one).
    void RecordDialogueConstruct(uint32_t speakerFormID, uint32_t topicInfoFormID, uint32_t topicFormID);
}
