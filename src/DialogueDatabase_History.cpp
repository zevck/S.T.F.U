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
    void Database::LogDialogue(const DialogueEntry& entry)
    {
        // Convert BlockedStatus to string for logging
        std::string statusStr;
        switch (entry.blockedStatus) {
            case BlockedStatus::Normal:
            case BlockedStatus::ToggledOff:
                statusStr = "ALLOWED";
                break;
            case BlockedStatus::SoftBlock:
                statusStr = "SOFT BLOCK";
                break;
            case BlockedStatus::HardBlock:
                statusStr = "HARD BLOCK";
                break;
            case BlockedStatus::SkyrimNetBlock:
                statusStr = "SKYRIMNET BLOCK";
                break;
            case BlockedStatus::FilteredByConfig:
                statusStr = "CONFIG FILTERED";
                break;
            default:
                statusStr = "UNKNOWN";
                break;
        }
        
        // Log to text file for YAML export
        DialogueLogger::LogEntry(
            entry.timestamp,
            statusStr,
            entry.topicSubtypeName,
            entry.questEditorID,
            entry.topicFormID,
            entry.topicEditorID,
            entry.speakerName,
            entry.responseText,
            entry.topicSourcePlugin,
            entry.skyrimNetBlockable
        );
        
        bool shouldFlush = false;
        {
            std::lock_guard<std::mutex> lock(queueMutex_);
            pendingEntries_.push(entry);

            // Get current time for flush decision
            auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count();

            // Auto-flush if either:
            // 1. Queue has reached batch size (200 entries)
            // 2. FLUSH_INTERVAL_MS (5 seconds) has passed since last flush
            bool sizeLimitReached = pendingEntries_.size() >= BATCH_SIZE;
            bool timeLimitReached = (now - lastFlushTime_ >= FLUSH_INTERVAL_MS);
            shouldFlush = sizeLimitReached || timeLimitReached;

            if (shouldFlush) {
                spdlog::trace("[DialogueDB] Auto-flushing: size={}, time={}, queue={}",
                    sizeLimitReached, timeLimitReached, pendingEntries_.size());
                lastFlushTime_ = now;
            }
        }

        // ProcessQueue runs outside queueMutex_ — it re-acquires queueMutex_ briefly
        // to swap the pending entries into a local queue, then processes that under
        // dbMutex_ only. This guarantees queueMutex_ and dbMutex_ are never held
        // simultaneously, eliminating any AB-BA deadlock between the two.
        if (shouldFlush) {
            ProcessQueue();
        }
    }

    void Database::ProcessQueue()
    {
        // Cheap pre-check: if the DB is already torn down (Close ran), don't
        // swap pendingEntries_ into a local — otherwise those entries would be
        // silently dropped when we early-return below. Held briefly, then
        // released before touching queueMutex_ so we keep the "queueMutex_ and
        // dbMutex_ never held simultaneously" invariant.
        {
            std::lock_guard<std::recursive_mutex> dbCheck(dbMutex_);
            if (!db_ || !insertDialogueStmt_) return;
        }

        // Hand off the pending entries under queueMutex_, then release it before
        // taking dbMutex_. queueMutex_ and dbMutex_ must never be held at the
        // same time — see Close() and LogDialogue() comments.
        std::queue<DialogueEntry> localQueue;
        {
            std::lock_guard<std::mutex> queueLock(queueMutex_);
            std::swap(pendingEntries_, localQueue);
        }

        if (localQueue.empty()) return;

        std::lock_guard<std::recursive_mutex> lock(dbMutex_);

        // Close can race in between the pre-check and here. If db_ has been
        // nulled, the swapped-out entries get dropped — acceptable rare loss
        // during shutdown, and safe because we hold dbMutex_ so sqlite state
        // can't disappear underneath us.
        if (!db_ || !insertDialogueStmt_) return;

        size_t entriesCount = localQueue.size();
        spdlog::debug("[DialogueDB] Flushing {} queued dialogue entries", entriesCount);

        // Begin transaction for batch insert
        sqlite3_exec(db_, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);

        while (!localQueue.empty()) {
            const auto& entry = localQueue.front();

            sqlite3_reset(insertDialogueStmt_);
            sqlite3_bind_int64(insertDialogueStmt_, 1, entry.timestamp);
            sqlite3_bind_text(insertDialogueStmt_, 2, entry.speakerName.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(insertDialogueStmt_, 3, entry.speakerFormID);
            sqlite3_bind_int(insertDialogueStmt_, 4, entry.speakerBaseFormID);
            sqlite3_bind_text(insertDialogueStmt_, 5, entry.topicEditorID.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(insertDialogueStmt_, 6, entry.topicFormID);
            sqlite3_bind_int(insertDialogueStmt_, 7, entry.topicSubtype);
            sqlite3_bind_text(insertDialogueStmt_, 8, entry.topicSubtypeName.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(insertDialogueStmt_, 9, entry.questEditorID.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(insertDialogueStmt_, 10, entry.questFormID);
            sqlite3_bind_text(insertDialogueStmt_, 11, entry.questName.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(insertDialogueStmt_, 12, entry.sceneEditorID.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(insertDialogueStmt_, 13, entry.topicInfoFormID);
            sqlite3_bind_text(insertDialogueStmt_, 14, entry.responseText.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(insertDialogueStmt_, 15, entry.voiceFilepath.c_str(), -1, SQLITE_TRANSIENT);
            
            // Serialize allResponses to JSON
            std::string responsesJson = ResponsesToJson(entry.allResponses);
            size_t previewLen = responsesJson.length() < 100 ? responsesJson.length() : 100;
            spdlog::debug("[DialogueDB] Writing responses_json for entry: {} responses, {} chars: {}", 
                entry.allResponses.size(), 
                responsesJson.length(),
                responsesJson.substr(0, previewLen));
            sqlite3_bind_text(insertDialogueStmt_, 16, responsesJson.c_str(), -1, SQLITE_TRANSIENT);
            
            sqlite3_bind_int(insertDialogueStmt_, 17, static_cast<int>(entry.blockedStatus));
            sqlite3_bind_int(insertDialogueStmt_, 18, entry.isScene ? 1 : 0);
            sqlite3_bind_int(insertDialogueStmt_, 19, entry.isBardSong ? 1 : 0);
            sqlite3_bind_int(insertDialogueStmt_, 20, entry.isHardcodedScene ? 1 : 0);
            sqlite3_bind_text(insertDialogueStmt_, 21, entry.sourcePlugin.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(insertDialogueStmt_, 22, entry.topicSourcePlugin.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(insertDialogueStmt_, 23, entry.skyrimNetBlockable ? 1 : 0);

            int stepResult = sqlite3_step(insertDialogueStmt_);
            if (stepResult != SQLITE_DONE) {
                spdlog::error("[DialogueDB] Failed to insert dialogue entry for TopicInfo 0x{:08X}: {} (code: {})", 
                    entry.topicInfoFormID, sqlite3_errmsg(db_), stepResult);
            } else {
                spdlog::trace("[DialogueDB] Successfully inserted dialogue entry for TopicInfo 0x{:08X}", entry.topicInfoFormID);
            }

            localQueue.pop();
        }

        // Commit transaction
        sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, nullptr);
        
        spdlog::debug("[DialogueDB] ProcessQueue complete, flushed {} entries to database", entriesCount);
        
        // Maintain history limit: delete oldest entries if count exceeds 100
        const int HISTORY_LIMIT = 100;
        const char* countSql = "SELECT COUNT(*) FROM dialogue_log;";
        sqlite3_stmt* countStmt = nullptr;
        
        if (sqlite3_prepare_v2(db_, countSql, -1, &countStmt, nullptr) == SQLITE_OK) {
            if (sqlite3_step(countStmt) == SQLITE_ROW) {
                int totalEntries = sqlite3_column_int(countStmt, 0);
                
                if (totalEntries > HISTORY_LIMIT) {
                    int entriesToDelete = totalEntries - HISTORY_LIMIT;
                    spdlog::info("[DialogueDB] History limit exceeded ({}/{}), deleting {} oldest entries", 
                        totalEntries, HISTORY_LIMIT, entriesToDelete);
                    
                    // Delete oldest entries beyond the limit
                    const char* deleteSql = "DELETE FROM dialogue_log WHERE id IN (SELECT id FROM dialogue_log ORDER BY timestamp ASC LIMIT ?);";
                    sqlite3_stmt* deleteStmt = nullptr;
                    
                    if (sqlite3_prepare_v2(db_, deleteSql, -1, &deleteStmt, nullptr) == SQLITE_OK) {
                        sqlite3_bind_int(deleteStmt, 1, entriesToDelete);
                        
                        if (sqlite3_step(deleteStmt) == SQLITE_DONE) {
                            spdlog::info("[DialogueDB] Successfully deleted {} oldest entries, new count: {}", 
                                entriesToDelete, HISTORY_LIMIT);
                        } else {
                            spdlog::error("[DialogueDB] Failed to delete old entries: {}", sqlite3_errmsg(db_));
                        }
                        
                        sqlite3_finalize(deleteStmt);
                    }
                }
            }
            sqlite3_finalize(countStmt);
        }
        
        // Defer UI refresh to the main thread via SKSE task interface so it
        // runs after dbMutex_ is released. Invoking PrismaUI from inside a DB
        // lock — especially from the audio thread under VR's threading model —
        // can deadlock once PrismaUI VR support is active.
        if (auto* tasks = SKSE::GetTaskInterface()) {
            tasks->AddTask([] {
                if (PrismaUIMenu::IsOpen()) {
                    PrismaUIMenu::SendHistoryData();
                }
            });
        }
    }

    void Database::FlushQueue()
    {
        // ProcessQueue handles its own queueMutex_/dbMutex_ acquisition without
        // overlap. Do not hold queueMutex_ across this call.
        ProcessQueue();

        std::lock_guard<std::mutex> lock(queueMutex_);
        lastFlushTime_ = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
    }

    // ========================================
    // QUERY
    // ========================================

    std::vector<DialogueEntry> Database::GetRecentDialogue(int limit)
    {
        std::lock_guard<std::recursive_mutex> lock(dbMutex_);
        std::vector<DialogueEntry> results;

        if (!db_) return results;

        std::string sql = "SELECT * FROM dialogue_log ORDER BY timestamp DESC LIMIT ?;";
        sqlite3_stmt* stmt = nullptr;

        if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            spdlog::error("[DialogueDB] Failed to prepare query: {}", sqlite3_errmsg(db_));
            return results;
        }

        sqlite3_bind_int(stmt, 1, limit);

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            DialogueEntry entry;
            
            // Use column names instead of hardcoded indices - resilient to schema changes
            int idx;
            entry.id = sqlite3_column_int64(stmt, GetColumnIndex(stmt, "id"));
            entry.timestamp = sqlite3_column_int64(stmt, GetColumnIndex(stmt, "timestamp"));
            
            if ((idx = GetColumnIndex(stmt, "speaker_name")) >= 0)
                entry.speakerName = reinterpret_cast<const char*>(sqlite3_column_text(stmt, idx));
            if ((idx = GetColumnIndex(stmt, "speaker_formid")) >= 0)
                entry.speakerFormID = sqlite3_column_int(stmt, idx);
            if ((idx = GetColumnIndex(stmt, "speaker_base_formid")) >= 0)
                entry.speakerBaseFormID = sqlite3_column_int(stmt, idx);
            
            if ((idx = GetColumnIndex(stmt, "topic_editorid")) >= 0)
                entry.topicEditorID = reinterpret_cast<const char*>(sqlite3_column_text(stmt, idx));
            if ((idx = GetColumnIndex(stmt, "topic_formid")) >= 0)
                entry.topicFormID = sqlite3_column_int(stmt, idx);
            if ((idx = GetColumnIndex(stmt, "topic_subtype")) >= 0)
                entry.topicSubtype = sqlite3_column_int(stmt, idx);
            if ((idx = GetColumnIndex(stmt, "topic_subtype_name")) >= 0)
                entry.topicSubtypeName = reinterpret_cast<const char*>(sqlite3_column_text(stmt, idx));
            
            if ((idx = GetColumnIndex(stmt, "quest_editorid")) >= 0)
                entry.questEditorID = reinterpret_cast<const char*>(sqlite3_column_text(stmt, idx));
            if ((idx = GetColumnIndex(stmt, "quest_formid")) >= 0)
                entry.questFormID = sqlite3_column_int(stmt, idx);
            if ((idx = GetColumnIndex(stmt, "quest_name")) >= 0)
                entry.questName = reinterpret_cast<const char*>(sqlite3_column_text(stmt, idx));
            
            if ((idx = GetColumnIndex(stmt, "scene_editorid")) >= 0)
                entry.sceneEditorID = reinterpret_cast<const char*>(sqlite3_column_text(stmt, idx));
            
            if ((idx = GetColumnIndex(stmt, "topicinfo_formid")) >= 0)
                entry.topicInfoFormID = sqlite3_column_int(stmt, idx);
            
            if ((idx = GetColumnIndex(stmt, "response_text")) >= 0)
                entry.responseText = reinterpret_cast<const char*>(sqlite3_column_text(stmt, idx));
            if ((idx = GetColumnIndex(stmt, "voice_filepath")) >= 0)
                entry.voiceFilepath = reinterpret_cast<const char*>(sqlite3_column_text(stmt, idx));
            
            // Deserialize responses_json - now resilient to column reordering
            if ((idx = GetColumnIndex(stmt, "responses_json")) >= 0) {
                const char* responsesJson = reinterpret_cast<const char*>(sqlite3_column_text(stmt, idx));
                if (responsesJson) {
                    std::string jsonStr(responsesJson);
                    entry.allResponses = JsonToResponses(jsonStr);
                }
            }
            
            if ((idx = GetColumnIndex(stmt, "blocked_status")) >= 0)
                entry.blockedStatus = static_cast<BlockedStatus>(sqlite3_column_int(stmt, idx));
            if ((idx = GetColumnIndex(stmt, "is_scene")) >= 0)
                entry.isScene = sqlite3_column_int(stmt, idx) != 0;
            if ((idx = GetColumnIndex(stmt, "is_bard_song")) >= 0)
                entry.isBardSong = sqlite3_column_int(stmt, idx) != 0;
            if ((idx = GetColumnIndex(stmt, "is_hardcoded_scene")) >= 0)
                entry.isHardcodedScene = sqlite3_column_int(stmt, idx) != 0;
            
            if ((idx = GetColumnIndex(stmt, "source_plugin")) >= 0) {
                const char* sourcePlugin = reinterpret_cast<const char*>(sqlite3_column_text(stmt, idx));
                entry.sourcePlugin = sourcePlugin ? sourcePlugin : "";
            }
            
            if ((idx = GetColumnIndex(stmt, "topic_source_plugin")) >= 0) {
                const char* topicSourcePlugin = reinterpret_cast<const char*>(sqlite3_column_text(stmt, idx));
                entry.topicSourcePlugin = topicSourcePlugin ? topicSourcePlugin : "";
            }
            
            if ((idx = GetColumnIndex(stmt, "skyrimnet_blockable")) >= 0)
                entry.skyrimNetBlockable = sqlite3_column_int(stmt, idx) != 0;

            results.push_back(entry);
        }

        sqlite3_finalize(stmt);
        return results;
    }

    int Database::DeleteDialogueEntriesBatch(const std::vector<int64_t>& ids)
    {
        if (ids.empty()) return 0;
        
        std::lock_guard<std::recursive_mutex> lock(dbMutex_);
        
        if (!db_) return 0;
        
        // Begin transaction directly (already have lock)
        sqlite3_exec(db_, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);
        
        int deletedCount = 0;
        const char* sql = "DELETE FROM dialogue_log WHERE id = ?;";
        sqlite3_stmt* stmt = nullptr;
        
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            spdlog::error("[DialogueDB] Failed to prepare batch delete statement: {}", sqlite3_errmsg(db_));
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
            spdlog::info("[DialogueDB] Deleted {} dialogue entries in batch", deletedCount);
        }
        
        return deletedCount;
    }

}
