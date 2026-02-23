# Quick Testing Guide - Actor::Say Hooks

## What's New
Added 4 Actor virtual function hooks to research when/how dialogue voice playback occurs.

## Location
**Plugin DLL**: `STFU\SKSE\Plugins\DialogueInterceptor.dll` (already built and installed)
**Log File**: `Documents\My Games\Skyrim Special Edition\SKSE\DialogueInterceptor.log`

## How to Test

### 1. Copy Plugin to Game Folder
```powershell
Copy-Item "c:\Nolvus\Instances\Nolvus Awakening\MODS\mods\STFU\SKSE\Plugins\DialogueInterceptor.dll" `
          "c:\Nolvus\Instances\Nolvus Awakening\STOCK GAME\Data\SKSE\Plugins\DialogueInterceptor.dll"
```

### 2. Launch Game
- Start Skyrim through Mod Organizer 2 (or your loader)
- Load a save with Lydia as follower (or any NPC)

### 3. Trigger Dialogue
**Test dialogue types:**
- **Idle chitchat**: Lydia's random comments while following
- **Command dialogue**: "I need you to do something" → "Follow me" / "Wait here"  
- **Greetings**: Just walk near Lydia, wait for her hello
- **Conversation**: Initiate full dialogue menu

### 4. Check Log
Open: `%USERPROFILE%\Documents\My Games\Skyrim Special Edition\SKSE\DialogueInterceptor.log`

**Look for:**
```
=== SetDialogueWithPlayer HOOK ===
=== UpdateInDialogue HOOK ===
=== SetSpeakingDone HOOK ===
=== QSpeakingDone HOOK ===
```

### 5. Compare Timestamps
**Key Question:** Which hook fires BEFORE `DialogueItem::Ctor`?

Example:
```
[12:45:30.123] === SetDialogueWithPlayer HOOK ===    ← Hook 1
[12:45:30.125] [HOOK] DialogueItem::Ctor             ← Existing hook
```

If Hook 1 timestamp < DialogueItem::Ctor timestamp = **WINNER!** 🎯

## What We're Looking For

### 🎯 Best Case Hook Properties:
1. **Fires BEFORE voice plays**
2. **Has TESTopicInfo access** (can identify dialogue line)
3. **Can be blocked** without breaking scripts
4. **Fires consistently** for all dialogue types

### 📊 Useful Info to Note:
- Hook _call order (1, 2, 3, 4?)
- Which hooks have topic/topicInfo data
- Which hooks fire multiple times per line
- Any crashes or instability

## Example Scenarios

### ✅ Success Scenario
```
[12:45:30.100] === SetDialogueWithPlayer HOOK ===
[12:45:30.100] Speaker: Lydia
[12:45:30.100] TopicInfo FormID: 0x000B0EE9
[12:45:30.100] Topic Subtype: 34
[12:45:30.125] [HOOK] DialogueItem::Ctor
[12:45:30.200] [voice plays]
[12:45:30.500] === SetSpeakingDone HOOK === Lydia set to true
```
✅ SetDialogueWithPlayer fires 25ms before DialogueItem::Ctor = TARGET FOUND!

### ⚠️ Acceptable Scenario
```
[12:45:30.100] [HOOK] DialogueItem::Ctor
[12:45:30.150] === UpdateInDialogue HOOK ===
[12:45:30.150] Speaker: Lydia
[12:45:30.150] Response Text: Lead the way.
[12:45:30.200] [voice plays]
```
⚠️ UpdateInDialogue fires 50ms after DialogueItem::Ctor but 50ms before voice = Might work!

### ❌ Too Late Scenario
```
[12:45:30.100] [HOOK] DialogueItem::Ctor
[12:45:30.150] [voice plays]
[12:45:30.200] === UpdateInDialogue HOOK ===
```
❌ Hook fires AFTER voice starts = Not useful for blocking

## Report Back

Share in log:
- Timestamps of each hook type
- Which hook(s) show topic information
- Whether any hooks fire before DialogueItem::Ctor
- Any crashes or instability

Most important: **Paste a sample of the log showing all 5 hooks** (4 new + 1 existing) for a single dialogue line.
