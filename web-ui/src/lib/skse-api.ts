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
  
  refreshBlacklist: () => {
    log('[SKSE_API] Refreshing blacklist data');
    SKSE_API.sendToSKSE('refreshBlacklist');
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

  updateWhitelistEntryAdvanced: (data: { id: number; notes: string; actorFilterNames: string[]; actorFilterFormIDs: string[]; factionFilterEditorIDs: string[] }) => {
    log(`[SKSE_API] Updating whitelist entry (advanced) ${data.id}`);
    SKSE_API.sendToSKSE('updateWhitelistEntryAdvanced', JSON.stringify(data));
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
