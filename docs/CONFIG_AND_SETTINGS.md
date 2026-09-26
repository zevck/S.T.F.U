# Configuration and Settings

STFU has three kinds of persistent state. **Toggles** (master switches and the 66 per-subtype switches) are `TESGlobal`s in `STFU.esp`, and their values persist in an INI file. **Lists** (blacklist, whitelist, subtype overrides) are rows in the SQLite DB. **YAML files** are import sources only: they are never loaded automatically and never written back. This doc covers every setting, where it is stored, which store wins at each lifecycle point, the YAML import format, and the MCM ↔ native bridge. For how a toggle actually blocks dialogue, see BLOCKING_RULES.md and SCENE_BLOCKING.md.

## Overview

```
                 ┌──────────────────────────────────────────────┐
                 │ STFU.esp  (74 GLOB records, all FLTV 0.0)     │
                 │   STFU_Blacklist / _Scenes / _BardSongs /     │
                 │   _FollowerCommentary / _PreserveGrunts +     │
                 │   66 subtype globals (+ 3 unused, see below)  │
                 └───────────────┬──────────────────────────────┘
       kDataLoaded: Config::Load()│ LookupByEditorID -> Settings.mcm / .blacklist
                                 v
  STFU_Config.ini ──LoadSettings()──> TESGlobal::value  <── engine restores from save
   [Settings]/[Subtypes]  <──SaveSettings()──┘   ▲  ▲         (then overwritten at
                                              │  │          kPostLoadGame)
               Prisma Settings tab ───────────┘  └──── MCM (STFU_MCM.psc,
               (sets global + SaveSettings now)          GlobalVariable.SetValue,
                                                         SaveSettings on MCM close)

  import/*.yaml ──"Import from YAML"──> ImportYAMLToDatabase() ──> dialogue.db
  (templates generated if missing)       (upsert, never deletes)    blacklist / whitelist
                                                                    filter_category ──> which global gates the row
```

Readers never cache global values. Every check reads `global->value` live, e.g. `Config::IsFilterCategoryEnabled`, `Config::ShouldSoftBlock`, and the patched scene conditions `GetGlobalValue(...) == 0`. A toggle change therefore takes effect immediately, with no re-patching.

## Key files

| File | What it holds |
|------|---------------|
| `src/Config.h` | `Config::Settings` (`BlacklistSettings`, `MCMSettings`, `HardcodedScenes`, `menuHotkey`) and the public Config API |
| `src/Config.cpp` | `SubtypeNameMap` (name→ID, also the INI key names), `SubtypeGlobalMap` (ID→global EditorID), `Load()`, `ToggleSubtypeFilter()`, `IsFilterCategoryEnabled()` |
| `src/ConfigInternal.h` | `g_settings`, `SubtypeNameMap` externs, plus `InitializeHardcodedScenes()` and `GenerateDefaultYAMLs()`, which are shared between the `Config_*.cpp` files |
| `src/Config_Yaml.cpp` | YAML paths, `GenerateDefaultYAMLs()`, `ParseFormIdentifierInternal()`, `ImportYAMLListSections()`, `ImportSubtypeOverrides()`, `ImportYAMLToDatabase()` |
| `src/Config_Blocking.cpp` | Consumers: `IsFilteredByMCM`, `HasDisabledSubtypeToggle`, `ShouldSoftBlock` (subtype and grunt logic) |
| `src/Config_Scenes.cpp` | `GetSceneGateGlobalForCategory()` maps a filterCategory to its gate global |
| `src/SettingsPersistence.cpp/.h` | INI `LoadSettings()` / `SaveSettings()` (the header comments still say "SQLite database", which is stale) |
| `src/PapyrusInterface.cpp/.h` | 7 native functions for `STFU_MCM` |
| `src/PrismaUIMenu_Settings.cpp` | Prisma settings listeners, `SetToggleFromUI()`, `SendSettingsData()`, `OnImportYAML()`, `OnImportScenes()` |
| `src/main.cpp` `MessageHandler()` | Lifecycle: kDataLoaded / kPostLoadGame / kNewGame. `InputEventSink` reads `menuHotkey` |
| `Source/Scripts/STFU_MCM.psc` → `Scripts/STFU_MCM.pex` | SkyUI MCM |
| `web-ui/src/components/settings.tsx`, `stores/settings.ts`, `lib/skse-api.ts` | Prisma settings tab |

