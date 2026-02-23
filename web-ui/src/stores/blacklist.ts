import { create } from 'zustand';
import { BlacklistEntry } from '../types';

interface BlacklistStore {
  entries: BlacklistEntry[];
  searchQuery: string;
  setEntries: (entries: BlacklistEntry[]) => void;
  setSearchQuery: (query: string) => void;
  clearEntries: () => void;
}

export const useBlacklistStore = create<BlacklistStore>((set) => ({
  entries: [],
  searchQuery: '',
  setEntries: (entries) => set({ entries }),
  setSearchQuery: (query) => set({ searchQuery: query }),
  clearEntries: () => set({ entries: [] }),
}));
