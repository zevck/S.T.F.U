# Dialogue Interceptor - SKSE Plugin Specification

## Objective
Create an SKSE plugin that intercepts dialogue **before** it plays audio/subtitle and **before** SkyrimNet logs it, while still allowing attached scripts to execute.

## Target Use Cases
1. **Mechanically-necessary dialogue** (trading, follower commands) - functional but silent and unlogged
2. **Repetitive NPC chatter** - completely silenced and unlogged
3. **Inane background dialogue** - blocked from being annoying

## Technical Architecture

### Core Requirements
1. **Hook Point**: Intercept dialogue between topic selection and audio/subtitle display
2. **Script Preservation**: Allow Papyrus scripts attached to dialogue to execute normally
3. **Coordination**: Hook earlier than SkyrimNet to prevent logging
4. **Configuration**: Read STFU filter configuration to determine what to block

### Dialogue Pipeline (Skyrim Engine)
```
[Player selects dialogue topic]
          ↓
[TopicInfo selected]
          ↓
[Scripts queued for execution] ← WE NEED TO HOOK HERE
          ↓
[Subtitle text prepared]
          ↓
[Voice audio prepared]
          ↓
[??? SkyrimNet hooks somewhere around here ???]
          ↓
[Subtitle displays]
          ↓
[Audio plays]
```

### Implementation Strategy

#### Phase 1: Minimal Viable Plugin (Detection Only)
- **Goal**: Detect when dialogue is about to play
- **Approach**: 
  - Hook into MenuTopicInfo processing
  - Log FormKey/EditorID of dialogue being played
  - No blocking yet, just observation

#### Phase 2: Add Conditional Blocking
- **Goal**: Block specific dialogue from playing
- **Approach**:
  - Read STFU configuration files (YAML/INI)
  - Check if dialogue should be blocked
  - Return early from dialogue processing if blocked
  - Log what was blocked

#### Phase 3: Script Preservation
- **Goal**: Ensure scripts still execute even when dialogue is blocked
- **Approach**:
  - Identify where scripts are attached to dialogue
  - Manually trigger script execution before blocking
  - Test with trading/follower command dialogues

#### Phase 4: Production Ready
- **Goal**: Stable, configurable, well-integrated
- **Features**:
  - MCM integration or INI config
  - Performance optimization
  - Extensive testing
  - Documentation

## Known Challenges

### 1. Hook Timing Uncertainty
**Problem**: We don't know exactly where SkyrimNet hooks dialogue
**Risk**: We might hook after SkyrimNet, making this pointless
**Mitigation**: 
- Start with early hooks (MenuTopicInfo processing)
- Test empirically with SkyrimNet logging
- May need to reverse-engineer SkyrimNet.dll

### 2. Script Execution Separation
**Problem**: Unknown if Skyrim's engine separates script exec from audio/text
**Risk**: Blocking dialogue might also block scripts
**Mitigation**:
- Research existing SKSE plugins that modify dialogue
- Test with simple scripts first (Message boxes, etc.)
- Progressive testing with complex scripts (trading)

### 3. SkyrimNet Closed-Source Coordination
**Problem**: Can't coordinate with SkyrimNet directly
**Risk**: Race conditions, incompatibility, conflicts
**Mitigation**:
- Hook as early as possible
- Make plugin load before SkyrimNet (metadata manipulation)
- Extensive compatibility testing

## Potential Hooks (CommonLibSSE)

### MenuTopicInfo Processing
- **Location**: When dialogue topic is selected from menu
- **Pros**: Very early in pipeline, before most processing
- **Cons**: May be too early, before scripts are set up

### DialogueMenu Events
- **Location**: Dialogue menu events (opening, closing, topic selection)
- **Pros**: Well-documented in CommonLibSSE
- **Cons**: May be too late (after audio prep)

### TESTopicInfo Processing
- **Location**: When TopicInfo record is being processed
- **Pros**: Access to full dialogue record data
- **Cons**: Uncertain timing relative to SkyrimNet

### Subtitle Display Hook
- **Location**: Before subtitle is drawn to screen
- **Pros**: Definitely before display
- **Cons**: Likely too late (SkyrimNet probably already logged)

## Configuration Integration

### Read STFU Configuration
- Parse `STFU_Config.ini` to get enabled/disabled subtypes
- Parse `STFU_Blacklist.yaml` for always-block topics
- Parse `STFU_Whitelist.yaml` for never-touch topics
- Parse `STFU_SubtypeOverrides.yaml` for manual classifications

### Decision Logic
```cpp
bool ShouldBlockDialogue(TESTopicInfo* topic) {
    // 1. Check whitelist (never block)
    if (IsWhitelisted(topic->formID or topic->editorID))
        return false;
    
    // 2. Check blacklist (always block)
    if (IsBlacklisted(topic->formID or topic->editorID))
        return true;
    
    // 3. Check subtype filtering (MCM controlled)
    string subtype = GetSubtype(topic);
    if (IsSubtypeBlocked(subtype))
        return true;
    
    // 4. Default: don't block
    return false;
}
```

## Testing Plan

### Test 1: Detection
- **Objective**: Verify plugin can detect dialogue
- **Method**: Install plugin, trigger dialogue, check logs
- **Success**: FormKeys logged correctly

### Test 2: Blocking
- **Objective**: Verify dialogue can be blocked
- **Method**: Block a simple idle chatter, verify no subtitle/audio
- **Success**: No dialogue plays, no subtitle shows

### Test 3: SkyrimNet Coordination
- **Objective**: Verify SkyrimNet doesn't log blocked dialogue
- **Method**: Block dialogue, check SkyrimNet event log
- **Success**: Blocked dialogue doesn't appear in log

### Test 4: Script Preservation
- **Objective**: Verify scripts still execute when dialogue blocked
- **Method**: Block follower trade dialogue, attempt trade
- **Success**: Trading menu opens despite no dialogue

### Test 5: STFU Integration
- **Objective**: Verify plugin reads STFU configuration correctly
- **Method**: Enable specific subtypes in STFU, verify blocking behavior
- **Success**: Matches STFU configuration exactly

## Development Environment

### Requirements
- Visual Studio 2022
- CMake 3.24+
- CommonLibSSE-NG
- vcpkg
- SKSE64 runtime

### Build Process
1. Clone CommonLibSSE-NG
2. Set up vcpkg dependencies
3. CMake configuration
4. Build DLL
5. Copy to `Data/SKSE/Plugins/`

## File Structure
```
DialogueInterceptor/
├── CMakeLists.txt
├── vcpkg.json
├── src/
│   ├── main.cpp          # SKSE plugin entry point
│   ├── DialogueHook.h    # Dialogue interception hooks
│   ├── DialogueHook.cpp  # Hook implementations
│   ├── Config.h          # STFU config parsing
│   ├── Config.cpp        # Config implementation
│   └── Logger.h          # Logging utilities
├── include/
│   └── version.h         # Plugin version info
└── README.md             # Build instructions
```

## Next Steps
1. Set up basic SKSE plugin project structure
2. Implement minimal logging (Phase 1)
3. Test detection with simple dialogue
4. Iterate based on empirical results
