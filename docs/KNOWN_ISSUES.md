# Known Issues (backlog)

Found while writing these docs (2026-09-26, v1.2.0), mostly by reading the code. Unless marked **verified**, an item has
not been reproduced in game: confirm it with a log before fixing (see [DEVELOPMENT.md](DEVELOPMENT.md#verifying-a-change-in-game)).
Items are grouped by impact. Numbers are stable: a fixed item is removed and its number not reused.

Fixed in 1.2.0: blacklist edits wiping faction filters (**verified**), rules breaking after load-order changes (FormKeys),
YAML FormKeys for light plugins, history badges using a different actor-filter rule, Quest Hard rows with an empty EditorID
matching every EditorID-less quest, positional list-row reads.

Fixed in 1.2.1: the MQ104IntroScene crash (`TESTopic::Load` in TopicResponseExtractor, **verified** from crash logs),
#2 hard-block fragment leak, #3 scene condition stacking, #6 bard quests imported as scenes, #10 History status filters,
#11 unescaped text sent to the page, #12 silent create/import failures, #16 NULL history text columns, #18 unbounded
`g_recentConstructs`, and parts of #20, #22 and #23.

## Likely bugs (user-facing)

1. **Hard Topic/Quest rules ignore toggles and filters.** The hard-block pre-check in `Hook_PopulateTopicInfo`
   (`PopulateTopicInfoHook.cpp`, "Check for HARD BLOCKING first") does its own matching and skips the whitelist,
   "Enable Blacklist Filter", category toggles and the row's actor/faction filters. A Hard rule scoped to one NPC blocks
   everyone; turning its toggle off does nothing. Since 6c0e8f9 (Feb 2026). See [BLOCKING_RULES.md](BLOCKING_RULES.md).
4. **Subtype naming mismatches** (`src/Config.cpp`):
   - `STFU_Refuse` is bound to subtype 13 (`kReject`); subtype 17 (`kRefuse`) has no toggle. Check which one vanilla
     follower refusals use before changing it (changing it resets users' stored value).
   - Overrides are validated against `SubtypeNameMap` but gated by `SubtypeGlobalMap` suffixes, so `MurderNPC`,
     `AssaultNPC`, `PickpocketNPC`, `StealFromNPC`, `TrespassAgainstNPC`, `WereTransformCrime`, `Reject` and subtypes
     without a global import "successfully" and never block.
   - The overrides YAML template lists invalid names (`Combat`, `Detection`, `Service`, `Misc`).
5. **Re-import overwrites curation.** "Import Scenes" and "Import from YAML" upsert every column of an existing row:
   block type, category and notes are reset, actor/faction filters cleared, deleted rows come back.
7. **Scene precedence gaps** in `Config::ShouldSoftBlock`: a topic whitelist loses to a blacklist on the scene containing
   the topic; a quest whitelist loses to a topic blacklist; actor/faction whitelists never override scene blacklists
   (the scene lookup passes no actor).
8. **Scene changes that wait for a relaunch.** Whitelist changes never call `SceneHook`; `ClearBlacklist()` leaves
   conditions in place; `PatchScenes` pass 2 re-gates bard scenes from the quest list even if the row was removed.
9. **Topic-triggered scene gating ignores category.** `PatchScenes` pass 3 and `UpdateSceneConditionsForTopic` always use
   `STFU_Scenes` (left out of scope in e86ddb7).
13. **Enable/Disable All** sends one `toggleSubtypeFilter` per subtype, each with an INI save and a full history
    re-serialize; with stale store state it can flip the wrong way. The MCM and menu "Enable All" cover different sets.

## Performance and data

14. **Per-line cost.** Every non-duplicate line copies the whole blacklist (`GetBlacklist()`) for the hard pre-check, and
    again for scene lines; `Config::ShouldSoftBlock` runs up to twice per line (phases 5 and 7).
15. **Database bloat.** A long-used `dialogue.db` was seen at ~70 MB with ~69 MB of free pages, plus a 37 MB WAL. Nothing runs
    `VACUUM` or checkpoints.
17. **`UpdateSchema` gaps.** `actor_filter_formids`, `actor_filter_names` (both tables) and `whitelist.source_plugin` are
    not added for old databases; inserts would fail on a DB older than them (unclear if any exist).
19. **Data quirks:** rows saved before the `targetType` default existed can have `target_type = 0`; earlier "Import Scenes"
    runs stored the bard quest names (`BardSongs`, `BardSongsInstrumental`, `MS05BardSongs`) as Scene rows, which log
    "not found in form table" every launch until the user deletes them; imports leave `added_timestamp = 0`; history is
    capped at 100 rows while the UI asks for 1000.

## Cleanup

20. **Dead code:** `requestBlacklist` duplicating `refreshBlacklist` (both have callers; merge them).
21. **MCM leftovers:** `LoadConfig()` reads the retired patcher INI as JSON; the inert "Block SkyrimNet Logging" toggle;
    hover texts pointing at removed YAML files; unused globals in `STFU.esp` (`STFU_SkyrimNetFilter`, `STFU_SharedInfo`,
    one GLOB without an EditorID). Needs a Papyrus recompile.
23. **Logging:** every `spdlog::trace` line is dead (the logger never enables trace).
24. **Build/repo:** `npm run lint` has no ESLint config; `index.html` is tracked while its assets are gitignored; preset
    `vs2022-windows` uses the VS 18 generator.
25. **Duplicated lists:** subtype/category lists in C++ `OnDetectIdentifierType`, `advanced-edit-modal.tsx`
    `TOPIC_CATEGORIES` and `settings.tsx`.
26. **Hand-parsed JSON** remains in `OnDeleteHistoryEntries`, `OnRemoveWhitelistBatch`, `OnRemoveFromWhitelist`,
    `OnToggleSubtypeFilter`, `SetToggleFromUI`; switch them to `PrismaUIMenuJson.h`.

## Open questions

- Which subtype vanilla follower refusals use (13 or 17); decides the fix for #4.
- Whether `RiftenKeepScene04Alternate` (still pre-included) has the same stalling fragment as the removed `RiftenKeepScene04`.
- Every bard song scene (8 in BardSongs, 12 in BardSongsInstrumental) has a script attached (VMAD), which fails the
  curation rule for hard-blocked scenes. Bard-song blocking is a long-standing feature with no known reports; check what
  the fragments do before deciding anything.
- `BardAudienceQuestScene` (BardAudienceQuest, also scripted) was auto-added by older first-run imports and is no longer
  added for new installs; existing installs keep the row.
- Faction matching reads only the NPC's base factions; runtime `AddToFaction` changes are invisible. Decide whether that matters.
- Behavior note, not a bug: since 1.2.0, rows saved before 1.2.0 match by EditorID alone, so such a rule could also match
  a same-EditorID record from another plugin (EditorID-only rows always worked this way).
