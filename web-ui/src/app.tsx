import { useEffect, useState, useRef } from 'react';
import { History } from './components/history';
import { Blacklist } from './components/blacklist';
import { Whitelist } from './components/whitelist';
import { Settings } from './components/settings';
import { Toast } from './components/toast';
import { useHistoryStore } from './stores/history';
import { useBlacklistStore } from './stores/blacklist';
import { useWhitelistStore } from './stores/whitelist';
import { useSettingsStore } from './stores/settings';
import { SKSE_API, log } from './lib/skse-api';
import { DialogueEntry, BlacklistEntry } from './types';
import { List, Ban, CheckCircle, Settings as SettingsIcon, Maximize2, Minimize2, Move, X } from 'lucide-react';

type Tab = 'history' | 'blacklist' | 'whitelist' | 'settings';

export const App = () => {
  const [activeTab, setActiveTab] = useState<Tab>('history');
  const prevTabRef = useRef<Tab>('history');
  const [isFullscreen, setIsFullscreen] = useState(() => {
    // Load fullscreen preference from localStorage
    const saved = localStorage.getItem('stfu-fullscreen');
    return saved === null ? true : saved === 'true'; // Default to fullscreen
  });

  // Window dragging state
  const [windowPos, setWindowPos] = useState({ x: 0, y: 0 });
  const [isDragging, setIsDragging] = useState(false);
  const [dragStart, setDragStart] = useState({ x: 0, y: 0 });
  const dragHandleRef = useRef<HTMLDivElement>(null);

  // Apply fullscreen class to root element
  useEffect(() => {
    const root = document.getElementById('root');
    if (root) {
      if (isFullscreen) {
        root.classList.remove('windowed');
        root.classList.add('fullscreen');
        // Remove inline transform to use CSS default
        root.style.transform = '';
      } else {
        root.classList.remove('fullscreen');
        root.classList.add('windowed');
        // Only apply transform if there's an offset, otherwise let CSS handle centering
        if (windowPos.x !== 0 || windowPos.y !== 0) {
          root.style.transform = `translate(calc(-50% + ${windowPos.x}px), calc(-50% + ${windowPos.y}px))`;
        } else {
          root.style.transform = '';
        }
      }
    }
  }, [isFullscreen, windowPos]);

  const toggleFullscreen = () => {
    const newValue = !isFullscreen;
    setIsFullscreen(newValue);
    localStorage.setItem('stfu-fullscreen', String(newValue));
    log(`[App] Fullscreen mode: ${newValue}`);
    // Reset window position when switching modes
    if (newValue) {
      setWindowPos({ x: 0, y: 0 });
    }
  };

  // Drag handlers
  const handleDragStart = (e: React.MouseEvent) => {
    if (isFullscreen) return;
    setIsDragging(true);
    setDragStart({ x: e.clientX - windowPos.x, y: e.clientY - windowPos.y });
  };

  useEffect(() => {
    if (!isDragging) return;

    const handleMouseMove = (e: MouseEvent) => {
      const newX = e.clientX - dragStart.x;
      const newY = e.clientY - dragStart.y;
      setWindowPos({ x: newX, y: newY });
    };

    const handleMouseUp = () => {
      setIsDragging(false);
    };

    window.addEventListener('mousemove', handleMouseMove);
    window.addEventListener('mouseup', handleMouseUp);

    return () => {
      window.removeEventListener('mousemove', handleMouseMove);
      window.removeEventListener('mouseup', handleMouseUp);
    };
  }, [isDragging, dragStart]);

  useEffect(() => {
    log('[App] useEffect started - initializing...');
    
    // Initialize SKSE_API to set up window.SKSE_API for C++ callbacks
    SKSE_API.init();
    
    // Subscribe to history updates from SKSE
    SKSE_API.subscribe('updateHistory', (jsonData: string) => {
      log('[App] updateHistory received');
      try {
        const entries = JSON.parse(jsonData) as DialogueEntry[];
        useHistoryStore.getState().setEntries(entries);
        log(`[App] Parsed ${entries.length} history entries`);
      } catch (error) {
        log(`[App] ERROR parsing history data: ${error}`);
      }
    });

    // Subscribe to blacklist updates from SKSE
    SKSE_API.subscribe('updateBlacklist', (jsonData: string) => {
      log('[App] updateBlacklist received');
      try {
        const entries = JSON.parse(jsonData) as BlacklistEntry[];
        useBlacklistStore.getState().setEntries(entries);
        log(`[App] Parsed ${entries.length} blacklist entries`);
      } catch (error) {
        log(`[App] ERROR parsing blacklist data: ${error}`);
      }
    });

    // Subscribe to whitelist updates from SKSE
    SKSE_API.subscribe('updateWhitelist', (jsonData: string) => {
      log('[App] updateWhitelist received');
      try {
        const entries = JSON.parse(jsonData) as BlacklistEntry[];
        useWhitelistStore.getState().setEntries(entries);
        log(`[App] Parsed ${entries.length} whitelist entries`);
      } catch (error) {
        log(`[App] ERROR parsing whitelist data: ${error}`);
      }
    });

    // Subscribe to settings updates from SKSE
    SKSE_API.subscribe('updateSettings', (jsonData: string) => {
      log('[App] updateSettings received');
      try {
        const data = JSON.parse(jsonData);
        useSettingsStore.getState().setSettings(data);
        log('[App] Updated settings from C++');
      } catch (error) {
        log(`[App] ERROR parsing settings data: ${error}`);
      }
    });

    // Check if JS listeners are registered
    log(`[App] Checking registered JS listeners...`);
    log(`[App] requestHistory exists: ${typeof (window as any).requestHistory === 'function'}`);
    log(`[App] requestBlacklist exists: ${typeof (window as any).requestBlacklist === 'function'}`);
    log(`[App] closeMenu exists: ${typeof (window as any).closeMenu === 'function'}`);
    log(`[App] jsLog exists: ${typeof (window as any).jsLog === 'function'}`);
    log(`[App] addToBlacklist exists: ${typeof (window as any).addToBlacklist === 'function'}`);
    log(`[App] addToWhitelist exists: ${typeof (window as any).addToWhitelist === 'function'}`);
    log(`[App] deleteBlacklistEntry exists: ${typeof (window as any).deleteBlacklistEntry === 'function'}`);

    // Request initial data
    SKSE_API.sendToSKSE('requestHistory');
    SKSE_API.sendToSKSE('requestBlacklist');
    SKSE_API.sendToSKSE('requestSettings', '');
    
    // Poll for settings updates every second (to catch MCM changes)
    const settingsInterval = setInterval(() => {
      SKSE_API.sendToSKSE('requestSettings', '');
    }, 1000);

    // ESC key handler to close menu
    const handleKeyDown = (e: KeyboardEvent) => {
      log(`[App] Key pressed: ${e.key} (keyCode: ${e.keyCode})`);
      // Use keyCode because Ultralight reports ESC as "Unidentified"
      if (e.keyCode === 27 || e.key === 'Escape') {
        log('[App] ESC pressed, calling closeMenu...');
        SKSE_API.sendToSKSE('closeMenu');
      }
    };
    
    window.addEventListener('keydown', handleKeyDown);

    return () => {
      SKSE_API.unsubscribe('updateHistory');
      SKSE_API.unsubscribe('updateBlacklist');
      SKSE_API.unsubscribe('updateWhitelist');
      SKSE_API.unsubscribe('updateSettings');
      clearInterval(settingsInterval);
      window.removeEventListener('keydown', handleKeyDown);
    };
  }, []);

  // Refresh data when switching tabs
  useEffect(() => {
    if (prevTabRef.current !== activeTab) {
      if (activeTab === 'history') {
        log('[App] Switching to history tab - requesting refresh');
        SKSE_API.requestHistoryRefresh();
      } else if (activeTab === 'blacklist') {
        log('[App] Switching to blacklist tab - requesting refresh');
        SKSE_API.requestBlacklistRefresh();
      }
      prevTabRef.current = activeTab;
    }
  }, [activeTab]);

  return (
    <>
    <div className="w-full h-full bg-gray-900 text-white flex flex-col">
      {/* Draggable Header (Windowed Mode Only) */}
      {!isFullscreen && (
        <div
          ref={dragHandleRef}
          onMouseDown={handleDragStart}
          className="bg-gray-800 border-b border-gray-700 px-4 py-2 flex items-center justify-between cursor-move select-none"
          style={{ cursor: isDragging ? 'grabbing' : 'grab' }}
        >
          <div className="flex items-center gap-2 text-gray-400">
            <Move size={16} />
            <span className="text-sm font-semibold">STFU - Dialogue Manager</span>
          </div>
          <div className="flex items-center gap-2">
            <button
              onClick={toggleFullscreen}
              className="px-3 py-1 bg-gray-700 hover:bg-gray-600 rounded text-xs transition-colors flex items-center gap-1"
              title="Switch to Fullscreen Mode"
            >
              <Maximize2 size={14} />
              Fullscreen
            </button>
            <button
              onClick={() => SKSE_API.sendToSKSE('closeMenu')}
              className="px-2 py-1 bg-red-600 hover:bg-red-500 rounded text-xs transition-colors flex items-center"
              title="Close Menu"
              onMouseDown={(e) => e.stopPropagation()}
            >
              <X size={14} />
            </button>
          </div>
        </div>
      )}

      {/* Main Content */}
      <div className="flex-1 flex flex-col p-4 overflow-hidden">
        {/* Header with Tab Navigation and Fullscreen Toggle */}
        <div className="flex justify-between items-center mb-4">
          <div className="flex gap-2">
            <button
              onClick={() => setActiveTab('history')}
              className={`flex items-center gap-2 px-4 py-2 rounded-lg transition-colors ${
                activeTab === 'history'
                  ? 'bg-blue-600 text-white'
                  : 'bg-gray-800 text-gray-400 hover:bg-gray-700'
              }`}
            >
              <List size={18} />
              History
            </button>
            <button
              onClick={() => setActiveTab('blacklist')}
              className={`flex items-center gap-2 px-4 py-2 rounded-lg transition-colors ${
                activeTab === 'blacklist'
                  ? 'bg-red-600 text-white'
                  : 'bg-gray-800 text-gray-400 hover:bg-gray-700'
              }`}
            >
              <Ban size={18} />
              Blacklist
            </button>
            <button
              onClick={() => setActiveTab('whitelist')}
              className={`flex items-center gap-2 px-4 py-2 rounded-lg transition-colors ${
                activeTab === 'whitelist'
                  ? 'bg-green-600 text-white'
                  : 'bg-gray-800 text-gray-400 hover:bg-gray-700'
              }`}
            >
              <CheckCircle size={18} />
              Whitelist
            </button>
            <button
              onClick={() => setActiveTab('settings')}
              className={`flex items-center gap-2 px-4 py-2 rounded-lg transition-colors ${
                activeTab === 'settings'
                  ? 'bg-purple-600 text-white'
                  : 'bg-gray-800 text-gray-400 hover:bg-gray-700'
              }`}
            >
              <SettingsIcon size={18} />
              Settings
            </button>
          </div>
          
          {/* Fullscreen Toggle and Close (Fullscreen Mode Only) */}
          {isFullscreen && (
            <div className="flex items-center gap-2">
              <button
                onClick={toggleFullscreen}
                className="flex items-center gap-2 px-4 py-2 bg-gray-800 text-gray-400 hover:bg-gray-700 rounded-lg transition-colors"
                title="Switch to Windowed Mode"
              >
                <Minimize2 size={18} />
                Windowed
              </button>
              <button
                onClick={() => SKSE_API.sendToSKSE('closeMenu')}
                className="flex items-center px-3 py-2 bg-red-600 text-white hover:bg-red-500 rounded-lg transition-colors"
                title="Close Menu"
              >
                <X size={18} />
              </button>
            </div>
          )}
        </div>

      {/* Tab Content */}
      <div className="flex-1 overflow-hidden">
        {activeTab === 'history' && <History />}
        {activeTab === 'blacklist' && <Blacklist />}
        {activeTab === 'whitelist' && <Whitelist />}
        {activeTab === 'settings' && <Settings />}
      </div>
      </div>
    </div>
    <Toast />
    </>
  );
};
