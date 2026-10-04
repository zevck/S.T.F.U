# Scene Blocking

STFU hard-blocks whole scenes (ambient NPC-to-NPC conversations, follower chatter, bard
performances) by stopping them from starting at all. It never hooks scene start. Instead it
injects a `GetGlobalValue(<toggle global>) == 0` condition into the **start conditions of
every phase** of each blocked scene when data loads. The toggle globals live in `STFU.esp`.
Setting a global to 1 makes the condition fail, so the phases can't start. Setting it to 0
lets them run. Turning a toggle on or off therefore takes effect immediately, with no
re-patching. Re-patching only happens when the *set* of blocked scenes changes (blacklist
add, remove, or edit). This doc covers that mechanism, how the pre-included scene lists are
imported, the bard-song special case, and the curation rule for which scenes are safe to
hard-block.

Soft-blocking (the scene plays, but its audio and subtitles are silenced) is covered in
BLOCKING_RULES.md. The two overlap: a Hard scene entry is *also* silenced by the soft path
if it somehow starts (see "Safety net" below).

## Overview

```
 kDataLoaded (main.cpp MessageHandler)
 ├─ Config::Load()                     InitializeHardcodedScenes() -> in-memory list (843 IDs)
 │                                     look up STFU_Scenes / STFU_BardSongs /
 │                                     STFU_FollowerCommentary / STFU_Blacklist globals
 ├─ DB init
 │   ├─ meta "hardcoded_scenes_initialized" unset?
 │   │     ImportHardcodedScenes(ambient, "Scene")
 │   │     ImportHardcodedScenes(follower, "FollowerCommentary")
 │   │     set flag
 │   ├─ SettingsPersistence::LoadSettings()      INI -> global values
 │   └─ SceneMonitor::Initialize()     meta "bard_scenes_initialized" unset?
 │                                     insert scenes of the 2 bard quests as "BardSongs" rows
 │                                     (GetBardSongScenesList, insert-only)
 ├─ SceneHook::Install()               (log banner only; no hook)
 ├─ register LoadingMenuSink
 └─ SceneHook::PatchScenes()           Pass 1: Hard Scene rows   -> gate by filterCategory
                                       Pass 2: bard quest scenes -> gate STFU_BardSongs
                                       Pass 3: scenes containing Hard Topic rows -> STFU_Scenes

 Runtime blacklist change (DialogueDatabase_Blacklist.cpp)
 └─ SceneHook::UpdateSceneConditions / ...ForTopic   (queued via SKSE task interface)
      Hard + scene playing  -> g_deferredScenes
      Hard + not playing    -> prepend condition to each phase
      Soft / removed        -> strip STFU conditions

 Loading Menu *opens* (LoadingMenuSink)
 └─ SceneHook::PatchDeferredScenes()   drain g_deferredScenes
```

## Key files and functions

