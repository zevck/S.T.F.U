# Blocking Rules

How STFU decides whether a line of NPC dialogue is blocked, and what kind of block it gets.
The decision is a runtime query against the SQLite `blacklist` / `whitelist` tables
(see DATABASE.md), combined with the TESGlobal toggles from `STFU.esp` (see CONFIG_AND_SETTINGS.md).
There is **no in-memory rule cache**: every evaluation hits SQLite. This doc covers the
rules. The hooks that call them are in DIALOGUE_HOOKS.md, and scene phase-condition gating is in SCENE_BLOCKING.md.

## Overview

```
PopulateTopicInfo hook (new TopicInfo, not a duplicate)
 │
 ├─(1) HARD pre-check ── Topic/Quest entry with BlockType::Hard? ──yes──> return 0
 │     (inline in the hook, reads the full blacklist via GetBlacklist())    (line never happens:
 │                                                                         no audio, no anim,
 │                                                                         no scripts)
 ├─(2) Config::ShouldSoftBlock(quest, topic, speakerName, text, speakerFormID, speakerRef)
 │        ├─ DialogueDB::Database::ShouldSoftBlock(topic ids, actor)  ← whitelist+blacklist+actor/faction
 │        ├─ owning-scene lookup → Database::ShouldSoftBlock(scene ids) (no actor)
 │        ├─ Database::IsWhitelisted(Topic / Quest, actor, ref)
 │        └─ MCM subtype toggle (+ PreserveGrunts exception)
 │     true → responseText = "" + SetEarlyBlockFlag + SetBlockingDecision(true)
 │
 └─ ConstructResponse / SetSubtitle hooks consume the cached decision (DIALOGUE_HOOKS.md)
```

"Soft block" means the line still runs (scripts, quest stages, packages fire), but its audio and subtitle are
suppressed. "Hard block" means the line never happens. For topics that is the early `return 0` above. For scenes it
means the scene is prevented from starting via phase conditions (SCENE_BLOCKING.md).

## Key files / types / functions

| Location | What |
|---|---|
| `include/DialogueDatabase.h` `BlacklistTarget`, `BlockType`, `BlockedStatus`, `BlacklistEntry` | Rule data model. Whitelist rows use the same struct. |
| `src/Config_Blocking.cpp` `Config::ShouldSoftBlock()` | Top-level soft-block decision. This is the precedence chain. |
| `src/DialogueDatabase_Query.cpp` `Database::ShouldSoftBlock()` | Per-ID DB check: whitelist, then actor/faction whitelist, then blacklist, then actor/faction blacklist. |
| `src/DialogueDatabase_Whitelist.cpp` `Database::IsWhitelisted()` (2 overloads) | Target-typed whitelist check with actor/faction filters. |
| `src/DialogueDatabase_Json.cpp` `ActorMatchesFilter()`, `FactionMatchesFilter()`, `GetActorFactionEditorIDs()` | Actor/faction filter matching. |
| `src/DialogueDatabase_Blacklist.cpp` `Database::AddToBlacklist()` | Insert/upsert with ESL-safe dedupe. Pushes scene condition updates. |
| `src/Config.cpp` `IsFilterCategoryEnabled()` | Maps `filterCategory` to its governing TESGlobal. |
| `src/Config.cpp` `SubtypeNameMap`, `SubtypeGlobalMap`, `VanillaSubtypeCorrections`, `GetAccurateSubtype()`, `GetSubtypeName()`, `ToggleSubtypeFilter()` | Subtype tables and toggles. |
| `src/Config_Blocking.cpp` `IsFilteredByMCM()`, `HasDisabledSubtypeToggle()`, `ShouldFilterFromHistory()` | UI status helpers and the grunt list. |
| `src/Config_Yaml.cpp` `ImportYAMLListSections()`, `ImportSubtypeOverrides()`, `ParseFormIdentifierInternal()` | YAML to DB rule import. |
| `src/PrismaUIMenu_Entries.cpp` `OnCreateAdvancedEntry()` | The only UI path that creates rules (listener `createAdvancedEntry`). |
| `src/PrismaUIMenu_History.cpp` `SerializeHistoryToJSON()` | A **separate** re-implementation of the rules, used only for history status badges (see Gotchas). |