## File locations

All paths are built from the game exe directory (`GetModuleFileNameW(nullptr)` + `Data/SKSE/Plugins/STFU/...`). Under MO2 the VFS redirects newly created files to `overwrite/SKSE/Plugins/STFU/`.

| Path | Created by | Notes |
|------|-----------|-------|
| `config/STFU_Config.ini` | `SaveSettings()` on first save (`create_directories` first) | `LoadSettings()` never creates it and falls back to defaults |
| `import/STFU_Blacklist.yaml` | `GenerateDefaultYAMLs()` in `Config::Load()`, only if missing | Commented template |
| `import/STFU_Whitelist.yaml` | same | |
| `import/STFU_SubtypeOverrides.yaml` | same | |
| `data/dialogue.db` | `main.cpp` kDataLoaded | See DATABASE.md |

The templates are written only when a file is missing, so template edits never reach existing users. There is no YAML export: nothing ever writes the DB back to YAML. `CONFIG_TUTORIAL.md` no longer exists. It was deleted in `9ce8991` ("Remove dead code and the inert SkyrimNet filter…"). Update README.md for user-facing config docs instead.

## Source of truth: who wins when

| Moment | What happens | Winner |
|--------|--------------|--------|
| Plugin load (`SKSE_PLUGIN_LOAD`) | Papyrus natives are registered. No config is read yet | n/a |
| `kDataLoaded` | `Config::Load()` generates missing YAML templates, runs `InitializeHardcodedScenes()`, and looks up the 5 master globals plus the `SubtypeGlobalMap` globals by EditorID. It then initializes the DB (first-run scene import is gated by meta flag `hardcoded_scenes_initialized`) and calls **`SettingsPersistence::LoadSettings()`** | **INI** (missing key → default 0) |
| New game (`kNewGame`) | `LoadSettings()` | **INI** |
| Save load | The engine restores every global's value from the save. **Then** `kPostLoadGame` → `LoadSettings()` overwrites them | **INI** (a save's stored values are discarded) |
| Prisma toggle | `SetToggleFromUI()` / `Config::ToggleSubtypeFilter()` set `global->value` → **`SaveSettings()` immediately** → `SendSettingsData()` | UI → global → INI |
| MCM toggle | `ToggleGlobal()` → `GlobalVariable.SetValue()`. The INI is written only in `OnConfigClose()` → native `SaveSettings()` | Global now, INI when the MCM closes |
| MCM hotkey | `SetMenuHotkey()` → `settings.menuHotkey` → `SaveSettings()` immediately | INI |

Consequences:
- The **ESP's default values do not matter**. `LoadSettings()` overwrites every global it found, using the INI value or `DEFAULT_*` (all 0). Effective defaults are **everything off; hotkey `0xD2` (Insert)**. The ESP also ships every GLOB at 0.0.
- `LoadSettings()` at kDataLoaded runs **only if the DB initialized**. If `Initialize()` fails, globals keep their ESP values until the next kNewGame/kPostLoadGame.
- Toggles are global across all saves because the INI is not per-save. This is intentional (the comment is in `SettingsPersistence.cpp`). `Register()` only logs; there is no SKSE co-save serialization.
- The DB stores **no toggle values**. It stores list rows, whose `filter_category` decides which global gates each row (see below).
- `SaveSettings()` writes a key only when its global was found, and never deletes keys. Stale keys from older versions (e.g. `SkyrimNetFilterEnabled=1`) remain in users' INIs and are ignored.

## Settings reference

### `Config::Settings` layout (`src/Config.h`)

