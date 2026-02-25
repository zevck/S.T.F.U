import { create } from 'zustand';

export interface SettingsState {
  // Master controls
  blacklistEnabled: boolean;
  skyrimNetEnabled: boolean;
  scenesEnabled: boolean;
  bardSongsEnabled: boolean;
  followerCommentaryEnabled: boolean;
  combatGruntsBlocked: boolean;
  
  // Subtype states (keyed by subtype ID)
 subtypes: Record<number, boolean>;
  
  // Actions
  setSettings: (settings: Partial<Omit<SettingsState, 'setSettings'>>) => void;
}

export const useSettingsStore = create<SettingsState>((set) => ({
  // Initial defaults (will be overwritten by C++ data)
  blacklistEnabled: true,
  skyrimNetEnabled: true,
  scenesEnabled: true,
  bardSongsEnabled: true,
  followerCommentaryEnabled: true,
  combatGruntsBlocked: false,
  subtypes: {},
  
  setSettings: (settings) => set((state) => ({ ...state, ...settings })),
}));
