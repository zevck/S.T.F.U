Scriptname STFU_MCM extends SKI_ConfigBase

; Config file tracking
int configHandle = 0
bool configLoaded = false

; Track which subtypes are enabled in config (false = hidden in MCM)
bool filterAttack = true
bool filterHit = true
bool filterPowerAttack = true
bool filterDeath = true
bool filterBlock = true
bool filterBleedout = true
bool filterBash = true
bool filterAllyKilled = true
bool filterTaunt = true
bool filterNormalToCombat = true
bool filterCombatToNormal = true
bool filterCombatToLost = true
bool filterNormalToAlert = true
bool filterAlertToCombat = true
bool filterAlertToNormal = true
bool filterAlertIdle = true
bool filterLostIdle = true
bool filterLostToCombat = true
bool filterLostToNormal = true
bool filterObserveCombat = true
bool filterFlee = true
bool filterAvoidThreat = true
bool filterYield = true
bool filterAcceptYield = true
bool filterPickpocketCombat = true
bool filterDetectFriendDie = true
bool filterPreserveGrunts = true
bool filterMoralRefusal = true
bool filterExitFavorState = true
bool filterAgree = true
bool filterShow = true
bool filterRefuse = true
bool filterFollowerCommentary = true
bool filterBardSongs = true
bool filterScenes = true
bool filterBlacklist = true
bool filterSkyrimNetFilter = true
bool filterMurder = true
bool filterMurderNC = true
bool filterAssault = true
bool filterAssaultNC = true
bool filterActorCollideWithActor = true
bool filterBarterExit = true
bool filterDestroyObject = true
bool filterGoodbye = true
bool filterHello = true
bool filterKnockOverObject = true
bool filterNoticeCorpse = true
bool filterPickpocketTopic = true
bool filterPickpocketNC = true
bool filterLockedObject = true
bool filterTrespass = true
bool filterTimeToGo = true
bool filterIdle = true
bool filterStealFromNC = true
bool filterTrespassAgainstNC = true
bool filterPlayerShout = true
bool filterPlayerInIronSights = true
bool filterPlayerCastProjectileSpell = true
bool filterPlayerCastSelfSpell = true
bool filterSteal = true
bool filterSwingMeleeWeapon = true
bool filterShootBow = true
bool filterStandOnFurniture = true
bool filterTrainingExit = true
bool filterZKeyObject = true
bool filterVoicePowerEndLong = true
bool filterVoicePowerEndShort = true
bool filterVoicePowerStartLong = true
bool filterVoicePowerStartShort = true
bool filterWerewolfTransformCrime = true
bool filterPursueIdleTopic = true

; Generic Combat Dialogue Globals
GlobalVariable Property STFU_Attack Auto
GlobalVariable Property STFU_Hit Auto
GlobalVariable Property STFU_PowerAttack Auto
GlobalVariable Property STFU_Death Auto
GlobalVariable Property STFU_Block Auto
GlobalVariable Property STFU_Bleedout Auto
GlobalVariable Property STFU_AllyKilled Auto
GlobalVariable Property STFU_Taunt Auto
GlobalVariable Property STFU_NormalToCombat Auto
GlobalVariable Property STFU_CombatToNormal Auto
GlobalVariable Property STFU_CombatToLost Auto
GlobalVariable Property STFU_NormalToAlert Auto
GlobalVariable Property STFU_AlertToCombat Auto
GlobalVariable Property STFU_AlertToNormal Auto
GlobalVariable Property STFU_AlertIdle Auto
GlobalVariable Property STFU_LostIdle Auto
GlobalVariable Property STFU_LostToCombat Auto
GlobalVariable Property STFU_LostToNormal Auto
GlobalVariable Property STFU_ObserveCombat Auto
GlobalVariable Property STFU_Flee Auto
GlobalVariable Property STFU_AvoidThreat Auto
GlobalVariable Property STFU_Yield Auto
GlobalVariable Property STFU_AcceptYield Auto
GlobalVariable Property STFU_Bash Auto
GlobalVariable Property STFU_PickpocketCombat Auto
GlobalVariable Property STFU_DetectFriendDie Auto
GlobalVariable Property STFU_PreserveGrunts Auto

; Follower Dialogue Globals
GlobalVariable Property STFU_MoralRefusal Auto
GlobalVariable Property STFU_ExitFavorState Auto
GlobalVariable Property STFU_Agree Auto
GlobalVariable Property STFU_Show Auto
GlobalVariable Property STFU_Refuse Auto
GlobalVariable Property STFU_FollowerCommentary Auto


; Other/Miscellaneous Globals
GlobalVariable Property STFU_BardSongs Auto
GlobalVariable Property STFU_Scenes Auto
GlobalVariable Property STFU_Blacklist Auto
GlobalVariable Property STFU_SkyrimNetFilter Auto

; Generic Dialogue Globals
GlobalVariable Property STFU_Murder Auto
GlobalVariable Property STFU_MurderNC Auto
GlobalVariable Property STFU_Assault Auto
GlobalVariable Property STFU_AssaultNC Auto
GlobalVariable Property STFU_ActorCollideWithActor Auto
GlobalVariable Property STFU_BarterExit Auto
GlobalVariable Property STFU_DestroyObject Auto
GlobalVariable Property STFU_Goodbye Auto
GlobalVariable Property STFU_Hello Auto
GlobalVariable Property STFU_KnockOverObject Auto
GlobalVariable Property STFU_NoticeCorpse Auto
GlobalVariable Property STFU_PickpocketTopic Auto
GlobalVariable Property STFU_PickpocketNC Auto
GlobalVariable Property STFU_LockedObject Auto
GlobalVariable Property STFU_Trespass Auto
GlobalVariable Property STFU_TimeToGo Auto
GlobalVariable Property STFU_Idle Auto
GlobalVariable Property STFU_StealFromNC Auto
GlobalVariable Property STFU_TrespassAgainstNC Auto
GlobalVariable Property STFU_PlayerShout Auto
GlobalVariable Property STFU_PlayerInIronSights Auto
GlobalVariable Property STFU_PlayerCastProjectileSpell Auto
GlobalVariable Property STFU_PlayerCastSelfSpell Auto
GlobalVariable Property STFU_Steal Auto
GlobalVariable Property STFU_SwingMeleeWeapon Auto
GlobalVariable Property STFU_ShootBow Auto
GlobalVariable Property STFU_StandOnFurniture Auto
GlobalVariable Property STFU_TrainingExit Auto
GlobalVariable Property STFU_ZKeyObject Auto
GlobalVariable Property STFU_VoicePowerEndLong Auto
GlobalVariable Property STFU_VoicePowerEndShort Auto
GlobalVariable Property STFU_VoicePowerStartLong Auto
GlobalVariable Property STFU_VoicePowerStartShort Auto
GlobalVariable Property STFU_WerewolfTransformCrime Auto
GlobalVariable Property STFU_PursueIdleTopic Auto

; Option IDs
int oid_EnableAll
int oid_DisableAll
int oid_Attack
int oid_Hit
int oid_PowerAttack
int oid_Death
int oid_Block
int oid_Bleedout
int oid_AllyKilled
int oid_Taunt
int oid_NormalToCombat
int oid_CombatToNormal
int oid_CombatToLost
int oid_NormalToAlert
int oid_AlertToCombat
int oid_AlertToNormal
int oid_AlertIdle
int oid_LostIdle
int oid_LostToCombat
int oid_LostToNormal
int oid_ObserveCombat
int oid_Flee
int oid_AvoidThreat
int oid_Yield
int oid_AcceptYield
int oid_Bash
int oid_PickpocketCombat
int oid_DetectFriendDie
int oid_PreserveGrunts

; Follower Dialogue Option IDs
int oid_EnableAllFollower
int oid_DisableAllFollower
int oid_MoralRefusal
int oid_ExitFavorState
int oid_Agree
int oid_Show
int oid_RefuseFollower
int oid_FollowerCommentary


; Other/Miscellaneous Option IDs
int oid_EnableAllOther
int oid_DisableAllOther
int oid_BardSongs
int oid_Scenes
int oid_Blacklist
int oid_SkyrimNetFilter

