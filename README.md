# S.T.F.U - Skyrim Talk Filter Utility
Skyrim Talk Filter Utility silences tedious frequent utterances

## What This Does
&emsp;Skyrim separates dialogue into topics, which are categorized with subtypes like Hello, KnockOverObject, CombatToNormal, Taunts, etc. This mod uses the power of Mutagen/Synthesis to scan your load order and patch those dialogue responses with a condition that must be true in order to play. The patcher copies the responses into a new ESP that overrides the originals without tampering with them. The conditions can be toggled on or off via the MCM to allow or block dialogue, broken down into 60+ categories based on subtype.

&emsp;Highly customizable with whitelists, blacklists, subtype overrides, and safe mode settings. Only block what you want blocked. This mod does not interfere with dialogue trees or quest dialogue (by default). The subtypes this mod blocks are ambient/generic dialogue that gets triggered automatically under normal conditions. S.T.F.U is lightweight, the only script is for the MCM toggles. Can safely be added or removed at any time.


### Blocked Dialogue Types
- **Combat Dialogue**: Taunt, CombatToNormal, Hit, AlertIdle, Yield, and many more
  - There is a separate toggle for grunt sounds so combat isn't awkwardly silent
- **Generic Dialogue**: Idle, KnockOverObject, ActorCollideWithActor, Hello, Goodbye, and many more
- **Follower Dialogue**: Follower commentary and favor reactions (when you hold E to command)
- **Other**: Curated list of safe to block repetitive scenes, bard songs, and a custom user blacklist

This mod does **NOT** block dialogue with the Scene or Custom subtypes, as these are important for game functions and quests.

