const listeners: { eventName: string; callback: (...args: any[]) => any }[] = [];

// Helper to send logs to SKSE log file
export const log = (message: string) => {
  try {
    if (typeof (window as any).jsLog === 'function') {
      (window as any).jsLog(message);
    }
  } catch (e) {
    // Fallback to console if jsLog isn't available
    console.log(message);
  }
};

export const SKSE_API = {
  init: () => {
    log('[SKSE_API] Initializing...');
    window.SKSE_API = {
      call: (eventName: string, ...args: any[]) => {
        log(`[SKSE_API] Event received: ${eventName}`);
        const filtered = listeners.filter((listener) => listener.eventName === eventName);

        for (const listener of filtered) {
          listener.callback(...args);
        }
      },
    };
    log('[SKSE_API] Initialized');
  },
  subscribe: (eventName: string, callback: (...args: any[]) => any) => {
    if (!window.SKSE_API) {
      throw new Error("Global SKSE_API doesn't exist!");
    }

    if (listeners.some((l) => l.eventName === eventName)) {
      log(`[SKSE_API] Subscriber for ${eventName} already exists, skipping...`);
      return;
    }

    listeners.push({ eventName, callback });
    log(`[SKSE_API] Subscribed to ${eventName}`);
  },
  unsubscribe: (eventName: string) => {
    if (!window.SKSE_API) {
      throw new Error("Global SKSE_API doesn't exist!");
    }

    const index = listeners.findIndex((l) => l.eventName === eventName);

    if (index !== -1) listeners.splice(index, 1);
  },
  sendToSKSE: (fnName: string, data?: string) => {
    log(`[SKSE_API] sendToSKSE called: ${fnName}${data ? ', data: ' + data : ''}`);
    try {
      // Call global function exposed by PrismaUI RegisterJSListener
      // @ts-ignore
      const fn = window[fnName];
      if (typeof fn === 'function') {
        fn(data);
        log(`[SKSE_API] Successfully called ${fnName}`);
      } else {
        log(`[SKSE_API] ERROR: ${fnName} is not a function! Type: ${typeof fn}`);
      }
    } catch (error) {
      log(`[SKSE_API] ERROR calling ${fnName}: ${error}`);
    }
  },
  
  // Blacklist management methods
  deleteBlacklistEntry: (id: number) => {
    log(`[SKSE_API] Deleting blacklist entry: ${id}`);
    SKSE_API.sendToSKSE('deleteBlacklistEntry', id.toString());
  },
  
  deleteBlacklistBatch: (ids: number[]) => {
    log(`[SKSE_API] Deleting ${ids.length} blacklist entries`);
    SKSE_API.sendToSKSE('deleteBlacklistBatch', ids.join(','));
  },
  
  clearBlacklist: () => {
    log('[SKSE_API] Clearing all blacklist entries');
    SKSE_API.sendToSKSE('clearBlacklist');
  },
  
  refreshBlacklist: () => {
    log('[SKSE_API] Refreshing blacklist data');
    SKSE_API.sendToSKSE('refreshBlacklist');
  },
  
  updateBlacklistEntry: (id: number, blockType: string, filterCategory: string, notes: string) => {
    log(`[SKSE_API] Updating blacklist entry ${id}: blockType=${blockType}, filterCategory=${filterCategory}`);
    // Escape notes for JSON
    const escapedNotes = notes.replace(/\\/g, '\\\\').replace(/"/g, '\\"').replace(/\n/g, '\\n');
    const jsonData = `{"id":${id},"blockType":"${blockType}","filterCategory":"${filterCategory}","notes":"${escapedNotes}"}`;
    SKSE_API.sendToSKSE('updateBlacklistEntry', jsonData);
  },
  
  updateBlacklistEntryAdvanced: (data: {
    id: number;
    blockType: string;
    filterCategory: string;
    notes: string;
    actorFilterNames: string[];
    actorFilterFormIDs: string[];
  }) => {
    log(`[SKSE_API] Updating blacklist entry (advanced) ${data.id}: blockType=${data.blockType}, actors=${data.actorFilterNames.length}`);
    const jsonData = JSON.stringify(data);
    SKSE_API.sendToSKSE('updateBlacklistEntryAdvanced', jsonData);
  },
  
  createAdvancedEntry: (data: {
    identifier: string;
    blockType: string;
    category: string;
    notes: string;
    isWhitelist: boolean;
    actorFilterNames: string[];
    actorFilterFormIDs: string[];
  }) => {
    log(`[SKSE_API] Creating advanced entry: identifier=${data.identifier}, actors=${data.actorFilterNames.length}`);
    const jsonData = JSON.stringify(data);
    SKSE_API.sendToSKSE('createAdvancedEntry', jsonData);
  },
  
  addToBlacklist: (entries: any[], blockType: 'Soft' | 'Hard' | 'SkyrimNet', filterCategory?: string, notes?: string) => {
    log(`[SKSE_API] addToBlacklist called with ${entries.length} entries, blockType: ${blockType}`);
    log(`[SKSE_API] Entries: ${JSON.stringify(entries)}`);
    const jsonData = JSON.stringify({ entries, blockType, filterCategory: filterCategory || '', notes: notes || '' });
    log(`[SKSE_API] JSON data: ${jsonData}`);
    log(`[SKSE_API] Calling sendToSKSE('addToBlacklist', jsonData)...`);
    SKSE_API.sendToSKSE('addToBlacklist', jsonData);
    log(`[SKSE_API] sendToSKSE call completed`);
  },
  
  addToWhitelist: (entries: any[], notes?: string) => {
    log(`[SKSE_API] addToWhitelist called with ${entries.length} entries`);
    log(`[SKSE_API] Entries: ${JSON.stringify(entries)}`);
    const jsonData = JSON.stringify({ entries, notes: notes || '' });
    log(`[SKSE_API] JSON data: ${jsonData}`);
    log(`[SKSE_API] Calling sendToSKSE('addToWhitelist', jsonData)...`);
    SKSE_API.sendToSKSE('addToWhitelist', jsonData);
    log(`[SKSE_API] sendToSKSE call completed`);
  },
  
  removeFromBlacklist: (entry: any) => {
    log(`[SKSE_API] removeFromBlacklist called - topicEditorID=${entry.topicEditorID}, topicFormID=${entry.topicFormID}, sceneEditorID=${entry.sceneEditorID}, isScene=${entry.isScene}`);
    const jsonData = JSON.stringify({
      topicFormID: entry.topicFormID || '',
      topicEditorID: entry.topicEditorID || '',
      sceneEditorID: entry.sceneEditorID || '',
      isScene: entry.isScene || false
    });
    log(`[SKSE_API] Sending JSON to C++: ${jsonData}`);
    SKSE_API.sendToSKSE('removeFromBlacklist', jsonData);
  },

  toggleSubtypeFilter: (topicSubtype: number) => {
    const jsonData = JSON.stringify({
      topicSubtype
    });
    SKSE_API.sendToSKSE('toggleSubtypeFilter', jsonData);
  },

  deleteHistoryEntries: (entryIds: number[]) => {
    const jsonData = JSON.stringify({
      entryIds
    });
    SKSE_API.sendToSKSE('deleteHistoryEntries', jsonData);
  },

  requestHistoryRefresh: () => {
    SKSE_API.sendToSKSE('requestHistory', '');
  },

  requestBlacklistRefresh: () => {
    SKSE_API.sendToSKSE('requestBlacklist', '');
  },

  // Whitelist management methods
  requestWhitelistRefresh: () => {
    log('[SKSE_API] Requesting whitelist refresh');
    SKSE_API.sendToSKSE('requestWhitelist', '');
  },

  removeFromWhitelist: (id: number) => {
    log(`[SKSE_API] Removing whitelist entry: ${id}`);
    const jsonData = JSON.stringify({ id });
    SKSE_API.sendToSKSE('removeFromWhitelist', jsonData);
  },

  updateWhitelistEntry: (updates: { id: number; filterCategory: string; note: string }) => {
    log(`[SKSE_API] Updating whitelist entry ${updates.id}`);
    const escapedNote = updates.note.replace(/\\/g, '\\\\').replace(/"/g, '\\"').replace(/\n/g, '\\n');
    const jsonData = `{"id":${updates.id},"filterCategory":"${updates.filterCategory}","note":"${escapedNote}"}`;
    SKSE_API.sendToSKSE('updateWhitelistEntry', jsonData);
  },

  moveToBlacklist: (id: number) => {
    log(`[SKSE_API] Moving whitelist entry ${id} to blacklist`);
    const jsonData = JSON.stringify({ id });
    SKSE_API.sendToSKSE('moveToBlacklist', jsonData);
  },

  moveToWhitelist: (id: number) => {
    log(`[SKSE_API] Moving blacklist entry ${id} to whitelist`);
    const jsonData = JSON.stringify({ id });
    SKSE_API.sendToSKSE('moveToWhitelist', jsonData);
  },

  removeWhitelistBatch: (ids: number[]) => {
    log(`[SKSE_API] Removing ${ids.length} whitelist entries`);
    const jsonData = JSON.stringify({ ids });
    SKSE_API.sendToSKSE('removeWhitelistBatch', jsonData);
  },

  // Settings methods
  importScenes: () => {
    log('[SKSE_API] Importing hardcoded scenes');
    SKSE_API.sendToSKSE('importScenes', '');
  },

  importYAML: () => {
    log('[SKSE_API] Importing from YAML files');
    SKSE_API.sendToSKSE('importYAML', '');
  },

  setCombatGruntsBlocked: (blocked: boolean) => {
    log(`[SKSE_API] Setting combat grunts blocked: ${blocked}`);
    const jsonData = JSON.stringify({ blocked });
    SKSE_API.sendToSKSE('setCombatGruntsBlocked', jsonData);
  },

  setFollowerCommentaryEnabled: (enabled: boolean) => {
    log(`[SKSE_API] Setting follower commentary enabled: ${enabled}`);
    const jsonData = JSON.stringify({ enabled });
    SKSE_API.sendToSKSE('setFollowerCommentaryEnabled', jsonData);
  },

  setBlacklistEnabled: (enabled: boolean) => {
    log(`[SKSE_API] Setting blacklist enabled: ${enabled}`);
    const jsonData = JSON.stringify({ enabled });
    SKSE_API.sendToSKSE('setBlacklistEnabled', jsonData);
  },

  setSkyrimNetEnabled: (enabled: boolean) => {
    log(`[SKSE_API] Setting SkyrimNet enabled: ${enabled}`);
    const jsonData = JSON.stringify({ enabled });
    SKSE_API.sendToSKSE('setSkyrimNetEnabled', jsonData);
  },

  setScenesEnabled: (enabled: boolean) => {
    log(`[SKSE_API] Setting scenes enabled: ${enabled}`);
    const jsonData = JSON.stringify({ enabled });
    SKSE_API.sendToSKSE('setScenesEnabled', jsonData);
  },

  setBardSongsEnabled: (enabled: boolean) => {
    log(`[SKSE_API] Setting bard songs enabled: ${enabled}`);
    const jsonData = JSON.stringify({ enabled });
    SKSE_API.sendToSKSE('setBardSongsEnabled', jsonData);
  },
  
  requestNearbyActors: () => {
    log('[SKSE_API] Requesting nearby actors');
    SKSE_API.sendToSKSE('getNearbyActors', '');
  },
};