; Non-Combat Option IDs
int oid_EnableAllNC
int oid_DisableAllNC
int oid_Murder
int oid_MurderNC
int oid_Assault
int oid_AssaultNC
int oid_ActorCollideWithActor
int oid_BarterExit
int oid_DestroyObject
int oid_Goodbye
int oid_Hello
int oid_KnockOverObject
int oid_NoticeCorpse
int oid_PickpocketTopic
int oid_PickpocketNC
int oid_LockedObject
int oid_Trespass
int oid_TimeToGo
int oid_Idle
int oid_StealFromNC
int oid_TrespassAgainstNC
int oid_PlayerShout
int oid_PlayerInIronSights
int oid_PlayerCastProjectileSpell
int oid_PlayerCastSelfSpell
int oid_Steal
int oid_SwingMeleeWeapon
int oid_ShootBow
int oid_StandOnFurniture
int oid_TrainingExit
int oid_ZKeyObject
int oid_VoicePowerEndLong
int oid_VoicePowerEndShort
int oid_VoicePowerStartLong
int oid_VoicePowerStartShort
int oid_WerewolfTransformCrime
int oid_PursueIdleTopic

Event OnConfigInit()
    ModName = "S.T.F.U"
    LoadConfig()
    BuildPageList()
EndEvent

Event OnConfigOpen()
    LoadConfig()
    BuildPageList()
EndEvent

Function BuildPageList()
    ; Build page list dynamically based on which pages have enabled subtypes
    ; Count how many pages we actually need first
    int pageCount = 0
    
    If HasCombatDialogue()
        pageCount += 1
    EndIf
    
    If HasGenericDialogue()
        pageCount += 1
    EndIf
    
    If HasFollowerDialogue()
        pageCount += 1
    EndIf
    
    If HasOtherDialogue()
        pageCount += 1
    EndIf
    
    ; Create properly sized array
    Pages = new string[4]
    int currentIndex = 0
    
    If HasCombatDialogue()
        Pages[currentIndex] = "Combat Dialogue"
        currentIndex += 1
    EndIf
    
    If HasGenericDialogue()
        Pages[currentIndex] = "Generic Dialogue"
        currentIndex += 1
    EndIf
    
    If HasFollowerDialogue()
        Pages[currentIndex] = "Follower Dialogue"
        currentIndex += 1
    EndIf
    
    If HasOtherDialogue()
        Pages[currentIndex] = "Other Dialogue"
        currentIndex += 1
    EndIf
EndFunction

bool Function HasCombatDialogue()
    Return filterAttack || filterHit || filterPowerAttack || filterDeath || filterBlock || filterBleedout || \
           filterBash || filterAllyKilled || filterTaunt || filterNormalToCombat || filterCombatToNormal || \
           filterCombatToLost || filterNormalToAlert || filterAlertToCombat || filterAlertToNormal || \
           filterAlertIdle || filterLostIdle || filterLostToCombat || filterLostToNormal || \
           filterObserveCombat || filterFlee || filterAvoidThreat || filterYield || filterAcceptYield || \
           filterPickpocketCombat || filterDetectFriendDie || filterPreserveGrunts
EndFunction

bool Function HasGenericDialogue()
    Return filterHello || filterGoodbye || filterIdle || filterTrainingExit || filterMurder || \
           filterMurderNC || filterAssault || filterAssaultNC || filterSteal || filterStealFromNC || \
           filterPickpocketTopic || filterPickpocketNC || filterTrespass || filterTrespassAgainstNC || \
           filterLockedObject || filterDestroyObject || filterKnockOverObject || \
           filterStandOnFurniture || filterActorCollideWithActor || filterNoticeCorpse || \
           filterTimeToGo || filterBarterExit || filterPlayerShout || filterPlayerInIronSights || \
           filterPlayerCastProjectileSpell || filterPlayerCastSelfSpell || filterSwingMeleeWeapon || filterShootBow
EndFunction

bool Function HasFollowerDialogue()
    Return filterMoralRefusal || filterExitFavorState || filterAgree || filterShow || \
           filterRefuse || filterFollowerCommentary
EndFunction

bool Function HasOtherDialogue()
    Return filterBardSongs || filterScenes || filterBlacklist
EndFunction

Function LoadConfig()
    ; Load config from Data/STFU Patcher/Config/STFU_Config.ini
    string configPath = "Data/STFU Patcher/Config/STFU_Config.ini"
    Debug.Trace("STFU MCM: Attempting to load config from: " + configPath)
    configHandle = JValue.readFromFile(configPath)
    Debug.Trace("STFU MCM: Config handle = " + configHandle)
    
    If configHandle != 0
        configLoaded = true
        Debug.Trace("STFU MCM: Config loaded successfully")
        ; Read all filter settings from JSON (1 = true, 0 = false)
        filterAttack = JValue.solveInt(configHandle, ".filterAttack", 1) as bool
        filterHit = JValue.solveInt(configHandle, ".filterHit", 1) as bool
        filterPowerAttack = JValue.solveInt(configHandle, ".filterPowerAttack", 1) as bool
        filterDeath = JValue.solveInt(configHandle, ".filterDeath", 1) as bool
        filterBlock = JValue.solveInt(configHandle, ".filterBlock", 1) as bool
        filterBleedout = JValue.solveInt(configHandle, ".filterBleedout", 1) as bool
        filterBash = JValue.solveInt(configHandle, ".filterBash", 1) as bool
        filterAllyKilled = JValue.solveInt(configHandle, ".filterAllyKilled", 1) as bool
        filterTaunt = JValue.solveInt(configHandle, ".filterTaunt", 1) as bool
        filterNormalToCombat = JValue.solveInt(configHandle, ".filterNormalToCombat", 1) as bool
        filterCombatToNormal = JValue.solveInt(configHandle, ".filterCombatToNormal", 1) as bool
        filterCombatToLost = JValue.solveInt(configHandle, ".filterCombatToLost", 1) as bool
        filterNormalToAlert = JValue.solveInt(configHandle, ".filterNormalToAlert", 1) as bool
        filterAlertToCombat = JValue.solveInt(configHandle, ".filterAlertToCombat", 1) as bool
        filterAlertToNormal = JValue.solveInt(configHandle, ".filterAlertToNormal", 1) as bool
        filterAlertIdle = JValue.solveInt(configHandle, ".filterAlertIdle", 1) as bool
        filterLostIdle = JValue.solveInt(configHandle, ".filterLostIdle", 1) as bool
        filterLostToCombat = JValue.solveInt(configHandle, ".filterLostToCombat", 1) as bool
        filterLostToNormal = JValue.solveInt(configHandle, ".filterLostToNormal", 1) as bool
        filterObserveCombat = JValue.solveInt(configHandle, ".filterObserveCombat", 1) as bool
        filterFlee = JValue.solveInt(configHandle, ".filterFlee", 1) as bool
        filterAvoidThreat = JValue.solveInt(configHandle, ".filterAvoidThreat", 1) as bool
        filterYield = JValue.solveInt(configHandle, ".filterYield", 1) as bool
        filterAcceptYield = JValue.solveInt(configHandle, ".filterAcceptYield", 1) as bool
        filterPickpocketCombat = JValue.solveInt(configHandle, ".filterPickpocketCombat", 1) as bool
        filterDetectFriendDie = JValue.solveInt(configHandle, ".filterDetectFriendDie", 1) as bool
        filterPreserveGrunts = JValue.solveInt(configHandle, ".filterPreserveGrunts", 1) as bool
        filterVoicePowerEndLong = JValue.solveInt(configHandle, ".filterVoicePowerEndLong", 1) as bool
        filterVoicePowerEndShort = JValue.solveInt(configHandle, ".filterVoicePowerEndShort", 1) as bool
        filterVoicePowerStartLong = JValue.solveInt(configHandle, ".filterVoicePowerStartLong", 1) as bool
        filterVoicePowerStartShort = JValue.solveInt(configHandle, ".filterVoicePowerStartShort", 1) as bool
        filterWerewolfTransformCrime = JValue.solveInt(configHandle, ".filterWerewolfTransformCrime", 1) as bool
        filterPursueIdleTopic = JValue.solveInt(configHandle, ".filterPursueIdleTopic", 1) as bool
        filterMoralRefusal = JValue.solveInt(configHandle, ".filterMoralRefusal", 1) as bool
        filterExitFavorState = JValue.solveInt(configHandle, ".filterExitFavorState", 1) as bool
        filterAgree = JValue.solveInt(configHandle, ".filterAgree", 1) as bool
        filterShow = JValue.solveInt(configHandle, ".filterShow", 1) as bool
        filterRefuse = JValue.solveInt(configHandle, ".filterRefuse", 1) as bool
        filterFollowerCommentary = JValue.solveInt(configHandle, ".filterFollowerCommentary", 1) as bool
        filterBardSongs = JValue.solveInt(configHandle, ".filterBardSongs", 1) as bool
        filterScenes = JValue.solveInt(configHandle, ".filterScenes", 1) as bool
        filterBlacklist = JValue.solveInt(configHandle, ".filterBlacklist", 1) as bool
        filterMurder = JValue.solveInt(configHandle, ".filterMurder", 1) as bool
        filterMurderNC = JValue.solveInt(configHandle, ".filterMurderNC", 1) as bool
        filterAssault = JValue.solveInt(configHandle, ".filterAssault", 1) as bool
        filterAssaultNC = JValue.solveInt(configHandle, ".filterAssaultNC", 1) as bool
        filterActorCollideWithActor = JValue.solveInt(configHandle, ".filterActorCollideWithActor", 1) as bool
        filterBarterExit = JValue.solveInt(configHandle, ".filterBarterExit", 1) as bool
        filterDestroyObject = JValue.solveInt(configHandle, ".filterDestroyObject", 1) as bool
        filterGoodbye = JValue.solveInt(configHandle, ".filterGoodbye", 1) as bool
        filterHello = JValue.solveInt(configHandle, ".filterHello", 1) as bool
        Debug.Trace("STFU MCM: filterHello = " + filterHello)
        filterKnockOverObject = JValue.solveInt(configHandle, ".filterKnockOverObject", 1) as bool
        filterNoticeCorpse = JValue.solveInt(configHandle, ".filterNoticeCorpse", 1) as bool
        filterPickpocketTopic = JValue.solveInt(configHandle, ".filterPickpocketTopic", 1) as bool
        filterPickpocketNC = JValue.solveInt(configHandle, ".filterPickpocketNC", 1) as bool
        filterLockedObject = JValue.solveInt(configHandle, ".filterLockedObject", 1) as bool
        filterTrespass = JValue.solveInt(configHandle, ".filterTrespass", 1) as bool
        filterTimeToGo = JValue.solveInt(configHandle, ".filterTimeToGo", 1) as bool
        filterIdle = JValue.solveInt(configHandle, ".filterIdle", 1) as bool
        Debug.Trace("STFU MCM: filterIdle = " + filterIdle)
        filterStealFromNC = JValue.solveInt(configHandle, ".filterStealFromNC", 1) as bool
        filterTrespassAgainstNC = JValue.solveInt(configHandle, ".filterTrespassAgainstNC", 1) as bool
        filterPlayerShout = JValue.solveInt(configHandle, ".filterPlayerShout", 1) as bool
        filterPlayerInIronSights = JValue.solveInt(configHandle, ".filterPlayerInIronSights", 1) as bool
        filterPlayerCastProjectileSpell = JValue.solveInt(configHandle, ".filterPlayerCastProjectileSpell", 1) as bool
        filterPlayerCastSelfSpell = JValue.solveInt(configHandle, ".filterPlayerCastSelfSpell", 1) as bool
        filterSteal = JValue.solveInt(configHandle, ".filterSteal", 1) as bool
        filterSwingMeleeWeapon = JValue.solveInt(configHandle, ".filterSwingMeleeWeapon", 1) as bool
        filterShootBow = JValue.solveInt(configHandle, ".filterShootBow", 1) as bool
        filterStandOnFurniture = JValue.solveInt(configHandle, ".filterStandOnFurniture", 1) as bool
        filterTrainingExit = JValue.solveInt(configHandle, ".filterTrainingExit", 1) as bool
        filterZKeyObject = JValue.solveInt(configHandle, ".filterZKeyObject", 1) as bool
    Else
        configLoaded = false
        Debug.Trace("STFU MCM: Failed to load config, using defaults (all enabled)")
        ; If config doesn't exist, default to all enabled (show all options)
    EndIf
