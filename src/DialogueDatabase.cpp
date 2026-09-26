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
    static std::unique_ptr<Database> g_database;

    Database* GetDatabase()
    {
        if (!g_database) {
            g_database = std::make_unique<Database>();
        }
        return g_database.get();
    }

    Database::Database()
    {
        // Initialize last flush time to current time
        lastFlushTime_ = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
    }

    Database::~Database()
    {
        Close();
    }

    bool Database::Initialize(const std::string& dbPath)
    {
        std::lock_guard<std::recursive_mutex> lock(dbMutex_);

        // Create directory if it doesn't exist
        std::filesystem::path path(dbPath);
        std::filesystem::create_directories(path.parent_path());

        // Open database
        int rc = sqlite3_open(dbPath.c_str(), &db_);
        if (rc != SQLITE_OK) {
            spdlog::error("[DialogueDB] Failed to open database: {}", sqlite3_errmsg(db_));
            return false;
        }

        // Enable WAL mode for better concurrency
        char* errMsg = nullptr;
        rc = sqlite3_exec(db_, "PRAGMA journal_mode=WAL;", nullptr, nullptr, &errMsg);
        if (rc != SQLITE_OK) {
            spdlog::warn("[DialogueDB] Failed to enable WAL mode: {}", errMsg);
            sqlite3_free(errMsg);
        }

        // Create tables and indexes
        if (!CreateTables()) {
            spdlog::error("[DialogueDB] Failed to create tables");
            return false;
        }

        // Update schema for existing databases (add missing columns)
        if (!UpdateSchema()) {
            spdlog::error("[DialogueDB] Failed to update schema");
            return false;
        }

        if (!CreateIndexes()) {
            spdlog::error("[DialogueDB] Failed to create indexes");
            return false;
        }

        PrepareStatements();

        // Initialize dialogue logger
        DialogueLogger::Initialize();

        spdlog::info("Database initialized: {}", dbPath);
        return true;
    }

    void Database::Close()
    {
        // Drain the queue first, with no outer lock held. FlushQueue → ProcessQueue
        // acquires queueMutex_ and dbMutex_ only briefly and never simultaneously
        // (it swaps pendingEntries_ out under queueMutex_, then inserts under
        // dbMutex_). Holding dbMutex_ across FlushQueue would break that invariant
        // and AB-BA deadlock with a concurrent LogDialogue that holds queueMutex_
        // while its ProcessQueue waits on dbMutex_.
        FlushQueue();

        std::lock_guard<std::recursive_mutex> lock(dbMutex_);
        FinalizeStatements();
        DialogueLogger::Shutdown();

        if (db_) {
            sqlite3_close(db_);
            db_ = nullptr;
        }
    }

    bool Database::CreateTables()
    {
        // Create tables if they don't exist (preserves existing data)
        const char* createDialogueTable = R"(
            CREATE TABLE IF NOT EXISTS dialogue_log (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                timestamp INTEGER NOT NULL,
                
                speaker_name TEXT,
                speaker_formid INTEGER,
                speaker_base_formid INTEGER,
                
                topic_editorid TEXT,
                topic_formid INTEGER NOT NULL,
                topic_subtype INTEGER NOT NULL,
                topic_subtype_name TEXT,
                
                quest_editorid TEXT,
                quest_formid INTEGER,
                quest_name TEXT,
                
                scene_editorid TEXT,
                
                topicinfo_formid INTEGER,
                
                response_text TEXT,
                voice_filepath TEXT,
                responses_json TEXT,
                
                blocked_status INTEGER NOT NULL,
                is_scene INTEGER NOT NULL,
                is_bard_song INTEGER NOT NULL,
                is_hardcoded_scene INTEGER NOT NULL,
                source_plugin TEXT,
                topic_source_plugin TEXT,
                skyrimnet_blockable INTEGER DEFAULT 0
            );
        )";

        const char* createBlacklistTable = R"(
            CREATE TABLE IF NOT EXISTS blacklist (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                target_type INTEGER NOT NULL,
                target_formid INTEGER,
                target_editorid TEXT,
                block_type INTEGER NOT NULL,
                added_timestamp INTEGER NOT NULL,
                notes TEXT,
                response_text TEXT,
                subtype INTEGER DEFAULT 0,
                subtype_name TEXT,
                filter_category TEXT DEFAULT 'Blacklist',
                block_skyrimnet INTEGER DEFAULT 1,
                source_plugin TEXT,
                quest_editorid TEXT,
                actor_filter_formids TEXT DEFAULT '[]',
                actor_filter_names TEXT DEFAULT '[]',
                faction_filter_editorids TEXT DEFAULT '[]',
                UNIQUE(target_type, target_formid, target_editorid)
            );
        )";

        const char* createWhitelistTable = R"(
            CREATE TABLE IF NOT EXISTS whitelist (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                target_type INTEGER NOT NULL,
                target_formid INTEGER,
                target_editorid TEXT,
                block_type INTEGER NOT NULL,
                added_timestamp INTEGER NOT NULL,
                notes TEXT,
                response_text TEXT,
                subtype INTEGER DEFAULT 0,
                subtype_name TEXT,
                filter_category TEXT DEFAULT 'Whitelist',
                block_skyrimnet INTEGER DEFAULT 1,
                source_plugin TEXT,
                quest_editorid TEXT,
                actor_filter_formids TEXT DEFAULT '[]',
                actor_filter_names TEXT DEFAULT '[]',
                faction_filter_editorids TEXT DEFAULT '[]',
                UNIQUE(target_type, target_formid, target_editorid)
            );
        )";

        // Small key/value store used for one-shot flags ("has the X auto-import
        // ever run?"). Decoupled from row counts so that removing scenes never
        // triggers a re-import.
        const char* createMetaTable = R"(
            CREATE TABLE IF NOT EXISTS meta (
                key TEXT PRIMARY KEY,
                value TEXT
            );
        )";

        // Settings table removed - settings now stored in STFU.ini

        char* errMsg = nullptr;
        int rc = sqlite3_exec(db_, createDialogueTable, nullptr, nullptr, &errMsg);
        if (rc != SQLITE_OK) {
            spdlog::error("[DialogueDB] Failed to create dialogue_log table: {}", errMsg);
            sqlite3_free(errMsg);
            return false;
        }

        rc = sqlite3_exec(db_, createBlacklistTable, nullptr, nullptr, &errMsg);
        if (rc != SQLITE_OK) {
            spdlog::error("[DialogueDB] Failed to create blacklist table: {}", errMsg);
            sqlite3_free(errMsg);
            return false;
        }

        rc = sqlite3_exec(db_, createWhitelistTable, nullptr, nullptr, &errMsg);
        if (rc != SQLITE_OK) {
            spdlog::error("[DialogueDB] Failed to create whitelist table: {}", errMsg);
            sqlite3_free(errMsg);
            return false;
        }

        rc = sqlite3_exec(db_, createMetaTable, nullptr, nullptr, &errMsg);
        if (rc != SQLITE_OK) {
            spdlog::error("[DialogueDB] Failed to create meta table: {}", errMsg);
            sqlite3_free(errMsg);
            return false;
        }

        spdlog::info("Database tables created (settings moved to INI)");
        return true;
    }

    bool Database::UpdateSchema()
    {
        // Add missing columns to existing databases
        // SQLite's ALTER TABLE ADD COLUMN will fail if column already exists, so we check first
        
        auto columnExists = [this](const char* table, const char* column) -> bool {
            std::string query = "PRAGMA table_info(" + std::string(table) + ");";
            sqlite3_stmt* stmt = nullptr;
            if (sqlite3_prepare_v2(db_, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
                return false;
            }
            
            bool found = false;
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                const char* colName = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
                if (colName && std::string(colName) == column) {
                    found = true;
                    break;
                }
            }
            sqlite3_finalize(stmt);
            return found;
        };
        
        char* errMsg = nullptr;
        
        // Add missing columns to dialogue_log
        if (!columnExists("dialogue_log", "responses_json")) {
            spdlog::info("[DialogueDB] Adding responses_json column to dialogue_log");
            int rc = sqlite3_exec(db_, "ALTER TABLE dialogue_log ADD COLUMN responses_json TEXT;", nullptr, nullptr, &errMsg);
            if (rc != SQLITE_OK) {
                spdlog::error("[DialogueDB] Failed to add responses_json column: {}", errMsg);
                sqlite3_free(errMsg);
                return false;
            }
        }
        
        if (!columnExists("dialogue_log", "source_plugin")) {
            spdlog::info("[DialogueDB] Adding source_plugin column to dialogue_log");
            int rc = sqlite3_exec(db_, "ALTER TABLE dialogue_log ADD COLUMN source_plugin TEXT;", nullptr, nullptr, &errMsg);
            if (rc != SQLITE_OK) {
                spdlog::error("[DialogueDB] Failed to add source_plugin column: {}", errMsg);
                sqlite3_free(errMsg);
                return false;
            }
        }
        
        if (!columnExists("dialogue_log", "topic_source_plugin")) {
            spdlog::info("[DialogueDB] Adding topic_source_plugin column to dialogue_log");
            int rc = sqlite3_exec(db_, "ALTER TABLE dialogue_log ADD COLUMN topic_source_plugin TEXT;", nullptr, nullptr, &errMsg);
            if (rc != SQLITE_OK) {
                spdlog::error("[DialogueDB] Failed to add topic_source_plugin column: {}", errMsg);
                sqlite3_free(errMsg);
                return false;
            }
        }
        
        if (!columnExists("dialogue_log", "skyrimnet_blockable")) {
            spdlog::info("[DialogueDB] Adding skyrimnet_blockable column to dialogue_log");
            int rc = sqlite3_exec(db_, "ALTER TABLE dialogue_log ADD COLUMN skyrimnet_blockable INTEGER DEFAULT 0;", nullptr, nullptr, &errMsg);
            if (rc != SQLITE_OK) {
                spdlog::error("[DialogueDB] Failed to add skyrimnet_blockable column: {}", errMsg);
                sqlite3_free(errMsg);
                return false;
            }
        }
        
        // Add missing columns to blacklist
        if (!columnExists("blacklist", "source_plugin")) {
            spdlog::info("[DialogueDB] Adding source_plugin column to blacklist");
            int rc = sqlite3_exec(db_, "ALTER TABLE blacklist ADD COLUMN source_plugin TEXT;", nullptr, nullptr, &errMsg);
            if (rc != SQLITE_OK) {
                spdlog::error("[DialogueDB] Failed to add source_plugin column to blacklist: {}", errMsg);
                sqlite3_free(errMsg);
                return false;
            }
        }
        
        if (!columnExists("blacklist", "quest_editorid")) {
            spdlog::info("[DialogueDB] Adding quest_editorid column to blacklist");
            int rc = sqlite3_exec(db_, "ALTER TABLE blacklist ADD COLUMN quest_editorid TEXT;", nullptr, nullptr, &errMsg);
            if (rc != SQLITE_OK) {
                spdlog::error("[DialogueDB] Failed to add quest_editorid column to blacklist: {}", errMsg);
                sqlite3_free(errMsg);
                return false;
            }
        }
        
        // Add missing columns to whitelist
        if (!columnExists("whitelist", "quest_editorid")) {
            spdlog::info("[DialogueDB] Adding quest_editorid column to whitelist");
            int rc = sqlite3_exec(db_, "ALTER TABLE whitelist ADD COLUMN quest_editorid TEXT;", nullptr, nullptr, &errMsg);
            if (rc != SQLITE_OK) {
                spdlog::error("[DialogueDB] Failed to add quest_editorid column to whitelist: {}", errMsg);
                sqlite3_free(errMsg);
                return false;
            }
        }
        
        // Add faction filtering column to blacklist
        if (!columnExists("blacklist", "faction_filter_editorids")) {
            spdlog::info("[DialogueDB] Adding faction_filter_editorids column to blacklist");
            int rc = sqlite3_exec(db_, "ALTER TABLE blacklist ADD COLUMN faction_filter_editorids TEXT DEFAULT '[]';", nullptr, nullptr, &errMsg);
            if (rc != SQLITE_OK) {
                spdlog::error("[DialogueDB] Failed to add faction_filter_editorids column to blacklist: {}", errMsg);
                sqlite3_free(errMsg);
                return false;
            }
        }
        
        // Add faction filtering column to whitelist
        if (!columnExists("whitelist", "faction_filter_editorids")) {
            spdlog::info("[DialogueDB] Adding faction_filter_editorids column to whitelist");
            int rc = sqlite3_exec(db_, "ALTER TABLE whitelist ADD COLUMN faction_filter_editorids TEXT DEFAULT '[]';", nullptr, nullptr, &errMsg);
            if (rc != SQLITE_OK) {
                spdlog::error("[DialogueDB] Failed to add faction_filter_editorids column to whitelist: {}", errMsg);
                sqlite3_free(errMsg);
                return false;
            }
        }
        
        spdlog::info("[DialogueDB] Schema update complete");
        return true;
    }

    bool Database::CreateIndexes()
    {
        const char* indexes[] = {
            "CREATE INDEX IF NOT EXISTS idx_timestamp ON dialogue_log(timestamp);",
            "CREATE INDEX IF NOT EXISTS idx_topic_formid ON dialogue_log(topic_formid);",
            "CREATE INDEX IF NOT EXISTS idx_topic_subtype ON dialogue_log(topic_subtype);",
            "CREATE INDEX IF NOT EXISTS idx_quest_formid ON dialogue_log(quest_formid);",
            "CREATE INDEX IF NOT EXISTS idx_blocked_status ON dialogue_log(blocked_status);",
            "CREATE INDEX IF NOT EXISTS idx_speaker_name ON dialogue_log(speaker_name);",
            "CREATE INDEX IF NOT EXISTS idx_blacklist_target ON blacklist(target_type, target_formid, target_editorid);",
            "CREATE INDEX IF NOT EXISTS idx_blacklist_filter_category ON blacklist(filter_category);"
        };

        for (const auto& indexSQL : indexes) {
            char* errMsg = nullptr;
            int rc = sqlite3_exec(db_, indexSQL, nullptr, nullptr, &errMsg);
            if (rc != SQLITE_OK) {
                spdlog::error("[DialogueDB] Failed to create index: {}", errMsg);
                sqlite3_free(errMsg);
                return false;
            }
        }

        return true;
    }

    void Database::PrepareStatements()
    {
        const char* insertDialogueSQL = R"(
            INSERT INTO dialogue_log (
                timestamp, speaker_name, speaker_formid, speaker_base_formid,
                topic_editorid, topic_formid, topic_subtype, topic_subtype_name,
                quest_editorid, quest_formid, quest_name, scene_editorid, topicinfo_formid,
                response_text, voice_filepath, responses_json, blocked_status,
                is_scene, is_bard_song, is_hardcoded_scene, source_plugin, topic_source_plugin, skyrimnet_blockable
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);
        )";

        const char* insertBlacklistSQL = R"(
            INSERT INTO blacklist (
                target_type, target_formid, target_editorid,
                block_type, added_timestamp, notes, response_text, subtype, subtype_name,
                filter_category, block_skyrimnet, source_plugin, quest_editorid,
                actor_filter_formids, actor_filter_names, faction_filter_editorids
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);
        )";

        const char* insertWhitelistSQL = R"(
            INSERT INTO whitelist (
                target_type, target_formid, target_editorid,
                block_type, added_timestamp, notes, response_text, subtype, subtype_name,
                filter_category, block_skyrimnet, source_plugin, quest_editorid,
                actor_filter_formids, actor_filter_names, faction_filter_editorids
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);
        )";

        if (sqlite3_prepare_v2(db_, insertDialogueSQL, -1, &insertDialogueStmt_, nullptr) != SQLITE_OK) {
            spdlog::error("[DialogueDB] Failed to prepare insertDialogue statement: {}", sqlite3_errmsg(db_));
        }
        
        if (sqlite3_prepare_v2(db_, insertBlacklistSQL, -1, &insertBlacklistStmt_, nullptr) != SQLITE_OK) {
            spdlog::error("[DialogueDB] Failed to prepare insertBlacklist statement: {}", sqlite3_errmsg(db_));
        }
        
        if (sqlite3_prepare_v2(db_, insertWhitelistSQL, -1, &insertWhitelistStmt_, nullptr) != SQLITE_OK) {
            spdlog::error("[DialogueDB] Failed to prepare insertWhitelist statement: {}", sqlite3_errmsg(db_));
        }
    }

    void Database::FinalizeStatements()
    {
        if (insertDialogueStmt_) {
            sqlite3_finalize(insertDialogueStmt_);
            insertDialogueStmt_ = nullptr;
        }
        if (insertBlacklistStmt_) {
            sqlite3_finalize(insertBlacklistStmt_);
            insertBlacklistStmt_ = nullptr;
        }
        if (insertWhitelistStmt_) {
            sqlite3_finalize(insertWhitelistStmt_);
            insertWhitelistStmt_ = nullptr;
        }
    }

    // ========================================
    // HELPER FUNCTIONS
    // ========================================
    
    int Database::GetColumnIndex(sqlite3_stmt* stmt, const char* columnName)
    {
        int columnCount = sqlite3_column_count(stmt);
        for (int i = 0; i < columnCount; ++i) {
            const char* name = sqlite3_column_name(stmt, i);
            if (name && std::strcmp(name, columnName) == 0) {
                return i;
            }
        }
        spdlog::error("[DialogueDB] Column '{}' not found in query result", columnName);
        return -1;  // Column not found
    }

    std::string Database::GetMetaValue(const std::string& key)
    {
        std::lock_guard<std::recursive_mutex> lock(dbMutex_);
        std::string result;
        if (!db_) return result;

        const char* sql = "SELECT value FROM meta WHERE key = ? LIMIT 1;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                const char* val = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                if (val) result = val;
            }
            sqlite3_finalize(stmt);
        }
        return result;
    }

    void Database::SetMetaValue(const std::string& key, const std::string& value)
    {
        std::lock_guard<std::recursive_mutex> lock(dbMutex_);
        if (!db_) return;

        const char* sql = "INSERT OR REPLACE INTO meta (key, value) VALUES (?, ?);";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 2, value.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }

    bool Database::GetMetaFlag(const std::string& key)
    {
        return GetMetaValue(key) == "1";
    }

    void Database::SetMetaFlag(const std::string& key, bool value)
    {
        SetMetaValue(key, value ? "1" : "0");
    }
}