| Location | What |
|---|---|
| `src/SceneHook.cpp` `CreateGlobalDisabledCondition()` | Allocates a `TESConditionItem` with `GetGlobalValue` (function ID 74), `params[0] = global`, `== 0.0`, `object = kSelf` |
| `src/SceneHook.cpp` `PatchScenes()` | Launch-time bulk patch (three passes). Loads blacklist and whitelist once into hash sets |
| `src/SceneHook.cpp` `RemoveConditionsFromScene()` | Unlinks and `RE::free`s every phase start condition that is `GetGlobalValue` on any of the 4 STFU toggle globals |
| `src/SceneHook.cpp` `ResolveSceneGateGlobal()` | Single-scene lookup of a scene's blacklist row, then its gate global (linear scan of `GetBlacklist()`) |
| `src/SceneHook.cpp` `UpdateSceneConditions()` | Runtime add/remove for one scene EditorID. Defers if `scene->isPlaying` |
| `src/SceneHook.cpp` `GateScene()` | File-local. Strips STFU's existing condition, then gates every phase on one global. Every gate goes through it |
| `src/SceneHook.cpp` `UpdateSceneConditionsForTopic()` | Runtime add/remove for every scene with a dialogue action using that topic, except scenes with their own gate (own Hard Scene row, or a bard scene). Always gates on `STFU_Scenes` |
| `src/SceneHook.cpp` `PatchDeferredScenes()` | Drains the in-memory `g_deferredScenes` set |
| `src/Config_Scenes.cpp` `InitializeHardcodedScenes()` | The curated ambient list, stored in `g_settings.hardcodedScenes.topicEditorIDs` (the field name is historical: it holds **scene** EditorIDs) |
| `src/Config_Scenes.cpp` `GetFollowerCommentaryScenesList()` | `WIFollowerChatter02Scene`, `WIFollowerChatter03Scene` |
| `src/Config_Scenes.cpp` `GetBardSongQuestsList()` / `GetBardSongScenes()` / `GetBardSongScenesList()` / `IsBardSongQuest()` | `BardSongs`, `BardSongsInstrumental` (**quest** EditorIDs), their `BGSScene*` (from `quest->scenes`), and those scenes' EditorIDs |
| `src/Config_Scenes.cpp` `GetSceneGateGlobalForCategory()` | filterCategory to global (table below) |
| `src/SceneMonitor.cpp` `Initialize()` | One-shot auto-population of bard scenes into the blacklist. The name is legacy: there is no longer any monitoring (its `Update()` was dead code and was removed) |
| `src/DialogueDatabase_Blacklist.cpp` `ImportHardcodedScenes()` | Upserts a list of scene EditorIDs as Hard `Scene` rows (`insertOnly` leaves existing rows untouched). Returns the number of rows written |
| `src/DialogueDatabase_Blacklist.cpp` `AddToBlacklist()` / `RemoveFromBlacklist()` / `RemoveFromBlacklistBatch()` | Call `SceneHook::Update*` after writing |
| `src/DialogueDatabase.cpp` `GetMetaFlag()` / `SetMetaFlag()` | `meta` table key/value (`"1"`/`"0"`) |
| `src/main.cpp` `LoadingMenuSink`, `MessageHandler` | Import gating, patch at kDataLoaded, deferred patch on Loading Menu |
| `src/Config.cpp` `Load()` | Looks up the toggle globals by EditorID from `STFU.esp` |
| `src/SettingsPersistence.cpp` | INI `[Settings]` keys, reapplied at kDataLoaded, kPostLoadGame and kNewGame |

## How it works

### The injected condition

`CreateGlobalDisabledCondition(global)` builds:

```
GetGlobalValue(global) == 0.0    (flags.global = false: compare against a literal float)
object = kSelf, isOR = false, prepended to phase->startConditions.head
```

The condition is prepended to **`BGSScenePhase::startConditions`** for *every* phase of the
scene. It is not added to the scene-level `BGSScene::conditions` (CTDA) or to actions. Being
an AND, it vetoes the phase whenever the global is non-zero. The code doesn't record why
phases were chosen over the scene-level `conditions`.