EndFunction

int Function GetVersion()
    return 7
EndFunction

Event OnVersionUpdate(int aiNewVersion)
    ModName = "S.T.F.U"
    Pages = new string[4]
    Pages[0] = "Combat Dialogue"
    Pages[1] = "Generic Dialogue"
    Pages[2] = "Follower Dialogue"
    Pages[3] = "Other Dialogue"
EndEvent

Event OnPageReset(string page)
    If page == "Combat Dialogue"
        SetCursorFillMode(TOP_TO_BOTTOM)
        
        ; Left column - Enable All button
        oid_EnableAll = AddTextOption("Enable All", "")
        AddEmptyOption()
        
        ; Left column - Attack subtypes
        AddHeaderOption("Attack Dialogue")
        If filterAttack
            oid_Attack = AddToggleOption("Block Attack", STFU_Attack.GetValue() as bool)
        EndIf
        If filterHit
            oid_Hit = AddToggleOption("Block Hit", STFU_Hit.GetValue() as bool)
        EndIf
        If filterPowerAttack
            oid_PowerAttack = AddToggleOption("Block Power Attack", STFU_PowerAttack.GetValue() as bool)
        EndIf
        If filterDeath
            oid_Death = AddToggleOption("Block Death", STFU_Death.GetValue() as bool)
        EndIf
        If filterBlock
            oid_Block = AddToggleOption("Block Block", STFU_Block.GetValue() as bool)
        EndIf
        If filterBleedout
            oid_Bleedout = AddToggleOption("Block Bleedout", STFU_Bleedout.GetValue() as bool)
        EndIf
        If filterBash
            oid_Bash = AddToggleOption("Block Bash", STFU_Bash.GetValue() as bool)
        EndIf
        AddEmptyOption()
        
        ; Left column - Combat transitions
        AddHeaderOption("Combat Transitions")
        If filterNormalToCombat
            oid_NormalToCombat = AddToggleOption("Block Normal To Combat", STFU_NormalToCombat.GetValue() as bool)
        EndIf
        If filterCombatToNormal
            oid_CombatToNormal = AddToggleOption("Block Combat To Normal", STFU_CombatToNormal.GetValue() as bool)
        EndIf
        If filterCombatToLost
            oid_CombatToLost = AddToggleOption("Block Combat To Lost", STFU_CombatToLost.GetValue() as bool)
        EndIf
        AddEmptyOption()
        
        ; Left column - Alert/Detection
        AddHeaderOption("Detection & Alert")
        If filterNormalToAlert
            oid_NormalToAlert = AddToggleOption("Block Normal To Alert", STFU_NormalToAlert.GetValue() as bool)
        EndIf
        If filterAlertToCombat
            oid_AlertToCombat = AddToggleOption("Block Alert To Combat", STFU_AlertToCombat.GetValue() as bool)
        EndIf
        If filterAlertToNormal
            oid_AlertToNormal = AddToggleOption("Block Alert To Normal", STFU_AlertToNormal.GetValue() as bool)
        EndIf
        If filterAlertIdle
            oid_AlertIdle = AddToggleOption("Block Alert Idle", STFU_AlertIdle.GetValue() as bool)
        EndIf
        
        ; Right column - Disable All button
        SetCursorPosition(1)
        oid_DisableAll = AddTextOption("Disable All", "")
        AddEmptyOption()
        
        ; Right column - Grunt preservation
        AddHeaderOption("Grunt Sounds")
        If filterPreserveGrunts
            oid_PreserveGrunts = AddToggleOption("Block Combat Grunts", STFU_PreserveGrunts.GetValue() as bool)
        EndIf
        AddEmptyOption()
        
        ; Right column - Combat commentary
        AddHeaderOption("Combat Commentary")
        If filterTaunt
            oid_Taunt = AddToggleOption("Block Taunt", STFU_Taunt.GetValue() as bool)
        EndIf
        If filterAllyKilled
            oid_AllyKilled = AddToggleOption("Block Ally Killed", STFU_AllyKilled.GetValue() as bool)
        EndIf
        If filterDetectFriendDie
            oid_DetectFriendDie = AddToggleOption("Block Detect Friend Die", STFU_DetectFriendDie.GetValue() as bool)
        EndIf
        If filterObserveCombat
            oid_ObserveCombat = AddToggleOption("Block Observe Combat", STFU_ObserveCombat.GetValue() as bool)
        EndIf
        AddEmptyOption()
        
        ; Right column - Lost/Search
        AddHeaderOption("Lost/Search Dialogue")
        If filterLostIdle
            oid_LostIdle = AddToggleOption("Block Lost Idle", STFU_LostIdle.GetValue() as bool)
        EndIf
        If filterLostToCombat
            oid_LostToCombat = AddToggleOption("Block Lost To Combat", STFU_LostToCombat.GetValue() as bool)
        EndIf
        If filterLostToNormal
            oid_LostToNormal = AddToggleOption("Block Lost To Normal", STFU_LostToNormal.GetValue() as bool)
        EndIf
        AddEmptyOption()
        
        ; Right column - Reactions
        AddHeaderOption("Combat Reactions")
        If filterFlee
            oid_Flee = AddToggleOption("Block Flee", STFU_Flee.GetValue() as bool)
        EndIf
        If filterAvoidThreat
            oid_AvoidThreat = AddToggleOption("Block Avoid Threat", STFU_AvoidThreat.GetValue() as bool)
        EndIf
        If filterYield
            oid_Yield = AddToggleOption("Block Yield", STFU_Yield.GetValue() as bool)
        EndIf
        If filterAcceptYield
            oid_AcceptYield = AddToggleOption("Block Accept Yield", STFU_AcceptYield.GetValue() as bool)
        EndIf
        If filterPickpocketCombat
            oid_PickpocketCombat = AddToggleOption("Block Pickpocket Combat", STFU_PickpocketCombat.GetValue() as bool)
        EndIf
    ElseIf page == "Generic Dialogue"
        SetCursorFillMode(TOP_TO_BOTTOM)
        
        ; Left column - Enable All button
        oid_EnableAllNC = AddTextOption("Enable All", "")
        AddEmptyOption()
        
        ; Left column - Social interactions
        AddHeaderOption("Social")
        If filterHello
            oid_Hello = AddToggleOption("Block Hello", STFU_Hello.GetValue() as bool)
        EndIf
        If filterGoodbye
            oid_Goodbye = AddToggleOption("Block Goodbye", STFU_Goodbye.GetValue() as bool)
        EndIf
        If filterIdle
            oid_Idle = AddToggleOption("Block Idle Chatter", STFU_Idle.GetValue() as bool)
        EndIf
        If filterTrainingExit
            oid_TrainingExit = AddToggleOption("Block Training Exit", STFU_TrainingExit.GetValue() as bool)
        EndIf
        AddEmptyOption()
        
        ; Left column - Crime/Stealth
        AddHeaderOption("Crime & Stealth")
        If filterMurder
            oid_Murder = AddToggleOption("Block Murder", STFU_Murder.GetValue() as bool)
        EndIf
        If filterMurderNC
            oid_MurderNC = AddToggleOption("Block Murder NC", STFU_MurderNC.GetValue() as bool)
        EndIf
        If filterAssault
            oid_Assault = AddToggleOption("Block Assault", STFU_Assault.GetValue() as bool)
        EndIf
        If filterAssaultNC
            oid_AssaultNC = AddToggleOption("Block Assault NC", STFU_AssaultNC.GetValue() as bool)
        EndIf
        If filterSteal
            oid_Steal = AddToggleOption("Block Steal", STFU_Steal.GetValue() as bool)
        EndIf
        If filterStealFromNC
            oid_StealFromNC = AddToggleOption("Block Steal From NC", STFU_StealFromNC.GetValue() as bool)
        EndIf
        If filterPickpocketTopic
            oid_PickpocketTopic = AddToggleOption("Block Pickpocket Topic", STFU_PickpocketTopic.GetValue() as bool)
        EndIf
        If filterPickpocketNC
            oid_PickpocketNC = AddToggleOption("Block Pickpocket NC", STFU_PickpocketNC.GetValue() as bool)
        EndIf
        If filterTrespass
            oid_Trespass = AddToggleOption("Block Trespass", STFU_Trespass.GetValue() as bool)
        EndIf
        If filterTrespassAgainstNC
            oid_TrespassAgainstNC = AddToggleOption("Block Trespass Against NC", STFU_TrespassAgainstNC.GetValue() as bool)
        EndIf
        If filterPursueIdleTopic
            oid_PursueIdleTopic = AddToggleOption("Block Pursue Idle", STFU_PursueIdleTopic.GetValue() as bool)
        EndIf
        If filterWerewolfTransformCrime
            oid_WerewolfTransformCrime = AddToggleOption("Block Werewolf Transform Crime", STFU_WerewolfTransformCrime.GetValue() as bool)
        EndIf
        
        ; Right column - Disable All button
        SetCursorPosition(1)
        oid_DisableAllNC = AddTextOption("Disable All", "")
        AddEmptyOption()
        
        ; Right column - Object Interactions
        AddHeaderOption("Object Interactions")
        If filterLockedObject
            oid_LockedObject = AddToggleOption("Block Locked Object", STFU_LockedObject.GetValue() as bool)
        EndIf
        If filterDestroyObject
            oid_DestroyObject = AddToggleOption("Block Destroy Object", STFU_DestroyObject.GetValue() as bool)
        EndIf
        If filterKnockOverObject
            oid_KnockOverObject = AddToggleOption("Block Knock Over Object", STFU_KnockOverObject.GetValue() as bool)
        EndIf
        If filterStandOnFurniture
            oid_StandOnFurniture = AddToggleOption("Block Stand On Furniture", STFU_StandOnFurniture.GetValue() as bool)
        EndIf
        If filterZKeyObject
            oid_ZKeyObject = AddToggleOption("Block ZKeyObject", STFU_ZKeyObject.GetValue() as bool)
        EndIf
        AddEmptyOption()
        
        ; Right column - Behaviors
        AddHeaderOption("Behaviors")
        If filterActorCollideWithActor
            oid_ActorCollideWithActor = AddToggleOption("Block Actor Collide", STFU_ActorCollideWithActor.GetValue() as bool)
        EndIf
        If filterNoticeCorpse
            oid_NoticeCorpse = AddToggleOption("Block Notice Corpse", STFU_NoticeCorpse.GetValue() as bool)
        EndIf
        If filterTimeToGo
            oid_TimeToGo = AddToggleOption("Block Time To Go", STFU_TimeToGo.GetValue() as bool)
        EndIf
        If filterBarterExit
            oid_BarterExit = AddToggleOption("Block Barter Exit", STFU_BarterExit.GetValue() as bool)
        EndIf
        AddEmptyOption()
        
        ; Right column - Player actions
        AddHeaderOption("Player Actions")
        If filterPlayerShout
            oid_PlayerShout = AddToggleOption("Block Player Shout", STFU_PlayerShout.GetValue() as bool)
        EndIf
        If filterPlayerInIronSights
            oid_PlayerInIronSights = AddToggleOption("Block Player In Iron Sights", STFU_PlayerInIronSights.GetValue() as bool)
        EndIf
        If filterPlayerCastProjectileSpell
            oid_PlayerCastProjectileSpell = AddToggleOption("Block Cast Projectile Spell", STFU_PlayerCastProjectileSpell.GetValue() as bool)
        EndIf
        If filterPlayerCastSelfSpell
            oid_PlayerCastSelfSpell = AddToggleOption("Block Cast Self Spell", STFU_PlayerCastSelfSpell.GetValue() as bool)
        EndIf
        If filterSwingMeleeWeapon
            oid_SwingMeleeWeapon = AddToggleOption("Block Swing Melee Weapon", STFU_SwingMeleeWeapon.GetValue() as bool)
        EndIf
        If filterShootBow
            oid_ShootBow = AddToggleOption("Block Shoot Bow", STFU_ShootBow.GetValue() as bool)
        EndIf
    ElseIf page == "Follower Dialogue"
        SetCursorFillMode(TOP_TO_BOTTOM)
        
        ; Left column - Enable All button
        oid_EnableAllFollower = AddTextOption("Enable All", "")
        AddEmptyOption()
        
        ; Left column - Follower subtypes
        AddHeaderOption("Follower Dialogue")
        If filterMoralRefusal
            oid_MoralRefusal = AddToggleOption("Block Moral Refusal", STFU_MoralRefusal.GetValue() as bool)
        EndIf
        If filterExitFavorState
            oid_ExitFavorState = AddToggleOption("Block Exit Favor State", STFU_ExitFavorState.GetValue() as bool)
        EndIf
        If filterAgree
            oid_Agree = AddToggleOption("Block Agree", STFU_Agree.GetValue() as bool)
        EndIf
        If filterShow
            oid_Show = AddToggleOption("Block Show", STFU_Show.GetValue() as bool)
        EndIf
        If filterRefuse
            oid_RefuseFollower = AddToggleOption("Block Refuse", STFU_Refuse.GetValue() as bool)
        EndIf
        
        ; Right column - Disable All button
        SetCursorPosition(1)
        oid_DisableAllFollower = AddTextOption("Disable All", "")
        AddEmptyOption()
        
        ; Right column - Follower Commentary
        AddHeaderOption("Follower Commentary")
        If filterFollowerCommentary
            oid_FollowerCommentary = AddToggleOption("Block Commentary Quests", STFU_FollowerCommentary.GetValue() as bool)
        EndIf
    ElseIf page == "Other Dialogue"
        SetCursorFillMode(TOP_TO_BOTTOM)
        
        ; Left column - Enable All button
        oid_EnableAllOther = AddTextOption("Enable All", "")
        AddEmptyOption()
        
        ; Left column - Other dialogue types
        AddHeaderOption("Other Dialogue Types")
        If filterBardSongs
            oid_BardSongs = AddToggleOption("Block Bard Songs", STFU_BardSongs.GetValue() as bool)
        EndIf
        If filterScenes
            oid_Scenes = AddToggleOption("Block Ambient Scenes", STFU_Scenes.GetValue() as bool)
        EndIf
        If filterBlacklist
            oid_Blacklist = AddToggleOption("Block Blacklisted Topics", STFU_Blacklist.GetValue() as bool)
        EndIf
        If filterSkyrimNetFilter
            oid_SkyrimNetFilter = AddToggleOption("Block SkyrimNet Filter Topics", STFU_SkyrimNetFilter.GetValue() as bool)
        EndIf
        
        ; Right column - Disable All button
        SetCursorPosition(1)
        oid_DisableAllOther = AddTextOption("Disable All", "")
        AddEmptyOption()
        
        ; Right column - Shouts
        AddHeaderOption("Shout Dialogue")
        If filterVoicePowerEndLong
            oid_VoicePowerEndLong = AddToggleOption("Block Shout End Long", STFU_VoicePowerEndLong.GetValue() as bool)
        EndIf
        If filterVoicePowerEndShort
            oid_VoicePowerEndShort = AddToggleOption("Block Shout End Short", STFU_VoicePowerEndShort.GetValue() as bool)
        EndIf
        If filterVoicePowerStartLong
            oid_VoicePowerStartLong = AddToggleOption("Block Shout Start Long", STFU_VoicePowerStartLong.GetValue() as bool)
        EndIf
        If filterVoicePowerStartShort
            oid_VoicePowerStartShort = AddToggleOption("Block Shout Start Short", STFU_VoicePowerStartShort.GetValue() as bool)
        EndIf
    EndIf
