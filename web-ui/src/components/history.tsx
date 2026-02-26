import { useMemo, useState, memo, useRef, useEffect, useCallback } from 'react';
import { useHistoryStore } from '@/stores/history';
import { useBlacklistStore } from '@/stores/blacklist';
import { useWhitelistStore } from '@/stores/whitelist';
import { DialogueEntry } from '@/types';
import { SKSE_API, log } from '@/lib/skse-api';
import { ResponsesModal } from './responses-modal';
import { AdvancedEntryModal } from './advanced-entry-modal';
import { AdvancedEditModal } from './advanced-edit-modal';
import { Search, Trash2, Save, Plus, Settings } from 'lucide-react';

const getStatusColor = (status: DialogueEntry['status']): string => {
  switch (status) {
    case 'Allowed':
      return 'text-green-400';
    case 'Soft Block':
      return 'text-orange-400';
    case 'Hard Block':
      return 'text-red-400';
    case 'Skyrim':
      return 'text-gray-400';
    case 'SkyrimNet Block':
      return 'text-indigo-400';
    case 'Filter':
      return 'text-yellow-400';  // Filter displays as "Soft Block" in yellow
    case 'Toggled Off':
      return 'text-cyan-400';
    case 'Whitelist':
      return 'text-white'; // Whitelist entries shown in white
    default:
      return 'text-white';
  }
};

const getStatusDisplay = (status: DialogueEntry['status']): string => {
  // Short names for history list
  switch (status) {
    case 'Toggled Off':
    case 'Skyrim':
    case 'Whitelist':
      return 'Allowed';
    case 'Filter':
      return 'Soft Blocked';  // Filter displays as Soft Blocked (yellow color)
    case 'Soft Block':
      return 'Soft Blocked';
    case 'Hard Block':
      return 'Hard Blocked';
    case 'SkyrimNet Block':
      return 'SkyrimNet Blocked';
    default:
      return status;
  }
};

const getStatusDescription = (status: DialogueEntry['status']): string => {
  // Descriptive text for detail panel
  switch (status) {
    case 'Allowed':
    case 'Toggled Off':
    case 'Skyrim':
      return 'Allowed';
    case 'Soft Block':
      return 'Soft blocked by blacklist';
    case 'Hard Block':
      return 'Hard blocked by blacklist';
    case 'SkyrimNet Block':
      return 'SkyrimNet blocked by blacklist';
    case 'Filter':
      return 'Soft blocked by filter';
    case 'Whitelist':
      return 'Allowed by whitelist';
    default:
      return status;
  }
};

const formatTimestamp = (timestamp: number): string => {
  const date = new Date(timestamp * 1000); // Convert from seconds to milliseconds
  return date.toLocaleTimeString();
};

// Memoized history item component
const HistoryItem = memo(({ 
  entry, 
  isSelected, 
  onClick,
  index
}: { 
  entry: DialogueEntry; 
  isSelected: boolean; 
  onClick: (event: React.MouseEvent) => void;
  index: number;
}) => (
  <div
    onClick={onClick}
    className={`p-4 border-b border-gray-700 cursor-pointer ${
      isSelected 
        ? 'bg-blue-900/50 border-l-4 border-blue-500' 
        : index % 2 === 0 
        ? 'bg-gray-800 hover:bg-blue-900/20 border-l-4 border-transparent' 
        : 'bg-gray-850 hover:bg-blue-900/20 border-l-4 border-transparent'
    }`}
  >
    <div className="flex items-center justify-between mb-1">
      <span className="text-base text-gray-400">{formatTimestamp(entry.timestamp)}</span>
      <span className={`text-base font-bold ${getStatusColor(entry.status)}`}>
        {getStatusDisplay(entry.status)}
      </span>
    </div>
    <div className="text-lg mb-1">
      <span className="text-green-300">{entry.speaker}</span>
      {entry.isScene && <span className="ml-2 text-cyan-400">[Scene]</span>}
      {entry.skyrimNetBlockable && <span className="ml-2 text-purple-400">[Menu]</span>}
    </div>
    <div className="text-lg text-gray-300 truncate">{entry.text}</div>
  </div>
));

HistoryItem.displayName = 'HistoryItem';

