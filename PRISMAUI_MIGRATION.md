# PrismaUI Migration for STFU

## Overview

Started migration from ImGui (SKSE Menu Framework) to PrismaUI for better VR support and unified UI ecosystem across mods.

## Current Status - History Tab (Display Only)

### What's Implemented ✅

**Web UI** ([web-ui/](c:/Nolvus/Instances/Nolvus Awakening/MODS/mods/STFU/web-ui/)):
- React/TypeScript UI with Tailwind CSS
- History component with search functionality
- Detail panel showing full entry information  
- Color-coded status indicators
- [Menu] tag for SkyrimNet-compatible dialogue
- Responsive layout

**C++ Integration**:
- [PrismaUIMenu.h](c:/Nolvus/Instances/Nolvus Awakening/MODS/mods/STFU/DialogueInterceptor/include/PrismaUIMenu.h) / [PrismaUIMenu.cpp](c:/Nolvus/Instances/Nolvus Awakening/MODS/mods/STFU/DialogueInterceptor/src/PrismaUIMenu.cpp)
- PrismaUI API initialization
- View creation for STFU-UI
- JSON serialization of history data
- JS listener for requestHistory events
- Hotkey toggle (Insert key by default)

**Updated Files**:
- [CMakeLists.txt](c:/Nolvus/Instances/Nolvus Awakening/MODS/mods/STFU/DialogueInterceptor/CMakeLists.txt) - Added PrismaUIMenu.cpp to build
- [main.cpp](c:/Nolvus/Instances/Nolvus Awakening/MODS/mods/STFU/DialogueInterceptor/src/main.cpp) - Initialize PrismaUIMenu, redirect hotkey to PrismaUI

### Testing Instructions

1. **Build the Web UI**:
   ```powershell
   cd "c:\Nolvus\Instances\Nolvus Awakening\MODS\mods\STFU\web-ui"
   npm install
   npm run build
   ```

2. **Deploy UI files**:
   Copy `web-ui/dist/*` to:
   ```
   Data/SKSE/Plugins/STFU/PrismaUI/views/STFU-UI/
   ```
   The HTML should be at: `Data/SKSE/Plugins/STFU/PrismaUI/views/STFU-UI/index.html`

3. **Build the C++ plugin**:
   ```powershell
   cd "c:\Nolvus\Instances\Nolvus Awakening\MODS\mods\STFU\DialogueInterceptor"
   cmake --build build --config Release
   cmake --install build --config Release
   ```

4. **Test in-game**:
   - Press Insert key (or configured hotkey)
   - PrismaUI menu should open showing dialogue history
   - Search should filter entries
   - Click entries to see details
   - ESC or Insert again to close

### Known Limitations

- **Display Only**: No blacklist/whitelist actions yet (next phase)
- **No Tabs**: Only History tab implemented
- **ImGui Menu**: Still available (can uncomment line in main.cpp to toggle both)
- **No Settings**: Settings tab not yet migrated

## Next Steps

### Phase 2: History Tab Actions
- Add Block/Whitelist buttons to detail panel
- Implement C++ handlers for add/remove operations
- Add confirmation dialogs
- Test multi-select for bulk operations

### Phase 3: Additional Tabs
- Blacklist tab (view + edit + delete)
- Whitelist tab (view + edit + delete)
- SkyrimNet Filter tab
- Subtype Overrides tab

### Phase 4: Settings Migration
- Settings tab with all options
- Hotkey configuration UI
- Replace ImGui settings completely

### Phase 5: Cleanup
- Remove SKSE Menu Framework dependency
- Remove all ImGui code (STFUMenu.cpp, STFUMenu_Settings.cpp)  
- Update documentation
- Final testing

## File Structure

```
STFU/
├── web-ui/                          # React/TypeScript UI project
│   ├── src/
│   │   ├── components/
│   │   │   └── history.tsx         # History tab component
│   │   ├── stores/
│   │   │   └── history.ts          # Zustand state management
│   │   ├── lib/
│   │   │   └── skse-api.ts         # SKSE bridge
│   │   ├── types.ts                # TypeScript interfaces
│   │   ├── app.tsx                 # Main app component
│   │   └── index.tsx               # Entry point
│   ├── package.json
│   ├── vite.config.ts
│   └── dist/                        # Build output (after npm run build)
│
└── DialogueInterceptor/
    ├── include/
    │   └── PrismaUIMenu.h          # PrismaUI menu interface
    └── src/
        └── PrismaUIMenu.cpp        # PrismaUI implementation

Deployed (after build):
Data/SKSE/Plugins/STFU/PrismaUI/views/STFU-UI/
└── index.html                       # + CSS/JS assets
```

## Development Workflow

**Edit UI**:
```powershell
cd web-ui
npm run dev                          # Live reload at http://localhost:5173
# Edit src/components/*.tsx
npm run build                        # When ready to test in-game
# Copy dist/* to Data/SKSE/Plugins/STFU/PrismaUI/views/STFU-UI/
```

**Edit C++**:
```powershell
cd DialogueInterceptor
# Edit src/PrismaUIMenu.cpp
cmake --build build --config Release
cmake --install build --config Release
```

## Communication Pattern

**C++ → UI**: Send data via `PrismaUI->Invoke()`
```cpp
std::string json = SerializeHistoryToJSON();
std::string js = "window.SKSE_API?.call('updateHistory', '" + json + "')";
prismaUI_->Invoke(view_, js.c_str());
```

**UI → C++**: Call registered listeners
```typescript
// In TypeScript:
SKSE_API.sendToSKSE('requestHistory');

// Triggers in C++:
prismaUI_->RegisterJSListener(view_, "requestHistory", &OnRequestHistory);
```

## References

- PrismaUI Docs: https://prismaui.dev
- Example Plugin: [.resources/PrismaUI/example-skse-plugin](c:/Nolvus/Instances/Nolvus Awakening/MODS/mods/STFU/.resources/PrismaUI/example-skse-plugin/)
- Example Web UI: [.resources/PrismaUI/example-web-ui](c:/Nolvus/Instances/Nolvus Awakening/MODS/mods/STFU/.resources/PrismaUI/example-web-ui/)
