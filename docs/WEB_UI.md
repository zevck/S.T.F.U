# Web UI (React menu in `web-ui/`)

The in-game STFU menu is a small React 18 + TypeScript single-page app, bundled by Vite,
styled with Tailwind, with state in zustand. PrismaUI renders it in an Ultralight webview
loaded from `PrismaUI/views/STFU/index.html`. It has four tabs — History, Blacklist,
Whitelist, Settings — and talks to the DLL only through PrismaUI's JS listener bridge. The
C++ half and the full message table are in [PRISMA_UI_BRIDGE.md](PRISMA_UI_BRIDGE.md).

## Overview

```
index.tsx ── SKSE_API.init() ── <App/>
                                  │  useEffect: SKSE_API.subscribe('updateHistory'|'updateBlacklist'|
                                  │             'updateWhitelist'|'updateSettings') → store.setX(JSON.parse(s))
                                  │  ESC → sendToSKSE('closeMenu'); tab switch → request<Tab>
                                  ├─ <History/>      components/history.tsx      ← useHistoryStore (+ blacklist/whitelist stores)
                                  ├─ <Blacklist/>    components/entry-list.tsx   ← useBlacklistStore  ┐ same EntryList,
                                  ├─ <Whitelist/>    components/entry-list.tsx   ← useWhitelistStore  ┘ different ListConfig
                                  ├─ <Settings/>     components/settings.tsx     ← useSettingsStore
                                  └─ <Toast/>        components/toast.tsx        ← useToastStore (window.showToast)
Modals: ManualEntryModal, AdvancedEditModal, ResponsesModal — all built on components/modal.tsx
        ManualEntryModal + AdvancedEditModal embed ActorFilterPicker
```

## Stack and build

| Item | Value |
|---|---|
| Framework | React 18.3, `react-dom/client` `createRoot`, `React.StrictMode` (`src/index.tsx`) |
| Language | TypeScript ~5.6, `strict`, `noUnusedLocals/Parameters` (`tsconfig.json`); `@/*` → `src/*` alias (tsconfig `paths` + `vite.config.ts` `resolve.alias`) |
| Bundler | Vite 6 + `@vitejs/plugin-react`; `base: './'` so asset URLs are relative (required: the page is loaded from a file path, not a server root); `outDir: dist`, `emptyOutDir: true` |
| State | zustand 5 |
| Styling | Tailwind 3.4 via PostCSS (`postcss.config.cjs`, `tailwind.config.cjs`), plus `src/index.css` |
| Icons | `lucide-react` |
| Declared but unused | `motion` (no imports in `src/`) |

Scripts (`package.json`): `dev` = `vite`; `build` = `tsc && vite build` (type errors fail the
build); `preview`; `lint` = `eslint .` and `format` = prettier. There is **no ESLint config
file in `web-ui/`**, so `npm run lint` is not expected to work as-is (not verified).

Deploy (per `web-ui/README.md`; nothing automates it):

```
cd web-ui
npm install          # once
npm run build        # → web-ui/dist/index.html + dist/assets/index-<hash>.{js,css}
# copy dist/* into PrismaUI/views/STFU/  (the DLL loads "STFU/index.html")
```

- `web-ui/dist/` and `PrismaUI/views/STFU/assets/` are git-ignored, but `PrismaUI/views/STFU/index.html` **is tracked**. Every build changes the hashed asset names in that `index.html`, so a clean checkout has an `index.html` pointing at assets that don't exist until you build and copy.
- Copying over the old folder leaves previous `index-<hash>.js/css` files behind in `PrismaUI/views/STFU/assets/` (three JS bundles are there as of 2026-09-26). Harmless (only the referenced ones load), but clear the folder when deploying if you care.
- The game must be restarted (or at least the view reloaded) to pick up a new bundle; the view is created once at `kDataLoaded`.

### Ultralight / PrismaUI constraints observed in the code

