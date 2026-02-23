# Actor Speech Hook Research

## Goal
Find Actor functions that:
1. Fire when NPCs speak dialogue
2. Provide access to TESTopic/TESTopicInfo
3. Can be blocked to prevent audio/subtitle WITHOUT breaking scripts

## Hooks Installed

### Hook 1: Actor::SetDialogueWithPlayer (vtable 0x40)
**Purpose:** Detects when NPC initiates dialogue with player
**Parameters:**
- `bool a_flag` - Dialogue flag
- `bool a_forceGreet` - Force greeting
- `TESTopicInfo* a_topicInfo` - The specific dialogue line

**Expected Output:** 
- Speaker name
- TopicInfo FormID
- Topic EditorID
- Topic Subtype

**Timing:** Unknown - need to compare with DialogueItem::Ctor timestamp

---

### Hook 2: Actor::UpdateInDialogue (vtable 0x4B)
**Purpose:** Updates dialogue state during conversation
**Parameters:**
- `DialogueResponse* a_response` - Contains dialogue text and voice sound
- `bool a_unused`

**Expected Output:**
- Speaker name
- Response text (subtitle text)
- Voice sound descriptor EditorID
- Voice keyword

**Timing:** Unknown - might fire during menu selection or during voice playback

**Note:** DialogueResponse does NOT have TESTopicInfo - we'd need to correlate with other hooks to identify the topic

---

### Hook 3: Actor::Q SpeakingDone (vtable 0x107)
**Purpose:** Check if actor has finished speaking
**Returns:** `bool` - true when finished speaking

**Expected Output:**
- Only logs when returning `true`
- Speaker name

**Timing:** Fires AFTER dialogue voice finishes

---

### Hook 4: Actor::SetSpeakingDone (vtable 0x108)
**Purpose:** Set actor's speaking state
**Parameters:**
- `bool a_set` - Speaking done state

**Expected Output:**
- Speaker name
- State being set (true/false)

**Timing:** Uncertain - might mark start OR end of speech

---

## Test Case

**NPC:** Lydia (or any follower)
**Dialogue:** "Lead the way." (FormID: 0x000B0EE9)
**Subtype:** Idle (34)

**Testing Steps:**
1. Launch game with updated plugin
2. Approach Lydia
3. Trigger command dialogue ("I need you to do something")
4. Select "Follow me"  
5. Lydia responds "Lead the way."

**What to Look For:**

**Timing Order:**
- Which hook fires FIRST?
- Which fires closest to DialogueItem::Ctor?
- Which fires BEFORE subtitle appears?
- Which fires BEFORE voice plays?

**Topic Information:**
- Which hooks have TESTopicInfo access?
- Which hooks have TESTopic access?
- Can we identify FormID 0x000B0EE9?
- Can we identify Subtype 34?

**Blocking Test:**
- If we return early from SetDialogueWithPlayer, does it block voice?
- If we return early from UpdateInDialogue, does it block voice?
- If we modify SetSpeakingDone, does it affect anything?

---

## Expected Log Output

```
[timestamp] [info] Installing Actor::Say research hooks...
[timestamp] [info] ✓ Hooked SetDialogueWithPlayer (0x40)
[timestamp] [info] ✓ Hooked UpdateInDialogue (0x4B)
[timestamp] [info] ✓ Hooked QSpeakingDone (0x107)
[timestamp] [info] ✓ Hooked SetSpeakingDone (0x108)
[timestamp] [info] All Actor::Say research hooks installed!

[... in-game dialogue ...]

[timestamp] [info] === SetDialogueWithPlayer HOOK ===
[timestamp] [info] Speaker: Lydia
[timestamp] [info] Flag: true, ForceGreet: false
[timestamp] [info] TopicInfo FormID: 0x000B0EE9
[timestamp] [info] Topic EditorID: (something)
[timestamp] [info] Topic Subtype: 34

[timestamp] [info] === UpdateInDialogue HOOK ===
[timestamp] [info] Speaker: Lydia
[timestamp] [info] Response Text: Lead the way.
[timestamp] [info] Voice Sound: (EditorID)
[timestamp] [info] Voice Keyword: (keyword)

[timestamp] [HOOK] DialogueItem::Ctor - DIALOGUE STARTING!
[timestamp] [SPEAKER] Lydia
[timestamp] [TOPIC INFO] FormID: 0x000B0EE9

[timestamp] [info] === SetSpeakingDone HOOK === Lydia set to false
[... voice plays ...]
[timestamp] [info] === SetSpeakingDone HOOK === Lydia set to true
[timestamp] [info] === QSpeakingDone HOOK === Lydia finished speaking
```

---

## Success Criteria

✅ **BEST CASE:** SetDialogueWithPlayer or UpdateInDialogue fires BEFORE DialogueItem::Ctor
- Has TESTopicInfo access
- Can block voice/subtitle by returning early
- Scripts still execute (follower command works)

⚠️ **ACCEPTABLE:** Hook fires slightly AFTER DialogueItem::Ctor but BEFORE voice plays
- Can still block audio if we act fast enough

❌ **NOT USEFUL:** Hook fires AFTER voice starts playing
- Too late to block without janky stop

---

## Next Steps After Testing

1. **If SetDialogueWithPlayer works:**
   - Test blocking by returning `false`
   - Verify audio/subtitle blocked
   - Verify scripts execute (follower obeys command)
   - Implement STFU configuration integration

2. **If UpdateInDialogue works:**
   - Test blocking by returning `false`
   - Challenge: No direct TESTopicInfo access
   - Would need to correlate DialogueResponse text with topic

3. **If neither works early enough:**
   - Research other Actor virtual functions
   - Look for HighProcessData voice state functions
   - Consider ExtraSayToTopicInfo creation hook

4. **If blocking breaks scripts:**
   - Research how to separate voice playback from script execution
   - Might need to call original but stop audio handle afterward
