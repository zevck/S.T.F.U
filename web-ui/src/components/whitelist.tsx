import { useState, useMemo, memo, useCallback, useRef, useEffect } from 'react';
import { useWhitelistStore } from '../stores/whitelist';
import { BlacklistEntry } from '../types';
import { Search, X, Plus, Trash2, Settings } from 'lucide-react';
import { SKSE_API, log } from '../lib/skse-api';
import { ResponsesModal } from './responses-modal';
import { ManualEntryModal } from './manual-entry-modal';
import { AdvancedEditModal } from './advanced-edit-modal';

// Memoized list item component for better performance
const WhitelistItem = memo(({ 
  entry, 
  isSelected, 
  onClick, 
  onRemove,
  index
}: { 
  entry: BlacklistEntry; 
  isSelected: boolean; 
  onClick: (event: React.MouseEvent) => void; 
  onRemove: () => void;
  index: number;
}) => (
  <div
    onClick={onClick}
    className={`p-4 rounded-lg cursor-pointer ${
      isSelected
        ? 'bg-blue-900/50 border-l-4 border-blue-500'
        : index % 2 === 0
        ? 'bg-gray-800 hover:bg-blue-900/20 border-l-4 border-transparent'
        : 'bg-gray-850 hover:bg-blue-900/20 border-l-4 border-transparent'
    }`}
  >
    <div className="flex justify-between items-start">
      <div className="flex-1 min-w-0">
        <div className="flex gap-2 mb-2">
          {entry.targetType && (
            <span className="px-3 py-1 text-base font-semibold rounded bg-blue-600/30 text-blue-300">
              {entry.targetType}
            </span>
          )}
          <span className="px-3 py-1 text-base font-semibold rounded bg-green-600/30 text-green-300">
            Allowed
          </span>
        </div>
        <div className="text-lg font-medium text-white truncate">
          {entry.questEditorID && (
            <span className="text-gray-300">{entry.questEditorID}</span>
          )}
          {entry.questEditorID && entry.topicEditorID && (
            <span> - {entry.topicEditorID}</span>
          )}
          {entry.questEditorID && !entry.topicEditorID && entry.topicFormID && (
            <span className="text-gray-400"> - {entry.topicFormID}</span>
          )}
          {!entry.questEditorID && entry.questName && (
            <span className="text-gray-300">{entry.questName}</span>
          )}
          {!entry.questEditorID && entry.topicEditorID && (
            <span>{entry.questName ? ` - ${entry.topicEditorID}` : entry.topicEditorID}</span>
          )}
          {!entry.questEditorID && !entry.questName && !entry.topicEditorID && entry.topicFormID && (
            <span className="text-gray-400">{entry.topicFormID}</span>
          )}
        </div>
      </div>
      <button
        onClick={(e) => {
          e.stopPropagation();
          onRemove();
        }}
        className="ml-2 p-1 text-red-400 hover:text-red-300 hover:bg-red-900/30 rounded transition-colors"
      >
        <X size={18} />
      </button>
    </div>
    {entry.note && (
      <div className="text-base text-gray-400 mt-2 italic">Note: {entry.note}</div>
    )}
  </div>
));

WhitelistItem.displayName = 'WhitelistItem';