**Value semantics: global = 1 means blocked.** `1` makes `== 0` false, so no phase starts.
`0` makes `== 0` true, so the phase runs normally, subject to its own conditions. This is
the same "1 = blocking enabled" convention used everywhere else (`IsFilterCategoryEnabled`,
`>= 0.5f`, and the UI's `scenesEnabled` etc.). The comment above `CreateGlobalDisabledCondition`
says the opposite and is wrong (see Gotchas).

The condition is written to the in-memory form only. Nothing is written to the ESP or the
save, so it has to be re-applied on every launch. That is why `PatchScenes()` runs at
kDataLoaded.

### Choosing the gate global

| `filterCategory` on the Scene row | Gate global | UI / MCM label | INI key |
|---|---|---|---|
| `Scene` (pre-included ambient) | `STFU_Scenes` | Block Ambient Scenes | `BlockScenes` |
| `FollowerCommentary` | `STFU_FollowerCommentary` | Block Follower Commentary / "Block Commentary Quests" | `BlockFollowerCommentary` |
| `BardSongs` | `STFU_BardSongs` | Block Bard Songs | `BlockBardSongs` |
| `Blacklist` (user-added) | `STFU_Blacklist` | Enable Blacklist Filter / "Block Blacklisted Topics" | `BlacklistEnabled` |
| anything else | `STFU_Scenes` (fallback) | | |

All four default to 0 (off) in `SettingsPersistence.cpp`. `LoadSettings()` runs again at
kPostLoadGame because loading a save overwrites the ESP globals' values with the values
stored in the save.

**Pre-included vs user-blacklisted scenes.** The mechanism is the same (Hard `Scene` row
→ phase conditions). What differs is the category and the gate:
- Pre-included rows come from `ImportHardcodedScenes()` (`notes = "Pre-included ambient scene"`,
  `responseText = "[]"`, enrichment skipped, `sourcePlugin` guessed from the `DLC1`/`DLC2`
  prefix). `SceneMonitor` uses the same function, so bard rows get the same notes; older DBs may
  still have bard rows with `notes = "Auto-added by SceneMonitor"`. They follow their category toggle.
- User rows come from the menu (`PrismaUIMenu_Entries.cpp` defaults `filterCategory` to
  `"Blacklist"`) or from YAML `scenes:` (`Config_Yaml.cpp`, Hard, category = list name). They
  follow `STFU_Blacklist`. The user picks Soft or Hard. Only **Hard** adds phase conditions;
  **Soft** lets the scene play and silences it through the dialogue hooks.

Before commit `e86ddb7` every Hard scene was gated on `STFU_Scenes`, so the Bard, Follower
and Blacklist toggles did nothing for scenes (see `RELEASE_NOTES_provisional.md`).

### PatchScenes() at kDataLoaded

`PatchScenes()` runs once at the end of the kDataLoaded handler. By then forms exist (EditorID
lookup works through po3 Tweaks), and no save is loaded yet, so no scene can start unpatched.
It bulk-loads `GetBlacklist()` / `GetWhitelist()` once, with no per-scene SQL. Then:

1. **Pass 1: Hard Scene rows.** For each `(editorID → gate)`: skip it if the scene is
   whitelisted or if the gate global is null. Otherwise
   `LookupByEditorID<BGSScene>`, then `patchScene()` (→ `GateScene()`, which calls
   `RemoveConditionsFromScene()` first, so a re-patch never stacks conditions). A miss logs
   `Hard-blocked scene not found in form table`.
2. **Pass 2: bard quests.** Every scene in `Config::GetBardSongScenes()` (the bard quests'
   `quest->scenes`) is patched on `STFU_BardSongs`. Only whitelisted scenes are skipped. **This pass ignores the blacklist**: every scene
   of those quests is gated whether or not it has a row.
3. **Pass 3: Hard Topic rows.** This pass runs only if some Hard Topic row exists. It
   full-scans all scenes, and any scene with a dialogue action whose topic is Hard-blocked
   gets patched on `STFU_Scenes`. A whitelisted topic in the same scene vetoes the patch.
   Scenes with their own gate are skipped: those with a Hard Scene row (Pass 1) and the bard
   scenes (Pass 2), so a topic rule never replaces another rule's gate.

Summary line: `[SCENE BLOCKER] Patching complete — N scenes (M phases) patched`.

### Runtime changes (add / edit / remove)

`AddToBlacklist()` (both the insert and the update path) calls
`UpdateSceneConditions(editorID, blockType)` for Scene rows, or `UpdateSceneConditionsForTopic`
for Topic rows that have an EditorID. `RemoveFromBlacklist*()` calls the same functions with
`blockType = 1` to strip the conditions. The topic path skips scenes that have their own gate
(their own Hard Scene row, or a bard scene), so a topic rule never replaces or strips another
rule's gate. The work is queued with `SKSE::GetTaskInterface()->AddTask`,
so it runs on the main thread.

- **Hard (2):** if `scene->isPlaying`, the scene is inserted into `g_deferredScenes` and
  nothing else happens. The code comments say that adding conditions to a running scene can
  softlock its NPCs. Otherwise `ResolveSceneGateGlobal()` is used and a condition is prepended
  to each phase.
- **Soft (1) / SkyrimNet (3) / removal:** `RemoveConditionsFromScene()`. This is safe while the scene is
  playing, because it only removes a preventive gate.

**Toggling** a category (MCM `ToggleGlobal`, Prisma `setScenesEnabled` / `setBardSongsEnabled`
/ `setFollowerCommentaryEnabled` / `setBlacklistEnabled`) only writes the global's value (and
the INI). No re-patch is needed: the condition reads the global each time the engine evaluates
the phase. A scene that is already playing keeps playing. Turning a toggle on only prevents
*future* starts.

