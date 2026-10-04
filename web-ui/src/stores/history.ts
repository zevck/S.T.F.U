import { create } from 'zustand';
import { DialogueEntry } from '@/types';

interface HistoryStore {
  entries: DialogueEntry[];
  searchQuery: string;
  selectedEntries: DialogueEntry[];
  setEntries: (entries: DialogueEntry[]) => void;
  setSearchQuery: (query: string) => void;
  setSelectedEntries: (entries: DialogueEntry[]) => void;
}

export const useHistoryStore = create<HistoryStore>((set) => ({
  entries: [],
  searchQuery: '',
  selectedEntries: [],
  setEntries: (entries) => set({ entries }),
  setSearchQuery: (query) => set({ searchQuery: query }),
  setSelectedEntries: (entries) => set({ selectedEntries: entries }),
}));
