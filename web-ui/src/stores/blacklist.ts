import { create } from 'zustand';
import { BlacklistEntry } from '../types';

interface BlacklistStore {
  entries: BlacklistEntry[];
  searchQuery: string;
  typeFilter: string;
  blockSoft: boolean;
  blockHard: boolean;
  blockSkyrimNet: boolean;
  showTopics: boolean;
  showScenes: boolean;
  setEntries: (entries: BlacklistEntry[]) => void;
  setSearchQuery: (query: string) => void;
  setTypeFilter: (filter: string) => void;
  setBlockSoft: (enabled: boolean) => void;
  setBlockHard: (enabled: boolean) => void;
  setBlockSkyrimNet: (enabled: boolean) => void;
  setShowTopics: (enabled: boolean) => void;
  setShowScenes: (enabled: boolean) => void;
  clearEntries: () => void;
}

export const useBlacklistStore = create<BlacklistStore>((set) => ({
  entries: [],
  searchQuery: '',
  typeFilter: 'All',
  blockSoft: true,
  blockHard: true,
  blockSkyrimNet: true,
  showTopics: true,
  showScenes: true,
  setEntries: (entries) => set({ entries }),
  setSearchQuery: (query) => set({ searchQuery: query }),
  setTypeFilter: (filter) => set({ typeFilter: filter }),
  setBlockSoft: (enabled) => set({ blockSoft: enabled }),
  setBlockHard: (enabled) => set({ blockHard: enabled }),
  setBlockSkyrimNet: (enabled) => set({ blockSkyrimNet: enabled }),
  setShowTopics: (enabled) => set({ showTopics: enabled }),
  setShowScenes: (enabled) => set({ showScenes: enabled }),
  clearEntries: () => set({ entries: [] }),
}));
