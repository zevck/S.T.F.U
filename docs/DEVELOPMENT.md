# Developing STFU

This doc covers how to build, deploy, debug and verify STFU, and lists what not to trip over. The repo root **is** the MO2 mod folder (`MODS/mods/STFU`). "Deploy" therefore means copying build output into the repo's own `SKSE/Plugins/` and `PrismaUI/views/STFU/` folders.

There is **no automated test suite**. `git ls-files` contains no test or spec files, CMake builds no test target (`BUILD_TESTS OFF` for CommonLib), and `web-ui/package.json` has no `test` script. Every change is verified **in game, through the logs** (see [Verifying a change in game](#verifying-a-change-in-game)).

---

## Build (C++ plugin)

Prerequisites:
- MSVC (x64) with C++23
- CMake ≥ 3.21
- vcpkg at `C:/vcpkg`. The path is hardcoded in `CMakePresets.json` → `CMAKE_TOOLCHAIN_FILE`.

```powershell
# once, or after pulling a submodule bump. --recursive matters: CommonLib has a nested
# submodule (lib/commonlibsse-ng/extern/openvr) that the build needs.
git submodule update --init --recursive

cmake --preset vs2022-windows                      # configures into build/
cmake --build build --config Release --target STFU # -> build/Release/STFU.dll
# equivalent: cmake --build --preset release
```

| Fact | Where |
|---|---|
| Plugin version `1.2.0` | `CMakeLists.txt` `project(VERSION)` and `vcpkg.json` `version-string`. Keep the two in sync. |
| vcpkg triplet forced to `x64-windows-static`, MSVC runtime static (`/MT`) | `CMakeLists.txt` top |
| vcpkg deps: `spdlog`, `yaml-cpp`, `sqlite3` (STFU); `rapidcsv`, `directxtk` (CommonLib) | `vcpkg.json` |
| CommonLibSSE-NG added with `add_subdirectory(... EXCLUDE_FROM_ALL)`; plugin declared with `add_commonlibsse_plugin(... USE_ADDRESS_LIBRARY SOURCES ...)` | `CMakeLists.txt` |
| Detours compiled from `lib/detours/src/*.cpp` as a static lib (C++17) | `CMakeLists.txt` `add_library(Detours STATIC ...)` |
| IPO/LTCG on; PCH `include/PCH.h` | `CMakeLists.txt` |
| **New `.cpp` files must be added to the `SOURCES` list by hand.** There is no glob. | `CMakeLists.txt` |
| The `vs2022-windows` preset uses the generator `"Visual Studio 18"` despite its name. There is also a `ninja-release` preset. | `CMakePresets.json` |

The submodules are pinned: `lib/commonlibsse-ng` at `v9.1.0` (alandtse fork, branch `ng`), `extern/openvr` at `v1.0.15`, and `lib/detours` at `v4.0.1-110-g309926a`.

## Build (web UI)

```powershell
cd web-ui
npm ci            # or npm install
npm run build     # = tsc && vite build  -> web-ui/dist/{index.html, assets/}
npm run dev       # browser dev server on :5173 (no SKSE bridge; window.* listeners absent)
```

`vite.config.ts` sets `base: './'`, which gives relative asset paths so PrismaUI can load the bundle from disk. `npm run lint` is defined, but the repo has no ESLint config file (`eslint.config.*`), so do not expect it to work as is.

## Deploy

**Close the game first.** The DLL is locked while Skyrim runs, and the build does **not** auto-deploy.

```powershell
# plugin
Copy-Item build\Release\STFU.dll SKSE\Plugins\STFU.dll
# (alternative: `cmake --install build --config Release` - install() rules copy the DLL + PDB
#  to <repo>/SKSE/Plugins because CMAKE_INSTALL_PREFIX is forced to the repo root; not the
#  author's usual path, not verified)

# web UI - clear old hashed bundles first, then copy dist contents
Remove-Item PrismaUI\views\STFU\assets -Recurse -Force
Copy-Item web-ui\dist\* PrismaUI\views\STFU\ -Recurse
```

- `PrismaUI/views/STFU/index.html` is tracked in git, but `assets/` is gitignored. A rebuild changes the hashed filenames, so `index.html` shows up as modified after every UI deploy. That is expected.
- Stale bundles pile up if `assets/` is not cleared. At the time of writing it holds 5 files, and `index.html` references only 2 of them.
- Papyrus: the repo has no build step for `Source/Scripts/STFU_MCM.psc` → `Scripts/STFU_MCM.pex`. Compile it by hand with the Papyrus compiler. It imports SkyUI's `SKI_ConfigBase` and JContainers' `JValue`.
- `STFU.esp` is edited with xEdit or the CK. It holds the toggle globals that `Config::Load()` looks up by EditorID.

---

## Logging

### `src/Logger.h` — the plugin log

| | |
|---|---|
| File | `Documents/My Games/Skyrim Special Edition/SKSE/STFU.log`, truncated on each launch (`basic_file_sink_mt(path, true)`) |
| Setup | `Logger::Setup()` is the first call in `SKSE_PLUGIN_LOAD`. `SKSE::Init(a_skse, false)` is passed `a_log=false` on purpose, so that SKSE does not replace the sink and level. |
| Level | `info` by default. It is `debug` if `stfu_debug.flag` (an empty file is fine) exists **next to STFU.log**. The flag is read once at startup, so restart the game after adding or removing it. `trace` calls are never emitted. |
| Pattern | `[%Y-%m-%d %H:%M:%S.%e] [%l] %v`, flushed on every message at the active level |
| Style | Use `spdlog::info/debug/warn/error/trace` and a bracketed tag prefix per area: `[POPULATE]`, `[HARD BLOCK]`, `[SOFT BLOCK]`, `[SetSubtitle]`, `[CONSTRUCT ENTRY]`, `[CTOR ENTRY]`, `[SCENE BLOCKER]`, `[SCENE UPDATE]`, `[PERSISTENCE]`, `[PrismaUIMenu]`, `[DialogueDB]`, `[MAIN]` |
| JS logs | The web UI calls `window.jsLog(msg)`, which is registered as listener `"jsLog"`. That becomes `[PrismaUIMenu::JS] ...` at info level. |

### `src/DialogueLogger.cpp` — the human-readable dialogue log

`Data/SKSE/Plugins/STFU/STFU_DialogueLog.txt` is opened in append mode by `DialogueLogger::Initialize()`, which is called from `Database::Initialize()`. It rotates to `.old` at 10 MB. `Database::LogDialogue()` writes two lines per history entry:

```
[HH:MM:SS] [STATUS] [Subtype] [QuestEditorID|NoQuest] [0FormID] [Menu]? Speaker: text (≤100 chars)
  - TopicEditorID-or-FormID          <- paste-ready YAML list item
```

`[Menu]` is printed when `skyrimNetBlockable` is set, meaning the TopicInfo was in `MenuTopicManager::dialogueList`. Only lines that pass every history filter appear here, so the file is not a record of everything that went through the hooks. The timestamp is the wall-clock time at write, not the entry's own timestamp.

---

## Verifying a change in game

At `info` level, a healthy start writes these lines, in this order:

| Log line (substring) | Proves |
|---|---|
| `STFU loaded (debug logging: on/off)` | The DLL loaded and the logger is set up. It also confirms whether the debug flag was picked up. |
| `Registered Papyrus interface for STFU_MCM`, later `[PapyrusInterface] Registered all Papyrus functions` | The MCM natives are bound |
| `Loaded N subtype globals from STFU.esp` | `STFU.esp` is loaded. Missing globals appear as `Global '...' not found in loaded ESPs`. |
| `Initializing database at: ...`, `[DialogueLogger] Initialized`, `Database initialized: ...` | The DB and dialogue log are open. Check the path under MO2. |
| `[PERSISTENCE] Master toggles loaded: blacklist=.., scenes=.., ...` | The INI was applied to the globals. It is logged again after `[MAIN] kPostLoadGame event fired`. |
| `SceneMonitor: Initialized with 3 bard quests` | Bard quests were found. This depends on EditorIDs, so po3's Tweaks must be present. |
| `PopulateTopicInfo hook installed successfully` | Detours hook #1 is in. Failure: `Failed to install PopulateTopicInfo hook!` |
| `Scene blocker ready` | `SceneHook::Install()` ran. It only logs. |
| `ConstructResponseHook: SetSubtitle call site 0x.. calls 0x..` + `SetSubtitle hook installed` | The E8 guard passed and the call site was patched. The logged call target is what you compare across runtimes. |
| same for `ConstructResponse` | Call site hook #2 is in. Failure: `no CALL at the ... call site ... hook skipped` |
| `DialogueItem::Ctor hook installed (Detours - early execution)` | Detours hook #2 is in |
| `[PrismaUIMenu] Initialized successfully`, `Registered menu hotkey handler` | The view and hotkey are set up. Failure: `Failed to get PrismaUI API. Is PrismaUI installed?` |
| `[MAIN] Registered LoadingMenuSink for deferred scene patching` | The deferred scene patching sink is registered |
| `[SCENE BLOCKER] Patching complete — N scenes (M phases) patched` | Scene gating was applied |
| `STFU initialized successfully` | `kDataLoaded` finished |
| `[PrismaUIMenu] DOM ready for view N` | The web bundle loaded, logged when the view's DOM is first ready |

To see behaviour, turn on the debug flag and grep for:
- `[POPULATE]` / `[HARD BLOCK]` / `[POPULATE SILENCE]` for decisions
- `[SetSubtitle] BLOCKING` / `[SOFT BLOCK]` / `[AUDIO CLEARED]` (warn) for what was blanked
- `MATCH FOUND` / `NO MATCH` / `COOLDOWN SKIP` / `Skipping unchosen candidate` / `TEXT DUPLICATE SKIP` to see why history did or did not record a line
- `[STALE CACHE]` (warn) when a cached decision was applied to a different TopicInfo

Follow the team rule: **diagnose with a log that shows the symptom before fixing anything**, and don't change behaviour because of a single anecdotal report.

---

## Engine touchpoints

These are every address, offset and engine-shape assumption the plugin makes. Anything here can break on a new runtime.

| Touchpoint | Value | File / function | Mechanism |
|---|---|---|---|
| `DialogueItem::Ctor` | `REL::VariantID(34413, 35220, 0x572FD0)` | `src/main.cpp` `DialogueItemCtorHook::Install()` | Detours `DetourAttach`. The signature is declared by hand: `(DialogueItem*, TESQuest*, TESTopic*, TESTopicInfo*, Actor*) → DialogueItem*` |
| `PopulateTopicInfo` | `REL::VariantID(34429, 35249, 0x573B70)` | `src/PopulateTopicInfoHook.cpp` `Install()` | Detours. The signature is declared by hand: `(int64, TESTopic*, TESTopicInfo*, Character*, TESResponse*) → int64`. Returning `0` without calling the original is the hard block. |
| SetSubtitle call site | same function `+ REL::Relocate(0x61, 0x61)` | `src/ConstructResponseHook.cpp` `Install()` | `trampoline.write_call<5>` after checking for an `0xE8` opcode. Hand-declared `(DialogueResponse*, char*, int32) → char*` |
| ConstructResponse call site | same function `+ REL::Relocate(0xDE, 0xDE)` | `src/ConstructResponseHook.cpp` `Install()` | `write_call<5>` + E8 guard. Hand-declared `(TESResponse*, char* path, BGSVoiceType*, TESTopic*, TESTopicInfo*) → bool`. It `strcpy`s `Sound\STFU\silent.fuz` into the engine's path buffer. |
| GetResponseList | `REL::VariantID(25083, 25626, 0x3A3000)` | `src/TopicResponseExtractor.cpp` `GetResponseList()` | Called directly, `(TESTopicInfo*, TESResponse**)`. The list comes back through the out param. |
| Trampoline | 256 bytes, taken from the SKSE branch pool or `trampoline.create()` | `src/main.cpp` `kDataLoaded` | Holds the two `write_call`s |
| Condition function `74` (GetGlobalValue) | `FUNCTION_DATA::FunctionID(74)` | `src/SceneHook.cpp` `CreateGlobalDisabledCondition()`, `RemoveConditionsFromScene()` | A hand-built `TESConditionItem` (`RE::malloc(sizeof)` + `memset`) pushed onto `BGSScene` phase `startConditions`. Depends on CommonLib's layout of `TESConditionItem` / `CONDITION_ITEM_DATA`. |
| Struct fields (via CommonLib layouts) | `BGSScene::{phases, actions, isPlaying, parentQuest}`, `TESQuest::scenes`, `BGSSceneActionDialogue::topic`, `TESTopic::{data.subtype, ownerQuest}`, `TESResponse::{responseText, speakerIdle, listenerIdle, emotionType, emotionValue, flags, next}`, `MenuTopicManager::dialogueList` / `parentTopicInfo` | `ConstructResponseHook.cpp`, `PopulateTopicInfoHook.cpp`, `SceneHook.cpp`, `SceneMonitor.cpp` | Reads, and writes to `isPlaying` and the response fields. These are correct only if CommonLib's RE headers match the runtime. |
| po3's Tweaks export | `GetProcAddress(GetModuleHandleW(L"po3_Tweaks"), "GetFormEditorID")` | `src/EditorID.h` `STFU::GetEditorID()` | Resolved once. Falls back to `TESForm::GetFormEditorID()` |
| Menu name | `"Loading Menu"` | `src/main.cpp` `LoadingMenuSink` | `MenuOpenCloseEvent` |
| Player FormID | `0x14` | `PopulateTopicInfoHook.cpp` | Excludes the player's lines from history |

The third value in each `VariantID` is the VR offset, which has not been tested. Only **1.6.1170** has been tested in game. 1.7.x (which CommonLib classifies as `Runtime::AE`, so it uses the AE IDs and offsets) and SE 1.5.97 have not been verified.

### Checklist: new Skyrim runtime or CommonLib bump

1. Update `lib/commonlibsse-ng` to a tag, run `git submodule update --init --recursive`, and rebuild from a clean `build/` (reconfigure).
2. Confirm that Address Library ships the IDs 34413/35220, 34429/35249 and 25083/25626 for the target runtime.
3. Start the game and compare the `ConstructResponseHook: ... call site 0x.. calls 0x..` lines with a known-good 1.6.1170 log. If there is `no CALL at the ... call site`, or the call target changed to an unrelated function, the +0x61 / +0xDE offsets need to be found again in a disassembler.
4. Check the hand-declared signatures (above) against the new binary. If they have changed, the compiler cannot catch it.
5. For a CommonLib bump: diff the RE headers for the struct fields listed above, `TESConditionItem`'s size and layout, and the `SKSE::Init` / trampoline APIs. (v9 dropped the trampoline fallback in `AllocTrampoline`, which is why `main.cpp` now allocates by hand.)
6. Check in game: a soft-blocked greeting has no subtitle or audio, a hard-blocked topic never starts, a blocked ambient scene does not start, a soft-blocked scene line advances (silent `.fuz`), bard songs stop, the History tab fills, and PrismaUI opens with the hotkey.
7. If Dynamic Dialogue Replacer is installed, repeat step 6 with it. It hooks the same three sites.

---

## Code conventions (as observed)

- **License header**: each file in `src/` and `include/` starts with the GPL-3.0 block (`STFU - a Skyrim SKSE plugin ... Copyright (C) 2026 Zevick`). The one exception is the vendored `include/PrismaUI_API.h`.
- **Line endings**: LF in the index (`.gitattributes` is `* text=auto`). New files should be LF.
- **Structure**: one namespace per component (`PopulateTopicInfoHook`, `ConstructResponseHook`, `SceneHook`, `SceneMonitor`, `Config`, `DialogueDB`, `SettingsPersistence`, `PapyrusInterface`, `DialogueLogger`, `TopicResponseExtractor`, `STFU` for helpers). `PrismaUIMenu` is a class with static members. Large components are split as `Component_Area.cpp` with a private `ComponentInternal.h` (for example `Config_Scenes.cpp` + `ConfigInternal.h`, `DialogueDatabase_Blacklist.cpp` + `DialogueDatabaseInternal.h`, `PrismaUIMenu_History.cpp`).
- **Naming**: PascalCase functions. Hook bodies are `Hook_<Name>`, and the originals are `_<Name>` / `_Original<Name>`. File-scope statics use a `g_` prefix. Class data members have a trailing `_` (`dbMutex_`, `prismaUI_`, `view_`). Web UI files are kebab-case (`entry-list.tsx`, `skse-api.ts`).
- **Includes**: `#pragma once`, `#include "../include/PCH.h"`, spdlog through the PCH or `<spdlog/spdlog.h>`.
- **EditorIDs**: always go through `STFU::GetEditorID(form)`, never `form->GetFormEditorID()` directly. Copy the result into a `std::string` if you keep it.
- **JSON from the UI**: use the helpers in `src/PrismaUIMenuJson.h` (`ReadJsonString`, `FindJsonValue`, `ExtractJsonValue`, `ExtractJsonStringArray`, `ParseHexFormIDs`). Don't hand-roll a parser in a handler.
- **DB policy**: no migrations. `UpdateSchema()` only adds missing columns. Keep legacy columns such as the SkyrimNet ones, and `BlockType::SkyrimNet`, for old rows.
- **Commits**: the author commits. Commit messages have unwrapped bodies.

---

## Gotchas

- **git `core.autocrlf=true`** on the author's machine. A file restored with `git checkout`/`git restore` comes back **CRLF** in the working tree (`include/PrismaUI_API.h` is currently `w/crlf`). Normalize it to LF before editing or committing if it matters.
- **Shell heredocs mangle backslashes** on this machine (Git Bash / PowerShell). Write files with a file-writing tool, not `cat <<EOF`, especially for Windows paths and C++ string escapes such as `"Sound\\STFU\\silent.fuz"`.
- **Dead-code checks need real call sites.** A handler is live if JS calls `sendToSKSE('name'` (or `window.name(...)`) **and** C++ registers it with `RegisterJSListener(view_, "name", ...)`. One C++ handler can be registered under several names (`OnUpdateBlacklistEntry` also serves `"updateBlacklistEntryAdvanced"`). A loose grep for the function name finds comments and near-namesakes, so it gives the wrong answer.
- **Hand-copied parsers drift.** Before 68a97b9 the handlers each had their own JSON readers. That commit deduplicated them and, in the same change, fixed whitelist edits that saved a bogus actor filter. Use `PrismaUIMenuJson.h`.
- **The MCM script has legacy leftovers.** `STFU_MCM.LoadConfig()` reads `Data/STFU Patcher/Config/STFU_Config.ini` through JContainers' `JValue.readFromFile`. That is not the plugin's INI (`SKSE/Plugins/STFU/config/STFU_Config.ini`), and `JValue` expects JSON. `CheckSkyrimNetInstallation()` still sets a SkyrimNet filter flag, but that filter has been removed. Treat the MCM's own config reading as unverified.
- **DLL locked while the game runs.** Copying to `SKSE/Plugins/STFU.dll` fails silently if a script ignores errors. Close the game first and check the file's timestamp.
- **Settings load only if the DB opens.** At `kDataLoaded`, `SettingsPersistence::LoadSettings()` and `SceneMonitor::Initialize()` are inside the `Initialize()` success branch. If the DB fails, the globals keep their ESP defaults until `kPostLoadGame`. The hooks install either way.
- **The debug flag is read once**, at `SKSE_PLUGIN_LOAD`.
- **Scene edits**: never add conditions to a running scene (NPCs soft-lock). Use the `isPlaying` check plus `PatchDeferredScenes`. Never edit scenes off the main thread.
- **`thread_local` decision cache**: the SetSubtitle and ConstructResponse hooks see only the decision made by PopulateTopicInfo on the *same thread*. Don't move the decision anywhere that breaks that nesting.
- **MO2**: runtime files (`dialogue.db`, INI, YAML, dialogue log) land in `overwrite/`, not the mod folder.
- **PrismaUI assets accumulate** unless `PrismaUI/views/STFU/assets/` is cleared before copying.

---

## Related docs

- [ARCHITECTURE.md](ARCHITECTURE.md): system overview, startup, threading
- [DIALOGUE_HOOKS.md](DIALOGUE_HOOKS.md): hook internals
- [SCENE_BLOCKING.md](SCENE_BLOCKING.md): condition patching
- [DATABASE.md](DATABASE.md): schema and queue
- [CONFIG_AND_SETTINGS.md](CONFIG_AND_SETTINGS.md): INI, globals, YAML, MCM
- [PRISMA_UI_BRIDGE.md](PRISMA_UI_BRIDGE.md) and [WEB_UI.md](WEB_UI.md): the menu
- [INDEX.md](INDEX.md)
