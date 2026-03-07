import { useState, useMemo, memo, useCallback, useRef, useEffect } from 'react';
import { useBlacklistStore } from '../stores/blacklist';
import { BlacklistEntry } from '../types';
import { Search, Trash2, X, Save, Plus, Settings } from 'lucide-react';
import { SKSE_API, log } from '../lib/skse-api';
import { ResponsesModal } from './responses-modal';
import { ManualEntryModal } from './manual-entry-modal';
import { AdvancedEditModal } from './advanced-edit-modal';

// Memoized list item component for better performance
const BlacklistItem = memo(({ 
  entry, 
  isSelected, 
  onClick, 
  onDelete,
  index
}: { 
  entry: BlacklistEntry; 
  isSelected: boolean; 
  onClick: (event: React.MouseEvent) => void; 
  onDelete: () => void;
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
          {entry.blockType && (
            <span className="px-3 py-1 text-base font-semibold rounded bg-red-600/30 text-red-300">
              {entry.blockType}
            </span>
          )}
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
          onDelete();
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

BlacklistItem.displayName = 'BlacklistItem';

export const Blacklist = () => {
  const { 
    entries,
    setEntries,
    searchQuery, 
    setSearchQuery,
    typeFilter,
    setTypeFilter,
    blockSoft,
    setBlockSoft,
    blockHard,
    setBlockHard,
    showTopics,
    setShowTopics,
    showScenes,
    setShowScenes,
    selectedEntries,
    setSelectedEntries
  } = useBlacklistStore();
  const [lastClickedIndex, setLastClickedIndex] = useState<number>(-1);
  const [displayCount, setDisplayCount] = useState(100); // Increased initial load
  const sentinelRef = useRef<HTMLDivElement>(null);
  const [showResponsesModal, setShowResponsesModal] = useState(false);
  const [showManualEntryModal, setShowManualEntryModal] = useState(false);
  const [showAdvancedEditModal, setShowAdvancedEditModal] = useState(false);
  const [editingEntry, setEditingEntry] = useState<BlacklistEntry | null>(null);
  const [modalTitle, setModalTitle] = useState('');
  const [modalResponses, setModalResponses] = useState<string[]>([]);
  
  // For single-item detail panel (first selected item)
  const selectedEntry = selectedEntries.length > 0 ? selectedEntries[0] : null;
  
  const [bulkFilterCategory, setBulkFilterCategory] = useState<string>('Blacklist'); // For bulk edit

  // Memoized filtering logic - only recalculates when dependencies change
  const filteredEntries = useMemo(() => {
    return entries.filter((entry) => {
      // Search query filter
      if (searchQuery) {
        const query = searchQuery.toLowerCase();
        const matches = (
          entry.targetType?.toLowerCase().includes(query) ||
          entry.blockType?.toLowerCase().includes(query) ||
          entry.questName?.toLowerCase().includes(query) ||
          entry.questEditorID?.toLowerCase().includes(query) ||
          entry.topicEditorID?.toLowerCase().includes(query) ||
          entry.topicFormID?.toLowerCase().includes(query) ||
          entry.sourcePlugin?.toLowerCase().includes(query) ||
          entry.note?.toLowerCase().includes(query)
        );
        if (!matches) return false;
      }
      
      // Type filter
      if (typeFilter !== 'All') {
        if (entry.targetType !== typeFilter) return false;
      }
      
      // Block type filter
      const blockTypeMatches = (
        (blockSoft && entry.blockType === 'Soft Block') ||
        (blockHard && entry.blockType === 'Hard Block') ||
        entry.blockType === 'SkyrimNet Block'
      );
      if (!blockTypeMatches) return false;
      
      // Target type filter (Topics/Scenes)
      if (!showTopics && entry.targetType === 'Topic') return false;
      if (!showScenes && entry.targetType === 'Scene') return false;
      
      return true;
    });
  }, [entries, searchQuery, typeFilter, blockSoft, blockHard, showTopics, showScenes]);

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

  // Reset display count when filters change
  useEffect(() => {
    setDisplayCount(100);
  }, [searchQuery, typeFilter, blockSoft, blockHard, showTopics, showScenes]);

  // Slice entries for lazy loading
  const displayedEntries = useMemo(() => {
    return filteredEntries.slice(0, displayCount);
  }, [filteredEntries, displayCount]);

  const handleDelete = useCallback((entry: BlacklistEntry) => {
    log(`[Blacklist] handleDelete called for entry ${entry.id}: ${entry.topicEditorID}`);
    SKSE_API.deleteBlacklistEntry(entry.id);
    setSelectedEntries(selectedEntries.filter(e => e.id !== entry.id));
    setTimeout(() => SKSE_API.requestHistoryRefresh(), 150);
  }, [selectedEntries, setSelectedEntries]);
  
  const handleDeleteSelected = useCallback(() => {
    if (selectedEntries.length === 0) return;
    
    log(`[Blacklist] handleDeleteSelected called for ${selectedEntries.length} entries`);
    const ids = selectedEntries.map(e => e.id);
    
    if (selectedEntries.length === 1) {
      SKSE_API.deleteBlacklistEntry(ids[0]);
    } else {
      SKSE_API.deleteBlacklistBatch(ids);
    }
    setTimeout(() => SKSE_API.requestHistoryRefresh(), 150);
    setSelectedEntries([]);
    setLastClickedIndex(-1);
  }, [selectedEntries]);
  
  const handleBulkSetBlockType = useCallback((blockType: string) => {
    if (selectedEntries.length === 0) return;
    
    log(`[Blacklist] handleBulkSetBlockType called for ${selectedEntries.length} entries, blockType=${blockType}`);
    
    selectedEntries.forEach(entry => {
      const filterCategory = entry.filterCategory || 'Blacklist';
      const notes = entry.note || '';
      SKSE_API.updateBlacklistEntry(entry.id, blockType, filterCategory, notes);
    });
    
    // Refresh after bulk update
    setTimeout(() => {
      SKSE_API.refreshBlacklist();
    }, 100);
  }, [selectedEntries]);
  
  const handleBulkSetFilterCategory = useCallback((filterCategory: string) => {
    if (selectedEntries.length === 0) return;
    
    log(`[Blacklist] handleBulkSetFilterCategory called for ${selectedEntries.length} entries, filterCategory=${filterCategory}`);
    
    selectedEntries.forEach(entry => {
      const blockType = entry.blockType === 'Soft Block' ? 'Soft' 
                      : entry.blockType === 'Hard Block' ? 'Hard'
                      : entry.blockType === 'SkyrimNet Block' ? 'SkyrimNet'
                      : 'Soft';
      const notes = entry.note || '';
      SKSE_API.updateBlacklistEntry(entry.id, blockType, filterCategory, notes);
    });
    
    // Refresh after bulk update
    setTimeout(() => {
      SKSE_API.refreshBlacklist();
    }, 100);
  }, [selectedEntries]);
  
  const handleResetFilters = useCallback(() => {
    setSearchQuery('');
    setTypeFilter('All');
    setBlockSoft(true);
    setBlockHard(true);
    setShowTopics(true);
    setShowScenes(true);
  }, [setSearchQuery]);
  
  
  // Sync bulk filter category when multiple entries selected
  useEffect(() => {
    if (selectedEntries.length > 1) {
      setBulkFilterCategory(selectedEntries[0].filterCategory || 'Blacklist');
    }
  }, [selectedEntries]);

  // Keep selectedEntries pointing to fresh objects after store updates (e.g. after saves)
  useEffect(() => {
    if (selectedEntries.length === 0) return;
    const idSet = new Set(selectedEntries.map(e => e.id));
    const fresh = entries.filter(e => idSet.has(e.id));
    setSelectedEntries(fresh);
  }, [entries]);
  
  // Handle multi-selection click
  const handleItemClick = useCallback((entry: BlacklistEntry, index: number, event?: React.MouseEvent) => {
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
  }, [lastClickedIndex, filteredEntries, selectedEntries, setSelectedEntries]);
  
  // Keyboard support for DEL key
  useEffect(() => {
    const handleKeyDown = (e: KeyboardEvent) => {
      if (e.key === 'Delete' && selectedEntries.length > 0) {
        e.preventDefault();
        handleDeleteSelected();
      }
    };
    
    window.addEventListener('keydown', handleKeyDown);
    return () => window.removeEventListener('keydown', handleKeyDown);
  }, [selectedEntries, handleDeleteSelected]);
  
  // Handle showing all responses
  const handleShowAllResponses = useCallback(() => {
    if (!selectedEntry) return;
    
    // Parse responseText JSON if available
    let responses: string[] = [];
    if (selectedEntry.responseText) {
      try {
        responses = JSON.parse(selectedEntry.responseText);
      } catch (e) {
        // If parsing fails, treat as single response
        responses = [selectedEntry.responseText];
      }
    }
    
    const identifier = selectedEntry.topicEditorID || selectedEntry.topicFormID || 'Unknown';
    setModalTitle(`All Responses: ${identifier}`);
    setModalResponses(responses);
    setShowResponsesModal(true);
  }, [selectedEntry]);
  
  // Bulk filter categories - only available if all selected entries are compatible
  const bulkFilterCategories = useMemo(() => {
    if (selectedEntries.length <= 1) return null;
    
    // Check if all entries are scenes
    const allScenes = selectedEntries.every(e => e.targetType === 'Scene');
    if (allScenes) {
      return ['Blacklist', 'Scene', 'BardSongs', 'FollowerCommentary'];
    }
    
    // Check if all entries are topics (not scenes)
    const allTopics = selectedEntries.every(e => e.targetType === 'Topic');
    if (allTopics) {
      return [
        'Blacklist',
        'AcceptYield', 'ActorCollideWithActor', 'Agree', 'AlertIdle', 'AlertToCombat', 'AlertToNormal',
        'AllyKilled', 'Assault', 'AssaultNC', 'Attack', 'AvoidThreat', 'BarterExit', 'Bash',
        'Bleedout', 'Block', 'CombatToLost', 'CombatToNormal', 'Death', 'DestroyObject',
        'DetectFriendDie', 'ExitFavorState', 'Flee', 'Goodbye', 'Hello', 'Hit', 'Idle',
        'KnockOverObject', 'LockedObject', 'LostIdle', 'LostToCombat', 'LostToNormal',
        'MoralRefusal', 'Murder', 'MurderNC', 'NormalToAlert', 'NormalToCombat', 'NoticeCorpse',
        'ObserveCombat', 'PickpocketCombat', 'PickpocketNC', 'PickpocketTopic',
        'PlayerCastProjectileSpell', 'PlayerCastSelfSpell', 'PlayerInIronSights', 'PlayerShout',
        'PowerAttack', 'PursueIdleTopic', 'Refuse', 'ShootBow', 'Show', 'StandOnFurniture', 'Steal',
        'StealFromNC', 'SwingMeleeWeapon', 'Taunt', 'TimeToGo', 'TrainingExit', 'Trespass',
        'TrespassAgainstNC', 'VoicePowerEndLong', 'VoicePowerEndShort', 'VoicePowerStartLong',
        'VoicePowerStartShort', 'WerewolfTransformCrime', 'Yield', 'ZKeyObject'
      ];
    }
    
    // Mixed types - no bulk edit available
    return null;
  }, [selectedEntries]);

  return (
    <div className="flex flex-col h-full">
      {/* Filter Panel */}
      <div className="bg-gray-800 rounded-lg p-4 mb-4 space-y-3">
        {/* Row 1: Search Bar */}
        <div className="relative">
          <Search className="absolute left-3 top-1/2 transform -translate-y-1/2 text-gray-400" size={20} />
          <input
            type="text"
            placeholder="Search blacklist..."
            value={searchQuery}
            onChange={(e) => setSearchQuery(e.target.value)}
            className="w-full pl-10 pr-4 py-2.5 text-base bg-gray-700 text-white rounded-lg border border-gray-600 focus:outline-none focus:border-blue-500"
          />
        </div>
        
        {/* Row 2: Block Type Checkboxes */}
        <div className="flex items-center gap-4">
          <span className="text-base text-gray-300 font-medium">Block Type:</span>
          <label className="flex items-center gap-2 cursor-pointer">
            <input
              type="checkbox"
              checked={blockSoft}
              onChange={(e) => setBlockSoft(e.target.checked)}
              className="w-5 h-5 rounded border-gray-600 bg-gray-700 text-blue-600 focus:ring-blue-500 cursor-pointer"
            />
            <span className="text-base text-white">Soft Block</span>
          </label>
          <label className="flex items-center gap-2 cursor-pointer">
            <input
              type="checkbox"
              checked={blockHard}
              onChange={(e) => setBlockHard(e.target.checked)}
              className="w-5 h-5 rounded border-gray-600 bg-gray-700 text-blue-600 focus:ring-blue-500 cursor-pointer"
            />
            <span className="text-base text-white">Hard Block</span>
          </label>

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
              {searchQuery || typeFilter !== 'All' ? 'No blacklist entries match your filters' : 'No blacklist entries'}
            </div>
          ) : (
            <>
              {displayedEntries.map((entry, index) => (
                <BlacklistItem
                  key={entry.id}
                  entry={entry}
                  isSelected={selectedEntries.some(e => e.id === entry.id)}
                  onClick={(event) => handleItemClick(entry, index, event)}
                  onDelete={() => handleDelete(entry)}
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
            <h3 className="text-xl font-semibold mb-2 text-red-400">
              {selectedEntries.length > 1 
                ? `${selectedEntries.length} Entries Selected` 
                : 'Manage Blacklist Entry'}
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
                  
                  {/* Bulk edit buttons */}
                  <div className="space-y-2 border-t border-gray-700 pt-4">
                    <div className="text-base text-gray-400 font-medium mb-2">Bulk Edit All Selected</div>
                    <button
                      onClick={() => handleBulkSetBlockType('Soft')}
                      className="w-full px-4 py-2 bg-blue-600 hover:bg-blue-700 text-white rounded-lg transition-colors flex items-center justify-center gap-2"
                    >
                      <Save size={18} />
                      Set All to Soft Block
                    </button>
                    <button
                      onClick={() => handleBulkSetBlockType('Hard')}
                      className="w-full px-4 py-2 bg-purple-600 hover:bg-purple-700 text-white rounded-lg transition-colors flex items-center justify-center gap-2"
                    >
                      <Save size={18} />
                      Set All to Hard Block
                    </button>
                    
                    {/* Filter category dropdown - only if all entries are compatible */}
                    {bulkFilterCategories && (
                      <div className="mt-3">
                        <div className="text-sm text-gray-400 mb-2">Filter Category (applies to all)</div>
                        <div className="flex gap-2">
                          <select
                            value={bulkFilterCategory}
                            onChange={(e) => setBulkFilterCategory(e.target.value)}
                            className="flex-1 px-3 py-2 text-base bg-gray-700 text-white rounded border border-gray-600 focus:outline-none focus:border-blue-500"
                          >
                            {bulkFilterCategories.map(cat => (
                              <option key={cat} value={cat}>{cat}</option>
                            ))}
                          </select>
                          <button
                            onClick={() => handleBulkSetFilterCategory(bulkFilterCategory)}
                            className="px-4 py-2 bg-green-600 hover:bg-green-700 text-white rounded-lg transition-colors"
                          >
                            <Save size={18} />
                          </button>
                        </div>
                        <div className="text-xs text-gray-500 mt-1">
                          {selectedEntries[0]?.targetType === 'Scene'
                            ? 'All selected entries are Scenes'
                            : 'All selected entries are Topics'}
                        </div>
                      </div>
                    )}
                  </div>
                  
                  {/* Multi-delete button */}
                  <button
                    onClick={handleDeleteSelected}
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
                      onClick={handleShowAllResponses}
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

      <div className="mt-4 text-base text-gray-500">
        Showing {displayedEntries.length} of {filteredEntries.length} filtered entries ({entries.length} total)
      </div>
      
      {/* Responses Modal */}
      <ResponsesModal
        isOpen={showResponsesModal}
        onClose={() => setShowResponsesModal(false)}
        title={modalTitle}
        responses={modalResponses}
      />
      
      {/* Manual Entry Modal */}
      <ManualEntryModal
        isOpen={showManualEntryModal}
        onClose={() => setShowManualEntryModal(false)}
      />
      
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