export const Whitelist = () => {
  const { 
    entries,
    setEntries,
    searchQuery, 
    setSearchQuery,
    showTopics,
    setShowTopics,
    showScenes,
    setShowScenes,
    showActors,
    setShowActors,
    showFactions,
    setShowFactions,
    selectedEntries,
    setSelectedEntries
  } = useWhitelistStore();
  const [lastClickedIndex, setLastClickedIndex] = useState<number>(-1);
  const [displayCount, setDisplayCount] = useState(100);
  const sentinelRef = useRef<HTMLDivElement>(null);
  const [showResponsesModal, setShowResponsesModal] = useState(false);
  const [showManualEntryModal, setShowManualEntryModal] = useState(false);
  const [showAdvancedEditModal, setShowAdvancedEditModal] = useState(false);
  const [editingEntry, setEditingEntry] = useState<BlacklistEntry | null>(null);
  const [modalTitle, setModalTitle] = useState('');
  const [modalResponses, setModalResponses] = useState<string[]>([]);
  
  // For single-item detail panel (first selected item)
  const selectedEntry = selectedEntries.length > 0 ? selectedEntries[0] : null;
  
  // Edit state for selected entry


  // Memoized filtering logic
  const filteredEntries = useMemo(() => {
    return entries.filter((entry) => {
      // Search query filter
      if (searchQuery) {
        const query = searchQuery.toLowerCase();
        const matches = (
          entry.targetType?.toLowerCase().includes(query) ||
          entry.questName?.toLowerCase().includes(query) ||
          entry.questEditorID?.toLowerCase().includes(query) ||
          entry.topicEditorID?.toLowerCase().includes(query) ||
          entry.topicFormID?.toLowerCase().includes(query) ||
          entry.sourcePlugin?.toLowerCase().includes(query) ||
          entry.note?.toLowerCase().includes(query) ||
          entry.actorFilterNames?.some(n => n.toLowerCase().includes(query)) ||
          entry.factionFilterEditorIDs?.some(f => f.toLowerCase().includes(query))
        );
        if (!matches) return false;
      }
      
      // Topic/Scene/Actor/Faction checkboxes
      if (!showTopics && entry.targetType === 'Topic') return false;
      if (!showScenes && entry.targetType === 'Scene') return false;
      if (!showActors && entry.targetType === 'Actor') return false;
      if (!showFactions && entry.targetType === 'Faction') return false;
      
      return true;
    });
  }, [entries, searchQuery, showTopics, showScenes, showActors, showFactions]);

  // Virtual scrolling: only render visible + buffer entries
  const displayedEntries = useMemo(() => {
    return filteredEntries.slice(0, displayCount);
  }, [filteredEntries, displayCount]);

  // Intersection observer to load more entries
  useEffect(() => {
    const sentinel = sentinelRef.current;
    if (!sentinel) return;

    const observer = new IntersectionObserver(
      (entries) => {
        if (entries[0].isIntersecting && displayCount < filteredEntries.length) {
          setDisplayCount((prev) => Math.min(prev + 100, filteredEntries.length));
        }
      },
      { threshold: 0.1 }
    );

    observer.observe(sentinel);
    return () => observer.disconnect();
  }, [displayCount, filteredEntries.length]);

  // Keep selectedEntries pointing to fresh objects after store updates (e.g. after saves)
  useEffect(() => {
    if (selectedEntries.length === 0) return;
    const idSet = new Set(selectedEntries.map(e => e.id));
    const fresh = entries.filter(e => idSet.has(e.id));
    setSelectedEntries(fresh);
  }, [entries]);

  const handleItemClick = useCallback((entry: BlacklistEntry, _index: number, event: React.MouseEvent) => {
    event.stopPropagation();
    const clickedIndex = filteredEntries.findIndex(e => e.id === entry.id);

    if (event.ctrlKey || event.metaKey) {
      // Ctrl+Click: toggle selection
      const isAlreadySelected = selectedEntries.some(e => e.id === entry.id);
      if (isAlreadySelected) {
        setSelectedEntries(selectedEntries.filter(e => e.id !== entry.id));
      } else {
        setSelectedEntries([...selectedEntries, entry]);
      }
      setLastClickedIndex(clickedIndex);
    } else if (event.shiftKey && lastClickedIndex !== -1) {
      // Shift+Click: range selection
      const start = Math.min(lastClickedIndex, clickedIndex);
      const end = Math.max(lastClickedIndex, clickedIndex);
      const rangeEntries = filteredEntries.slice(start, end + 1);
      setSelectedEntries(rangeEntries);
    } else {
      // Normal click: select only this entry
      setSelectedEntries([entry]);
      setLastClickedIndex(clickedIndex);
    }
  }, [filteredEntries, lastClickedIndex, selectedEntries, setSelectedEntries]);

  const handleRemove = useCallback((entry: BlacklistEntry) => {
    SKSE_API.removeFromWhitelist(entry.id);
    setSelectedEntries([]);
  }, []);

  const handleRemoveMultiple = useCallback(() => {
    const ids = selectedEntries.map(e => e.id);
    SKSE_API.removeWhitelistBatch(ids);
    setSelectedEntries([]);
  }, [selectedEntries]);

  // Keyboard support for DEL key
  useEffect(() => {
    const handleKeyDown = (e: KeyboardEvent) => {
      if (e.key === 'Delete' && selectedEntries.length > 0) {
        e.preventDefault();
        handleRemoveMultiple();
      }
    };
    
    window.addEventListener('keydown', handleKeyDown);
    return () => window.removeEventListener('keydown', handleKeyDown);
  }, [selectedEntries, handleRemoveMultiple]);

  const handleShowResponses = useCallback((entry: BlacklistEntry) => {
    if (!entry.responseText) {
      log('No responses available');
      return;
    }
    
    try {
      const responses = JSON.parse(entry.responseText);
      const responseTexts = Array.isArray(responses)
        ? responses.map((r: any) => r.response_text || r)
        : [entry.responseText];
      
      setModalResponses(responseTexts);
      setModalTitle(`Responses: ${entry.topicEditorID || entry.topicFormID || 'Unknown'}`);
      setShowResponsesModal(true);
    } catch (e) {
      setModalResponses([entry.responseText]);
      setModalTitle(`Response: ${entry.topicEditorID || entry.topicFormID || 'Unknown'}`);
      setShowResponsesModal(true);
    }
  }, []);

  const handleResetFilters = useCallback(() => {
    setSearchQuery('');
    setShowTopics(true);
    setShowScenes(true);
    setShowActors(true);
    setShowFactions(true);
  }, [setSearchQuery, setShowTopics, setShowScenes, setShowActors, setShowFactions]);

  return (
    <div className="flex flex-col h-full">
      {/* Filter Panel */}
      <div className="bg-gray-800 rounded-lg p-4 mb-4 space-y-3">
        {/* Search Bar */}
        <div className="relative">
          <Search className="absolute left-3 top-1/2 transform -translate-y-1/2 text-gray-400" size={20} />
          <input
            type="text"
            placeholder="Search whitelist..."
            value={searchQuery}
            onChange={(e) => setSearchQuery(e.target.value)}
            className="w-full pl-10 pr-4 py-2.5 text-base bg-gray-700 border border-gray-600 rounded-lg text-white placeholder-gray-400 focus:outline-none focus:border-blue-500"
          />
        </div>

        {/* Filter Controls */}
        <div className="flex flex-wrap gap-4 items-center">
          <span className="text-base text-gray-300 font-medium">Show:</span>
          <label className="flex items-center gap-2 cursor-pointer">
            <input
              type="checkbox"
              checked={showTopics}
              onChange={(e) => setShowTopics(e.target.checked)}
              className="w-5 h-5 rounded border-gray-600 bg-gray-700 text-blue-600 focus:ring-blue-500 cursor-pointer"
            />
            <span className="text-base text-white">Topics</span>
          </label>
          <label className="flex items-center gap-2 cursor-pointer">
            <input
              type="checkbox"
              checked={showScenes}
              onChange={(e) => setShowScenes(e.target.checked)}
              className="w-5 h-5 rounded border-gray-600 bg-gray-700 text-blue-600 focus:ring-blue-500 cursor-pointer"
            />
            <span className="text-base text-white">Scenes</span>
          </label>
          <label className="flex items-center gap-2 cursor-pointer">
            <input
              type="checkbox"
              checked={showActors}
              onChange={(e) => setShowActors(e.target.checked)}
              className="w-5 h-5 rounded border-gray-600 bg-gray-700 text-blue-600 focus:ring-blue-500 cursor-pointer"
            />
            <span className="text-base text-white">Actors</span>
          </label>
          <label className="flex items-center gap-2 cursor-pointer">
            <input
              type="checkbox"
              checked={showFactions}
              onChange={(e) => setShowFactions(e.target.checked)}
              className="w-5 h-5 rounded border-gray-600 bg-gray-700 text-blue-600 focus:ring-blue-500 cursor-pointer"
            />
            <span className="text-base text-white">Factions</span>
          </label>
          <button
            onClick={handleResetFilters}
            className="px-4 py-2 text-base bg-gray-700 hover:bg-gray-600 text-white rounded-lg transition-colors"
          >
            Reset Filters
          </button>
          <button
            onClick={() => setShowManualEntryModal(true)}
            className="px-4 py-2 text-base bg-green-600 hover:bg-green-700 text-white rounded-lg flex items-center gap-2 transition-colors ml-auto"
          >
            <Plus size={18} />
            Manual Entry
          </button>
        </div>
      </div>

      <div className="flex gap-4 flex-1 overflow-hidden">
        {/* List */}
        <div className="flex-1 overflow-y-auto space-y-2">
          {filteredEntries.length === 0 ? (
            <div className="text-center text-lg text-gray-400 mt-8">
              {searchQuery ? 'No whitelist entries match your filters' : 'No whitelist entries'}
            </div>
          ) : (
            <>
              {displayedEntries.map((entry, index) => (
                <WhitelistItem
                  key={entry.id}
                  entry={entry}
                  isSelected={selectedEntries.some(e => e.id === entry.id)}
                  onClick={(event) => handleItemClick(entry, index, event)}
                  onRemove={() => handleRemove(entry)}
                  index={index}
                />
              ))}
              {/* Sentinel element for lazy loading */}
              <div ref={sentinelRef} className="h-px" />
              {displayCount < filteredEntries.length && (
                <div className="p-4 text-center text-gray-400 bg-gray-800 rounded-lg">
                  Loaded {displayCount} of {filteredEntries.length}
                </div>
              )}
            </>
          )}
        </div>

        {/* Detail Panel */}
        {selectedEntry && (
          <div className="w-96 bg-gray-800 rounded-lg p-4 overflow-y-auto">
            <h3 className="text-xl font-semibold mb-2 text-green-400">
              {selectedEntries.length > 1 
                ? `${selectedEntries.length} Entries Selected` 
                : 'Manage Whitelist Entry'}
            </h3>
            <div className="space-y-2">
              {selectedEntries.length > 1 ? (
                <>
                  {/* Multi-selection info */}
                  <div className="text-base text-gray-300 bg-gray-700 rounded-lg p-4">
                    <p className="mb-2">Multiple entries selected</p>
                    <ul className="text-sm text-gray-400 space-y-1 max-h-40 overflow-y-auto">
                      {selectedEntries.map(e => (
                        <li key={e.id} className="truncate">
                          • {e.questName && <>{e.questName}</>}
                          {e.questName && !e.topicEditorID && e.topicFormID && <> - {e.topicFormID}</>}
                          {e.topicEditorID && <>{e.questName ? ` - ${e.topicEditorID}` : e.topicEditorID}</>}
                          {!e.questName && !e.topicEditorID && e.topicFormID && <>{e.topicFormID}</>}
                        </li>
                      ))}
                    </ul>
                  </div>
                  
                  {/* Multi-delete button */}
                  <button
                    onClick={handleRemoveMultiple}
                    className="w-full px-4 py-2 bg-red-600 hover:bg-red-700 text-white rounded-lg transition-colors flex items-center justify-center gap-2 font-medium"
                  >
                    <Trash2 size={18} />
                    Delete {selectedEntries.length} Entries
                  </button>
                  
                  <div className="text-sm text-gray-400 text-center pt-2 border-t border-gray-700">
                    Hint: Press DEL key to delete selected entries
                  </div>
                </>
              ) : (
                <>
              {/* Read-only info */}
              <div>
                <div className="text-sm text-gray-400 font-medium">Entry Type</div>
                <div className="text-base text-white font-semibold">{selectedEntry.targetType}</div>
              </div>
              
              <div>
                <div className="text-sm text-gray-400 font-medium">
                  {selectedEntry.targetType === 'Scene' ? 'Scene' : 'Topic'}
                </div>
                {selectedEntry.topicEditorID ? (
                  <div className="text-base text-yellow-400 font-mono break-all">{selectedEntry.topicEditorID}</div>
                ) : (
                  <div className="text-base text-gray-400 font-mono break-all">FormID: {selectedEntry.topicFormID || 'N/A'}</div>
                )}
              </div>
              
              {selectedEntry.questName && (
                <div>
                  <div className="text-sm text-gray-400 font-medium">Quest</div>
                  <div className="text-base text-white">{selectedEntry.questName}</div>
                </div>
              )}
              
              {selectedEntry.sourcePlugin && (
                <div>
                  <div className="text-sm text-gray-400 font-medium">Source Plugin</div>
                  <div className="text-base text-white">{selectedEntry.sourcePlugin}</div>
                </div>
              )}
              
              {/* Show All Responses button */}
              {selectedEntry.responseText && (() => {
                let responseCount = 0;
                try {
                  const responses = JSON.parse(selectedEntry.responseText);
                  responseCount = Array.isArray(responses) ? responses.length : 1;
                } catch {
                  responseCount = 1;
                }
                return responseCount > 0 ? (
                  <div className="border-t border-gray-700 pt-4 mt-4">
                    <button
                      onClick={() => handleShowResponses(selectedEntry)}
                      className="w-full px-4 py-2 bg-cyan-600 hover:bg-cyan-700 text-white rounded-lg transition-colors"
                    >
                      Show All Responses ({responseCount})
                    </button>
                  </div>
                ) : null;
              })()}
              
              {/* Action Buttons */}
              <div className="space-y-2 border-t border-gray-700 pt-4">
                <button
                  onClick={() => {
                    setEditingEntry(selectedEntry);
                    setShowAdvancedEditModal(true);
                  }}
                  className="w-full px-4 py-2 bg-purple-600 hover:bg-purple-700 text-white rounded-lg transition-colors flex items-center justify-center gap-2 font-medium"
                >
                  <Settings size={18} />
                  Edit
                </button>
              </div>
              </>
              )}
            </div>
          </div>
        )}
      </div>

      {/* Modals */}
      {showResponsesModal && (
        <ResponsesModal
          isOpen={showResponsesModal}
          title={modalTitle}
          responses={modalResponses}
          onClose={() => setShowResponsesModal(false)}
        />
      )}
      
      {showManualEntryModal && (
        <ManualEntryModal
          isOpen={showManualEntryModal}
          onClose={() => setShowManualEntryModal(false)}
          isWhitelist={true}
        />
      )}
      
      {/* Advanced Edit Modal */}
      <AdvancedEditModal
        isOpen={showAdvancedEditModal}
        onClose={() => {
          setShowAdvancedEditModal(false);
          setEditingEntry(null);
        }}
        onSave={(updatedEntry) => {
          setEntries(entries.map(e => e.id === updatedEntry.id ? updatedEntry : e));
          setSelectedEntries(selectedEntries.map(e => e.id === updatedEntry.id ? updatedEntry : e));
        }}
        entry={editingEntry}
      />
    </div>
  );
};
