import { create } from 'zustand';
import { DialogueEntry } from '@/types';

interface HistoryStore {
  entries: DialogueEntry[];
  searchQuery: string;
  setEntries: (entries: DialogueEntry[]) => void;
  setSearchQuery: (query: string) => void;
  clearEntries: () => void;
}

export const useHistoryStore = create<HistoryStore>((set) => ({
  entries: [],
  searchQuery: '',
  setEntries: (entries) => set({ entries }),
  setSearchQuery: (query) => set({ searchQuery: query }),
  clearEntries: () => set({ entries: [], searchQuery: '' }),
}));
