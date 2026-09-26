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

Output goes to `dist/`. Copy its contents (`index.html` and `assets/`) to `PrismaUI/views/STFU/` in the mod folder. The plugin loads the view as `STFU/index.html`.

## Structure

- `src/components/` - React components
- `src/stores/` - Zustand state management
- `src/lib/` - SKSE API bridge
- `src/types.ts` - TypeScript interfaces
