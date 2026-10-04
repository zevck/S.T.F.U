# PrismaUI Bridge (C++ side of the in-game menu)

`PrismaUIMenu` is a static class that owns STFU's single PrismaUI view (`STFU/index.html`),
registers the JS→C++ listeners the React app calls, serializes the history / blacklist /
whitelist / settings to JSON, and pushes them back into the page. Everything is hand-rolled
string JSON — there is no JSON library on either the read or the write side. This doc is the
protocol reference; the React side is in [WEB_UI.md](WEB_UI.md).

## Overview

```
 React (web-ui)                     PrismaUI.dll                        STFU.dll (game main thread)
 ──────────────                     ────────────                        ───────────────────────────
 SKSE_API.sendToSKSE(name, str) ──> window.<name>(str)                  
                                    (bound by RegisterJSListener)       
                                    wrapper: SKSE::GetTaskInterface()   
                                             ->AddTask(...)       ────> PrismaUIMenu::On<Xxx>(const char*)
                                                                          │ parse with PrismaUIMenuJson.h
                                                                          │ touch DialogueDB / Config / RE::
                                                                          ▼
 window.SKSE_API.call(evt, json) <── Invoke(view, "window.SKSE_API?.call('updateHistory','<json>')")
 window.handleXxx({...})          <── Invoke(view, "window.handleXxx({...});")
 window.showToast(msg, type)      <── Invoke(view, "window.showToast('...', 'success')")
```

Two push styles exist and both are live:
- **Event bus** — `BuildSKSEUpdateScript(event, json)` emits `window.SKSE_API?.call('<event>', '<json as single-quoted JS string>')`; the page `JSON.parse`s the string. Used for `updateHistory`, `updateBlacklist`, `updateWhitelist`, `updateSettings`.
- **Direct global call** — `window.handleIdentifierDetection(<json literal>)`, `window.handleNearbyActors(<json literal>)`, `window.showToast('<msg>', '<type>')`. The JSON is spliced in as a JS object literal, not a string.

`InteropCall` (declared in `include/PrismaUI_API.h`) is **not used** anywhere in STFU; every push is `Invoke`.

## Key files