### Deferred scenes

"Deferred" means that a Hard block was requested while that scene was playing. The set is
**in memory only**: it is not persisted, and it is not stored in the DB. `LoadingMenuSink`
calls `PatchDeferredScenes()` when the `"Loading Menu"` **opens**. The comment at
`main.cpp` `LoadingMenuSink` gives the reasoning: the engine has stopped running scenes by
then, and patching on open gets the conditions in place before the next cell's scenes can
start. (That engine behaviour is taken from the code comment and has not been checked
separately.) `PatchDeferredScenes()` strips the scene's conditions and then re-adds them using the
category gate. If the game exits before a load screen, nothing is lost: the DB row exists,
so the next launch's `PatchScenes()` picks the scene up.

### Bard songs and SceneMonitor

One quest list, `Config::GetBardSongQuestsList()` (`BardSongs`, `BardSongsInstrumental`: 8 + 12 scenes in
Skyrim.esm), drives everything. `Config::GetBardSongScenesList()` turns it into the quests' scene EditorIDs.

| Where | Effect |
|---|---|
| `SceneMonitor::Initialize()` | One-shot: `ImportHardcodedScenes(GetBardSongScenesList(), "BardSongs", insertOnly=true)`, gated by meta flag `bard_scenes_initialized`. Existing rows are left untouched |
| Manual "Import Scenes" (MCM `PapyrusInterface.cpp`, menu `OnImportScenes`) | `GetBardSongScenesList()`, upserted |
| `SceneHook::PatchScenes()` Pass 2 | Gates every scene in `GetBardSongScenes()` on `STFU_BardSongs` at launch |
| `SceneHook::UpdateSceneConditionsForTopic()` | Skips bard scenes, so a topic rule never touches their gate |
| `ConstructResponseHook` / `Config::IsBardSongQuest()` | When `STFU_BardSongs` is on, it sets `scene->isPlaying = false` on the scene that owns the topic and skips the original `ConstructResponse`. This is the only place STFU force-stops a running scene. The comment explains that bards have no deferred-patch path |

**Leftover rows.** Until 1.2.0 the manual import stored the quest EditorIDs (`BardSongs`, `BardSongsInstrumental`,
and the nonexistent `MS05BardSongs`) as Scene rows, and older first-run imports also added `BardAudienceQuestScene`.
Those rows stay in existing databases (no migrations); the quest-name rows log
`[SCENE BLOCKER] Hard-blocked scene not found in form table: …` every launch until deleted.

Every bard scene has a script attached (VMAD), which would fail the curation rule below; bard-song blocking predates
the rule and has no known reports. See KNOWN_ISSUES.md open questions.

### First-run import gating

`main.cpp` imports the ambient list (`"Scene"`) and the follower list (`"FollowerCommentary"`)
only when `GetMetaFlag("hardcoded_scenes_initialized")` is false, and then sets the flag.
`SceneMonitor` does the same with `bard_scenes_initialized`. These are flags, not row counts.
The old `HasScenesImported` count check re-imported everything whenever the user cleared or
emptied the scene blacklist. With the flag, a user who deletes pre-included rows keeps them deleted.
The manual MCM/Prisma import ignores the flag and always runs.

