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
    bool Database::AddToBlacklist(const BlacklistEntry& entry, bool skipEnrichment)
    {
        std::lock_guard<std::recursive_mutex> lock(dbMutex_);

        if (!db_) return false;

        // Make a mutable copy for enrichment
        BlacklistEntry enrichedEntry = entry;
        
        // Enrich with extracted response text (unless skipped for performance)
        if (!skipEnrichment) {
            if (enrichedEntry.targetType == BlacklistTarget::Topic && !enrichedEntry.targetEditorID.empty()) {
                spdlog::debug("[DialogueDB] Extracting responses for topic: {}", enrichedEntry.targetEditorID);
                auto responses = TopicResponseExtractor::ExtractAllResponsesForTopic(enrichedEntry.targetEditorID);
                spdlog::debug("[DialogueDB] Extracted {} responses for topic {}", responses.size(), enrichedEntry.targetEditorID);
                
                if (!responses.empty()) {
                    // Store all responses as JSON array in response_text
                    enrichedEntry.responseText = ResponsesToJson(responses);
                }
            } else if (enrichedEntry.targetType == BlacklistTarget::Scene && !enrichedEntry.targetEditorID.empty()) {
                spdlog::debug("[DialogueDB] Extracting responses for scene: {}", enrichedEntry.targetEditorID);
                auto responses = TopicResponseExtractor::ExtractAllResponsesForScene(enrichedEntry.targetEditorID);
                spdlog::debug("[DialogueDB] Extracted {} responses for scene {}", responses.size(), enrichedEntry.targetEditorID);
                
                if (!responses.empty()) {
                    // Store all responses as JSON array in response_text
                    enrichedEntry.responseText = ResponsesToJson(responses);
                }
            }
        }

        // Check if entry already exists using ESL-safe matching
        // Priority order:
        // 1. For Topics with quest+plugin: Match by quest_editorid + source_plugin + local FormID (ESL-safe)
        // 2. Match by EditorID (works for all ESP/ESM with EditorIDs)
        // 3. Match by full FormID (fallback for topics without EditorIDs)
        
        int64_t existingId = -1;
        int existingBlockType = -1;
        
        // Try ESL-safe match first (only for Topics with quest and plugin info)
        if (enrichedEntry.targetType == BlacklistTarget::Topic && 
            !enrichedEntry.questEditorID.empty() && 
            !enrichedEntry.sourcePlugin.empty() &&
            enrichedEntry.targetFormID != 0) {
            
            uint32_t localFormID = enrichedEntry.targetFormID & 0xFFF;  // Extract last 3 hex digits (stable part)
            
            const char* eslCheckSql = R"(
                SELECT id, block_type FROM blacklist 
                WHERE target_type = ? 
                  AND quest_editorid = ? 
                  AND source_plugin = ? 
                  AND (target_formid & 4095) = ?
                LIMIT 1;
            )";
            
            sqlite3_stmt* eslCheckStmt = nullptr;
            if (sqlite3_prepare_v2(db_, eslCheckSql, -1, &eslCheckStmt, nullptr) == SQLITE_OK) {
                sqlite3_bind_int(eslCheckStmt, 1, static_cast<int>(enrichedEntry.targetType));
                sqlite3_bind_text(eslCheckStmt, 2, enrichedEntry.questEditorID.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(eslCheckStmt, 3, enrichedEntry.sourcePlugin.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_int(eslCheckStmt, 4, localFormID);
                
                if (sqlite3_step(eslCheckStmt) == SQLITE_ROW) {
                    existingId = sqlite3_column_int64(eslCheckStmt, 0);
                    existingBlockType = sqlite3_column_int(eslCheckStmt, 1);
                    spdlog::debug("[DialogueDB] Found existing entry via ESL-safe match (id={}): Quest={}, Plugin={}, LocalFormID=0x{:03X}", 
                        existingId, enrichedEntry.questEditorID, enrichedEntry.sourcePlugin, localFormID);
                }
                sqlite3_finalize(eslCheckStmt);
            }
        }
        
        // If not found via ESL-safe match, try standard EditorID/FormID match
        if (existingId == -1) {
            const char* checkSql = "SELECT id, block_type FROM blacklist WHERE target_type = ? AND (((target_formid = ? OR target_formid = 0) AND target_editorid = ?) OR (target_editorid = '' AND target_formid = ?)) LIMIT 1;";
            sqlite3_stmt* checkStmt = nullptr;
            
            if (sqlite3_prepare_v2(db_, checkSql, -1, &checkStmt, nullptr) != SQLITE_OK) {
                spdlog::error("[DialogueDB] Failed to prepare check statement: {}", sqlite3_errmsg(db_));
                return false;
            }
            
            sqlite3_bind_int(checkStmt, 1, static_cast<int>(enrichedEntry.targetType));
            sqlite3_bind_int(checkStmt, 2, enrichedEntry.targetFormID);
            sqlite3_bind_text(checkStmt, 3, enrichedEntry.targetEditorID.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(checkStmt, 4, enrichedEntry.targetFormID);
            
            if (sqlite3_step(checkStmt) == SQLITE_ROW) {
                existingId = sqlite3_column_int64(checkStmt, 0);
                existingBlockType = sqlite3_column_int(checkStmt, 1);
            }
            sqlite3_finalize(checkStmt);
        }
        
        if (existingId > 0) {
            // Check if block type is changing (for logging purposes)
            bool blockTypeChanged = (existingBlockType != static_cast<int>(enrichedEntry.blockType));
            
            // Update existing entry (always update to catch changes in notes, filterCategory, etc.)
            if (blockTypeChanged) {
                spdlog::info("[DialogueDB] Updating existing blacklist entry (id={}): {} (FormID: 0x{:08X}) - block type changing from {} to {}", 
                    existingId, enrichedEntry.targetEditorID, enrichedEntry.targetFormID, existingBlockType, static_cast<int>(enrichedEntry.blockType));
            } else {
                spdlog::info("[DialogueDB] Updating existing blacklist entry (id={}): {} (FormID: 0x{:08X})", 
                    existingId, enrichedEntry.targetEditorID, enrichedEntry.targetFormID);
            }
            
            const char* updateSql = R"(
                UPDATE blacklist 
                SET block_type = ?, added_timestamp = ?, notes = ?, response_text = ?, subtype = ?, subtype_name = ?,
                    filter_category = ?, block_skyrimnet = ?, source_plugin = ?, quest_editorid = ?,
                    actor_filter_formids = ?, actor_filter_names = ?, faction_filter_editorids = ?
                WHERE id = ?;
            )";
            sqlite3_stmt* updateStmt = nullptr;
            
            if (sqlite3_prepare_v2(db_, updateSql, -1, &updateStmt, nullptr) != SQLITE_OK) {
                spdlog::error("[DialogueDB] Failed to prepare update statement: {}", sqlite3_errmsg(db_));
                return false;
            }
            
            // Serialize actor and faction filters to JSON
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
                spdlog::error("[DialogueDB] Failed to update blacklist entry: {}", sqlite3_errmsg(db_));
            }
            sqlite3_finalize(updateStmt);
            
            // VERIFICATION: Read back actor filters to ensure both columns were updated
            if (success && (!enrichedEntry.actorFilterFormIDs.empty() || !enrichedEntry.actorFilterNames.empty())) {
                const char* verifySql = "SELECT actor_filter_formids, actor_filter_names FROM blacklist WHERE id = ?;";
                sqlite3_stmt* verifyStmt = nullptr;
                if (sqlite3_prepare_v2(db_, verifySql, -1, &verifyStmt, nullptr) == SQLITE_OK) {
                    sqlite3_bind_int64(verifyStmt, 1, existingId);
                    if (sqlite3_step(verifyStmt) == SQLITE_ROW) {
                        const char* savedFormIDs = reinterpret_cast<const char*>(sqlite3_column_text(verifyStmt, 0));
                        const char* savedNames = reinterpret_cast<const char*>(sqlite3_column_text(verifyStmt, 1));
                        
                        if (!savedFormIDs || !savedNames) {
                            spdlog::error("[DialogueDB] VERIFICATION FAILED: Actor filter columns are NULL after update!");
                            success = false;
                        } else {
                            std::string formIDsStr = savedFormIDs;
                            std::string namesStr = savedNames;
                            
                            if (formIDsStr == "[]" && actorFormIDsJson != "[]") {
                                spdlog::error("[DialogueDB] VERIFICATION FAILED: actor_filter_formids is empty, expected: {}", actorFormIDsJson);
                                success = false;
                            }
                            if (namesStr == "[]" && actorNamesJson != "[]") {
                                spdlog::error("[DialogueDB] VERIFICATION FAILED: actor_filter_names is empty, expected: {}", actorNamesJson);
                                success = false;
                            }
                            
                            if (success) {
                                spdlog::debug("[DialogueDB] Verification passed: actor filters updated correctly");
                            }
                        }
                    }
                    sqlite3_finalize(verifyStmt);
                } else {
                    spdlog::warn("[DialogueDB] Could not prepare verification statement: {}", sqlite3_errmsg(db_));
                }
            }
            
            // Update scene conditions at runtime based on BlockType
            // Only Hard blocks (2) prevent scenes from starting
            if (success) {
                if (enrichedEntry.targetType == BlacklistTarget::Scene) {
                    // First, remove any auto-added topics from previous soft/skyrimnet blocks
                    std::string notePattern = "Auto-added from scene: " + enrichedEntry.targetEditorID;
                    const char* deleteSql = "DELETE FROM blacklist WHERE target_type = 1 AND notes = ?;";
                    sqlite3_stmt* deleteStmt = nullptr;
                    
                    if (sqlite3_prepare_v2(db_, deleteSql, -1, &deleteStmt, nullptr) == SQLITE_OK) {
                        sqlite3_bind_text(deleteStmt, 1, notePattern.c_str(), -1, SQLITE_TRANSIENT);
                        int deletedCount = 0;
                        if (sqlite3_step(deleteStmt) == SQLITE_DONE) {
                            deletedCount = sqlite3_changes(db_);
                        }
                        sqlite3_finalize(deleteStmt);
                        if (deletedCount > 0) {
                            spdlog::info("[DialogueDB] Removed {} auto-added topics from previous scene block", deletedCount);
                        }
                    }
                    
                    // Update scene phase conditions
                    SceneHook::UpdateSceneConditions(enrichedEntry.targetEditorID, static_cast<uint8_t>(enrichedEntry.blockType));
                } else if (enrichedEntry.targetType == BlacklistTarget::Topic) {
                    // Only update scene conditions if topic has an EditorID
                    // Topics with empty EditorIDs can't be reliably matched to scene dialogue actions
                    if (!enrichedEntry.targetEditorID.empty()) {
                        SceneHook::UpdateSceneConditionsForTopic(enrichedEntry.targetEditorID, static_cast<uint8_t>(enrichedEntry.blockType));
                    }
                }
            }
            
            return success;
        } else {
            // Insert new entry
            spdlog::debug("[DialogueDB] Inserting new blacklist entry: {} (FormID: 0x{:08X}, Type: {}, FilterCategory: '{}')", 
                enrichedEntry.targetEditorID, enrichedEntry.targetFormID, static_cast<int>(enrichedEntry.targetType), enrichedEntry.filterCategory);
            
            if (!insertBlacklistStmt_) {
                spdlog::error("[DialogueDB] insertBlacklistStmt_ is null!");
                return false;
            }
            
            sqlite3_reset(insertBlacklistStmt_);
            sqlite3_bind_int(insertBlacklistStmt_, 1, static_cast<int>(enrichedEntry.targetType));
            sqlite3_bind_int(insertBlacklistStmt_, 2, enrichedEntry.targetFormID);
            sqlite3_bind_text(insertBlacklistStmt_, 3, enrichedEntry.targetEditorID.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(insertBlacklistStmt_, 4, static_cast<int>(enrichedEntry.blockType));
            sqlite3_bind_int64(insertBlacklistStmt_, 5, enrichedEntry.addedTimestamp);
            sqlite3_bind_text(insertBlacklistStmt_, 6, enrichedEntry.notes.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(insertBlacklistStmt_, 7, enrichedEntry.responseText.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(insertBlacklistStmt_, 8, enrichedEntry.subtype);
            sqlite3_bind_text(insertBlacklistStmt_, 9, enrichedEntry.subtypeName.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(insertBlacklistStmt_, 10, enrichedEntry.filterCategory.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(insertBlacklistStmt_, 11, enrichedEntry.blockSkyrimNet ? 1 : 0);
            sqlite3_bind_text(insertBlacklistStmt_, 12, enrichedEntry.sourcePlugin.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(insertBlacklistStmt_, 13, enrichedEntry.questEditorID.c_str(), -1, SQLITE_TRANSIENT);
            
            // Serialize actor and faction filters to JSON
            std::string actorFormIDsJson = ActorFormIDsToJson(enrichedEntry.actorFilterFormIDs);
            std::string actorNamesJson = ActorNamesToJson(enrichedEntry.actorFilterNames);
            std::string factionEditorIDsJson = FactionEditorIDsToJson(enrichedEntry.factionFilterEditorIDs);
            sqlite3_bind_text(insertBlacklistStmt_, 14, actorFormIDsJson.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(insertBlacklistStmt_, 15, actorNamesJson.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(insertBlacklistStmt_, 16, factionEditorIDsJson.c_str(), -1, SQLITE_TRANSIENT);

            if (sqlite3_step(insertBlacklistStmt_) != SQLITE_DONE) {
                spdlog::error("[DialogueDB] Failed to add blacklist entry: {}", sqlite3_errmsg(db_));
                return false;
            }

            int64_t newId = sqlite3_last_insert_rowid(db_);
            spdlog::debug("[DialogueDB] Successfully inserted blacklist entry (id={})", newId);
            
            // VERIFICATION: Read back actor filters to ensure both columns were written
            if (!enrichedEntry.actorFilterFormIDs.empty() || !enrichedEntry.actorFilterNames.empty()) {
                const char* verifySql = "SELECT actor_filter_formids, actor_filter_names FROM blacklist WHERE id = ?;";
                sqlite3_stmt* verifyStmt = nullptr;
                if (sqlite3_prepare_v2(db_, verifySql, -1, &verifyStmt, nullptr) == SQLITE_OK) {
                    sqlite3_bind_int64(verifyStmt, 1, newId);
                    if (sqlite3_step(verifyStmt) == SQLITE_ROW) {
                        const char* savedFormIDs = reinterpret_cast<const char*>(sqlite3_column_text(verifyStmt, 0));
                        const char* savedNames = reinterpret_cast<const char*>(sqlite3_column_text(verifyStmt, 1));
                        
                        if (!savedFormIDs || !savedNames) {
                            spdlog::error("[DialogueDB] VERIFICATION FAILED: Actor filter columns are NULL after insert!");
                            sqlite3_finalize(verifyStmt);
                            return false;
                        }
                        
                        std::string formIDsStr = savedFormIDs;
                        std::string namesStr = savedNames;
                        
                        if (formIDsStr == "[]" && actorFormIDsJson != "[]") {
                            spdlog::error("[DialogueDB] VERIFICATION FAILED: actor_filter_formids is empty, expected: {}", actorFormIDsJson);
                            sqlite3_finalize(verifyStmt);
                            return false;
                        }
                        if (namesStr == "[]" && actorNamesJson != "[]") {
                            spdlog::error("[DialogueDB] VERIFICATION FAILED: actor_filter_names is empty, expected: {}", actorNamesJson);
                            sqlite3_finalize(verifyStmt);
                            return false;
                        }
                        
                        spdlog::debug("[DialogueDB] Verification passed: actor filters written correctly");
                    }
                    sqlite3_finalize(verifyStmt);
                } else {
                    spdlog::warn("[DialogueDB] Could not prepare verification statement: {}", sqlite3_errmsg(db_));
                }
            }
            
            // Update scene conditions at runtime based on BlockType
            if (enrichedEntry.targetType == BlacklistTarget::Scene) {
                // Scene was directly blocked
                SceneHook::UpdateSceneConditions(enrichedEntry.targetEditorID, static_cast<uint8_t>(enrichedEntry.blockType));
            } else if (enrichedEntry.targetType == BlacklistTarget::Topic) {
                // Topic was blocked - update any scenes containing this topic
                // Only update scene conditions if topic has an EditorID
                if (!enrichedEntry.targetEditorID.empty()) {
                    SceneHook::UpdateSceneConditionsForTopic(enrichedEntry.targetEditorID, static_cast<uint8_t>(enrichedEntry.blockType));
                }
            }
            
            return true;
        }
    }

    bool Database::RemoveFromBlacklist(int64_t id)
    {
        std::lock_guard<std::recursive_mutex> lock(dbMutex_);

        if (!db_) return false;
        
        // First, get the entry details before deleting so we can unblock scenes
        const char* selectSql = "SELECT target_type, target_editorid FROM blacklist WHERE id = ?;";
        sqlite3_stmt* selectStmt = nullptr;
        
        BlacklistTarget targetType = BlacklistTarget::Topic;
        std::string targetEditorID;
        
        if (sqlite3_prepare_v2(db_, selectSql, -1, &selectStmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int64(selectStmt, 1, id);
            if (sqlite3_step(selectStmt) == SQLITE_ROW) {
                targetType = static_cast<BlacklistTarget>(sqlite3_column_int(selectStmt, 0));
                const char* editorID = reinterpret_cast<const char*>(sqlite3_column_text(selectStmt, 1));
                targetEditorID = editorID ? editorID : "";
            }
            sqlite3_finalize(selectStmt);
        }

        const char* sql = "DELETE FROM blacklist WHERE id = ?;";
        sqlite3_stmt* stmt = nullptr;

        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            return false;
        }

        sqlite3_bind_int64(stmt, 1, id);
        bool success = sqlite3_step(stmt) == SQLITE_DONE;
        sqlite3_finalize(stmt);
        
        // Update scene conditions at runtime if deletion succeeded
        // Pass blockType=1 (Soft) to remove scene conditions since entry is being deleted
        if (success && !targetEditorID.empty()) {
            if (targetType == BlacklistTarget::Scene) {
                // Scene was directly unblocked - remove scene conditions
                SceneHook::UpdateSceneConditions(targetEditorID, 1);  // 1=Soft (removes conditions)
                
                // (no cache to rebuild - scene lookup is done at query time in Config::ShouldSoftBlock)
            } else if (targetType == BlacklistTarget::Topic) {
                // Topic was unblocked - remove scene conditions from affected scenes
                // Only update scene conditions if topic has an EditorID
                if (!targetEditorID.empty()) {
                    SceneHook::UpdateSceneConditionsForTopic(targetEditorID, 1);  // 1=Soft (removes conditions)
                }
            }
        }

        return success;
    }

    int Database::AddToBlacklistBatch(const std::vector<BlacklistEntry>& entries, bool skipEnrichment)
    {
        if (entries.empty()) return 0;
        
        std::lock_guard<std::recursive_mutex> lock(dbMutex_);
        
        if (!db_) return 0;
        
        int addedCount = 0;
        
        // Begin transaction within the lock
        sqlite3_exec(db_, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);
        
        try {
            for (const auto& entry : entries) {
                // Inline the AddToBlacklist logic without acquiring lock
                BlacklistEntry enrichedEntry = entry;
                
                // Enrich with extracted response text (unless skipped for performance)
                if (!skipEnrichment) {
                    if (enrichedEntry.targetType == BlacklistTarget::Topic && !enrichedEntry.targetEditorID.empty()) {
                        auto responses = TopicResponseExtractor::ExtractAllResponsesForTopic(enrichedEntry.targetEditorID);
                        if (!responses.empty()) {
                            enrichedEntry.responseText = ResponsesToJson(responses);
                        }
                    } else if (enrichedEntry.targetType == BlacklistTarget::Scene && !enrichedEntry.targetEditorID.empty()) {
                        auto responses = TopicResponseExtractor::ExtractAllResponsesForScene(enrichedEntry.targetEditorID);
                        if (!responses.empty()) {
                            enrichedEntry.responseText = ResponsesToJson(responses);
                        }
                    }
                }
                
                // Check if entry already exists using ESL-safe matching (prioritize EditorID)
                const char* checkSql = "SELECT id FROM blacklist WHERE target_type = ? AND (((target_formid = ? OR target_formid = 0) AND target_editorid = ?) OR (target_editorid = '' AND target_formid = ?)) LIMIT 1;";
                sqlite3_stmt* checkStmt = nullptr;
                
                if (sqlite3_prepare_v2(db_, checkSql, -1, &checkStmt, nullptr) != SQLITE_OK) {
                    continue;
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
                
                bool success = false;
                if (existingId > 0) {
                    // Update existing entry
                    const char* updateSql = R"(
                        UPDATE blacklist 
                        SET block_type = ?, added_timestamp = ?, notes = ?, response_text = ?, subtype = ?, subtype_name = ?,
                            filter_category = ?, block_skyrimnet = ?, source_plugin = ?, quest_editorid = ?
                        WHERE id = ?;
                    )";
                    sqlite3_stmt* updateStmt = nullptr;
                    
                    if (sqlite3_prepare_v2(db_, updateSql, -1, &updateStmt, nullptr) == SQLITE_OK) {
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
                        sqlite3_bind_int64(updateStmt, 11, existingId);
                        
                        success = (sqlite3_step(updateStmt) == SQLITE_DONE);
                        sqlite3_finalize(updateStmt);
                    }
                    
                    // Update scene conditions based on block type
                    if (success && enrichedEntry.targetType == BlacklistTarget::Scene) {
                        // Remove auto-added topics from previous blocks
                        std::string notePattern = "Auto-added from scene: " + enrichedEntry.targetEditorID;
                        const char* deleteSql = "DELETE FROM blacklist WHERE target_type = 1 AND notes = ?;";
                        sqlite3_stmt* deleteStmt = nullptr;
                        
                        if (sqlite3_prepare_v2(db_, deleteSql, -1, &deleteStmt, nullptr) == SQLITE_OK) {
                            sqlite3_bind_text(deleteStmt, 1, notePattern.c_str(), -1, SQLITE_TRANSIENT);
                            sqlite3_step(deleteStmt);
                            sqlite3_finalize(deleteStmt);
                        }
                        
                        // Note: Scene condition updates will be done after transaction commits
                    }
                } else {
                    // Insert new entry
                    sqlite3_reset(insertBlacklistStmt_);
                    sqlite3_bind_int(insertBlacklistStmt_, 1, static_cast<int>(enrichedEntry.targetType));
                    sqlite3_bind_int(insertBlacklistStmt_, 2, enrichedEntry.targetFormID);
                    sqlite3_bind_text(insertBlacklistStmt_, 3, enrichedEntry.targetEditorID.c_str(), -1, SQLITE_TRANSIENT);
                    sqlite3_bind_int(insertBlacklistStmt_, 4, static_cast<int>(enrichedEntry.blockType));
                    sqlite3_bind_int64(insertBlacklistStmt_, 5, enrichedEntry.addedTimestamp);
                    sqlite3_bind_text(insertBlacklistStmt_, 6, enrichedEntry.notes.c_str(), -1, SQLITE_TRANSIENT);
                    sqlite3_bind_text(insertBlacklistStmt_, 7, enrichedEntry.responseText.c_str(), -1, SQLITE_TRANSIENT);
                    sqlite3_bind_int(insertBlacklistStmt_, 8, enrichedEntry.subtype);
                    sqlite3_bind_text(insertBlacklistStmt_, 9, enrichedEntry.subtypeName.c_str(), -1, SQLITE_TRANSIENT);
                    sqlite3_bind_text(insertBlacklistStmt_, 10, enrichedEntry.filterCategory.c_str(), -1, SQLITE_TRANSIENT);
                    sqlite3_bind_int(insertBlacklistStmt_, 11, enrichedEntry.blockSkyrimNet ? 1 : 0);
                    sqlite3_bind_text(insertBlacklistStmt_, 12, enrichedEntry.sourcePlugin.c_str(), -1, SQLITE_TRANSIENT);
                    sqlite3_bind_text(insertBlacklistStmt_, 13, enrichedEntry.questEditorID.c_str(), -1, SQLITE_TRANSIENT);
                    
                    success = (sqlite3_step(insertBlacklistStmt_) == SQLITE_DONE);
                    
                    // Note: Scene condition updates will be done after transaction commits
                }
                
                if (success) {
                    addedCount++;
                }
            }
            
            // Commit transaction
            sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, nullptr);
            
            // Now update scene conditions outside of transaction (can be slow)
            for (const auto& entry : entries) {
                if (entry.targetType == BlacklistTarget::Scene) {
                    SceneHook::UpdateSceneConditions(entry.targetEditorID, static_cast<uint8_t>(entry.blockType));
                } else if (entry.targetType == BlacklistTarget::Topic) {
                    // Only update scene conditions if topic has an EditorID
                    if (!entry.targetEditorID.empty()) {
                        SceneHook::UpdateSceneConditionsForTopic(entry.targetEditorID, static_cast<uint8_t>(entry.blockType));
                    }
                }
            }
            
            spdlog::info("[DialogueDB] Batch added/updated {} entries to blacklist", addedCount);
        } catch (const std::exception& e) {
            spdlog::error("[DialogueDB] Error in batch add, rolling back: {}", e.what());
            sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
            addedCount = 0;
        }
        
        return addedCount;
    }

    int Database::RemoveFromBlacklistBatch(const std::vector<int64_t>& ids)
    {
        if (ids.empty()) return 0;
        
        std::lock_guard<std::recursive_mutex> lock(dbMutex_);
        
        if (!db_) return 0;
        
        int removedCount = 0;
        
        // Collect scene/topic info before deletion for cleanup
        struct EntryInfo {
            BlacklistTarget targetType;
            std::string targetEditorID;
        };
        std::vector<EntryInfo> entriesToCleanup;
        
        // Begin transaction within the lock
        sqlite3_exec(db_, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);
        
        try {
            // First pass: collect info for scene condition cleanup
            const char* selectSql = "SELECT target_type, target_editorid FROM blacklist WHERE id = ?;";
            sqlite3_stmt* selectStmt = nullptr;
            
            if (sqlite3_prepare_v2(db_, selectSql, -1, &selectStmt, nullptr) == SQLITE_OK) {
                for (int64_t id : ids) {
                    sqlite3_reset(selectStmt);
                    sqlite3_bind_int64(selectStmt, 1, id);
                    
                    if (sqlite3_step(selectStmt) == SQLITE_ROW) {
                        EntryInfo info;
                        info.targetType = static_cast<BlacklistTarget>(sqlite3_column_int(selectStmt, 0));
                        const char* editorID = reinterpret_cast<const char*>(sqlite3_column_text(selectStmt, 1));
                        info.targetEditorID = editorID ? editorID : "";
                        entriesToCleanup.push_back(info);
                    }
                }
                sqlite3_finalize(selectStmt);
            }
            
            // Second pass: delete entries
            const char* deleteSql = "DELETE FROM blacklist WHERE id = ?;";
            sqlite3_stmt* deleteStmt = nullptr;
            
            if (sqlite3_prepare_v2(db_, deleteSql, -1, &deleteStmt, nullptr) == SQLITE_OK) {
                for (int64_t id : ids) {
                    sqlite3_reset(deleteStmt);
                    sqlite3_bind_int64(deleteStmt, 1, id);
                    
                    if (sqlite3_step(deleteStmt) == SQLITE_DONE) {
                        removedCount++;
                    }
                }
                sqlite3_finalize(deleteStmt);
            }
            
            // Clean up auto-added topics for scenes
            for (const auto& info : entriesToCleanup) {
                if (info.targetType == BlacklistTarget::Scene && !info.targetEditorID.empty()) {
                    std::string notePattern = "Auto-added from scene: " + info.targetEditorID;
                    const char* cleanupSql = "DELETE FROM blacklist WHERE target_type = 1 AND notes = ?;";
                    sqlite3_stmt* cleanupStmt = nullptr;
                    
                    if (sqlite3_prepare_v2(db_, cleanupSql, -1, &cleanupStmt, nullptr) == SQLITE_OK) {
                        sqlite3_bind_text(cleanupStmt, 1, notePattern.c_str(), -1, SQLITE_TRANSIENT);
                        sqlite3_step(cleanupStmt);
                        sqlite3_finalize(cleanupStmt);
                    }
                }
            }
            
            // Commit transaction
            sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, nullptr);
            
            // Now update scene conditions outside of transaction (can be slow)
            for (const auto& info : entriesToCleanup) {
                if (!info.targetEditorID.empty()) {
                    if (info.targetType == BlacklistTarget::Scene) {
                        SceneHook::UpdateSceneConditions(info.targetEditorID, 1);  // 1=Soft (removes conditions)
                    } else if (info.targetType == BlacklistTarget::Topic) {
                        SceneHook::UpdateSceneConditionsForTopic(info.targetEditorID, 1);  // 1=Soft (removes conditions)
                    }
                }
            }
            
            spdlog::info("[DialogueDB] Batch removed {} entries from blacklist", removedCount);
        } catch (const std::exception& e) {
            spdlog::error("[DialogueDB] Error in batch remove, rolling back: {}", e.what());
            sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
            removedCount = 0;
        }
        
        return removedCount;
    }

    int Database::ClearBlacklist()
    {
        std::lock_guard<std::recursive_mutex> lock(dbMutex_);

        if (!db_) return 0;

        // First, get count of entries to be deleted
        int count = 0;
        const char* countSql = "SELECT COUNT(*) FROM blacklist;";
        sqlite3_stmt* countStmt = nullptr;
        
        if (sqlite3_prepare_v2(db_, countSql, -1, &countStmt, nullptr) == SQLITE_OK) {
            if (sqlite3_step(countStmt) == SQLITE_ROW) {
                count = sqlite3_column_int(countStmt, 0);
            }
            sqlite3_finalize(countStmt);
        }

        if (count == 0) {
            spdlog::info("[DialogueDB] No blacklist entries to clear");
            return 0;
        }

        // Delete all entries
        const char* sql = "DELETE FROM blacklist;";
        char* errMsg = nullptr;
        int rc = sqlite3_exec(db_, sql, nullptr, nullptr, &errMsg);
        
        if (rc != SQLITE_OK) {
            spdlog::error("[DialogueDB] Failed to clear blacklist: {}", errMsg ? errMsg : "unknown error");
            if (errMsg) sqlite3_free(errMsg);
            return 0;
        }

        spdlog::info("[DialogueDB] Cleared {} blacklist entries", count);
        
        // Clear all scene conditions since all scenes are now unblocked
        // Note: This is a placeholder - in reality we'd need to iterate all scenes
        // but since we're clearing everything, it's acceptable to leave stale conditions
        // that will get cleaned up on next game load
        
        return count;
    }

    int64_t Database::GetBlacklistEntryId(uint32_t formID, const std::string& editorID)
    {
        std::lock_guard<std::recursive_mutex> lock(dbMutex_);

        if (!db_) return -1;

        // Prioritize EditorID matching (stable for ESL plugins), fall back to FormID if EditorID is empty
        const char* sql = "SELECT id FROM blacklist WHERE ((target_formid = ? OR target_formid = 0) AND target_editorid = ?) OR (target_editorid = '' AND target_formid = ?) LIMIT 1;";
        sqlite3_stmt* stmt = nullptr;

        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            return -1;
        }

        sqlite3_bind_int(stmt, 1, formID);
        sqlite3_bind_text(stmt, 2, editorID.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 3, formID);

        int64_t entryId = -1;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            entryId = sqlite3_column_int64(stmt, 0);
        }

        sqlite3_finalize(stmt);
        spdlog::debug("[DialogueDB] GetBlacklistEntryId(formID=0x{:08X}, editorID='{}') returned {}", 
            formID, editorID, entryId);
        return entryId;
    }

    std::vector<BlacklistEntry> Database::GetBlacklist()
    {
        std::lock_guard<std::recursive_mutex> lock(dbMutex_);
        std::vector<BlacklistEntry> results;

        if (!db_) return results;

        const char* sql = "SELECT * FROM blacklist ORDER BY added_timestamp ASC;";
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
            entry.filterCategory = filterCategory ? filterCategory : "Blacklist";
            
            // Column 11: block_skyrimnet
            entry.blockSkyrimNet = sqlite3_column_int(stmt, 11) != 0;
            
            // Column 12: source_plugin (may not exist in old databases)
            if (sqlite3_column_type(stmt, 12) != SQLITE_NULL) {
                const char* sourcePlugin = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 12));
                entry.sourcePlugin = sourcePlugin ? sourcePlugin : "";
            } else {
                entry.sourcePlugin = "";
            }
            
            // Column 13: quest_editorid (may not exist in old databases)
            if (sqlite3_column_type(stmt, 13) != SQLITE_NULL) {
                const char* questEditorID = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 13));
                entry.questEditorID = questEditorID ? questEditorID : "";
            } else {
                entry.questEditorID = "";
            }
            
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

    // Hardcoded Scene Import (now uses unified blacklist)

    void Database::ImportHardcodedScenes(const std::vector<std::string>& sceneEditorIDs, const std::string& filterCategory)
    {
        // Import hardcoded scenes WITHOUT enrichment (enrichment deferred to kPostLoadGame)
        spdlog::info("[DialogueDB] Importing {} scenes with filter category '{}'...", 
            sceneEditorIDs.size(), filterCategory);
        
        int sceneCount = 0;
        for (const auto& editorID : sceneEditorIDs) {
            // Determine source plugin from EditorID prefix
            std::string sourcePlugin;
            if (editorID.starts_with("DLC1")) {
                sourcePlugin = "Dawnguard.esm";
            } else if (editorID.starts_with("DLC2")) {
                sourcePlugin = "Dragonborn.esm";
            } else {
                sourcePlugin = "Skyrim.esm";
            }
            
            BlacklistEntry entry;
            entry.targetType = BlacklistTarget::Scene;
            entry.targetFormID = 0;
            entry.targetEditorID = editorID;
            entry.blockType = BlockType::Hard;
            entry.notes = "Pre-included ambient scene";
            entry.subtype = 14;  // Scene subtype
            entry.subtypeName = "Scene";
            entry.filterCategory = filterCategory;
            entry.blockAudio = true;
            entry.blockSubtitles = true;
            entry.blockSkyrimNet = true;
            entry.sourcePlugin = sourcePlugin;
            
            // Note: Don't extract responses here - Skyrim uses lazy loading for TopicInfos.
            // Responses will be populated at runtime when the scene first plays (via EnrichBlacklistEntryAtRuntime).
            entry.responseText = "[]";
            
            spdlog::debug("[DialogueDB] Importing scene '{}' with filterCategory: '{}'", editorID, filterCategory);
            
            // Skip enrichment during import (will enrich after kPostLoadGame)
            if (AddToBlacklist(entry, true)) {
                sceneCount++;
            }
        }
        spdlog::info("[DialogueDB] Imported {} new scenes to unified blacklist (enrichment deferred)", sceneCount);
    }

    void Database::EnrichBlacklistEntryAtRuntime(BlacklistTarget targetType, const std::string& targetEditorID, const std::string& responseText)
    {
        if (targetEditorID.empty() || responseText.empty()) return;
        
        std::lock_guard<std::recursive_mutex> lock(dbMutex_);
        if (!db_) return;
        
        // Check if entry exists
        const char* checkSql = "SELECT id, response_text FROM blacklist WHERE target_type = ? AND target_editorid = ? LIMIT 1;";
        sqlite3_stmt* checkStmt = nullptr;
        
        if (sqlite3_prepare_v2(db_, checkSql, -1, &checkStmt, nullptr) != SQLITE_OK) {
            return;
        }
        
        sqlite3_bind_int(checkStmt, 1, static_cast<int>(targetType));
        sqlite3_bind_text(checkStmt, 2, targetEditorID.c_str(), -1, SQLITE_TRANSIENT);
        
        int64_t entryId = -1;
        std::string existingJson;
        
        if (sqlite3_step(checkStmt) == SQLITE_ROW) {
            entryId = sqlite3_column_int64(checkStmt, 0);
            const char* responseTextCol = reinterpret_cast<const char*>(sqlite3_column_text(checkStmt, 1));
            existingJson = responseTextCol ? responseTextCol : "";
        }
        sqlite3_finalize(checkStmt);
        
        if (entryId == -1) {
            // Entry doesn't exist
            return;
        }
        
        // Parse existing responses and append new one if not a duplicate
        auto responses = JsonToResponses(existingJson);
        
        // Check if this response already exists
        bool isDuplicate = false;
        for (const auto& existing : responses) {
            if (existing == responseText) {
                isDuplicate = true;
                break;
            }
        }
        
        if (!isDuplicate) {
            responses.push_back(responseText);
            std::string updatedJson = ResponsesToJson(responses);
            
            const char* updateSql = "UPDATE blacklist SET response_text = ? WHERE id = ?;";
            sqlite3_stmt* updateStmt = nullptr;
            
            if (sqlite3_prepare_v2(db_, updateSql, -1, &updateStmt, nullptr) == SQLITE_OK) {
                sqlite3_bind_text(updateStmt, 1, updatedJson.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_int64(updateStmt, 2, entryId);
                
                if (sqlite3_step(updateStmt) == SQLITE_DONE) {
                    spdlog::debug("[DialogueDB] Runtime enriched blacklist entry {} - now has {} responses", targetEditorID, responses.size());
                }
                sqlite3_finalize(updateStmt);
            }
        }
    }

    void Database::EnrichBlacklistEntryAtRuntime(BlacklistTarget targetType, const std::string& targetEditorID, const std::vector<std::string>& allResponses)
    {
        if (targetEditorID.empty() || allResponses.empty()) return;
        
        std::lock_guard<std::recursive_mutex> lock(dbMutex_);
        if (!db_) return;
        
        // Check if entry exists
        const char* checkSql = "SELECT id, response_text FROM blacklist WHERE target_type = ? AND target_editorid = ? LIMIT 1;";
        sqlite3_stmt* checkStmt = nullptr;
        
        if (sqlite3_prepare_v2(db_, checkSql, -1, &checkStmt, nullptr) != SQLITE_OK) {
            return;
        }
        
        sqlite3_bind_int(checkStmt, 1, static_cast<int>(targetType));
        sqlite3_bind_text(checkStmt, 2, targetEditorID.c_str(), -1, SQLITE_TRANSIENT);
        
        int64_t entryId = -1;
        std::string existingJson;
        
        if (sqlite3_step(checkStmt) == SQLITE_ROW) {
            entryId = sqlite3_column_int64(checkStmt, 0);
            const char* responseTextCol = reinterpret_cast<const char*>(sqlite3_column_text(checkStmt, 1));
            existingJson = responseTextCol ? responseTextCol : "";
        }
        sqlite3_finalize(checkStmt);
        
        if (entryId == -1) {
            // Entry doesn't exist
            return;
        }
        
        // Parse existing responses
        auto responses = JsonToResponses(existingJson);
        
        // Add all new responses that aren't duplicates
        int addedCount = 0;
        for (const auto& newResponse : allResponses) {
            if (newResponse.empty()) continue;
            
            bool isDuplicate = false;
            for (const auto& existing : responses) {
                if (existing == newResponse) {
                    isDuplicate = true;
                    break;
                }
            }
            
            if (!isDuplicate) {
                responses.push_back(newResponse);
                addedCount++;
            }
        }
        
        if (addedCount > 0) {
            std::string updatedJson = ResponsesToJson(responses);
            
            const char* updateSql = "UPDATE blacklist SET response_text = ? WHERE id = ?;";
            sqlite3_stmt* updateStmt = nullptr;
            
            if (sqlite3_prepare_v2(db_, updateSql, -1, &updateStmt, nullptr) == SQLITE_OK) {
                sqlite3_bind_text(updateStmt, 1, updatedJson.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_int64(updateStmt, 2, entryId);
                
                if (sqlite3_step(updateStmt) == SQLITE_DONE) {
                    spdlog::info("[DialogueDB] Runtime enriched blacklist entry {} - added {} responses (total: {})", 
                        targetEditorID, addedCount, responses.size());
                }
                sqlite3_finalize(updateStmt);
            }
        }
    }

    void Database::MarkTopicsAsSkyrimNetBlockable(const std::vector<uint32_t>& topicInfoFormIDs)
    {
        if (topicInfoFormIDs.empty()) return;
        
        std::lock_guard<std::recursive_mutex> lock(dbMutex_);
        if (!db_) return;
        
        // Build UPDATE statement with IN clause
        // IMPORTANT: Exclude scenes - they should NEVER be SkyrimNet blockable
        std::string sql = "UPDATE dialogue_log SET skyrimnet_blockable = 1 WHERE is_scene = 0 AND topicinfo_formid IN (";
        for (size_t i = 0; i < topicInfoFormIDs.size(); ++i) {
            if (i > 0) sql += ",";
            sql += "?";
        }
        sql += ");";
        
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            spdlog::error("[DialogueDB] Failed to prepare skyrimnet_blockable UPDATE: {}", sqlite3_errmsg(db_));
            return;
        }
        
        // Bind all FormIDs
        for (size_t i = 0; i < topicInfoFormIDs.size(); ++i) {
            sqlite3_bind_int64(stmt, static_cast<int>(i + 1), topicInfoFormIDs[i]);
        }
        
        int result = sqlite3_step(stmt);
        if (result == SQLITE_DONE) {
            int changed = sqlite3_changes(db_);
            if (changed > 0) {
                spdlog::info("[DialogueDB] Marked {} dialogue entries as SkyrimNet blockable", changed);
            }
        } else {
            spdlog::error("[DialogueDB] Failed to update skyrimnet_blockable: {}", sqlite3_errmsg(db_));
        }
        
        sqlite3_finalize(stmt);
    }
}
