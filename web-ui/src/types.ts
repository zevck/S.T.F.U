export interface DialogueEntry {
  id: number;
  timestamp: number;
  speaker: string;
  speakerFormID?: string;
  text: string;
  questName: string;
  questEditorID: string;
  topicEditorID: string;
  topicFormID: string;
  sourcePlugin: string;
  subtypeName: string;
  topicSubtype: number;
  status: 'Allowed' | 'Soft Block' | 'Hard Block' | 'Skyrim' | 'SkyrimNet Block' | 'Filter' | 'Toggled Off' | 'Whitelist';
  responseCount: number;
  skyrimNetBlockable: boolean;
  isScene: boolean;
  isBardSong: boolean;
  sceneEditorID: string;
  allResponses?: string[];
  isActorBlocked?: boolean;
  blockingFactionEditorID?: string;
  isActorWhitelisted?: boolean;
  whitelistFactionEditorID?: string;
  isSubtypeFiltered?: boolean;
  isSubtypeToggledOff?: boolean;
}

export interface BlacklistEntry {
  id: number;
  targetType?: string;
  blockType?: string;
  topicEditorID?: string;
  topicFormID?: string;
  questName?: string;
  questEditorID?: string;
  sourcePlugin?: string;
  note?: string;
  dateAdded?: number;
  filterCategory?: string;
  responseText?: string;
  allResponses?: string[];
  actorFilterFormIDs?: string[];       // Hex FormIDs like ["0x0001A68C"]
  actorFilterNames?: string[];         // Actor names like ["Lydia", "Guard"]
  factionFilterEditorIDs?: string[];   // Faction EditorIDs like ["WhiterunGuardFaction"]
}
