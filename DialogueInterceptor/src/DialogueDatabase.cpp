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
    
    // Helper function to check if actor matches filter (ESL-safe)
    // Empty filter vectors = matches all actors
    // Returns true if actor should be filtered (blocked/whitelisted)
    bool ActorMatchesFilter(uint32_t actorFormID, const std::string& actorName,
                            const std::vector<uint32_t>& filterFormIDs, 
                            const std::vector<std::string>& filterNames)
    {
        // Empty filter = affects all actors
        if (filterNames.empty()) return true;
        
        // Check if actor is in the filter list
        // ESL-safe: match by name AND last 3 digits of FormID
        for (size_t i = 0; i < filterNames.size() && i < filterFormIDs.size(); ++i) {
            if (actorName == filterNames[i] && 
                (actorFormID & 0xFFF) == (filterFormIDs[i] & 0xFFF)) {
                return true;  // Actor matches filter
            }
        }
        
        return false;  // Actor not in filter
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
        std::lock_guard<std::mutex> lock(dbMutex_);

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
        std::lock_guard<std::mutex> lock(dbMutex_);
        
        FlushQueue();
        FinalizeStatements();

        // Shutdown dialogue logger
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
                UNIQUE(target_type, target_formid, target_editorid)
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
                actor_filter_formids, actor_filter_names
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);
        )";

        const char* insertWhitelistSQL = R"(
            INSERT INTO whitelist (
                target_type, target_formid, target_editorid,
                block_type, added_timestamp, notes, response_text, subtype, subtype_name,
                filter_category, block_skyrimnet, source_plugin, quest_editorid,
                actor_filter_formids, actor_filter_names
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);
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
        
        std::lock_guard<std::mutex> lock(queueMutex_);
        pendingEntries_.push(entry);

        // Get current time for flush decision
        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
        
        // Auto-flush if either:
        // 1. Queue has reached batch size (200 entries)
        // 2. FLUSH_INTERVAL_MS (5 seconds) has passed since last flush
        // This balances between batching efficiency and preventing queue buildup
        bool sizeLimitReached = pendingEntries_.size() >= BATCH_SIZE;
        bool timeLimitReached = (now - lastFlushTime_ >= FLUSH_INTERVAL_MS);
        bool shouldFlush = sizeLimitReached || timeLimitReached;
        
        if (shouldFlush) {
            spdlog::trace("[DialogueDB] Auto-flushing: size={}, time={}, queue={}", 
                sizeLimitReached, timeLimitReached, pendingEntries_.size());
            ProcessQueue();
            lastFlushTime_ = now;
        }
    }

    void Database::ProcessQueue()
    {
        std::lock_guard<std::mutex> lock(dbMutex_);
        
        if (!db_ || !insertDialogueStmt_) return;
        
        size_t entriesCount = pendingEntries_.size();
        if (entriesCount == 0) return;  // Nothing to process
        
        spdlog::debug("[DialogueDB] Flushing {} queued dialogue entries", entriesCount);

        // Begin transaction for batch insert
        sqlite3_exec(db_, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);

        while (!pendingEntries_.empty()) {
            const auto& entry = pendingEntries_.front();

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

            pendingEntries_.pop();
        }

        // Commit transaction
        sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, nullptr);
        
        spdlog::debug("[DialogueDB] ProcessQueue complete, flushed {} entries to database", entriesCount);
        
        // Notify UI to refresh if menu is open
        if (PrismaUIMenu::IsOpen()) {
            spdlog::trace("[DialogueDB] Menu is open, triggering UI refresh");
            PrismaUIMenu::SendHistoryData();
        }
    }

    void Database::FlushQueue()
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        ProcessQueue();
        
        // Reset flush timer after manual flush
        lastFlushTime_ = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
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

    // ========================================
    // QUERY
    // ========================================

    std::vector<DialogueEntry> Database::GetRecentDialogue(int limit)
    {
        std::lock_guard<std::mutex> lock(dbMutex_);
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

    bool Database::HasRecentDuplicate(uint32_t speakerFormID, const std::string& responseText, int64_t withinSeconds)
    {
        std::lock_guard<std::mutex> lock(dbMutex_);
        
        if (!db_) return false;
        
        // Calculate the timestamp threshold
        auto now = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        int64_t threshold = now - withinSeconds;
        
        // Query for matching entries within the time window
        std::string sql = "SELECT COUNT(*) FROM dialogue_log WHERE speaker_formid = ? AND response_text = ? AND timestamp > ?;";
        sqlite3_stmt* stmt = nullptr;
        
        if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            spdlog::error("[DialogueDB] Failed to prepare duplicate check query: {}", sqlite3_errmsg(db_));
            return false;
        }
        
        sqlite3_bind_int(stmt, 1, speakerFormID);
        sqlite3_bind_text(stmt, 2, responseText.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 3, threshold);
        
        bool hasDuplicate = false;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            int count = sqlite3_column_int(stmt, 0);
            hasDuplicate = (count > 0);
            if (hasDuplicate) {
                spdlog::info("[DialogueDB] Found {} duplicate(s) within last {}s for speaker 0x{:08X}", 
                    count, withinSeconds, speakerFormID);
            }
        }
        
        sqlite3_finalize(stmt);
        return hasDuplicate;
    }

    bool Database::DeleteDialogueEntry(int64_t id)
    {
        std::lock_guard<std::mutex> lock(dbMutex_);
        
        if (!db_) return false;
        
        const char* sql = "DELETE FROM dialogue_log WHERE id = ?;";
        sqlite3_stmt* stmt = nullptr;
        
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            spdlog::error("[DialogueDB] Failed to prepare delete statement: {}", sqlite3_errmsg(db_));
            return false;
        }
        
        sqlite3_bind_int64(stmt, 1, id);
        bool success = sqlite3_step(stmt) == SQLITE_DONE;
        sqlite3_finalize(stmt);
        
        if (success) {
            spdlog::debug("[DialogueDB] Deleted dialogue entry with ID {}", id);
        }
        
        return success;
    }

    int Database::DeleteDialogueEntriesBatch(const std::vector<int64_t>& ids)
    {
        if (ids.empty()) return 0;
        
        std::lock_guard<std::mutex> lock(dbMutex_);
        
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

    bool Database::AddToBlacklist(const BlacklistEntry& entry, bool skipEnrichment)
    {
        std::lock_guard<std::mutex> lock(dbMutex_);

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
                    actor_filter_formids = ?, actor_filter_names = ?
                WHERE id = ?;
            )";
            sqlite3_stmt* updateStmt = nullptr;
            
            if (sqlite3_prepare_v2(db_, updateSql, -1, &updateStmt, nullptr) != SQLITE_OK) {
                spdlog::error("[DialogueDB] Failed to prepare update statement: {}", sqlite3_errmsg(db_));
                return false;
            }
            
            // Serialize actor filters to JSON
            std::string actorFormIDsJson = ActorFormIDsToJson(enrichedEntry.actorFilterFormIDs);
            std::string actorNamesJson = ActorNamesToJson(enrichedEntry.actorFilterNames);
            
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
            sqlite3_bind_int64(updateStmt, 13, existingId);
            
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
                    
                    // For soft blocks (1) and SkyrimNet blocks (3), also blacklist individual dialogue topics
                    // Hard blocks (2) use phase conditions and don't need individual topic blocking
                    if (enrichedEntry.blockType == BlockType::Soft || enrichedEntry.blockType == BlockType::SkyrimNet) {
                        spdlog::info("[DialogueDB] Soft/SkyrimNet blocking scene {} - extracting dialogue topics", enrichedEntry.targetEditorID);
                        auto topics = TopicResponseExtractor::ExtractDialogueTopicsFromScene(enrichedEntry.targetEditorID);
                        
                        for (const auto& topicInfo : topics) {
                            if (topicInfo.editorID.empty()) {
                                spdlog::warn("[DialogueDB] Skipping topic with empty EditorID (FormID: 0x{:08X})", topicInfo.formID);
                                continue;
                            }
                            
                            // Create a blacklist entry for this topic
                            BlacklistEntry topicEntry;
                            topicEntry.targetType = BlacklistTarget::Topic;
                            topicEntry.targetFormID = topicInfo.formID;
                            topicEntry.targetEditorID = topicInfo.editorID;
                            topicEntry.blockType = enrichedEntry.blockType;
                            topicEntry.addedTimestamp = enrichedEntry.addedTimestamp;
                            topicEntry.notes = "Auto-added from scene: " + enrichedEntry.targetEditorID;
                            topicEntry.responseText = "";  // Will be enriched if topic fires
                            topicEntry.subtype = 14;  // Scene subtype
                            topicEntry.subtypeName = "Scene";
                            topicEntry.filterCategory = enrichedEntry.filterCategory;
                            topicEntry.sourcePlugin = topicInfo.sourcePlugin;
                            
                            // Set granular blocking options based on block type
                            if (enrichedEntry.blockType == BlockType::SkyrimNet) {
                                topicEntry.blockAudio = false;
                                topicEntry.blockSubtitles = false;
                                topicEntry.blockSkyrimNet = true;
                            } else {  // Soft
                                topicEntry.blockAudio = true;
                                topicEntry.blockSubtitles = true;
                                topicEntry.blockSkyrimNet = true;
                            }
                            
                            // Add to blacklist (skip enrichment to avoid recursion and improve performance)
                            if (AddToBlacklist(topicEntry, true)) {
                                spdlog::debug("[DialogueDB] Auto-blocked topic {} from scene {}", topicInfo.editorID, enrichedEntry.targetEditorID);
                            } else {
                                spdlog::warn("[DialogueDB] Failed to auto-block topic {} from scene {}", topicInfo.editorID, enrichedEntry.targetEditorID);
                            }
                        }
                        
                        spdlog::info("[DialogueDB] Soft/SkyrimNet blocked {} dialogue topics from scene {}", topics.size(), enrichedEntry.targetEditorID);
                    }
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
            spdlog::info("[DialogueDB] Inserting new blacklist entry: {} (FormID: 0x{:08X}, Type: {}, FilterCategory: '{}')", 
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
            
            // Serialize actor filters to JSON
            std::string actorFormIDsJson = ActorFormIDsToJson(enrichedEntry.actorFilterFormIDs);
            std::string actorNamesJson = ActorNamesToJson(enrichedEntry.actorFilterNames);
            sqlite3_bind_text(insertBlacklistStmt_, 14, actorFormIDsJson.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(insertBlacklistStmt_, 15, actorNamesJson.c_str(), -1, SQLITE_TRANSIENT);

            if (sqlite3_step(insertBlacklistStmt_) != SQLITE_DONE) {
                spdlog::error("[DialogueDB] Failed to add blacklist entry: {}", sqlite3_errmsg(db_));
                return false;
            }

            int64_t newId = sqlite3_last_insert_rowid(db_);
            spdlog::info("[DialogueDB] Successfully inserted blacklist entry (id={})", newId);
            
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
            // Only Hard blocks (2) prevent scenes from starting
            if (enrichedEntry.targetType == BlacklistTarget::Scene) {
                // Scene was directly blocked
                SceneHook::UpdateSceneConditions(enrichedEntry.targetEditorID, static_cast<uint8_t>(enrichedEntry.blockType));
                
                // For soft blocks (1) and SkyrimNet blocks (3), also blacklist individual dialogue topics
                // Hard blocks (2) use phase conditions and don't need individual topic blocking
                if (enrichedEntry.blockType == BlockType::Soft || enrichedEntry.blockType == BlockType::SkyrimNet) {
                    spdlog::info("[DialogueDB] Soft/SkyrimNet blocking scene {} - extracting dialogue topics", enrichedEntry.targetEditorID);
                    auto topics = TopicResponseExtractor::ExtractDialogueTopicsFromScene(enrichedEntry.targetEditorID);
                    
                    for (const auto& topicInfo : topics) {
                        if (topicInfo.editorID.empty()) {
                            spdlog::warn("[DialogueDB] Skipping topic with empty EditorID (FormID: 0x{:08X})", topicInfo.formID);
                            continue;
                        }
                        
                        // Create a blacklist entry for this topic
                        BlacklistEntry topicEntry;
                        topicEntry.targetType = BlacklistTarget::Topic;
                        topicEntry.targetFormID = topicInfo.formID;
                        topicEntry.targetEditorID = topicInfo.editorID;
                        topicEntry.blockType = enrichedEntry.blockType;
                        topicEntry.addedTimestamp = enrichedEntry.addedTimestamp;
                        topicEntry.notes = "Auto-added from scene: " + enrichedEntry.targetEditorID;
                        topicEntry.responseText = "";  // Will be enriched if topic fires
                        topicEntry.subtype = 14;  // Scene subtype
                        topicEntry.subtypeName = "Scene";
                        topicEntry.filterCategory = enrichedEntry.filterCategory;
                        topicEntry.sourcePlugin = topicInfo.sourcePlugin;
                        
                        // Set granular blocking options based on block type
                        if (enrichedEntry.blockType == BlockType::SkyrimNet) {
                            topicEntry.blockAudio = false;
                            topicEntry.blockSubtitles = false;
                            topicEntry.blockSkyrimNet = true;
                        } else {  // Soft
                            topicEntry.blockAudio = true;
                            topicEntry.blockSubtitles = true;
                            topicEntry.blockSkyrimNet = true;
                        }
                        
                        // Add to blacklist (skip enrichment to avoid recursion and improve performance)
                        if (AddToBlacklist(topicEntry, true)) {
                            spdlog::debug("[DialogueDB] Auto-blocked topic {} from scene {}", topicInfo.editorID, enrichedEntry.targetEditorID);
                        } else {
                            spdlog::warn("[DialogueDB] Failed to auto-block topic {} from scene {}", topicInfo.editorID, enrichedEntry.targetEditorID);
                        }
                    }
                    
                    spdlog::info("[DialogueDB] Soft/SkyrimNet blocked {} dialogue topics from scene {}", topics.size(), enrichedEntry.targetEditorID);
                }
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
        std::lock_guard<std::mutex> lock(dbMutex_);

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
                
                // Also remove any auto-added topics from soft/skyrimnet blocks
                std::string notePattern = "Auto-added from scene: " + targetEditorID;
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
                        spdlog::info("[DialogueDB] Removed {} auto-added topics when unblocking scene {}", deletedCount, targetEditorID);
                    }
                }
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

    void Database::BeginTransaction()
    {
        std::lock_guard<std::mutex> lock(dbMutex_);
        if (db_) {
            sqlite3_exec(db_, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);
        }
    }

    void Database::CommitTransaction()
    {
        std::lock_guard<std::mutex> lock(dbMutex_);
        if (db_) {
            sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, nullptr);
        }
    }

    void Database::RollbackTransaction()
    {
        std::lock_guard<std::mutex> lock(dbMutex_);
        if (db_) {
            sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
        }
    }

    int Database::AddToBlacklistBatch(const std::vector<BlacklistEntry>& entries, bool skipEnrichment)
    {
        if (entries.empty()) return 0;
        
        std::lock_guard<std::mutex> lock(dbMutex_);
        
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
        
        std::lock_guard<std::mutex> lock(dbMutex_);
        
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
        std::lock_guard<std::mutex> lock(dbMutex_);

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
        std::lock_guard<std::mutex> lock(dbMutex_);

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
        std::lock_guard<std::mutex> lock(dbMutex_);
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

    // ============================================================================
    // Whitelist Management Functions (parallel to blacklist)
    // ============================================================================

    bool Database::AddToWhitelist(const BlacklistEntry& entry, bool skipEnrichment)
    {
        std::lock_guard<std::mutex> lock(dbMutex_);

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
                    filter_category = ?, block_skyrimnet = ?, source_plugin = ?, quest_editorid = ?
                WHERE id = ?;
            )";
            sqlite3_stmt* updateStmt = nullptr;
            
            if (sqlite3_prepare_v2(db_, updateSql, -1, &updateStmt, nullptr) != SQLITE_OK) {
                spdlog::error("[DialogueDB] Failed to prepare whitelist update statement: {}", sqlite3_errmsg(db_));
                return false;
            }
            
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
        std::lock_guard<std::mutex> lock(dbMutex_);

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
        
        std::lock_guard<std::mutex> lock(dbMutex_);
        
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
        std::lock_guard<std::mutex> lock(dbMutex_);

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
        std::lock_guard<std::mutex> lock(dbMutex_);

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
        std::lock_guard<std::mutex> lock(dbMutex_);
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

    bool Database::IsWhitelisted(BlacklistTarget targetType, uint32_t formID, const std::string& editorID)
    {
        std::lock_guard<std::mutex> lock(dbMutex_);

        if (!db_) return false;

        // Prioritize EditorID matching (stable for ESL plugins), fall back to FormID if EditorID is empty
        const char* sql = "SELECT 1 FROM whitelist WHERE target_type = ? AND (((target_formid = ? OR target_formid = 0) AND target_editorid = ?) OR (target_editorid = '' AND target_formid = ?)) LIMIT 1;";
        sqlite3_stmt* stmt = nullptr;

        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            return false;
        }

        sqlite3_bind_int(stmt, 1, static_cast<int>(targetType));
        sqlite3_bind_int(stmt, 2, formID);
        sqlite3_bind_text(stmt, 3, editorID.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 4, formID);

        bool found = (sqlite3_step(stmt) == SQLITE_ROW);
        sqlite3_finalize(stmt);

        return found;
    }

    bool Database::IsPluginWhitelisted(const std::string& pluginName)
    {
        std::lock_guard<std::mutex> lock(dbMutex_);

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

    void Database::CleanupOldEntries(int daysToKeep)
    {
        std::lock_guard<std::mutex> lock(dbMutex_);

        if (!db_) return;

        auto now = std::chrono::system_clock::now();
        auto cutoff = now - std::chrono::hours(daysToKeep * 24);
        int64_t cutoffTimestamp = std::chrono::duration_cast<std::chrono::seconds>(cutoff.time_since_epoch()).count();

        const char* sql = "DELETE FROM dialogue_log WHERE timestamp < ?;";
        sqlite3_stmt* stmt = nullptr;

        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int64(stmt, 1, cutoffTimestamp);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);

            spdlog::info("[DialogueDB] Cleaned up entries older than {} days", daysToKeep);
        }
    }

    int64_t Database::GetTotalEntries()
    {
        std::lock_guard<std::mutex> lock(dbMutex_);

        if (!db_) return 0;

        const char* sql = "SELECT COUNT(*) FROM dialogue_log;";
        sqlite3_stmt* stmt = nullptr;
        int64_t count = 0;

        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                count = sqlite3_column_int64(stmt, 0);
            }
            sqlite3_finalize(stmt);
        }

        return count;
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
    
    bool Database::HasScenesImported()
    {
        const char* checkSql = "SELECT COUNT(*) FROM blacklist WHERE target_type = 4;";
        sqlite3_stmt* checkStmt = nullptr;
        
        std::lock_guard<std::mutex> lock(dbMutex_);
        if (!db_) return false;
        
        if (sqlite3_prepare_v2(db_, checkSql, -1, &checkStmt, nullptr) == SQLITE_OK) {
            if (sqlite3_step(checkStmt) == SQLITE_ROW) {
                int64_t count = sqlite3_column_int64(checkStmt, 0);
                sqlite3_finalize(checkStmt);
                return count > 0;
            }
            sqlite3_finalize(checkStmt);
        }
        return false;
    }


    
    
    void Database::EnrichBlacklistEntryAtRuntime(BlacklistTarget targetType, const std::string& targetEditorID, const std::string& responseText)
    {
        if (targetEditorID.empty() || responseText.empty()) return;
        
        std::lock_guard<std::mutex> lock(dbMutex_);
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
        
        std::lock_guard<std::mutex> lock(dbMutex_);
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
        
        std::lock_guard<std::mutex> lock(dbMutex_);
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

    bool Database::ShouldBlockAudio(uint32_t formID, const std::string& editorID, uint32_t actorFormID, const std::string& actorName)
    {
        std::lock_guard<std::mutex> lock(dbMutex_);
        
        if (!db_) return false;
        
        // Prioritize EditorID matching (stable for ESL plugins), fall back to FormID if EditorID is empty
        // Match: (FormID matches AND EditorID matches) OR (EditorID matches and DB entry has FormID=0) OR (EditorID is empty and FormID matches)
        const char* sql = "SELECT block_type, filter_category, actor_filter_formids, actor_filter_names FROM blacklist WHERE ((target_formid = ? OR target_formid = 0) AND target_editorid = ?) OR (target_editorid = '' AND target_formid = ?) LIMIT 1;";
        sqlite3_stmt* stmt = nullptr;
        
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            return false;
        }
        
        sqlite3_bind_int(stmt, 1, formID);
        sqlite3_bind_text(stmt, 2, editorID.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 3, formID);
        
        bool shouldBlock = false;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            BlockType blockType = static_cast<BlockType>(sqlite3_column_int(stmt, 0));
            
            // Get filterCategory (default to "Blacklist" if not set)
            const char* filterCategoryC = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            std::string filterCategory = (filterCategoryC && filterCategoryC[0]) ? filterCategoryC : "Blacklist";
            
            // Get actor filters
            const char* actorFormIDsJson = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            const char* actorNamesJson = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            
            std::string actorFormIDsStr = actorFormIDsJson ? actorFormIDsJson : "[]";
            std::string actorNamesStr = actorNamesJson ? actorNamesJson : "[]";
            
            // Parse actor filters from database
            std::vector<uint32_t> filterFormIDs = ParseActorFormIDsFromJson(actorFormIDsStr);
            std::vector<std::string> filterNames = ParseActorNamesFromJson(actorNamesStr);
            
            // Debug: Log what we're checking
            spdlog::info("[DialogueDB] ShouldBlockAudio: FormID=0x{:08X}, speakerFormID=0x{:08X}, speakerName='{}', filterFormIDs.size()={}, filterNames.size()={}", 
                formID, actorFormID, actorName, filterFormIDs.size(), filterNames.size());
            
            // Check if actor matches filter (if actor info provided AND filter exists)
            if ((actorFormID > 0 || !actorName.empty()) && (!filterFormIDs.empty() || !filterNames.empty())) {
                if (!ActorMatchesFilter(actorFormID, actorName, filterFormIDs, filterNames)) {
                    // Entry doesn't apply to this actor
                    spdlog::info("[DialogueDB] ShouldBlockAudio: actor '{}' (0x{:08X}) not in filter -> ALLOW", 
                        actorName, actorFormID);
                    sqlite3_finalize(stmt);
                    return false;
                } else {
                    spdlog::info("[DialogueDB] ShouldBlockAudio: actor '{}' (0x{:08X}) matches filter -> checking block type", 
                        actorName, actorFormID);
                }
            } else if (!filterFormIDs.empty() || !filterNames.empty()) {
                // Filter exists but no actor info provided - skip actor-specific entries
                spdlog::info("[DialogueDB] ShouldBlockAudio: Actor filter exists but no speaker info provided -> ALLOW");
                sqlite3_finalize(stmt);
                return false;
            }
            
            // SkyrimNet-only blocks should never block audio
            if (blockType == BlockType::SkyrimNet) {
                shouldBlock = false;
            } else if (!Config::IsFilterCategoryEnabled(filterCategory)) {
                // Check if this entry's filterCategory toggle is enabled
                shouldBlock = false;  // Toggle disabled, don't block
            } else {
                // Toggle enabled - both hard and soft blocks always block audio
                shouldBlock = true;
            }
        }
        
        sqlite3_finalize(stmt);
        return shouldBlock;
    }
    
    bool Database::ShouldBlockSubtitles(uint32_t formID, const std::string& editorID, uint32_t actorFormID, const std::string& actorName)
    {
        std::lock_guard<std::mutex> lock(dbMutex_);
        
        if (!db_) return false;
        
        // Prioritize EditorID matching (stable for ESL plugins), fall back to FormID if EditorID is empty
        const char* sql = "SELECT block_type, filter_category, actor_filter_formids, actor_filter_names FROM blacklist WHERE ((target_formid = ? OR target_formid = 0) AND target_editorid = ?) OR (target_editorid = '' AND target_formid = ?) LIMIT 1;";
        sqlite3_stmt* stmt = nullptr;
        
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            spdlog::error("[DialogueDB] ShouldBlockSubtitles: Failed to prepare statement: {}", sqlite3_errmsg(db_));
            return false;
        }
        
        sqlite3_bind_int(stmt, 1, formID);
        sqlite3_bind_text(stmt, 2, editorID.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 3, formID);
        
        bool shouldBlock = false;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            BlockType blockType = static_cast<BlockType>(sqlite3_column_int(stmt, 0));
            
            // Get filterCategory (default to "Blacklist" if not set)
            const char* filterCategoryC = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            std::string filterCategory = (filterCategoryC && filterCategoryC[0]) ? filterCategoryC : "Blacklist";
            
            // Get actor filters
            const char* actorFormIDsJson = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            const char* actorNamesJson = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            
            std::string actorFormIDsStr = actorFormIDsJson ? actorFormIDsJson : "[]";
            std::string actorNamesStr = actorNamesJson ? actorNamesJson : "[]";
            
            // Parse actor filters from database
            std::vector<uint32_t> filterFormIDs = ParseActorFormIDsFromJson(actorFormIDsStr);
            std::vector<std::string> filterNames = ParseActorNamesFromJson(actorNamesStr);
            
            // Debug: Log what we're checking
            spdlog::info("[DialogueDB] ShouldBlockSubtitles: FormID=0x{:08X}, speakerFormID=0x{:08X}, speakerName='{}', filterFormIDs.size()={}, filterNames.size()={}", 
                formID, actorFormID, actorName, filterFormIDs.size(), filterNames.size());
            
            // Check if actor matches filter (if actor info provided AND filter exists)
            if ((actorFormID > 0 || !actorName.empty()) && (!filterFormIDs.empty() || !filterNames.empty())) {
                if (!ActorMatchesFilter(actorFormID, actorName, filterFormIDs, filterNames)) {
                    // Entry doesn't apply to this actor
                    spdlog::info("[DialogueDB] ShouldBlockSubtitles: actor '{}' (0x{:08X}) not in filter -> ALLOW", 
                        actorName, actorFormID);
                    sqlite3_finalize(stmt);
                    return false;
                } else {
                    spdlog::info("[DialogueDB] ShouldBlockSubtitles: actor '{}' (0x{:08X}) matches filter -> checking block type", 
                        actorName, actorFormID);
                }
            } else if (!filterFormIDs.empty() || !filterNames.empty()) {
                // Filter exists but no actor info provided - skip actor-specific entries
                spdlog::info("[DialogueDB] ShouldBlockSubtitles: Actor filter exists but no speaker info provided -> ALLOW");
                sqlite3_finalize(stmt);
                return false;
            }
            
            // SkyrimNet-only blocks should never block subtitles
            if (blockType == BlockType::SkyrimNet) {
                shouldBlock = false;
                spdlog::info("[DialogueDB] ShouldBlockSubtitles: FormID=0x{:08X}, EditorID='{}', SkyrimNet-only block -> ALLOW", 
                    formID, editorID);
            } else if (!Config::IsFilterCategoryEnabled(filterCategory)) {
                // Check if this entry's filterCategory toggle is enabled
                shouldBlock = false;  // Toggle disabled, don't block
                spdlog::info("[DialogueDB] ShouldBlockSubtitles: FormID=0x{:08X}, EditorID='{}', category='{}' toggle DISABLED -> ALLOW", 
                    formID, editorID, filterCategory);
            } else {
                // Toggle enabled - both hard and soft blocks always block subtitles
                shouldBlock = true;
                spdlog::info("[DialogueDB] ShouldBlockSubtitles: Found {} block for FormID=0x{:08X}, EditorID='{}', category='{}' -> BLOCK", 
                    blockType == BlockType::Hard ? "HARD" : "soft", formID, editorID, filterCategory);
            }
        } else {
            spdlog::info("[DialogueDB] ShouldBlockSubtitles: No entry found for FormID=0x{:08X}, EditorID='{}'", formID, editorID);
        }
        
        sqlite3_finalize(stmt);
        return shouldBlock;
    }
    
    bool Database::ShouldBlockSkyrimNet(uint32_t formID, const std::string& editorID, uint32_t actorFormID, const std::string& actorName)
    {
        std::lock_guard<std::mutex> lock(dbMutex_);
        
        if (!db_) return false;
        
        // Prioritize EditorID matching (stable for ESL plugins), fall back to FormID if EditorID is empty
        const char* sql = "SELECT block_skyrimnet, block_type, filter_category, actor_filter_formids, actor_filter_names FROM blacklist WHERE ((target_formid = ? OR target_formid = 0) AND target_editorid = ?) OR (target_editorid = '' AND target_formid = ?) LIMIT 1;";
        sqlite3_stmt* stmt = nullptr;
        
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            return false;
        }
        
        sqlite3_bind_int(stmt, 1, formID);
        sqlite3_bind_text(stmt, 2, editorID.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 3, formID);
        
        bool shouldBlock = false;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            if (sqlite3_column_count(stmt) > 1) {
                BlockType blockType = static_cast<BlockType>(sqlite3_column_int(stmt, 1));
                
                // Get filterCategory (default to "Blacklist" if not set)
                const char* filterCategoryC = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
                std::string filterCategory = (filterCategoryC && filterCategoryC[0]) ? filterCategoryC : "Blacklist";
                
                // Get actor filters
                const char* actorFormIDsJson = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
                const char* actorNamesJson = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
                
                std::string actorFormIDsStr = actorFormIDsJson ? actorFormIDsJson : "[]";
                std::string actorNamesStr = actorNamesJson ? actorNamesJson : "[]";
                
                // Check if actor matches filter (if actor info provided)
                if (actorFormID > 0 && !actorName.empty()) {
                    std::vector<uint32_t> filterFormIDs = ParseActorFormIDsFromJson(actorFormIDsStr);
                    std::vector<std::string> filterNames = ParseActorNamesFromJson(actorNamesStr);
                    
                    if (!ActorMatchesFilter(actorFormID, actorName, filterFormIDs, filterNames)) {
                        // Entry doesn't apply to this actor
                        sqlite3_finalize(stmt);
                        return false;
                    }
                }
                
                // SkyrimNet-only blocks ignore filter_category - only check the flag
                if (blockType == BlockType::SkyrimNet) {
                    shouldBlock = sqlite3_column_int(stmt, 0) != 0;
                } else if (!Config::IsFilterCategoryEnabled(filterCategory)) {
                    // Check if this entry's filterCategory toggle is enabled
                    shouldBlock = false;  // Toggle disabled, don't block
                } else {
                    // Toggle enabled, check block flags
                    if (blockType == BlockType::Hard) {
                        shouldBlock = true;
                    } else {
                        shouldBlock = sqlite3_column_int(stmt, 0) != 0;
                    }
                }
            }
        }
        
        sqlite3_finalize(stmt);
        return shouldBlock;
    }
}