export const History = () => {
  const { entries, searchQuery, setSearchQuery, selectedEntries, setSelectedEntries } = useHistoryStore();
  const { entries: blacklistEntries } = useBlacklistStore();
  const { entries: whitelistEntries } = useWhitelistStore();
  const [lastClickedIndex, setLastClickedIndex] = useState<number>(-1);
  const [displayCount, setDisplayCount] = useState(100); // Increased initial load
  const sentinelRef = useRef<HTMLDivElement>(null);
  const containerRef = useRef<HTMLDivElement>(null);
  const listRef = useRef<HTMLDivElement>(null);
  const [showResponsesModal, setShowResponsesModal] = useState(false);
  const [modalTitle, setModalTitle] = useState('');
  const [modalResponses, setModalResponses] = useState<string[]>([]);
  const [statusFilters, setStatusFilters] = useState<Set<string>>(new Set(['Allowed', 'Soft Blocked', 'Hard Blocked', 'SkyrimNet Blocked', 'Whitelisted']));
  const [showAdvancedEntryModal, setShowAdvancedEntryModal] = useState(false);
  const [showBlacklistEditModal, setShowBlacklistEditModal] = useState(false);
  const [showWhitelistEditModal, setShowWhitelistEditModal] = useState(false);
  const [editingBlacklistEntry, setEditingBlacklistEntry] = useState<any>(null);
  const [editingWhitelistEntry, setEditingWhitelistEntry] = useState<any>(null);
  
  // For single-item detail panel (first selected item)
  const selectedEntry = selectedEntries.length > 0 ? selectedEntries[0] : null;
  
  // Edit state for selected entry
  const [editSoftBlock, setEditSoftBlock] = useState<boolean>(false);
  const [editHardBlock, setEditHardBlock] = useState<boolean>(false);
  const [editSkyrimNetBlock, setEditSkyrimNetBlock] = useState<boolean>(false);
  const [editWhitelist, setEditWhitelist] = useState<boolean>(false);
  const [editFilterCategory, setEditFilterCategory] = useState<string>('Blacklist');
  const [editNotes, setEditNotes] = useState<string>('');

  // Find corresponding blacklist/whitelist entries for selected history entry
  const blacklistEntry = useMemo(() => {
    if (!selectedEntry) return null;
    return blacklistEntries.find(bl => 
      (bl.topicFormID === selectedEntry.topicFormID && bl.topicFormID !== '' && bl.topicFormID !== undefined) ||
      (bl.topicEditorID === selectedEntry.topicEditorID && bl.topicEditorID !== '' && bl.topicEditorID !== undefined)
    );
  }, [selectedEntry, blacklistEntries]);

  const whitelistEntry = useMemo(() => {
    if (!selectedEntry) return null;
    return whitelistEntries.find(wl => 
      (wl.topicFormID === selectedEntry.topicFormID && wl.topicFormID !== '' && wl.topicFormID !== undefined) ||
      (wl.topicEditorID === selectedEntry.topicEditorID && wl.topicEditorID !== '' && wl.topicEditorID !== undefined)
    );
  }, [selectedEntry, whitelistEntries]);

  // Toggle status filter
  const toggleStatusFilter = useCallback((status: string) => {
    setStatusFilters(prev => {
      const newFilters = new Set(prev);
      if (newFilters.has(status)) {
        newFilters.delete(status);
      } else {
        newFilters.add(status);
      }
      return newFilters;
    });
  }, []);

  // Reset filters
  const handleResetFilters = useCallback(() => {
    setSearchQuery('');
    setStatusFilters(new Set(['Allowed', 'Soft Blocked', 'Hard Blocked', 'SkyrimNet Blocked', 'Whitelisted']));
  }, [setSearchQuery]);
  
  // Initialize edit state when selected entry changes
  useEffect(() => {
    if (selectedEntry && selectedEntries.length === 1) {
      // Initialize checkboxes based on current status
      setEditSoftBlock(selectedEntry.status === 'Soft Block' || selectedEntry.status === 'Filter');
      setEditHardBlock(selectedEntry.status === 'Hard Block');
      setEditSkyrimNetBlock(selectedEntry.status === 'SkyrimNet Block');
      setEditWhitelist(selectedEntry.status === 'Whitelist');
      
      // If none are set (Allowed, Toggled Off, Skyrim), default to nothing checked
      if (selectedEntry.status === 'Allowed' || selectedEntry.status === 'Toggled Off' || selectedEntry.status === 'Skyrim') {
        setEditSoftBlock(false);
        setEditHardBlock(false);
        setEditSkyrimNetBlock(false);
        setEditWhitelist(false);
      }
      
      // Initialize filter category - always default to 'Blacklist'
      setEditFilterCategory('Blacklist');
      setEditNotes('');
    }
  }, [selectedEntry, selectedEntries.length]);

  // Update selectedEntries when entries change (e.g., after toggle)
  useEffect(() => {
    if (selectedEntries.length > 0) {
      const selectedIds = selectedEntries.map(e => e.id);
      const updatedSelected = entries.filter(e => selectedIds.includes(e.id));
      
      // Only update if entries actually changed (not just selection changed)
      const hasChanges = updatedSelected.some((updated, idx) => {
        const current = selectedEntries[idx];
        return current && (updated.status !== current.status || updated.topicSubtype !== current.topicSubtype);
      });
      
      if (hasChanges && updatedSelected.length > 0) {
        setSelectedEntries(updatedSelected);
      }
    }
  }, [entries]);

  const filteredEntries = useMemo(() => {
    let filtered = entries;
    
    // Apply search filter
    if (searchQuery) {
      filtered = filtered.filter(
        (entry) =>
          entry.text.toLowerCase().includes(searchQuery.toLowerCase()) ||
          entry.speaker.toLowerCase().includes(searchQuery.toLowerCase()) ||
          entry.questName.toLowerCase().includes(searchQuery.toLowerCase()) ||
          entry.topicEditorID.toLowerCase().includes(searchQuery.toLowerCase())
      );
    }
    
    // Apply status filters - show entries whose status is checked
    filtered = filtered.filter(entry => {
      // Normalize status for filtering
      const normalizedStatus = getStatusDisplay(entry.status);
      return statusFilters.has(normalizedStatus);
    });
    
    // Reverse so oldest entries are at top, newest at bottom (like a chat log)
    return [...filtered].reverse();
  }, [entries, searchQuery, statusFilters]);

  // Lazy loading with IntersectionObserver
  useEffect(() => {
    const sentinel = sentinelRef.current;
    if (!sentinel) return;

    const observer = new IntersectionObserver(
      (entries) => {
        // Load more when sentinel becomes visible
        if (entries[0].isIntersecting && displayCount < filteredEntries.length) {
          setDisplayCount(prev => Math.min(prev + 100, filteredEntries.length));
        }
      },
      { threshold: 0.5, rootMargin: '200px' } // Load earlier with margin
    );

    observer.observe(sentinel);
    return () => observer.disconnect();
  }, [displayCount, filteredEntries.length]);

  // Reset display count when search changes
  useEffect(() => {
    setDisplayCount(100);
  }, [searchQuery]);

  // Slice entries for lazy loading
  const displayedEntries = useMemo(() => {
    return filteredEntries.slice(0, displayCount);
  }, [filteredEntries, displayCount]);

  // Auto-scroll to bottom when entries change (like a chat log)
  useEffect(() => {
    if (listRef.current) {
      listRef.current.scrollTop = listRef.current.scrollHeight;
    }
  }, [entries.length]);
  
  // Check if all selected entries are SkyrimNet blockable (menu topics)
  const allSelectedAreSkyrimNetBlockable = useMemo(() => {
    return selectedEntries.length > 0 && selectedEntries.every(e => e.skyrimNetBlockable);
  }, [selectedEntries]);
  
  const handleAddToBlacklist = useCallback((blockType: 'Soft' | 'Hard' | 'SkyrimNet') => {
    log(`[History] handleAddToBlacklist called with blockType: ${blockType}`);
    // Use selected entries array if available, otherwise use single selected entry
    const entriesToAdd = selectedEntries.length > 0 ? selectedEntries : (selectedEntry ? [selectedEntry] : []);
    if (entriesToAdd.length > 0) {
      log(`[History] Adding to blacklist, ${entriesToAdd.length} entries:`);
      entriesToAdd.forEach((e, idx) => {
        log(`  Entry ${idx}: topicEditorID=${e.topicEditorID}, topicFormID=${e.topicFormID}, questEditorID=${e.questEditorID}, questName=${e.questName}, sceneEditorID=${e.sceneEditorID}, isScene=${e.isScene}`);
      });
    }
    if (entriesToAdd.length === 0) {
      log(`[History] No entries to add, returning`);
      return;
    }
    
    // Call SKSE API to add entries to blacklist
    // The C++ backend will update the database and send back refreshed history data
    log(`[History] Calling SKSE_API.addToBlacklist...`);
    SKSE_API.addToBlacklist(entriesToAdd, blockType);
    log(`[History] SKSE_API.addToBlacklist call completed`);
    
    // Clear selection after adding
    setSelectedEntries([]);
    setLastClickedIndex(-1);
  }, [selectedEntries, selectedEntry]);
  
  const handleAddToWhitelist = useCallback(() => {
    log(`[History] handleAddToWhitelist called`);
    // Use selected entries array if available, otherwise use single selected entry
    const entriesToAdd = selectedEntries.length > 0 ? selectedEntries : (selectedEntry ? [selectedEntry] : []);
    if (entriesToAdd.length > 0) {
      log(`[History] Adding to whitelist, ${entriesToAdd.length} entries:`);
      entriesToAdd.forEach((e, idx) => {
        log(`  Entry ${idx}: topicEditorID=${e.topicEditorID}, topicFormID=${e.topicFormID}, questEditorID=${e.questEditorID}, questName=${e.questName}, sceneEditorID=${e.sceneEditorID}, isScene=${e.isScene}`);
      });
    }
    if (entriesToAdd.length === 0) {
      log(`[History] No entries to add, returning`);
      return;
    }
    
    // Call SKSE API to add entries to whitelist
    log(`[History] Calling SKSE_API.addToWhitelist...`);
    SKSE_API.addToWhitelist(entriesToAdd);
    log(`[History] SKSE_API.addToWhitelist call completed`);
    
    // Clear selection
    setSelectedEntries([]);
    setLastClickedIndex(-1);
  }, [selectedEntries, selectedEntry]);
  
  const handleRemoveFromBlacklist = useCallback(() => {
    const entryToRemove = selectedEntries.length > 0 ? selectedEntries[0] : selectedEntry;
    if (!entryToRemove) return;
    
    log(`[History] Removing from blacklist, entry: topicEditorID=${entryToRemove.topicEditorID}, topicFormID=${entryToRemove.topicFormID}, sceneEditorID=${entryToRemove.sceneEditorID}, isScene=${entryToRemove.isScene}, questEditorID=${entryToRemove.questEditorID}`);
    SKSE_API.removeFromBlacklist(entryToRemove);
    
    // Request immediate refresh to show updated status
    setTimeout(() => {
      log('[History] Requesting refresh after remove from blacklist');
      SKSE_API.requestHistoryRefresh();
    }, 100);
    
    // Clear selection
    setSelectedEntries([]);
    setLastClickedIndex(-1);
  }, [selectedEntries, selectedEntry]);
  
  const handleApplyChanges = useCallback(() => {
    if (!selectedEntry || selectedEntries.length !== 1) return;
    
    log(`[History] handleApplyChanges called for entry ${selectedEntry.id}`);
    
    // Determine action based on checkboxes
    if (editWhitelist) {
      // Add to whitelist
      log(`[History] Applying whitelist with notes: ${editNotes}`);
      SKSE_API.addToWhitelist([selectedEntry], editNotes);
    } else if (editSoftBlock) {
      // Add to blacklist with Soft block
      log(`[History] Applying Soft block with category=${editFilterCategory}, notes=${editNotes}`);
      SKSE_API.addToBlacklist([selectedEntry], 'Soft', editFilterCategory, editNotes);
    } else if (editHardBlock) {
      // Add to blacklist with Hard block
      log(`[History] Applying Hard block with category=${editFilterCategory}, notes=${editNotes}`);
      SKSE_API.addToBlacklist([selectedEntry], 'Hard', editFilterCategory, editNotes);
    } else if (editSkyrimNetBlock) {
      // Add to blacklist with SkyrimNet block
      log(`[History] Applying SkyrimNet block with notes=${editNotes}`);
      SKSE_API.addToBlacklist([selectedEntry], 'SkyrimNet', 'SkyrimNet', editNotes);
    }
    
    // Clear selection
    setSelectedEntries([]);
    setLastClickedIndex(-1);
  }, [selectedEntry, selectedEntries.length, editSoftBlock, editHardBlock, editSkyrimNetBlock, editWhitelist, editFilterCategory, editNotes, setSelectedEntries]);
  
  const handleShowAllResponses = useCallback(() => {
    if (!selectedEntry) return;
    
    const responses = selectedEntry.allResponses && selectedEntry.allResponses.length > 0
      ? selectedEntry.allResponses
      : selectedEntry.text
      ? [selectedEntry.text]
      : [];
    
    const identifier = selectedEntry.isScene
      ? selectedEntry.sceneEditorID || 'Unknown Scene'
      : selectedEntry.topicEditorID || selectedEntry.topicFormID || 'Unknown';
    setModalTitle(`All Responses: ${identifier}`);
    setModalResponses(responses);
    setShowResponsesModal(true);
  }, [selectedEntry]);
  
  const handleToggleSubtypeFilter = useCallback(() => {
    if (!selectedEntry || selectedEntry.topicSubtype === 0) return;
    
    log(`[History] Toggling subtype filter for subtype: ${selectedEntry.topicSubtype}`);
    SKSE_API.toggleSubtypeFilter(selectedEntry.topicSubtype);
    
    // Request immediate refresh to show updated status
    setTimeout(() => {
      log('[History] Requesting refresh after toggle');
      SKSE_API.requestHistoryRefresh();
    }, 100);
  }, [selectedEntry]);
  
  // Handle multi-selection click
  const handleItemClick = useCallback((entry: DialogueEntry, index: number, event?: React.MouseEvent) => {
    const isCtrlClick = event?.ctrlKey || event?.metaKey;
    const isShiftClick = event?.shiftKey;
    
    if (isShiftClick && lastClickedIndex >= 0 && filteredEntries.length > 0) {
      // Shift-click: select range
      const start = Math.min(lastClickedIndex, index);
      const end = Math.max(lastClickedIndex, index);
      const rangeEntries = filteredEntries.slice(start, end + 1);
      setSelectedEntries(rangeEntries);
    } else if (isCtrlClick) {
      // Ctrl-click: toggle selection
      const isSelected = selectedEntries.some(e => e.id === entry.id);
      if (isSelected) {
        setSelectedEntries(selectedEntries.filter(e => e.id !== entry.id));
      } else {
        setSelectedEntries([...selectedEntries, entry]);
      }
      setLastClickedIndex(index);
    } else {
      // Normal click: single selection
      setSelectedEntries([entry]);
      setLastClickedIndex(index);
    }
    
    // Focus the container so DEL key works immediately
    log('[History] Item clicked, focusing container');
    setTimeout(() => {
      containerRef.current?.focus();
      const activeEl = document.activeElement;
      log(`[History] Container focus attempted, activeElement: ${activeEl?.className || 'unknown'}`);
    }, 10);
  }, [lastClickedIndex, filteredEntries, selectedEntries, setSelectedEntries]);
  
  // Keyboard support for DEL key - handle directly on the history container
  const handleKeyDown = (e: React.KeyboardEvent) => {
    log(`[History DEL] Key pressed: ${e.key}, selectedEntries: ${selectedEntries.length}`);
    if (e.key === 'Delete' && selectedEntries.length > 0) {
      log(`[History DEL] Handling delete, entry IDs: ${selectedEntries.map(e => e.id).join(', ')}`);
      e.preventDefault();
      e.stopPropagation();
      // Delete entries from dialogue_log table
      const entryIds = selectedEntries.map(entry => entry.id);
      log(`[History DEL] Calling deleteHistoryEntries with: ${JSON.stringify(entryIds)}`);
      SKSE_API.deleteHistoryEntries(entryIds);
      setSelectedEntries([]);
      setLastClickedIndex(-1);
      log('[History DEL] Delete complete, selection cleared');
    } else {
      log('[History DEL] Not handling - either not Delete key or no selection');
    }
  };

  return (
    <div 
      ref={containerRef}
      className="flex flex-col h-full" 
      tabIndex={0}
      onKeyDown={handleKeyDown}
      onFocus={() => log('[History] Container gained focus')}
      onBlur={() => log('[History] Container lost focus')}
      style={{ outline: 'none' }}
    >
      {/* Filter Panel */}
      <div className="bg-gray-800 rounded-lg p-4 mb-4 space-y-3">
        {/* Row 1: Search Bar */}
        <div className="relative">
          <Search className="absolute left-3 top-1/2 transform -translate-y-1/2 text-gray-400" size={20} />
          <input
            type="text"
            placeholder="Search dialogue history..."
            value={searchQuery}
            onChange={(e) => setSearchQuery(e.target.value)}
            className="w-full pl-10 pr-4 py-2.5 text-base bg-gray-700 text-white rounded-lg border border-gray-600 focus:outline-none focus:border-blue-500"
          />
        </div>
        
        {/* Row 2: Status Filters */}
        <div className="flex items-center gap-4 flex-wrap">
          <span className="text-base text-gray-300 font-medium">Status:</span>
          <label className="flex items-center gap-2 cursor-pointer">
            <input
              type="checkbox"
              checked={statusFilters.has('Allowed')}
              onChange={() => toggleStatusFilter('Allowed')}
              className="w-5 h-5 rounded border-gray-600 bg-gray-700 text-blue-600 focus:ring-blue-500 cursor-pointer"
            />
            <span className="text-base text-white">Allowed</span>
          </label>
          <label className="flex items-center gap-2 cursor-pointer">
            <input
              type="checkbox"
              checked={statusFilters.has('Soft Blocked')}
              onChange={() => toggleStatusFilter('Soft Blocked')}
              className="w-5 h-5 rounded border-gray-600 bg-gray-700 text-blue-600 focus:ring-blue-500 cursor-pointer"
            />
            <span className="text-base text-white">Soft Blocked</span>
          </label>
          <label className="flex items-center gap-2 cursor-pointer">
            <input
              type="checkbox"
              checked={statusFilters.has('Hard Blocked')}
              onChange={() => toggleStatusFilter('Hard Blocked')}
              className="w-5 h-5 rounded border-gray-600 bg-gray-700 text-blue-600 focus:ring-blue-500 cursor-pointer"
            />
            <span className="text-base text-white">Hard Blocked</span>
          </label>
          <label className="flex items-center gap-2 cursor-pointer">
            <input
              type="checkbox"
              checked={statusFilters.has('SkyrimNet Blocked')}
              onChange={() => toggleStatusFilter('SkyrimNet Blocked')}
              className="w-5 h-5 rounded border-gray-600 bg-gray-700 text-blue-600 focus:ring-blue-500 cursor-pointer"
            />
            <span className="text-base text-white">SkyrimNet Blocked</span>
          </label>
          <label className="flex items-center gap-2 cursor-pointer">
            <input
              type="checkbox"
              checked={statusFilters.has('Whitelisted')}
              onChange={() => toggleStatusFilter('Whitelisted')}
              className="w-5 h-5 rounded border-gray-600 bg-gray-700 text-blue-600 focus:ring-blue-500 cursor-pointer"
            />
            <span className="text-base text-white">Whitelisted</span>
          </label>
          <button
            onClick={handleResetFilters}
            className="px-4 py-2 text-base bg-gray-700 hover:bg-gray-600 text-white rounded-lg transition-colors"
          >
            Reset Filters
          </button>
        </div>
      </div>

      <div className="flex-1 flex gap-4 overflow-hidden">
        {/* Entry List */}
        <div ref={listRef} className="flex-1 overflow-y-auto border border-gray-700 rounded bg-gray-800">
          {filteredEntries.length === 0 ? (
            <div className="p-4 text-lg text-gray-500 text-center">No dialogue entries found</div>
          ) : (
            <>
              {displayedEntries.map((entry, index) => (
                <HistoryItem
                  key={entry.id}
                  entry={entry}
                  isSelected={selectedEntries.some(e => e.id === entry.id)}
                  onClick={(event) => handleItemClick(entry, index, event)}
                  index={index}
                />
              ))}
              {/* Sentinel element for lazy loading */}
              <div ref={sentinelRef} className="h-px" />
              {displayCount < filteredEntries.length && (
                <div className="p-4 text-center text-gray-400">
                  Loaded {displayCount} of {filteredEntries.length}
                </div>
              )}
            </>
          )}
        </div>

        {/* Detail Panel */}
        <div className="w-96 overflow-y-auto border border-gray-700 rounded bg-gray-800 p-4">
          {selectedEntry ? (
            <div className="space-y-3">
              <h2 className="text-xl font-bold mb-2">
                {selectedEntries.length > 1 
                  ? `${selectedEntries.length} Entries Selected` 
                  : 'Details'}
              </h2>

              {selectedEntries.length > 1 ? (
                <>
                  {/* Multi-selection info */}
                  <div className="text-base text-gray-300 bg-gray-700 rounded-lg p-4">
                    <p className="mb-2">Multiple entries selected</p>
                    <ul className="text-sm text-gray-400 space-y-1 max-h-96 overflow-y-auto">
                      {selectedEntries.map(e => (
                        <li key={e.id} className="truncate">
                          • <span className="text-green-300">{e.speaker}</span>: {e.text}
                        </li>
                      ))}
                    </ul>
                  </div>
                  
                  {/* Blacklist controls for multi-selection */}
                  <div className="space-y-2 border-t border-gray-700 pt-4">
                    <div className="text-base text-gray-400 font-medium mb-2">Add Selected to Blacklist</div>
                    <button
                      onClick={() => handleAddToBlacklist('Soft')}
                      className="w-full px-4 py-2 bg-yellow-600 hover:bg-yellow-700 text-white rounded-lg transition-colors disabled:opacity-50 disabled:cursor-not-allowed"
                    >
                      Soft Block
                    </button>
                    <button
                      onClick={() => handleAddToBlacklist('Hard')}
                      className="w-full px-4 py-2 bg-red-800 hover:bg-red-900 text-white rounded-lg transition-colors disabled:opacity-50 disabled:cursor-not-allowed"
                    >
                      Hard Block
                    </button>
                    {allSelectedAreSkyrimNetBlockable && (
                      <button
                        onClick={() => handleAddToBlacklist('SkyrimNet')}
                        className="w-full px-4 py-2 bg-indigo-500 hover:bg-indigo-600 text-white rounded-lg transition-colors disabled:opacity-50 disabled:cursor-not-allowed"
                      >
                        SkyrimNet Block
                      </button>
                    )}
                    {allSelectedAreSkyrimNetBlockable && (
                      <div className="text-xs text-green-400 italic">
                        ✓ All selected are SkyrimNet compatible
                      </div>
                    )}
                  </div>
                  
                  {/* Remove from Blacklist for multi-selection - show if any entry is blacklisted */}
                  {selectedEntries.some(e => e.status === 'Soft Block' || e.status === 'Hard Block' || e.status === 'SkyrimNet Block') && (
                    <div className="space-y-2 border-t border-gray-700 pt-4 mt-4">
                      <div className="text-base text-gray-400 font-medium mb-2">Remove from Blacklist</div>
                      <button
                        onClick={() => {
                          selectedEntries.forEach(entry => {
                            if (entry.status === 'Soft Block' || entry.status === 'Hard Block' || entry.status === 'SkyrimNet Block') {
                              SKSE_API.removeFromBlacklist(entry);
                            }
                          });
                          setSelectedEntries([]);
                          setLastClickedIndex(-1);
                        }}
                        className="w-full px-4 py-2 bg-red-600 hover:bg-red-700 text-white rounded-lg transition-colors"
                      >
                        Remove Selected from Blacklist
                      </button>
                    </div>
                  )}
                  
                  {/* Whitelist controls */}
                  <div className="space-y-2 border-t border-gray-700 pt-4 mt-4">
                    <div className="text-base text-gray-400 font-medium mb-2">Add Selected to Whitelist</div>
                    <button
                      onClick={handleAddToWhitelist}
                      className="w-full px-4 py-2 bg-gray-100 hover:bg-gray-200 text-gray-800 rounded-lg transition-colors font-medium"
                    >
                      Add to Whitelist
                    </button>
                    <div className="text-xs text-gray-400 italic">
                      Whitelisted entries will never be blocked
                    </div>
                  </div>
                  
                  <div className="text-sm text-gray-400 text-center pt-2 border-t border-gray-700 mt-2">
                    Hint: Press DEL key to delete selected entries
                  </div>
                </>
              ) : (
                <>
              <div>
                <label className="text-sm text-gray-400 font-medium">Status</label>
                <div className={`text-base font-bold ${getStatusColor(selectedEntry.status)}`}>
                  {getStatusDescription(selectedEntry.status)}
                </div>
              </div>

              <div>
                <label className="text-sm text-gray-400 font-medium">Quest</label>
                <div className="text-base text-purple-400">{selectedEntry.questName}</div>
                {selectedEntry.questEditorID && (
                  <div className="text-sm text-gray-500">{selectedEntry.questEditorID}</div>
                )}
              </div>

              <div>
                <label className="text-sm text-gray-400 font-medium">{selectedEntry.isScene ? 'Scene' : 'Topic'}</label>
                <div className="text-base text-yellow-400">
                  {selectedEntry.isScene ? selectedEntry.sceneEditorID : selectedEntry.topicEditorID}
                </div>
                {!selectedEntry.isScene && selectedEntry.topicFormID && (
                  <div className="text-sm text-gray-500">FormID: {selectedEntry.topicFormID}</div>
                )}
              </div>

              <div>
                <label className="text-sm text-gray-400 font-medium">Source Plugin</label>
                <div className="text-base text-gray-300">{selectedEntry.sourcePlugin}</div>
              </div>

              <div>
                <label className="text-sm text-gray-400 font-medium">Subtype</label>
                <div className="text-base text-blue-400">{selectedEntry.subtypeName}</div>
              </div>
              
              {/* Show All Responses button */}
              {selectedEntry.responseCount > 0 && (
                <div className="border-t border-gray-700 pt-4 mt-4">
                  <button
                    onClick={handleShowAllResponses}
                    className="w-full px-4 py-2 bg-cyan-600 hover:bg-cyan-700 text-white rounded-lg transition-colors"
                  >
                    Show All Responses ({selectedEntry.responseCount})
                  </button>
                </div>
              )}
              
              {/* Blocking Settings */}
              <div className="border-t border-gray-700 pt-3 mt-4">
                <h4 className="text-lg font-semibold mb-3 text-blue-400">Blocking Settings</h4>
                
                {/* Block Type Checkboxes (mutually exclusive with whitelist) */}
                <div className="mb-4">
                  <div className="text-base text-gray-400 font-medium mb-2">Action</div>
                  <div className="space-y-2">
                    <label className="flex items-start gap-3 cursor-pointer p-2 rounded hover:bg-gray-700 transition-colors">
                      <input
                        type="checkbox"
                        checked={editSoftBlock}
                        onChange={(e) => {
                          setEditSoftBlock(e.target.checked);
                          if (e.target.checked) {
                            setEditHardBlock(false);
                            setEditSkyrimNetBlock(false);
                            setEditWhitelist(false);
                          }
                        }}
                        className="w-5 h-5 mt-0.5 rounded border-gray-600 bg-gray-700 text-orange-600 focus:ring-orange-500 cursor-pointer"
                      />
                      <div className="flex-1">
                        <div className="text-base text-white font-medium">Soft Block</div>
                        <div className="text-sm text-gray-400">Blocks audio/subtitles and SkyrimNet logging</div>
                      </div>
                    </label>
                    
                    {selectedEntry.skyrimNetBlockable && (
                      <label className="flex items-start gap-3 cursor-pointer p-2 rounded hover:bg-gray-700 transition-colors">
                        <input
                          type="checkbox"
                          checked={editSkyrimNetBlock}
                          onChange={(e) => {
                            setEditSkyrimNetBlock(e.target.checked);
                            if (e.target.checked) {
                              setEditSoftBlock(false);
                              setEditHardBlock(false);
                              setEditWhitelist(false);
                            }
                          }}
                          className="w-5 h-5 mt-0.5 rounded border-gray-600 bg-gray-700 text-purple-600 focus:ring-purple-500 cursor-pointer"
                        />
                        <div className="flex-1">
                          <div className="text-base text-white font-medium">SkyrimNet Block</div>
                          <div className="text-sm text-gray-400">Block SkyrimNet logging only (audio/subtitles play normally)</div>
                        </div>
                      </label>
                    )}
                    
                    <label className="flex items-start gap-3 cursor-pointer p-2 rounded hover:bg-gray-700 transition-colors">
                      <input
                        type="checkbox"
                        checked={editHardBlock}
                        onChange={(e) => {
                          setEditHardBlock(e.target.checked);
                          if (e.target.checked) {
                            setEditSoftBlock(false);
                            setEditSkyrimNetBlock(false);
                            setEditWhitelist(false);
                          }
                        }}
                        className="w-5 h-5 mt-0.5 rounded border-gray-600 bg-gray-700 text-red-600 focus:ring-red-500 cursor-pointer"
                      />
                      <div className="flex-1">
                        <div className="text-base text-white font-medium">Hard Block</div>
                        <div className="text-sm text-gray-400">
                          {selectedEntry.isScene 
                            ? 'Prevent scene from starting entirely'
                            : 'Block dialogue completely'}
                        </div>
                      </div>
                    </label>
                    
                    <label className="flex items-start gap-3 cursor-pointer p-2 rounded hover:bg-gray-700 transition-colors">
                      <input
                        type="checkbox"
                        checked={editWhitelist}
                        onChange={(e) => {
                          setEditWhitelist(e.target.checked);
                          if (e.target.checked) {
                            setEditSoftBlock(false);
                            setEditHardBlock(false);
                            setEditSkyrimNetBlock(false);
                          }
                        }}
                        className="w-5 h-5 mt-0.5 rounded border-gray-600 bg-gray-700 text-green-600 focus:ring-green-500 cursor-pointer"
                      />
                      <div className="flex-1">
                        <div className="text-base text-white font-medium">Whitelist</div>
                        <div className="text-sm text-gray-400">Entry will never be blocked</div>
                      </div>
                    </label>
                  </div>
                </div>
                
                {/* Notes */}
                <div className="mb-4">
                  <div className="text-base text-gray-400 font-medium mb-2">Notes (Optional)</div>
                  <textarea
                    value={editNotes}
                    onChange={(e) => setEditNotes(e.target.value)}
                    className="w-full px-3 py-2 text-base bg-gray-700 text-white rounded border border-gray-600 focus:outline-none focus:border-blue-500 min-h-[80px] resize-y font-mono"
                    placeholder="Add notes about this entry..."
                  />
                </div>
              </div>
              
              {/* Action Buttons */}
              <div className="space-y-2 border-t border-gray-700 pt-4">
                {/* Edit Blacklist button - only show if blacklist entry exists */}
                {blacklistEntry && (
                  <button
                    onClick={() => {
                      setEditingBlacklistEntry(blacklistEntry);
                      setShowBlacklistEditModal(true);
                    }}
                    className="w-full px-4 py-2 bg-orange-600 hover:bg-orange-700 text-white rounded-lg transition-colors flex items-center justify-center gap-2 font-medium"
                  >
                    <Settings size={18} />
                    Edit Blacklist Entry
                  </button>
                )}
                
                {/* Edit Whitelist button - only show if whitelist entry exists */}
                {whitelistEntry && (
                  <button
                    onClick={() => {
                      setEditingWhitelistEntry(whitelistEntry);
                      setShowWhitelistEditModal(true);
                    }}
                    className="w-full px-4 py-2 bg-gray-100 hover:bg-gray-200 text-gray-800 rounded-lg transition-colors flex items-center justify-center gap-2 font-medium"
                  >
                    <Settings size={18} />
                    Edit Whitelist Entry
                  </button>
                )}
                
                {/* Advanced Entry button - only show if not both blacklist and whitelist entries exist */}
                {!(blacklistEntry && whitelistEntry) && (
                  <button
                    onClick={() => setShowAdvancedEntryModal(true)}
                    className="w-full px-4 py-2 bg-purple-600 hover:bg-purple-700 text-white rounded-lg transition-colors flex items-center justify-center gap-2 font-medium"
                  >
                    <Plus size={18} />
                    Advanced Entry
                  </button>
                )}

                <button
                  onClick={handleApplyChanges}
                  disabled={!editSoftBlock && !editHardBlock && !editSkyrimNetBlock && !editWhitelist}
                  className="w-full px-4 py-2 bg-blue-600 hover:bg-blue-700 disabled:bg-gray-600 disabled:cursor-not-allowed text-white rounded-lg transition-colors flex items-center justify-center gap-2 font-medium"
                >
                  <Save size={18} />
                  Apply Changes
                </button>
                
                {/* Remove from blacklist button - only show when already blacklisted */}
                {(selectedEntry.status === 'Soft Block' || selectedEntry.status === 'Hard Block' || selectedEntry.status === 'SkyrimNet Block') && (
                  <button
                    onClick={handleRemoveFromBlacklist}
                    className="w-full px-4 py-2 bg-red-600 hover:bg-red-700 text-white rounded-lg transition-colors flex items-center justify-center gap-2"
                  >
                    <Trash2 size={18} />
                    Remove from Blacklist
                  </button>
                )}
              </div>
              
              {/* Subtype Filter Toggle - only show for entries filtered by MCM or toggled off */}
              {(selectedEntry.status === 'Filter' || selectedEntry.status === 'Toggled Off') && (
                <div className="space-y-2 border-t border-gray-700 pt-4 mt-4">
                  <div className="text-base text-gray-400 font-medium mb-2">
                    Subtype Filter
                    <span className="text-xs ml-2">
                      ({selectedEntry.subtypeName})
                    </span>
                  </div>
                  <button
                    onClick={handleToggleSubtypeFilter}
                    className={`w-full px-4 py-2 text-white rounded-lg transition-colors ${
                      selectedEntry.status === 'Toggled Off' 
                        ? 'bg-cyan-600 hover:bg-cyan-700' 
                        : 'bg-yellow-600 hover:bg-yellow-700'
                    }`}
                  >
                    Toggle {selectedEntry.subtypeName} Filter
                  </button>
                  <div className="text-xs text-gray-500 italic">
                    Quick toggle for MCM subtype filter setting
                  </div>
                </div>
              )}
              </>
              )}
            </div>
          ) : (
            <div className="text-lg text-gray-500 text-center mt-8">Select an entry to view details</div>
          )}
        </div>
      </div>

      <div className="mt-4 text-base text-gray-500">
        Showing {displayedEntries.length} of {filteredEntries.length} entries ({entries.length} total)
      </div>
      
      {/* Responses Modal */}
      <ResponsesModal
        isOpen={showResponsesModal}
        onClose={() => setShowResponsesModal(false)}
        title={modalTitle}
        responses={modalResponses}
      />
      
      {/* Advanced Entry Modal */}
      <AdvancedEntryModal
        isOpen={showAdvancedEntryModal}
        onClose={() => setShowAdvancedEntryModal(false)}
        prefillEntry={selectedEntry}
      />
      
      {/* Advanced Edit Modal for Blacklist */}
      <AdvancedEditModal
        isOpen={showBlacklistEditModal}
        onClose={() => {
          setShowBlacklistEditModal(false);
          setEditingBlacklistEntry(null);
        }}
        entry={editingBlacklistEntry}
      />
      
      {/* Advanced Edit Modal for Whitelist */}
      <AdvancedEditModal
        isOpen={showWhitelistEditModal}
        onClose={() => {
          setShowWhitelistEditModal(false);
          setEditingWhitelistEntry(null);
        }}
        entry={editingWhitelistEntry}
      />
    </div>
  );
};