## Data model

### `BlacklistTarget` (column `target_type`)

| Value | Name | Created by | Evaluated at runtime? |
|---|---|---|---|
| 1 | Topic | UI (FormID/EditorID of a `TESTopic`), YAML `topics:`, YAML `quests:` (expanded into one Topic row per topic), subtype overrides | Yes. Soft via `Database::ShouldSoftBlock`, Hard via the PopulateTopicInfo pre-check |
| 2 | Quest | **Nothing creates these today.** The YAML `quests:` section expands to Topic rows. | Blacklist: Hard only (PopulateTopicInfo pre-check). Whitelist: `IsWhitelisted(Quest, …)` in `Config::ShouldSoftBlock` |
| 3 | Subtype | Nothing | No. Display only (`PrismaUIMenu_Blacklist.cpp`) |
| 4 | Scene | UI, YAML `scenes:`, pre-included scene import, `SceneMonitor.cpp` (bard songs) | Soft: via the owning-scene lookup in `Config::ShouldSoftBlock`. Hard: phase conditions (SCENE_BLOCKING.md) |
| 5 | Plugin | UI (identifier ending `.esp/.esm/.esl`), whitelist YAML `plugins:` | **No.** No runtime code checks Plugin rows |
| 6 | Actor | UI (a FormID that resolves to an `Actor` ref). `targetEditorID` holds the actor's display **name** | Yes. Exact **reference** FormID match |
| 7 | Faction | UI (an EditorID that resolves to a `TESFaction`) | Yes. Match by faction **EditorID** against the speaker's base-NPC factions |

### `BlockType` (column `block_type`)

| Value | Name | Meaning |
|---|---|---|
| 1 | Soft | Silence audio and subtitle. The line still executes. |
| 2 | Hard | Topic: prevented entirely (`return 0` in PopulateTopicInfo). Scene: prevented via phase conditions. **A Hard row also soft-blocks**: `Database::ShouldSoftBlock` treats Hard like Soft, which covers the case where the prevention path doesn't fire. |
| 3 | SkyrimNet | **Legacy.** The SkyrimNet filter was removed. `ShouldSoftBlock` explicitly returns "allow" for these rows, and `GetBlacklist()` derives `blockAudio=blockSubtitles=false`. Kept so old rows load. The UI can still create one if sent `blockType:"SkyrimNet"`. |

Actor and Faction targets are always forced to Soft (`OnCreateAdvancedEntry`: "hard block would be game-breaking").

### `BlacklistEntry` fields that affect matching

| Field | Column | Role |
|---|---|---|
| `targetFormKey` | `target_formkey` | Load-order independent identity, `"02707A:Skyrim.esm"` (see `src/FormKey.h`). Set whenever a row is saved from 1.2.0 on; empty on older rows until they are saved again. |
| `targetFormID` | `target_formid` | Runtime FormID when saved. Only used for matching on rows with no FormKey and no EditorID. |
| `targetEditorID` | `target_editorid` | EditorID, or actor name (Actor), plugin filename (Plugin). |
| `questEditorID` + `sourcePlugin` | `quest_editorid`, `source_plugin` | ESL-safe identity for Topic rows. Used in dedupe and the Hard pre-check only. |
| `filterCategory` | `filter_category` | Which toggle gates the row (next section). Default `'Blacklist'`. Whitelist rows are forced to `'Whitelist'`. |
| `actorFilterFormIDs` + `actorFilterNames` + `actorFilterFormKeys` | `actor_filter_formids`, `actor_filter_names`, `actor_filter_formkeys` (JSON arrays) | **Parallel arrays**: index `i` is one actor. Empty = applies to everyone. A key is `""` when it couldn't be resolved (pre-1.2.0 filter, runtime-spawned actor). |
| `factionFilterEditorIDs` | `faction_filter_editorids` (JSON) | Row applies only to speakers in one of these factions. |
| `blockAudio`, `blockSubtitles` | *(not stored)* | Derived from `blockType` on load. Nothing reads them for decisions. |
| `blockSkyrimNet` | `block_skyrimnet` | Legacy. Not read by the blocking path. |