EndEvent

Event OnOptionSelect(int option)
    If option == oid_EnableAll
        EnableAllOptions()
    ElseIf option == oid_DisableAll
        DisableAllOptions()
    ElseIf option == oid_Attack
        ToggleGlobal(STFU_Attack, oid_Attack)
    ElseIf option == oid_Hit
        ToggleGlobal(STFU_Hit, oid_Hit)
    ElseIf option == oid_PowerAttack
        ToggleGlobal(STFU_PowerAttack, oid_PowerAttack)
    ElseIf option == oid_Death
        ToggleGlobal(STFU_Death, oid_Death)
    ElseIf option == oid_Block
        ToggleGlobal(STFU_Block, oid_Block)
    ElseIf option == oid_Bleedout
        ToggleGlobal(STFU_Bleedout, oid_Bleedout)
    ElseIf option == oid_AllyKilled
        ToggleGlobal(STFU_AllyKilled, oid_AllyKilled)
    ElseIf option == oid_Taunt
        ToggleGlobal(STFU_Taunt, oid_Taunt)
    ElseIf option == oid_NormalToCombat
        ToggleGlobal(STFU_NormalToCombat, oid_NormalToCombat)
    ElseIf option == oid_CombatToNormal
        ToggleGlobal(STFU_CombatToNormal, oid_CombatToNormal)
    ElseIf option == oid_CombatToLost
        ToggleGlobal(STFU_CombatToLost, oid_CombatToLost)
    ElseIf option == oid_NormalToAlert
        ToggleGlobal(STFU_NormalToAlert, oid_NormalToAlert)
    ElseIf option == oid_AlertToCombat
        ToggleGlobal(STFU_AlertToCombat, oid_AlertToCombat)
    ElseIf option == oid_AlertToNormal
        ToggleGlobal(STFU_AlertToNormal, oid_AlertToNormal)
    ElseIf option == oid_AlertIdle
        ToggleGlobal(STFU_AlertIdle, oid_AlertIdle)
    ElseIf option == oid_LostIdle
        ToggleGlobal(STFU_LostIdle, oid_LostIdle)
    ElseIf option == oid_LostToCombat
        ToggleGlobal(STFU_LostToCombat, oid_LostToCombat)
    ElseIf option == oid_LostToNormal
        ToggleGlobal(STFU_LostToNormal, oid_LostToNormal)
    ElseIf option == oid_ObserveCombat
        ToggleGlobal(STFU_ObserveCombat, oid_ObserveCombat)
    ElseIf option == oid_Flee
        ToggleGlobal(STFU_Flee, oid_Flee)
    ElseIf option == oid_AvoidThreat
        ToggleGlobal(STFU_AvoidThreat, oid_AvoidThreat)
    ElseIf option == oid_Yield
        ToggleGlobal(STFU_Yield, oid_Yield)
    ElseIf option == oid_AcceptYield
        ToggleGlobal(STFU_AcceptYield, oid_AcceptYield)
    ElseIf option == oid_Bash
        ToggleGlobal(STFU_Bash, oid_Bash)
    ElseIf option == oid_PickpocketCombat
        ToggleGlobal(STFU_PickpocketCombat, oid_PickpocketCombat)
    ElseIf option == oid_DetectFriendDie
        ToggleGlobal(STFU_DetectFriendDie, oid_DetectFriendDie)
    ElseIf option == oid_PreserveGrunts
        ToggleGlobal(STFU_PreserveGrunts, oid_PreserveGrunts)
    ElseIf option == oid_VoicePowerEndLong
        ToggleGlobal(STFU_VoicePowerEndLong, oid_VoicePowerEndLong)
    ElseIf option == oid_VoicePowerEndShort
        ToggleGlobal(STFU_VoicePowerEndShort, oid_VoicePowerEndShort)
    ElseIf option == oid_VoicePowerStartLong
        ToggleGlobal(STFU_VoicePowerStartLong, oid_VoicePowerStartLong)
    ElseIf option == oid_VoicePowerStartShort
        ToggleGlobal(STFU_VoicePowerStartShort, oid_VoicePowerStartShort)
    ElseIf option == oid_WerewolfTransformCrime
        ToggleGlobal(STFU_WerewolfTransformCrime, oid_WerewolfTransformCrime)
    ElseIf option == oid_PursueIdleTopic
        ToggleGlobal(STFU_PursueIdleTopic, oid_PursueIdleTopic)
    ; Follower handlers
    ElseIf option == oid_EnableAllFollower
        EnableAllFollowerOptions()
    ElseIf option == oid_DisableAllFollower
        DisableAllFollowerOptions()
    ElseIf option == oid_MoralRefusal
        ToggleGlobal(STFU_MoralRefusal, oid_MoralRefusal)
    ElseIf option == oid_ExitFavorState
        ToggleGlobal(STFU_ExitFavorState, oid_ExitFavorState)
    ElseIf option == oid_Agree
        ToggleGlobal(STFU_Agree, oid_Agree)
    ElseIf option == oid_Show
        ToggleGlobal(STFU_Show, oid_Show)
    ElseIf option == oid_RefuseFollower
        ToggleGlobal(STFU_Refuse, oid_RefuseFollower)
    ElseIf option == oid_FollowerCommentary
        ToggleGlobal(STFU_FollowerCommentary, oid_FollowerCommentary)
    ; Other handlers
    ElseIf option == oid_EnableAllOther
        EnableAllOtherOptions()
    ElseIf option == oid_DisableAllOther
        DisableAllOtherOptions()
    ElseIf option == oid_BardSongs
        ToggleGlobal(STFU_BardSongs, oid_BardSongs)
    ElseIf option == oid_Scenes
        ToggleGlobal(STFU_Scenes, oid_Scenes)
    ElseIf option == oid_Blacklist
        ToggleGlobal(STFU_Blacklist, oid_Blacklist)
    ElseIf option == oid_SkyrimNetFilter
        ToggleGlobal(STFU_SkyrimNetFilter, oid_SkyrimNetFilter)
    ; Non-Combat handlers
    ElseIf option == oid_EnableAllNC
        EnableAllNonCombatOptions()
    ElseIf option == oid_DisableAllNC
        DisableAllNonCombatOptions()
    ElseIf option == oid_Murder
        ToggleGlobal(STFU_Murder, oid_Murder)
    ElseIf option == oid_MurderNC
        ToggleGlobal(STFU_MurderNC, oid_MurderNC)
    ElseIf option == oid_Assault
        ToggleGlobal(STFU_Assault, oid_Assault)
    ElseIf option == oid_AssaultNC
        ToggleGlobal(STFU_AssaultNC, oid_AssaultNC)
    ElseIf option == oid_ActorCollideWithActor
        ToggleGlobal(STFU_ActorCollideWithActor, oid_ActorCollideWithActor)
    ElseIf option == oid_BarterExit
        ToggleGlobal(STFU_BarterExit, oid_BarterExit)
    ElseIf option == oid_DestroyObject
        ToggleGlobal(STFU_DestroyObject, oid_DestroyObject)
    ElseIf option == oid_Goodbye
        ToggleGlobal(STFU_Goodbye, oid_Goodbye)
    ElseIf option == oid_Hello
        ToggleGlobal(STFU_Hello, oid_Hello)
    ElseIf option == oid_KnockOverObject
        ToggleGlobal(STFU_KnockOverObject, oid_KnockOverObject)
    ElseIf option == oid_NoticeCorpse
        ToggleGlobal(STFU_NoticeCorpse, oid_NoticeCorpse)
    ElseIf option == oid_PickpocketTopic
        ToggleGlobal(STFU_PickpocketTopic, oid_PickpocketTopic)
    ElseIf option == oid_PickpocketNC
        ToggleGlobal(STFU_PickpocketNC, oid_PickpocketNC)
    ElseIf option == oid_LockedObject
        ToggleGlobal(STFU_LockedObject, oid_LockedObject)
    ElseIf option == oid_Trespass
        ToggleGlobal(STFU_Trespass, oid_Trespass)
    ElseIf option == oid_TimeToGo
        ToggleGlobal(STFU_TimeToGo, oid_TimeToGo)
    ElseIf option == oid_Idle
        ToggleGlobal(STFU_Idle, oid_Idle)
    ElseIf option == oid_StealFromNC
        ToggleGlobal(STFU_StealFromNC, oid_StealFromNC)
    ElseIf option == oid_TrespassAgainstNC
        ToggleGlobal(STFU_TrespassAgainstNC, oid_TrespassAgainstNC)
    ElseIf option == oid_PlayerShout
        ToggleGlobal(STFU_PlayerShout, oid_PlayerShout)
    ElseIf option == oid_PlayerInIronSights
        ToggleGlobal(STFU_PlayerInIronSights, oid_PlayerInIronSights)
    ElseIf option == oid_PlayerCastProjectileSpell
        ToggleGlobal(STFU_PlayerCastProjectileSpell, oid_PlayerCastProjectileSpell)
    ElseIf option == oid_PlayerCastSelfSpell
        ToggleGlobal(STFU_PlayerCastSelfSpell, oid_PlayerCastSelfSpell)
    ElseIf option == oid_Steal
        ToggleGlobal(STFU_Steal, oid_Steal)
    ElseIf option == oid_SwingMeleeWeapon
        ToggleGlobal(STFU_SwingMeleeWeapon, oid_SwingMeleeWeapon)
    ElseIf option == oid_ShootBow
        ToggleGlobal(STFU_ShootBow, oid_ShootBow)
    ElseIf option == oid_StandOnFurniture
        ToggleGlobal(STFU_StandOnFurniture, oid_StandOnFurniture)
    ElseIf option == oid_TrainingExit
        ToggleGlobal(STFU_TrainingExit, oid_TrainingExit)
    ElseIf option == oid_ZKeyObject
        ToggleGlobal(STFU_ZKeyObject, oid_ZKeyObject)
    EndIf
