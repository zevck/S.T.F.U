# DialogueInterceptor - Testing Phase 2

## Current Status

✅ Plugin loads and initializes
✅ Basic monitoring code implemented
⚠️ **No actual hooks yet** - need to find the right interception point

## Testing Instructions

### 1. Copy Updated DLL
```
From: STFU\DialogueInterceptor\SKSE\Plugins\DialogueInterceptor.dll
To: C:\Nolvus\Instances\Nolvus Awakening\STOCK GAME\Data\SKSE\Plugins\DialogueInterceptor.dll
```

### 2. Launch Game & Trigger Dialogue
- Talk to any NPC
- Trigger some greetings, dialogue options
- Close dialogue menu
- Let NPC finish speaking

### 3. Check Log
**Location:** `Documents\My Games\Skyrim Special Edition\SKSE\DialogueInterceptor.log`

**Look for:**
```
[timestamp] [info] Phase 2 - Active monitoring enabled
[timestamp] [info] Setup complete!
```

### 4. Compare with SkyrimNet Logs
Open SkyrimNet's event monitor and check when it logs dialogue events.

**Key question:** Can we detect dialogue state changes in MenuTopicManager?

## Next Steps - Finding Hook Points

We need to intercept dialogue **before** it reaches:
- SubtitleManager (shows subtitles)
- Audio system (plays voice audio)
- SkyrimNet's logging system

### Potential Hook Points to Test

#### Option 1: Actor::SayTopicInfoResponse
When an actor starts speaking a dialogue line.
```cpp
// Offset: RELOCATION_ID(???, ???)
// Would let us block before audio/subtitle
```

#### Option 2: SubtitleManager::AddSubtitle
When subtitle is added to display queue.
```cpp
// Could kill subtitle immediately after it's added
// Or prevent it from being added
```

#### Option 3: MenuTopicManager::SetCurrentTopicInfo
When dialogue topic info changes.
```cpp
// This happens when dialogue is selected
// Might be too early (before scripts run?)
```

#### Option 4: DialogueMenu::ProcessMessage
When dialogue menu processes UI messages.
```cpp
// Hook ProcessMessage to intercept dialogue selection
```

## Research Needed

1. **Find the function offset** for where dialogue starts playing
2. **Test timing** - does it fire before SkyrimNet sees it?
3. **Verify scripts still execute** when we block audio/subtitle

### How to Find Offsets

Search in CommonLibSSE-NG for:
- `Actor::Say*` functions
- `SubtitleManager::*` functions  
- `MenuTopicManager::*` functions

Look in `Offsets.h` or `Offsets_NiRTTI.h` for RELOCATION_ID values.

## Hook Implementation Pattern

Once we find the right function:

```cpp
struct FunctionName_Hook
{
    static ReturnType thunk(Args...)
    {
        // Log what we're intercepting
        spdlog::info("[HOOK] Function called with...");
        
        // Check if we should block
        if (ShouldBlockDialogue(...)) {
            spdlog::info("[BLOCKED] Dialogue blocked!");
            return; // Don't call original
        }
        
        // Call original function
        return func(args...);
    }
    static inline REL::Relocation<decltype(thunk)> func;
};

// In Install():
REL::Relocation<std::uintptr_t> hook_target{ RELOCATION_ID(???, ???) };
auto& trampoline = SKSE::GetTrampoline();
trampoline.create(/* size */);
FunctionName_Hook::func = trampoline.write_call<5>(hook_target.address(), FunctionName_Hook::thunk);
```

## Success Criteria

✅ Hook fires when NPC speaks
✅ Hook fires **before** SkyrimNet logs the dialogue
✅ We can block audio/subtitle
✅ Scripts still execute (test with "I need you to do something" → follower commands)
✅ STFU configuration is respected (blacklist blocks, whitelist allows)