| Field | Type | Filled by |
|-------|------|-----------|
| `blacklist.toggleGlobal` | `TESGlobal*` | `STFU_Blacklist` |
| `mcm.blockScenesGlobal` | `TESGlobal*` | `STFU_Scenes` |
| `mcm.blockBardSongsGlobal` | `TESGlobal*` | `STFU_BardSongs` |
| `mcm.blockFollowerCommentaryGlobal` | `TESGlobal*` | `STFU_FollowerCommentary` |
| `mcm.preserveGruntsGlobal` | `TESGlobal*` | `STFU_PreserveGrunts` (the header comment "0=filter, 1=preserve" is **wrong**, see below) |
| `mcm.subtypeGlobals` | `unordered_map<uint16_t, TESGlobal*>` | `SubtypeGlobalMap` (66 entries) |
| `hardcodedScenes.topicEditorIDs` | `unordered_set<string>` | `InitializeHardcodedScenes()` (see SCENE_BLOCKING.md) |
| `menuHotkey` | `uint32_t` | INI only, no global. Default `0xD2` |

Mutation goes through `const_cast<Config::Settings&>(Config::GetSettings())` in `SettingsPersistence` and `PapyrusInterface::SetMenuHotkey`. There is no setter API.

### Master settings

All toggles follow the same convention: **1 = blocking on**, and readers treat `value >= 0.5` as on. The INI writer uses `> 0.5`, which is equivalent for 0/1 values.

| Setting | INI `[Settings]` key | Global | Default | Controls | Prisma | MCM |
|---------|---------------------|--------|---------|----------|--------|-----|
| Blacklist filter | `BlacklistEnabled` | `STFU_Blacklist` | 0 | Gates DB rows with `filter_category = "Blacklist"` (user entries, YAML blacklist imports, and Actor/Faction rows with an empty category) and hard-blocked scenes in that category | Master Controls → "Enable Blacklist Filter" | Other Dialogue → "Block Blacklisted Topics" |
| Ambient scenes | `BlockScenes` | `STFU_Scenes` | 0 | Category `"Scene"` rows and scene gate fallback (`GetSceneGateGlobalForCategory` default) | Master → "Block Ambient Scenes" | Other → "Block Ambient Scenes" |
| Bard songs | `BlockBardSongs` | `STFU_BardSongs` | 0 | Category `"BardSongs"`, `ShouldBlockBardSongs()`, SceneMonitor | Master → "Block Bard Songs" | Other → "Block Bard Songs" |
| Follower commentary | `BlockFollowerCommentary` | `STFU_FollowerCommentary` | 0 | Category `"FollowerCommentary"` rows and scene gate | Follower tab → "Block Follower Commentary" | Follower Dialogue → "Block Commentary Quests" |
| Combat grunts | `PreserveGrunts` | `STFU_PreserveGrunts` | 0 | **1 = grunts blocked.** At 0, a response whose text matches the hard-coded grunt list (`ShouldFilterFromHistory`) is let through even when its subtype toggle is on | Combat tab → "Block Combat Grunts" | Combat Dialogue → "Block Combat Grunts" |
| Menu hotkey | `MenuHotkey` (written as `0x%X`, read with `wcstoul(..., 0)`, so hex or decimal both work) | none | `0xD2` | Keyboard DirectInput scancode that toggles the Prisma menu (`main.cpp` `InputEventSink`) | not surfaced | Settings → "Menu Hotkey" (key map). `SetMenuHotkey` rejects `<= 0` or `> 0x1FF` |

Prisma JSON field names (`SendSettingsData` ↔ `stores/settings.ts`) are `blacklistEnabled`, `scenesEnabled`, `bardSongsEnabled`, `followerCommentaryEnabled`, `combatGruntsBlocked`, and `subtypes: { "<id>": bool }`.

### Subtype toggles

`SubtypeGlobalMap` binds 66 subtype IDs to globals. The **INI `[Subtypes]` key is `GetSubtypeName(id)`, taken from `SubtypeNameMap`, not the global's suffix.** The two usually match. These are the exceptions:

| ID | INI key (`SubtypeNameMap`) | Global EditorID | UI label |
|----|---------------------------|-----------------|----------|
| 13 | `Reject` | `STFU_Refuse` | "Refuse" (see Gotchas) |
| 44 | `AssaultNPC` | `STFU_AssaultNC` | Assault NC |
| 45 | `MurderNPC` | `STFU_MurderNC` | Murder NC |
| 46 | `PickpocketNPC` | `STFU_PickpocketNC` | Pickpocket NC |
| 47 | `StealFromNPC` | `STFU_StealFromNC` | Steal From NC |
| 48 | `TrespassAgainstNPC` | `STFU_TrespassAgainstNC` | Trespass Against NC |
| 50 | `WereTransformCrime` | `STFU_WerewolfTransformCrime` | Werewolf Transform Crime |