| File | Contents |
|------|----------|
| `include/PrismaUI_API.h` | Vendored PrismaUI modder API (`IVPrismaUI1`, `RequestPluginAPI`). Don't edit; re-copy from PrismaUI if the API changes. |
| `include/PrismaUIMenu.h` | The static class: public `Initialize/Toggle/IsOpen/Send*Data`, private `On*` handlers and `Serialize*ToJSON`. |
| `src/PrismaUIMenu.cpp` | `Initialize()` (API request, `CreateView`, listener registration, scroll size, hide), `OnDomReady`, `OnCloseMenu`, `OnLog`, `Toggle`, `IsOpen`. |
| `src/PrismaUIMenu_History.cpp` | `OnRequestHistory`, `SendHistoryData`, `SerializeHistoryToJSON` (recomputes each row's status), `OnDeleteHistoryEntries`. |
| `src/PrismaUIMenu_Blacklist.cpp` | `OnRequestBlacklist`, `OnDeleteBlacklistEntry`, `OnDeleteBlacklistBatch`, `OnRefreshBlacklist`, `OnUpdateBlacklistEntry`, `SendBlacklistData`, `SerializeBlacklistToJSON`. |
| `src/PrismaUIMenu_Whitelist.cpp` | `SendWhitelistData`, `SerializeWhitelistToJSON`, `OnRequestWhitelist`, `OnRemoveFromWhitelist`, `OnUpdateWhitelistEntryAdvanced`, `OnRemoveWhitelistBatch`. |
| `src/PrismaUIMenu_Entries.cpp` | `OnDetectIdentifierType`, `OnCreateAdvancedEntry`, `OnGetNearbyActors`. |
| `src/PrismaUIMenu_Settings.cpp` | `OnToggleSubtypeFilter`, `OnImportScenes`, `OnImportYAML`, anon-namespace `SetToggleFromUI` + the five `OnSet*` toggles, `SendSettingsData`, `OnRequestSettings`. |
| `src/PrismaUIMenuJson.h` | `PrismaUIMenuDetail` helpers: `escapeJSON`, `EscapeSingleQuotedJSString`, `BuildSKSEUpdateScript`, `ReadJsonString`, `FindJsonValue`, `ExtractJsonValue`, `ExtractJsonStringArray`, `ParseHexFormIDs`. |
| `src/main.cpp` | `InputEventSink` (hotkey), and the `kDataLoaded` block that calls `PrismaUIMenu::Initialize()` then registers the sink. |
| `src/DialogueDatabase_History.cpp` `ProcessQueue()` | The only non-UI caller that pushes to the page: after a history write it queues `SendHistoryData()` via `AddTask` if the menu is open. |

## Lifecycle

1. **`kDataLoaded`** (`src/main.cpp`, after the hooks are installed): `PrismaUIMenu::Initialize()`.
   - `RequestPluginAPI(V1)` → `GetModuleHandleW(L"PrismaUI.dll")`. If PrismaUI isn't loaded: `error` log and return; the menu silently doesn't exist, everything else in STFU still works.
   - `CreateView("STFU/index.html", &OnDomReady)` — path is relative to PrismaUI's views root, i.e. `PrismaUI/views/STFU/index.html` in the mod folder.
   - 25 `RegisterJSListener` calls (table below), `SetScrollingPixelSize(view_, 100)`, `Hide(view_)`, `initialized_ = true`.
   - Then `BSInputDeviceManager::AddEventSink(InputEventSink::GetSingleton())`.
2. **DOM ready** → `OnDomReady`: pushes history, blacklist, whitelist **and settings**.
3. **Hotkey** (`InputEventSink::ProcessEvent`): keyboard `kButton` events with `IsDown()` whose `GetIDCode() == Config::GetSettings().menuHotkey` (default `0xD2` = Insert; persisted as `MenuHotkey` in `STFU_Config.ini`; MCM changes it through Papyrus `STFU_MCM.SetMenuHotkey`) call `PrismaUIMenu::Toggle()`.
4. **`Toggle()`**: if `HasFocus(view_)` → `Unfocus` + `Hide`. Otherwise `Show` + `Focus(view_, /*pauseGame*/true, /*disableFocusMenu*/false)` and re-push history, blacklist, whitelist (**not** settings — the Settings tab re-requests them on tab switch).
5. **Close from the page**: ESC (`keyCode 27`, Ultralight reports the key as "Unidentified") or the X button → `closeMenu` → `OnCloseMenu` → `Toggle()`.

`IsOpen()` is `HasFocus(view_)`, not "is visible". Not verified in-game whether the hotkey
still reaches `InputEventSink` while the view holds focus; ESC and the X button are the
dependable close paths.

## Threading

Per the vendored PrismaUI framework source (`.resources/PrismaUI/framework/src/API/API.cpp`):

- `RegisterJSListener` wraps every callback in `SKSE::GetTaskInterface()->AddTask(...)`, and the `OnDomReady` callback is wrapped the same way. **All `PrismaUIMenu::On*` handlers therefore run on the game's main thread**, which is why they can call `RE::TESForm::LookupByID`, walk `ProcessLists`, write `TESGlobal::value`, and hit SQLite directly without further marshalling. The argument is copied into the task, so `data` is valid for the whole handler.
- `Invoke` only enqueues the script (`Core::pendingScripts`); it runs on PrismaUI's Ultralight thread before its next update. Pushes are fire-and-forget and asynchronous; order of `Invoke` calls from one thread is preserved.
- Listeners are bound onto `window` when the page **finishes loading** (`Listeners.cpp`, `isLoadingFinished = true` → `BindJSCallbacks`). The React app's mount-time `sendToSKSE('requestHistory')` etc. may run before that and log `is not a function`; `OnDomReady` pushing all four data sets is what actually populates the first render. (Timing inferred from source, not measured.)
- Non-UI code must not call `Send*Data` from arbitrary threads or while holding DB locks. The one existing case (`DialogueDatabase_History.cpp` `ProcessQueue()`) defers with `AddTask` and checks `IsOpen()` first — copy that pattern.

The PrismaUI source in `.resources/` is a reference copy; the installed PrismaUI.dll could differ. Re-check `API.cpp` if you upgrade PrismaUI.

## Message protocol

JS always sends **one string argument** (PrismaUI stringifies whatever is passed; `undefined`
arrives as `""`). All callers go through `SKSE_API.sendToSKSE(name, data)` in
`web-ui/src/lib/skse-api.ts`, either directly or via a named wrapper on `SKSE_API`.

### JS → C++ listeners

| Listener (as registered) | C++ handler | Payload | Does | Pushes back | JS callers |
|---|---|---|---|---|---|
| `requestHistory` | `OnRequestHistory` | ignored | `SendHistoryData()` (flushes the DB write queue first) | `updateHistory` | `app.tsx` mount + tab switch; `requestHistoryRefresh()` from history.tsx, entry-list.tsx, advanced-edit-modal.tsx (usually `setTimeout(...,100–150)` after a mutation) |
| `requestBlacklist` | `OnRequestBlacklist` | ignored | `SendBlacklistData()` | `updateBlacklist` | `app.tsx` mount + tab switch (`requestBlacklistRefresh`) |
| `refreshBlacklist` | `OnRefreshBlacklist` | ignored | identical to `requestBlacklist` (logs at info) | `updateBlacklist` | `advanced-edit-modal.tsx` after save |
| `requestWhitelist` | `OnRequestWhitelist` | ignored | `SendWhitelistData()` | `updateWhitelist` | `app.tsx` mount + tab switch; advanced-edit-modal after save |
| `requestSettings` | `OnRequestSettings` | ignored | `SendSettingsData()` | `updateSettings` | `app.tsx` mount + Settings tab switch |
| `closeMenu` | `OnCloseMenu` | ignored | `Toggle()` | — | `app.tsx` ESC handler, X buttons |
| `jsLog` | `OnLog` | free text | `spdlog::info("[PrismaUIMenu::JS] {}")` if non-empty | — | `log()` in skse-api.ts (every UI log line) |
| `deleteBlacklistEntry` | `OnDeleteBlacklistEntry` | bare id, e.g. `"42"` (not JSON) | `std::stoll` → `RemoveFromBlacklist` | `updateBlacklist` (on success only) | `SKSE_API.deleteBlacklistEntry`: entry-list (single delete), history (remove actor/faction entry), advanced-edit-modal Delete |
| `deleteBlacklistBatch` | `OnDeleteBlacklistBatch` | comma list `"1,2,3"` (not JSON) | `RemoveFromBlacklistBatch` | `updateBlacklist` | `SKSE_API.deleteBlacklistBatch`: entry-list multi-delete |
| `updateBlacklistEntryAdvanced` | `OnUpdateBlacklistEntry` | `{id, blockType:"Soft"\|"Hard"\|"SkyrimNet", filterCategory, notes, actorFilterNames[], actorFilterFormIDs[], factionFilterEditorIDs[]}` | Loads the row from `GetBlacklist()`, overwrites blockType (+ blockAudio/Subtitles/SkyrimNet flags), filterCategory, notes and **all three filter arrays**, re-saves via `AddToBlacklist` (upsert) | `updateBlacklist` | `advanced-edit-modal.tsx` `handleSave` (blacklist entries) |
| `createAdvancedEntry` | `OnCreateAdvancedEntry` | `{identifier, blockType, category, notes, isWhitelist, actorFilterNames[], actorFilterFormIDs[], factionFilterEditorIDs[]}` | Resolves identifier (below), builds a `BlacklistEntry`, `AddToWhitelist` or `AddToBlacklist` | `updateWhitelist` or `updateBlacklist`, then `updateHistory` | `manual-entry-modal.tsx` `handleCreate` |
| `detectIdentifierType` | `OnDetectIdentifierType` | `{identifier}` | Classifies the identifier (below) | `window.handleIdentifierDetection({type, displayName, categories[]})` | `manual-entry-modal.tsx` (300 ms debounce) |
| `getNearbyActors` | `OnGetNearbyActors` | ignored | Walks `ProcessLists::highActorHandles` within 4096 units of the player, skipping unnamed actors | `window.handleNearbyActors({actors:[{name, formID:"0x%08X", distance}]})` | `actor-filter-picker.tsx` on mount (`requestNearbyActors`) |
| `removeFromWhitelist` | `OnRemoveFromWhitelist` | `{"id":N}` | `RemoveFromWhitelist` | `updateWhitelist` + `updateHistory` (on success) | entry-list (single), history (actor/faction entry), advanced-edit-modal Delete |
| `removeWhitelistBatch` | `OnRemoveWhitelistBatch` | `{"ids":[...]}` | `RemoveFromWhitelist` per id | `updateWhitelist` + `updateHistory` | entry-list multi-delete |
| `updateWhitelistEntryAdvanced` | `OnUpdateWhitelistEntryAdvanced` | `{id, notes, actorFilterNames[], actorFilterFormIDs[], factionFilterEditorIDs[]}` | Overwrites notes + filter arrays, `AddToWhitelist` | `updateWhitelist` | `advanced-edit-modal.tsx` (whitelist entries) |
| `deleteHistoryEntries` | `OnDeleteHistoryEntries` | `{"entryIds":[...]}` | `DeleteDialogueEntriesBatch` | `updateHistory` | `history.tsx` DEL key |
| `toggleSubtypeFilter` | `OnToggleSubtypeFilter` | `{"topicSubtype":N}` | `Config::ToggleSubtypeFilter(N)` (flips the subtype's MCM global) | `updateSettings`, then `updateHistory` (serialized inline, **without** `FlushQueue`) | `settings.tsx` toggles + Enable/Disable All; `history.tsx` "Toggle <subtype> Filter" |
| `setBlacklistEnabled` | `OnSetBlacklistEnabled` | `{"enabled":bool}` | `SetToggleFromUI` → `blacklist.toggleGlobal` | `updateSettings` | settings.tsx Master Controls |
| `setScenesEnabled` | `OnSetScenesEnabled` | `{"enabled":bool}` | → `mcm.blockScenesGlobal` | `updateSettings` | settings.tsx |
| `setBardSongsEnabled` | `OnSetBardSongsEnabled` | `{"enabled":bool}` | → `mcm.blockBardSongsGlobal` | `updateSettings` | settings.tsx |
| `setFollowerCommentaryEnabled` | `OnSetFollowerCommentaryEnabled` | `{"enabled":bool}` | → `mcm.blockFollowerCommentaryGlobal` | `updateSettings` | settings.tsx (Follower tab + Enable/Disable All) |
| `setCombatGruntsBlocked` | `OnSetCombatGruntsBlocked` | `{"blocked":bool}` | → `mcm.preserveGruntsGlobal` (1 = grunts blocked, despite the name) | `updateSettings` | settings.tsx (Combat tab + Enable/Disable All) |
| `importScenes` | `OnImportScenes` | ignored | `ImportHardcodedScenes` for `GetHardcodedScenesList()` ("Scene"), `GetBardSongScenesList()` ("BardSongs", the scenes of the bard song quests), `GetFollowerCommentaryScenesList()` ("FollowerCommentary") | `showToast(success\|error)`, `updateBlacklist`, `updateHistory` | settings.tsx Import Scenes |
| `importYAML` | `OnImportYAML` | ignored | Checks `<exe dir>/Data/SKSE/Plugins/STFU/import/STFU_{Blacklist,Whitelist}.yaml` exist, then `Config::ImportYAMLToDatabase()` | `showToast`, `updateBlacklist`, `updateWhitelist`, `updateHistory` | settings.tsx Import from YAML |

`SetToggleFromUI` (all five `set*` toggles) writes `global->value = 1.0/0.0`, calls
`SettingsPersistence::SaveSettings()` (INI), then `SendSettingsData()`. It does **not** refresh
history, so history statuses stay stale until the next `requestHistory`.

**Cross-check result (web-ui/src, 2026-09-26):** every registered listener has at least one
caller, and every `sendToSKSE` name has a listener. `requestBlacklist` and `refreshBlacklist`
are functionally duplicates.

### C++ → JS pushes

| Target | Sent by | Payload shape |
|---|---|---|
| `SKSE_API.call('updateHistory', s)` | `SendHistoryData`, `OnToggleSubtypeFilter` | array of up to 1000 rows (`GetRecentDialogue(1000)`): `id, timestamp (s), speaker, speakerFormID "0x%08X", text, questName, questEditorID, topicEditorID, topicFormID "0x%08X", sourcePlugin, subtypeName, topicSubtype, status, responseCount, skyrimNetBlockable, isScene, isBardSong, sceneEditorID, allResponses[], isActorBlocked, blockingFactionEditorID, isActorWhitelisted, whitelistFactionEditorID, isSubtypeFiltered, isSubtypeToggledOff`. `status` ∈ `Allowed, Soft Block, Hard Block, SkyrimNet Block, Filter, Toggled Off, Whitelist, Unknown`. |
| `SKSE_API.call('updateBlacklist', s)` | `SendBlacklistData` | array: `id, targetType (Topic/Quest/Subtype/Scene/Plugin/Actor/Faction/Unknown), blockType ("Soft Block"/"Hard Block"/"SkyrimNet Block"), topicFormID ("0x%08X" or null), topicEditorID (= targetEditorID), questEditorID, sourcePlugin, questName (= questEditorID), filterCategory, note, dateAdded, responseText, actorFilterFormIDs[], actorFilterNames[], factionFilterEditorIDs[]`. `topicFormID` and `actorFilterFormIDs` are **current-session** FormIDs, resolved from the rows' FormKeys (`FormKey::CurrentFormID`), so the menu never holds a FormID from an older load order. FormKeys themselves never cross into JS; `ResolveFormKeys()` recomputes them when an edit is saved. |
| `SKSE_API.call('updateWhitelist', s)` | `SendWhitelistData` | same as blacklist but `blockType:"Allowed"`. |
| `SKSE_API.call('updateSettings', s)` | `SendSettingsData` | `{blacklistEnabled, scenesEnabled, bardSongsEnabled, followerCommentaryEnabled, combatGruntsBlocked, subtypes:{"<id>":bool,...}}` — each is `global->value >= 0.5`; `subtypes` iterates `mcm.subtypeGlobals`. |
| `window.handleIdentifierDetection(obj)` | `OnDetectIdentifierType` | `{type:"topic"\|"scene"\|"plugin"\|"actor"\|"faction"\|"unknown", displayName, categories[]}` (empty identifier → `{type:"topic",categories:["Blacklist"]}`) |
| `window.handleNearbyActors(obj)` | `OnGetNearbyActors` | `{actors:[{name, formID, distance}]}` |
| `window.showToast(msg, type)` | `OnImportScenes`, `OnImportYAML`, `OnCreateAdvancedEntry` | messages with dynamic text go through `BuildToastScript(message, type)`; fixed messages are hand-written literals. `type` ∈ `success`/`error` |

### Identifier resolution (`detectIdentifierType` vs `createAdvancedEntry`)

These two handlers classify the same user input with **separate code**:

| Step | `OnDetectIdentifierType` | `OnCreateAdvancedEntry` |
|---|---|---|
| Plugin | `IsPluginFileName(identifier)` (file-local, case-insensitive `.esp/.esm/.esl`) | `IsPluginFileName(parsedEditorID)` (same helper) |
| FormID test | starts with `0x`, or all hex digits | `Config::ParseFormIdentifier(identifier)` returns non-zero FormID |
| FormID → type | `LookupByID`: Actor → Scene → Topic, else `unknown` | same order; Actor stores the actor **name** in `targetEditorID`; Scene/Topic also extract responses (`TopicResponseExtractor`) into `responseText` |
| EditorID → type | Faction (`LookupByEditorID<TESFaction>`) → Scene → Topic (`Config::SafeLookupForm`) | Faction → linear scan of `GetFormArray<BGSScene>` → `GetFormArray<TESTopic>` |
| Categories | Scene: Blacklist/Scene/BardSongs/FollowerCommentary; Actor/Faction: Blacklist; else Blacklist + a hard-coded list of subtype names (duplicated verbatim in `advanced-edit-modal.tsx` `TOPIC_CATEGORIES`) | — |

Block type in `OnCreateAdvancedEntry`: Actor/Faction targets are forced to Soft; `"SkyrimNet"`
is still accepted (legacy). An identifier that resolves to nothing (`targetType` still `None`)
is refused: the handler logs a warn, shows the error toast "No topic, scene, actor, faction or
plugin found for ...", and **saves nothing**. Actor filters must be index-paired: if
`actorFilterFormIDs.size() != actorFilterNames.size()` or any FormID fails to parse, the
handler logs `PAIRING ERROR` / `FORMID PARSING FAILED`, shows an error toast, and **saves nothing**.

## JSON helpers and escaping

Reading (JS → C++), from `src/PrismaUIMenuJson.h`:
- `FindJsonValue(json, key)` does a plain `find("\"key\":")` — first textual match anywhere, including inside string values or nested objects. Fine for the flat `JSON.stringify` objects the UI sends; don't send nested objects.
- `ExtractJsonValue` returns an unescaped string, or the raw token for numbers/bools, or `""` if absent. `ExtractJsonStringArray` reads `["a","b"]` only (string elements). `ParseHexFormIDs` accepts `0x`-prefixed or bare hex, skips empty/unparseable ones with a warn.
- `ReadJsonString` decodes `\uXXXX` to UTF-8 but not surrogate pairs (fine: `JSON.stringify` only emits `\u` for control chars and lone surrogates).
- Older handlers still hand-parse: `OnDeleteHistoryEntries` and `OnRemoveWhitelistBatch` (manual `[`/`]` scan), `OnRemoveFromWhitelist` (manual `"id":`), `OnToggleSubtypeFilter`, `SetToggleFromUI`. Prefer the helpers in new code.

Writing (C++ → JS):
- String fields in serializers go through `escapeJSON` (quotes, backslash, control chars → `\uXXXX`).
- `BuildSKSEUpdateScript` then wraps the whole JSON in a single-quoted JS string with `EscapeSingleQuotedJSString` (`'`, `\`, `\n`, `\r`, `\t`, and any other control char < 0x20 as `\xNN`). Both layers are required: JSON escaping for `JSON.parse`, JS escaping for the literal. `SendSettingsData` repeats the JS-escape loop inline instead of calling the helper.
- Toasts with dynamic text use `BuildToastScript(message, type)`, which runs the message through `EscapeSingleQuotedJSString`.
- The direct-call pushes (`handleIdentifierDetection`, `handleNearbyActors`) splice JSON in as a JS literal. `OnDetectIdentifierType` escapes `displayName` and `OnGetNearbyActors` escapes names with `escapeJSON`.
- Encoding: game strings are Windows-1252. PrismaUI's `Invoke` checks `isValidUTF8(script)` and, if the whole script isn't valid UTF-8, converts the whole script from ANSI. STFU does no conversion of its own.

## Log lines to grep

`STFU.log` (debug lines need `stfu_debug.flag`):

| Grep | Meaning |
|---|---|
| `[PrismaUIMenu] Failed to get PrismaUI API` | PrismaUI.dll not loaded — no menu. |
| `[PrismaUIMenu] Initialized successfully` / `DOM ready for view` | Startup OK. |
| `Registered menu hotkey handler` / `[HOTKEY] Menu hotkey` | Hotkey sink installed / fired (debug). |
| `[PrismaUIMenu::JS]` | Every `log()` line from the page, including `[SKSE_API] sendToSKSE called: <name>, data: ...` and `ERROR: <name> is not a function`. |
| `Sending N bytes of history data` | Every history push (info level — noisy). |
| `Not initialized or invalid view` | A `Send*Data` before `Initialize` finished or after the view died. |
| `[PrismaUIMenu::On<Handler>]` | Per-handler receive/parse/result lines; most log the raw payload at info. |
| `PAIRING ERROR` / `FORMID PARSING FAILED` | `createAdvancedEntry` rejected the actor filter arrays. |
| `did not resolve to a topic, scene, actor, faction or plugin - not saved` | `createAdvancedEntry` refused an unresolved identifier (warn). |

## Gotchas and invariants

- **The update handlers overwrite all three filter arrays with whatever the modal sends.** Any field a serializer leaves out of `updateBlacklist`/`updateWhitelist` is therefore wiped on the next Save. Before 1.2.0 the blacklist serializer omitted `factionFilterEditorIDs`, which erased blacklist faction filters on every edit. Keep the serializers and the update handlers in step.
- Every edit handler **replaces** all three filter arrays with the payload. A caller that omits them clears them.
- `OnUpdateBlacklistEntry` has `// TODO: Invalidate cache` — whether the blocking hooks see the edit immediately depends on the DB/Config caching (see [DATABASE.md](DATABASE.md), [BLOCKING_RULES.md](BLOCKING_RULES.md)).
- Blacklist deletes (`deleteBlacklistEntry/Batch`) push only `updateBlacklist`; the UI requests history itself 150 ms later. Whitelist removals push history too. Inconsistent but harmless.
- History statuses are **recomputed at serialization time** from the current blacklist/whitelist/MCM state (`SerializeHistoryToJSON`), not read from the logged row. That loop duplicates the matching rules of the real filter; if you change blocking semantics, update it too or the History tab will lie.
- `settings.tsx` "Enable/Disable All" sends one `toggleSubtypeFilter` per subtype, and each one re-serializes up to 1000 history rows and pushes settings + history. Slow on big histories; batch it if it becomes a problem.
- `SendHistoryData` flushes the DB write queue; `OnToggleSubtypeFilter`'s inline push doesn't.
- `OnDetectIdentifierType` treats any all-hex string (`"BAD"`, `"Dead"`) as a FormID.
- `OnImportYAML` builds the path from the game exe directory (`<exe>/Data/SKSE/...`); under MO2 this resolves through the VFS.
- Build any toast that carries dynamic text with `BuildToastScript`; hand-written `window.showToast('...')` literals are only safe for fixed text.
- `Focus(view_, true, false)` pauses the game while the menu is open.

## How to: add a new UI action end to end

Example: a "Clear history" button.

1. **Handler declaration** — `include/PrismaUIMenu.h`: `static void OnClearHistory(const char* data);`.
2. **Handler** — put it in the `PrismaUIMenu_<Area>.cpp` that owns the data (here `_History.cpp`). Skeleton:
   ```cpp
   void PrismaUIMenu::OnClearHistory(const char* data)
   {
       try {
           std::string json(data ? data : "");
           bool keepBlocked = ExtractJsonValue(json, "keepBlocked") == "true";
           auto* db = DialogueDB::GetDatabase();
           if (!db) { spdlog::error("[PrismaUIMenu::OnClearHistory] Database not available"); return; }
           // ... mutate ...
           SendHistoryData();   // push fresh state back
       } catch (const std::exception& e) {
           spdlog::error("[PrismaUIMenu::OnClearHistory] Exception: {}", e.what());
       }
   }
   ```
   You're on the main thread; no `AddTask` needed. Always wrap in try/catch (`std::stoll` etc. throw) and always push the affected data back — the UI treats C++ pushes as the source of truth.
3. **Register** — `src/PrismaUIMenu.cpp` `Initialize()`: `prismaUI_->RegisterJSListener(view_, "clearHistory", &OnClearHistory);`. The string is the global function name JS will call.
4. **JS wrapper** — `web-ui/src/lib/skse-api.ts`: `clearHistory: (keepBlocked: boolean) => SKSE_API.sendToSKSE('clearHistory', JSON.stringify({ keepBlocked })),`. Send flat JSON objects (or nothing).
5. **Call it** from a component; update the store only from the pushed `updateHistory`, not optimistically (the edit modal's optimistic `onSave` is the one exception and it re-requests 150 ms later).
6. If the handler needs to return something that isn't one of the four data sets, either add a new `SKSE_API.subscribe('<event>')` in `app.tsx` and push with `BuildSKSEUpdateScript("<event>", json)` (preferred — escaping is handled), or register a `window.handleXxx` in the component while mounted, as `actor-filter-picker.tsx` does.
7. Build the DLL **and** the web UI (`npm run build`, copy `dist/` to `PrismaUI/views/STFU/`) — see [WEB_UI.md](WEB_UI.md) and [DEVELOPMENT.md](DEVELOPMENT.md). Verify in `STFU.log` with `grep "sendToSKSE called: clearHistory"` then your handler's log line.

## Related docs

- [WEB_UI.md](WEB_UI.md) — the React app, stores, components.
- [DATABASE.md](DATABASE.md) — `DialogueDB` API used by the handlers (blacklist/whitelist upserts, history queue).
- [CONFIG_AND_SETTINGS.md](CONFIG_AND_SETTINGS.md) — the globals behind the settings toggles, INI persistence, menu hotkey.
- [BLOCKING_RULES.md](BLOCKING_RULES.md) — the real filter logic that `SerializeHistoryToJSON` mirrors.
- [SCENE_BLOCKING.md](SCENE_BLOCKING.md) — what `importScenes` feeds.
- [ARCHITECTURE.md](ARCHITECTURE.md), [INDEX.md](INDEX.md).
