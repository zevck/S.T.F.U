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

#include "../include/PCH.h"
#include "EditorID.h"
#include "DialogueDatabase.h"
#include "Config.h"
#include "PopulateTopicInfoHook.h"
#include "TopicResponseExtractor.h"
#include "SceneHook.h"
#include "DialogueLogger.h"
#include "PrismaUIMenu.h"
#include <spdlog/spdlog.h>
#include <filesystem>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <format>
#include <unordered_map>
#include "DialogueDatabaseInternal.h"
#include "FormKey.h"

namespace DialogueDB
{
    // Helper functions for JSON array serialization
    std::string EscapeJsonString(const std::string& str) {
        std::string escaped;
        escaped.reserve(str.size());
        for (char c : str) {
            switch (c) {
                case '\"': escaped += "\\\""; break;
                case '\\': escaped += "\\\\"; break;
                case '\b': escaped += "\\b"; break;
                case '\f': escaped += "\\f"; break;
                case '\n': escaped += "\\n"; break;
                case '\r': escaped += "\\r"; break;
                case '\t': escaped += "\\t"; break;
                default: escaped += c; break;
            }
        }
        return escaped;
    }
    
    std::string UnescapeJsonString(const std::string& str) {
        std::string unescaped;
        unescaped.reserve(str.size());
        for (size_t i = 0; i < str.size(); ++i) {
            if (str[i] == '\\' && i + 1 < str.size()) {
                switch (str[i + 1]) {
                    case '"': unescaped += '"'; ++i; break;
                    case '\\': unescaped += '\\'; ++i; break;
                    case 'b': unescaped += '\b'; ++i; break;
                    case 'f': unescaped += '\f'; ++i; break;
                    case 'n': unescaped += '\n'; ++i; break;
                    case 'r': unescaped += '\r'; ++i; break;
                    case 't': unescaped += '\t'; ++i; break;
                    default: unescaped += str[i]; break;
                }
            } else {
                unescaped += str[i];
            }
        }
        return unescaped;
    }
    
    std::string ResponsesToJson(const std::vector<std::string>& responses) {
        if (responses.empty()) return "[]";
        
        std::ostringstream json;
        json << "[";
        for (size_t i = 0; i < responses.size(); ++i) {
            if (i > 0) json << ",";
            json << "\"" << EscapeJsonString(responses[i]) << "\"";
        }
        json << "]";
        return json.str();
    }
    
    // Serialize actor FormIDs to JSON: [0x12345, 0x67890]
    std::string ActorFormIDsToJson(const std::vector<uint32_t>& formIDs) {
        if (formIDs.empty()) return "[]";
        
        std::ostringstream json;
        json << "[";
        for (size_t i = 0; i < formIDs.size(); ++i) {
            if (i > 0) json << ",";
            json << "\"0x" << std::hex << std::setw(8) << std::setfill('0') << formIDs[i] << "\"";
        }
        json << "]";
        return json.str();
    }
    
    // Serialize actor names to JSON: ["Lydia", "Guard"]
    std::string ActorNamesToJson(const std::vector<std::string>& names) {
        return ResponsesToJson(names);  // Reuse string array serializer
    }
    
    // Serialize faction EditorIDs to JSON: ["WhiterunGuardFaction", "_W_HealerFaction"]
    std::string FactionEditorIDsToJson(const std::vector<std::string>& editorIDs) {
        return ResponsesToJson(editorIDs);  // Reuse string array serializer
    }
    
