# DialogueInterceptor Hook Research

## Current Status (SUCCESS ✅ + PARTIAL ✅)

### Hook 1: DialogueItem::Ctor - **WORKING ✅**
- **Location**: `RELOCATION_ID(34413, 35220)`
- **Method**: `write_branch<5>` detour at function entry
- **Timing**: Fires **11ms BEFORE** SkyrimNet logs dialogue
- **Effect when returning `nullptr`**:
  - ✅ **Blocks SkyrimNet logging** (DialogueItem not created)
  - ❌ **Does NOT block audio playback**
  - ❌ **Does NOT block subtitle display**
- **Status**: **STABLE - No crashes, successfully blocks logging**

### Audio Hook Attempts: **ALL FAILED ❌**

**Problem**: All audio-related functions in Skyrim are used globally for UI sounds, music, ambient effects, and dialogue. Hooking them blocks ALL audio in the game, causing crashes.

**Attempts made:**
1. ❌ **BSSoundHandle::Play** (RELOCATION_ID 66355, 67616)
   - **Result**: Crash on startup - blocks ALL sound playback (UI, music, ambient)
   
2. ❌ **BSAudioManager::BuildSoundDataFromDescriptor** (RELOCATION_ID 66404, 67666)  
   - **Result**: Crash on main menu - blocks ALL sound descriptor building

**Why they failed**: These are low-level audio engine functions called for every sound in the game, not just dialogue.

**Conclusion**: Need to find a higher-level dialogue-specific function that only handles NPC speech, not all audio.

---

## Test Results

### Test Case: Lydia saying "Lead the way." (FormID: 0x000B0EE9)

**Without blocking (logging only):**
```
[2026-02-11 19:24:46.236] [HOOK] DialogueItem::Ctor - DIALOGUE STARTING!
[2026-02-11 19:24:46.236] [SPEAKER] Lydia
[2026-02-11 19:24:46.236] [TOPIC INFO] FormID: 0x000B0EE9
[2026-02-11 19:24:46.247] [SkyrimNet] (Dialogue) Lydia: Lead the way.
```

**With blocking (returning nullptr):**
```
[HOOK] DialogueItem::Ctor - DIALOGUE STARTING!
[SPEAKER] Lydia
[TOPIC INFO] FormID: 0x000B0EE9
[BLOCKED] Dialogue blocked - returning nullptr
```
- ✅ SkyrimNet does NOT log the dialogue
- ❌ Audio still plays
- ❌ Subtitle still displays
- ✅ Follow command still works (scripts execute)

---

## Architecture Discovery

### Dialogue Flow (Simplified)
```
1. Player selects dialogue option
   ↓
2. MenuTopicManager processes selection
   ↓
3. [AUDIO/SUBTITLE TRIGGERS HERE - NOT FOUND YET] ← Target for Hook 2
   ↓
4. DialogueItem::Ctor (OUR HOOK) ← Target for Hook 1
   ↓
5. TESTopicInfoEvent fires
   ↓
6. SkyrimNet logs dialogue
```

### Key Structures

**DialogueItem** (created by our hook target):
```cpp
struct DialogueItem {
    TESTopicInfo* info;
    Actor* speaker;
    TESTopic* topic;
    TESQuest* quest;
    ExtraSayToTopicInfo* extraData;  // Has BSSoundHandle sound
};
```

**ExtraSayToTopicInfo** (audio container):
```cpp
struct ExtraSayToTopicInfo {
    BSSoundHandle sound;              // 28 - Audio playback
    DialogueItem* item;               // 38 - Links to DialogueItem
    float subtitleSpeechDelay;        // 1C
    bool voicePaused;                 // 18
};
```

**SubtitleManager** (subtitle display):
```cpp
class SubtitleManager {
    BSTArray<SubtitleInfo> subtitles;  // Active subtitles
    void KillSubtitles();              // RELOCATION_ID(51755, 52628)
};
```

---

## Proposed Two-Toggle System

### Toggle 1: Block SkyrimNet Logging ✅ **WORKING**
- **Hook**: `DialogueItem::Ctor`
- **Action**: Return `nullptr` to prevent DialogueItem creation
- **Effect**: SkyrimNet cannot log dialogue (no DialogueItem to track)
- **Use case**: Disable AI logging while keeping voice/subtitle

### Toggle 2: Block Audio/Subtitle ❌ **NOT IMPLEMENTED YET**
- **Target**: Function that plays audio or displays subtitle
- **Candidates to investigate**:
  1. Function that populates `ExtraSayToTopicInfo::sound` (BSSoundHandle)
  2. Function that adds subtitle to `SubtitleManager::subtitles`
  3. Actor speech function (Actor::SayTo or similar)
  4. MenuTopicManager response processing
  
- **Use case**: Silence specific dialogue lines

---

## Next Steps

1. **Find audio trigger point**:
   - Search for function that plays dialogue audio
   - Possible candidates:
     - Function that creates/populates `ExtraSayToTopicInfo`
     - `BSSoundHandle::Play()` or similar
     - Actor speech methods
   
