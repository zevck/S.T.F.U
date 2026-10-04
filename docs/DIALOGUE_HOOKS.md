# Dialogue Hooks (runtime interception)

This document covers the layer that intercepts NPC dialogue while the game is running:
the `DialogueItem::Ctor` detour, the `PopulateTopicInfo` detour, and the two call-site
hooks inside PopulateTopicInfo (`SetSubtitle`, `ConstructResponse`). It explains what
each hook does, the order they fire in, what "soft block" and "hard block" mean
mechanically here, the state the hooks share, and which log lines to grep for.

How the block *decision* is computed (blacklist/whitelist matching, subtype toggles,
actor/faction filters, filter categories) is in [BLOCKING_RULES.md](BLOCKING_RULES.md).
Scene prevention by condition patching is in [SCENE_BLOCKING.md](SCENE_BLOCKING.md).
This doc only covers where the decision is asked for, and what the hooks do with the answer.

> **Fragile path.** Everything here runs on engine worker threads, inside a tight
> call sequence that DDR and SkyrimNet also hook. Read [Before you change this](#before-you-change-this)
> before editing anything.

## Overview

```
                  DialogueItem::Ctor  (Detours, main.cpp)
                         │  records (speaker, topicInfo, topic, t) ──► g_recentConstructs
                         ▼
   ┌───────────── PopulateTopicInfo  (Detours, PopulateTopicInfoHook.cpp) ─────────────┐
   │ 1. extract text        2. duplicate check ──dup──► reuse cached soft decision,   │
   │                                                     call original, return        │
   │ 3. ClearBlockingDecision()                                                       │
   │ 4. HARD-block check (DB blacklist, Topic/Quest, Hard) ──hit──► log, return 0      │
   │ 5. soft decision: Config::ShouldSoftBlock ─► SetBlockingDecision(),              │
   │                   if soft: responseText = "", SetEarlyBlockFlag(true)            │
   │ 6. call ORIGINAL ────────────────────────────────────────────────┐              │
   │        │  original body:  +0x61 call SetSubtitle     ──► Hook_SetSubtitle        │
   │        │                  +0xDE call ConstructResponse ──► Hook_ConstructResponse│
   │        ◄────────────────────────────────────────────────────────┘              │
   │ 7. history logging (correlation, cooldown, burst filter, text dedup, distance)  │
   │ 8. scene safety-net check                                                         │
   └───────────────────────────────────────────────────────────────────────────────────┘
```

The call-site offsets are *inside* PopulateTopicInfo (ID 34429/35249), so SetSubtitle and
ConstructResponse only ever run nested inside step 6, on the same thread. SetSubtitle's call
site (+0x61) comes before ConstructResponse's (+0xDE); that is why PopulateTopicInfo has to
set the subtitle flag before it calls the original (see [Shared state](#shared-state-and-thread-safety)).

## Hook table

| Hook | ID (SE / AE / VR) | Mechanism | File / function |
|------|-------------------|-----------|-----------------|
| `DialogueItem::Ctor` | 34413 / 35220 / VR `0x572FD0` | Microsoft Detours (`DetourAttach`) | `src/main.cpp` `DialogueItemCtorHook::Hook_DialogueItemCtor`, `Install()` |
| `PopulateTopicInfo` | 34429 / 35249 / VR `0x573B70` | Microsoft Detours | `src/PopulateTopicInfoHook.cpp` `Hook_PopulateTopicInfo`, `Install()` |
| `SetSubtitle` call | call site at PopulateTopicInfo `+0x61` (same on SE/AE; VR uses the SE value) | `SKSE::Trampoline::write_call<5>`, guarded by an E8 check | `src/ConstructResponseHook.cpp` `Hook_SetSubtitle`, `Install()` |
| `ConstructResponse` call | call site at PopulateTopicInfo `+0xDE` (same on SE/AE; VR uses the SE value) | `write_call<5>`, E8 guard | `src/ConstructResponseHook.cpp` `Hook_ConstructResponse`, `Install()` |
| `GetResponseList` (called, not hooked) | 25083 / 25626 / VR `0x3A3000` | `REL::Relocation` call | `src/TopicResponseExtractor.cpp` `GetResponseList()` |

Install order (`src/main.cpp` `MessageHandler`, `kDataLoaded`): trampoline allocated
(256 bytes: SKSE branch pool if there is a `TrampolineInterface`, otherwise `trampoline.create`)
→ `PopulateTopicInfoHook::Install()` → `SceneHook::Install()` → `ConstructResponseHook::Install()`
→ `DialogueItemCtorHook::Install()`. The Detours hook on PopulateTopicInfo only rewrites the
function's prologue, so the `write_call` patches at +0x61/+0xDE are left alone.
Only runtime 1.6.1170 has been tested in game.

### The E8 guard (`ConstructResponseHook::Install`)

The +0x61/+0xDE offsets are hardcoded. Before patching, the `isCall` lambda checks that the
byte at the site is `0xE8` (rel32 CALL):

- If it is, it logs `ConstructResponseHook: <name> call site 0x<rva> calls 0x<target rva>` and
  installs the hook. The guard can't tell whether it's the *right* call, so the logged target RVA
  is there to compare against a known-good runtime when porting to a new game version.
- If it isn't, it logs an error `no CALL at the <name> call site ... - hook skipped` with the runtime
  version, and that hook is **not installed**. PopulateTopicInfo still runs, so soft blocking
  degrades to "response text cleared only" (no audio wipe / subtitle blanking).

Another plugin that already `write_call`ed the same site (DDR) still leaves an E8 there, so the
guard passes and STFU chains through it. Whichever plugin writes last is outermost. Per
the DDR notes, STFU installs at kDataLoaded and ends up outermost, so it runs first.

## Per-hook walkthrough

### DialogueItem::Ctor (`src/main.cpp` `DialogueItemCtorHook`)

Logs `[CTOR ENTRY]` (debug), then calls `PopulateTopicInfoHook::RecordDialogueConstruct(speaker,
topicInfo, topic)`, which pushes a timestamped `DialogueConstruct` onto `g_recentConstructs`
under `g_constructMutex`, and then calls the original. It **never blocks** anything. It only
provides the correlation signal for history logging.

A former SkyrimNet caller-identity gate (`_ReturnAddress()`) was removed. SkyrimNet
MinHooks the same ctor, so every real call arrived "from SkyrimNet.dll" and history got lost.
The comment in `main.cpp` explains how filtering could come back (a SkyrimNet export).

Inferred, not verified: the Ctor records *before* it calls the original, and the correlation
window is 50 ms. Both fit the Ctor calling PopulateTopicInfo once per response in the
TopicInfo's response chain.

### PopulateTopicInfo (`src/PopulateTopicInfoHook.cpp` `Hook_PopulateTopicInfo`)

Signature: `(int64 a_1, TESTopic*, TESTopicInfo*, Character* speaker, TESResponse* a_responseData)`.
It's one ~790-line function, left as-is on purpose until the rewrite. Its phases:

| # | Phase | What it does | Lines (approx.) |
|---|-------|--------------|-----------------|
| 1 | **Extraction** | `TopicResponseExtractor::ExtractResponsesFromTopicInfo(topicInfo)`, fragments joined with spaces → `fullResponseText`. Read from the static record, so it's available even after the text gets blanked. `[POPULATE EXTRACTED]`. | ~116–131 |
| 2 | **Duplicate detection** | `ConstructResponseHook::IsDuplicateDialogue(topicInfoFormID, speaker)`: same TopicInfo + same speaker pointer within 5 s of the *last non-duplicate* call. On a duplicate, if `GetCachedHardBlock()` is true it **returns 0 without calling the original** (a duplicate of a hard-blocked line stays blocked). Otherwise, if `GetCachedSoftBlock()` is true it blanks `a_responseData->responseText` and sets the early flag, then **calls the original and returns**. All later phases are skipped. | ~135–159 |
| 3 | **Reset** | `ClearBlockingDecision()`: clears `g_shouldSoftBlock`, `g_hardBlocked`, `g_wasEvaluated`, `g_evaluatedTopicInfoFormID`, `g_shouldBlockCurrent`. | ~166 |
| 4 | **Hard-block check** | Needs topic + speaker. It calls `db->GetBlacklist()` and does its own matching: Topic entries with `BlockType::Hard` (FormKey, then for pre-1.2.0 rows the ESL-safe quest EditorID + plugin + `formID & 0xFFF`, then topic EditorID, then full FormID for keyless rows), then Quest entries (`EntryMatchesTarget`). See BLOCKING_RULES.md "Target identity". On a hit it first calls `ConstructResponseHook::SetHardBlockDecision(topicInfoFormID)` (so phase 2 blocks later duplicates), then logs to history as `HardBlock` (skipped if the text is empty, if `ShouldFilterFromHistory` matches, or if the speaker is >5000 units away) and **returns 0 without calling the original**. | ~160–338 |
| 5 | **Soft decision** | Needs topic + `a_responseData` + non-empty text. `Config::ShouldSoftBlock(quest, topic, speakerName, text, speakerFormID, speaker)` → `SetBlockingDecision(soft, topicInfoFormID)`. If soft: `a_responseData->responseText = ""` and `SetEarlyBlockFlag(true)`. | ~340–360 |
| 6 | **Original** | `_OriginalPopulateTopicInfo(...)`, which runs the SetSubtitle and ConstructResponse call sites. | ~363 |
| 7 | **History logging** | See [History logging filters](#history-logging-filters). Ends in `DialogueDB::LogDialogue(entry)` and `EnrichBlacklistEntryAtRuntime` (Scene or Topic target). | ~366–762 |
| 8 | **Scene safety net** | For subtype 14 it finds the parent scene. If that scene is a Hard DB entry whose category gate global is ≥0.5, or it's a bard song with the toggle on, it logs `[POPULATE SCENE SAFETY]` at error level (the scene "started despite conditions"). It takes no action. | ~776–865 |

#### History logging filters

Phase 7 runs after the original and only when topic and speaker are non-null. These gates
are applied in order, and each one returns early without logging:

1. Trimmed text is empty (`[POPULATE] Skipping database log...`, trace).
2. Speaker is the player (`0x14`).
3. `Config::ShouldFilterFromHistory(text, subtype, topic)`: grunts/breathing (`[POPULATE FILTERED]`, trace).
4. **Construct correlation**: under `g_constructMutex`, it drops `g_recentConstructs` entries
   older than 1 s and looks for one with the same speaker + TopicInfo that is younger than
   **50 ms**. A match is erased (single use). No match → `[POPULATE] NO MATCH - Skipping logging`.
5. **(speaker, TopicInfo) cooldown**: 5 s in `g_recentlyLogged` (`[COOLDOWN SKIP]`). A
   periodic sweep every 30 s drops keys older than 2 min.
6. Decides the logged status: `ShouldSoftBlock` is **re-evaluated** here (not read from the cache).
   Subtype-14 hardcoded ambient scenes with scene blocking off are forced to `Normal`.
   Otherwise `SoftBlock` if blocked, else `Normal`.
7. Builds the entry. For scenes it walks `quest->scenes` to find the scene EditorID, then
   extracts all responses (scene → topic EditorID → topic FormID → fallback: the current text).
8. `skyrimNetBlockable` = the TopicInfo is in `MenuTopicManager::dialogueList`. This also
   serves as the "chosen line" marker for the next gate.
9. **Candidate-burst filter**: if another construct for the *same parent Topic* but a
   *different TopicInfo* happened within **100 ms**, and this line isn't in `dialogueList`, it's
   treated as a rejected candidate (follower Wait/Follow/Trade variants) and skipped.
10. **Text dedup**: same speaker + identical text within 5 s (`[POPULATE] TEXT DUPLICATE SKIP`).
    This map is swept on every call (entries older than 10 s).
11. Distance >5000 units from the player → skipped.

Hard-blocked lines skip all of this. They're logged in phase 4 with no correlation,
cooldown, or dedup.

### SetSubtitle (`Hook_SetSubtitle`)

`char* SetSubtitle(DialogueResponse*, char* text, int32)`. If `g_shouldBlockCurrent` is set, it
forwards a static empty string in place of `text`. Otherwise it passes the text through. It
logs `[SetSubtitle]` on every call.

### ConstructResponse (`Hook_ConstructResponse`)

`bool ConstructResponse(TESResponse*, char* filePath, BGSVoiceType*, TESTopic*, TESTopicInfo*)`.
The original fills `filePath` with the voice file path.

1. `[CONSTRUCT ENTRY]` (debug).
2. **Decision**: if `g_wasEvaluated`, it uses the cached `g_shouldSoftBlock`. If the cached
   decision was made for a different TopicInfo, it logs `[STALE CACHE]` (warn) but still uses it.
   If nothing is cached (phase 5 didn't run, e.g. the text was empty), it falls back to
   `Config::ShouldSoftBlock(quest, topic, nullptr, nullptr)`. There is **no actor context** in
   that case, so entries with actor/faction filters won't match.
3. It sets `g_shouldBlockCurrent` from the decision *before* calling the original.
4. **Bard songs** (subtype 14, `IsBardSongQuest` and the bard toggle on): it finds the scene
   holding this topic, sets `scene->isPlaying = false`, and returns `false` **without calling the
   original**. This is the only place that hard-stops a running scene. The code comments say
   doing this to other scenes freezes NPCs.
5. It calls the original. If the call failed or the pointers are null, it returns.
6. **Soft block, scene line (subtype 14)**: it blanks `responseText` on the whole chain and
   `strcpy`s `Sound\STFU\silent.fuz` into `filePath`. The engine then plays roughly 0.1 s of
   silence, the audio-complete callback fires, and the scene advances. An empty path or a
   `false` return hangs the scene's state machine.
7. **Soft block, regular line**: it logs `[AUDIO CLEARED]` (debug) and sets `*filePath = '\0'`.
   On every response in the chain it clears `responseText`, `speakerIdle`/`listenerIdle`,
   sets emotion to neutral/0, and `flags = kNone`. That removes audio, subtitle text, lip/idle
   animation, and head-turn jerk.

## Soft block vs hard block at this layer

| | Soft block | Hard block (Topic/Quest DB entry, `BlockType::Hard`) |
|---|---|---|
| Decided where | `Config::ShouldSoftBlock` (phase 5), cached for ConstructResponse | Inline matching in phase 4 of `Hook_PopulateTopicInfo` |
| Original PopulateTopicInfo | **Runs** | **Skipped** (returns 0) |
| SetSubtitle / ConstructResponse | Run. Subtitle forwarded as `""`, audio path wiped (or silent.fuz for scenes), idles/emotion cleared | Never reached |
| TopicInfo scripts / fragments, follower commands | Still run (the dialogue item still gets built; this is the point of soft block) | Not verified at this layer. The code comment says "no turning, no animations, nothing". What the caller does with a 0 return is unclear |
| History status | `SoftBlock` (re-evaluated in phase 7) | `HardBlock`, logged before returning |
| Scenes | DB-listed scenes → silent.fuz. Bard songs → `isPlaying=false` in ConstructResponse | Scene hard blocks happen **upstream** by condition patching (SceneHook). Here there's only the `[POPULATE SCENE SAFETY]` error log |

Hard-block matching in phase 4 is its own copy of the logic. It doesn't go through `Config`
or `DB::ShouldSoftBlock`, so it ignores filter-category toggles, actor/faction filters, and the
whitelist (see Known issues). In `DB::ShouldSoftBlock`, Hard entries also return "block"
(`DialogueDatabase_Query.cpp`), so a Hard entry that phase 4 misses still gets soft-blocked.

## Order of calls

### A normal spoken line (ambient/combat bark, or a line after a menu choice)

```
DialogueItem::Ctor            [CTOR ENTRY]            -> RecordDialogueConstruct
  PopulateTopicInfo           [POPULATE EXTRACTED]    [POPULATE] New dialogue detected
                              [ConstructResponse] Blocking decision cleared / set
    (original)
      SetSubtitle  (+0x61)    [SetSubtitle] ... g_shouldBlockCurrent=<soft>
      ConstructResponse(+0xDE)[CONSTRUCT ENTRY]  [ConstructResponse] Using cached decision
                              ([SOFT BLOCK] + [AUDIO CLEARED] if soft)
  (back in PopulateTopicInfo) [POPULATE] MATCH FOUND!  [DIALOGUE] ...  -> LogDialogue
  (2nd+ fragment of the same TopicInfo, if the chain has several responses)
  PopulateTopicInfo           [POPULATE] Duplicate detected ... -> cached soft decision reused
```

(The fragment ordering is inferred from the duplicate/single-use-correlation design and
hasn't been confirmed with a log.)

### The dialogue-menu candidate burst (pressing E on an NPC)

The engine builds a DialogueItem for **every** menu candidate (greeting plus each available
topic) in a burst of about 3 ms. Each one goes through the whole sequence above: Ctor →
PopulateTopicInfo → SetSubtitle → ConstructResponse. Consequences:

- The Ctor correlation (gate 4) **does not** tell spoken lines from menu candidates, because
  the Ctor really does fire for each candidate. The code comments that say otherwise predate
  that finding.
- The only thing filtering candidates out of history is the stacked heuristics: the
  candidate-burst filter (only when the candidates share a parent Topic), `dialogueList`
  membership, the cooldowns, and text dedup.
- Every candidate runs `ClearBlockingDecision` and `SetBlockingDecision`, so after the burst
  the thread-local cache and `g_shouldBlockCurrent` hold the **last candidate's** decision.
  `[STALE CACHE]` exists to catch a later ConstructResponse using that leftover decision.
- Soft-blocked candidates get `[AUDIO CLEARED]` even though they're never spoken.

## Shared state and thread-safety

The hooks run on the engine's BSJobs worker threads, and more than one can run at once
(e.g. the first frames after a save load).

| State | Where | Protection | Notes |
|-------|-------|------------|-------|
| `g_recentConstructs` (vector) | `PopulateTopicInfoHook.cpp` | `std::mutex g_constructMutex` | Written by the Ctor hook (via `RecordDialogueConstruct`), read and pruned in phase 7 (correlation + burst filter). Entries older than `CONSTRUCT_WINDOW_MS` (1000 ms) are pruned both on insert in `RecordDialogueConstruct` and in phase 7 gate 4, so the list stays bounded |
| `g_recentlyLogged`, `g_recentlyLoggedByText`, `g_lastCleanupTime` | same | `std::mutex g_loggedMutex` | Cooldown and text dedup |
| `g_shouldBlockCurrent`, `g_shouldSoftBlock`, `g_hardBlocked`, `g_wasEvaluated`, `g_evaluatedTopicInfoFormID` | `ConstructResponseHook.cpp` | `thread_local` | Safe only because PopulateTopicInfo → SetSubtitle/ConstructResponse is one nested call on one thread. A decision made on one thread is invisible on another |
| `g_lastTopicInfoFormID`, `g_lastSpeaker`, `g_lastDialogueTime` | `ConstructResponseHook.cpp` | `thread_local` | Duplicate detection is per thread. The same TopicInfo on two threads is never seen as a duplicate |
| `po3GetEditorID` fn pointer | `EditorID.h` | function-local static (C++11 magic static) | Resolved once |
| Blacklist | `DialogueDB` | `dbMutex_` (recursive) inside `GetBlacklist()` | Returns a **copy** built by `SELECT * FROM blacklist` |

**The past data race** (commit `68bc0d4`) was in `Config::GetSubtypeName` (`src/Config.cpp`),
not in the hook files. Its reverse map was filled lazily with "declare empty, fill if empty()".
The static-init guard only covers creating the empty map, so hooks calling it at the same time
on worker threads corrupted it (freeze/crash). The fix fills the map inside the static
initializer. **Rule:** any static cache reachable from these hooks has to be filled inside its
initializer or protected by a lock.

The `thread_local` decision cache and the `[STALE CACHE]` / `evaluatedFor` diagnostics came in
with commit `8829f74`.

## EditorIDs (`src/EditorID.h` `STFU::GetEditorID`)

On AE the engine throws away EditorIDs for most form types, and STFU keys topic/quest/scene
matching on them. `GetEditorID(form)`:

1. The first time it's called, it resolves `GetFormEditorID(uint32 formID)` exported from `po3_Tweaks`
   (`GetModuleHandleW` + `GetProcAddress`) and caches it in a magic static.
2. If po3 returns a non-empty string, it uses that (the string is interned for the whole process).
3. Otherwise it falls back to the vanilla `TESForm::GetFormEditorID()` vfunc.
4. It can return `nullptr`. Every caller null-checks and converts to `""`.

Without powerofthree's Tweaks, EditorID matching quietly falls back to FormID matching or no
match at all for most AE forms.

## TopicResponseExtractor (`src/TopicResponseExtractor.cpp`)

- `GetResponseList(topicInfo)` calls the game function 25083/25626. The response chain is
  returned through an out-pointer (the function's own return value is ignored). Unclear: who
  owns the returned chain and how long it lives. STFU never frees it.
- `ExtractResponsesFromTopicInfo`: walks `->next` and collects the non-empty `responseText`.
  Used in phase 1 for every PopulateTopicInfo call.
- `ExtractAllResponsesForTopic(editorID | formID)`, `ExtractAllResponsesForScene(sceneEditorID)`:
  gather the text of every TopicInfo of a topic, or of every dialogue action's topic in a scene.
  Topics with no loaded TopicInfos yield no responses. Used in phases 4 and 7 to fill
  `allResponses`, and for runtime blacklist enrichment.
- **Never call a form's `Load(file)`.** `TESTopic::Load` (AE id 25517) parses whatever record the
  shared plugin file is positioned at, not the form's own, and nothing resolves its FormID
  references afterwards. Through 1.2.0 the extractor did this for topics with no loaded infos; logging
  a scene line could overwrite the scene's next topic with an unrelated one whose owner quest was a
  raw FormID, crashing in id 25544 (`test [ownerQuest+0xDC]`). Reported in MQ104IntroScene.
- The `try/catch (std::exception)` blocks won't catch access violations (SEH).

## Log lines to grep for

`STFU.log` runs at info level unless there's a `stfu_debug.flag` file next to it, in which
case it's debug. **Trace is never enabled** (`Logger::Setup` only picks debug or info), so the
`spdlog::trace` lines below are dead unless you change the logger.

| Tag | Level | Meaning |
|-----|-------|---------|
| `ConstructResponseHook: <X> call site 0x.. calls 0x..` | info | E8 guard passed. Compare the target RVA across runtimes |
| `no CALL at the <X> call site ... hook skipped` | error | Offset mismatch. That hook isn't installed |
| `[CTOR ENTRY]` | debug | DialogueItem::Ctor fired (spoken **or** menu candidate) |
| `[CTOR RECORD]`, `[CTOR ALLOW]`, `[DIALOGUE_CTOR]` | trace | Correlation recording (off in practice) |
| `[POPULATE EXTRACTED]` | debug | Phase 1 text for this call |
| `[POPULATE] Duplicate detected` / `New dialogue detected` | debug | Phase 2 result |
| `[POPULATE] Duplicate is soft-blocked (cached decision)` | debug | Duplicate got blanked using the cached decision |
| `[POPULATE] Duplicate is hard-blocked (cached decision)` | debug | Duplicate of a hard-blocked line. Original not called |
| `[ConstructResponse] Blocking decision cleared / set` | debug | Phase 3 / phase 5 cache writes (`soft=`, `topicInfo=`) |
| `[ConstructResponse] Early flag set` | debug | Phase 5 soft → `g_shouldBlockCurrent=true` |
| `[HARD BLOCK]` | info (debug for skip variants) | Phase 4 hard block. Original not called |
| `[POPULATE] Clearing NPC response text for SOFT-BLOCKED` | debug | Phase 5 soft |
| `[SetSubtitle] Called with text=..., g_shouldBlockCurrent=` / `BLOCKING subtitle` | debug | Subtitle hook, every call |
| `[CONSTRUCT ENTRY]` | debug | ConstructResponse call site reached |
| `[ConstructResponse] Using cached decision ... evaluatedFor=, current=` | debug | Decision source for this response |
| `[ConstructResponse] No cached decision` / `Fallback evaluation` | debug | Actor-less fallback |
| `[STALE CACHE]` | **warn** | Cached decision belongs to a different TopicInfo. Suspect a wrong-line silence |
| `[SOFT BLOCK] Silencing audio + subtitles` | debug | ConstructResponse is applying the soft block |
| `[AUDIO CLEARED]` | debug | Regular line's audio/animation wiped, including for menu candidates |
| `[SCENE BLOCK] Blocking bard song` | debug | Bard scene stopped (`isPlaying=false`) |
| `[POPULATE] MATCH FOUND!` / `NO MATCH` | debug | Correlation gate |
| `[COOLDOWN SKIP]`, `[POPULATE] TEXT DUPLICATE SKIP` | debug | Dedup gates |
| `[POPULATE] Skipping unchosen candidate in burst` | debug | Candidate-burst filter |
| `[DIALOGUE] <speaker>: "..."` | debug | About to write history (emitted twice per entry) |
| `[POPULATE EXTRACT]`, `[ResponseExtractor] ...` | debug (warn for lookup failures) | All-responses extraction |
| `[POPULATE SCENE SAFETY]` | error | A hard-blocked scene is playing anyway |

Useful sequence to grep: `CTOR ENTRY|POPULATE|SetSubtitle|CONSTRUCT ENTRY|AUDIO CLEARED|STALE CACHE`.

## Known issues

- **No "actually spoken" signal.** Nothing at this layer can reliably tell a TopicInfo that was
  spoken from one that was only built as a dialogue-menu candidate. History logging and
  subtitle blanking both depend on stacked heuristics (50 ms correlation, 100 ms burst filter,
  `dialogueList`, 5 s cooldowns, text dedup). The planned rewrite is supposed to fix this. The
  big function is left alone until then.
- **Stale greeting subtitle** (deferred to the rewrite): a follower's greeting subtitle can come
  back when the NPC doesn't give a greeting, because of the problem above.
- **DDR subtitle override gap.** DDR hooks the same SetSubtitle (+0x61), ConstructResponse (+0xDE),
  and PopulateTopicInfo. Policy is "STFU block wins", but DDR can still replace the subtitle
  text STFU blanked.
- **Hard block ignores toggles and filters.** Phase 4 matches any Topic/Quest `BlockType::Hard`
  entry without checking `IsFilterCategoryEnabled`, actor/faction filters on the entry, or
  the whitelist. Not verified in game.
- **Cost per line.** Every non-duplicate call copies the whole blacklist
  (`GetBlacklist()` = `SELECT *` under `dbMutex_`). Scene lines do it a second time in phase 8.
  `Config::ShouldSoftBlock` is evaluated up to twice per line (phases 5 and 7).
- **Quest hard-block EditorID match with empty strings.** `e.targetEditorID == questEditorIDStr`
  matches when both are `""`. A FormID-only Quest Hard entry with no EditorID then matches any
  quest that has no EditorID.

## Before you change this

1. **Get a witnessing log first.** Turn on `stfu_debug.flag`, reproduce the problem, and find
   the `[CTOR ENTRY] → [POPULATE] → [SetSubtitle] → [CONSTRUCT ENTRY]` sequence for the line
   involved. Don't change this path because of a single anecdotal report.
2. **Ordering is load-bearing.** SetSubtitle runs before ConstructResponse inside the original,
   so any flag SetSubtitle reads has to be set in PopulateTopicInfo *before* the original is called
   (`SetBlockingDecision`/`ClearBlockingDecision` write `g_shouldBlockCurrent` right away for
   this reason; the comments mention VR ordering).
3. **Keep the decision state `thread_local`**, or if you move it, key it by TopicInfo/speaker.
   A process-wide flag would leak decisions between worker threads.
4. **Any static cache reachable from here must be filled in its initializer or locked** (see the
   `GetSubtypeName` race).
5. **Scene lines must keep producing audio.** Never clear the path or return `false` for
   subtype 14 unless you mean to stop the scene. Use `silent.fuz`, otherwise the scene hangs.
6. **Don't hard-stop scenes** (`isPlaying=false`) except bard songs. Other scenes freeze NPCs.
7. **Porting to a new runtime:** compare the E8 guard's logged call targets with 1.6.1170.
   A skipped hook is quiet apart from one error line.
8. Build doesn't auto-deploy. Close the game before deploying the DLL.

## How to…

- **Add a new per-line action for soft-blocked dialogue:** put it in `Hook_ConstructResponse`
  after the original call, inside `if (a_topic && shouldSoftBlock)`. Remember that it will run
  for menu candidates too.
- **Change what gets logged to history:** add or adjust a gate in phase 7. Keep new gates after
  the correlation gate so the single-use construct entry still gets consumed.
- **Change how a line is judged blocked:** edit `Config::ShouldSoftBlock` / `DB::ShouldSoftBlock`
  ([BLOCKING_RULES.md](BLOCKING_RULES.md)), not the hooks.

## Related docs

- [ARCHITECTURE.md](ARCHITECTURE.md): where these hooks sit in the plugin
- [BLOCKING_RULES.md](BLOCKING_RULES.md): how the soft-block decision is evaluated
- [SCENE_BLOCKING.md](SCENE_BLOCKING.md): condition patching, scene hard blocks, bard songs
- [DATABASE.md](DATABASE.md): `LogDialogue`, blacklist schema, runtime enrichment
- [DEVELOPMENT.md](DEVELOPMENT.md): build, deploy, logging
- [DIALOGUE_SUBTYPES.md](../DIALOGUE_SUBTYPES.md): subtype numbers (14 = scene, 79 = greeting, …)
