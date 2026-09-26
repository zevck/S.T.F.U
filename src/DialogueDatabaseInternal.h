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
    bool ActorMatchesFilter(uint32_t actorFormID, const std::string& actorName, const std::vector<uint32_t>& filterFormIDs,
                            const std::vector<std::string>& filterNames, const std::vector<std::string>& filterFormKeys);
    std::vector<uint32_t> ParseActorFormIDsFromJson(const std::string& json);
    std::vector<std::string> ParseActorNamesFromJson(const std::string& json);
    std::vector<std::string> ParseFactionEditorIDsFromJson(const std::string& json);
    std::vector<std::string> GetActorFactionEditorIDs(RE::TESObjectREFR* actorRef);
    bool FactionMatchesFilter(RE::TESObjectREFR* actorRef, const std::vector<std::string>& factionFilter);

    // Fills in the FormKeys of an entry about to be saved (see the comment in the definition)
    void ResolveFormKeys(BlacklistEntry& entry);

    // Id of the saved row for the same record as entry in table ("blacklist"/"whitelist"), or -1.
    // Also returns that row's block_type through existingBlockType when given.
    int64_t FindExistingEntry(sqlite3* db, const char* table, const BlacklistEntry& entry, int* existingBlockType = nullptr);

    // Reads one blacklist/whitelist row from a "SELECT *" statement
    BlacklistEntry ReadListEntry(sqlite3_stmt* stmt, const char* defaultCategory);

    // WHERE fragment matching a topic/scene/quest/subtype/plugin row. Binds, in order: FormKey,
    // EditorID, FormID. Same rule as EntryMatchesTarget.
    inline constexpr const char* kTargetMatchSql =
        "(target_type NOT IN (6, 7) AND ((target_formkey <> '' AND target_formkey = ?)"
        " OR (target_editorid <> '' AND target_editorid = ?)"
        " OR (target_formkey = '' AND target_editorid = '' AND target_formid <> 0 AND target_formid = ?)))";

    // WHERE fragment matching an Actor row. Binds, in order: FormKey, FormID. Same rule as
    // EntryMatchesActor.
    inline constexpr const char* kActorMatchSql =
        "(target_type = 6 AND ((target_formkey <> '' AND target_formkey = ?)"
        " OR (target_formkey = '' AND target_formid <> 0 AND target_formid = ?)))";
}