The web README says nothing about Ultralight; these are the constraints the code itself works around:
- ESC arrives with `e.key === "Unidentified"` → handlers check `e.keyCode === 27` (`app.tsx`, `modal.tsx`).
- `user-select: none` globally, re-enabled only for `input/textarea/[contenteditable]` (`index.css`), to stop drag-selecting the whole UI.
- `select` dropdowns are forced dark with `color-scheme: dark` + `!important` backgrounds because native popups otherwise render light.
- The C++ side sets scroll speed to 100 px per wheel step (`SetScrollingPixelSize`).
- No network/CDN: everything must be in the bundle (fonts are the system `Consolas`/`Courier New` stack).
- `IntersectionObserver` (lazy lists) and `localStorage` (fullscreen preference key `stfu-fullscreen`) are used and evidently work in the webview.

## App structure and tabs

`src/app.tsx` `App`:
- On mount: `SKSE_API.init()` (also called in `index.tsx`; harmless double init — the listener list is module-level), four `subscribe` calls, logs whether a few listener globals exist, requests all four data sets, installs the ESC → `closeMenu` keydown handler.
- Tab switch (`activeTab` effect) re-requests that tab's data (`requestHistory` / `requestBlacklist` / `requestWhitelist` / `requestSettings`).
- **Fullscreen vs windowed**: toggles `#root` class `fullscreen`/`windowed` (CSS in `index.css`: windowed is a 2000×1200 centered box, max 95vw/95vh). Windowed mode shows a drag header; position is kept in component state only (resets on reload). Preference persists in `localStorage['stfu-fullscreen']` (default fullscreen).

| Tab | Component | What it shows / does |
|---|---|---|
| History | `components/history.tsx` `History` | Last ≤1000 dialogue rows, oldest at top, auto-scrolls to bottom when `entries.length` changes. Search (text/speaker/questName/topicEditorID) + status checkboxes. Detail panel: status, quest, topic/scene, plugin, subtype, responses; Add/Edit Blacklist and Whitelist buttons (matching entry found by topic FormID/EditorID or scene EditorID); remove buttons for actor/faction list entries affecting the speaker; "Toggle <subtype> Filter" when `status` is `Filter`/`Toggled Off`. DEL deletes the selected history rows (container is `tabIndex=0`; clicking a row focuses it). |
| Blacklist / Whitelist | `components/entry-list.tsx` `Blacklist` / `Whitelist` | Shared `EntryList` (see below). |
| Settings | `components/settings.tsx` `Settings` | Sub-tabs Master Controls / Combat / Generic / Follower / Other. Master: blacklist/scenes/bard-song globals + Import Scenes / Import from YAML. Others: per-subtype toggles (hard-coded `{id, name, tooltip}` tables `COMBAT_SUBTYPES`, `GENERIC_SUBTYPES`, `FOLLOWER_SUBTYPES`, `OTHER_SUBTYPES`), combat grunts, follower commentary, Enable/Disable All per panel. |

History status mapping (`getStatusDisplay`): `Toggled Off`/`Skyrim`/`Whitelist` → "Allowed",
`Filter`/`Soft Block` → "Soft Blocked", `Hard Block` → "Hard Blocked", `SkyrimNet Block` →
"SkyrimNet Blocked". The status checkboxes filter on those display strings.

## Stores (`src/stores/`)