## filterCategory → governing toggle

`Config::IsFilterCategoryEnabled(category)` (`src/Config.cpp`). A blacklist row only blocks when this returns true.
Everything is compared as `global->value >= 0.5`.

| filterCategory | TESGlobal (EditorID) | UI / MCM label | Typical rows |
|---|---|---|---|
| `Blacklist` (default; also used when the column is empty/NULL) | `STFU_Blacklist` | "Enable Blacklist Filter" / MCM "Block Blacklisted Topics" | User-added entries, YAML blacklist |
| `Scene` | `STFU_Scenes` | Block Ambient Scenes | Pre-included ambient scenes |
| `BardSongs` | `STFU_BardSongs` | Block Bard Songs | Bard song scenes |
| `FollowerCommentary` | `STFU_FollowerCommentary` | Block Follower Commentary | Follower commentary scenes |
| `<SubtypeName>` e.g. `Hello`, `Idle`, `MurderNC` | `STFU_<SubtypeName>` via `SubtypeGlobalMap` | Per-subtype toggle | Subtype overrides, UI category picker |
| anything else (incl. `Whitelist`, `SkyrimNet`, subtypes with no global) | none | n/a | **Always evaluates false, so the row never blocks** |

So the "master" Enable Blacklist Filter switch is not a global kill switch. It only gates rows whose category is
`Blacklist`. Scene, BardSongs, FollowerCommentary and subtype-category rows, plus the MCM subtype filter itself, ignore it.
The Hard topic pre-check ignores **all** toggles (see Gotchas).

For scenes, the same category maps to the phase-condition gate via `Config::GetSceneGateGlobalForCategory()`
(`src/Config_Scenes.cpp`). That function falls back to `STFU_Scenes` for unknown categories, whereas
`IsFilterCategoryEnabled` falls back to `false`.

## Order of evaluation: `Config::ShouldSoftBlock`

Inputs: `quest` (= `topic->ownerQuest`), `topic`, `speakerName`, `responseText`, `speakerFormID` (**reference**
FormID), `speakerRef`. The first step that decides wins.

```
Config::ShouldSoftBlock
│
├─ A. db->ShouldSoftBlock(topicFormID, topicEditorID, speakerFormID, speakerName, speakerRef)
│     A1 whitelist row matching topic ids (kTargetMatchSql, LIMIT 1)
│         no actor/faction filter ............................ → A returns false (continue to B)
│         filter + speaker/faction matches ................... → A returns false (continue to B)
│         filter doesn't match ............................... → fall through
│     A2 whitelist Actor row (type 6, target_formid == speakerFormID) → A returns false
│     A3 whitelist Faction row (type 7, editorid ∈ speaker base factions) → A returns false
│     A4 blacklist row matching topic ids (kTargetMatchSql, LIMIT 1)
│         has actor/faction filter & no speaker info ......... → A returns false
│         has filter & speaker matches neither ............... → A returns false
│         BlockType::SkyrimNet .............................. → not blocked
│         !IsFilterCategoryEnabled(category) ................. → not blocked
│         else (Soft or Hard) ............................... → blocked
│     A5 if not blocked: blacklist Actor row (type 6, exact ref FormID), category-gated
│     A6 if not blocked: blacklist Faction row (type 7, per base faction EditorID), category-gated
│     → true ⇒ BLOCK
│
├─ B. if quest: find FIRST scene in quest->scenes whose dialogue action uses this topic
│       db->ShouldSoftBlock(sceneFormID, sceneEditorID)      ← NO actor info
│       → true ⇒ BLOCK
│
├─ C. db->IsWhitelisted(Topic, topic ids, speaker, ref)  → true ⇒ ALLOW
│     db->IsWhitelisted(Quest, quest ids, speaker, ref)  → true ⇒ ALLOW
│
├─ D. subtype = GetAccurateSubtype(topic)
│     subtypeGlobals[subtype] exists && value > 0.0
│         PreserveGrunts exception: STFU_PreserveGrunts < 0.5 AND responseText is a known grunt
│         (ShouldFilterFromHistory) ................................... → fall through (ALLOW)
│         else ........................................................ → BLOCK
│
└─ E. ALLOW
```

