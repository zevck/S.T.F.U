import { memo, useState, useEffect, useMemo } from 'react';
import { Trash2 } from 'lucide-react';
import { SKSE_API, log } from '../lib/skse-api';
import { BlacklistEntry } from '../types';
import { Modal } from './modal';
import { ActorFilterPicker, ActorFilters } from './actor-filter-picker';

interface AdvancedEditModalProps {
  isOpen: boolean;
  onClose: () => void;
  onSave?: (updatedEntry: BlacklistEntry) => void;
  entry: BlacklistEntry | null;
}

const TOPIC_CATEGORIES = [
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

export const AdvancedEditModal = memo(({ isOpen, onClose, onSave, entry }: AdvancedEditModalProps) => {
  const [blockType, setBlockType] = useState<'Soft' | 'Hard'>('Soft');
  const [filterCategory, setFilterCategory] = useState('Blacklist');
  const [notes, setNotes] = useState('');
  const [filters, setFilters] = useState<ActorFilters>({ actorFilterNames: [], actorFilterFormIDs: [], factionFilterEditorIDs: [] });

  // Get filter categories based on entry type
  const filterCategories = useMemo(() => {
    if (!entry) return ['Blacklist'];

    // Whitelist entries always use Whitelist category
    if (entry.filterCategory === 'Whitelist') {
      return ['Whitelist'];
    }

    // SkyrimNet entries
    if (entry.filterCategory === 'SkyrimNet') {
      return ['SkyrimNet'];
    }

    // Scene entries can use Scene-related categories
    if (entry.targetType === 'Scene') {
      return ['Blacklist', 'Scene', 'BardSongs', 'FollowerCommentary'];
    }

    // Regular topic entries - should include all subtype categories
    return TOPIC_CATEGORIES;
  }, [entry]);

  // Load entry data when modal opens
  useEffect(() => {
    if (entry && isOpen) {
      // Parse block type
      if (entry.blockType === 'Soft Block') {
        setBlockType('Soft');
      } else if (entry.blockType === 'Hard Block') {
        setBlockType('Hard');
      }

      setFilterCategory(entry.filterCategory || 'Blacklist');
      setNotes(entry.note || '');
      setFilters({
        actorFilterNames: entry.actorFilterNames || [],
        actorFilterFormIDs: entry.actorFilterFormIDs || [],
        factionFilterEditorIDs: entry.factionFilterEditorIDs || [],
      });
    }
  }, [entry, isOpen]);

  if (!isOpen || !entry) return null;

  const isWhitelist = entry.filterCategory === 'Whitelist';
  const targetIsActorOrFaction = entry.targetType === 'Actor' || entry.targetType === 'Faction';

  const handleSave = () => {
    const { actorFilterNames, actorFilterFormIDs, factionFilterEditorIDs } = filters;

    // Ensure actor arrays stay in sync — truncate to shorter length if desynced
    const syncedLength = Math.min(actorFilterNames.length, actorFilterFormIDs.length);
    if (actorFilterNames.length !== actorFilterFormIDs.length) {
      log(`[AdvancedEdit] WARNING: Array desync (names=${actorFilterNames.length}, formIDs=${actorFilterFormIDs.length}), truncating to ${syncedLength}`);
    }
    const safeActorNames = actorFilterNames.slice(0, syncedLength);
    const safeActorFormIDs = actorFilterFormIDs.slice(0, syncedLength);

    log(`[AdvancedEdit] Updating entry ${entry.id} (${isWhitelist ? 'whitelist' : 'blacklist'}): actors=${safeActorNames.length}, factions=${factionFilterEditorIDs.length}`);

    if (isWhitelist) {
      SKSE_API.updateWhitelistEntryAdvanced({
        id: entry.id,
        notes,
        actorFilterNames: safeActorNames,
        actorFilterFormIDs: safeActorFormIDs,
        factionFilterEditorIDs
      });
    } else {
      SKSE_API.sendToSKSE('updateBlacklistEntryAdvanced', JSON.stringify({
        id: entry.id,
        blockType,
        filterCategory,
        notes,
        actorFilterNames: safeActorNames,
        actorFilterFormIDs: safeActorFormIDs,
        factionFilterEditorIDs
      }));
    }

    // Notify parent to update its store immediately (same React batch as onClose)
    if (onSave) {
      const blockTypeDisplay = blockType === 'Hard' ? 'Hard Block' : 'Soft Block';
      onSave({
        ...entry,
        blockType: isWhitelist ? entry.blockType : blockTypeDisplay,
        filterCategory,
        note: notes,
        actorFilterNames: safeActorNames,
        actorFilterFormIDs: safeActorFormIDs,
        factionFilterEditorIDs,
      });
    }

    onClose();
    // Also request C++ to push fresh confirmed data
    setTimeout(() => {
      if (isWhitelist) {
        SKSE_API.requestWhitelistRefresh();
      } else {
        SKSE_API.refreshBlacklist();
      }
    }, 150);
  };

  const handleDelete = () => {
    log(`[AdvancedEdit] Deleting entry ${entry.id} (${isWhitelist ? 'whitelist' : 'blacklist'})`);
    if (isWhitelist) {
      SKSE_API.removeFromWhitelist(entry.id);
    } else {
      SKSE_API.deleteBlacklistEntry(entry.id);
    }
    setTimeout(() => SKSE_API.requestHistoryRefresh(), 150);
    onClose();
  };

  const identifier = entry.targetType === 'Scene'
    ? entry.topicEditorID || 'Unknown Scene'
    : entry.topicEditorID || entry.topicFormID || 'Unknown';

  return (
    <Modal
      isOpen={isOpen}
      onClose={onClose}
      title={isWhitelist ? 'Edit Whitelist Entry' : 'Edit Blacklist Entry'}
      subtitle={identifier}
      sizeClassName="max-w-3xl max-h-[90vh]"
    >
      {/* Content - Scrollable */}
      <div className="p-6 space-y-4 overflow-y-auto">
        {/* Entry Info (Read-only) */}
        <div className="bg-gray-700 rounded-lg p-4 space-y-2">
          <div className="grid grid-cols-2 gap-2 text-sm">
            <div>
              <span className="text-gray-400">Type:</span>
              <span className="text-white ml-2">{entry.targetType}</span>
            </div>
            <div>
              <span className="text-gray-400">FormID:</span>
              <span className="text-white ml-2">{entry.topicFormID || 'N/A'}</span>
            </div>
            {entry.questEditorID && (
              <div className="col-span-2">
                <span className="text-gray-400">Quest:</span>
                <span className="text-white ml-2">{entry.questEditorID}</span>
              </div>
            )}
            {entry.sourcePlugin && (
              <div className="col-span-2">
                <span className="text-gray-400">Plugin:</span>
                <span className="text-white ml-2">{entry.sourcePlugin}</span>
              </div>
            )}
          </div>
        </div>

        {/* Block Type and Filter Category — hidden for whitelist entries and Actor/Faction targets (soft-only) */}
        {!isWhitelist && !targetIsActorOrFaction && (
          <>
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
              </div>
            </div>

            <div>
              <label className="block text-base font-medium text-gray-300 mb-2">
                Filter Category
              </label>
              <select
                value={filterCategory}
                onChange={(e) => setFilterCategory(e.target.value)}
                className="w-full px-4 py-2.5 text-base bg-gray-700 text-white rounded-lg border border-gray-600 focus:outline-none focus:border-blue-500"
              >
                {filterCategories.map((cat) => (
                  <option key={cat} value={cat}>
                    {cat}
                  </option>
                ))}
              </select>
            </div>
          </>
        )}

        {/* Actor & Faction Filters — hidden for Actor/Faction targets (they ARE the target) */}
        {!targetIsActorOrFaction && (
          <ActorFilterPicker
            filters={filters}
            onChange={setFilters}
            label="Actor & Faction Filters"
            logTag="AdvancedEdit"
          />
        )}

        {/* Notes */}
        <div>
          <label className="block text-base font-medium text-gray-300 mb-2">
            Notes
          </label>
          <textarea
            value={notes}
            onChange={(e) => setNotes(e.target.value)}
            placeholder="Add notes about this entry..."
            rows={4}
            className="w-full px-4 py-2.5 text-base bg-gray-700 text-white rounded-lg border border-gray-600 focus:outline-none focus:border-blue-500 resize-none"
          />
        </div>
      </div>

      {/* Footer */}
      <div className="p-4 border-t border-gray-700 flex items-center justify-between gap-3">
        <button
          onClick={handleDelete}
          className="px-4 py-2.5 text-base bg-red-700 hover:bg-red-800 text-white rounded-lg transition-colors flex items-center gap-2"
        >
          <Trash2 size={16} />
          Delete Entry
        </button>
        <div className="flex gap-3">
          <button
            onClick={onClose}
            className="px-6 py-2.5 text-base bg-gray-700 hover:bg-gray-600 text-white rounded-lg transition-colors"
          >
            Cancel
          </button>
          <button
            onClick={handleSave}
            className="px-6 py-2.5 text-base bg-blue-600 hover:bg-blue-700 text-white rounded-lg transition-colors"
          >
            Save Changes
          </button>
        </div>
      </div>
    </Modal>
  );
});

AdvancedEditModal.displayName = 'AdvancedEditModal';
