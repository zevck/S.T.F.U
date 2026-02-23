import { useState, useMemo, memo, useCallback, useRef, useEffect } from 'react';
import { useBlacklistStore } from '../stores/blacklist';
import { BlacklistEntry } from '../types';
import { Search, Trash2, X, RotateCw, Save } from 'lucide-react';
import { SKSE_API, log } from '../lib/skse-api';

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
    className={`p-4 rounded-lg cursor-pointer transition-colors ${
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
        <div className="text-lg font-medium text-white truncate">{entry.topicEditorID || 'No Topic ID'}</div>
        {entry.questName && (
          <div className="text-base text-gray-400 truncate mt-1">{entry.questName}</div>
        )}
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
  const { entries, searchQuery, setSearchQuery } = useBlacklistStore();
  const [selectedEntries, setSelectedEntries] = useState<BlacklistEntry[]>([]);
  const [lastClickedIndex, setLastClickedIndex] = useState<number>(-1);
  const [displayCount, setDisplayCount] = useState(100); // Increased initial load
  const sentinelRef = useRef<HTMLDivElement>(null);
  
  // For single-item detail panel (first selected item)
  const selectedEntry = selectedEntries.length > 0 ? selectedEntries[0] : null;
  
  // Edit state for selected entry
  const [editSoftBlock, setEditSoftBlock] = useState<boolean>(false);
  const [editHardBlock, setEditHardBlock] = useState<boolean>(false);
  const [editSkyrimNetBlock, setEditSkyrimNetBlock] = useState<boolean>(false);
  const [editFilterCategory, setEditFilterCategory] = useState<string>('Blacklist');
  const [editNotes, setEditNotes] = useState<string>('');  // For single entry edit
  const [bulkFilterCategory, setBulkFilterCategory] = useState<string>('Blacklist'); // For bulk edit
  
  // Filter states
  const [typeFilter, setTypeFilter] = useState<string>('All');
  const [blockSoft, setBlockSoft] = useState(true);
  const [blockHard, setBlockHard] = useState(true);
  const [blockSkyrimNet, setBlockSkyrimNet] = useState(true);
  const [showTopics, setShowTopics] = useState(true);
  const [showScenes, setShowScenes] = useState(true);

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
          entry.topicEditorID?.toLowerCase().includes(query) ||
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
        (blockSkyrimNet && entry.blockType === 'SkyrimNet Block')
      );
      if (!blockTypeMatches) return false;
      
      // Target type filter (Topics/Scenes)
      if (!showTopics && entry.targetType === 'Topic') return false;
      if (!showScenes && entry.targetType === 'Scene') return false;
      
      return true;
    });
  }, [entries, searchQuery, typeFilter, blockSoft, blockHard, blockSkyrimNet, showTopics, showScenes]);

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
  }, [searchQuery, typeFilter, blockSoft, blockHard, blockSkyrimNet, showTopics, showScenes]);

  // Slice entries for lazy loading
  const displayedEntries = useMemo(() => {
    return filteredEntries.slice(0, displayCount);
  }, [filteredEntries, displayCount]);

  const handleDelete = useCallback((entry: BlacklistEntry) => {
    log(`[Blacklist] handleDelete called for entry ${entry.id}: ${entry.topicEditorID}`);
    SKSE_API.deleteBlacklistEntry(entry.id);
    // Remove from selection if it was selected
    setSelectedEntries(prev => prev.filter(e => e.id !== entry.id));
  }, []);
  
  const handleDeleteSelected = useCallback(() => {
    if (selectedEntries.length === 0) return;
    
    log(`[Blacklist] handleDeleteSelected called for ${selectedEntries.length} entries`);
    const ids = selectedEntries.map(e => e.id);
    
    if (selectedEntries.length === 1) {
      SKSE_API.deleteBlacklistEntry(ids[0]);
    } else {
      SKSE_API.deleteBlacklistBatch(ids);
    }
    
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

  const handleClear = useCallback(() => {
    log(`[Blacklist] handleClear called - clearing ${entries.length} entries`);
    SKSE_API.clearBlacklist();
  }, [entries.length]);
  
  const handleResetFilters = useCallback(() => {
    setSearchQuery('');
    setTypeFilter('All');
    setBlockSoft(true);
    setBlockHard(true);
    setBlockSkyrimNet(true);
    setShowTopics(true);
    setShowScenes(true);
  }, [setSearchQuery]);
  
  const handleRefresh = useCallback(() => {
    log('[Blacklist] handleRefresh called');
    SKSE_API.refreshBlacklist();
  }, []);
  
  const handleApplyChanges = useCallback(() => {
    if (!selectedEntry || selectedEntries.length !== 1) return;
    
    log(`[Blacklist] handleApplyChanges called for entry ${selectedEntry.id}`);
    
    // Determine block type from checkboxes
    let blockType = 'Soft'; // default
    if (editHardBlock) {
      blockType = 'Hard';
    } else if (editSkyrimNetBlock) {
      blockType = 'SkyrimNet';
    } else if (editSoftBlock) {
      blockType = 'Soft';
    }
    
    log(`[Blacklist] Applying changes: blockType=${blockType}, filterCategory=${editFilterCategory}`)
    SKSE_API.updateBlacklistEntry(selectedEntry.id, blockType, editFilterCategory, editNotes);
  }, [selectedEntry, selectedEntries.length, editSoftBlock, editHardBlock, editSkyrimNetBlock, editFilterCategory, editNotes]);
  
  // Update edit state when selected entry changes
  useEffect(() => {
    if (selectedEntry && selectedEntries.length === 1) {
      const blockType = selectedEntry.blockType || 'Soft Block';
      setEditSoftBlock(blockType === 'Soft Block');
      setEditHardBlock(blockType === 'Hard Block');
      setEditSkyrimNetBlock(blockType === 'SkyrimNet Block');
      setEditFilterCategory(selectedEntry.filterCategory || 'Blacklist');
      setEditNotes(selectedEntry.note || '');
    }
    
    // Initialize bulk filter category when multiple entries selected
    if (selectedEntries.length > 1) {
      // Set to first entry's category or default based on type
      const firstEntry = selectedEntries[0];
      if (firstEntry.filterCategory === 'SkyrimNet' && selectedEntries.every(e => e.filterCategory === 'SkyrimNet')) {
        setBulkFilterCategory('SkyrimNet');
      } else {
        setBulkFilterCategory(firstEntry.filterCategory || 'Blacklist');
      }
    }
  }, [selectedEntry, selectedEntries.length, selectedEntries]);
  
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
        handleDeleteSelected();
      }
    };
    
    window.addEventListener('keydown', handleKeyDown);
    return () => window.removeEventListener('keydown', handleKeyDown);
  }, [selectedEntries, handleDeleteSelected]);
  
  // Filter categories based on target type
  const filterCategories = useMemo(() => {
    const isScene = selectedEntry?.targetType === 'Scene';
    if (isScene) {
      return ['Blacklist', 'Scene', 'BardSongs', 'FollowerCommentary'];
    } else {
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
    
    // Check if all entries are SkyrimNet blockable
    const allSkyrimNet = selectedEntries.every(e => e.filterCategory === 'SkyrimNet');
    if (allSkyrimNet) {
      return ['SkyrimNet'];
    }
    
    // Mixed types - no bulk edit available
    return null;
  }, [selectedEntries]);

  return (
    <div className="flex flex-col h-full">
      {/* Filter Panel */}
      <div className="bg-gray-800 rounded-lg p-4 mb-4 space-y-3">
        {/* Row 1: Search and Type Filter */}
        <div className="flex gap-3">
          <div className="relative flex-1">
            <Search className="absolute left-3 top-1/2 transform -translate-y-1/2 text-gray-400" size={20} />
            <input
              type="text"
              placeholder="Search blacklist..."
              value={searchQuery}
              onChange={(e) => setSearchQuery(e.target.value)}
              className="w-full pl-10 pr-4 py-2.5 text-base bg-gray-700 text-white rounded-lg border border-gray-600 focus:outline-none focus:border-blue-500"
            />
          </div>
          <div className="flex items-center gap-2">
            <label className="text-base text-gray-300 font-medium">Type:</label>
            <select
              value={typeFilter}
              onChange={(e) => setTypeFilter(e.target.value)}
              className="px-3 py-2.5 text-base bg-gray-700 text-white rounded-lg border border-gray-600 focus:outline-none focus:border-blue-500"
            >
              <option value="All">All</option>
              <option value="Topic">Topic</option>
              <option value="Quest">Quest</option>
              <option value="Subtype">Subtype</option>
              <option value="Scene">Scene</option>
            </select>
          </div>
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
              checked={blockSkyrimNet}
              onChange={(e) => setBlockSkyrimNet(e.target.checked)}
              className="w-5 h-5 rounded border-gray-600 bg-gray-700 text-blue-600 focus:ring-blue-500 cursor-pointer"
            />
            <span className="text-base text-white">SkyrimNet Block</span>
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
        </div>
        
        {/* Row 3: Action Buttons */}
        <div className="flex gap-2">
          <button
            onClick={handleResetFilters}
            className="px-4 py-2 text-base bg-gray-700 hover:bg-gray-600 text-white rounded-lg transition-colors"
          >
            Reset Filters
          </button>
          <button
            onClick={handleRefresh}
            className="px-4 py-2 text-base bg-gray-700 hover:bg-gray-600 text-white rounded-lg flex items-center gap-2 transition-colors"
          >
            <RotateCw size={18} />
            Refresh
          </button>
          <button
            onClick={handleClear}
            className="ml-auto px-4 py-2 text-base bg-red-600 hover:bg-red-700 text-white rounded-lg flex items-center gap-2 transition-colors"
          >
            <Trash2 size={18} />
            Clear All
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
                        <li key={e.id} className="truncate">• {e.topicEditorID || 'No Topic ID'}</li>
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
                          {bulkFilterCategories.length === 1 
                            ? 'All selected entries are SkyrimNet compatible'
                            : selectedEntries[0]?.targetType === 'Scene'
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
                <div className="text-sm text-gray-400 font-medium">Topic Editor ID</div>
                <div className="text-base text-white font-mono break-all">{selectedEntry.topicEditorID || 'N/A'}</div>
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
              
              <div className="border-t border-gray-700 pt-3">
                <h4 className="text-lg font-semibold mb-3 text-blue-400">Blocking Settings</h4>
                
                {/* Block Type Checkboxes (mutually exclusive) */}
                <div className="mb-4">
                  <div className="text-base text-gray-400 font-medium mb-2">Block Type</div>
                  <div className="space-y-2">
                    <label className="flex items-start gap-3 cursor-pointer p-2 rounded hover:bg-gray-700 transition-colors">
                      <input
                        type="checkbox"
                        checked={editSoftBlock}
                        onChange={(e) => {
                          // Only allow unchecking if another box is checked
                          if (!e.target.checked && !editHardBlock && !editSkyrimNetBlock) {
                            return; // Prevent unchecking the last checkbox
                          }
                          setEditSoftBlock(e.target.checked);
                          if (e.target.checked) {
                            setEditHardBlock(false);
                            setEditSkyrimNetBlock(false);
                          }
                        }}
                        className="w-5 h-5 mt-0.5 rounded border-gray-600 bg-gray-700 text-orange-600 focus:ring-orange-500 cursor-pointer"
                      />
                      <div className="flex-1">
                        <div className="text-base text-white font-medium">Soft Block</div>
                        <div className="text-sm text-gray-400">Blocks audio/subtitles and SkyrimNet logging</div>
                      </div>
                    </label>
                    
                    {selectedEntry.filterCategory === 'SkyrimNet' && (
                      <label className="flex items-start gap-3 cursor-pointer p-2 rounded hover:bg-gray-700 transition-colors">
                        <input
                          type="checkbox"
                          checked={editSkyrimNetBlock}
                          onChange={(e) => {
                            // Only allow unchecking if another box is checked
                            if (!e.target.checked && !editSoftBlock && !editHardBlock) {
                              return; // Prevent unchecking the last checkbox
                            }
                            setEditSkyrimNetBlock(e.target.checked);
                            if (e.target.checked) {
                              setEditSoftBlock(false);
                              setEditHardBlock(false);
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
                          // Only allow unchecking if another box is checked
                          if (!e.target.checked && !editSoftBlock && !editSkyrimNetBlock) {
                            return; // Prevent unchecking the last checkbox
                          }
                          setEditHardBlock(e.target.checked);
                          if (e.target.checked) {
                            setEditSoftBlock(false);
                            setEditSkyrimNetBlock(false);
                          }
                        }}
                        className="w-5 h-5 mt-0.5 rounded border-gray-600 bg-gray-700 text-red-600 focus:ring-red-500 cursor-pointer"
                      />
                      <div className="flex-1">
                        <div className="text-base text-white font-medium">Hard Block</div>
                        <div className="text-sm text-gray-400">
                          {selectedEntry.targetType === 'Scene' 
                            ? 'Prevent scene from starting entirely'
                            : 'Block dialogue completely'}
                        </div>
                      </div>
                    </label>
                  </div>
                </div>
                
                {/* Filter Category */}
                <div className="mb-4">
                  <div className="text-base text-gray-400 font-medium mb-2">Filter Category</div>
                  <select
                    value={editFilterCategory}
                    onChange={(e) => setEditFilterCategory(e.target.value)}
                    className="w-full px-3 py-2 text-base bg-gray-700 text-white rounded border border-gray-600 focus:outline-none focus:border-blue-500"
                  >
                    {filterCategories.map(cat => (
                      <option key={cat} value={cat}>{cat}</option>
                    ))}
                  </select>
                  <div className="text-sm text-gray-400 mt-1">Which setting toggles blocking for this entry</div>
                </div>
                
                {/* Notes */}
                <div className="mb-4">
                  <div className="text-base text-gray-400 font-medium mb-2">Notes</div>
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
                <button
                  onClick={handleApplyChanges}
                  className="w-full px-4 py-2 bg-blue-600 hover:bg-blue-700 text-white rounded-lg transition-colors flex items-center justify-center gap-2 font-medium"
                >
                  <Save size={18} />
                  Apply Changes
                </button>
                
                <button
                  onClick={handleDeleteSelected}
                  className="w-full px-4 py-2 bg-red-600 hover:bg-red-700 text-white rounded-lg transition-colors flex items-center justify-center gap-2"
                >
                  <Trash2 size={18} />
                  Remove from Blacklist
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
    </div>
  );
};
