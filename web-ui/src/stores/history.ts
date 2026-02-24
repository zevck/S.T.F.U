import { create } from 'zustand';
import { DialogueEntry } from '@/types';

interface HistoryStore {
  entries: DialogueEntry[];
  searchQuery: string;
  setEntries: (entries: DialogueEntry[]) => void;
  setSearchQuery: (query: string) => void;
  clearEntries: () => void;
  updateEntryStatus: (entryIds: number[], newStatus: DialogueEntry['status']) => void;
}

export const useHistoryStore = create<HistoryStore>((set) => ({
  entries: [],
  searchQuery: '',
  setEntries: (entries) => set({ entries }),
  setSearchQuery: (query) => set({ searchQuery: query }),
  clearEntries: () => set({ entries: [], searchQuery: '' }),
  updateEntryStatus: (entryIds, newStatus) => set((state) => ({
    entries: state.entries.map(entry => 
      entryIds.includes(entry.id) ? { ...entry, status: newStatus } : entry
    )
  })),
}));