    std::vector<std::string> JsonToResponses(const std::string& json) {
        std::vector<std::string> responses;
        
        if (json.empty() || json == "[]") {
            return responses;
        }
        
        // Simple JSON array parser
        size_t pos = json.find('[');
        if (pos == std::string::npos) {
            return responses;
        }
        
        ++pos; // Skip '['
        bool inString = false;
        std::string current;
        
        while (pos < json.size()) {
            char c = json[pos];
            
            if (!inString) {
                if (c == '"') {
                    inString = true;
                    current.clear();
                } else if (c == ']') {
                    break;
                }
            } else {
                if (c == '\\' && pos + 1 < json.size()) {
                    // Escape sequence
                    current += c;
                    current += json[pos + 1];
                    ++pos;
                } else if (c == '"') {
                    // End of string
                    responses.push_back(UnescapeJsonString(current));
                    inString = false;
                } else {
                    current += c;
                }
            }
            ++pos;
        }
        
        return responses;
    }
    
    // Helper function to check if actor matches filter
    // Empty filter vectors = doesn't match by actor (a faction filter might still match)
    // Returns true if actor should be filtered (blocked/whitelisted)
    bool ActorMatchesFilter(uint32_t actorFormID, const std::string& actorName,
                            const std::vector<uint32_t>& filterFormIDs,
                            const std::vector<std::string>& filterNames,
                            const std::vector<std::string>& filterFormKeys)
    {
        if (filterFormIDs.empty() && filterNames.empty()) return false;

        std::string actorFormKey;
        bool actorFormKeyResolved = false;
        for (size_t i = 0; i < filterNames.size() && i < filterFormIDs.size(); ++i) {
            if (i < filterFormKeys.size() && !filterFormKeys[i].empty()) {
                if (!actorFormKeyResolved) {
                    actorFormKey = FormKey::OfFormID(actorFormID);
                    actorFormKeyResolved = true;
                }
                if (!actorFormKey.empty() && actorFormKey == filterFormKeys[i]) {
                    return true;
                }
                continue;
            }
            // Filters saved before 1.2.0 have no FormKey: match by name AND the last 3 hex
            // digits of the FormID, the only part of it that survives a load-order change
            if (actorName == filterNames[i] &&
                (actorFormID & 0xFFF) == (filterFormIDs[i] & 0xFFF)) {
                return true;
            }
        }

        return false;
    }

    bool EntryMatchesTarget(const BlacklistEntry& entry, uint32_t formID, const std::string& editorID)
    {
        if (!entry.targetFormKey.empty() && entry.targetFormKey == FormKey::OfFormID(formID)) {
            return true;
        }
        if (!entry.targetEditorID.empty()) {
            return entry.targetEditorID == editorID;
        }
        // Rows saved before 1.2.0 with no EditorID only have the FormID from when they were saved
        return entry.targetFormKey.empty() && entry.targetFormID != 0 && entry.targetFormID == formID;
    }

    bool EntryMatchesActor(const BlacklistEntry& entry, uint32_t actorFormID)
    {
        if (!entry.targetFormKey.empty()) {
            return entry.targetFormKey == FormKey::OfFormID(actorFormID);
        }
        return entry.targetFormID != 0 && entry.targetFormID == actorFormID;
    }

    bool EntryActorFilterMatches(const BlacklistEntry& entry, uint32_t actorFormID, const std::string& actorName)
    {
        return ActorMatchesFilter(actorFormID, actorName, entry.actorFilterFormIDs, entry.actorFilterNames, entry.actorFilterFormKeys);
    }

    namespace
    {
        bool IsTargetForm(const RE::TESForm* form, BlacklistTarget type)
        {
            switch (type) {
            case BlacklistTarget::Topic:   return form->Is(RE::FormType::Dialogue);
            case BlacklistTarget::Scene:   return form->Is(RE::FormType::Scene);
            case BlacklistTarget::Quest:   return form->Is(RE::FormType::Quest);
            case BlacklistTarget::Faction: return form->Is(RE::FormType::Faction);
            default:                       return false;
            }
        }

        // The loaded actor a FormID refers to, if its name is the one the entry recorded
        const RE::TESForm* LookupNamedActor(uint32_t formID, const std::string& name)
        {
            auto* form = formID ? RE::TESForm::LookupByID(formID) : nullptr;
            auto* actor = form ? form->As<RE::Actor>() : nullptr;
            const char* actorName = actor ? actor->GetName() : nullptr;
            return actorName && name == actorName ? form : nullptr;
        }
    }

