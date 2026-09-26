# Dialogue Database (SQLite)

STFU keeps all persistent user curation (blacklist, whitelist, one-shot import
flags) and a short rolling dialogue history in a single SQLite file,
`dialogue.db`. The `DialogueDB::Database` class, declared in
`include/DialogueDatabase.h`, wraps it. The hooks read it on every blocking
decision, the PrismaUI menu reads and writes it through the bridge, and
YAML/MCM imports write to it. Settings are **not** in the DB. They live in the
INI (see CONFIG_AND_SETTINGS.md).

## Overview

```
 PopulateTopicInfoHook ──LogDialogue()──► pendingEntries_ (std::queue, queueMutex_)
                                              │  flush when ≥200 entries, or ≥1 s since
                                              │  last flush (checked on the next LogDialogue),
                                              │  or on FlushQueue()
                                              ▼
                                        ProcessQueue() ── one transaction ──► dialogue_log
                                              │                               (pruned to 100 rows)
                                              └─ SKSE task → PrismaUIMenu::SendHistoryData() if open

 Config_Blocking / PopulateTopicInfoHook ──ShouldSoftBlock / IsWhitelisted──► whitelist, blacklist (sync reads)
 PrismaUIMenu_* / Config_Yaml / SceneMonitor / main ──Add/Remove/Clear/Import──► blacklist, whitelist, meta
 AddToBlacklist / RemoveFromBlacklist* ──► SceneHook::UpdateSceneConditions[ForTopic]  (side effect)
```

Everything except history inserts runs **synchronously** under `dbMutex_` on
the caller's thread.

## Key files

| File | Contents |
|---|---|
| `include/DialogueDatabase.h` | Enums (`BlockedStatus`, `BlacklistTarget`, `BlockType`), `DialogueEntry`, `BlacklistEntry`, `Database` class, `GetDatabase()` |
| `src/DialogueDatabase.cpp` | Singleton, `Initialize`/`Close`, `CreateTables`, `UpdateSchema`, `CreateIndexes`, prepared statements, `GetColumnIndex`, meta get/set |
| `src/DialogueDatabase_History.cpp` | `LogDialogue`, `ProcessQueue` (batch insert + prune), `FlushQueue`, `GetRecentDialogue`, `DeleteDialogueEntriesBatch` |
| `src/DialogueDatabase_Blacklist.cpp` | Blacklist CRUD (upsert), scene-condition side effects, `ImportHardcodedScenes`, `EnrichBlacklistEntryAtRuntime` |
| `src/DialogueDatabase_Whitelist.cpp` | Whitelist CRUD (upsert), both `IsWhitelisted` overloads |
| `src/DialogueDatabase_Query.cpp` | `ShouldSoftBlock`, the main whitelist→blacklist→actor→faction decision query |
| `src/DialogueDatabase_Json.cpp` | Hand-rolled JSON array (de)serialisers, `ActorMatchesFilter`, `FactionMatchesFilter`, `GetActorFactionEditorIDs` |
| `src/DialogueDatabaseInternal.h` | Declarations of the `_Json.cpp` helpers shared across the `DialogueDatabase_*.cpp` units |

## File location and lifetime

`src/main.cpp` `MessageHandler()` handles `kDataLoaded`:

1. `Config::Load()` (INI) runs first.
2. The path is built from the **game exe location**, not from SKSE's plugin path:
   `GetModuleFileNameW(nullptr)` → `<exe dir>/Data/SKSE/Plugins/STFU/data/dialogue.db`.
   The directories are created if missing.
3. Under MO2 the VFS redirects the new file to
   `MODS/overwrite/SKSE/Plugins/STFU/data/dialogue.db`. Because WAL mode is on,
   the `dialogue.db-wal` and `dialogue.db-shm` files sit next to it.
