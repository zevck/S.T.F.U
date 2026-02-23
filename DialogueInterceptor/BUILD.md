# Build Instructions

## Prerequisites

### 1. Install Visual Studio 2022
- Download from https://visualstudio.microsoft.com/
- Select "Desktop development with C++" workload
- Install C++ CMake tools

### 2. Install CMake
- Download from https://cmake.org/download/
- Version 3.24 or higher
- Add to PATH during installation

### 3. Install vcpkg
```powershell
# Clone vcpkg
cd C:\
git clone https://github.com/microsoft/vcpkg
cd vcpkg
.\bootstrap-vcpkg.bat

# Set environment variable
$env:VCPKG_ROOT = "C:\vcpkg"
[System.Environment]::SetEnvironmentVariable('VCPKG_ROOT', 'C:\vcpkg', 'User')
```

### 4. Install CommonLibSSE-NG
```powershell
cd C:\
git clone https://github.com/CharmedBaryon/CommonLibSSE-NG.git
cd CommonLibSSE-NG
git checkout ng

# Set environment variable
$env:CommonLibSSEPath = "C:\CommonLibSSE-NG"
[System.Environment]::SetEnvironmentVariable('CommonLibSSEPath', 'C:\CommonLibSSE-NG', 'User')
```

### 5. Set Skyrim Path (Optional - for auto-install)
```powershell
# Replace with your actual Skyrim path
$env:SkyrimPath = "C:\Path\To\Skyrim Special Edition"
[System.Environment]::SetEnvironmentVariable('SkyrimPath', 'C:\Path\To\Skyrim Special Edition', 'User')
```

## Building

### Option 1: Visual Studio (Recommended for development)
```powershell
# Open project in Visual Studio
cd "C:\Nolvus\Instances\Nolvus Awakening\MODS\mods\Wounds SkyrimNet\DialogueInterceptor"
code .  # Or open CMakeLists.txt in VS2022

# Visual Studio will auto-detect CMake and configure
# Press F7 to build, or use Build menu
```

### Option 2: Command Line
```powershell
cd "C:\Nolvus\Instances\Nolvus Awakening\MODS\mods\Wounds SkyrimNet\DialogueInterceptor"

# Configure (first time only)
cmake --preset vs2022-windows

# Build
cmake --build build --config Release

# Output: build/Release/DialogueInterceptor.dll
```

## Installing

### If SkyrimPath was set:
DLL is automatically copied to `Skyrim\Data\SKSE\Plugins\` after build

### Manual install:
```powershell
# Copy DLL to Skyrim
Copy-Item "build/Release/DialogueInterceptor.dll" `
          "C:\Path\To\Skyrim\Data\SKSE\Plugins\DialogueInterceptor.dll"
```

## Testing

1. Launch Skyrim with SKSE64
2. Talk to any NPC
3. Check log file:
   ```
   Documents\My Games\Skyrim Special Edition\SKSE\DialogueInterceptor.log
   ```
4. Should see dialogue events being logged

## Troubleshooting

### "CommonLibSSE not found"
- Verify `$env:CommonLibSSEPath` is set correctly
- Restart PowerShell/VS after setting environment variable

### "vcpkg not found"  
- Verify `$env:VCPKG_ROOT` is set correctly
- Run `.\vcpkg\bootstrap-vcpkg.bat` if not already done

### DLL doesn't load in game
- Ensure you're using SKSE64 to launch game (not regular launcher)
- Check SKSE log for errors: `Documents\My Games\Skyrim Special Edition\SKSE\skse64.log`
- Verify Address Library is installed

### No log file created
- Plugin may not be loading at all
- Check SKSE log first
- Try running game with admin privileges

## Common Errors

**Error: "cannot open file 'CommonLibSSE.lib'"**
Solution: CommonLibSSE path not set correctly

**Error: "SKSE headers not found"**  
Solution: Reinstall CommonLibSSE-NG with proper environment variable

**Error: "Plugin failed to load - incompatible"**
Solution: Rebuild plugin, ensure targeting correct Skyrim version (SE/AE)