`ImportHardcodedScenes()` goes through `AddToBlacklist(entry, skipEnrichment=true)`, which
**upserts** by `(target_type, editorID)`. Re-importing therefore overwrites existing rows:
`block_type` goes back to Hard, and `notes`, `filter_category`, `response_text` (`"[]"`) and the
actor/faction filters (cleared) are reset. See Gotchas. The exception is `insertOnly=true`
(used by `SceneMonitor`), which skips scenes that already have a row.

### Safety net (Hard scene that starts anyway)

In `PopulateTopicInfoHook.cpp`, for subtype-14 (Scene) topics: the hook finds the owning scene
through `quest->scenes` → dialogue actions. If that scene has a Hard row whose category gate is
on (or it is a bard quest with bard blocking on), it logs
`[POPULATE SCENE SAFETY] Hard-blocked scene '…' started despite conditions!` at **error**
level. It does not stop the scene. `ShouldSoftBlock` still matches the Hard row, so the audio
and subtitles are silenced by the ConstructResponse/SetSubtitle path (BLOCKING_RULES.md).

## Curation rule for the pre-included list

**If a scene *does* something, let it play.** Only purely ambient chatter belongs in
`InitializeHardcodedScenes()`. Hard-blocking vetoes every phase, so the scene never plays its
content. The failure classes found in the audit (commit `fee6d65`) are:

- **Script fragments.** Scene, phase or topic-info fragments never run. Stage changes on other
  quests, `DLC2Init`, package/quest completion and globals are lost.
- **Quest state.** Anything that `SetStage`s or completes something.
- **Stranded NPCs.** The parent quest forces an actor into a scene package and relies on
  `StopQuestOnEnd` (or the scene ending) to release it. If the scene never ends properly, the NPC
  stays in the package. Examples were BrandyMug farm, the Riverwood family and the Windhelm blacksmith.

The motivating bug was **`RiftenKeepScene04`**. A topic-info fragment in it advances a global
that Jarl Laila's and Anuriel's `RDSStartGame` packages wait on. With the scene blocked, both
stayed stuck in the throne area (commit `68bc0d4`). The audit that followed removed 40 more
scenes (25 fragment runners, 6 forced-package + StopQuestOnEnd, 8 non-vanilla Kynesgrove,
plus `WIFollowerChatter01Scene` from the follower list). `RELEASE_NOTES_provisional.md` lists
them all. Note that `RiftenKeepScene04Alternate` is still in the list. It was not flagged by
the audit. Whether it shares the fragment has not been checked here.

Removing a name from the source affects **new imports only**. Existing DBs keep the row until
the user deletes it, because the author declined DB migrations.

## Settings and data touched

- Globals (`STFU.esp`): `STFU_Scenes`, `STFU_BardSongs`, `STFU_FollowerCommentary`,
  `STFU_Blacklist`. They are looked up in `Config::Load()`. If one is missing, it's `nullptr`, and
  Pass 1 skips every scene that maps to it (silently: no log per scene).
- INI `SKSE/Plugins/STFU/config/STFU_Config.ini` `[Settings]`: `BlockScenes`, `BlockBardSongs`,
  `BlockFollowerCommentary`, `BlacklistEnabled`.
- DB `blacklist` rows with `target_type = Scene` (4), `subtype = 14`, `subtype_name = "Scene"`.
  DB `meta` keys `hardcoded_scenes_initialized`, `bard_scenes_initialized`. See DATABASE.md.
- Whitelist Scene/Topic rows veto patching (in `PatchScenes()` only).

## Log lines to grep

Log file: `Documents/My Games/Skyrim Special Edition/SKSE/STFU.log`. For the debug-level lines,
create an empty `stfu_debug.flag` next to it.

