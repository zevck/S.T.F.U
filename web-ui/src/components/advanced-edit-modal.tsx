import { memo, useState, useEffect, useRef, useMemo } from 'react';
import { SKSE_API, log } from '../lib/skse-api';
import { useHistoryStore } from '../stores/history';
import { BlacklistEntry } from '../types';

interface AdvancedEditModalProps {
  isOpen: boolean;
  onClose: () => void;
  entry: BlacklistEntry | null;
}

interface Actor {
  name: string;
  formID: string;
  lastSeen: number;
}

export const AdvancedEditModal = memo(({ isOpen, onClose, entry }: AdvancedEditModalProps) => {
  const [blockType, setBlockType] = useState<'Soft' | 'Hard' | 'SkyrimNet'>('Soft');
  const [filterCategory, setFilterCategory] = useState('Blacklist');
  const [notes, setNotes] = useState('');
  const [actorFilterNames, setActorFilterNames] = useState<string[]>([]);
  const [actorFilterFormIDs, setActorFilterFormIDs] = useState<string[]>([]);
  const [newActorName, setNewActorName] = useState('');
  const [showActorDropdown, setShowActorDropdown] = useState(false);
  const [nearbyActors, setNearbyActors] = useState<Actor[]>([]);
  const dropdownRef = useRef<HTMLDivElement>(null);
  
  const historyEntries = useHistoryStore(state => state.entries);
  
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
  }, [entry]);
  
  // Get unique actors from recent history (last 30 minutes)
  const recentActors = (): Actor[] => {
    const now = Date.now();
    const thirtyMinutesAgo = now - (30 * 60 * 1000);
    
    const actorMap = new Map<string, Actor>();
    
    historyEntries.forEach(entry => {
      if (entry.timestamp > thirtyMinutesAgo && entry.speaker) {
        const formID = entry.speakerFormID || '';
        const key = entry.speaker.toLowerCase();
        
        if (!actorMap.has(key)) {
          actorMap.set(key, {
            name: entry.speaker,
            formID: formID,
            lastSeen: entry.timestamp
          });
        } else {
          const existing = actorMap.get(key)!;
          if (entry.timestamp > existing.lastSeen) {
            actorMap.set(key, {
              name: entry.speaker,
              formID: formID,
              lastSeen: entry.timestamp
            });
          }
        }
      }
    });
    
    return Array.from(actorMap.values()).sort((a, b) => b.lastSeen - a.lastSeen);
  };
  
  const actors = recentActors();
  
  // Combine nearby and recent actors, remove duplicates by FormID
  const allActors = useMemo(() => {
    const actorMap = new Map<string, Actor>();
    
    // Add nearby actors first (higher priority)
    nearbyActors.forEach(actor => {
      if (actor.formID && actor.formID !== '0x0') {
        actorMap.set(actor.formID.toUpperCase(), actor);
      }
    });
    
    // Add recent actors (won't overwrite if FormID already exists)
    actors.forEach(actor => {
      if (actor.formID && actor.formID !== '0x0' && !actorMap.has(actor.formID.toUpperCase())) {
        actorMap.set(actor.formID.toUpperCase(), actor);
      }
    });
    
    return Array.from(actorMap.values());
  }, [nearbyActors, actors]);
  
  // Filter actors based on search input (search both name and FormID)
  const filteredActors = useMemo(() => {
    const search = newActorName.trim().toLowerCase();
    if (!search) return allActors.filter(actor => !actorFilterNames.includes(actor.name));
    
    return allActors.filter(actor => {
      const matchesName = actor.name.toLowerCase().includes(search);
      const matchesFormID = actor.formID.toLowerCase().replace('0x', '').includes(search.replace('0x', ''));
      const notSelected = !actorFilterNames.includes(actor.name);
      return (matchesName || matchesFormID) && notSelected;
    });
  }, [allActors, newActorName, actorFilterNames]);
  
  // Auto-load nearby actors when modal opens
  useEffect(() => {
    if (isOpen) {
      log('[AdvancedEdit] Modal opened, requesting nearby actors');
      SKSE_API.requestNearbyActors();
    }
  }, [isOpen]);
  
  // Set up handler for nearby actors
  useEffect(() => {
    if (!isOpen) return;
    
    (window as any).handleNearbyActors = (data: { actors: Actor[] }) => {
      log(`[AdvancedEdit] Received ${data.actors.length} nearby actors`);
      setNearbyActors(data.actors);
    };

    return () => {
      delete (window as any).handleNearbyActors;
    };
  }, [isOpen]);
  
  // Load entry data when modal opens
  useEffect(() => {
    if (entry && isOpen) {
      // Parse block type
      if (entry.blockType === 'Soft Block') {
        setBlockType('Soft');
      } else if (entry.blockType === 'Hard Block') {
        setBlockType('Hard');
      } else if (entry.blockType === 'SkyrimNet Block') {
        setBlockType('SkyrimNet');
      }
      
      setFilterCategory(entry.filterCategory || 'Blacklist');
      setNotes(entry.note || '');
      setActorFilterNames(entry.actorFilterNames || []);
      setActorFilterFormIDs(entry.actorFilterFormIDs || []);
    }
  }, [entry, isOpen]);

  // Reset form when modal closes
  useEffect(() => {
    if (!isOpen) {
      setNewActorName('');
      setShowActorDropdown(false);
    }
  }, [isOpen]);

  // Handle ESC key to close modal
  useEffect(() => {
    if (!isOpen) return;

    const handleEsc = (e: KeyboardEvent) => {
      if (e.keyCode === 27 || e.key === 'Escape') {
        e.stopImmediatePropagation();
        e.preventDefault();
        onClose();
      }
    };

    window.addEventListener('keydown', handleEsc, { capture: true });
    return () => window.removeEventListener('keydown', handleEsc, { capture: true });
  }, [isOpen, onClose]);
  
  // Close dropdown when clicking outside
  useEffect(() => {
    if (!showActorDropdown) return;
    
    const handleClickOutside = (e: MouseEvent) => {
      if (dropdownRef.current && !dropdownRef.current.contains(e.target as Node)) {
        setShowActorDropdown(false);
      }
    };
    
    document.addEventListener('mousedown', handleClickOutside);
    return () => document.removeEventListener('mousedown', handleClickOutside);
  }, [showActorDropdown]);

  if (!isOpen || !entry) return null;

  const handleBackdropClick = (e: React.MouseEvent) => {
    if (e.target === e.currentTarget) {
      onClose();
    }
  };
  
  const addActor = (actor: Actor) => {
    if (!actorFilterNames.includes(actor.name)) {
      setActorFilterNames([...actorFilterNames, actor.name]);
      // Always add FormID (empty string if not available) to maintain array sync
      setActorFilterFormIDs([...actorFilterFormIDs, actor.formID || '']);
    }
    setNewActorName('');
    setShowActorDropdown(false);
  };
  
  const addActorManually = () => {
    const name = newActorName.trim();
    if (name && !actorFilterNames.includes(name)) {
      setActorFilterNames([...actorFilterNames, name]);
      setActorFilterFormIDs([...actorFilterFormIDs, '']);
    }
    setNewActorName('');
    setShowActorDropdown(false);
  };
  
  const removeActor = (index: number) => {
    setActorFilterNames(actorFilterNames.filter((_, i) => i !== index));
    setActorFilterFormIDs(actorFilterFormIDs.filter((_, i) => i !== index));
  };

  const handleSave = () => {
    if (!entry) return;

    // Validate array synchronization
    if (actorFilterNames.length !== actorFilterFormIDs.length) {
      log(`[AdvancedEdit] ERROR: Array desync! Names: ${actorFilterNames.length}, FormIDs: ${actorFilterFormIDs.length}`);
      return;
    }

    log(`[AdvancedEdit] Updating entry ${entry.id}: blockType=${blockType}, filterCategory=${filterCategory}, actors=${actorFilterNames.length}`);

    // Update the entry with actor filters
    SKSE_API.sendToSKSE('updateBlacklistEntryAdvanced', JSON.stringify({
      id: entry.id,
      blockType,
      filterCategory,
      notes,
      actorFilterNames,
      actorFilterFormIDs
    }));

    onClose();
  };
  
  const getIdentifier = () => {
    if (entry.targetType === 'Scene') {
      return entry.topicEditorID || 'Unknown Scene';
    }
    return entry.topicEditorID || entry.topicFormID || 'Unknown';
  };

  return (
    <div
      className="fixed inset-0 z-50 flex items-center justify-center bg-black bg-opacity-75"
      onClick={handleBackdropClick}
      role="dialog"
      aria-modal="true"
      aria-labelledby="modal-title"
    >
      <div className="bg-gray-800 border border-gray-700 rounded-lg shadow-2xl w-full max-w-3xl flex flex-col max-h-[90vh]">
        {/* Header */}
        <div className="flex items-center justify-between p-4 border-b border-gray-700">
          <div>
            <h2 id="modal-title" className="text-xl font-bold text-white">
              Edit Blacklist Entry
            </h2>
            <p className="text-sm text-gray-400 mt-1">{getIdentifier()}</p>
          </div>
          <button
            onClick={onClose}
            className="text-gray-400 hover:text-white transition-colors"
            aria-label="Close modal"
          >
            <svg className="w-6 h-6" fill="none" stroke="currentColor" viewBox="0 0 24 24">
              <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M6 18L18 6M6 6l12 12" />
            </svg>
          </button>
        </div>

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

          {/* Block Type */}
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
              {entry.filterCategory === 'SkyrimNet' && (
                <label className="flex items-center gap-2 cursor-pointer">
                  <input
                    type="radio"
                    name="blockType"
                    checked={blockType === 'SkyrimNet'}
                    onChange={() => setBlockType('SkyrimNet')}
                    className="w-5 h-5 text-blue-600 focus:ring-blue-500 cursor-pointer"
                  />
                  <span className="text-base text-white">SkyrimNet Only</span>
                </label>
              )}
            </div>
          </div>

          {/* Filter Category */}
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

          {/* Actor & Faction Filters */}
          <div>
            <label className="block text-base font-medium text-gray-300 mb-2">
              Actor & Faction Filters
            </label>
            
            {/* Selected Actors */}
            {actorFilterNames.length > 0 && (
              <div className="flex flex-wrap gap-2 mb-3">
                {actorFilterNames.map((name, index) => (
                  <div key={index} className="flex items-center gap-2 px-3 py-1.5 bg-blue-600 text-white rounded-full text-sm">
                    <span>{name}</span>
                    <button
                      onClick={() => removeActor(index)}
                      className="hover:text-red-300 transition-colors"
                      aria-label={`Remove ${name}`}
                    >
                      <svg className="w-4 h-4" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                        <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M6 18L18 6M6 6l12 12" />
                      </svg>
                    </button>
                  </div>
                ))}
              </div>
            )}
            
            {/* Actor Input with Dropdown */}
            <div className="relative" ref={dropdownRef}>
              <input
                type="text"
                value={newActorName}
                onChange={(e) => {
                  setNewActorName(e.target.value);
                  setShowActorDropdown(true);
                }}
                onClick={() => setShowActorDropdown(true)}
                onKeyDown={(e) => {
                  if (e.key === 'Enter') {
                    e.preventDefault();
                    if (filteredActors.length > 0) {
                      addActor(filteredActors[0]);
                    } else {
                      addActorManually();
                    }
                  }
                }}
                placeholder="Actor name or faction EditorID (e.g., WhiterunGuardFaction)..."
                className="w-full px-4 py-2.5 text-base bg-gray-700 text-white rounded-lg border border-gray-600 focus:outline-none focus:border-blue-500"
              />
              
              {/* Dropdown */}
              {showActorDropdown && (newActorName || allActors.length > 0) && (
                <div className="absolute z-10 w-full mt-1 bg-gray-700 border border-gray-600 rounded-lg shadow-lg max-h-60 overflow-y-auto">
                  {filteredActors.length > 0 ? (
                    <>
                      <div className="px-3 py-2 text-xs text-gray-400 border-b border-gray-600 sticky top-0 bg-gray-700">
                        {nearbyActors.length > 0 ? 'Nearby & Recent Actors' : 'Recent Actors (Last 30 min)'}
                      </div>
                      {filteredActors.map((actor, index) => {
                        const isNearby = nearbyActors.some(na => na.formID.toUpperCase() === actor.formID.toUpperCase());
                        return (
                          <button
                            key={index}
                            onClick={() => addActor(actor)}
                            className="w-full px-4 py-2 text-left hover:bg-gray-600 transition-colors"
                          >
                            <div className="flex items-center justify-between">
                              <div className="flex-1">
                                <div className="text-white font-medium">{actor.name}</div>
                                <div className="text-xs text-blue-300">{actor.formID}</div>
                              </div>
                              <div className="text-xs text-gray-400 ml-2">
                                {isNearby ? (
                                  <span className="text-green-400">● Nearby</span>
                                ) : (
                                  new Date(actor.lastSeen).toLocaleTimeString()
                                )}
                              </div>
                            </div>
                          </button>
                        );
                      })}
                    </>
                  ) : newActorName ? (
                    <button
                      onClick={addActorManually}
                      className="w-full px-4 py-2.5 text-left hover:bg-gray-600 transition-colors text-white"
                    >
                      Add "{newActorName}" manually
                    </button>
                  ) : (
                    <div className="px-4 py-2.5 text-gray-400 text-sm">
                      No recent actors found
                    </div>
                  )}
                </div>
              )}
            </div>
            
            <div className="text-sm text-gray-400 mt-2">
              {actorFilterNames.length === 0 
                ? 'Empty = affects all actors. Add actors to make this entry actor-specific.'
                : `This entry will only affect: ${actorFilterNames.join(', ')}`
              }
            </div>
          </div>

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
        <div className="p-4 border-t border-gray-700 flex justify-end gap-3">
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
    </div>
  );
});

AdvancedEditModal.displayName = 'AdvancedEditModal';