    void ResolveFormKeys(BlacklistEntry& entry)
    {
        // A FormID is only turned into a FormKey once something confirms it still means the
        // same record: an EditorID lookup, the actor's name, or (for a new entry) the fact that
        // it came from this session. A saved row's FormID can be from an older load order, and
        // guessing would attach the rule to an unrelated record. Unconfirmed rows stay keyless
        // and keep matching the pre-1.2.0 way.
        if (entry.targetFormKey.empty()) {
            const RE::TESForm* form = nullptr;
            switch (entry.targetType) {
            case BlacklistTarget::Actor:
                form = LookupNamedActor(entry.targetFormID, entry.targetEditorID);  // targetEditorID holds the name
                break;
            case BlacklistTarget::Topic:
            case BlacklistTarget::Scene:
            case BlacklistTarget::Quest:
            case BlacklistTarget::Faction:
                if (!entry.targetEditorID.empty()) {
                    auto* candidate = RE::TESForm::LookupByEditorID(entry.targetEditorID);
                    form = candidate && IsTargetForm(candidate, entry.targetType) ? candidate : nullptr;
                }
                if (!form && entry.targetFormID != 0) {
                    auto* candidate = RE::TESForm::LookupByID(entry.targetFormID);
                    auto* file = candidate ? candidate->GetFile(0) : nullptr;
                    const bool sameRecord = entry.id == 0 ||
                        (!entry.sourcePlugin.empty() && file && entry.sourcePlugin == file->GetFilename());
                    form = candidate && IsTargetForm(candidate, entry.targetType) && sameRecord ? candidate : nullptr;
                }
                break;
            default:
                break;  // Subtype and Plugin rows don't name a record
            }
            if (form) {
                entry.targetFormKey = FormKey::Of(form);
                entry.targetFormID = form->GetFormID();
            }
        }

        // Keys the entry already had stay attached to their actors, even ones not loaded now.
        // actorFilterFormKeys still holds the saved keys here: the menu handlers only replace
        // the FormID and name arrays.
        std::unordered_map<uint32_t, std::string> knownKeys;
        for (const auto& key : entry.actorFilterFormKeys) {
            if (const auto formID = FormKey::ToFormID(key)) {
                knownKeys[formID] = key;
            }
        }

        std::vector<std::string> keys;
        const size_t count = (std::min)(entry.actorFilterFormIDs.size(), entry.actorFilterNames.size());
        for (size_t i = 0; i < count; ++i) {
            const uint32_t formID = entry.actorFilterFormIDs[i];
            if (const auto known = knownKeys.find(formID); known != knownKeys.end()) {
                keys.push_back(known->second);
            } else if (const auto* actor = LookupNamedActor(formID, entry.actorFilterNames[i])) {
                keys.push_back(FormKey::Of(actor));
            } else {
                keys.emplace_back();
            }
        }
        entry.actorFilterFormKeys = std::move(keys);
    }