Surfacing. Prisma and the MCM show the same set on each page:

| Prisma tab / MCM page | Subtype IDs |
|-----------------------|-------------|
| Combat / "Combat Dialogue" (26) | 26 27 28 29 30 31 32 33 35 36 37 39 40 41 55 56 57 58 59 60 61 62 63 64 65 75, plus Combat Grunts |
| Generic / "Generic Dialogue" (31) | 38 42 43 44 45 46 47 48 49 50 70 74 76 77 78 79 80 81 82 84 85 86 87 88 89 91 92 93 94 98 99 |
| Follower / "Follower Dialogue" (5) | 13 15 16 18 19, plus Follower Commentary |
| Other / "Other Dialogue" (4) | 51 52 53 54 (voice powers). The MCM page also carries Bard Songs, Scenes, Blacklist and the inert SkyrimNet toggle |

The subtype IDs and names are listed in DIALOGUE_SUBTYPES.md. Subtypes without a global (0–12, 14, 17, 90, 95–97, 100–102) cannot be toggled. `ToggleSubtypeFilter` logs `has no MCM toggle global` and returns false.

### Globals in STFU.esp that the DLL does not use

STFU.esp is **ESL-flagged** (TES4 flags `0x200`, master `Skyrim.esm`) and contains 74 GLOBs, 1 QUST (the MCM quest), and 2 FACT. Three GLOBs are not looked up by the DLL:
- `STFU_SkyrimNetFilter`: toggled by the MCM only (inert, see below).
- `STFU_SharedInfo` (subtype 90): not in `SubtypeGlobalMap` and not in any UI.
- An unnamed GLOB (`xx000D62`, no EDID, value 1.0): orphan.

## The MCM (`Source/Scripts/STFU_MCM.psc`)

The MCM script extends `SKI_ConfigBase`. `ModName = "S.T.F.U"`. Its pages are **Combat Dialogue, Generic Dialogue, Follower Dialogue, Other Dialogue, Settings**.

Natives are registered in `PapyrusInterface::RegisterFunctions()` under class `"STFU_MCM"` and declared `native global` in the psc:

| Native | C++ | Behaviour |
|--------|-----|-----------|
| `ImportHardcodedScenes()` | `PapyrusInterface::ImportHardcodedScenes` | **Detached `std::thread`**. Calls `db->ImportHardcodedScenes` for the ambient list ("Scene"), bard song quests ("BardSongs") and follower commentary ("FollowerCommentary"). Fire-and-forget: the MCM shows "Importing scenes..." with no completion notice |
| `int ImportFromYAML()` | `ImportFromYAML` → `Config::ImportYAMLToDatabase()` | **Synchronous on the Papyrus VM call**, and imports enrich response text, so a large quest import can stall. Returns total rows added or updated |
| `int GetMenuHotkey()` / `SetMenuHotkey(int)` | same names | Read/write `Settings::menuHotkey`. Set validates and saves the INI |
| `SaveSettings()` | `SaveSettings` → `SettingsPersistence::SaveSettings()` | Called from `OnConfigClose` |
| `int ClearBlacklist()` / `int ClearWhitelist()` | `db->ClearBlacklist()` / `ClearWhitelist()` | "Clear Database" calls both. It does not clear history |

Page behaviour:
- Toggles call `ToggleGlobal(global, oid)`, which flips 0↔1 via `GlobalVariable.SetValue`. The option state shown is `global.GetValue() as bool`.
- "Enable All" / "Disable All" on a page set every global on that page. That includes `STFU_PreserveGrunts` (Combat), `STFU_FollowerCommentary` (Follower), and `STFU_BardSongs`/`_Scenes`/`_Blacklist`/`_SkyrimNetFilter` plus the voice powers (Other). Prisma's Other-tab Enable/Disable All only touches the 4 voice powers, and its master toggles live on a separate tab.
- Settings page: Menu Hotkey (`AddKeyMapOption`), Import Scenes, Import from YAML, Clear Blacklist / Clear Whitelist / Clear Database (each confirms with `ShowMessage`).
- `GetVersion()` returns **7**. SkyUI calls `OnVersionUpdate` when a save's stored MCM version is lower, and that handler resets `ModName`/`Pages`. Bump it only when the page list or other `OnConfigInit` state must be re-initialized on existing saves. Adding options to an existing page does not need a bump. `OnConfigOpen` rebuilds `Pages` every time anyway via `BuildPageList()`.

