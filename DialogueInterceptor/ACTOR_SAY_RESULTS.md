# Actor::Say Hook Research - Results

## Test Results: Actor Virtual Functions

**Hooks Attempted:**
- `Actor::SetDialogueWithPlayer` (vtable 0x40)
- `Actor::UpdateInDialogue` (vtable 0x4B)
- `Actor::QSpeakingDone` (vtable 0x107)
- `Actor::SetSpeakingDone` (vtable 0x108)

**Result:** ❌ **NONE of these fire during normal NPC dialogue**

## What Works

✅ **DialogueItem::Ctor** (RELOCATION_ID 34413, 35220)
- Fires reliably for all NPC dialogue
- Provides: Speaker, TESTopic, TESTopicInfo, TESQuest
- Timing: 11ms BEFORE SkyrimNet logs
- **Problem:** Returning `nullptr` blocks SkyrimNet logging but NOT audio/subtitle

## Dialogue Flow Discovery

```
[Player initiates dialogue or NPC greets]
   ↓
[AUDIO/SUBTITLE TRIGGER - Unknown location] ← Need to find this
   ↓
DialogueItem::Ctor (0ms) ← We hook here
   ↓  
TESTopicInfoEvent fires (+11ms)
   ↓
SkyrimNet logs dialogue
```

## Why Actor Virtual Functions Failed

**Theory:** Actor virtual functions (SetDialogueWithPlayer, UpdateInDialogue) might be:
1. **Player-specific** - Only called for player character dialogue
2. **Special cases only** - Only for specific dialogue types (not regular speech)
3. **Wrong inheritance level** - Maybe Character class overrides them but we're not seeing it

**Evidence:**
- SetSpeakingDone fired hundreds of times on load (all actors finishing animations)
- But NEVER fired with `a_set = false` (starting to speak) during actual dialogue
- No other Actor hooks triggered during Lydia conversation

## HighProcessData Voice Members

Found in `HighProcessData.h`:
```cpp
VOICE_STATE voiceState;        // 0x000
float voiceTimeElapsed;        // 0x014
float voiceRecoveryTime;       // 0x018
BSFixedString voiceSubtitle;   // 0x218
float voiceTimer;              // 0x328
```

**These are data members, not hookable functions.** We'd need to find the functions that MODIFY these.

## Next Research Directions

### 1. MenuTopicManager
Look for functions that trigger dialogue responses:
- ProcessTopicInfo
- SelectTopic
- HandleDialogueResponse

### 2. ExtraSayToTopicInfo Creation
Find where `ExtraSayToTopicInfo` is created and `BSSoundHandle` is populated:
- This is the structure that contains the actual audio handle
- Hooking its creation might let us intercept before audio plays

### 3. AIProcess Voice Functions
Search for non-virtual functions in AIProcess/HighProcessData:
- Functions that modify `voiceState`
- Functions that play voice files
- `ProcessVoice()` or similar

### 4. BSSound Handle Play
Hook BSSoundHandle::Play but filter by:
- Sound descriptor type (dialogue vs effects)
- Calling context (check call stack)
- Sound category

### 5. Subtitle Manager
Since audio and subtitles might be separate:
- Hook subtitle display (already attempted, killed them post-creation)
- Audio might need separate hook

## Conclusion So Far

**The Actor class virtual function table approach failed.** 

The dialogue voice playback system appears to use:
- Non-virtual functions
- Different class hierarchy (AIProcess/HighProcessData)
- Direct sound system calls (BSSoundHandle)

**Next step:** Research MenuTopicManager and AIProcess non-virtual functions, or try hooking at the sound handle level with better filtering.
