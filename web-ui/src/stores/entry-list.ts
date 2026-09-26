import { create } from 'zustand';
import { BlacklistEntry } from '../types';

// State behind the Blacklist and Whitelist tabs (see components/entry-list.tsx).
// blockSoft/blockHard are only shown on the blacklist.
export interface EntryListState {
  entries: BlacklistEntry[];
  searchQuery: string;
  blockSoft: boolean;
  blockHard: boolean;
  showTopics: boolean;
  showScenes: boolean;
  showActors: boolean;
  showFactions: boolean;
  selectedEntries: BlacklistEntry[];
  setEntries: (entries: BlacklistEntry[]) => void;
  setSearchQuery: (query: string) => void;
  setBlockSoft: (enabled: boolean) => void;
  setBlockHard: (enabled: boolean) => void;
  setShowTopics: (enabled: boolean) => void;
  setShowScenes: (enabled: boolean) => void;
  setShowActors: (enabled: boolean) => void;
  setShowFactions: (enabled: boolean) => void;
  setSelectedEntries: (entries: BlacklistEntry[]) => void;
  resetFilters: () => void;
}

const DEFAULT_FILTERS = {
  searchQuery: '',
  blockSoft: true,
  blockHard: true,
  showTopics: true,
  showScenes: true,
  showActors: true,
  showFactions: true,
};

export const createEntryListStore = () => create<EntryListState>((set) => ({
  entries: [],
  selectedEntries: [],
  ...DEFAULT_FILTERS,
  setEntries: (entries) => set({ entries }),
  setSearchQuery: (query) => set({ searchQuery: query }),
  setBlockSoft: (enabled) => set({ blockSoft: enabled }),
  setBlockHard: (enabled) => set({ blockHard: enabled }),
  setShowTopics: (enabled) => set({ showTopics: enabled }),
  setShowScenes: (enabled) => set({ showScenes: enabled }),
  setShowActors: (enabled) => set({ showActors: enabled }),
  setShowFactions: (enabled) => set({ showFactions: enabled }),
  setSelectedEntries: (entries) => set({ selectedEntries: entries }),
  resetFilters: () => set(DEFAULT_FILTERS),
}));