Legacy and dead parts of the psc (removing them needs an edit, a Papyrus recompile to `Scripts/STFU_MCM.pex`, and an in-game check):
- **"Block SkyrimNet Logging" is inert.** It is shown only if `SkyrimNet.esp` is loaded (`CheckSkyrimNetInstallation`) and toggles `STFU_SkyrimNetFilter`, which the DLL never reads (the SkyrimNet filter was removed in `9ce8991`). Its hover text still references `STFU_SkyrimNetFilter.yaml`.
- `LoadConfig()` reads `"Data/STFU Patcher/Config/STFU_Config.ini"` as JSON via JContainers `JValue` to hide options (`filterXxx` flags). That file comes from a retired patcher and is unrelated to the DLL's INI. Normally it is absent, so every option is shown. This is dead code.
- The hover text for "Block Blacklisted Topics" says entries are "defined in STFU_Blacklist.yaml". They live in the DB. The hover for Import from YAML omits SubtypeOverrides.
- `pageCount` in `BuildPageList` is computed but unused. The array is always size 5.

## Prisma settings tab

`web-ui/src/components/settings.tsx` has the tabs Master Controls, Combat, Generic, Follower, and Other. The listeners are in `src/PrismaUIMenu.cpp`:

| JS → C++ listener | Payload | Handler |
|-------------------|---------|---------|
| `requestSettings` | `''` | `OnRequestSettings` → `SendSettingsData()` → JS `updateSettings` |
| `toggleSubtypeFilter` | `{"topicSubtype": n}` | `OnToggleSubtypeFilter` → `Config::ToggleSubtypeFilter` (**flip, not set**), then `SendSettingsData` and a history refresh (`updateHistory`) |
| `setBlacklistEnabled` / `setScenesEnabled` / `setBardSongsEnabled` / `setFollowerCommentaryEnabled` | `{"enabled": bool}` | `SetToggleFromUI(handler, data, "enabled", global)` |
| `setCombatGruntsBlocked` | `{"blocked": bool}` | `SetToggleFromUI(..., "blocked", preserveGruntsGlobal)` |
| `importScenes` | `''` | `OnImportScenes` (synchronous, same three lists as the MCM), toast, `SendBlacklistData`, `SendHistoryData` |
| `importYAML` | `''` | `OnImportYAML`: errors if **neither** the Blacklist nor the Whitelist YAML exists, otherwise calls `ImportYAMLToDatabase()`, shows a toast, and refreshes the lists and history |

`SetToggleFromUI` performs **no inversion**: `true` → 1.0. The display side is `SendSettingsData`: `value >= 0.5` → `true`. The toggle `History` view also calls `toggleSubtypeFilter` (`history.tsx`). See PRISMA_UI_BRIDGE.md and WEB_UI.md for the transport details.

## YAML import

### Format

All three files are optional. Lists must be YAML sequences of **scalars**. Map items are skipped silently, and a section containing only comments parses as null and is skipped.

```yaml
# STFU_Blacklist.yaml
topics:   [ WICastMagicNonHostileSpellStealthTopic, "0x012345:MyMod.esp", 0001A2B3 ]
scenes:   [ WhiterunMikaelSongScene ]      # EditorID only
quests:   [ DA07MuseumScenes ]             # EditorID only; expands to its topics

# STFU_Whitelist.yaml  (same three sections, plus:)
plugins:  [ "ImportantQuestMod.esp" ]

# STFU_SubtypeOverrides.yaml
overrides:
  DLC2PillarBlockingTopic: Idle            # key = topic identifier, value = SubtypeNameMap name
```

### What each section produces (`ImportYAMLListSections`, `ImportSubtypeOverrides`)

