# Dialogue Subtypes Reference

Skyrim organizes dialogue topics into various subtypes based on the conditions in which they happen. These can subtypes can be used to create a broad filters encompassing thousands of responses all with the same behavior.

This document lists all dialogue subtypes used in Skyrim's dialogue system, organized by category. Subtype names match those used in YAML configuration files. These subtypes determine when and how NPCs speak during various situations. 

There are more subtypes than this but not all are safe to block. For example, "Rumors" isn't background dialogue, but the dialogue responses when you ask an innkeeper if they have heard any rumors, which is necessary for starting some quests. Most notably, Custom and Scene subtypes are very important for game functions.

## Combat Dialogue

**Attack:** "I've had enough of you!", "Die, beast!", "You can't win this!". NPCs shouting as they initiate or execute attacks.

**Hit:** "Do your worst!", "That your best? Huh?", "Damn you!". Reactions when the NPC takes damage or is hit by the player.

**PowerAttack:** Only grunts by default, mods might add dialogue. Vocalizations during power attacks.

**Death:** "Thank you.", "Free...again.", "At last.". Final words spoken when an NPC dies.

**Block:** "Close...", "Easily blocked!", "Oho! Not so fast!". NPC successfully blocks an attack.

**Bleedout:** "Help me...", "I can't move...", "Someone...". NPC enters bleedout/downed state (essential NPCs only).

**Bash:** "Hunh!", "Gah!", "Yah!", "Rargh!". Shield bash or bash attack vocalizations (vanilla is only grunts, mods may add dialogue).

**AllyKilled:** "No!! How dare you!", "Why, why?!", "Gods, no!". NPC witnesses an ally's death.

**Taunt:** "Skyrim belongs to the Nords!", "I'll rip your heart out!", "Prepare to die!". Aggressive taunting during combat.

## Combat State Transitions

**NormalToCombat:** "You're dead!", "By Ysmir, you won't leave here alive!". NPC detects enemy and enters combat.

**CombatToNormal:** "That takes care of that.", "All over now.". NPC exits combat after defeating enemies or losing sight of them.

**CombatToLost:** "Is it safe?", "Where are you hiding?". NPC loses track of enemy during combat.

**NormalToAlert:** "Did you hear something?", "Hello? Who's there?", "Is... is someone there?". NPC becomes suspicious of possible threats.

**AlertToCombat:** "Now you're mine!", "Time to end this little game!", "I knew it!". NPC transitions from alert to active combat.

**AlertToNormal:** "Must have been nothing.", "Mind's playing tricks on me...". NPC dismisses alert and returns to normal behavior.

**AlertIdle:** "Anybody there?", "Hello? Who's there?", "I know I heard something.". NPC searching while in alert state.

**LostIdle:** "I'm going to find you.", "You afraid to fight me?". NPC searching for lost enemy.

**LostToCombat:** "There you are.", "I knew I'd find you!". NPC reacquires enemy after losing them.

**LostToNormal:** "Oh well. Must have run off.", "Nobody here anymore.". NPC gives up searching and returns to normal.

## Combat Reactions

**ObserveCombat:** "Gods! Another fight.", "Somebody help!", "I'm getting out of here!". Non-combatant NPCs reacting to nearby combat.

**Flee:** "Ah! It burns!", "You win! I submit!", "Aaaiiee!". NPC attempting to escape from combat.

**AvoidThreat:** "I need to get away!", "Too dangerous!", "Back off!". NPC avoiding dangerous situations or threats.

**Yield:** "I yield! I yield!", "Please, mercy!", "I give up!". NPC surrendering during combat.

**AcceptYield:** "I'll let you live. This time.", "You're not worth it.", "All right, you've had enough.". NPC accepting another's surrender.

**PickpocketCombat:** NPCs reacting to pickpocket attempts during combat.

**DetectFriendDie:** NPCs reacting when they detect a friend has died.

## Voice Powers (Shouts)

**VoicePowerStartShort:** Short shout starting dialogue. "Fus!"

**VoicePowerStartLong:** Long shout starting dialogue. "Fus..."

**VoicePowerEndShort:** Short shout ending dialogue. "Ro!"

**VoicePowerEndLong:** Long shout ending dialogue. "Ro Dah!"

## Non-Combat Dialogue

**Hello:** Greetings from NPCs. Standard NPC greetings when you pass by or initiate conversation.

**Goodbye:** Farewells when ending conversations. NPCs bid farewell at the end of dialogue.

**Idle:** Random comments NPCs make to themselves. Ambient chatter NPCs say to themselves or nearby NPCs.

## Crime Reactions

**Murder:** Civilian NPCs reacting to witnessing murder. Witnessing NPC reactions when someone is killed.

**MurderNC:** Murder reactions outside crime system. Murder reactions that don't trigger the crime system.

**Assault:** Civilian NPCs reacting to witnessing assault. Witnessing NPC reactions when someone is attacked.

**AssaultNC:** Assault reactions outside crime system. Assault reactions that don't trigger the crime system.

**Steal:** NPCs reacting to you stealing items. Witnessing NPCs call out theft.

**StealFromNC:** NPCs commenting on theft outside crime system. Theft comments that don't trigger crime.

**PickpocketTopic:** Dialogue about pickpocketing attempts. NPCs catching you trying to pickpocket.

**PickpocketNC:** Pickpocket comments outside crime system. Pickpocket reactions that don't trigger crime.

**Trespass:** NPCs warning you about entering restricted areas. Warnings when entering forbidden locations.

**TrespassAgainstNC:** Trespass warnings outside crime system. Trespass comments that don't trigger crime.

**WerewolfTransformCrime:** NPCs reacting to werewolf transformations. Civilian reactions to witnessing werewolf transformation.

## Environmental Reactions

**LockedObject:** NPCs commenting when you try to open locked containers/doors.

**DestroyObject:** NPCs reacting to you destroying objects.

**KnockOverObject:** NPCs commenting when you knock things over.

**StandOnFurniture:** NPCs commenting when you stand on furniture.

**ActorCollideWithActor:** NPCs reacting when you bump into them.

**NoticeCorpse:** NPCs reacting to seeing dead bodies.

**ZKeyObject:** NPCs commenting when you pick up objects through the physics system (Z key).

## Player Action Reactions

**PlayerShout:** NPCs reacting to you using shouts.

**PlayerInIronSights:** NPCs reacting to you aiming at them with ranged weapons.

**PlayerCastProjectileSpell:** NPCs reacting to you casting projectile spells near them.

**PlayerCastSelfSpell:** NPCs reacting to you casting spells on yourself.

**SwingMeleeWeapon:** NPCs reacting to you swinging weapons near them.

**ShootBow:** NPCs reacting to you shooting arrows near them.

## Transaction Endings

**TimeToGo:** NPCs telling you to leave or wrapping up. Generic "time to leave" dialogue.

**BarterExit:** Merchants' closing remarks when ending trade. Merchant goodbye after shopping.

**TrainingExit:** Trainer dialogue when exiting training menu. Trainer farewell after training session.

## Follower Dialogue

**MoralRefusal:** Follower refuses requests that conflict with their morals. Example: Refusing to commit crimes or attack innocents.

**ExitFavorState:** Follower dialogue when ending favor/command mode. When you tell a follower to stop waiting or following.

**Agree:** Follower agreeing to requests or commands. Acknowledging orders like "wait here" or "carry this."

**Show:** Follower ready for a command. Generic "what do you need?" follower dialogue.

**Refuse:** Follower refusing general requests. General refusals (not moral-based).