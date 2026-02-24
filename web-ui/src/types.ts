export interface DialogueEntry {
  id: number;
  timestamp: number;
  speaker: string;
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
}
