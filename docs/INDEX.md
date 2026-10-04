# STFU Documentation Index

Developer docs for STFU (Skyrim Talk Filter Utility): an SKSE plugin that silences or blocks NPC dialogue and scenes, plus its PrismaUI menu and Papyrus MCM. They are written for coding agents and the author. The user-facing docs are the repo-root [README.md](../README.md) and [DIALOGUE_SUBTYPES.md](../DIALOGUE_SUBTYPES.md).

New here? Read [ARCHITECTURE.md](ARCHITECTURE.md) first, then [DEVELOPMENT.md](DEVELOPMENT.md).

## By Task (What Are You Trying To Do?)

| Task | Start Here | Also See |
|------|------------|----------|
| Understand how the pieces fit together | [ARCHITECTURE.md](ARCHITECTURE.md) | [DIALOGUE_HOOKS.md](DIALOGUE_HOOKS.md), [DATABASE.md](DATABASE.md) |
| Build, deploy, and check a change in game | [DEVELOPMENT.md](DEVELOPMENT.md#build-script) (`Build_Local.ps1`) | [WEB_UI.md](WEB_UI.md) |
| Package a release zip | [DEVELOPMENT.md](DEVELOPMENT.md#releases) (`Build_Release.ps1`) | |
| Support a new Skyrim runtime or bump CommonLib | [DEVELOPMENT.md](DEVELOPMENT.md#engine-touchpoints) | [DIALOGUE_HOOKS.md](DIALOGUE_HOOKS.md#hook-table) |
| Debug why a line did or didn't play | [DIALOGUE_HOOKS.md](DIALOGUE_HOOKS.md#log-lines-to-grep-for) | [BLOCKING_RULES.md](BLOCKING_RULES.md), [DEVELOPMENT.md](DEVELOPMENT.md#logging) |
| Change how the block decision is made (precedence, whitelist, filters) | [BLOCKING_RULES.md](BLOCKING_RULES.md) | [DATABASE.md](DATABASE.md), [DIALOGUE_HOOKS.md](DIALOGUE_HOOKS.md) |
| Change soft/hard block mechanics (audio, subtitles, animation) | [DIALOGUE_HOOKS.md](DIALOGUE_HOOKS.md#soft-block-vs-hard-block-at-this-layer) | [BLOCKING_RULES.md](BLOCKING_RULES.md) |
| Add or remove a pre-included scene | [SCENE_BLOCKING.md](SCENE_BLOCKING.md#how-to-add-or-remove-a-pre-included-scene) | [SCENE_BLOCKING.md](SCENE_BLOCKING.md#curation-rule-for-the-pre-included-list) |
| Add a scene or filter category | [SCENE_BLOCKING.md](SCENE_BLOCKING.md#how-to-add-a-new-scene-category) | [BLOCKING_RULES.md](BLOCKING_RULES.md), [CONFIG_AND_SETTINGS.md](CONFIG_AND_SETTINGS.md) |
| Add a subtype toggle | [BLOCKING_RULES.md](BLOCKING_RULES.md#subtypes) | [CONFIG_AND_SETTINGS.md](CONFIG_AND_SETTINGS.md), [WEB_UI.md](WEB_UI.md) |
| Add a setting end to end (INI, global, menu, MCM) | [CONFIG_AND_SETTINGS.md](CONFIG_AND_SETTINGS.md) | [PRISMA_UI_BRIDGE.md](PRISMA_UI_BRIDGE.md), [WEB_UI.md](WEB_UI.md) |
| Work on YAML import or subtype overrides | [CONFIG_AND_SETTINGS.md](CONFIG_AND_SETTINGS.md#yaml-import) | [BLOCKING_RULES.md](BLOCKING_RULES.md) |
| Work on the MCM | [CONFIG_AND_SETTINGS.md](CONFIG_AND_SETTINGS.md#the-mcm-sourcescriptsstfu_mcmpsc) | |
| Add a DB column or query the live database | [DATABASE.md](DATABASE.md) | [DATABASE.md](DATABASE.md#how-to-inspect-the-live-db) |
| Work on history logging (what gets recorded, dedup) | [DIALOGUE_HOOKS.md](DIALOGUE_HOOKS.md) | [DATABASE.md](DATABASE.md#write-queue-and-threading) |
| Add a menu action (JS ↔ C++) | [PRISMA_UI_BRIDGE.md](PRISMA_UI_BRIDGE.md#how-to-add-a-new-ui-action-end-to-end) | [WEB_UI.md](WEB_UI.md) |
| Look up a listener name or its payload | [PRISMA_UI_BRIDGE.md](PRISMA_UI_BRIDGE.md#message-protocol) | [WEB_UI.md](WEB_UI.md#how-data-arrives-from-c) |
| Change the React menu | [WEB_UI.md](WEB_UI.md) | [PRISMA_UI_BRIDGE.md](PRISMA_UI_BRIDGE.md) |
| Understand compatibility with Dynamic Dialogue Replacer | [DIALOGUE_HOOKS.md](DIALOGUE_HOOKS.md#known-issues) | |
| Pick up a known bug or cleanup task | [KNOWN_ISSUES.md](KNOWN_ISSUES.md) | |

## By System

### Runtime
| Document | Description |
|----------|-------------|
| [ARCHITECTURE.md](ARCHITECTURE.md) | Repo layout, component map, startup sequence, one line of dialogue end to end, threading, dependencies, the "actually spoken" limitation |
| [DIALOGUE_HOOKS.md](DIALOGUE_HOOKS.md) | PopulateTopicInfo, ConstructResponse/SetSubtitle and DialogueItem::Ctor hooks; soft vs hard block; history-logging gates; log tags |
| [BLOCKING_RULES.md](BLOCKING_RULES.md) | Entry types, block types, filter categories, evaluation order and precedence, subtypes, actor/faction matching |
| [SCENE_BLOCKING.md](SCENE_BLOCKING.md) | Condition injection into scene phases, per-category gate globals, deferred patching, bard songs, the curation rule |

### Data and configuration
| Document | Description |
|----------|-------------|
| [DATABASE.md](DATABASE.md) | SQLite schema, schema evolution, write queue, history retention, API, inspecting the live DB |
| [CONFIG_AND_SETTINGS.md](CONFIG_AND_SETTINGS.md) | Settings reference (INI key, global, default), source of truth between INI/globals/DB, YAML import, MCM |

### In-game menu
| Document | Description |
|----------|-------------|
| [PRISMA_UI_BRIDGE.md](PRISMA_UI_BRIDGE.md) | C++ side of the menu: lifecycle, threading, the full JS ↔ C++ message protocol, JSON helpers |
| [WEB_UI.md](WEB_UI.md) | React/Vite/zustand app: structure, stores, shared components, build and deploy |

### Development
| Document | Description |
|----------|-------------|
| [DEVELOPMENT.md](DEVELOPMENT.md) | Build, deploy, logging, verifying in game, engine touchpoints (Address Library IDs and offsets), conventions, gotchas |
| [KNOWN_ISSUES.md](KNOWN_ISSUES.md) | Backlog of known bugs, performance issues and cleanup, found while writing these docs |

## Ground rules for changing STFU

- **Verify with a log before fixing.** The dialogue path is fragile and many heuristics overlap. Reproduce the problem, read a log (`stfu_debug.flag` for debug level) that shows it, then change code. See [DEVELOPMENT.md](DEVELOPMENT.md#verifying-a-change-in-game).
- **Hard block stops scripts.** Only purely ambient content gets hard-blocked. A scene that runs scripts, changes quest state or forces packages must be allowed to play. See [SCENE_BLOCKING.md](SCENE_BLOCKING.md#curation-rule-for-the-pre-included-list).
- **No data migrations.** Schema changes add columns; existing rows are not rewritten. See [DATABASE.md](DATABASE.md#how-to-add-a-column-no-migrations-policy).
- **There is no automated test suite.** Changes are verified in game through `STFU.log`.
- **These docs describe the code as of 2026-10-04 (v1.2.1).** Line numbers drift; function names are the durable anchor. When code and a doc disagree, the code wins. Fix the doc in the same change.
