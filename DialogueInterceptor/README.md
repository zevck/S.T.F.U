# Dialogue Interceptor - SKSE Plugin

Intercepts and blocks dialogue in Skyrim before audio/subtitle plays and before SkyrimNet logs it.

## Purpose
- Block repetitive/annoying NPC dialogue
- Prevent SkyrimNet from logging filtered dialogue  
- Preserve mechanical functionality (trading, follower commands work)
- Integration with STFU (Skyrim Talk Filter Utility)

## Development Status
**Phase 1 - Detection Only** (Current)
- Logs dialogue events for research
- No blocking yet
- Identifies hook points

## Building

### Requirements
- Visual Studio 2022
- CMake 3.24+
- vcpkg
- CommonLibSSE-NG
- Skyrim Special Edition with SKSE64

### Setup
```powershell
# Set environment variable for CommonLibSSE
$env:CommonLibSSEPath = "C:\path\to\CommonLibSSE-NG"

# Set Skyrim path for auto-install
$env:SkyrimPath = "C:\path\to\Skyrim Special Edition"

# Configure CMake
cmake --preset vs2022-windows

# Build
cmake --build build --config Release
```

### Manual Install
Copy `DialogueInterceptor.dll` to:
```
<Skyrim>\Data\SKSE\Plugins\DialogueInterceptor.dll
```

## Testing

### Phase 1 - Detection
1. Install plugin
2. Run game with SKSE
3. Talk to NPCs
4. Check `Documents\My Games\Skyrim Special Edition\SKSE\DialogueInterceptor.log`
5. Verify dialogue events are being logged

### Expected Log Output
```
[2026-02-11 10:30:15.123] [info] DialogueInterceptor v0.1.0 initialized
[2026-02-11 10:30:20.456] [info] Data loaded - Initializing dialogue hooks
[2026-02-11 10:30:20.457] [info] Installing dialogue hooks...
[2026-02-11 10:30:20.458] [info] Dialogue hooks installed (detection only)
[2026-02-11 10:30:20.459] [info] Loading STFU configuration...
[2026-02-11 10:30:20.460] [info] Configuration loaded (empty - Phase 1)
[2026-02-11 10:31:05.789] [info] Dialogue selected - EditorID: HelloWhiterun FormID: 00012345
[2026-02-11 10:31:05.790] [info]   -> Allowing this dialogue
```

## Architecture
See [SKSE_PLUGIN_SPEC.md](SKSE_PLUGIN_SPEC.md) for detailed technical specification.

## Current Limitations
- **Phase 1**: Detection only, no actual blocking
- Hook points are placeholder (need research)
- STFU config parsing not implemented yet
- Does not actually prevent SkyrimNet logging yet

## Next Steps
1. Research actual dialogue hook points
2. Test with SkyrimNet to verify hook timing
3. Implement blocking mechanism
4. Add STFU configuration parsing
5. Test script preservation with trading dialogues