| Store | File | State |
|---|---|---|
| `useHistoryStore` | `history.ts` | `entries: DialogueEntry[]`, `searchQuery`, `selectedEntries`; setters. `clearEntries` and `updateEntryStatus` exist but have no callers. |
| `useBlacklistStore` / `useWhitelistStore` | `blacklist.ts` / `whitelist.ts` | Each is `createEntryListStore()` — two independent instances of the same shape. |
| `createEntryListStore` / `EntryListState` | `entry-list.ts` | `entries: BlacklistEntry[]`, `searchQuery`, `blockSoft`, `blockHard`, `showTopics/Scenes/Actors/Factions`, `selectedEntries`; setters; `resetFilters()` restores `DEFAULT_FILTERS`. Filter state lives in the store, so it survives tab switches. |
| `useSettingsStore` | `settings.ts` | `blacklistEnabled, scenesEnabled, bardSongsEnabled, followerCommentaryEnabled, combatGruntsBlocked, subtypes: Record<number, boolean>`; `setSettings(partial)` merges. Defaults are placeholders until C++ pushes. |
| `useToastStore` | `toast.ts` | `toasts[]`, `addToast`, `removeToast`. **Module side effect**: assigns `window.showToast` at import time. |

Rule of thumb: stores hold what C++ last pushed. Components don't mutate entries
optimistically, except `EntryList`'s `AdvancedEditModal onSave` (patches the edited entry in
place, then re-requests the list 150 ms later).

## Shared components and hooks

| Piece | File | Notes |
|---|---|---|
| `Modal`, `CloseIcon` | `components/modal.tsx` | Backdrop (click outside closes), ESC closes via a **capture-phase** window listener with `stopImmediatePropagation` — so ESC inside a modal closes the modal, not the whole menu. Returns `null` when closed, so children (and their effects) unmount. `sizeClassName` sets width/height. |
| `ResponsesModal` | `components/responses-modal.tsx` | Read-only list of response strings. |
| `ManualEntryModal` | `components/manual-entry-modal.tsx` | Create blacklist/whitelist entry. Debounced (300 ms) `detectIdentifierType`; receives `window.handleIdentifierDetection` only while open; 2 s fallback marks type `unknown`. Optional prefill from a history row, with a "block/whitelist speaker instead" checkbox that swaps the identifier to the speaker FormID. Sends `createAdvancedEntry` (whitelist forces `category: 'Whitelist'`). Refuses to send if actor name/FormID arrays differ in length. |
| `AdvancedEditModal` | `components/advanced-edit-modal.tsx` | Edit an existing entry. Decides whitelist vs blacklist by `entry.filterCategory === 'Whitelist'` (the DB forces that category on whitelist rows). Blacklist: block type Soft/Hard + filter category (`TOPIC_CATEGORIES` or scene categories) + filters + notes → `updateBlacklistEntryAdvanced`. Whitelist: filters + notes → `updateWhitelistEntryAdvanced`. Block type/category hidden for whitelist and for Actor/Faction targets; filter picker hidden for Actor/Faction targets. Also has Delete. |
| `ActorFilterPicker`, `ActorFilters` | `components/actor-filter-picker.tsx` | Chips + input + dropdown. On mount registers `window.handleNearbyActors` and sends `getNearbyActors`; merges nearby actors with speakers heard in the last 30 min of history (dedup by FormID, drops `0x00000000`). Picking an actor appends to `actorFilterNames` and `actorFilterFormIDs` **in lockstep** (index-aligned — C++ rejects mismatches). Free text that isn't a known actor name becomes a **faction EditorID** filter. |
| `EntryList` + `ListConfig` | `components/entry-list.tsx` | See next section. |
| `Toast` | `components/toast.tsx` | Bottom-right stack, auto-dismiss 4 s. The `animate-in slide-in-from-right` classes do nothing (no `tailwindcss-animate` plugin). |
| `useLazyList(items, resetKey)` | `lib/list-hooks.ts` | Renders `PAGE_SIZE = 100` items, adds 100 more when the sentinel `<div ref={sentinelRef}>` intersects (200 px margin). Resets to the first page when `resetKey` changes — pass a string derived from the active filters. |
| `useMultiSelect(items, selected, setSelected)` | `lib/list-hooks.ts` | Click = select one; Ctrl/Cmd = toggle; Shift = range from the last clicked index **within `items`** (so pass the filtered list). `clearAnchor()` after deletes. |

### `EntryList` and `ListConfig`

