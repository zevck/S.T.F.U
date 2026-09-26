# Known Issues (backlog)

Found while writing these docs (2026-09-26, v1.2.0), mostly by reading the code. Unless marked **verified**, an item has
not been reproduced in game: confirm it with a log before fixing (see [DEVELOPMENT.md](DEVELOPMENT.md#verifying-a-change-in-game)).
Items are grouped by impact and numbered for reference; remove an item when it is fixed.

Already fixed in 1.2.0, for reference: blacklist edits wiping faction filters (**verified**), rules breaking after
load-order changes (FormKeys), YAML FormKeys for light plugins, history badges using a different actor-filter rule,
Quest Hard rows with an empty EditorID matching every EditorID-less quest, positional list-row reads.

## Likely bugs (user-facing)

1. **Hard Topic/Quest rules ignore toggles and filters.** The hard-block pre-check in `Hook_PopulateTopicInfo`
   (`PopulateTopicInfoHook.cpp`, "Check for HARD BLOCKING first") does its own matching and skips the whitelist,
   "Enable Blacklist Filter", category toggles and the row's actor/faction filters. A Hard rule scoped to one NPC blocks
   everyone; turning its toggle off does nothing. Since 6c0e8f9 (Feb 2026). See [BLOCKING_RULES.md](BLOCKING_RULES.md).
2. **Hard blocks can leak on later fragments.** The hard-block path returns before `SetBlockingDecision`, so the cached
   decision stays false. A second PopulateTopicInfo for the same TopicInfo + speaker within 5 s takes the duplicate path
   and calls the original unblocked. See [DIALOGUE_HOOKS.md](DIALOGUE_HOOKS.md).
3. **Scene conditions stack.** `SceneHook::UpdateSceneConditions` / `UpdateSceneConditionsForTopic` add a condition on the
   Hard path without `RemoveConditionsFromScene()` first, so every upsert of a Hard scene row adds another. Old, and
   harmless while every gate was `STFU_Scenes`; since e86ddb7 (per-category gates) a mid-session **category change**
   leaves the scene gated by both the old and new toggle until relaunch. The first-run import probably double-gates
   every imported scene (redundant, not wrong). See [SCENE_BLOCKING.md](SCENE_BLOCKING.md).
4. **Subtype naming mismatches** (`src/Config.cpp`):
   - `STFU_Refuse` is bound to subtype 13 (`kReject`); subtype 17 (`kRefuse`) has no toggle. Check which one vanilla
     follower refusals use before changing it (changing it resets users' stored value).
   - Overrides are validated against `SubtypeNameMap` but gated by `SubtypeGlobalMap` suffixes, so `MurderNPC`,
     `AssaultNPC`, `PickpocketNPC`, `StealFromNPC`, `TrespassAgainstNPC`, `WereTransformCrime`, `Reject` and subtypes
     without a global import "successfully" and never block.
   - The overrides YAML template lists invalid names (`Combat`, `Detection`, `Service`, `Misc`).
5. **Re-import overwrites curation.** "Import Scenes" and "Import from YAML" upsert every column of an existing row:
   block type, category and notes are reset, actor/faction filters cleared, deleted rows come back.
6. **Bard song quests imported as Scene rows.** The manual scene import (`PapyrusInterface.cpp`, `PrismaUIMenu_Settings.cpp`
   `OnImportScenes`) passes `Config::GetBardSongQuestsList()` (quest EditorIDs) to `ImportHardcodedScenes(..., "BardSongs")`.
   Source of the "Hard-blocked scene not found in form table: BardSongs…" warnings on every launch. Related:
   `SceneMonitor::Initialize` uses `BardAudienceQuest` where the rest uses `MS05BardSongs`.
7. **Scene precedence gaps** in `Config::ShouldSoftBlock`: a topic whitelist loses to a blacklist on the scene containing
   the topic; a quest whitelist loses to a topic blacklist; actor/faction whitelists never override scene blacklists
   (the scene lookup passes no actor).
8. **Scene changes that wait for a relaunch.** Whitelist changes never call `SceneHook`; `ClearBlacklist()` leaves
   conditions in place; `PatchScenes` pass 2 re-gates bard scenes from the quest list even if the row was removed.
9. **Topic-triggered scene gating ignores category.** `PatchScenes` pass 3 and `UpdateSceneConditionsForTopic` always use
   `STFU_Scenes` (left out of scope in e86ddb7).
10. **History tab filters** (`web-ui/src/components/history.tsx`): the "Whitelisted" checkbox matches nothing
    (`getStatusDisplay('Whitelist')` returns "Allowed"); rows with the legacy SkyrimNet status never show.
11. **Unescaped text sent to the page.** `OnDetectIdentifierType` builds `displayName` into JS without escaping (a `"` in
    an actor name breaks it) and reads `identifier` without unescaping; `OnGetNearbyActors` escapes only `"` and `\`.
12. **Silent create failures.** `OnCreateAdvancedEntry` saves an unresolved identifier with an uninitialized
    `targetType` (`BlacklistEntry::targetType` has no default); mismatched actor-filter arrays fail only in the log.
    `OnImportYAML` reports success even when parsing failed.
13. **Enable/Disable All** sends one `toggleSubtypeFilter` per subtype, each with an INI save and a full history
    re-serialize; with stale store state it can flip the wrong way. The MCM and menu "Enable All" cover different sets.

## Performance and data

14. **Per-line cost.** Every non-duplicate line copies the whole blacklist (`GetBlacklist()`) for the hard pre-check, and
    again for scene lines; `ShouldSoftBlock` runs up to three times per line. `SceneMonitor::Initialize` calls
    `GetBlacklist()` per scene (O(n²)).
15. **Database bloat.** The author's `dialogue.db` was ~70 MB with ~69 MB of free pages, plus a 37 MB WAL. Nothing runs
    `VACUUM` or checkpoints.
16. **NULL text columns** are assigned to `std::string` without checks in the history reader (`GetRecentDialogue`).
17. **`UpdateSchema` gaps.** `actor_filter_formids`, `actor_filter_names` (both tables) and `whitelist.source_plugin` are
    not added for old databases; inserts would fail on a DB older than them (unclear if any exist).
18. **`g_recentConstructs` may grow without bound**; it is only pruned in the phase-7 correlation gate.
19. **Data quirks:** one blacklist row with `target_type = 0` (`BardAudienceScene`); imports leave `added_timestamp = 0`;
    history is capped at 100 rows while the UI asks for 1000.

## Cleanup

20. **Dead code:** `CellLoadEventHandler` (never registered), unused locals in the hooks (`isHardcoded`,
    `shouldBlockScene*`, `scenesEnabled`), `SettingsPersistence::Register`, `useHistoryStore.clearEntries` /
    `updateEntryStatus`, `requestBlacklist` duplicating `refreshBlacklist`, unused `motion` dependency.
21. **MCM leftovers:** `LoadConfig()` reads the retired patcher INI as JSON; the inert "Block SkyrimNet Logging" toggle;
    hover texts pointing at removed YAML files; unused globals in `STFU.esp` (`STFU_SkyrimNetFilter`, `STFU_SharedInfo`,
    one GLOB without an EditorID). Needs a Papyrus recompile.
22. **Stale or wrong comments:** inverted comment above `CreateGlobalDisabledCondition`; "Loading Menu closing" where the
    code acts on opening; `PatchDeferredScenes` said to run at kPostLoadGame; Ctor-hook comments claiming it fires only
    for playback; `STFU_PreserveGrunts` comments (1 = grunts **blocked**); the "5 seconds" flush comment (it is 1 s);
    `SettingsPersistence.h` mentioning SQLite.
23. **Logging:** `[AUDIO CLEARED]` / `[STALE CACHE]` at warn on every soft block; `[ResponseExtractor]` at info per history
    write; every `spdlog::trace` line is dead (the logger never enables trace); hard-coded "5" in the subtype-globals log.
24. **Build/repo:** `npm run lint` has no ESLint config; old hashed bundles accumulate in `PrismaUI/views/STFU/assets/`;
    `index.html` is tracked while its assets are gitignored; preset `vs2022-windows` uses the VS 18 generator.
25. **Duplicated lists:** subtype/category lists in C++ `OnDetectIdentifierType`, `advanced-edit-modal.tsx`
    `TOPIC_CATEGORIES` and `settings.tsx`.
26. **Hand-parsed JSON** remains in `OnDeleteHistoryEntries`, `OnRemoveWhitelistBatch`, `OnRemoveFromWhitelist`,
    `OnToggleSubtypeFilter`, `SetToggleFromUI`; switch them to `PrismaUIMenuJson.h`.

## Open questions

- Which subtype vanilla follower refusals use (13 or 17); decides the fix for #4.
- Whether `RiftenKeepScene04Alternate` (still pre-included) has the same stalling fragment as the removed `RiftenKeepScene04`.
- Faction matching reads only the NPC's base factions; runtime `AddToFaction` changes are invisible. Decide whether that matters.
- Behavior note, not a bug: since 1.2.0, rows saved before 1.2.0 match by EditorID alone, so such a rule could also match
  a same-EditorID record from another plugin (EditorID-only rows always worked this way).