4. `GetDatabase()->Initialize(dbPath)` runs `sqlite3_open`, `PRAGMA journal_mode=WAL`,
   then `CreateTables()`, `UpdateSchema()`, `CreateIndexes()`, `PrepareStatements()` and
   `DialogueLogger::Initialize()` (the text log `STFU_DialogueLog.txt`).
5. First-run scene import is gated on a meta flag (see [meta](#meta)).
6. Hooks are installed **after** the DB is initialised. If `Initialize` fails,
   STFU logs `Failed to initialize dialogue database!` and still installs the
   hooks. Every DB method checks `if (!db_)` and returns the empty or false value.

**Singleton:** `DialogueDB::GetDatabase()` lazily creates a file-static
`std::unique_ptr<Database>`. It never returns null, but `db_` may be null
before `kDataLoaded` or after an init failure.

**Shutdown:** nothing calls `Close()` explicitly. It runs only from
`~Database()` when the static is destroyed at DLL unload. If Skyrim terminates
without running static destructors, queued history is lost and the WAL is left
un-checkpointed. SQLite recovers the WAL on the next open, so no committed data
is lost.

## Schema

These are the verbatim `CREATE TABLE IF NOT EXISTS` statements from
`CreateTables()`. The live DB matches them, checked against
`sqlite_master` in the author's copy on 2026-09-26.

### `dialogue_log` (history)

| Column | Type | Meaning |
|---|---|---|
| `id` | INTEGER PK AUTOINCREMENT | Row id. The UI uses it for deletes. |
| `timestamp` | INTEGER NOT NULL | Unix **seconds** (`now/1000` in `PopulateTopicInfoHook.cpp`) |
| `speaker_name` | TEXT | Display name of the speaker |
| `speaker_formid` | INTEGER | Speaker reference FormID |
| `speaker_base_formid` | INTEGER | Speaker base (TESNPC) FormID |
| `topic_editorid` | TEXT | DIAL EditorID (needs po3 Tweaks on AE) |
| `topic_formid` | INTEGER NOT NULL | DIAL FormID |
| `topic_subtype` | INTEGER NOT NULL | Engine subtype number (see DIALOGUE_SUBTYPES.md) |
| `topic_subtype_name` | TEXT | Human-readable subtype |
| `quest_editorid` / `quest_formid` / `quest_name` | TEXT / INTEGER / TEXT | Owning quest |
| `scene_editorid` | TEXT | Set for scene dialogue |
| `topicinfo_formid` | INTEGER | INFO FormID that was selected |
| `response_text` | TEXT | The line captured at log time |
| `voice_filepath` | TEXT | Voice file path |
| `responses_json` | TEXT | JSON string array of **all** responses for the topic (`allResponses`). Added by `UpdateSchema`. |
| `blocked_status` | INTEGER NOT NULL | `BlockedStatus` value **at log time**. The UI recomputes it on read (see below). |
| `is_scene` / `is_bard_song` / `is_hardcoded_scene` | INTEGER NOT NULL (0/1) | Classification flags |
| `source_plugin` | TEXT | Plugin that added this INFO. Added by `UpdateSchema`. |
| `topic_source_plugin` | TEXT | Plugin that owns the DIAL, used for plugin blacklisting. Added by `UpdateSchema`. |
| `skyrimnet_blockable` | INTEGER DEFAULT 0 | **Legacy** SkyrimNet flag. Still written, no longer used by any filter. |

`BlockedStatus`: 0 Normal, 1 SoftBlock, 2 HardBlock, 3 SkyrimNetBlock
(legacy), 4 FilteredByConfig, 5 ToggledOff, 6 Whitelisted.

### `blacklist` and `whitelist` (identical shape)

Both tables have the same 19 columns in the same order (on fresh databases;
upgraded ones can differ, which is why `ReadListEntry()` reads by name), and
both are read and written as `BlacklistEntry`.

| # | Column | Type | Meaning |
|---|---|---|---|
| 0 | `id` | INTEGER PK AUTOINCREMENT | |
| 1 | `target_type` | INTEGER NOT NULL | `BlacklistTarget`: 1 Topic, 2 Quest, 3 Subtype, 4 Scene, 5 Plugin, 6 Actor (ref FormID), 7 Faction |
| 2 | `target_formid` | INTEGER | Runtime FormID when saved, or 0 (subtypes, imported scenes). Only matched on rows with no FormKey and no EditorID. Stored **signed** (see Gotchas). |
| 3 | `target_editorid` | TEXT | EditorID, matched after the FormKey. For Actor rows it holds the NPC name, for Faction rows the faction EditorID. |
| 4 | `block_type` | INTEGER NOT NULL | `BlockType`: 1 Soft, 2 Hard, 3 SkyrimNet (**legacy**). It is stored in whitelist rows too but has no meaning there. |
| 5 | `added_timestamp` | INTEGER NOT NULL | Unix seconds. **0** for rows from `ImportHardcodedScenes` and YAML import, which don't set it. |
| 6 | `notes` | TEXT | Free text. The string `Auto-added from scene: <SceneEDID>` is load-bearing (see Gotchas). |
| 7 | `response_text` | TEXT | Usually a JSON string array of the target's responses (from enrichment). It can be plain text from older rows. |
| 8 | `subtype` | INTEGER DEFAULT 0 | Subtype number (14 = "Scene" for scene rows) |
| 9 | `subtype_name` | TEXT | Human-readable subtype |
| 10 | `filter_category` | TEXT DEFAULT `'Blacklist'` (`'Whitelist'` in whitelist) | Which toggle gates the row: `Blacklist`, `Scene`, `BardSongs`, `FollowerCommentary`, or a subtype name. Checked with `Config::IsFilterCategoryEnabled`. |
| 11 | `block_skyrimnet` | INTEGER DEFAULT 1 | **Legacy** SkyrimNet flag. Still round-tripped, not acted on. |
| 12 | `source_plugin` | TEXT | Plugin filename, used for ESL-safe dedupe |
| 13 | `quest_editorid` | TEXT | Quest EditorID, used for ESL-safe dedupe |
| 14 | `actor_filter_formids` | TEXT DEFAULT `'[]'` | JSON array of `"0x%08x"` strings |
| 15 | `actor_filter_names` | TEXT DEFAULT `'[]'` | JSON array of names, index-paired with 14 |
| 16 | `faction_filter_editorids` | TEXT DEFAULT `'[]'` | JSON array of faction EditorIDs |
| 17 | `target_formkey` | TEXT DEFAULT `''` | FormKey of the target (`"02707A:Skyrim.esm"`), the primary match key. Added in 1.2.0; `''` on older rows until they are saved again. |
| 18 | `actor_filter_formkeys` | TEXT DEFAULT `'[]'` | JSON array of FormKeys, index-paired with 14/15; `""` where unresolved. Added in 1.2.0. |

Constraint: `UNIQUE(target_type, target_formid, target_editorid)`.
`blockAudio` and `blockSubtitles` on `BlacklistEntry` are **not stored**.
`GetBlacklist()` and `GetWhitelist()` derive them from `block_type`
(SkyrimNet → false, otherwise true).

### `meta`

`key TEXT PRIMARY KEY, value TEXT`. `GetMetaFlag()` returns true only when
`value == "1"`.

| Key | Set by | Purpose |
|---|---|---|
| `hardcoded_scenes_initialized` | `main.cpp` `kDataLoaded` | First-run import of ambient scenes (category `Scene`) and follower-commentary scenes (`FollowerCommentary`). Gating on the flag, not a row count, means that clearing or removing scenes never re-imports them. |
| `bard_scenes_initialized` | `SceneMonitor.cpp` `SceneMonitor::Initialize()` | First-run auto-add of every scene in the bard quests (category `BardSongs`, Hard) |

To force a re-import on a copy, `DELETE FROM meta WHERE key=...`. Explicit
re-import (MCM `ImportHardcodedScenes` or the UI `OnImportScenes`) ignores the
flags.

### Indexes (`CreateIndexes()`)

`dialogue_log`: `idx_timestamp`, `idx_topic_formid`, `idx_topic_subtype`,
`idx_quest_formid`, `idx_blocked_status`, `idx_speaker_name`.
`blacklist`: `idx_blacklist_target(target_type, target_formid, target_editorid)`,
which duplicates the UNIQUE autoindex, and `idx_blacklist_filter_category`.
`whitelist` only has its UNIQUE autoindex.

## Schema evolution

There is **no versioned migration system**: no `user_version`, no migration
table, no data transforms, no drops or renames. The author has declined to add
migrations. What does exist:

- `CreateTables()` uses `CREATE TABLE IF NOT EXISTS` with the current full shape,
  so fresh installs get every column in canonical order.
- `UpdateSchema()` is an idempotent **additive** patcher. A `columnExists`
  lambda (`PRAGMA table_info`) is followed by `ALTER TABLE ... ADD COLUMN`
  when the column is missing. It covers:
  - `dialogue_log`: `responses_json`, `source_plugin`, `topic_source_plugin`, `skyrimnet_blockable`
  - `blacklist`: `source_plugin`, `quest_editorid`, `faction_filter_editorids`
  - `whitelist`: `quest_editorid`, `faction_filter_editorids`
  - both: `target_formkey`, `actor_filter_formkeys` (1.2.0). Existing rows are **not** converted; they keep
    matching by EditorID (or FormID) and get FormKeys when saved again. See BLOCKING_RULES.md "Target identity".
- Legacy SkyrimNet columns (`skyrimnet_blockable`, `block_skyrimnet`) and enum
  values (`BlockType::SkyrimNet`, `BlockedStatus::SkyrimNetBlock`) stay so old
  rows still load. The SkyrimNet filter itself is gone.
- `actor_filter_formids` and `actor_filter_names` (both tables) and
  `whitelist.source_plugin` are **not** in `UpdateSchema`. A DB older than those
  columns would fail the prepared inserts. Unclear whether any such DB exists in
  the wild. Not verified.

## Write queue and threading

- **Enqueue:** only `PopulateTopicInfoHook.cpp` calls `LogDialogue()`, in two
  places: the hard-block path (~:332) and the main path (~:743). It fires on
  whatever thread the engine runs that hook on. `LogDialogue` first writes the
  line to `STFU_DialogueLog.txt` synchronously (`DialogueLogger::LogEntry`),
  then pushes a copy onto `pendingEntries_` under `queueMutex_`.
- **Auto-flush** happens inside `LogDialogue` when the queue holds at least
  `BATCH_SIZE` (200) entries, or when `FLUSH_INTERVAL_MS` (1000 ms) has passed
  since `lastFlushTime_`. The code comment says 5 seconds, but the constant is 1 s.
  There is **no timer**. The last few entries sit in the queue until the next
  `LogDialogue`, a `FlushQueue()`, or shutdown.
- **Explicit `FlushQueue()` callers:** `PrismaUIMenu::SendHistoryData()` (so the
  UI sees fresh rows), `main.cpp` `kPostLoadGame`, and `Close()`.
- **`ProcessQueue()`:** it swaps the queue out under `queueMutex_`, then takes
  `dbMutex_`, wraps all inserts in `BEGIN`/`COMMIT` using the prepared
  `insertDialogueStmt_`, and then prunes. Afterwards it posts
  `SendHistoryData()` via `SKSE::GetTaskInterface()->AddTask`, so the UI is
  refreshed on the main thread after the lock is released. An empty queue
  returns early, so the resend doesn't recurse.
- **Lock invariant:** `queueMutex_` and `dbMutex_` are **never held at the same
  time**. `Close()` calls `FlushQueue()` before taking `dbMutex_` for this
  reason. Breaking the invariant would bring back an AB-BA deadlock.
- `dbMutex_` is a `std::recursive_mutex` because `ImportHardcodedScenes` →
  `AddToBlacklist` and `SetMetaFlag` → `SetMetaValue` re-enter it.
- Blacklist and whitelist writes, and all reads, are synchronous. `ShouldSoftBlock`
  runs several un-cached prepared queries per call on the hook's hot path.
- MCM `ImportHardcodedScenes` (`PapyrusInterface.cpp`) runs on a detached
  `std::thread`, so `AddToBlacklist` → `SceneHook::UpdateSceneConditions` runs
  off the main thread there. Not verified to be safe.

## History retention

`ProcessQueue()` ends with a hard `HISTORY_LIMIT = 100` (a local constant, not
configurable):
`DELETE FROM dialogue_log WHERE id IN (SELECT id ... ORDER BY timestamp ASC LIMIT n)`.
`SerializeHistoryToJSON` asks for `GetRecentDialogue(1000)`, but it can never
get more than 100 rows back. Users can also delete rows from the UI with
`DeleteDialogueEntriesBatch`. Nothing runs `VACUUM` and `auto_vacuum` is off,
so freed pages stay in the file. The author's DB is ~70 MB, of which ~69 MB is
freelist (16,900 of 17,092 pages), with ~100 live history rows.

## Public API

**Lifecycle / meta**
| Method | Purpose |
|---|---|
| `Initialize(path)` / `Close()` | Open, WAL, create/patch schema, prepare statements / flush, finalize, close |
| `GetMetaValue` / `SetMetaValue` | Raw key/value in `meta` (`INSERT OR REPLACE`) |
| `GetMetaFlag` / `SetMetaFlag` | "1"/"0" booleans on top of the above |

**History**
| Method | Purpose |
|---|---|
| `LogDialogue(entry)` | Text-log the entry and enqueue it for a batched insert |
| `FlushQueue()` | Drain the queue now and reset the flush timer |
| `GetRecentDialogue(limit)` | `SELECT * ... ORDER BY timestamp DESC`. Reads columns by name via `GetColumnIndex`. |
| `DeleteDialogueEntriesBatch(ids)` | Delete rows by id in one transaction |

**Blacklist**
| Method | Purpose |
|---|---|
| `AddToBlacklist(entry, skipEnrichment)` | **Upsert.** Optionally enriches `response_text` via `TopicResponseExtractor` (Topic/Scene). Assigns FormKeys (`ResolveFormKeys`), then dedupes with `FindExistingEntry` (FormKey, EditorID, keyless FormID, then the pre-1.2.0 quest + plugin + `formid & 0xFFF` triple). On update it overwrites **every** field. It reads back the actor filters to verify them, then calls `SceneHook::UpdateSceneConditions` (Scene) or `UpdateSceneConditionsForTopic` (Topic with EditorID). |
| `RemoveFromBlacklist(id)` | Delete, then lift scene conditions (passes blockType 1 = "remove") |
| `RemoveFromBlacklistBatch(ids)` | Same in one transaction. Also deletes `Auto-added from scene:` topics for removed scenes. |
| `ClearBlacklist()` | `DELETE FROM blacklist`. Does **not** reset scene conditions (a stale-until-reload comment says so). |
| `GetBlacklist()` | All rows, ordered by `added_timestamp`, via `ReadListEntry()` (reads columns by name). |
| `GetBlacklistEntryId(formID, edid)` | First matching id of any target type, or -1 |
| `ImportHardcodedScenes(edids, category)` | Upserts Scene/Hard rows with `skipEnrichment=true`, `responseText="[]"`, plugin guessed from the `DLC1`/`DLC2` prefix |
| `EnrichBlacklistEntryAtRuntime(type, edid, text \| vector)` | Appends unseen responses to a row's `response_text` JSON. Called from `PopulateTopicInfoHook` (~:749). |

**Whitelist**
| Method | Purpose |
|---|---|
| `AddToWhitelist(entry, skipEnrichment)` | Upsert that forces `filterCategory="Whitelist"`. Same FormKey resolution and dedupe as the blacklist, no scene side effects. |
| `RemoveFromWhitelist(id)` / `ClearWhitelist()` / `GetWhitelist()` | As for the blacklist |
| `IsWhitelisted(type, formID, edid, actorFormID, actorName, actorRef)` | Typed lookup. Checks actor filters and faction filters. |

**Query**
| Method | Purpose |
|---|---|
| `ShouldSoftBlock(formID, edid, actorFormID, actorName, actorRef)` | Evaluates in this order: whitelist row → Actor whitelist (type 6) → Faction whitelist (type 7) → blacklist row (actor/faction filters, SkyrimNet skip, `IsFilterCategoryEnabled`) → Actor blacklist → Faction blacklist. Full semantics in BLOCKING_RULES.md. |

## JSON-encoded columns

`responses_json`, `response_text` (mostly), and the three `*_filter_*` columns
hold JSON **string arrays**. They are produced and parsed by the hand-rolled
helpers in `DialogueDatabase_Json.cpp`. The code does not use a JSON library.

- `ResponsesToJson` / `JsonToResponses` handle generic string arrays. Escaping
  covers `" \ \b \f \n \r \t` only, so `\uXXXX` is not decoded.
- `ActorNamesToJson` and `FactionEditorIDsToJson` are aliases of `ResponsesToJson`.
- `ActorFormIDsToJson` writes `["0x000a2c94", ...]`. `ParseActorFormIDsFromJson`
  reads the values back with `stoul(..., 0)` and warns on failure.
- `actor_filter_formkeys` is a plain string array (`ResponsesToJson`).
- `ActorMatchesFilter` matches an index by FormKey when it has one, otherwise
  when the name **and** `formID & 0xFFF` match (pre-1.2.0 filters). An empty
  actor filter returns false, meaning "no actor match". The callers decide that
  "no filters at all" means "applies to everyone".
- `FactionMatchesFilter` / `GetActorFactionEditorIDs` iterate over
  `actorBase->factions`. That covers **base** NPC factions only, not factions
  added at runtime.

## How the UI reads it

The React UI never touches SQLite. The C++ handlers in `src/PrismaUIMenu_*.cpp`
call the API and push JSON back to the page (see PRISMA_UI_BRIDGE.md):

- History: `PrismaUIMenu_History.cpp` `SendHistoryData()` → `FlushQueue()` →
  `SerializeHistoryToJSON()`, which loads history, blacklist and whitelist and
  **recomputes each row's `blockedStatus`** against the current rules and
  toggles. The stored `blocked_status` is only a log-time snapshot.
- Blacklist / whitelist tabs: `PrismaUIMenu_Blacklist.cpp` and
  `PrismaUIMenu_Whitelist.cpp` call `GetBlacklist`, `GetWhitelist`,
  `AddTo*`, `RemoveFrom*` and `RemoveFromBlacklistBatch`.
  `PrismaUIMenu_Entries.cpp` (~:449) adds rows from the history view.
- Settings: `PrismaUIMenu_Settings.cpp` `OnImportScenes` re-imports the scene lists.

## How to inspect the live DB

**Never open the live file while Skyrim is running.** A second connection can
take locks or checkpoint the WAL under the game, and the MO2 VFS view differs
from the real disk. Close the game, then copy **all three files**. The copy
needs the `-wal` file, which can hold data not yet checkpointed into
`dialogue.db`.

```bash
SRC="/c/Nolvus/Instances/Nolvus Awakening/MODS/overwrite/SKSE/Plugins/STFU/data"
mkdir -p /c/tmp/stfu-db && cp "$SRC"/dialogue.db* /c/tmp/stfu-db/
sqlite3 /c/tmp/stfu-db/dialogue.db     # or: python -c "import sqlite3; ..."
```

`sqlite3.exe` is not on PATH on the author's machine. Python's built-in
`sqlite3` module works.

```sql
-- latest history, FormIDs as unsigned hex
SELECT datetime(timestamp,'unixepoch','localtime') t, speaker_name, topic_editorid,
       topic_subtype_name, blocked_status, printf('%08X', topicinfo_formid & 0xFFFFFFFF) info
FROM dialogue_log ORDER BY timestamp DESC LIMIT 20;

-- rule counts by category / type / block type
SELECT filter_category, target_type, block_type, COUNT(*) FROM blacklist GROUP BY 1,2,3;

-- find a rule by EditorID and show its filters
SELECT id, target_type, printf('%08X', target_formid & 0xFFFFFFFF) fid, target_editorid,
       block_type, filter_category, actor_filter_names, faction_filter_editorids
FROM blacklist WHERE target_editorid LIKE '%Hello%';

-- one-shot flags
SELECT * FROM meta;

-- actor/faction-scoped rules
SELECT * FROM blacklist WHERE actor_filter_names <> '[]' OR faction_filter_editorids <> '[]';

-- file bloat
PRAGMA page_count; PRAGMA freelist_count;
```

To test edits, change the **copy**, then put it back with the game closed. Keep
a backup first.

## How to add a column (no-migrations policy)

Follow the pattern that already exists: an additive, idempotent column with a
safe default.

1. Add the column at the **end** of the `CREATE TABLE` in `CreateTables()`, with
   a `DEFAULT` that preserves the old behaviour.
2. Add a `columnExists` → `ALTER TABLE ... ADD COLUMN ... DEFAULT ...` block in
   `UpdateSchema()`. For blacklist and whitelist columns, do both tables.
3. Extend the prepared INSERT in `PrepareStatements()`, and the UPDATE in
   `AddToBlacklist` / `AddToWhitelist`. Renumber the `?` binds carefully.
4. Reading:
   - `GetRecentDialogue` looks columns up by name, so a new column just needs
     `GetColumnIndex` plus a null check.
   - `GetBlacklist` and `GetWhitelist` go through `ReadListEntry()`, which also
     reads by name; add the field there.
5. Add the field to `DialogueEntry` / `BlacklistEntry`, and to the JSON the
   `PrismaUIMenu_*` handlers send (see PRISMA_UI_BRIDGE.md).

Trade-off: this works for adding columns and nothing else. You can't rename,
retype or drop a column, or backfill or transform existing data, so wrong
columns live forever (see the SkyrimNet columns). On a DB where `ALTER`
appended columns in a different order than `CREATE` declares, column N holds
different data, so never read list columns by position. Every existing user DB
has to keep loading, so never reorder or remove columns, and never change the
meaning of an existing value.

## Log lines to grep (STFU.log)

| Line | Meaning |
|---|---|
| `Initializing database at: <path>` / `Database initialized: <path>` | Open OK. Check the path when MO2 redirection is in doubt. |
| `[DialogueDB] Failed to open database` / `Failed to create ... table` / `Failed to update schema` | Init failure. The DB is disabled for the whole session. |
| `[DialogueDB] Adding <col> column to <table>` | `UpdateSchema` patched an older DB |
| `Importing hardcoded scenes (first-run)...` / `Hardcoded scenes already initialized, skipping auto-import` | Meta-flag gate |
| `SceneMonitor: bard scenes already initialized, skipping auto-population` | Bard flag gate |
| `[DialogueDB] Flushing N queued dialogue entries` (debug) | Queue flush |
| `[DialogueDB] History limit exceeded (n/100), deleting k oldest entries` | Pruning (info, fires on nearly every flush once full) |
| `[DialogueDB] Failed to insert dialogue entry for TopicInfo 0x...` | Insert failure (schema mismatch?) |
| `[DialogueDB] Updating existing blacklist entry (id=...)` / `Inserting new blacklist entry` | Upsert path taken |
| `[DialogueDB] VERIFICATION FAILED: ...` | Actor filter read-back mismatch |
| `[DialogueDB] ShouldSoftBlock: ...` (debug) | Per-decision trace. Very noisy. |
| `[DialogueDB] Column '<x>' not found in query result` | `GetColumnIndex` miss (older DB) |
| `[DialogueDB] Runtime enriched blacklist entry ...` | Responses appended |

Debug-level lines need `stfu_debug.flag` (see DEVELOPMENT.md).

## Gotchas / invariants

- **Signed FormIDs.** FormIDs are bound with `sqlite3_bind_int`, so values of
  `0x80000000` and above (every `FE`/`FF` light-plugin ID) are stored negative
  (e.g. `_W_HealerFaction` = `-33277950`). Comparisons still work because both
  sides are bound the same way. Use `& 0xFFFFFFFF` in ad-hoc SQL, and never
  mix `bind_int64` for FormIDs.
- **Match order is FormKey, EditorID, then FormID for keyless rows**
  (`kTargetMatchSql` / `kActorMatchSql` in `DialogueDatabaseInternal.h`). The
  SQL and the C++ helpers (`EntryMatchesTarget` etc.) must stay in step. Without
  po3 Tweaks on AE, EditorIDs come back empty, so rows without a FormKey fall
  back to FormID.
- **`target_formid` is not a stable identity.** It is the FormID at save time.
  Never match on it for rows that have a FormKey, and never derive a FormKey
  from a saved FormID without confirming the record (see `ResolveFormKeys`).
- **The main `ShouldSoftBlock` lookups exclude Actor and Faction rows** but
  otherwise don't filter on `target_type`, so a Scene, Quest or Subtype row
  with a matching EditorID or FormKey can hit.
- **Re-import and upsert overwrite curation.** `AddToBlacklist` on an existing
  row rewrites every column, so importing the scene lists again resets
  block type, category and notes, and **clears actor/faction filters** on
  those scenes.
- **`Auto-added from scene: <EDID>`** in `notes` is used as a foreign key.
  Changing a scene's block, or removing it in a batch, deletes Topic rows
  carrying that note. Don't reuse that prefix for anything else.
- `ClearBlacklist()` leaves patched scene conditions in place until the next
  reload.
- The readers `GetRecentDialogue` / `GetBlacklist` / `GetWhitelist` assign
  some `sqlite3_column_text` results straight to `std::string` without a
  null check (`speaker_name` etc. in history, `target_editorid` in the lists).
  A NULL there, such as from a hand-edited DB, is undefined behaviour. Insert
  `''`, never NULL.
- Don't call PrismaUI from inside a DB lock. Queue the call with
  `AddTask` as `ProcessQueue` does, because doing it inside the lock can
  deadlock (notably under VR threading).
- The history table is a 100-row ring. Don't build features that assume
  long-term history.

## Related docs

- ARCHITECTURE.md: where the DB sits in the plugin
- BLOCKING_RULES.md: full decision semantics built on `ShouldSoftBlock` / `IsWhitelisted`
- SCENE_BLOCKING.md: `SceneHook::UpdateSceneConditions*`, category globals, pre-included scene lists
- DIALOGUE_HOOKS.md: where `LogDialogue` and enrichment are called
- PRISMA_UI_BRIDGE.md: the listeners that read and write the DB
- CONFIG_AND_SETTINGS.md: INI, `IsFilterCategoryEnabled`, YAML import
- DEVELOPMENT.md: build, deploy, debug flag
- DIALOGUE_SUBTYPES.md: subtype numbers stored in `topic_subtype` / `subtype`