| Pattern | Meaning |
|---|---|
| `Importing hardcoded scenes (first-run)` / `Hardcoded scenes already initialized` | First-run gate result |
| `[DialogueDB] Importing N scenes with filter category` / `Wrote N of M scenes to the blacklist (category '…')` | Import ran. N counts every row written (insert or upsert); `, existing rows kept` is appended for insert-only imports |
| `[SceneMonitor] Added X of Y bard song scenes to the blacklist (first run)` / `[SceneMonitor] Bard scenes already initialized, skipping auto-population` | Bard auto-population |
| `Global 'STFU_…' not found in loaded ESPs` | Gate global missing (STFU.esp not loaded?) |
| `[SCENE BLOCKER] Patching complete —` | Launch summary |
| `[SCENE BLOCKER] Hard-blocked scene not found in form table:` | Row with no matching `BGSScene` (typo, missing plugin, or bard **quest** names from manual import) |
| `[Config] Bard song quest not found:` (warn) | `GetBardSongScenes()` lookup failed (Pass 2, topic path, imports) |
| `[SCENE BLOCKER] Patched Hard-blocked scene: X (gate: STFU_…)` (debug) | Confirms which global gates a scene |
| `[SCENE UPDATE] Scene X is currently running - queuing for next load screen` | Deferred |
| `[SCENE UPDATE] PatchDeferredScenes: patching N scene(s)` / `Patched deferred scene` | Deferred drain |
| `[SCENE UPDATE] Added blocking conditions` / `Removed blocking conditions` (debug) | Runtime changes |
| `[SCENE UPDATE] Topic X - hard blocked N scenes` | Topic-driven runtime patch |
| `[POPULATE SCENE SAFETY]` | A Hard scene started anyway |
| `[SCENE BLOCK] Blocking bard song` (debug) | ConstructResponse force-stop of a bard scene |

## Gotchas / known issues

- **Every gate goes through `GateScene()`**, which strips STFU's existing condition before adding one, so
  re-saving a Hard row or changing its category replaces the gate instead of stacking a second one.
  Before this was fixed, the runtime Hard paths stacked conditions and a category change left a scene
  gated on both toggles until relaunch. Like Pass 3, the runtime topic path (`UpdateSceneConditionsForTopic`,
  both Hard and Soft/remove) skips scenes with their own gate (own Hard Scene row, or a bard scene), so a topic
  rule never replaces or strips that gate.
- **Whitelist changes aren't applied at runtime.** `DialogueDatabase_Whitelist.cpp` never calls
  `SceneHook`. Whitelisting a Hard-blocked scene only takes effect at the next launch.
- **`ClearBlacklist()` leaves conditions in place** until the next launch (see the placeholder comment
  in the source).
- **Pass 3 and `UpdateSceneConditionsForTopic` always gate on `STFU_Scenes`**, whatever
  the Topic row's category is. A user-Hard-blocked topic (category `Blacklist`) therefore gates its scenes
  on Block Ambient Scenes, not on the Blacklist toggle. This is inconsistent with the per-category scene gating.
- **Removing a bard scene row doesn't un-gate it for good.** At runtime the conditions are
  stripped, but at the next launch Pass 2 gates every scene of the bard quests regardless of the blacklist. To exempt one, whitelist it.
- **Manual re-import clobbers curation.** It upserts, so it resets pre-included rows the user
  changed (Soft → Hard, actor filters cleared, a scene the user had re-categorised as
  `Blacklist` goes back to `Scene`). Scenes the user *deleted* come back.
- **`isPlaying` guard only applies to Hard adds.** Removal is applied immediately. That's
  intended.
- **Deferred set is not persisted.** It's harmless (see Deferred scenes). `PatchDeferredScenes` runs from
  `LoadingMenuSink` when the Loading Menu *opens*.
- **EditorIDs are required.** Everything is keyed on scene EditorIDs through
  `STFU::GetEditorID` / `LookupByEditorID`. On AE that needs powerofthree's Tweaks. Without it,
  lookups fail and the result is a wall of "not found in form table" warnings.
- **Hardcoded fallback compares a topic to scene IDs.** `IsHardcodedAmbientScene(TESTopic*)`
  falls back to checking the *topic* EditorID against the scene-ID set, which almost never matches.
  It logs `[HARDCODED CHECK]` at debug level when it does, or when the set is empty.

## How to add or remove a pre-included scene