2. **Find subtitle trigger point**:
   - Function that adds to `SubtitleManager::subtitles` array
   - Might be same function as audio or separate

3. **Implement Hook 2**:
   - Hook the audio/subtitle trigger
   - Add toggle to enable/disable independently from logging

---

## Code Locations

### Main Hook Implementation
- **File**: `src/main.cpp`
- **Function**: `Hook_DialogueItemCtor()`
- **Installation**: In `kDataLoaded` message handler

### Current Blocking Logic (Phase 2 - Partial)
```cpp
bool ShouldBlockDialogue(uint32_t formID, const char* speakerName) {
    // TEST: Block FormID 0x000B0EE9 ("Lead the way.")
    if (formID == 0x000B0EE9) {
        return true;
    }
    return false;
}
```

---

## Success Metrics

### Phase 1 ✅ COMPLETE
- [x] Detect individual dialogue lines
- [x] Fire before SkyrimNet logs
- [x] Get speaker name and FormID

### Phase 2 ✅ PARTIAL
- [x] Block SkyrimNet logging
- [ ] Block audio playback
- [ ] Block subtitle display
- [x] Scripts still execute

### Phase 3 - TODO
- [ ] STFU configuration integration
- [ ] Two independent toggles (logging vs audio/subtitle)
- [ ] Filter by FormID, speaker, or other criteria

## Next Steps

**Finding a dialogue-specific audio hook:**

1. **Analyze call stack** - Use debugger to capture stack trace when dialogue audio plays, find dialogue-specific caller
2. **Actor voice functions** - Search for Actor member functions specifically related to voice/speech
3. **HighProcessData voice state** - Hook functions that modify `voiceState` in HighProcessData
4. **ExtraSayToTopicInfo creation** - Find where ExtraSayToTopicInfo is created and sound handle is populated
5. **MenuTopicManager dialogue flow** - Hook functions in MenuTopicManager that trigger dialogue playback
6. **Reverse engineer from SkyrimNet** - If SkyrimNet can intercept dialogue (via TESTopicInfoEvent), trace backward to find earlier trigger

**Alternative approaches:**

1. **Subtitle-only blocking** - Find subtitle display function (more likely to be dialogue-specific)
2. **Post-block cleanup** - Let audio start, then immediately stop it (using BSSoundHandle::Stop on the handle)
3. **Modify DialogueResponse** - Hook where DialogueResponse audio is queued, before it reaches audio engine

---

## Subtitle Hook Attempts

### ⚡ Attempt 5 (IN PROGRESS): SubtitleManager Active Monitoring
- **RELOCATION_ID**: 51755/52628 (KillSubtitles)
- **Status**: Build successful, ready for in-game testing
- **Approach**: Since we can't prevent subtitle creation, kill them immediately after they appear
  1. When dialogue is blocked in DialogueItem::Ctor:
     - Immediately call `SubtitleManager::GetSingleton()->KillSubtitles()`
     - Spawn background thread monitoring `SubtitleManager::subtitles` array for 5 seconds
     - Poll every 50ms: if subtitle from blocked speaker appears, kill it
     - Track blocked speaker via `ObjectRefHandle` from DialogueItem
  2. Each `SubtitleInfo` has `ObjectRefHandle speaker` field for identification
  3. Only kill subtitles matching our blocked speaker handle
- **Test case**: FormID 0x000B0EE9 ("Lead the way." - Lydia)
- **Expected results**:
  - ✅ SkyrimNet logging blocked (via nullptr return)
  - ⚡ **TESTING**: Subtitles killed/prevented from display
  - ❓ **UNKNOWN**: Will blocking subtitles also block audio playback?
  - ❓ **UNKNOWN**: Will detached thread cause stability issues?
- **Implementation**:
  ```cpp
  // Immediate kill + spawn monitoring thread
  auto subtitleMgr = RE::SubtitleManager::GetSingleton();
  subtitleMgr->KillSubtitles();
  
  std::thread([speakerHandle, formID]() {
      for (5 seconds) {
          if (subtitle.speaker == speakerHandle) {
              subtitleMgr->KillSubtitles();
          }
          sleep(50ms);
      }
  }).detach();
  ```

**Why this might work**:
- SubtitleManager is dialogue-specific (doesn't affect UI or other text)
- Killing subtitles doesn't crash the game (safe operation)
- Thread-safe with mutex locking
- Detached thread won't block game execution

**Testing goals**:
1. Do subtitles disappear/flicker/never appear?
2. Does audio still play when subtitles are killed?
3. Any crashes or stability issues?
4. Does it feel janky or smooth?
4. **Voice file interception** - Hook file loading for .fuz/.wav voice files
5. **Accept partial blocking** - Keep current SkyrimNet logging block, add subtitle block, document that audio can't be safely blocked

---

## Performance Notes

- Hook fires rapidly during conversations (multiple times per second)
- Returning `nullptr` has minimal performance impact
- No crashes or stability issues observed (with single hook)
- Scripts continue to execute normally (follow commands work)
