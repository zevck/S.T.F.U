#pragma once

#include <sqlite3.h>
#include <string>
#include <vector>
#include <mutex>
#include <queue>
#include <memory>
#include <cstdint>

namespace DialogueDB
{
    enum class BlockedStatus : uint8_t
    {
        Normal = 0,       // Not blocked
        SoftBlock = 1,    // Audio/subtitles silenced, scripts execute
        HardBlock = 2,    // Scene stopped entirely
        SkyrimNetBlock = 3,  // Blocked from SkyrimNet logging only
        FilteredByConfig = 4,  // Blocked by MCM subtype filter
        ToggledOff = 5    // In blacklist but toggle disabled (allowed)
    };

    enum class BlacklistTarget : uint8_t
    {
        Topic = 1,
        Quest = 2,
        Subtype = 3,
        Scene = 4,
        Plugin = 5
    };

    enum class BlockType : uint8_t
    {
        Soft = 1,         // Silence (audio/subtitle/animation removal)
        Hard = 2,         // Scene prevention
        SkyrimNet = 3     // SkyrimNet filter only
    };

    struct DialogueEntry
    {
        int64_t id = 0;
        int64_t timestamp = 0;

        // Speaker
        std::string speakerName;
        uint32_t speakerFormID = 0;
        uint32_t speakerBaseFormID = 0;

        // Topic
        std::string topicEditorID;
        uint32_t topicFormID = 0;
        uint16_t topicSubtype = 0;
        std::string topicSubtypeName;

        // Quest
        std::string questEditorID;
        uint32_t questFormID = 0;
        std::string questName;

        // Scene (for scene dialogue)
        std::string sceneEditorID;

        // TopicInfo
        uint32_t topicInfoFormID = 0;

        // Response
        std::string responseText;
        std::string voiceFilepath;
        std::vector<std::string> allResponses;  // All responses for this topic (captured at log time)

        // Source
        std::string sourcePlugin;  // TopicInfo source - which mod added this specific response
        std::string topicSourcePlugin;  // Topic source - which mod owns the topic container (for blacklisting)

        // Metadata
        BlockedStatus blockedStatus = BlockedStatus::Normal;
        bool isScene = false;
        bool isBardSong = false;
        bool isHardcodedScene = false;
        
        // SkyrimNet compatibility
        bool skyrimNetBlockable = false;  // True if confirmed menu-based (Hello subtype or appeared in PopulateTopicInfo)
    };

    struct BlacklistEntry
    {
        int64_t id = 0;
        BlacklistTarget targetType;
        uint32_t targetFormID = 0;  // 0 for subtypes, optional for scenes
        std::string targetEditorID;
        BlockType blockType;
        int64_t addedTimestamp = 0;
        std::string notes;
        std::string responseText;  // The actual dialogue text for user-friendly searching
        uint16_t subtype = 0;  // Dialogue subtype for filtering
        std::string subtypeName;  // Human-readable subtype name
        std::string sourcePlugin;  // Plugin filename (e.g., "Skyrim.esm", "MyMod.esp")
        std::string questEditorID;  // Quest EditorID for ESL-safe matching (topics belong to quests)
        
        // Filter category determines which MCM toggle controls blocking
        // Values: "Blacklist" (default), "Scene", or any subtype name ("Hello", "Idle", etc)
        std::string filterCategory = "Blacklist";
        
        // Granular blocking options (for soft blocks)
        bool blockAudio = true;
        bool blockSubtitles = true;
        bool blockSkyrimNet = true;
    };

    // SceneBlacklistEntry removed - scenes now use BlacklistEntry with targetType=Scene

    struct QueryFilter
    {
        std::string subtypeName;
        std::string questEditorID;
        std::string speakerName;
        BlockedStatus blockedStatus = BlockedStatus::Normal;
        bool filterByBlocked = false;
        std::string textSearch;
        int64_t startTimestamp = 0;
        int64_t endTimestamp = 0;
        int limit = 100;
        int offset = 0;
    };

    // JSON serialization helpers for response arrays
    std::string ResponsesToJson(const std::vector<std::string>& responses);
    std::vector<std::string> JsonToResponses(const std::string& json);

    class Database
    {
    public:
        Database();
        ~Database();

        // Initialize database (create tables, indexes)
        bool Initialize(const std::string& dbPath);

        // Close database
        void Close();

        // Dialogue logging (async via queue)
        void LogDialogue(const DialogueEntry& entry);
        void FlushQueue();  // Force flush queued entries

