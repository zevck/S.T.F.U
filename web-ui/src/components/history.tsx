import { useMemo, useState, memo, useRef, useEffect, useCallback } from 'react';
import { useHistoryStore } from '@/stores/history';
import { DialogueEntry } from '@/types';
import { SKSE_API } from '@/lib/skse-api';

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
    case 'Filter':
      return 'text-yellow-400';
    case 'Toggled Off':
      return 'text-cyan-400';
    case 'Whitelist':
      return 'text-purple-300';
    default:
      return 'text-white';
  }
};

const formatTimestamp = (timestamp: number): string => {
  const date = new Date(timestamp);
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
    className={`p-4 border-b border-gray-700 cursor-pointer transition-colors ${
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
        {entry.status}
      </span>
    </div>
    <div className="text-lg mb-1">
      <span className="text-green-300">{entry.speaker}</span>
      {entry.skyrimNetBlockable && <span className="ml-2 text-purple-400">[Menu]</span>}
    </div>
    <div className="text-lg text-gray-300 truncate">{entry.text}</div>
  </div>
));

HistoryItem.displayName = 'HistoryItem';

export const History = () => {
  const { entries, searchQuery, setSearchQuery } = useHistoryStore();
  const [selectedEntries, setSelectedEntries] = useState<DialogueEntry[]>([]);
  const [lastClickedIndex, setLastClickedIndex] = useState<number>(-1);
  const [displayCount, setDisplayCount] = useState(100); // Increased initial load
  const sentinelRef = useRef<HTMLDivElement>(null);
  
  // For single-item detail panel (first selected item)
  const selectedEntry = selectedEntries.length > 0 ? selectedEntries[0] : null;

  const filteredEntries = useMemo(() => {
    if (!searchQuery) return entries;

    const query = searchQuery.toLowerCase();
    return entries.filter(
      (entry) =>
        entry.text.toLowerCase().includes(query) ||
        entry.speaker.toLowerCase().includes(query) ||
        entry.questName.toLowerCase().includes(query) ||
        entry.topicEditorID.toLowerCase().includes(query)
    );
  }, [entries, searchQuery]);

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
  
  // Check if all selected entries are SkyrimNet blockable (menu topics)
  const allSelectedAreSkyrimNetBlockable = useMemo(() => {
    return selectedEntries.length > 0 && selectedEntries.every(e => e.skyrimNetBlockable);
  }, [selectedEntries]);
  
  const handleAddToBlacklist = useCallback((blockType: 'Soft' | 'Hard' | 'SkyrimNet') => {
    if (selectedEntries.length === 0) return;
    
    // Call SKSE API to add entries to blacklist
    SKSE_API.addToBlacklist(selectedEntries, blockType);
    
    // Clear selection after adding
    setSelectedEntries([]);
    setLastClickedIndex(-1);
  }, [selectedEntries]);
  
  const handleAddToWhitelist = useCallback(() => {
    if (selectedEntries.length === 0) return;
    
    // Call SKSE API to add entries to whitelist (placeholder)
    SKSE_API.addToWhitelist(selectedEntries);
    
    // Clear selection
    setSelectedEntries([]);
    setLastClickedIndex(-1);
  }, [selectedEntries]);
  
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
      setSelectedEntries(prev => {
        const isSelected = prev.some(e => e.id === entry.id);
        if (isSelected) {
          return prev.filter(e => e.id !== entry.id);
        } else {
          return [...prev, entry];
        }
      });
      setLastClickedIndex(index);
    } else {
      // Normal click: single selection
      setSelectedEntries([entry]);
      setLastClickedIndex(index);
    }
  }, [lastClickedIndex, filteredEntries]);
  
  // Keyboard support for DEL key
  useEffect(() => {
    const handleKeyDown = (e: KeyboardEvent) => {
      if (e.key === 'Delete' && selectedEntries.length > 0) {
        e.preventDefault();
        // Clear selection
        setSelectedEntries([]);
      }
    };
    
    window.addEventListener('keydown', handleKeyDown);
    return () => window.removeEventListener('keydown', handleKeyDown);
  }, [selectedEntries]);

  return (
    <div className="flex flex-col h-full">
      <div className="mb-4">
        <h1 className="text-2xl font-bold mb-2">Dialogue History</h1>
        <input
          type="text"
          placeholder="Search..."
          value={searchQuery}
          onChange={(e) => setSearchQuery(e.target.value)}
          className="w-full px-4 py-2.5 text-base bg-gray-800 text-white border border-gray-700 rounded focus:outline-none focus:border-blue-500"
        />
      </div>

      <div className="flex-1 flex gap-4 overflow-hidden">
        {/* Entry List */}
        <div className="flex-1 overflow-y-auto border border-gray-700 rounded bg-gray-800">
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
                      className="w-full px-4 py-2 bg-blue-600 hover:bg-blue-700 text-white rounded-lg transition-colors disabled:opacity-50 disabled:cursor-not-allowed"
                    >
                      Add as Soft Block
                    </button>
                    <button
                      onClick={() => handleAddToBlacklist('Hard')}
                      className="w-full px-4 py-2 bg-purple-600 hover:bg-purple-700 text-white rounded-lg transition-colors disabled:opacity-50 disabled:cursor-not-allowed"
                    >
                      Add as Hard Block
                    </button>
                    {allSelectedAreSkyrimNetBlockable && (
                      <button
                        onClick={() => handleAddToBlacklist('SkyrimNet')}
                        className="w-full px-4 py-2 bg-indigo-600 hover:bg-indigo-700 text-white rounded-lg transition-colors disabled:opacity-50 disabled:cursor-not-allowed"
                      >
                        Add as SkyrimNet Block
                      </button>
                    )}
                    {allSelectedAreSkyrimNetBlockable && (
                      <div className="text-xs text-green-400 italic">
                        ✓ All selected are SkyrimNet compatible
                      </div>
                    )}
                  </div>
                  
                  {/* Whitelist controls (placeholder) */}
                  <div className="space-y-2 border-t border-gray-700 pt-4 mt-4">
                    <div className="text-base text-gray-400 font-medium mb-2">Add Selected to Whitelist</div>
                    <button
                      onClick={handleAddToWhitelist}
                      className="w-full px-4 py-2 bg-green-600 hover:bg-green-700 text-white rounded-lg transition-colors opacity-50 cursor-not-allowed"
                      disabled
                    >
                      Add to Whitelist
                    </button>
                    <div className="text-xs text-gray-500 italic">
                      Whitelist functionality coming soon
                    </div>
                  </div>
                  
                  <div className="text-sm text-gray-400 text-center pt-2 border-t border-gray-700 mt-2">
                    Hint: Press DEL key to clear selection
                  </div>
                </>
              ) : (
                <>
              <div>
                <label className="text-sm text-gray-400 font-medium">Status</label>
                <div className={`text-base font-bold ${getStatusColor(selectedEntry.status)}`}>
                  {selectedEntry.status}
                </div>
              </div>

              <div>
                <label className="text-sm text-gray-400 font-medium">Speaker</label>
                <div className="text-base text-green-300">{selectedEntry.speaker}</div>
              </div>

              <div>
                <label className="text-sm text-gray-400 font-medium">Text</label>
                <div className="text-base text-white break-words">{selectedEntry.text}</div>
              </div>

              <div>
                <label className="text-sm text-gray-400 font-medium">Quest</label>
                <div className="text-base text-purple-400">{selectedEntry.questName}</div>
                <div className="text-xs text-gray-500">{selectedEntry.questEditorID}</div>
              </div>

              <div>
                <label className="text-sm text-gray-400 font-medium">Topic</label>
                <div className="text-base text-yellow-400">{selectedEntry.topicEditorID}</div>
                <div className="text-xs text-gray-500">FormID: {selectedEntry.topicFormID}</div>
              </div>

              <div>
                <label className="text-sm text-gray-400 font-medium">Source Plugin</label>
                <div className="text-base text-gray-300">{selectedEntry.sourcePlugin}</div>
              </div>

              <div>
                <label className="text-sm text-gray-400 font-medium">Subtype</label>
                <div className="text-base text-blue-400">{selectedEntry.subtypeName}</div>
              </div>

              <div>
                <label className="text-sm text-gray-400 font-medium">Response Count</label>
                <div className="text-base text-white">{selectedEntry.responseCount}</div>
              </div>

              <div>
                <label className="text-sm text-gray-400 font-medium">Timestamp</label>
                <div className="text-base text-gray-300">{formatTimestamp(selectedEntry.timestamp)}</div>
              </div>

              {selectedEntry.skyrimNetBlockable && (
                <div className="mt-2 p-2 bg-purple-900 bg-opacity-30 border border-purple-700 rounded">
                  <div className="text-sm text-purple-300">
                    ✓ SkyrimNet compatible (Menu dialogue)
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
        Showing {displayedEntries.length} of {filteredEntries.length} filtered entries ({entries.length} total)
      </div>
    </div>
  );
};