EndEvent

Function ToggleGlobal(GlobalVariable akGlobal, int aiOID)
    If akGlobal.GetValue() == 0.0
        akGlobal.SetValue(1.0)
        SetToggleOptionValue(aiOID, true)
    Else
        akGlobal.SetValue(0.0)
        SetToggleOptionValue(aiOID, false)
    EndIf
EndFunction

Function EnableAllOptions()
    STFU_Attack.SetValue(1.0)
    STFU_Hit.SetValue(1.0)
    STFU_PowerAttack.SetValue(1.0)
    STFU_Death.SetValue(1.0)
    STFU_Block.SetValue(1.0)
    STFU_Bleedout.SetValue(1.0)
    STFU_AllyKilled.SetValue(1.0)
    STFU_Taunt.SetValue(1.0)
    STFU_NormalToCombat.SetValue(1.0)
    STFU_CombatToNormal.SetValue(1.0)
    STFU_CombatToLost.SetValue(1.0)
    STFU_NormalToAlert.SetValue(1.0)
    STFU_AlertToCombat.SetValue(1.0)
    STFU_AlertToNormal.SetValue(1.0)
    STFU_AlertIdle.SetValue(1.0)
    STFU_LostIdle.SetValue(1.0)
    STFU_LostToCombat.SetValue(1.0)
    STFU_LostToNormal.SetValue(1.0)
    STFU_ObserveCombat.SetValue(1.0)
    STFU_Flee.SetValue(1.0)
    STFU_AvoidThreat.SetValue(1.0)
    STFU_Yield.SetValue(1.0)
    STFU_AcceptYield.SetValue(1.0)
    STFU_Bash.SetValue(1.0)
    STFU_PickpocketCombat.SetValue(1.0)
    STFU_DetectFriendDie.SetValue(1.0)
    STFU_PreserveGrunts.SetValue(1.0)
    ForcePageReset()