### Precedence consequences (verified against code)

| Question | Answer |
|---|---|
| Topic whitelist vs topic blacklist | Whitelist wins (A1 short-circuits A4). |
| Actor/faction whitelist vs topic blacklist | Whitelist wins (A2/A3 run before A4). |
| Actor/faction whitelist vs actor/faction blacklist | Whitelist wins. |
| Topic whitelist vs **scene** blacklist of the scene containing the topic | **Scene blacklist wins.** A returns false, then B blocks before C runs. |
| Actor whitelist vs scene blacklist | Scene blacklist wins (B passes no actor). |
| Quest whitelist vs topic blacklist | **Topic blacklist wins.** Quest whitelist is only checked in C. |
| Any whitelist vs MCM subtype toggle | Whitelist wins (C before D). This is intentional per the comment in `Config_Blocking.cpp`. |
| Blacklist row whose category toggle is off vs MCM subtype toggle on | Still blocked by D. |
| Anything vs Hard topic/quest row | Hard pre-check wins. It runs before `Config::ShouldSoftBlock` and ignores whitelist, toggles and actor filters. |

## Matching rules

### Target identity (FormKey, EditorID, FormID)

Runtime FormIDs change with the load order (and light plugins are re-slotted), so rows identify records by
**FormKey**: the ID local to the plugin whose load-order slot the FormID is in, plus that plugin's filename,
e.g. `02707A:Skyrim.esm` or `000801:MyMod.esl`. `src/FormKey.h` converts both ways using a plugin table
built once after data load.

A row matches a record if, in order:

1. it has a FormKey equal to the record's FormKey, or
2. it has an EditorID equal to the record's EditorID, or
3. it has neither a FormKey nor an EditorID, and its saved FormID equals the record's FormID (pre-1.2.0 rows only;
   this is the one case that still breaks when the load order changes).

The SQL form is `kTargetMatchSql` (`src/DialogueDatabaseInternal.h`), used by `Database::ShouldSoftBlock`,
`IsWhitelisted` and `GetBlacklistEntryId`. The C++ form is `DialogueDB::EntryMatchesTarget()`, used by the history
status badges and the Hard quest pre-check. Callers pass current-session FormIDs; the FormKey is computed from them
inside the lookup. `kTargetMatchSql` excludes Actor and Faction rows (types 6/7), whose `target_editorid` holds a
name or faction EditorID rather than the record's own.

FormKeys are assigned by `ResolveFormKeys()` whenever a row is saved (`AddToBlacklist` / `AddToWhitelist`). It only
converts a FormID once something confirms it still means the same record: an EditorID lookup of the right form type,
the actor's name, or (for a brand-new entry, `id == 0`) the fact that the FormID came from this session. A saved
row's FormID can be from an older load order, so rows that can't be confirmed stay keyless and keep rule 3. There is
**no migration**: old rows get a FormKey only when they are saved again (for example edited in the menu).

EditorIDs come from `STFU::GetEditorID()` (`src/EditorID.h`). On AE this needs powerofthree's Tweaks, otherwise most
forms have no EditorID.

The **Hard pre-check** in `PopulateTopicInfoHook.cpp` is a C++ lambda over `GetBlacklist()`. A topic row matches on
FormKey, the ESL-safe triple `questEditorID` + `sourcePlugin` + `(targetFormID & 0xFFF)` (pre-1.2.0 rows), EditorID
alone, or (keyless rows) full FormID. Quest rows use `EntryMatchesTarget`.

`FindExistingEntry()` (dedupe on save) uses the same identity, per target type: FormKey, else EditorID (not for Actor
rows, whose EditorID column is a name), else a keyless row's FormID; then the ESL-safe triple for old topic rows.
A match updates the existing row in place, including `block_type`.

### Actor / faction filters (per-row)

`ActorMatchesFilter()` (`src/DialogueDatabase_Json.cpp`), per filter index `i`:

- **Filter has a FormKey:** matches when the speaker reference's FormKey equals it.
- **No FormKey** (pre-1.2.0 filter, or an actor whose FormKey couldn't be resolved): matches when the name equals
  `names[i]` **and** the low 12 bits of the reference FormID equal those of `formIDs[i]`. The low 3 hex digits are the
  only part of a FormID that survives a load-order change for both full and light plugins; the name disambiguates.
  Caveats of this fallback: same-named refs sharing the last 3 digits both match (about 1 in 4096 per pair), renamed
  actors stop matching, and a FormID-only filter with no names never matches (the loop is bounded by the shorter
  array; `OnCreateAdvancedEntry` rejects unpaired arrays).

Runtime-spawned refs (`FF……`) have no FormKey and get a new FormID per spawn, so no filter can follow them.

When a row is saved, `ResolveFormKeys()` recomputes the filter keys: a key the row already had stays attached to its
actor (matched by current FormID, so it survives even if the actor isn't loaded), a new actor gets a key if it is
loaded and its name matches, otherwise `""`. The list serializers send the menu current-session FormIDs resolved from
the keys (`FormKey::CurrentFormID`), so a filter edited after a load-order change round-trips correctly.

`FactionMatchesFilter()` uses `GetActorFactionEditorIDs()`, which reads **`actorBase->factions`** (the TESNPC's
static faction list). Runtime `AddToFaction` changes are invisible to it, and factions without an EditorID are skipped.

Filter semantics in `Database::ShouldSoftBlock` (A4): no filters means the row applies to everyone. With filters, the
speaker must match the actor filter **or** the faction filter. With filters and no speaker info passed, the row is
treated as **not matching** (allow).

### Actor / Faction target rows (types 6 and 7)

These are standalone rules ("silence Lydia everywhere"), distinct from the per-row filters above:

- Actor: `kActorMatchSql` / `EntryMatchesActor()`. FormKey of the reference, or for keyless pre-1.2.0 rows the exact
  saved reference FormID.
- Faction: `target_type = 7 AND target_editorid = <each base-NPC faction EditorID>`. Already load-order independent.
- Both are still category-gated (`IsFilterCategoryEnabled`) and SkyrimNet-typed rows are skipped.
- They are only consulted inside step A, so they need the actor args. The `ConstructResponse` fallback and the scene
  lookup (B) pass none. The logging-only call at `PopulateTopicInfoHook.cpp` near line 870 passes `speakerFormID` but
  no ref, so faction rules are skipped there.

## Subtypes

- `Config::GetAccurateSubtype(topic)` returns `VanillaSubtypeCorrections[formID]` if present. That map is **empty**:
  "infrastructure kept for future use". Otherwise it returns `topic->data.subtype`.
- `SubtypeNameMap` (`Config.cpp`) holds the canonical names ↔ IDs, 0–102. The reverse map is built once in `GetSubtypeName()`
  under a thread-safe static init. Don't regress this: hooks run on BSJobs worker threads. Unknown IDs render as `Unknown_<n>`.
- `SubtypeGlobalMap` (`Config.cpp`) holds subtype ID ↔ `STFU_<Name>` global. About 70 subtypes have a toggle. The ones
  without one are 0–2, 4–12 (ForceGreet, Rumors, the persuasion/favor/follow set), 14 Scene, 17 Refuse(!), 66–69,
  71–73, 83, 90, 95–97 and 100–102.
  Globals are resolved once in `Config::Load()` into `g_settings.mcm.subtypeGlobals`.
- **Two naming schemes.** `SubtypeNameMap` uses `MurderNPC`, `AssaultNPC`, `PickpocketNPC`, `StealFromNPC`,
  `TrespassAgainstNPC`, `WereTransformCrime`. The globals (and therefore `filterCategory` and the web UI's
  `TOPIC_CATEGORIES`) use `MurderNC`, `AssaultNC`, `PickpocketNC`, `StealFromNC`, `TrespassAgainstNC`,
  `WerewolfTransformCrime`. The INI `[Subtypes]` section uses `SubtypeNameMap` names.
- Note that `SubtypeGlobalMap` maps **13 → `STFU_Refuse`**, while `SubtypeNameMap` says 13 = Reject and 17 = Refuse.
  Check the ESP/MCM before "fixing" either side.
- Per-subtype toggle: step D blocks when `value > 0.0`. `IsFilteredByMCM()` and `HasDisabledSubtypeToggle()` (UI status)
  use `>= 0.5`. They are equivalent for 0/1 globals.
- `ToggleSubtypeFilter()` flips the global and calls `SettingsPersistence::SaveSettings()`.
- **Subtype overrides** (`import/STFU_SubtypeOverrides.yaml`, `overrides: {TopicId: SubtypeName}`) do **not** change a
  topic's subtype. `ImportSubtypeOverrides()` inserts a Soft **Topic blacklist row** with `filterCategory = SubtypeName`,
  so the topic is silenced whenever that subtype's toggle is on. The name is validated against `SubtypeNameMap`, but the
  gate is looked up via `SubtypeGlobalMap` names. So `MurderNPC`, `Custom`, `Rumors`, `Reject`, `CombatGrunt` and the
  like import "successfully" and then never block.
- Full list with descriptions: repo-root [DIALOGUE_SUBTYPES.md](../DIALOGUE_SUBTYPES.md).

### Grunts

`Config::ShouldFilterFromHistory()` returns true for subtypes 95/100/102 (breathing) or when the trimmed response
text case-insensitively equals one of the ~90 hardcoded grunt strings. It serves two purposes: hiding lines from
history, and (in step D) letting grunts through when a combat subtype is blocked but `STFU_PreserveGrunts < 0.5`.
The global name is inverted relative to its label. `STFU_PreserveGrunts = 1` means "Block Combat Grunts" is ON
(`PrismaUIMenu_Settings.cpp` `OnSetCombatGruntsBlocked`).

## Caching and call sites

- No rule cache. `Config.cpp` says so explicitly ("database lookups are fast enough and avoid cache invalidation issues").
  Every `Database::ShouldSoftBlock` prepares and finalizes 2–4+ statements under `dbMutex_`.
- The Hard pre-check calls `db->GetBlacklist()` (full table read and JSON parse) on every non-duplicate PopulateTopicInfo
  that has a topic and a speaker.
- Decision cache: `PopulateTopicInfoHook` calls `ConstructResponseHook::SetBlockingDecision(shouldSoftBlock, topicInfoFormID)`.
  ConstructResponse reuses it (`g_wasEvaluated`), and only re-evaluates (without actor context) when nothing was cached.
  Duplicate PopulateTopicInfo calls (same TopicInfo+speaker within 5 s, `IsDuplicateDialogue`) reuse
  `GetCachedSoftBlock()` and skip evaluation. Details are in DIALOGUE_HOOKS.md.
- TESGlobal pointers are cached in `g_settings` at `Config::Load()`. Values are read live, so toggles apply immediately.

| Call site | Purpose |
|---|---|
| `PopulateTopicInfoHook.cpp` `Hook_PopulateTopicInfo` ~L160–230 | Hard pre-check (inline) |
| same, ~L345 `Config::ShouldSoftBlock(..., a_speaker)` | **The** soft-block decision. Only runs when the TopicInfo has text. |
| same, ~L507 | Recomputed for the history `blockedStatus` |
| same, ~L809 `db->ShouldSoftBlock(sceneFormID, sceneEditorID)`, ~L870 | Scene path: logging and safety net only |
| `ConstructResponseHook.cpp` ~L196 | Fallback evaluation, no actor |
| `Config_Scenes.cpp` ~L263/297 `ShouldSoftBlock(0, sceneEditorID)` | Scene checks (SCENE_BLOCKING.md) |

## Worked examples

**1. Lydia's idle chatter, with a faction whitelist.** Rules: Topic `DialogueGenericIdle…` is blacklisted (Soft,
category `Blacklist`), and there is a whitelist Faction row `PotentialFollowerFaction`. Lydia speaks, and the hook passes her ref.
A1 finds no whitelist row for the topic. A2 finds no Actor row. A3: her base NPC has `PotentialFollowerFaction` →
`Database::ShouldSoftBlock` returns false. B: the topic isn't in a scene. C: no Topic/Quest whitelist. D: subtype
94 Idle; if `STFU_Idle = 1` the line is **blocked here anyway**. The faction whitelist (A3) only suppresses DB rows.
To exempt her from the subtype toggle too, you need a Topic or Quest whitelist row with a faction filter (step C).

**2. Toggling off the master switch.** A user blacklisted topic X from the menu. That row stores FormID `F` and
EditorID `X`, with category `Blacklist`, Soft. The user then imports a subtype override `X: Hello`. The override parses
to (FormID 0, EditorID `X`) and has no quest, so the dedupe predicate binds `target_formid = 0` and does not find the `F`
row. A **second** row (0, `X`, category `Hello`) is inserted. At runtime both rows match the topic, and `LIMIT 1` picks
one. In practice that is usually the first-inserted row, but it is unspecified. Result: turning off "Enable Blacklist
Filter" may or may not unblock X, depending on which row SQLite returns. If either row is **Hard**, nothing unblocks X
except deleting that row, because the pre-check ignores toggles.

## Log lines to grep (`STFU.log`; debug lines need `stfu_debug.flag`)

| Pattern | Level | Meaning |
|---|---|---|
| `[HARD BLOCK] Speaker:` | info | Hard pre-check fired (topic/quest). |
| `ShouldSoftBlock: Whitelisted` / `is whitelisted -> DON'T BLOCK` | debug | A1–A3 matched. |
| `ShouldSoftBlock: Found HARD\|soft block for` | debug | A4 blocked. The line shows the category. |
| `category='…' toggle DISABLED -> ALLOW` | debug | Row found but its gate is off. |
| `not in actor filter and not in faction filter -> ALLOW` | debug | Row found, filters excluded this speaker. |
| `filter exists but no speaker info provided -> ALLOW` | debug | Called without actor context (fallback/scene). |
| `Actor 0x… is blacklisted -> BLOCK` / `Actor's faction '…' is blacklisted` | debug | A5/A6. |
| `SkyrimNet-only block -> ALLOW` | debug | Legacy row hit. |
| `Faction match found: actor has faction` | debug | `FactionMatchesFilter` hit. |
| `[POPULATE] Clearing NPC response text for SOFT-BLOCKED` | debug | Soft decision applied. |
| `[ConstructResponse] No cached decision - evaluating` | debug | Fallback path (no actor filters applied). |
| `[STALE CACHE]` | warn | Cached decision belonged to another TopicInfo. |
| `Global '…' not found in loaded ESPs` | warn | A toggle global is missing, so its category never blocks. |
| `Loaded N subtype globals from STFU.esp` | info | Startup count of subtype toggles. |
| `Unknown subtype name '…' for topic` | warn | Bad override YAML. |

## Gotchas / invariants

- **Hard topic/quest rows bypass everything**: whitelist, `STFU_Blacklist`, category toggles, actor/faction filters.
  The UI lets users pick Hard for topics.
- The master "Enable Blacklist Filter" gates only `filterCategory == "Blacklist"` rows.
- A topic whitelist does not beat a blacklist on the topic's owning scene, and a quest whitelist does not beat a topic blacklist.
- Rows saved before 1.2.0 with **neither an EditorID nor a FormKey** (YAML `- 0x0501ABCD` entries) still match only the
  saved full FormID, so they break when the load order changes. Saving them again in 1.2.0+ gives them a FormKey.
- Plugin (5) and Subtype (3) rows are never evaluated. Quest (2) blacklist rows only work as Hard.
- `LIMIT 1` without `ORDER BY`: if several rows match the same record, which one decides is unspecified.
- Scene lookup (B) takes the **first** scene in `quest->scenes` that uses the topic. A topic shared by several scenes is
  judged by that one only.
- `SerializeHistoryToJSON()` re-implements the precedence for UI badges and **differs** from runtime. Record and actor
  matching use the shared `EntryMatchesTarget` / `EntryMatchesActor` / `EntryActorFilterMatches`, but it matches factions by
  FormID or EditorID, ignores category toggles for blacklist rows, and skips the quest whitelist. A badge can disagree
  with what actually happened in game.
- `ParseFormIdentifierInternal` treats any all-hex string of 8 digits or fewer as a FormID. An EditorID like `DEADBEEF`
  or `Cafe` becomes a FormID.
- Invariant: whitelist rows always have `filterCategory = "Whitelist"` (forced in `AddToWhitelist`). Don't gate
  whitelist rows through `IsFilterCategoryEnabled`, because it returns false for that category.

## How to…

### Add a new filter category (new master toggle)
1. Add a `GlobalVariable` `STFU_<Name>` to `STFU.esp`, plus an MCM property and toggle in `Source/Scripts/STFU_MCM.psc`.
2. `src/Config.h` `MCMSettings`: add a `RE::TESGlobal*`. Resolve it in `Config::Load()` with `lookupGlobal("STFU_<Name>")`,
   and update the "5 master toggles" count in the log.
3. `Config::IsFilterCategoryEnabled()`: add an `if (filterCategory == "<Name>")` branch **before** the subtype loop.
4. If scenes can carry it: add a branch to `Config::GetSceneGateGlobalForCategory()` (`Config_Scenes.cpp`) and an importer
   like `ImportHardcodedScenes(list, "<Name>")`.
5. Persistence: add Save/Load lines in `src/SettingsPersistence.cpp`.
6. UI: add a setter in `PrismaUIMenu_Settings.cpp` (use `SetToggleFromUI`), register it in `PrismaUIMenu.cpp`, emit it in
   the settings JSON, and add it to `web-ui` settings plus the category list in `advanced-edit-modal.tsx`.
7. Consider mirroring it in `SerializeHistoryToJSON()` if the badge should reflect it.

### Add a new subtype toggle
1. Add a global `STFU_<GlobalName>` in `STFU.esp` and its MCM toggle.
2. Add `{id, "STFU_<GlobalName>"}` to `SubtypeGlobalMap` (`Config.cpp`). If the ID is missing from `SubtypeNameMap`, add it
   there too. Persistence and `GetSubtypeName` key off that map.
3. Add `<GlobalName>` to `TOPIC_CATEGORIES` in `web-ui/src/components/advanced-edit-modal.tsx` and to the web UI's
   subtype toggle list (see WEB_UI.md). Prefer `<GlobalName> == SubtypeNameMap name` to avoid a new naming mismatch.
4. Nothing else is needed. Step D and `IsFilterCategoryEnabled` pick it up from the map.

### Add a new entry field (column)
1. `include/DialogueDatabase.h` `BlacklistEntry`: add the member with a default.
2. Schema: `CREATE TABLE` for **both** `blacklist` and `whitelist`, plus `UpdateSchema()` (ALTER TABLE ADD COLUMN) in
   `src/DialogueDatabase.cpp`, and the prepared insert statements in `PrepareStatements()`. The author declined versioned
   migrations, so keep it additive (see DATABASE.md).
3. `AddToBlacklist` / `AddToWhitelist`: bind it in both the UPDATE and INSERT paths (the parameter indices shift).
4. `ReadListEntry()` (`DialogueDatabase_Json.cpp`): read it by column name. It serves both `GetBlacklist` and
   `GetWhitelist`, and returns the fallback for a missing column.
5. If it affects matching: extend `kTargetMatchSql` / `kActorMatchSql` **and** the matching C++ helper
   (`EntryMatchesTarget` etc.) together, plus `FindExistingEntry` and the Hard pre-check lambda in
   `PopulateTopicInfoHook.cpp`.
6. UI: serialize it in `PrismaUIMenu_Blacklist.cpp` / `_Whitelist.cpp`, parse it in `OnCreateAdvancedEntry` / `OnUpdate*`
   with the `PrismaUIMenuJson.h` readers, and add it to `web-ui/src/types.ts`.

## Related docs

- DIALOGUE_HOOKS.md: PopulateTopicInfo / ConstructResponse / SetSubtitle, and how decisions are applied
- SCENE_BLOCKING.md: Hard scene blocking via phase conditions, pre-included scenes
- DATABASE.md: table schemas, meta flags, history logging
- CONFIG_AND_SETTINGS.md: globals, INI, YAML import files
- PRISMA_UI_BRIDGE.md / WEB_UI.md: entry creation and editing from the menu
- [DIALOGUE_SUBTYPES.md](../DIALOGUE_SUBTYPES.md): subtype reference