    int64_t FindExistingEntry(sqlite3* db, const char* table, const BlacklistEntry& entry, int* existingBlockType)
    {
        // Same record: FormKey, else EditorID (except Actor rows, where it holds a name), else
        // a keyless row's FormID (Actor rows and rows without an EditorID)
        const std::string sql = std::format(
            "SELECT id, block_type FROM {} WHERE target_type = ?1 AND ("
            "(target_formkey <> '' AND target_formkey = ?2)"
            " OR (target_type <> 6 AND target_editorid <> '' AND target_editorid = ?3)"
            " OR (target_formkey = '' AND target_formid <> 0 AND target_formid = ?4 AND (target_type = 6 OR target_editorid = ''))"
            ") LIMIT 1;", table);

        // Pre-1.2.0 topic rows without an EditorID: quest + plugin + last 3 hex digits of the FormID
        const std::string eslSql = std::format(
            "SELECT id, block_type FROM {} WHERE target_type = ?1 AND quest_editorid = ?2 AND source_plugin = ?3"
            " AND (target_formid & 4095) = ?4 LIMIT 1;", table);

        auto query = [db](const std::string& text, auto&& bind, int* blockType) -> int64_t {
            sqlite3_stmt* stmt = nullptr;
            if (sqlite3_prepare_v2(db, text.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
                spdlog::error("[DialogueDB] Failed to prepare existing-entry lookup: {}", sqlite3_errmsg(db));
                return -1;
            }
            bind(stmt);
            int64_t id = -1;
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                id = sqlite3_column_int64(stmt, 0);
                if (blockType) {
                    *blockType = sqlite3_column_int(stmt, 1);
                }
            }
            sqlite3_finalize(stmt);
            return id;
        };

        int64_t id = query(sql, [&entry](sqlite3_stmt* stmt) {
            sqlite3_bind_int(stmt, 1, static_cast<int>(entry.targetType));
            sqlite3_bind_text(stmt, 2, entry.targetFormKey.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 3, entry.targetEditorID.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(stmt, 4, entry.targetFormID);
        }, existingBlockType);

        if (id == -1 && entry.targetType == BlacklistTarget::Topic && !entry.questEditorID.empty() &&
            !entry.sourcePlugin.empty() && entry.targetFormID != 0) {
            id = query(eslSql, [&entry](sqlite3_stmt* stmt) {
                sqlite3_bind_int(stmt, 1, static_cast<int>(entry.targetType));
                sqlite3_bind_text(stmt, 2, entry.questEditorID.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(stmt, 3, entry.sourcePlugin.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_int(stmt, 4, entry.targetFormID & 0xFFF);
            }, existingBlockType);
        }
        return id;
    }

    BlacklistEntry ReadListEntry(sqlite3_stmt* stmt, const char* defaultCategory)
    {
        // Columns are read by name: tables upgraded by UpdateSchema() can have them in a
        // different order than a freshly created table
        auto column = [stmt](const char* name) -> int {
            for (int i = 0; i < sqlite3_column_count(stmt); ++i) {
                if (std::strcmp(sqlite3_column_name(stmt, i), name) == 0) {
                    return i;
                }
            }
            return -1;
        };
        auto text = [stmt, &column](const char* name, const char* fallback = "") -> std::string {
            const int i = column(name);
            const auto* value = i >= 0 ? reinterpret_cast<const char*>(sqlite3_column_text(stmt, i)) : nullptr;
            return value ? value : fallback;
        };
        auto integer = [stmt, &column](const char* name) -> int64_t {
            const int i = column(name);
            return i >= 0 ? sqlite3_column_int64(stmt, i) : 0;
        };

        BlacklistEntry entry;
        entry.id = integer("id");
        entry.targetType = static_cast<BlacklistTarget>(integer("target_type"));
        entry.targetFormID = static_cast<uint32_t>(integer("target_formid"));
        entry.targetEditorID = text("target_editorid");
        entry.targetFormKey = text("target_formkey");
        entry.blockType = static_cast<BlockType>(integer("block_type"));
        entry.addedTimestamp = integer("added_timestamp");
        entry.notes = text("notes");
        entry.responseText = text("response_text");
        entry.subtype = static_cast<uint16_t>(integer("subtype"));
        entry.subtypeName = text("subtype_name");
        entry.filterCategory = text("filter_category", defaultCategory);
        if (entry.filterCategory.empty()) {
            entry.filterCategory = defaultCategory;
        }
        entry.blockSkyrimNet = integer("block_skyrimnet") != 0;
        entry.sourcePlugin = text("source_plugin");
        entry.questEditorID = text("quest_editorid");
        entry.actorFilterFormIDs = ParseActorFormIDsFromJson(text("actor_filter_formids", "[]"));
        entry.actorFilterNames = ParseActorNamesFromJson(text("actor_filter_names", "[]"));
        entry.actorFilterFormKeys = JsonToResponses(text("actor_filter_formkeys", "[]"));
        entry.factionFilterEditorIDs = ParseFactionEditorIDsFromJson(text("faction_filter_editorids", "[]"));

        // Not stored: SkyrimNet-only blocks never block audio/subtitles, Hard and Soft always do
        entry.blockAudio = entry.blockSubtitles = entry.blockType != BlockType::SkyrimNet;
        return entry;
    }
    
    // Parse JSON array of actor FormIDs/Names from database TEXT column
    std::vector<uint32_t> ParseActorFormIDsFromJson(const std::string& json) {
        std::vector<uint32_t> formIDs;
        if (json.empty() || json == "[]") return formIDs;
        
        // Simple JSON array parser: ["0x12345", "0x67890"]
        // We expect hex FormIDs as strings
        size_t pos = 1;  // Skip leading '['
        std::string current;
        bool inString = false;
        
        while (pos < json.size()) {
            char c = json[pos];
            if (!inString) {
                if (c == '"') {
                    inString = true;
                    current.clear();
                } else if (c == ']') {
                    break;
                }
            } else {
                if (c == '"') {
                    // End of string - parse as hex FormID
                    if (!current.empty()) {
                        try {
                            uint32_t formID = std::stoul(current, nullptr, 0);  // auto-detect base (0x prefix)
                            formIDs.push_back(formID);
                        } catch (...) {
                            spdlog::warn("[DialogueDB] Failed to parse actor FormID: {}", current);
                        }
                    }
                    inString = false;
                } else {
                    current += c;
                }
            }
            ++pos;
        }
        
        return formIDs;
    }
    
    std::vector<std::string> ParseActorNamesFromJson(const std::string& json) {
        // Reuse existing JsonToResponses parser - it handles string arrays
        return JsonToResponses(json);
    }
    
    // Parse JSON array of faction EditorIDs from database TEXT column
    std::vector<std::string> ParseFactionEditorIDsFromJson(const std::string& json) {
        // Reuse existing JsonToResponses parser - it handles string arrays
        return JsonToResponses(json);
    }
    
    // Get faction EditorIDs from an actor reference
    std::vector<std::string> GetActorFactionEditorIDs(RE::TESObjectREFR* actorRef) {
        std::vector<std::string> factionEditorIDs;
        
        if (!actorRef) return factionEditorIDs;
        
        // Cast to Actor
        RE::Actor* actor = actorRef->As<RE::Actor>();
        if (!actor) return factionEditorIDs;
        
        // Get actor base
        RE::TESNPC* actorBase = actor->GetActorBase();
        if (!actorBase) return factionEditorIDs;
        
        // Iterate through factions
        for (auto& factionInfo : actorBase->factions) {
            if (factionInfo.faction) {
                const char* editorID = STFU::GetEditorID(factionInfo.faction);
                if (editorID && editorID[0]) {
                    factionEditorIDs.push_back(editorID);
                }
            }
        }
        
        return factionEditorIDs;
    }
    
    // Check if actor's factions match any in the filter
    bool FactionMatchesFilter(RE::TESObjectREFR* actorRef, const std::vector<std::string>& factionFilter) {
        if (factionFilter.empty()) return false;  // No faction filter
        if (!actorRef) return false;
        
        std::vector<std::string> actorFactions = GetActorFactionEditorIDs(actorRef);
        
        // Check if any of the actor's factions are in the filter
        for (const auto& actorFaction : actorFactions) {
            for (const auto& filterFaction : factionFilter) {
                if (actorFaction == filterFaction) {
                    spdlog::debug("[DialogueDB] Faction match found: actor has faction '{}'", actorFaction);
                    return true;
                }
            }
        }
        
        return false;
    }
}