EndFunction

Function DisableAllOptions()
    STFU_Attack.SetValue(0.0)
    STFU_Hit.SetValue(0.0)
    STFU_PowerAttack.SetValue(0.0)
    STFU_Death.SetValue(0.0)
    STFU_Block.SetValue(0.0)
    STFU_Bleedout.SetValue(0.0)
    STFU_AllyKilled.SetValue(0.0)
    STFU_Taunt.SetValue(0.0)
    STFU_NormalToCombat.SetValue(0.0)
    STFU_CombatToNormal.SetValue(0.0)
    STFU_CombatToLost.SetValue(0.0)
    STFU_NormalToAlert.SetValue(0.0)
    STFU_AlertToCombat.SetValue(0.0)
    STFU_AlertToNormal.SetValue(0.0)
    STFU_AlertIdle.SetValue(0.0)
    STFU_LostIdle.SetValue(0.0)
    STFU_LostToCombat.SetValue(0.0)
    STFU_LostToNormal.SetValue(0.0)
    STFU_ObserveCombat.SetValue(0.0)
    STFU_Flee.SetValue(0.0)
    STFU_AvoidThreat.SetValue(0.0)
    STFU_Yield.SetValue(0.0)
    STFU_AcceptYield.SetValue(0.0)
    STFU_Bash.SetValue(0.0)
    STFU_PickpocketCombat.SetValue(0.0)
    STFU_DetectFriendDie.SetValue(0.0)
    STFU_PreserveGrunts.SetValue(0.0)
    ForcePageReset()
EndFunction

Function EnableAllFollowerOptions()
    STFU_MoralRefusal.SetValue(1.0)
    STFU_ExitFavorState.SetValue(1.0)
    STFU_Agree.SetValue(1.0)
    STFU_Show.SetValue(1.0)
    STFU_Refuse.SetValue(1.0)
    STFU_FollowerCommentary.SetValue(1.0)
    ForcePageReset()
EndFunction

Function DisableAllFollowerOptions()
    STFU_MoralRefusal.SetValue(0.0)
    STFU_ExitFavorState.SetValue(0.0)
    STFU_Agree.SetValue(0.0)
    STFU_Show.SetValue(0.0)
    STFU_Refuse.SetValue(0.0)
    STFU_FollowerCommentary.SetValue(0.0)
    ForcePageReset()
