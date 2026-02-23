# ExtraSayToTopicInfo Research - The Missing Link

## Key Discovery

**DialogueItem has an `extraData` member** at offset 0x40 that holds `ExtraSayToTopicInfo*`.

```cpp
class DialogueItem : public BSIntrusiveRefCounted {
    // ...
    ExtraSayToTopicInfo* extraData{ nullptr };  // 40
};
```

## ExtraSayToTopicInfo Structure

```cpp
class ExtraSayToTopicInfo : public BSExtraData {
    TESTopic*          topic;                // 0x10
    bool               voicePaused;          // 0x18
    std::uint8_t       pad19;                // 0x19
    std::uint16_t      pad1A;                // 0x1A
    float              subtitleSpeechDelay;  // 0x1C
    BGSDialogueBranch* exclusiveBranch;      // 0x20
    BSSoundHandle      sound;                // 0x28 ← THE DIALOGUE VOICE SOUND
    std::uint32_t      pad34;                // 0x34
    DialogueItem*      item;                 // 0x38 (back-reference)
};
```

**This is the dialogue-specific sound handle we need to block!**

## The Dialogue Flow

1. **DialogueItem::Ctor** - Creates dialogue item (✅ we hook this at RELOCATION_ID 34413, 35220)
2. **ExtraSayToTopicInfo Created** - Somewhere after DialogueItem creation (⚠️ NOT YET FOUND)
3. **extraData Assigned** - `item->extraData = newExtraData` (⚠️ NOT YET FOUND)
4. **BSSoundHandle Populated** - `extraData->sound` is set up with voice file
5. **Sound Plays** - `extraData->sound.Play()` or similar

## The Gap We're Trying toFill

- **Too Early**: DialogueItem::Ctor (returning nullptr doesn't stop audio)
- **Too Late/General**: BSSoundHandle::Play (crashes, called for ALL sounds)
- **🎯 TARGET**: ExtraSayToTopicInfo creation or `item->extraData` assignment

## Failed Approaches

1. **Actor Virtual Functions** - Don't fire for normal NPC dialogue:
   - SetDialogueWithPlayer (vtable 0x40)
   - UpdateInDialogue (vtable 0x4B) 
   - QSpeakingDone (vtable 0x107)
   - SetSpeakingDone (vtable 0x108)

2. **BSSoundHandle::Play Hook** (RELOCATION_ID 66357, 67618):
   - ❌ Crashes game on save load
   - Too low-level, called thousands of times per frame
   - Handles UI clicks, footsteps, music, ambience, effects - EVERYTHING

3. **BSSoundHandle::FadeInPlay Hook** (RELOCATION_ID 66370, 67631):
   - ❌ Same crash as Play
   - Same issue - too fundamental

## What We Haven't Found Yet

1. **ExtraSayToTopicInfo Constructor**:
   - VTABLE: RELOCATION_ID(187033, 229905)
   - RTTI: RELOCATION_ID(392475, unknown)
   - Pattern from other ExtraData: Uses `stl::emplace_vtable(this)` in constructor
   - CommonLibSSE doesn't expose a Ctor function for it

2. **Where `item->extraData` is Assigned**:
   - No grep hits for `->extraData =` or `= new ExtraSayToTopicInfo`
   - Likely happens in engine code not exposed in CommonLibSSE headers

3. **Where `extraData->sound` is Played**:
   - Could be a dialogue-specific wrapper around BSSoundHandle::Play
   - Something between "create dialogue item" and "play any sound"

## Potential Next Research Directions

### 1. Hook ExtraSayToTopicInfo Virtual Functions
Since ExtraSayToTopicInfo has a virtual destructor, we could try hooking:
- **Destructor** (vtable offset 0x00)
- **GetType()** (vtable offset 0x01)

If we hook the destructor, we could track when ExtraSayToTopicInfo objects are created/destroyed.

### 2. Search for BSSoundHandle Member Functions

Instead of hooking `BSSoundHandle::Play` globally, look for functions that:
- Take a `BSSoundHandle*` parameter
- Are called FROM dialogue code specifically
- Might be something like `PlayDialogueSound(BSSoundHandle*)`

### 3. Hook MenuTopicManager Functions

MenuTopicManager has:
- `currentTopicInfo` - The currently playing dialogue
- `speaker` - The NPC speaking
- Dialogue response lists

Potential hooks:
- ProcessEvent (MenuOpenCloseEvent)
- ProcessEvent (PositionPlayerEvent)
- Functions that iterate through `dialogueList` or `responses`

### 4. Post-DialogueItem Hook

Find a function that's called AFTER DialogueItem::Ctor returns:
- Where responses are processed
- Where extraData is accessed for the first time
- Could log stack traces from DialogueItem::Ctor to see what calls it

### 5. Sabotage DialogueItem Post-Creation

Instead of blocking creation, let it succeed then:
```cpp
auto result = _DialogueItemCtor(...);
if (shouldBlock && result) {
    // Option A: Null out the extraData pointer after it's created
    // Option B: Replace extraData with a fake one that has a broken sound handle
    // Option C: Hook a function that checks result->extraData and block there
}
```

### 6. SubtitleManager Integration

SubtitleManager might be tightly coupled with voice playback:
- RELOCATION_ID(514283, 400443) - Singleton
- Has `KillSubtitles()` function
- Maybe blocking subtitle creation also blocks audio?

### 7. Search for "Say" or "Speak" Functions

Look for high-level functions with names like:
- `Actor::Say(...)`
- `Character::Speak(...)`  
- `SpeakDialogue(...)`
- `PlayVoiceLine(...)`

That sit between DialogueItem creation and raw audio playback.

## Current Status

**Enabled Hooks:**
- ✅ DialogueItem::Ctor (RELOCATION_ID 34413, 35220) - Fires, logs dialogue
- ✅ Actor virtual functions (4 hooks) - Install successfully but never fire

**Disabled Hooks:**
- ❌ BSSoundHandle::Play - Causes crash
- ❌ BSSoundHandle::FadeInPlay - Causes crash

**Current Behavior:**
- DialogueItem creation is flagged when should be blocked
- But audio/subtitle still play (no actual blocking yet)
- SkyrimNet still sees the dialogue (not blocked from logging)

## Success Criteria

Find a hook point that:
1. ✅ Has access to topic info (to filter what to block)
2. ✅ Is dialogue-specific (not called for UI/music/effects)
3. ✅ Fires BEFORE audio plays
4. ✅ Doesn't crash when blocked
5. ✅ Blocks both audio AND subtitle
6. ✅ Doesn't break scripts (follower commands still execute)

## Research Strategy

**Priority 1**: Find where ExtraSayToTopicInfo is created
- Hook its virtual destructor to track lifetime
- Search for RELOCATION_IDs related to ExtraSayToTopicInfo creation
- Look for `BSExtraData::Create<ExtraSayToTopicInfo>()` calls

**Priority 2**: Find where `item->extraData` is assigned
- Search for functions that take DialogueItem* and modify it
- Look for functions between DialogueItem::Ctor and audio playback

**Priority 3**: Find dialogue-specific BSSoundHandle usage
- Functions that call BSSoundHandle::Play specifically for voice
- Wrapper functions around BSSoundHandle used only by dialogue system

**Priority 4**: Experimental sabotage approaches
- Modify DialogueItem after creation to break audio
- Hook post-DialogueItem functions to intercept extraData usage
