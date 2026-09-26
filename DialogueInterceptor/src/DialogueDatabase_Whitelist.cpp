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

        // Check if entry already exists using ESL-safe matching (prioritize EditorID)
        const char* checkSql = "SELECT id FROM whitelist WHERE target_type = ? AND (((target_formid = ? OR target_formid = 0) AND target_editorid = ?) OR (target_editorid = '' AND target_formid = ?)) LIMIT 1;";
        sqlite3_stmt* checkStmt = nullptr;
        
        if (sqlite3_prepare_v2(db_, checkSql, -1, &checkStmt, nullptr) != SQLITE_OK) {
            spdlog::error("[DialogueDB] Failed to prepare whitelist check statement: {}", sqlite3_errmsg(db_));
            return false;
        }
        
        sqlite3_bind_int(checkStmt, 1, static_cast<int>(enrichedEntry.targetType));
        sqlite3_bind_int(checkStmt, 2, enrichedEntry.targetFormID);
        sqlite3_bind_text(checkStmt, 3, enrichedEntry.targetEditorID.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(checkStmt, 4, enrichedEntry.targetFormID);
        
        int64_t existingId = -1;
        if (sqlite3_step(checkStmt) == SQLITE_ROW) {
            existingId = sqlite3_column_int64(checkStmt, 0);
        }
        sqlite3_finalize(checkStmt);
        
        if (existingId > 0) {
            // Update existing entry
            spdlog::info("[DialogueDB] Updating existing whitelist entry (id={}): {} (FormID: 0x{:08X})", 
                existingId, enrichedEntry.targetEditorID, enrichedEntry.targetFormID);
            
            const char* updateSql = R"(
                UPDATE whitelist 
                SET block_type = ?, added_timestamp = ?, notes = ?, response_text = ?, subtype = ?, subtype_name = ?,
                    filter_category = ?, block_skyrimnet = ?, source_plugin = ?, quest_editorid = ?,
                    actor_filter_formids = ?, actor_filter_names = ?, faction_filter_editorids = ?
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
            sqlite3_bind_int64(updateStmt, 14, existingId);
            
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

    int Database::RemoveFromWhitelistBatch(const std::vector<int64_t>& ids)
    {
        if (ids.empty()) return 0;
        
        std::lock_guard<std::recursive_mutex> lock(dbMutex_);
        
        if (!db_) return 0;
        
        // Begin transaction directly (already have lock)
        sqlite3_exec(db_, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);
        
        int deletedCount = 0;
        const char* sql = "DELETE FROM whitelist WHERE id = ?;";
        sqlite3_stmt* stmt = nullptr;
        
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            spdlog::error("[DialogueDB] Failed to prepare batch whitelist delete statement: {}", sqlite3_errmsg(db_));
            sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
            return 0;
        }
        
        for (int64_t id : ids) {
            sqlite3_bind_int64(stmt, 1, id);
            if (sqlite3_step(stmt) == SQLITE_DONE) {
                deletedCount++;
            }
            sqlite3_reset(stmt);
        }
        
        sqlite3_finalize(stmt);
        sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, nullptr);
        
        if (deletedCount > 0) {
            spdlog::info("[DialogueDB] Deleted {} whitelist entries in batch", deletedCount);
        }
        
        return deletedCount;
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

    int64_t Database::GetWhitelistEntryId(uint32_t formID, const std::string& editorID)
    {
        std::lock_guard<std::recursive_mutex> lock(dbMutex_);

        if (!db_) return -1;

        // Prioritize EditorID matching (stable for ESL plugins), fall back to FormID if EditorID is empty
        const char* sql = "SELECT id FROM whitelist WHERE ((target_formid = ? OR target_formid = 0) AND target_editorid = ?) OR (target_editorid = '' AND target_formid = ?) LIMIT 1;";
        sqlite3_stmt* stmt = nullptr;

        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            return -1;
        }

        sqlite3_bind_int(stmt, 1, formID);
        sqlite3_bind_text(stmt, 2, editorID.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 3, formID);

        int64_t id = -1;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            id = sqlite3_column_int64(stmt, 0);
        }

        sqlite3_finalize(stmt);
        return id;
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
            BlacklistEntry entry;
            entry.id = sqlite3_column_int64(stmt, 0);
            entry.targetType = static_cast<BlacklistTarget>(sqlite3_column_int(stmt, 1));
            entry.targetFormID = sqlite3_column_int(stmt, 2);
            entry.targetEditorID = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            entry.blockType = static_cast<BlockType>(sqlite3_column_int(stmt, 4));
            entry.addedTimestamp = sqlite3_column_int64(stmt, 5);
            
            const char* notes = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
            entry.notes = notes ? notes : "";
            
            const char* responseText = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
            entry.responseText = responseText ? responseText : "";
            
            entry.subtype = sqlite3_column_int(stmt, 8);
            const char* subtypeName = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 9));
            entry.subtypeName = subtypeName ? subtypeName : "";
            
            const char* filterCategory = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 10));
            entry.filterCategory = filterCategory ? filterCategory : "Whitelist";
            
            // Column 11: block_skyrimnet
            entry.blockSkyrimNet = sqlite3_column_int(stmt, 11) != 0;
            
            // Column 12: source_plugin
            const char* sourcePlugin = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 12));
            entry.sourcePlugin = sourcePlugin ? sourcePlugin : "";
            
            // Column 13: quest_editorid
            const char* questEditorID = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 13));
            entry.questEditorID = questEditorID ? questEditorID : "";
            
            // Column 14: actor_filter_formids (JSON array)
            if (sqlite3_column_type(stmt, 14) != SQLITE_NULL) {
                const char* actorFormIDsJson = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 14));
                if (actorFormIDsJson) {
                    entry.actorFilterFormIDs = ParseActorFormIDsFromJson(actorFormIDsJson);
                }
            }
            
            // Column 15: actor_filter_names (JSON array)
            if (sqlite3_column_type(stmt, 15) != SQLITE_NULL) {
                const char* actorNamesJson = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 15));
                if (actorNamesJson) {
                    entry.actorFilterNames = ParseActorNamesFromJson(actorNamesJson);
                }
            }
            
            // Column 16: faction_filter_editorids (JSON array)
            if (sqlite3_column_count(stmt) > 16 && sqlite3_column_type(stmt, 16) != SQLITE_NULL) {
                const char* factionEditorIDsJson = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 16));
                if (factionEditorIDsJson) {
                    entry.factionFilterEditorIDs = ParseFactionEditorIDsFromJson(factionEditorIDsJson);
                }
            }
            
            // Derive blockAudio and blockSubtitles from blockType (not stored in DB)
            // SkyrimNet-only blocks never block audio/subtitles
            // Hard and Soft blocks always block both audio and subtitles
            if (entry.blockType == BlockType::SkyrimNet) {
                entry.blockAudio = false;
                entry.blockSubtitles = false;
            } else {
                entry.blockAudio = true;
                entry.blockSubtitles = true;
            }

            results.push_back(entry);
        }

        sqlite3_finalize(stmt);
        return results;
    }

    bool Database::IsWhitelisted(BlacklistTarget targetType, uint32_t formID, const std::string& editorID, uint32_t actorFormID, const std::string& actorName)
    {
        std::lock_guard<std::recursive_mutex> lock(dbMutex_);

        if (!db_) return false;

        // Prioritize EditorID matching (stable for ESL plugins), fall back to FormID if EditorID is empty
        const char* sql = "SELECT actor_filter_formids, actor_filter_names FROM whitelist WHERE target_type = ? AND (((target_formid = ? OR target_formid = 0) AND target_editorid = ?) OR (target_editorid = '' AND target_formid = ?)) LIMIT 1;";
        sqlite3_stmt* stmt = nullptr;

        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            return false;
        }

        sqlite3_bind_int(stmt, 1, static_cast<int>(targetType));
        sqlite3_bind_int(stmt, 2, formID);
        sqlite3_bind_text(stmt, 3, editorID.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 4, formID);

        bool isWhitelisted = false;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            // Entry found - check actor filters
            const char* actorFormIDsJson = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            const char* actorNamesJson = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            
            std::string actorFormIDsStr = actorFormIDsJson ? actorFormIDsJson : "[]";
            std::string actorNamesStr = actorNamesJson ? actorNamesJson : "[]";
            
            // Parse actor filters from database
            std::vector<uint32_t> filterFormIDs = ParseActorFormIDsFromJson(actorFormIDsStr);
            std::vector<std::string> filterNames = ParseActorNamesFromJson(actorNamesStr);
            
            // If no actor filter specified, whitelist applies to all actors
            if (filterFormIDs.empty() && filterNames.empty()) {
                isWhitelisted = true;
                spdlog::trace("[DialogueDB] IsWhitelisted: No actor filter -> WHITELISTED for all");
            }
            // If actor filter exists, check if current actor matches
            else if ((actorFormID > 0 || !actorName.empty())) {
                if (ActorMatchesFilter(actorFormID, actorName, filterFormIDs, filterNames)) {
                    isWhitelisted = true;
                    spdlog::trace("[DialogueDB] IsWhitelisted: Actor '{}' (0x{:08X}) matches filter -> WHITELISTED", 
                        actorName, actorFormID);
                } else {
                    isWhitelisted = false;
                    spdlog::trace("[DialogueDB] IsWhitelisted: Actor '{}' (0x{:08X}) not in filter -> NOT WHITELISTED", 
                        actorName, actorFormID);
                }
            }
            // Actor filter exists but no actor info provided - can't determine if whitelisted
            else {
                isWhitelisted = false;
                spdlog::trace("[DialogueDB] IsWhitelisted: Actor filter exists but no actor info provided -> NOT WHITELISTED");
            }
        }
        
        sqlite3_finalize(stmt);
        return isWhitelisted;
    }

    bool Database::IsWhitelisted(BlacklistTarget targetType, uint32_t formID, const std::string& editorID, uint32_t actorFormID, const std::string& actorName, RE::TESObjectREFR* actorRef)
    {
        std::lock_guard<std::recursive_mutex> lock(dbMutex_);
        if (!db_) return false;

        // Select actor, name, and faction filters
        const char* sql = "SELECT actor_filter_formids, actor_filter_names, faction_filter_editorids FROM whitelist WHERE target_type = ? AND (((target_formid = ? OR target_formid = 0) AND target_editorid = ?) OR (target_editorid = '' AND target_formid = ?)) LIMIT 1;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

        sqlite3_bind_int(stmt, 1, static_cast<int>(targetType));
        sqlite3_bind_int(stmt, 2, formID);
        sqlite3_bind_text(stmt, 3, editorID.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 4, formID);

        bool isWhitelisted = false;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const char* actorFormIDsJson = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            const char* actorNamesJson   = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            const char* factionJson      = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));

            std::vector<uint32_t>    filterFormIDs = ParseActorFormIDsFromJson(actorFormIDsJson ? actorFormIDsJson : "[]");
            std::vector<std::string> filterNames   = ParseActorNamesFromJson(actorNamesJson ? actorNamesJson : "[]");
            std::vector<std::string> factionFilter = ParseFactionEditorIDsFromJson(factionJson ? factionJson : "[]");

            bool hasActorFilter   = !filterFormIDs.empty() || !filterNames.empty();
            bool hasFactionFilter = !factionFilter.empty();

            if (!hasActorFilter && !hasFactionFilter) {
                // No filters - whitelist applies to everyone
                isWhitelisted = true;
                spdlog::debug("[DialogueDB] IsWhitelisted(+ref): No filter -> WHITELISTED for all");
            } else if ((actorFormID > 0 || !actorName.empty()) || actorRef) {
                bool actorMatches   = hasActorFilter   && ActorMatchesFilter(actorFormID, actorName, filterFormIDs, filterNames);
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

    bool Database::IsPluginWhitelisted(const std::string& pluginName)
    {
        std::lock_guard<std::recursive_mutex> lock(dbMutex_);

        if (!db_ || pluginName.empty()) return false;

        // Check if this plugin is whitelisted (target_type = Plugin, target_editorid = plugin name)
        const char* sql = "SELECT 1 FROM whitelist WHERE target_type = ? AND target_editorid = ? LIMIT 1;";
        sqlite3_stmt* stmt = nullptr;

        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            return false;
        }

        sqlite3_bind_int(stmt, 1, static_cast<int>(BlacklistTarget::Plugin));
        sqlite3_bind_text(stmt, 2, pluginName.c_str(), -1, SQLITE_TRANSIENT);

        bool found = (sqlite3_step(stmt) == SQLITE_ROW);
        sqlite3_finalize(stmt);

        return found;
    }
}