EndFunction

Function EnableAllOtherOptions()
    STFU_BardSongs.SetValue(1.0)
    STFU_Scenes.SetValue(1.0)
    STFU_Blacklist.SetValue(1.0)
    STFU_SkyrimNetFilter.SetValue(1.0)
    STFU_VoicePowerEndLong.SetValue(1.0)
    STFU_VoicePowerEndShort.SetValue(1.0)
    STFU_VoicePowerStartLong.SetValue(1.0)
    STFU_VoicePowerStartShort.SetValue(1.0)
    ForcePageReset()
EndFunction

Function DisableAllOtherOptions()
    STFU_BardSongs.SetValue(0.0)
    STFU_Scenes.SetValue(0.0)
    STFU_Blacklist.SetValue(0.0)
    STFU_SkyrimNetFilter.SetValue(0.0)
    STFU_VoicePowerEndLong.SetValue(0.0)
    STFU_VoicePowerEndShort.SetValue(0.0)
    STFU_VoicePowerStartLong.SetValue(0.0)
    STFU_VoicePowerStartShort.SetValue(0.0)
    ForcePageReset()
EndFunction

Function EnableAllNonCombatOptions()
    STFU_Murder.SetValue(1.0)
    STFU_MurderNC.SetValue(1.0)
    STFU_Assault.SetValue(1.0)
    STFU_AssaultNC.SetValue(1.0)
    STFU_ActorCollideWithActor.SetValue(1.0)
    STFU_BarterExit.SetValue(1.0)
    STFU_DestroyObject.SetValue(1.0)
    STFU_Goodbye.SetValue(1.0)
    STFU_Hello.SetValue(1.0)
    STFU_KnockOverObject.SetValue(1.0)
    STFU_NoticeCorpse.SetValue(1.0)
    STFU_PickpocketTopic.SetValue(1.0)
    STFU_PickpocketNC.SetValue(1.0)
    STFU_LockedObject.SetValue(1.0)
    STFU_Trespass.SetValue(1.0)
    STFU_TimeToGo.SetValue(1.0)
    STFU_Idle.SetValue(1.0)
    STFU_StealFromNC.SetValue(1.0)
    STFU_TrespassAgainstNC.SetValue(1.0)
    STFU_PlayerShout.SetValue(1.0)
    STFU_PlayerInIronSights.SetValue(1.0)
    STFU_PlayerCastProjectileSpell.SetValue(1.0)
    STFU_PlayerCastSelfSpell.SetValue(1.0)
    STFU_Steal.SetValue(1.0)
    STFU_SwingMeleeWeapon.SetValue(1.0)
    STFU_ShootBow.SetValue(1.0)
    STFU_StandOnFurniture.SetValue(1.0)
    STFU_TrainingExit.SetValue(1.0)
    STFU_ZKeyObject.SetValue(1.0)
    STFU_PursueIdleTopic.SetValue(1.0)
    STFU_WerewolfTransformCrime.SetValue(1.0)
    ForcePageReset()
EndFunction

Function DisableAllNonCombatOptions()
    STFU_Murder.SetValue(0.0)
    STFU_MurderNC.SetValue(0.0)
    STFU_Assault.SetValue(0.0)
    STFU_AssaultNC.SetValue(0.0)
    STFU_ActorCollideWithActor.SetValue(0.0)
    STFU_BarterExit.SetValue(0.0)
    STFU_DestroyObject.SetValue(0.0)
    STFU_Goodbye.SetValue(0.0)
    STFU_Hello.SetValue(0.0)
    STFU_KnockOverObject.SetValue(0.0)
    STFU_NoticeCorpse.SetValue(0.0)
    STFU_PickpocketTopic.SetValue(0.0)
    STFU_PickpocketNC.SetValue(0.0)
    STFU_LockedObject.SetValue(0.0)
    STFU_Trespass.SetValue(0.0)
    STFU_TimeToGo.SetValue(0.0)
    STFU_Idle.SetValue(0.0)
    STFU_StealFromNC.SetValue(0.0)
    STFU_TrespassAgainstNC.SetValue(0.0)
    STFU_PlayerShout.SetValue(0.0)
    STFU_PlayerInIronSights.SetValue(0.0)
    STFU_PlayerCastProjectileSpell.SetValue(0.0)
    STFU_PlayerCastSelfSpell.SetValue(0.0)
    STFU_Steal.SetValue(0.0)
    STFU_SwingMeleeWeapon.SetValue(0.0)
    STFU_ShootBow.SetValue(0.0)
    STFU_StandOnFurniture.SetValue(0.0)
    STFU_TrainingExit.SetValue(0.0)
    STFU_ZKeyObject.SetValue(0.0)
    STFU_PursueIdleTopic.SetValue(0.0)
    STFU_WerewolfTransformCrime.SetValue(0.0)
    ForcePageReset()
EndFunction