1. **Checklist before adding.** The scene must be ambient chatter only. Check it in xEdit / CK
   (base game plus DLC and any overriding plugin):
   - [ ] No scene begin/end fragments, and no phase fragments.
   - [ ] No topic-info fragments on any of its dialogue actions' INFOs (this is where
     `RiftenKeepScene04` hid its global update).
   - [ ] No actions other than Dialogue (and trivial Timer): no Package actions that move
     actors somewhere they depend on the scene to leave.
   - [ ] The parent quest doesn't force actors into packages (alias packages) together with
     `StopQuestOnEnd`, and no other quest or package waits on this scene or its quest stage.
   - [ ] Its EditorID resolves (vanilla/DLC master, not a mod that may be absent).
   If any box fails, **don't add it.** Users can still Soft-block it themselves.
2. Add the EditorID (exact case) to `sceneNames` in `src/Config_Scenes.cpp`
   `InitializeHardcodedScenes()`. For follower chatter, use `GetFollowerCommentaryScenesList()` instead.
3. Existing installs **won't** receive it, because the first-run flag is already set. They need the manual
   "Import scenes" action (which has the side effects above), or they add it themselves.
4. **Removing:** delete the name from the list and add it to the release notes under
   "Removed scenes", with the reason. Existing DB rows stay (there are no migrations). Tell users to delete
   them from the blacklist.
5. Verify in game. Look for `Patched Hard-blocked scene: <ID> (gate: STFU_Scenes)` in a debug log. For a
   removal, check that the NPCs involved go on with their schedule.

## How to add a new scene category

Example: a `Guards` category with its own toggle.

1. **ESP:** add a `STFU_Guards` GLOBAL (short, default 0) to `STFU.esp`.
2. **Config:** add a `TESGlobal*` field to `Settings::mcm` (`src/Config.h`), look it up in
   `Config::Load()`, and add a `GetGuardsGlobal()` accessor.
3. **Gate mapping:** add `if (filterCategory == "Guards") return GetGuardsGlobal();` to
   `GetSceneGateGlobalForCategory()` (`src/Config_Scenes.cpp`).
4. **Strip list:** add the global to the recognised set in `RemoveConditionsFromScene()`.
   If you skip this, re-patching leaves the old condition behind and the scene ends up double-gated.
5. **Soft path:** add the category to `Config::IsFilterCategoryEnabled()` (`src/Config.cpp`),
   so `ShouldSoftBlock` and the safety net respect the toggle.
6. **List + import:** add `GetGuardsScenesList()`, and import it with
   `ImportHardcodedScenes(list, "Guards")` behind a **new** meta flag in `main.cpp`. Don't reuse
   `hardcoded_scenes_initialized`: existing users have already set it. Also add it to the
   manual import in `PapyrusInterface.cpp` `ImportHardcodedScenes` and in `PrismaUIMenu_Settings.cpp`
   `OnImportScenes`.
7. **Persistence:** add a `[Settings]` INI key in `SettingsPersistence.cpp` Save and Load.
8. **UI:** add a Prisma listener (`PrismaUIMenu.cpp`, a `SetToggleFromUI` handler), a
   `SendSettingsData()` field, the web-ui toggle (`web-ui/src/components/settings.tsx`,
   `skse-api.ts`) and an MCM option (`Source/Scripts/STFU_MCM.psc`). See PRISMA_UI_BRIDGE.md,
   WEB_UI.md and CONFIG_AND_SETTINGS.md.
9. Apply the curation checklist above to every scene in the list.

## Related docs

- BLOCKING_RULES.md: Soft vs Hard, `ShouldSoftBlock`, and the order of evaluation
- DIALOGUE_HOOKS.md: PopulateTopicInfo / ConstructResponse (safety net, bard force-stop)
- DATABASE.md: `blacklist` / `whitelist` / `meta` schema and upsert matching
- CONFIG_AND_SETTINGS.md: globals, INI, and the reload at kPostLoadGame
- ARCHITECTURE.md: plugin lifecycle
- DIALOGUE_SUBTYPES.md: subtype 14 (Scene)