| Section | Row | target_type | block_type | filter_category | Identifier parsing |
|---------|-----|-------------|------------|-----------------|--------------------|
| `topics` | blacklist/whitelist | Topic | Soft | `"Blacklist"` / `"Whitelist"` | `ParseFormIdentifierInternal` |
| `scenes` | " | Scene (subtype 14, "Scene") | **Hard** | same | Raw EditorID (`targetFormID = 0`) |
| `quests` | one Topic row **per topic** in the quest (regular `topics[]` arrays and `branchedDialogue`), note `"(quest: X[, branched])"` | Topic | Soft | same | `SafeLookupForm<TESQuest>` by EditorID. Warns `Quest not found` if the lookup fails |
| `plugins` (whitelist only) | whitelist | Plugin (name in `target_editorid` and `source_plugin`) | Soft | `"Whitelist"` | raw |
| `overrides` | **blacklist** | Topic | Soft | **the subtype name** (e.g. `"Idle"`) | `ParseFormIdentifierInternal`. The value must be a `SubtypeNameMap` key, otherwise it warns `Unknown subtype name` |

A subtype "override" does not change the topic's subtype. It adds a blacklist row whose `filter_category` is a subtype name. `IsFilterCategoryEnabled` then resolves that name against the **global suffixes** in `SubtypeGlobalMap`, so the topic is silenced whenever that subtype's toggle is on. `AddToWhitelist` always forces `filterCategory = "Whitelist"`. Quest expansion is a **snapshot** taken at import time; it does not create a Quest row.

`ParseFormIdentifierInternal` tries these forms in order:
1. **FormKey** `local:Plugin` (optional `0x`, at most 8 hex digits before the colon). Resolved to a current-session FormID by `FormKey::ToFormID`, which handles light (ESL/ESPFE) plugins. A string with a colon that isn't a valid FormKey (an EditorID with `:` in it) falls through silently. When the entry is saved, `ResolveFormKeys()` stores the FormKey, so the row keeps matching after load-order changes (see BLOCKING_RULES.md).
2. **`0x` hex**. Leading zeros are stripped, the value must be at most 8 hex digits, and it is taken as a full FormID.
3. **Bare hex**. Also stripped, also at most 8 digits, and logged as `Parsed bare hex string as FormID`.
4. Otherwise the string is an **EditorID** (returns `{0, value}`).

### Merge semantics

The import is **additive upsert**. Both `AddToBlacklist(e, false)` and `AddToWhitelist(e)` run response-text enrichment, assign FormKeys, then look for an existing row (`FindExistingEntry`: FormKey, EditorID, then FormID for keyless rows). A matching row is **UPDATEd with every column**, including notes, filter_category, block_type, the actor/faction filters (reset to empty), and response_text. Rows are never deleted: removing a line from the YAML does nothing. Use the Clear actions or the per-row delete in the UI. Scene rows call `SceneHook::UpdateSceneConditions` inside `AddToBlacklist`, so the gate applies immediately (see SCENE_BLOCKING.md). Order: Blacklist YAML → Whitelist YAML → SubtypeOverrides. A topic listed in both the Blacklist YAML and the overrides therefore ends up with the override's category.

### "Import Scenes" vs "Import from YAML"

- **Import Scenes** (MCM and Prisma) re-imports the curated hard-coded lists from `Config_Scenes.cpp` (`GetHardcodedScenesList`, `GetBardSongQuestsList`, `GetFollowerCommentaryScenesList`) with categories Scene / BardSongs / FollowerCommentary. It ignores the meta flag, so it restores rows the user deleted. The first-run import at kDataLoaded does only Scenes and FollowerCommentary. See SCENE_BLOCKING.md.
- **Import from YAML** runs `ImportYAMLToDatabase()` over the three files above.

## Log lines to grep (`STFU.log`)