Event OnOptionHighlight(int option)
    If option == oid_EnableAll
        SetInfoText("Enable all combat dialogue blocking options at once")
    ElseIf option == oid_DisableAll
        SetInfoText("Disable all combat dialogue blocking options at once")
    ElseIf option == oid_Attack
        SetInfoText("Attack dialogue - e.g. 'I've had enough of you!', 'Die, beast!', 'You can't win this!'")
    ElseIf option == oid_Hit
        SetInfoText("Hit dialogue - e.g. 'Do your worst!', 'That your best? Huh?', 'Damn you!'")
    ElseIf option == oid_PowerAttack
        SetInfoText("Power attack dialogue - Only grunts by default, mods might add dialogue.'")
    ElseIf option == oid_Death
        SetInfoText("Death dialogue - e.g. 'Thank you.', 'Free...again.', 'At last.'")
    ElseIf option == oid_Block
        SetInfoText("Block dialogue - e.g. 'Close...', 'Easily blocked!', 'Oho! Not so fast!'")
    ElseIf option == oid_Bleedout
        SetInfoText("Bleedout dialogue - e.g. 'Help me...', 'I can't move...', 'Someone...'")
    ElseIf option == oid_Bash
        SetInfoText("Bash dialogue - e.g. 'Hunh!', 'Gah!', 'Yah!', 'Rargh!' (vanilla is only grunts, mods may add dialogue)")
    ElseIf option == oid_AllyKilled
        SetInfoText("Ally killed dialogue - e.g. 'No!! How dare you!', 'Why, why?!', 'Gods, no!'")
    ElseIf option == oid_Taunt
        SetInfoText("Taunt dialogue - e.g. 'Skyrim belongs to the Nords!', 'I'll rip your heart out!', 'Prepare to die!'")
    ElseIf option == oid_Murder
        SetInfoText("Murder dialogue - e.g. 'Help, someone!', 'Someone do something!', 'By the gods, no!'")
    ElseIf option == oid_Assault
        SetInfoText("Assault dialogue - e.g. 'Bad idea.', 'Help me, help me!', 'Help! I'm being attacked!'")
    ElseIf option == oid_NormalToCombat
        SetInfoText("Normal to combat transition - e.g. 'You're dead!', 'By Ysmir, you won't leave here alive!'")
    ElseIf option == oid_CombatToNormal
        SetInfoText("Combat to normal transition - e.g. 'That takes care of that.', 'All over now.'")
    ElseIf option == oid_CombatToLost
        SetInfoText("Combat to lost transition - e.g. 'Is it safe?', 'Where are you hiding?'")
    ElseIf option == oid_NormalToAlert
        SetInfoText("Normal to alert transition - e.g. 'Did you hear something?', 'Hello? Who's there?', 'Is... is someone there?'")
    ElseIf option == oid_AlertToCombat
        SetInfoText("Alert to combat transition - e.g. 'Now you're mine!', 'Time to end this little game!', 'I knew it!'")
    ElseIf option == oid_AlertToNormal
        SetInfoText("Alert to normal transition - e.g. 'Must have been nothing.', 'Mind's playing tricks on me...'")
    ElseIf option == oid_AlertIdle
        SetInfoText("Alert idle dialogue - e.g. 'Anybody there?', 'Hello? Who's there?', 'I know I heard something.'")
    ElseIf option == oid_LostIdle
        SetInfoText("Lost idle dialogue - e.g. 'I'm going to find you.', 'You afraid to fight me?'")
    ElseIf option == oid_LostToCombat
        SetInfoText("Lost to combat transition - e.g. 'There you are.', 'I knew I'd find you!'")
    ElseIf option == oid_LostToNormal
        SetInfoText("Lost to normal transition - e.g. 'Oh well. Must have run off.', 'Nobody here anymore.'")
    ElseIf option == oid_ObserveCombat
        SetInfoText("Observe combat dialogue - e.g. 'Gods! Another fight.', 'Somebody help!', 'I'm getting out of here!'")
    ElseIf option == oid_Flee
        SetInfoText("Flee dialogue - e.g. 'Ah! It burns!', 'You win! I submit!', 'Aaaiiee!'")
    ElseIf option == oid_AvoidThreat
        SetInfoText("Avoid threat dialogue - e.g. 'I need to get away!', 'Too dangerous!', 'Back off!'")
    ElseIf option == oid_Yield
        SetInfoText("Yield dialogue - e.g. 'I yield! I yield!', 'Please, mercy!', 'I give up!'")
    ElseIf option == oid_AcceptYield
        SetInfoText("Accept yield dialogue - e.g. 'I'll let you live. This time.', 'You're not worth it.', 'All right, you've had enough.'")
    ElseIf option == oid_Bash
        SetInfoText("Bash dialogue - Only grunts by default, mods might add dialogue.")
    ElseIf option == oid_PickpocketCombat
        SetInfoText("Pickpocket combat - NPCs reacting to pickpocket attempts during combat")
    ElseIf option == oid_DetectFriendDie
        SetInfoText("Detect friend die - NPCs reacting when they detect a friend has died")
    ElseIf option == oid_PreserveGrunts
        SetInfoText("Combat grunts - e.g. 'Agh!', 'Oof!', 'Nargh!', 'Argh!', 'Yeagh!'")
    ElseIf option == oid_VoicePowerEndLong
        SetInfoText("Voice power end long - Long shout ending dialogue")
    ElseIf option == oid_VoicePowerEndShort
        SetInfoText("Voice power end short - Short shout ending dialogue")
    ElseIf option == oid_VoicePowerStartLong
        SetInfoText("Voice power start long - Long shout starting dialogue")
    ElseIf option == oid_VoicePowerStartShort
        SetInfoText("Voice power start short - Short shout starting dialogue")
    ElseIf option == oid_WerewolfTransformCrime
        SetInfoText("Werewolf transform crime - NPCs reacting to werewolf transformations")
    ElseIf option == oid_PursueIdleTopic
        SetInfoText("Pursue idle topic - Guards pursuing criminals")
    ; Follower hover texts
    ElseIf option == oid_EnableAllFollower
        SetInfoText("Enable all follower dialogue blocking options at once")
    ElseIf option == oid_DisableAllFollower
        SetInfoText("Disable all follower dialogue blocking options at once")
    ElseIf option == oid_MoralRefusal
        SetInfoText("Moral refusal - Follower refuses requests that conflict with their morals")
    ElseIf option == oid_ExitFavorState
        SetInfoText("Exit favor state - Follower dialogue when ending favor/command mode")
    ElseIf option == oid_Agree
        SetInfoText("Agree - Follower agreeing to requests or commands")
    ElseIf option == oid_Show
        SetInfoText("Show - Follower ready for a command")
    ElseIf option == oid_RefuseFollower
        SetInfoText("Refuse - Follower refusing general requests")
    ElseIf option == oid_FollowerCommentary
        SetInfoText("Follower commentary quests - Blocks scene-based follower comments (dungeon entrance, impressive view, danger ahead)")
    ; Other hover texts
    ElseIf option == oid_EnableAllOther
        SetInfoText("Enable all miscellaneous dialogue blocking options at once")
    ElseIf option == oid_DisableAllOther
        SetInfoText("Disable all miscellaneous dialogue blocking options at once")
    ElseIf option == oid_BardSongs
        SetInfoText("Bard songs - Blocks bard performances and applause")
    ElseIf option == oid_Scenes
        SetInfoText("Ambient scenes - Blocks town ambient dialogue scenes. WARNING: Toggle in an area without active scenes to avoid stuck scenes.")
    ElseIf option == oid_Blacklist
        SetInfoText("Blacklist - Blocks specific dialogue topics and scenes defined in STFU_Blacklist.yaml")
    ElseIf option == oid_SkyrimNetFilter
        SetInfoText("SkyrimNet Filter - Blocks dialogue topics in STFU_SkyrimNetFilter.yaml from being logged in SkyrimNet")
    ; Non-Combat hover texts
    ElseIf option == oid_EnableAllNC
        SetInfoText("Enable all non-combat dialogue blocking options at once")
    ElseIf option == oid_DisableAllNC
        SetInfoText("Disable all non-combat dialogue blocking options at once")
    ElseIf option == oid_Hello
        SetInfoText("Hello dialogue - Greetings from NPCs")
    ElseIf option == oid_Goodbye
        SetInfoText("Goodbye dialogue - Farewells when ending conversations")
    ElseIf option == oid_Idle
        SetInfoText("Idle chatter - Random comments NPCs make to themselves")
    ElseIf option == oid_Murder
        SetInfoText("Murder dialogue - Civilian NPCs reacting to witnessing murder")
    ElseIf option == oid_MurderNC
        SetInfoText("Murder non-crime - Murder reactions outside crime system")
    ElseIf option == oid_Assault
        SetInfoText("Assault dialogue - Civilian NPCs reacting to witnessing assault")
    ElseIf option == oid_AssaultNC
        SetInfoText("Assault non-crime - Assault reactions outside crime system")
    ElseIf option == oid_Steal
        SetInfoText("Steal dialogue - NPCs reacting to you stealing items")
    ElseIf option == oid_StealFromNC
        SetInfoText("Steal from non-crime - NPCs commenting on theft outside crime system")
    ElseIf option == oid_PickpocketTopic
        SetInfoText("Pickpocket topic - Dialogue about pickpocketing attempts")
    ElseIf option == oid_PickpocketNC
        SetInfoText("Pickpocket non-crime - Pickpocket comments outside crime system")
    ElseIf option == oid_Trespass
        SetInfoText("Trespass dialogue - NPCs warning you about entering restricted areas")
    ElseIf option == oid_TrespassAgainstNC
        SetInfoText("Trespass against non-crime - Trespass warnings outside crime system")
    ElseIf option == oid_LockedObject
        SetInfoText("Locked object - NPCs commenting when you try to open locked containers/doors")
    ElseIf option == oid_DestroyObject
        SetInfoText("Destroy object - NPCs reacting to you destroying objects")
    ElseIf option == oid_KnockOverObject
        SetInfoText("Knock over object - NPCs commenting when you knock things over")
    ElseIf option == oid_StandOnFurniture
        SetInfoText("Stand on furniture - NPCs commenting when you stand on furniture")
    ElseIf option == oid_ActorCollideWithActor
        SetInfoText("Actor collide - NPCs reacting when you bump into them")
    ElseIf option == oid_NoticeCorpse
        SetInfoText("Notice corpse - NPCs reacting to seeing dead bodies")
    ElseIf option == oid_TimeToGo
        SetInfoText("Time to go - NPCs telling you to leave or wrapping up")
    ElseIf option == oid_BarterExit
        SetInfoText("Barter exit - Merchants' closing remarks when ending trade")
    ElseIf option == oid_PlayerShout
        SetInfoText("Player shout - NPCs reacting to you using shouts")
    ElseIf option == oid_PlayerInIronSights
        SetInfoText("Player in iron sights - NPCs reacting to you aiming at them with ranged weapons")
    ElseIf option == oid_PlayerCastProjectileSpell
        SetInfoText("Cast projectile spell - NPCs reacting to you casting projectile spells near them")
    ElseIf option == oid_PlayerCastSelfSpell
        SetInfoText("Cast self spell - NPCs reacting to you casting spells on yourself")
    ElseIf option == oid_SwingMeleeWeapon
        SetInfoText("Swing melee weapon - NPCs reacting to you swinging weapons near them")
    ElseIf option == oid_ShootBow
        SetInfoText("Shoot bow - NPCs reacting to you shooting arrows near them")
    ElseIf option == oid_TrainingExit
        SetInfoText("Training exit - Trainer dialogue when exiting training menu")
    ElseIf option == oid_ZKeyObject
        SetInfoText("ZKeyObject - NPCs commenting when you pick up objects through the physics system")
    EndIf
EndEvent