`EntryList` renders both list tabs; the differences are in a `ListConfig`:

```ts
interface ListConfig {
  name: string;          // "blacklist" / "whitelist" for UI text
  isWhitelist: boolean;  // hides Soft/Hard filter + badge becomes "Allowed"
  headingClass: string;  // detail panel heading colour
  useStore: UseBoundStore<StoreApi<EntryListState>>;
  deleteEntries: (ids: number[]) => void;  // single → delete/remove, many → *Batch
}
```

`BLACKLIST` and `WHITELIST` constants are the two instances. The component: filters `entries`
by search (targetType, blockType, quest, topic EditorID/FormID, plugin, note, actor names,
faction EditorIDs), by block type (blacklist only; `SkyrimNet Block` rows always pass) and
by target type; lazy-renders with `useLazyList`; handles selection with `useMultiSelect`;
DEL deletes the selection; detail panel for one entry (type, topic/scene, quest, plugin,
responses parsed from `responseText` JSON, Edit) or a summary + bulk delete for many; hosts
`ResponsesModal`, `ManualEntryModal`, `AdvancedEditModal`. After deletes it requests a history
refresh after 150 ms. An effect re-points `selectedEntries` at the fresh objects whenever
`entries` changes.

## How data arrives from C++

All inbound data is C++ calling JavaScript through PrismaUI `Invoke`:

| Global | Defined in | Called by C++ with |
|---|---|---|
| `window.SKSE_API.call(event, jsonString)` | `lib/skse-api.ts` `SKSE_API.init()`; dispatches to `subscribe`d callbacks (one per event name — duplicates are skipped) | events `updateHistory`, `updateBlacklist`, `updateWhitelist`, `updateSettings` (subscribed in `app.tsx`) |
| `window.handleIdentifierDetection(obj)` | `manual-entry-modal.tsx`, only while the modal is open | `detectIdentifierType` reply |
| `window.handleNearbyActors(obj)` | `actor-filter-picker.tsx`, while mounted | `getNearbyActors` reply |
| `window.showToast(message, type)` | `stores/toast.ts` at module load | `importScenes` / `importYAML` results |

Outbound, the page calls globals that PrismaUI binds for each C++ `RegisterJSListener`
(`window.requestHistory`, `window.closeMenu`, …) — always through
`SKSE_API.sendToSKSE(name, data?)`, which checks `typeof window[name] === 'function'` and logs
an error line instead of throwing if it isn't. `log(msg)` sends to `jsLog` (→ `STFU.log` as
`[PrismaUIMenu::JS] ...`). If `jsLog` doesn't exist, `log` does **nothing** (the
`console.log` fallback only runs if the call throws), so in a plain browser you see no logs.

Global types: `src/global.d.ts` declares only `window.SKSE_API`; the other globals are
accessed through `(window as any)`.

## Types (`src/types.ts`)

- `DialogueEntry` — one history row, mirroring `SerializeHistoryToJSON`. `status` union includes `'Skyrim'`, which C++ never sends (C++ can send `'Unknown'`, which the union lacks). FormIDs are `"0x%08X"` strings.
- `BlacklistEntry` — used for **both** lists, mirroring `SerializeBlacklistToJSON`/`SerializeWhitelistToJSON`. Naming is historical: `topicEditorID`/`topicFormID` hold the target's EditorID/FormID for every target type (scene, actor, faction…), `note` (singular) is the notes field, `questName` is actually the quest EditorID. `allResponses` is declared but C++ never sends it for list entries (responses come in `responseText`, a JSON array string).
- `ActorFilters` (actor-filter-picker.tsx) — the three filter arrays.

## Styling

Tailwind utility classes inline everywhere; dark palette (`bg-gray-900/800/700`), custom
`gray-750`/`gray-850` in `tailwind.config.cjs`. Colour conventions: blacklist red, whitelist
green/white, settings purple, history statuses green/orange/red/yellow/cyan/indigo
(`getStatusColor`). `index.css` holds the global font (Consolas monospace), root sizing,
windowed-mode frame, dark scrollbars, dark `select`, and the user-select rules. No CSS modules,
no component library.

