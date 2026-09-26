import { useState, useMemo, memo, useCallback, useEffect } from 'react';
import { Search, Trash2, X, Plus, Settings } from 'lucide-react';
import { StoreApi, UseBoundStore } from 'zustand';
import { BlacklistEntry } from '../types';
import { SKSE_API, log } from '../lib/skse-api';
import { useLazyList, useMultiSelect } from '../lib/list-hooks';
import { EntryListState } from '../stores/entry-list';
import { useBlacklistStore } from '../stores/blacklist';
import { useWhitelistStore } from '../stores/whitelist';
import { ResponsesModal } from './responses-modal';
import { ManualEntryModal } from './manual-entry-modal';
import { AdvancedEditModal } from './advanced-edit-modal';

// What differs between the Blacklist and Whitelist tabs
interface ListConfig {
  name: string;               // "blacklist" / "whitelist", used in UI text
  isWhitelist: boolean;
  headingClass: string;       // detail-panel heading color
  useStore: UseBoundStore<StoreApi<EntryListState>>;
  deleteEntries: (ids: number[]) => void;
}

const BLACKLIST: ListConfig = {
  name: 'blacklist',
  isWhitelist: false,
  headingClass: 'text-red-400',
  useStore: useBlacklistStore,
  deleteEntries: (ids) => ids.length === 1 ? SKSE_API.deleteBlacklistEntry(ids[0]) : SKSE_API.deleteBlacklistBatch(ids),
};

const WHITELIST: ListConfig = {
  name: 'whitelist',
  isWhitelist: true,
  headingClass: 'text-green-400',
  useStore: useWhitelistStore,
  deleteEntries: (ids) => ids.length === 1 ? SKSE_API.removeFromWhitelist(ids[0]) : SKSE_API.removeWhitelistBatch(ids),
};

// "Quest - Topic" label used in the list and the multi-selection summary
const EntryLabel = ({ entry }: { entry: BlacklistEntry }) => (
  <>
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
  </>
);

