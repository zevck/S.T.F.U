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

#include <sqlite3.h>
#include <string>
#include <vector>
#include <mutex>
#include <queue>
#include <memory>
#include <cstdint>

// Forward declarations
namespace RE
{
    class TESObjectREFR;
}

namespace DialogueDB
{
    enum class BlockedStatus : uint8_t
    {
        Normal = 0,       // Not blocked
        SoftBlock = 1,    // Audio/subtitles silenced, scripts execute
        HardBlock = 2,    // Scene stopped entirely
        SkyrimNetBlock = 3,  // Blocked from SkyrimNet logging only
        FilteredByConfig = 4,  // Blocked by MCM subtype filter
        ToggledOff = 5,   // In blacklist but toggle disabled (allowed)
        Whitelisted = 6   // In whitelist (always allowed)
    };

    enum class BlacklistTarget : uint8_t
    {
        Topic = 1,
        Quest = 2,
        Subtype = 3,
        Scene = 4,
        Plugin = 5,
        Actor = 6,    // Reference FormID (specific NPC instance); name stored in targetEditorID
        Faction = 7   // Faction EditorID stored in targetEditorID
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

        // Block source info (populated when an Actor or Faction blacklist entry caused the block)
        bool isActorBlocked = false;           // True when an Actor-type blacklist entry matched
        std::string blockingFactionEditorID;   // EditorID of the faction that triggered the block

        // Whitelist source info (populated when an Actor or Faction whitelist entry matched)
        bool isActorWhitelisted = false;       // True when an Actor-type whitelist entry matched
        std::string whitelistFactionEditorID;  // EditorID of the faction that triggered the whitelist
    };

    struct BlacklistEntry
    {
        int64_t id = 0;
        BlacklistTarget targetType;
        uint32_t targetFormID = 0;  // Runtime FormID when saved; 0 for subtypes, optional for scenes
        std::string targetEditorID;
        std::string targetFormKey;  // Load-order independent identity ("02707A:Skyrim.esm"); empty on rows saved before 1.2.0
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
        
        // Actor filtering: empty vectors = affects all actors
        std::vector<uint32_t> actorFilterFormIDs;  // Actor FormIDs
        std::vector<std::string> actorFilterNames;  // Actor names (ESL-safe matching with FormID)
        std::vector<std::string> actorFilterFormKeys;  // FormKey per actor, parallel to the above; "" = match by name + FormID
        std::vector<std::string> factionFilterEditorIDs;  // Faction EditorIDs for faction-based filtering
    };

    // SceneBlacklistEntry removed - scenes now use BlacklistEntry with targetType=Scene

    // Matching rules shared by the SQL lookups and the in-memory checks (hard-block pre-check,
    // history status). FormIDs passed in are current-session FormIDs.
    //   Target: FormKey, else EditorID, else (legacy rows with neither) the stored FormID.
    //   Actor rows: FormKey, else (legacy) the stored FormID.
    //   Actor filters: FormKey, else (legacy) name + last 3 hex digits of the FormID.
    bool EntryMatchesTarget(const BlacklistEntry& entry, uint32_t formID, const std::string& editorID);
    bool EntryMatchesActor(const BlacklistEntry& entry, uint32_t actorFormID);
    bool EntryActorFilterMatches(const BlacklistEntry& entry, uint32_t actorFormID, const std::string& actorName);

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
        
        // Delete dialogue entries
        int DeleteDialogueEntriesBatch(const std::vector<int64_t>& ids);

        // Blacklist management
        bool AddToBlacklist(const BlacklistEntry& entry, bool skipEnrichment = false);
        bool RemoveFromBlacklist(int64_t id);
        int64_t GetBlacklistEntryId(uint32_t formID, const std::string& editorID);
        std::vector<BlacklistEntry> GetBlacklist();
        int ClearBlacklist();  // Remove all blacklist entries, returns count removed
        
        // Batch operations for better performance
        int RemoveFromBlacklistBatch(const std::vector<int64_t>& ids);
        
        // Whitelist management (uses same BlacklistEntry struct with filterCategory="Whitelist")
        bool AddToWhitelist(const BlacklistEntry& entry, bool skipEnrichment = false);
        bool RemoveFromWhitelist(int64_t id);
        std::vector<BlacklistEntry> GetWhitelist();
        // Checks the entry's actor filters against actorFormID/actorName and its faction filters against actorRef
        bool IsWhitelisted(BlacklistTarget targetType, uint32_t formID, const std::string& editorID, uint32_t actorFormID, const std::string& actorName, RE::TESObjectREFR* actorRef);
        int ClearWhitelist();  // Remove all whitelist entries, returns count removed
        
        // Query blocking flags (works for both topics and scenes)
        // Optional actor parameters for actor-specific filtering (0 / "" = ignore actor filter)
        // ShouldSoftBlock: Returns true if dialogue should be soft-blocked (silences BOTH audio AND subtitles)
        bool ShouldSoftBlock(uint32_t formID, const std::string& editorID, uint32_t actorFormID = 0, const std::string& actorName = "", RE::TESObjectREFR* actorRef = nullptr);
        
        // Helper to import hardcoded scenes (sets filterCategory="Scene" by default, targetType=Scene)
        void ImportHardcodedScenes(const std::vector<std::string>& sceneEditorIDs, const std::string& filterCategory = "Scene");

        // Key/value meta store for one-shot flags (see meta table). Used to gate
        // automatic scene imports so removing scenes never causes them to be
        // re-added on the next launch.
        std::string GetMetaValue(const std::string& key);
        void SetMetaValue(const std::string& key, const std::string& value);
        bool GetMetaFlag(const std::string& key);
        void SetMetaFlag(const std::string& key, bool value);
        
        // Runtime enrichment: Update blacklist entry with captured response text
        void EnrichBlacklistEntryAtRuntime(BlacklistTarget targetType, const std::string& targetEditorID, const std::string& responseText);
        void EnrichBlacklistEntryAtRuntime(BlacklistTarget targetType, const std::string& targetEditorID, const std::vector<std::string>& allResponses);

    private:
        sqlite3* db_ = nullptr;
        std::recursive_mutex dbMutex_;
        std::mutex queueMutex_;
        std::queue<DialogueEntry> pendingEntries_;
        int64_t lastFlushTime_ = 0;  // Timestamp of last queue flush

        // Performance tuning constants
        static constexpr size_t BATCH_SIZE = 200;  // Increased from 50 for better batching
        static constexpr int64_t FLUSH_INTERVAL_MS = 1000;  // Flush every 1 second for responsive UI
        
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
