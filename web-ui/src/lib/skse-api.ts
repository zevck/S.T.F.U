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
};