| Line | Meaning |
|------|---------|
| `Looking up TESGlobals from STFU.esp...` / `Global 'X' not found in loaded ESPs` | `Config::Load` lookups. A warning here means STFU.esp is missing or disabled, or an EditorID is misspelled. The toggle then silently does nothing and is never written to the INI |
| `Loaded N subtype globals from STFU.esp (5 master toggles + M subtypes)` | Expected: `71 … + 66` (the "5" is hard-coded, not counted) |
| `[PERSISTENCE] Loading settings from INI:` / `INI file not found, will be created on first save` | `LoadSettings` start |
| `[PERSISTENCE] Master toggles loaded: blacklist=…, hotkey=0x…` | Post-load values (`-1` = global missing) |
| `[PERSISTENCE] Loaded N subtype toggles` | Should be 66 |
| `[MAIN] kPostLoadGame event fired - reloading settings from INI` | The save's global values are being overwritten |
| `[PERSISTENCE] Settings saved to INI` | debug level only (`stfu_debug.flag`) |
| `[Config::ToggleSubtypeFilter] Toggled subtype N filter from … to …` | Prisma subtype toggle |
| `[PrismaUIMenu::OnSetXxx] Set STFU_Xxx global to V` | Prisma master toggle |
| `[PrismaUIMenu] Sending settings data: {...}` | Exact state pushed to the UI |
| `[PapyrusInterface] … called from MCM` | Every MCM native |
| `[Config] Blacklist YAML import: N topics, …` / `Whitelist YAML import: …` / `Subtype overrides import complete: N successful, M failed` / `YAML import complete - …` | Import summary |
| `[Config] Blacklist YAML import error:` / `Subtype overrides YAML parse error:` | yaml-cpp exception. That file's import stops; the others continue |
| `[Config] Plugin not found for FormKey:` | The FormKey's plugin isn't loaded. The string is then retried as hex and finally treated as an EditorID |
| `[DialogueDB] ShouldSoftBlock: … category='X' toggle DISABLED -> ALLOW` | Row exists but its gate global is off (debug) |

## Gotchas / known issues

- **Follower Commentary checkbox was once inverted** (fixed in `68a97b9`). The UI showed it ticked while `STFU_FollowerCommentary = 0`, and "Enable All" turned commentary on. Before `e86ddb7` this was harmless, because commentary scenes followed the Scenes toggle. The invariant now: **every toggle global is 1 = blocked; the UI sends `enabled`/`blocked` straight through and reads back `value >= 0.5`.** Never add an inversion in `SetToggleFromUI` or `SendSettingsData`.
- **`STFU_PreserveGrunts` is named backwards.** 1 means *block* grunts. The `Config.h` comment "(0=filter, 1=preserve)" and the `DEFAULT_PRESERVE_GRUNTS` comment "(filter grunts)" are misleading. The grunt exemption applies only when the topic's subtype toggle is on (`ShouldSoftBlock`).
- **`STFU_Refuse` is bound to subtype 13 (`kReject`), not 17 (`kRefuse`)** in `SubtypeGlobalMap`, and its INI key is `Reject`. Subtype 17 has no toggle. It has not been verified in game which subtype vanilla follower refusals use. Check this before changing it, because changing it silently resets users' stored value.
- **Subtype-override names must match the global suffix.** `SubtypeNameMap` is used to validate `overrides` values, but `IsFilterCategoryEnabled` matches against `SubtypeGlobalMap` suffixes. Overrides to `MurderNPC`, `AssaultNPC`, `PickpocketNPC`, `StealFromNPC`, `TrespassAgainstNPC`, `WereTransformCrime`, `Reject`, or any subtype without a global (e.g. `Scene`, `ForceGreet`) import "successfully" but **never block**. The template's "COMMON SUBTYPES" list also includes `Combat`, `Detection`, `Service`, and `Misc`, which are not valid names and are rejected.
- **An all-hex EditorID (e.g. `BEEF`) is parsed as a FormID.** Use the record's FormKey for such records instead. (Before 1.2.0, FormKeys for light plugins also failed to resolve; `FormKey::ToFormID` fixed that.)
- **Prisma "Enable/Disable All" sends one `toggleSubtypeFilter` per subtype** whose store state differs. Each message is a flip, plus an INI save, plus a full history re-serialize. With a stale store this can flip the wrong way. The store's initial defaults (`stores/settings.ts`) are `true` for all master toggles until the first `updateSettings` arrives.
- **Subtype checks are inconsistent about the threshold.** `ShouldSoftBlock` uses `value > 0.0f` while everything else uses `>= 0.5f`. This is harmless while values stay 0/1, but a console `set STFU_Hello to 0.3` would split them.
- **MCM changes are not saved until the MCM closes.** A crash with the MCM open loses them. After the next load the INI wins.
- **Setting a global from the console or another mod is temporary.** It is overwritten on the next save load unless something calls `SaveSettings()`.
- Prisma `OnImportYAML` reports success whenever a file exists. Parse errors are caught inside `ImportYAMLToDatabase` and only logged. It also refuses to run when only `STFU_SubtypeOverrides.yaml` exists.
- The `CellLoadEventHandler` in `main.cpp` is a no-op and is not related to settings.