// Memoized list item component for better performance
const EntryItem = memo(({
  entry,
  isSelected,
  isWhitelist,
  onClick,
  onDelete,
  index
}: {
  entry: BlacklistEntry;
  isSelected: boolean;
  isWhitelist: boolean;
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
          {isWhitelist ? (
            <span className="px-3 py-1 text-base font-semibold rounded bg-green-600/30 text-green-300">
              Allowed
            </span>
          ) : entry.blockType && (
            <span className="px-3 py-1 text-base font-semibold rounded bg-red-600/30 text-red-300">
              {entry.blockType}
            </span>
          )}
        </div>
        <div className="text-lg font-medium text-white truncate">
          <EntryLabel entry={entry} />
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

EntryItem.displayName = 'EntryItem';

const FilterCheckbox = ({ label, checked, onChange }: { label: string; checked: boolean; onChange: (checked: boolean) => void }) => (
  <label className="flex items-center gap-2 cursor-pointer">
    <input
      type="checkbox"
      checked={checked}
      onChange={(e) => onChange(e.target.checked)}
      className="w-5 h-5 rounded border-gray-600 bg-gray-700 text-blue-600 focus:ring-blue-500 cursor-pointer"
    />
    <span className="text-base text-white">{label}</span>
  </label>
);

// Parses an entry's responseText (a JSON array, or a plain string) into lines
const parseResponses = (responseText: string): string[] => {
  try {
    const parsed = JSON.parse(responseText);
    return Array.isArray(parsed) ? parsed.map((r: any) => r.response_text || r) : [responseText];
  } catch {
    return [responseText];
  }
};

const EntryList = ({ config }: { config: ListConfig }) => {
  const {
    entries, setEntries,
    searchQuery, setSearchQuery,
    blockSoft, setBlockSoft,
    blockHard, setBlockHard,
    showTopics, setShowTopics,
    showScenes, setShowScenes,
    showActors, setShowActors,
    showFactions, setShowFactions,
    selectedEntries, setSelectedEntries,
    resetFilters,
  } = config.useStore();
  const { isWhitelist } = config;
  const [showResponsesModal, setShowResponsesModal] = useState(false);
  const [showManualEntryModal, setShowManualEntryModal] = useState(false);
  const [editingEntry, setEditingEntry] = useState<BlacklistEntry | null>(null);

  // For single-item detail panel (first selected item)
  const selectedEntry = selectedEntries.length > 0 ? selectedEntries[0] : null;

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
          entry.note?.toLowerCase().includes(query) ||
          entry.actorFilterNames?.some(n => n.toLowerCase().includes(query)) ||
          entry.factionFilterEditorIDs?.some(f => f.toLowerCase().includes(query))
        );
        if (!matches) return false;
      }

      // Block type filter (blacklist only; legacy SkyrimNet entries always shown)
      if (!isWhitelist) {
        const blockTypeMatches = (
          (blockSoft && entry.blockType === 'Soft Block') ||
          (blockHard && entry.blockType === 'Hard Block') ||
          entry.blockType === 'SkyrimNet Block'
        );
        if (!blockTypeMatches) return false;
      }

      // Target type filter (Topics/Scenes/Actors/Factions)
      if (!showTopics && entry.targetType === 'Topic') return false;
      if (!showScenes && entry.targetType === 'Scene') return false;
      if (!showActors && entry.targetType === 'Actor') return false;
      if (!showFactions && entry.targetType === 'Faction') return false;

      return true;
    });
  }, [entries, searchQuery, isWhitelist, blockSoft, blockHard, showTopics, showScenes, showActors, showFactions]);

  const { displayed: displayedEntries, displayCount, sentinelRef } = useLazyList(
    filteredEntries,
    [searchQuery, blockSoft, blockHard, showTopics, showScenes, showActors, showFactions].join('|')
  );
  const { handleItemClick, clearAnchor } = useMultiSelect(filteredEntries, selectedEntries, setSelectedEntries);

  const deleteEntries = useCallback((toDelete: BlacklistEntry[]) => {
    if (toDelete.length === 0) return;
    log(`[EntryList] Deleting ${toDelete.length} ${config.name} entries: ${toDelete.map(e => e.id).join(', ')}`);
    config.deleteEntries(toDelete.map(e => e.id));
    const deletedIds = new Set(toDelete.map(e => e.id));
    setSelectedEntries(selectedEntries.filter(e => !deletedIds.has(e.id)));
    clearAnchor();
    setTimeout(() => SKSE_API.requestHistoryRefresh(), 150);
  }, [config, selectedEntries, setSelectedEntries, clearAnchor]);

  // Keep selectedEntries pointing to fresh objects after store updates (e.g. after saves)
  useEffect(() => {
    if (selectedEntries.length === 0) return;
    const idSet = new Set(selectedEntries.map(e => e.id));
    setSelectedEntries(entries.filter(e => idSet.has(e.id)));
  }, [entries]);

  // Keyboard support for DEL key
  useEffect(() => {
    const handleKeyDown = (e: KeyboardEvent) => {
      if (e.key === 'Delete' && selectedEntries.length > 0) {
        e.preventDefault();
        deleteEntries(selectedEntries);
      }
    };

    window.addEventListener('keydown', handleKeyDown);
    return () => window.removeEventListener('keydown', handleKeyDown);
  }, [selectedEntries, deleteEntries]);

  const responses = selectedEntry?.responseText ? parseResponses(selectedEntry.responseText) : [];

  return (
    <div className="flex flex-col h-full">
      {/* Filter Panel */}
      <div className="bg-gray-800 rounded-lg p-4 mb-4 space-y-3">
        {/* Row 1: Search Bar */}
        <div className="relative">
          <Search className="absolute left-3 top-1/2 transform -translate-y-1/2 text-gray-400" size={20} />
          <input
            type="text"
            placeholder={`Search ${config.name}...`}
            value={searchQuery}
            onChange={(e) => setSearchQuery(e.target.value)}
            className="w-full pl-10 pr-4 py-2.5 text-base bg-gray-700 text-white rounded-lg border border-gray-600 placeholder-gray-400 focus:outline-none focus:border-blue-500"
          />
        </div>

        {/* Row 2: Filter checkboxes */}
        <div className="flex flex-wrap items-center gap-4">
          {isWhitelist ? (
            <span className="text-base text-gray-300 font-medium">Show:</span>
          ) : (
            <>
              <span className="text-base text-gray-300 font-medium">Block Type:</span>
              <FilterCheckbox label="Soft Block" checked={blockSoft} onChange={setBlockSoft} />
              <FilterCheckbox label="Hard Block" checked={blockHard} onChange={setBlockHard} />
            </>
          )}
          <FilterCheckbox label="Topics" checked={showTopics} onChange={setShowTopics} />
          <FilterCheckbox label="Scenes" checked={showScenes} onChange={setShowScenes} />
          <FilterCheckbox label="Actors" checked={showActors} onChange={setShowActors} />
          <FilterCheckbox label="Factions" checked={showFactions} onChange={setShowFactions} />
          <button
            onClick={resetFilters}
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
              {searchQuery ? `No ${config.name} entries match your filters` : `No ${config.name} entries`}
            </div>
          ) : (
            <>
              {displayedEntries.map((entry, index) => (
                <EntryItem
                  key={entry.id}
                  entry={entry}
                  isSelected={selectedEntries.some(e => e.id === entry.id)}
                  isWhitelist={isWhitelist}
                  onClick={(event) => handleItemClick(entry, index, event)}
                  onDelete={() => deleteEntries([entry])}
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
            <h3 className={`text-xl font-semibold mb-2 ${config.headingClass}`}>
              {selectedEntries.length > 1
                ? `${selectedEntries.length} Entries Selected`
                : `Manage ${isWhitelist ? 'Whitelist' : 'Blacklist'} Entry`}
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
                    onClick={() => deleteEntries(selectedEntries)}
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
              {responses.length > 0 && (
                <div className="border-t border-gray-700 pt-4 mt-4">
                  <button
                    onClick={() => setShowResponsesModal(true)}
                    className="w-full px-4 py-2 bg-cyan-600 hover:bg-cyan-700 text-white rounded-lg transition-colors"
                  >
                    Show All Responses ({responses.length})
                  </button>
                </div>
              )}

              {/* Action Buttons */}
              <div className="space-y-2 border-t border-gray-700 pt-4">
                <button
                  onClick={() => setEditingEntry(selectedEntry)}
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
        isOpen={showResponsesModal && !!selectedEntry}
        onClose={() => setShowResponsesModal(false)}
        title={`All Responses: ${selectedEntry?.topicEditorID || selectedEntry?.topicFormID || 'Unknown'}`}
        responses={responses}
      />

      {/* Manual Entry Modal */}
      <ManualEntryModal
        isOpen={showManualEntryModal}
        onClose={() => setShowManualEntryModal(false)}
        isWhitelist={isWhitelist}
      />

      {/* Advanced Edit Modal */}
      <AdvancedEditModal
        isOpen={editingEntry !== null}
        onClose={() => setEditingEntry(null)}
        onSave={(updatedEntry) => {
          setEntries(entries.map(e => e.id === updatedEntry.id ? updatedEntry : e));
          setSelectedEntries(selectedEntries.map(e => e.id === updatedEntry.id ? updatedEntry : e));
        }}
        entry={editingEntry}
      />
    </div>
  );
};

export const Blacklist = () => <EntryList config={BLACKLIST} />;
export const Whitelist = () => <EntryList config={WHITELIST} />;
