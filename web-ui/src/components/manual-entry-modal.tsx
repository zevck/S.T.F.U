import { memo, useState, useEffect, useRef } from 'react';
import { SKSE_API, log } from '../lib/skse-api';
import { Modal } from './modal';
import { ActorFilterPicker, ActorFilters } from './actor-filter-picker';

interface ManualEntryModalProps {
  isOpen: boolean;
  onClose: () => void;
  isWhitelist?: boolean;
  prefillIdentifier?: string;
  prefillSpeakerFormID?: string;
  prefillSpeakerName?: string;
}

type DetectedType = 'topic' | 'scene' | 'plugin' | 'actor' | 'faction' | 'unknown';

interface DetectionResult {
  type: DetectedType;
  categories: string[];
  displayName?: string;
}

const NO_FILTERS: ActorFilters = { actorFilterNames: [], actorFilterFormIDs: [], factionFilterEditorIDs: [] };

export const ManualEntryModal = memo(({ isOpen, onClose, isWhitelist = false, prefillIdentifier, prefillSpeakerFormID, prefillSpeakerName }: ManualEntryModalProps) => {
  const [identifier, setIdentifier] = useState('');
  const [filterSpeaker, setFilterSpeaker] = useState(false);
  const [blockType, setBlockType] = useState<'Soft' | 'Hard'>('Soft');
  const [notes, setNotes] = useState('');
  const [detectedType, setDetectedType] = useState<DetectedType | null>(null);
  const [detectedDisplayName, setDetectedDisplayName] = useState<string>('');
  const [categories, setCategories] = useState<string[]>(['Blacklist']);
  const [selectedCategory, setSelectedCategory] = useState('Blacklist');
  const [filters, setFilters] = useState<ActorFilters>(NO_FILTERS);
  const detectionTimeoutRef = useRef<number | null>(null);
  const detectionFallbackRef = useRef<number | null>(null);

  // Receive identifier detection results while open
  // (only registered while open to avoid conflicts when multiple instances are mounted)
  useEffect(() => {
    if (!isOpen) return;

    (window as any).handleIdentifierDetection = (result: DetectionResult) => {
      log(`[ManualEntry] Detection result: type=${result.type}, categories=${result.categories.length}`);
      setDetectedType(result.type);
      setDetectedDisplayName(result.displayName || '');
      setCategories(result.categories);
      // Actor/faction only support soft blocking
      if (result.type === 'actor' || result.type === 'faction') {
        setBlockType('Soft');
      }

      // Reset to "Blacklist" if current selection is not in new categories
      if (!result.categories.includes(selectedCategory)) {
        setSelectedCategory('Blacklist');
      }
    };

    return () => {
      delete (window as any).handleIdentifierDetection;
    };
  }, [isOpen, selectedCategory]);

  // Detect identifier type when it changes (debounced)
  useEffect(() => {
    if (detectionTimeoutRef.current) {
      clearTimeout(detectionTimeoutRef.current);
    }

    if (identifier.trim()) {
      setDetectedType(null);
      detectionTimeoutRef.current = window.setTimeout(() => {
        log(`[ManualEntry] Detecting type for: ${identifier}`);
        SKSE_API.sendToSKSE('detectIdentifierType', JSON.stringify({ identifier }));
      }, 300); // 300ms debounce
      // Fallback: if no response within 2s, show unknown
      if (detectionFallbackRef.current) clearTimeout(detectionFallbackRef.current);
      detectionFallbackRef.current = window.setTimeout(() => {
        setDetectedType(prev => prev === null ? 'unknown' : prev);
      }, 2000);
    } else {
      // Reset to default when empty
      setDetectedType(null);
      setDetectedDisplayName('');
      setCategories(['Blacklist']);
      setSelectedCategory('Blacklist');
    }

    return () => {
      if (detectionTimeoutRef.current) {
        clearTimeout(detectionTimeoutRef.current);
      }
      if (detectionFallbackRef.current) {
        clearTimeout(detectionFallbackRef.current);
      }
    };
  }, [identifier]);

  // Apply prefill identifier when the modal opens
  useEffect(() => {
    if (isOpen) {
      log('[ManualEntry] Modal opened');
      setFilterSpeaker(false);
      if (prefillIdentifier) {
        log(`[ManualEntry] Prefilling identifier: ${prefillIdentifier}`);
        setIdentifier(prefillIdentifier);
      }
    }
  }, [isOpen, prefillIdentifier]);

  // When filterSpeaker checkbox changes, swap identifier between topic and speaker FormID
  useEffect(() => {
    if (!isOpen) return;
    if (filterSpeaker && prefillSpeakerFormID) {
      setIdentifier(prefillSpeakerFormID);
    } else if (!filterSpeaker && prefillIdentifier) {
      setIdentifier(prefillIdentifier);
    }
  }, [filterSpeaker]);

  // Reset form when modal closes
  useEffect(() => {
    if (!isOpen) {
      setIdentifier('');
      setNotes('');
      setBlockType('Soft');
      setFilterSpeaker(false);
      setDetectedType(null);
      setDetectedDisplayName('');
      setCategories(['Blacklist']);
      setSelectedCategory('Blacklist');
      setFilters(NO_FILTERS);
    }
  }, [isOpen]);

  const handleCreate = () => {
    if (!identifier.trim()) {
      log('[ManualEntry] Empty identifier, aborting');
      return;
    }

    const { actorFilterNames, actorFilterFormIDs, factionFilterEditorIDs } = filters;

    // Validate array synchronization
    if (actorFilterNames.length !== actorFilterFormIDs.length) {
      log(`[ManualEntry] ERROR: Array desync! Names: ${actorFilterNames.length}, FormIDs: ${actorFilterFormIDs.length}`);
      return;
    }

    const category = isWhitelist ? 'Whitelist' : selectedCategory;
    log(`[ManualEntry] Creating entry: identifier=${identifier}, blockType=${blockType}, category=${category}, isWhitelist=${isWhitelist}, actors=${actorFilterNames.length}`);

    // createAdvancedEntry supports actor/faction filters
    SKSE_API.sendToSKSE('createAdvancedEntry', JSON.stringify({
      identifier,
      blockType,
      category,
      notes,
      isWhitelist,
      actorFilterNames,
      actorFilterFormIDs,
      factionFilterEditorIDs
    }));

    onClose();
  };

  const targetIsActorOrFaction = detectedType === 'actor' || detectedType === 'faction';

  return (
    <Modal
      isOpen={isOpen}
      onClose={onClose}
      title={isWhitelist ? 'Create Whitelist Entry' : 'Create Blacklist Entry'}
      sizeClassName="max-w-2xl"
    >
      {/* Content */}
      <div className="p-6 space-y-4">
        {/* Identifier Input */}
        <div>
          <label className="block text-base font-medium text-gray-300 mb-2">
            Identifier
          </label>
          <input
            type="text"
            value={identifier}
            onChange={(e) => setIdentifier(e.target.value)}
            placeholder={isWhitelist ? "EditorID, FormID, or Plugin.esp" : "EditorID or FormID (e.g., DragonBridgeFarmScene02 or 02707A)"}
            className="w-full px-4 py-2.5 text-base bg-gray-700 text-white rounded-lg border border-gray-600 focus:outline-none focus:border-blue-500"
            autoFocus
          />
          <div className="text-sm text-gray-400 mt-1 min-h-[20px]">
            {(detectedType === null || detectedType === 'unknown') && !identifier.trim() && (isWhitelist ? 'Supports: Topic, Scene, Quest, Actor, Faction, Plugin' : 'Supports: Topic, Scene, Quest, Actor, Faction')}
            {detectedType === null && identifier.trim() && 'Detecting...'}
            {detectedType === 'unknown' && identifier.trim() && 'Could not detect type'}
            {detectedType === 'scene' && '🎬 Detected as Scene'}
            {detectedType === 'topic' && '💬 Detected as Topic'}
            {detectedType === 'plugin' && '📦 Detected as Plugin'}
            {detectedType === 'actor' && `🧑 Detected as Actor${detectedDisplayName ? `: ${detectedDisplayName}` : ''}`}
            {detectedType === 'faction' && `⚔️ Detected as Faction${detectedDisplayName ? `: ${detectedDisplayName}` : ''}`}
          </div>
          {prefillSpeakerFormID && prefillSpeakerFormID !== '0x00000000' && (
            <label className="flex items-center gap-2 mt-2 cursor-pointer select-none">
              <input
                type="checkbox"
                checked={filterSpeaker}
                onChange={(e) => setFilterSpeaker(e.target.checked)}
                className="w-4 h-4 rounded border-gray-500 text-purple-500 focus:ring-purple-500 cursor-pointer"
              />
              <span className="text-sm text-purple-300">
                {isWhitelist ? 'Whitelist speaker' : 'Block speaker'}{prefillSpeakerName ? ` (${prefillSpeakerName})` : ''} instead of this topic
              </span>
            </label>
          )}
        </div>

        {/* Block Type - only for blacklist */}
        {!isWhitelist && (
        <div>
          <label className="block text-base font-medium text-gray-300 mb-2">
            Block Type
          </label>
          <div className="flex gap-4">
            <label className="flex items-center gap-2 cursor-pointer">
              <input
                type="radio"
                name="blockType"
                checked={blockType === 'Soft'}
                onChange={() => setBlockType('Soft')}
                className="w-5 h-5 text-blue-600 focus:ring-blue-500 cursor-pointer"
              />
              <span className="text-base text-white">Soft Block</span>
            </label>
            {!targetIsActorOrFaction && (
            <label className="flex items-center gap-2 cursor-pointer">
              <input
                type="radio"
                name="blockType"
                checked={blockType === 'Hard'}
                onChange={() => setBlockType('Hard')}
                className="w-5 h-5 text-blue-600 focus:ring-blue-500 cursor-pointer"
              />
              <span className="text-base text-white">Hard Block</span>
            </label>
            )}
          </div>
          <div className="text-sm text-gray-400 mt-1">
            {targetIsActorOrFaction
              ? 'Actor and faction blocks are always soft (mute audio and hide subtitles).'
              : 'Soft blocks mute audio and hide subtitles. Hard blocks prevent dialogue before it plays.'}
          </div>
        </div>
        )}

        {/* Filter Category - only for blacklist */}
        {!isWhitelist && (
        <div>
          <label className="block text-base font-medium text-gray-300 mb-2">
            Filter Category
          </label>
          <select
            value={selectedCategory}
            onChange={(e) => setSelectedCategory(e.target.value)}
            className="w-full px-4 py-2.5 text-base bg-gray-700 text-white rounded-lg border border-gray-600 focus:outline-none focus:border-blue-500"
          >
            {categories.map((category) => (
              <option key={category} value={category}>
                {category}
              </option>
            ))}
          </select>
          <div className="text-sm text-gray-400 mt-1">
            Categories change based on detected type
          </div>
        </div>
        )}

        {/* Actor & Faction Filtering — hidden when the target itself is an actor/faction */}
        {!targetIsActorOrFaction && (
          <ActorFilterPicker
            filters={filters}
            onChange={setFilters}
            label="Actor & Faction Filters (Optional)"
            description={isWhitelist
              ? 'Leave empty to whitelist for all actors. Add specific actors/factions to only allow their dialogue.'
              : 'Leave empty to affect all actors. Add specific actors/factions to only block their dialogue.'}
            logTag="ManualEntry"
          />
        )}

        {/* Notes */}
        <div>
          <label className="block text-base font-medium text-gray-300 mb-2">
            Notes (Optional)
          </label>
          <textarea
            value={notes}
            onChange={(e) => setNotes(e.target.value)}
            placeholder="Add notes about why this entry was blocked..."
            rows={3}
            className="w-full px-4 py-2.5 text-base bg-gray-700 text-white rounded-lg border border-gray-600 focus:outline-none focus:border-blue-500 resize-none"
          />
        </div>
      </div>

      {/* Footer */}
      <div className="p-4 border-t border-gray-700 flex justify-end gap-3">
        <button
          onClick={onClose}
          className="px-6 py-2.5 text-base bg-gray-700 hover:bg-gray-600 text-white rounded-lg transition-colors"
        >
          Cancel
        </button>
        <button
          onClick={handleCreate}
          disabled={!identifier.trim()}
          className="px-6 py-2.5 text-base bg-green-600 hover:bg-green-700 disabled:bg-gray-600 disabled:cursor-not-allowed text-white rounded-lg transition-colors"
        >
          Create Entry
        </button>
      </div>
    </Modal>
  );
});

ManualEntryModal.displayName = 'ManualEntryModal';