## Developing and iterating

- **Browser dev mode exists but has no mocks.** `npm run dev` serves on `http://localhost:5173`; the app renders empty because none of the listener globals exist (`sendToSKSE` logs "not a function" into a `log` that goes nowhere). You can feed data by hand from DevTools:
  ```js
  window.SKSE_API.call('updateHistory', JSON.stringify([{ id: 1, timestamp: Date.now()/1000, speaker: 'Lydia', speakerFormID: '0x000A2C94', text: 'I am sworn to carry your burdens.', questName: '', questEditorID: '', topicEditorID: 'Foo', topicFormID: '0x00012345', sourcePlugin: 'Skyrim.esm', subtypeName: 'Idle', topicSubtype: 94, status: 'Allowed', responseCount: 1, skyrimNetBlockable: false, isScene: false, isBardSong: false, sceneEditorID: '' }]))
  window.SKSE_API.call('updateSettings', JSON.stringify({ blacklistEnabled: true, subtypes: { 94: true } }))
  ```
  Chrome is not Ultralight — check key handling, selects and layout in game before trusting them.
- **In game**: build, copy `dist/*` to `PrismaUI/views/STFU/`, restart the game (under MO2 the mod folder is the source; the build doesn't deploy itself). Debug with `STFU.log` — every `log()` call and every `sendToSKSE` lands there as `[PrismaUIMenu::JS]`. PrismaUI's API has `CreateInspectorView`, but STFU doesn't call it.
- `React.StrictMode` double-runs effects in dev builds only; the subscribe/unsubscribe pairs in `app.tsx` are written to tolerate that.

## Gotchas

- **The edit modal saves back every filter field it was given.** If a list payload is missing a field, `AdvancedEditModal` loads `[]` and Save writes `[]` over the stored value. That is how blacklist faction filters were being erased before 1.2.0 (see [PRISMA_UI_BRIDGE.md](PRISMA_UI_BRIDGE.md)).
- **History "Whitelisted" checkbox is dead.** `getStatusDisplay('Whitelist')` returns `"Allowed"`, so whitelisted rows are controlled by the Allowed checkbox and the Whitelisted checkbox matches nothing. Rows with status `SkyrimNet Block` map to `"SkyrimNet Blocked"`, which has no checkbox, so legacy SkyrimNet-blocked rows are never shown.
- The subtype ID → name tables in `settings.tsx`, `TOPIC_CATEGORIES` in `advanced-edit-modal.tsx`, and the category list in C++ `OnDetectIdentifierType` are three hand-maintained copies. Keep them in sync with [DIALOGUE_SUBTYPES.md](../DIALOGUE_SUBTYPES.md) / `Config`.
- Settings toggles are **not** optimistic: the checkbox only flips when C++ pushes `updateSettings`. A toggle whose global is missing never flips (C++ logs `Toggle global not found`).
- "Enable/Disable All" fires one `toggleSubtypeFilter` per subtype that differs; each triggers a full history re-serialization on the C++ side.
- Toast timers reset for every visible toast whenever a new toast is added (the effect depends on the whole `toasts` array).
- Multiple `ManualEntryModal`/`AdvancedEditModal` instances are mounted at once (History has two of each, each list tab has one). The `window.handle*` callbacks are single globals; this works only because at most one modal is open at a time.

## How to: add a settings toggle to the UI

Example: a new global "Block Guard Comments".

1. **C++** (see [PRISMA_UI_BRIDGE.md](PRISMA_UI_BRIDGE.md) and [CONFIG_AND_SETTINGS.md](CONFIG_AND_SETTINGS.md)): add the `TESGlobal*` to `Config` settings and INI persistence; in `src/PrismaUIMenu_Settings.cpp` add `OnSetGuardCommentsEnabled` that calls `SetToggleFromUI("OnSetGuardCommentsEnabled", data, "enabled", <global>)`; declare it in `include/PrismaUIMenu.h`; register `"setGuardCommentsEnabled"` in `Initialize()`; emit `"guardCommentsEnabled":true|false` in `SendSettingsData()`.
2. **Store** — `src/stores/settings.ts`: add `guardCommentsEnabled: boolean` to `SettingsState` and a default. `setSettings` merges automatically.
3. **API** — `src/lib/skse-api.ts`: `setGuardCommentsEnabled: (enabled: boolean) => SKSE_API.sendToSKSE('setGuardCommentsEnabled', JSON.stringify({ enabled })),`.
4. **UI** — `src/components/settings.tsx`: `const guardCommentsEnabled = useSettingsStore(s => s.guardCommentsEnabled);` and a `<Toggle label=... checked={guardCommentsEnabled} onChange={(c) => SKSE_API.setGuardCommentsEnabled(c)} tooltip=... />` in the right panel. If it belongs to a panel with Enable/Disable All, extend that panel's `setAll*` callback like `setAllCombat` does for grunts.
5. For a new **subtype** toggle instead, you only need a `{ id, name, tooltip }` row in the right `*_SUBTYPES` table — `toggleSubtypeFilter` and `subtypes[id]` already cover it, provided C++ has a global for that subtype in `mcm.subtypeGlobals`.
6. `npm run build`, copy, restart; confirm `Sending settings data: {... "guardCommentsEnabled":...}` in `STFU.log`.

## How to: add a column/field to the entry list

Example: show `dateAdded` on each Blacklist/Whitelist row.

1. **Is it in the payload?** Check `SerializeBlacklistToJSON` / `SerializeWhitelistToJSON` (`src/PrismaUIMenu_Blacklist.cpp`, `_Whitelist.cpp`). `dateAdded` already is. For a new field, add it to **both** serializers (the lists share one TS type), escaping strings with `escapeJSON`.
2. **Type** — add the optional field to `BlacklistEntry` in `src/types.ts`.
3. **Row** — render it in `EntryItem` in `components/entry-list.tsx` (e.g. under the label: `{entry.dateAdded ? new Date(entry.dateAdded * 1000).toLocaleDateString() : null}` — timestamps are seconds). If only one list should show it, pass a flag through `ListConfig` rather than branching on `name`.
4. **Detail panel** — add a labelled block in the single-selection branch of `EntryList`.
5. **Search** (optional) — add it to the `filteredEntries` predicate. **Filter checkbox** (optional) — add the boolean + setter to `EntryListState`/`DEFAULT_FILTERS` in `stores/entry-list.ts`, a `FilterCheckbox` in the filter row, the condition in `filteredEntries`, and the flag to the `useLazyList` reset key.
6. **Editable?** Then it must also go through `AdvancedEditModal` → the update payload → `OnUpdateBlacklistEntry`/`OnUpdateWhitelistEntryAdvanced` → DB. Remember those handlers overwrite every field they read, so the serializer **must** emit any field the modal sends back (the faction-filter bug above is exactly this mistake).

## Related docs

- [PRISMA_UI_BRIDGE.md](PRISMA_UI_BRIDGE.md) — listener table, push payloads, threading, escaping.
- [CONFIG_AND_SETTINGS.md](CONFIG_AND_SETTINGS.md) — globals behind the Settings tab.
- [DATABASE.md](DATABASE.md) — what the lists and history are stored as.
- [DEVELOPMENT.md](DEVELOPMENT.md) — build/deploy workflow for DLL + UI.
- [DIALOGUE_SUBTYPES.md](../DIALOGUE_SUBTYPES.md) — subtype IDs used in `settings.tsx`.
- [INDEX.md](INDEX.md), [ARCHITECTURE.md](ARCHITECTURE.md).
