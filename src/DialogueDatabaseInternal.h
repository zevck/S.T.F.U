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
#include "DialogueDatabase.h"

// File-local helpers shared by the DialogueDatabase_*.cpp translation units.
// Definitions live in DialogueDatabase_Json.cpp.
namespace DialogueDB
{
    std::string EscapeJsonString(const std::string& str);
    std::string UnescapeJsonString(const std::string& str);
    std::string ResponsesToJson(const std::vector<std::string>& responses);
    std::string ActorFormIDsToJson(const std::vector<uint32_t>& formIDs);
    std::string ActorNamesToJson(const std::vector<std::string>& names);
    std::string FactionEditorIDsToJson(const std::vector<std::string>& editorIDs);
    std::vector<std::string> JsonToResponses(const std::string& json);
    bool ActorMatchesFilter(uint32_t actorFormID, const std::string& actorName, const std::vector<uint32_t>& filterFormIDs, const std::vector<std::string>& filterNames);
    std::vector<uint32_t> ParseActorFormIDsFromJson(const std::string& json);
    std::vector<std::string> ParseActorNamesFromJson(const std::string& json);
    std::vector<std::string> ParseFactionEditorIDsFromJson(const std::string& json);
    std::vector<std::string> GetActorFactionEditorIDs(RE::TESObjectREFR* actorRef);
    bool FactionMatchesFilter(RE::TESObjectREFR* actorRef, const std::vector<std::string>& factionFilter);
}