## How to…

### Add a new master toggle end to end
1. **ESP**: add a GLOB `STFU_NewThing` (short, value 0) in xEdit/CK. STFU.esp is ESL-flagged, so the new FormID must stay in the ESL range (current highest `0xD62`).
2. **Config**: add `RE::TESGlobal* newThingGlobal` to `MCMSettings` (`src/Config.h`), look it up in `Config::Load()` (and fix the hard-coded "5 master toggles" count in the log), and add a getter if other modules need it.
3. **Meaning**: if it gates DB rows, add a `filterCategory` branch in `IsFilterCategoryEnabled` (`Config.cpp`). If it gates scenes, add it to `GetSceneGateGlobalForCategory` (`Config_Scenes.cpp`) and to the SceneHook global set (see SCENE_BLOCKING.md).
4. **INI**: add `DEFAULT_NEW_THING`, plus one line each in `SaveSettings()` and `LoadSettings()` (`[Settings]`, new key), and extend the `Master toggles loaded` log.
5. **Prisma C++**: declare `OnSetNewThing` in `PrismaUIMenu.h`, implement it as a one-liner via `SetToggleFromUI` in `PrismaUIMenu_Settings.cpp`, register the listener in `PrismaUIMenu.cpp`, and add the field to `SendSettingsData()`.
6. **Web UI**: add the field to `stores/settings.ts`, a setter to `lib/skse-api.ts`, and a `<Toggle>` to `settings.tsx`. Rebuild and copy to `PrismaUI/views/STFU/` (see WEB_UI.md).
7. **MCM** (optional): add a `GlobalVariable Property STFU_NewThing Auto`, an `oid_`, an `AddToggleOption`, an `OnOptionSelect` branch, `OnOptionHighlight` text, and entries in Enable/Disable All. **Fill the property on the MCM quest's script in STFU.esp**, recompile to `Scripts/STFU_MCM.pex`, and test.
8. For a non-global setting (like `menuHotkey`), add a `Settings` field, the INI read/write, and a Papyrus native (`RegisterFunctions`) and/or a Prisma listener. Skip steps 1 and 3.

### Add a new subtype toggle
Add the GLOB to the ESP, then add `{id, "STFU_Name"}` to `SubtypeGlobalMap`. The INI key comes from `SubtypeNameMap` automatically. Keep the global suffix **identical** to the `SubtypeNameMap` name so that subtype overrides work. Add `{ id, name, tooltip }` to the right list in `settings.tsx`. For the MCM, follow step 7 above.

### Add a new YAML section
1. Parse it in `ImportYAMLListSections` if it applies to both lists, or in the per-file block in `ImportYAMLToDatabase` (like `plugins:`). Build a `DialogueDB::BlacklistEntry` with the right `targetType` (see `BlacklistTarget` in `include/DialogueDatabase.h` and DATABASE.md) and `filterCategory = listName`, then call `add(...)`.
2. Add a counter, include it in the summary log and in the returned total.
3. Add a commented example to the template in `GenerateDefaultYAMLs()`. Existing users will not get it, so document the section in README.md as well.
4. Say in the section comment whether identifiers go through `ParseFormIdentifierInternal` (topics, overrides) or are EditorID-only (scenes, quests, plugins).

## Related docs

INDEX.md · ARCHITECTURE.md · BLOCKING_RULES.md (how globals and filter categories are evaluated) · SCENE_BLOCKING.md (scene gates, hard-coded lists) · DATABASE.md (upsert matching, meta flags, Clear) · PRISMA_UI_BRIDGE.md · WEB_UI.md · DEVELOPMENT.md (building, deploying the pex/esp) · README.md · DIALOGUE_SUBTYPES.md
