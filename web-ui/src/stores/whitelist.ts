import { create } from 'zustand';
import { BlacklistEntry } from '../types';

interface WhitelistStore {
  entries: BlacklistEntry[];
  searchQuery: string;
  showTopics: boolean;
  showScenes: boolean;
  showActors: boolean;
  showFactions: boolean;
  selectedEntries: BlacklistEntry[];
  setEntries: (entries: BlacklistEntry[]) => void;
  setSearchQuery: (query: string) => void;
  setShowTopics: (enabled: boolean) => void;
  setShowScenes: (enabled: boolean) => void;
  setShowActors: (enabled: boolean) => void;
  setShowFactions: (enabled: boolean) => void;
  setSelectedEntries: (entries: BlacklistEntry[]) => void;
  clearEntries: () => void;
}

export const useWhitelistStore = create<WhitelistStore>((set) => ({
  entries: [],
  searchQuery: '',
  showTopics: true,
  showScenes: true,
  showActors: true,
  showFactions: true,
  selectedEntries: [],
  setEntries: (entries) => set({ entries }),
  setSearchQuery: (query) => set({ searchQuery: query }),
  setShowTopics: (enabled) => set({ showTopics: enabled }),
  setShowScenes: (enabled) => set({ showScenes: enabled }),
  setShowActors: (enabled) => set({ showActors: enabled }),
  setShowFactions: (enabled) => set({ showFactions: enabled }),
  setSelectedEntries: (entries) => set({ selectedEntries: entries }),
  clearEntries: () => set({ entries: [], selectedEntries: [] }),
}));
