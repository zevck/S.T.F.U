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
  status: 'Allowed' | 'Soft Block' | 'Hard Block' | 'Skyrim' | 'Filter' | 'Toggled Off' | 'Whitelist';
  responseCount: number;
  skyrimNetBlockable: boolean;
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
}
