import { create } from 'zustand';
import { DialogueEntry } from '@/types';

interface HistoryStore {
  entries: DialogueEntry[];
  searchQuery: string;
  selectedEntries: DialogueEntry[];
  setEntries: (entries: DialogueEntry[]) => void;
  setSearchQuery: (query: string) => void;
  setSelectedEntries: (entries: DialogueEntry[]) => void;
  clearEntries: () => void;
  updateEntryStatus: (entryIds: number[], newStatus: DialogueEntry['status']) => void;
}

export const useHistoryStore = create<HistoryStore>((set) => ({
  entries: [],
  searchQuery: '',
  selectedEntries: [],
  setEntries: (entries) => set({ entries }),
  setSearchQuery: (query) => set({ searchQuery: query }),
  setSelectedEntries: (entries) => set({ selectedEntries: entries }),
  clearEntries: () => set({ entries: [], searchQuery: '', selectedEntries: [] }),
  updateEntryStatus: (entryIds, newStatus) => set((state) => ({
    entries: state.entries.map(entry => 
      entryIds.includes(entry.id) ? { ...entry, status: newStatus } : entry
    )
  })),
}));
