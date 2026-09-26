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
#include "DialogueDatabaseInternal.h"
#include "FormKey.h"

namespace DialogueDB
{
    // ============================================================================
    // Whitelist Management Functions (parallel to blacklist)
    // ============================================================================

    bool Database::AddToWhitelist(const BlacklistEntry& entry, bool skipEnrichment)
    {
        std::lock_guard<std::recursive_mutex> lock(dbMutex_);

        if (!db_) return false;

        // Make a mutable copy for enrichment
        BlacklistEntry enrichedEntry = entry;
        enrichedEntry.filterCategory = "Whitelist";  // Force whitelist category
        
        // Enrich with extracted response text (unless skipped for performance)
        if (!skipEnrichment) {
            if (enrichedEntry.targetType == BlacklistTarget::Topic && !enrichedEntry.targetEditorID.empty()) {
                spdlog::debug("[DialogueDB] Extracting responses for whitelisted topic: {}", enrichedEntry.targetEditorID);
                auto responses = TopicResponseExtractor::ExtractAllResponsesForTopic(enrichedEntry.targetEditorID);
                
                if (!responses.empty()) {
                    enrichedEntry.responseText = ResponsesToJson(responses);
                }
            } else if (enrichedEntry.targetType == BlacklistTarget::Scene && !enrichedEntry.targetEditorID.empty()) {
                spdlog::debug("[DialogueDB] Extracting responses for whitelisted scene: {}", enrichedEntry.targetEditorID);
                auto responses = TopicResponseExtractor::ExtractAllResponsesForScene(enrichedEntry.targetEditorID);
                
                if (!responses.empty()) {
                    enrichedEntry.responseText = ResponsesToJson(responses);
                }
            }
        }

        ResolveFormKeys(enrichedEntry);

        const int64_t existingId = FindExistingEntry(db_, "whitelist", enrichedEntry);

        if (existingId > 0) {
            // Update existing entry
            spdlog::info("[DialogueDB] Updating existing whitelist entry (id={}): {} (FormID: 0x{:08X})", 
                existingId, enrichedEntry.targetEditorID, enrichedEntry.targetFormID);
            
            const char* updateSql = R"(
                UPDATE whitelist 
                SET block_type = ?, added_timestamp = ?, notes = ?, response_text = ?, subtype = ?, subtype_name = ?,
                    filter_category = ?, block_skyrimnet = ?, source_plugin = ?, quest_editorid = ?,
                    actor_filter_formids = ?, actor_filter_names = ?, faction_filter_editorids = ?,
                    target_formkey = ?, actor_filter_formkeys = ?
                WHERE id = ?;
            )";
            sqlite3_stmt* updateStmt = nullptr;
            
            if (sqlite3_prepare_v2(db_, updateSql, -1, &updateStmt, nullptr) != SQLITE_OK) {
                spdlog::error("[DialogueDB] Failed to prepare whitelist update statement: {}", sqlite3_errmsg(db_));
                return false;
            }
            
            // Serialize actor filters to JSON
            std::string actorFormIDsJson = ActorFormIDsToJson(enrichedEntry.actorFilterFormIDs);
            std::string actorNamesJson = ActorNamesToJson(enrichedEntry.actorFilterNames);
            std::string factionEditorIDsJson = FactionEditorIDsToJson(enrichedEntry.factionFilterEditorIDs);
            
            sqlite3_bind_int(updateStmt, 1, static_cast<int>(enrichedEntry.blockType));
            sqlite3_bind_int64(updateStmt, 2, enrichedEntry.addedTimestamp);
            sqlite3_bind_text(updateStmt, 3, enrichedEntry.notes.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(updateStmt, 4, enrichedEntry.responseText.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(updateStmt, 5, enrichedEntry.subtype);
            sqlite3_bind_text(updateStmt, 6, enrichedEntry.subtypeName.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(updateStmt, 7, enrichedEntry.filterCategory.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(updateStmt, 8, enrichedEntry.blockSkyrimNet ? 1 : 0);
            sqlite3_bind_text(updateStmt, 9, enrichedEntry.sourcePlugin.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(updateStmt, 10, enrichedEntry.questEditorID.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(updateStmt, 11, actorFormIDsJson.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(updateStmt, 12, actorNamesJson.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(updateStmt, 13, factionEditorIDsJson.c_str(), -1, SQLITE_TRANSIENT);
            std::string actorFormKeysJson = ResponsesToJson(enrichedEntry.actorFilterFormKeys);
            sqlite3_bind_text(updateStmt, 14, enrichedEntry.targetFormKey.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(updateStmt, 15, actorFormKeysJson.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int64(updateStmt, 16, existingId);
            
            bool success = sqlite3_step(updateStmt) == SQLITE_DONE;
            if (!success) {
                spdlog::error("[DialogueDB] Failed to update whitelist entry: {}", sqlite3_errmsg(db_));
            }
            sqlite3_finalize(updateStmt);
            
            return success;
        } else {
            // Insert new entry
            spdlog::info("[DialogueDB] Inserting new whitelist entry: {} (FormID: 0x{:08X}, Type: {})", 
                enrichedEntry.targetEditorID, enrichedEntry.targetFormID, static_cast<int>(enrichedEntry.targetType));
            
            if (!insertWhitelistStmt_) {
                spdlog::error("[DialogueDB] insertWhitelistStmt_ is null!");
                return false;
            }
            
            sqlite3_reset(insertWhitelistStmt_);
            
            // Serialize actor and faction filters to JSON
            std::string actorFormIDsJson = ActorFormIDsToJson(enrichedEntry.actorFilterFormIDs);
            std::string actorNamesJson = ActorNamesToJson(enrichedEntry.actorFilterNames);
            std::string factionEditorIDsJson = FactionEditorIDsToJson(enrichedEntry.factionFilterEditorIDs);
            
            sqlite3_bind_int(insertWhitelistStmt_, 1, static_cast<int>(enrichedEntry.targetType));
            sqlite3_bind_int(insertWhitelistStmt_, 2, enrichedEntry.targetFormID);
            sqlite3_bind_text(insertWhitelistStmt_, 3, enrichedEntry.targetEditorID.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(insertWhitelistStmt_, 4, static_cast<int>(enrichedEntry.blockType));
            sqlite3_bind_int64(insertWhitelistStmt_, 5, enrichedEntry.addedTimestamp);
            sqlite3_bind_text(insertWhitelistStmt_, 6, enrichedEntry.notes.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(insertWhitelistStmt_, 7, enrichedEntry.responseText.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(insertWhitelistStmt_, 8, enrichedEntry.subtype);
            sqlite3_bind_text(insertWhitelistStmt_, 9, enrichedEntry.subtypeName.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(insertWhitelistStmt_, 10, enrichedEntry.filterCategory.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(insertWhitelistStmt_, 11, enrichedEntry.blockSkyrimNet ? 1 : 0);
            sqlite3_bind_text(insertWhitelistStmt_, 12, enrichedEntry.sourcePlugin.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(insertWhitelistStmt_, 13, enrichedEntry.questEditorID.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(insertWhitelistStmt_, 14, actorFormIDsJson.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(insertWhitelistStmt_, 15, actorNamesJson.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(insertWhitelistStmt_, 16, factionEditorIDsJson.c_str(), -1, SQLITE_TRANSIENT);
            std::string actorFormKeysJson = ResponsesToJson(enrichedEntry.actorFilterFormKeys);
            sqlite3_bind_text(insertWhitelistStmt_, 17, enrichedEntry.targetFormKey.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(insertWhitelistStmt_, 18, actorFormKeysJson.c_str(), -1, SQLITE_TRANSIENT);

            if (sqlite3_step(insertWhitelistStmt_) != SQLITE_DONE) {
                spdlog::error("[DialogueDB] Failed to add whitelist entry: {}", sqlite3_errmsg(db_));
                return false;
            }

            spdlog::info("[DialogueDB] Successfully inserted whitelist entry (id={})", sqlite3_last_insert_rowid(db_));
            return true;
        }
    }

    bool Database::RemoveFromWhitelist(int64_t id)
    {
        std::lock_guard<std::recursive_mutex> lock(dbMutex_);

        if (!db_) return false;

        const char* sql = "DELETE FROM whitelist WHERE id = ?;";
        sqlite3_stmt* stmt = nullptr;

        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            return false;
        }

        sqlite3_bind_int64(stmt, 1, id);
        bool success = sqlite3_step(stmt) == SQLITE_DONE;
        sqlite3_finalize(stmt);

        return success;
    }

    int Database::ClearWhitelist()
    {
        std::lock_guard<std::recursive_mutex> lock(dbMutex_);

        if (!db_) return 0;

        // First, get count of entries to be deleted
        int count = 0;
        const char* countSql = "SELECT COUNT(*) FROM whitelist;";
        sqlite3_stmt* countStmt = nullptr;
        
        if (sqlite3_prepare_v2(db_, countSql, -1, &countStmt, nullptr) == SQLITE_OK) {
            if (sqlite3_step(countStmt) == SQLITE_ROW) {
                count = sqlite3_column_int(countStmt, 0);
            }
            sqlite3_finalize(countStmt);
        }

        if (count == 0) {
            spdlog::info("[DialogueDB] No whitelist entries to clear");
            return 0;
        }

        // Delete all entries
        const char* sql = "DELETE FROM whitelist;";
        char* errMsg = nullptr;
        int rc = sqlite3_exec(db_, sql, nullptr, nullptr, &errMsg);
        
        if (rc != SQLITE_OK) {
            spdlog::error("[DialogueDB] Failed to clear whitelist: {}", errMsg ? errMsg : "unknown error");
            if (errMsg) sqlite3_free(errMsg);
            return 0;
        }

        spdlog::info("[DialogueDB] Cleared {} whitelist entries", count);
        return count;
    }

    std::vector<BlacklistEntry> Database::GetWhitelist()
    {
        std::lock_guard<std::recursive_mutex> lock(dbMutex_);
        std::vector<BlacklistEntry> results;

        if (!db_) return results;

        const char* sql = "SELECT * FROM whitelist ORDER BY added_timestamp ASC;";
        sqlite3_stmt* stmt = nullptr;

        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            return results;
        }

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            results.push_back(ReadListEntry(stmt, "Whitelist"));
        }

        sqlite3_finalize(stmt);
        return results;
    }

    bool Database::IsWhitelisted(BlacklistTarget targetType, uint32_t formID, const std::string& editorID, uint32_t actorFormID, const std::string& actorName, RE::TESObjectREFR* actorRef)
    {
        std::lock_guard<std::recursive_mutex> lock(dbMutex_);
        if (!db_) return false;

        const std::string sql = std::string("SELECT actor_filter_formids, actor_filter_names, faction_filter_editorids, actor_filter_formkeys"
            " FROM whitelist WHERE target_type = ? AND ") + kTargetMatchSql + " LIMIT 1;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return false;

        const std::string formKey = FormKey::OfFormID(formID);
        sqlite3_bind_int(stmt, 1, static_cast<int>(targetType));
        sqlite3_bind_text(stmt, 2, formKey.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, editorID.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 4, formID);

        bool isWhitelisted = false;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const char* actorFormIDsJson = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            const char* actorNamesJson   = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            const char* factionJson      = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            const char* actorFormKeysJson = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));

            std::vector<uint32_t>    filterFormIDs = ParseActorFormIDsFromJson(actorFormIDsJson ? actorFormIDsJson : "[]");
            std::vector<std::string> filterNames   = ParseActorNamesFromJson(actorNamesJson ? actorNamesJson : "[]");
            std::vector<std::string> factionFilter = ParseFactionEditorIDsFromJson(factionJson ? factionJson : "[]");
            std::vector<std::string> filterFormKeys = JsonToResponses(actorFormKeysJson ? actorFormKeysJson : "[]");

            bool hasActorFilter   = !filterFormIDs.empty() || !filterNames.empty();
            bool hasFactionFilter = !factionFilter.empty();

            if (!hasActorFilter && !hasFactionFilter) {
                // No filters - whitelist applies to everyone
                isWhitelisted = true;
                spdlog::debug("[DialogueDB] IsWhitelisted(+ref): No filter -> WHITELISTED for all");
            } else if ((actorFormID > 0 || !actorName.empty()) || actorRef) {
                bool actorMatches   = hasActorFilter   && ActorMatchesFilter(actorFormID, actorName, filterFormIDs, filterNames, filterFormKeys);
                bool factionMatches = hasFactionFilter && FactionMatchesFilter(actorRef, factionFilter);
                if (actorMatches || factionMatches) {
                    isWhitelisted = true;
                    spdlog::debug("[DialogueDB] IsWhitelisted(+ref): Actor '{}' (0x{:08X}) or faction matches -> WHITELISTED", actorName, actorFormID);
                } else {
                    isWhitelisted = false;
                    spdlog::debug("[DialogueDB] IsWhitelisted(+ref): Actor '{}' (0x{:08X}) not in filter -> NOT WHITELISTED", actorName, actorFormID);
                }
            } else {
                // Filter exists but no actor info provided
                isWhitelisted = false;
                spdlog::debug("[DialogueDB] IsWhitelisted(+ref): Filter exists but no actor info -> NOT WHITELISTED");
            }
        }

        sqlite3_finalize(stmt);
        return isWhitelisted;
    }

}
