# STFU PrismaUI Web Interface

React/TypeScript UI for STFU using PrismaUI framework.

## Development

```bash
npm install
npm run dev
```

Visit http://localhost:5173 to develop in browser.

## Build

```bash
npm run build
```

Output goes to `dist/` folder. Copy contents to `Data/SKSE/Plugins/STFU/PrismaUI/views/STFU-UI/`.

## Structure

- `src/components/` - React components
- `src/stores/` - Zustand state management
- `src/lib/` - SKSE API bridge
- `src/types.ts` - TypeScript interfaces

## Current Features

- **History Tab**: Display dialogue history with search and details panel (read-only)