        // Query recent dialogue
        std::vector<DialogueEntry> GetRecentDialogue(int limit = 100);
        std::vector<DialogueEntry> QueryDialogue(const QueryFilter& filter);
        DialogueEntry GetDialogueByID(int64_t id);
        
        // Delete dialogue entries
        bool DeleteDialogueEntry(int64_t id);
        int DeleteDialogueEntriesBatch(const std::vector<int64_t>& ids);
        
        // Check if a recent duplicate exists (same speaker + response text within timeframe)
        bool HasRecentDuplicate(uint32_t speakerFormID, const std::string& responseText, int64_t withinSeconds = 5);

        // Blacklist management
        bool AddToBlacklist(const BlacklistEntry& entry, bool skipEnrichment = false);
        bool RemoveFromBlacklist(int64_t id);
        int64_t GetBlacklistEntryId(uint32_t formID, const std::string& editorID);
        std::vector<BlacklistEntry> GetBlacklist();
        bool IsBlacklisted(BlacklistTarget targetType, uint32_t formID, const std::string& editorID);
        int ClearBlacklist();  // Remove all blacklist entries, returns count removed
        
        // Batch operations for better performance
        void BeginTransaction();
        void CommitTransaction();
        void RollbackTransaction();
        int AddToBlacklistBatch(const std::vector<BlacklistEntry>& entries, bool skipEnrichment = false);
        int RemoveFromBlacklistBatch(const std::vector<int64_t>& ids);
        
        // Whitelist management (uses same BlacklistEntry struct with filterCategory="Whitelist")
        bool AddToWhitelist(const BlacklistEntry& entry, bool skipEnrichment = false);
        bool RemoveFromWhitelist(int64_t id);
        int RemoveFromWhitelistBatch(const std::vector<int64_t>& ids);
        int64_t GetWhitelistEntryId(uint32_t formID, const std::string& editorID);
        std::vector<BlacklistEntry> GetWhitelist();
        bool IsWhitelisted(BlacklistTarget targetType, uint32_t formID, const std::string& editorID);
        bool IsPluginWhitelisted(const std::string& pluginName);
        int ClearWhitelist();  // Remove all whitelist entries, returns count removed
        
        // Query granular blocking flags (works for both topics and scenes)
        bool ShouldBlockAudio(uint32_t formID, const std::string& editorID);
        bool ShouldBlockSubtitles(uint32_t formID, const std::string& editorID);
        bool ShouldBlockSkyrimNet(uint32_t formID, const std::string& editorID);
        
        // Mark topics as SkyrimNet blockable (retroactive update for menu detection)
        void MarkTopicsAsSkyrimNetBlockable(const std::vector<uint32_t>& topicInfoFormIDs);
        
        // Helper to import hardcoded scenes (sets filterCategory="Scene" by default, targetType=Scene)
        void ImportHardcodedScenes(const std::vector<std::string>& sceneEditorIDs, const std::string& filterCategory = "Scene");
        
        // Check if scenes have already been imported
        bool HasScenesImported();
        
        // Runtime enrichment: Update blacklist entry with captured response text
        void EnrichBlacklistEntryAtRuntime(BlacklistTarget targetType, const std::string& targetEditorID, const std::string& responseText);

        // Maintenance
        void CleanupOldEntries(int daysToKeep = 30);
        int64_t GetTotalEntries();

    private:
        sqlite3* db_ = nullptr;
        std::mutex dbMutex_;
        std::mutex queueMutex_;
        std::queue<DialogueEntry> pendingEntries_;
        int64_t lastFlushTime_ = 0;  // Timestamp of last queue flush
        
        // Performance tuning constants
        static constexpr size_t BATCH_SIZE = 200;  // Increased from 50 for better batching
        static constexpr int64_t FLUSH_INTERVAL_MS = 5000;  // Flush every 5 seconds
        
        bool CreateTables();
        bool UpdateSchema();
        bool CreateIndexes();
        void ProcessQueue();
        
        // Helper to get column index by name from prepared statement
        int GetColumnIndex(sqlite3_stmt* stmt, const char* columnName);
        
        // Prepared statements for performance
        sqlite3_stmt* insertDialogueStmt_ = nullptr;
        sqlite3_stmt* insertBlacklistStmt_ = nullptr;
        sqlite3_stmt* insertWhitelistStmt_ = nullptr;
        
        void PrepareStatements();
        void FinalizeStatements();
    };

    // Global database instance
    Database* GetDatabase();
}