## Requirements
- **[SkyUI](https://www.nexusmods.com/skyrimspecialedition/mods/12604)** (for MCM)
- **[Synthesis](https://github.com/Mutagen-Modding/Synthesis/releases)** (patcher framework)
- **[.NET 8.0 Runtime](https://dotnet.microsoft.com/download/dotnet/8.0)** or newer

## Installation

### Step 1: Install the Mod
1. Install **STFU** via your mod manager
2. Enable the mod
3. **Important**: Place `STFU.esp` and any mods with dialogue **ABOVE** any other Synthesis patches in your load order (`Synthesis.esp` for example)
   - If `STFU.esp` is after another patch, the patcher won't detect it

### Step 2: Add Patcher to Synthesis
1. Launch Synthesis through your mod manager
2. (Optional)*: Create a new group for the patcher called "STFU_Patch" or similar
3. Click **"External Program"** in the top left corner
4. Browse to STFU.exe in the "STFU Patcher" folder in the mod directory
5. Click **"Confirm"**
6. (Optional)*: Drag the patcher into your new group
7. Run (this will create a new ESP)

*You will need to rerun the patcher whenever you add or remove mods containing dialogue. If you have multiple patchers all in one group, you will have to rerun all of them each time.

### Step 3: Enable the new ESP in your mod manager
1. Find the new ESP and enable it (it will be named after the Synthesis group)
2. Load the new ESP after STFU.esp and any mods containing dialogue

**Done**

## Configuration
All config files are in `\STFU Patcher\Config\`:

### STFU_Config.json
- **vanillaOnly**: if true, the patcher will skip any modded topics and only patch Skyrim.esm, Dawnguard.esm, Dragonborn.esm, Hearthfires.esm, and Update.esm topics. Mods that alter and override vanilla topics will still have those responses patched.

- **safeMode**: if true, the patcher will skip any responses that have scripts attached
  - Enabling this will result in blocking ~40% less dialogue and might make whatever doesn't get patched more repetitive due to reduced variety. The vast majority if not all scripts attached to these subtypes are harmless
  - Important dialogue *should* use the Custom or Scene subtypes, which this mod doesn't touch
  - The only problem I've had is with The Great Cities mods Hello scripts for triggering the home buying quest (now whitelisted)

- **filterHello**: if false, that subtype won't be processed by STFU at all
  - I haven't had any mechanical issues blocking Hello, but sometimes dialogue tree options don't make sense without the initial greeting
  
- **filterX**: same as above for every subtype

### STFU_Blacklist.yaml
Blacklist specific dialogue topics or scenes:
```yaml
# Blacklist - Topics that will be blocked when MCM toggle is OFF
# Supports both FormKeys (with :) and Editor IDs. Editor IDs are recommended for ESPFE plugins.
# For any ESP or Editor ID with special YAML characters like [, ], {, }, :, #, @, &, *, etc., use quotes
topics:
 - 021405:Skyrim.esm #DialogueGenericPoisonCoughBranchTopic "Cough"
 
plugins: #Block every dialogue response in a plugin. Probably not a good idea.
  - "This is a bad idea.esp"
  
#---------------------------
#      Scene blocking
#---------------------------
#Use carefully, this blocks scenes from playing entirely and may break quests that rely on them.
scenes:
  - WhiterunMikaelSongScene
  # nwsFollowerFramework.esp
  - nwsFollowerLeveledScene #Congratulating
  
quests: #Block every scene referenced by a quest
  # FaceSculptorExpanded.esp scenes
  - FaceSculptor_Events

quest_patterns: #EditorIDs only. Use * wildcard to catch multiple quests with similar naming schemes
  # Companions Dialogue Bundle.esp
  - "_JQ_CompanionsScenes*"
```

### STFU_Whitelist.yaml
**Never patch** specific dialogue (protection list):
```yaml
# Whitelist - Topics that will NEVER be touched by the patcher
# Use this to exclude specific dialogue you want to keep
# Supports both FormKeys and Editor IDs. Editor IDs are recommended for ESPFE plugins
# For any ESP or Editor ID with special YAML characters like [, ], {, }, :, #, @, &, *, etc., use quotes
topics:
  # Example: Editor ID
  - WEJS27Hello #Taunting adventurer greeting
  # Example FormKey
  - 062103:Skyrim.esm #Scavenger encounter dialogue
  - 04C592:Skyrim.esm #Draugr Attack dialogue (shouts and laughs)
  - 04C2D2:Skyrim.esm #Ayarg garag gar! (giants)
  # A Cat's Life cat noises
  - ACLDialogueCatsHello
  - ACLCatDialogueFollow
  - ACLCatDialogueBranchReFollowTopic
  - ACLCatDialogueBranchMortalityTopic
  - ACLCatDialogueAdopt
  - ACLCatDialogueDismiss
  - ACLCatDialogueAbandonReject
  - ACLCatDialogueWait
  - ACLCatDialogueCommand
  - ACLDialogueCatsHit
  - CLCatDialogueAssignHome
  - ACLCatDialoguePet
  - ACLCatDialogueAbandonConfirm
  - ACLCatDialogueGiveYarnTopic
  - ACLDialogueCatsMiscSwingWeapon
  - ACLDialogueCatsMiscBow
  - ACLDialogueCatsMiscShout
  - ACLDialogueCatsFlee
  - ACLDialogueCatsMiscCollide

plugins:
  # Example: Don't filter any dialogue from Skyrim on Skooma
  - "Skyrim On Skooma.esp"
  
scenes:
  - 

quests: 
  # Great Cities (houses won't be purchasable if Hello dialogue doesn't trigger quest)
  - WinterholdBuyHomeTGCoWH
  - DialogueKynesgroveBuyHomeTGCoKG
quest_patterns:
 - 
```

### STFU_SubtypeOverrides.yaml
Override subtype classification for specific topics. Originally to fix miscategorized dialogue, it can also be used to blacklist a topic by assigning it to a specific subtype's MCM toggle instead of the blacklist MCM toggle:
```yaml
# Subtype Overrides - Manually correct miscategorized dialogue
# Format: key: subtype
# Key can be FormKey or Editor ID. Editor IDs are recommended for ESPFE plugins.
# For any ESP or Editor ID with special YAML characters like [, ], {, }, :, #, @, &, *, etc., use quotes
overrides:
  021405:Skyrim.esm: Hit #Patches the coughing topic to be filtered with the Hit condition
```

After editing configs, re-run Synthesis to apply changes.

## In-Game MCM
Open the **S.T.F.U** MCM menu to toggle dialogue types on/off in real-time:

- **Combat Dialogue**: Attack, Hit, Taunt, Death, Block, Grunts, etc
- **Generic Dialogue**: Hello, Goodbye, Idle, KnockOverObject, etc
- **Follower Dialogue**: Follower Commentary, Agree, Refuse, ExitFavorState, etc
- **Other Dialogue**: Bard songs, scenes*, and blacklist

*To be safe only toggle scenes when there aren't any scenes running to prevent them from potentially getting stuck (unsure if this is an issue, Skyrim has a built in timeout I think)

No patcher re-run needed - changes apply immediately

## Troubleshooting

### Missing globals, topics aren't patched
  **Problem**: STFU.esp not detected by Synthesis

  **Solution**: 
  1. Check that `STFU.esp` is enabled in your mod manager
  2. Move STFU.esp **ABOVE** any other Synthesis patches in load order
  3. Re-run Synthesis

### Config changes not working
  After editing YAML/JSON files, you must **re-run Synthesis** to regenerate the patch.

### Dialogue I want is blocked
  Add it to `STFU_Whitelist.yaml` by FormID or EditorID, then re-run Synthesis.

### Dialogue I don't want is NOT blocked
  Add it to `STFU_Blacklist.yaml` by FormID or EditorID, then re-run Synthesis. Look below for how to locate dialogue's FormID or EditorID.

### Patch isn't being updated when re-running Synthesis
  Delete the old patch's ESP and run it again.

### S.T.F.U MCM is loading but nothing is being blocked
  Make sure the patch's ESP is enabled and below any mods with dialogue

### Can't find the patch ESP after running
  It should be named either `Synthesis.esp` by default or whatever you named the group (`STFU_Patch.esp` for example). Open Synthesis and check the group names on the left and find which one contains STFU. If you have multiple Synthesis patchers in the same group they will all be in the same ESP. Also confirm that the patcher completed successfully and didn't give an error in the output window.

## Useful Tools
You can use these to find the EditorID or FormID of dialogue topics so they can be added to configuration yamls. EditorIDs are better, but FormIDs will work fine for non-ESL flagged mods.
- [xTranslator](https://www.nexusmods.com/skyrimspecialedition/mods/134) Useful for quickly searching through dialogue to find what you want blacklisted or whitelisted
- [xEdit](https://www.nexusmods.com/skyrimspecialedition/mods/164) More detailed information than xTranslator but slower to search
  - [Dialogue Search Scipt](https://gist.github.com/tasairis/51e530a1af9e8a4be089328376e41108)

## Reporting Issues
If you encounter any vanilla dialogue not being blocked when it should, send me the quote and the context that it happened. I won't make patches for modded dialogue, that should be done with the yaml configs.

If you suspect that blocked dialogue broke a quest, please try reloading a save and testing again before blaming me. It's possible a blocked dialogue script somewhere is important, but Skyrim quests can break for any number of reasons.

Ideal test flow:
  - Test quest with STFU enabled --> Quest doesn't work
  - Reload and test quest AGAIN with STFU enabled
    1. Quest works, not my fault
    2. Quest still doesn't work, continue
  - Reload and test quest AGAIN with STFU disabled
    1. Quest still doesn't work, not my fault
    2. Quest works, continue
  - Let me know the quest name or editor ID (preferable) and I'll check what it's scripts are (if any)

Conversely, don't blame other mod authors if blocking dialogue breaks their quests, it might be my fault.

If dialogue you want to hear is being blocked, whitelist it or disable that subtype, leave me alone. You know what you signed up for.

If the patcher gives an error when running, close Synthesis and try again, then check the troubleshooting section. If it's still not working send me the log from the Synthesis output window.

Submit a [github issue](https://github.com/zevck/S.T.F.U/issues) if you think something is wrong

## Credits
- Mutagen/Synthesis Framework