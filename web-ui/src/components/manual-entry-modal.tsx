import { memo, useState, useEffect, useRef, useMemo } from 'react';
import { SKSE_API, log } from '../lib/skse-api';
import { useHistoryStore } from '../stores/history';

interface ManualEntryModalProps {
  isOpen: boolean;
  onClose: () => void;
  isWhitelist?: boolean;
}

interface DetectionResult {
  type: 'topic' | 'scene' | 'plugin';
  categories: string[];
}

interface Actor {
  name: string;
  formID: string;
  lastSeen: number;
}

export const ManualEntryModal = memo(({ isOpen, onClose, isWhitelist = false }: ManualEntryModalProps) => {
  const [identifier, setIdentifier] = useState('');
  const [blockType, setBlockType] = useState<'Soft' | 'Hard'>('Soft');
  const [notes, setNotes] = useState('');
  const [detectedType, setDetectedType] = useState<'topic' | 'scene' | 'plugin'>('topic');
  const [categories, setCategories] = useState<string[]>(['Blacklist']);
  const [selectedCategory, setSelectedCategory] = useState('Blacklist');
  const [actorFilterNames, setActorFilterNames] = useState<string[]>([]);
  const [actorFilterFormIDs, setActorFilterFormIDs] = useState<string[]>([]);
  const [newActorName, setNewActorName] = useState('');
  const [showActorDropdown, setShowActorDropdown] = useState(false);
  const [nearbyActors, setNearbyActors] = useState<Actor[]>([]);
  const detectionTimeoutRef = useRef<number | null>(null);
  const dropdownRef = useRef<HTMLDivElement>(null);
  
  const historyEntries = useHistoryStore(state => state.entries);
  
  // Get unique actors from recent history (last 30 minutes)
  const actors = useMemo(() => {
    const now = Date.now();
    const thirtyMinutesAgo = now - (30 * 60 * 1000);
    
    const actorMap = new Map<string, Actor>();
    
    // Iterate through history entries
    historyEntries.forEach(entry => {
      // Convert timestamp from seconds to milliseconds
      if (entry.timestamp * 1000 > thirtyMinutesAgo && entry.speaker) {
        const formID = entry.speakerFormID || '';
        const key = entry.speaker.toLowerCase();
        
        if (!actorMap.has(key)) {
          actorMap.set(key, {
            name: entry.speaker,
            formID: formID,
            lastSeen: entry.timestamp * 1000
          });
        } else {
          // Update if this entry is more recent
          const existing = actorMap.get(key)!;
          if (entry.timestamp * 1000 > existing.lastSeen) {
            actorMap.set(key, {
              name: entry.speaker,
              formID: formID,
              lastSeen: entry.timestamp * 1000
            });
          }
        }
      }
    });
    
    // Convert to array and sort by most recent
    return Array.from(actorMap.values()).sort((a, b) => b.lastSeen - a.lastSeen);
  }, [historyEntries]);
  
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

  // Set up global handlers for detection results and nearby actors
  useEffect(() => {
    (window as any).handleIdentifierDetection = (result: DetectionResult) => {
      log(`[ManualEntry] Detection result: type=${result.type}, categories=${result.categories.length}`);
      setDetectedType(result.type);
      setCategories(result.categories);
      
      // Reset to "Blacklist" if current selection is not in new categories
      if (!result.categories.includes(selectedCategory)) {
        setSelectedCategory('Blacklist');
      }
    };
    
    // Set up handler for nearby actors
    (window as any).handleNearbyActors = (data: { actors: Actor[] }) => {
      log(`[ManualEntry] Received ${data.actors.length} nearby actors`);
      setNearbyActors(data.actors);
    };

    return () => {
      delete (window as any).handleIdentifierDetection;
      delete (window as any).handleNearbyActors;
    };
  }, [selectedCategory]);

  // Detect identifier type when it changes (debounced)
  useEffect(() => {
    if (detectionTimeoutRef.current) {
      clearTimeout(detectionTimeoutRef.current);
    }

    if (identifier.trim()) {
      detectionTimeoutRef.current = window.setTimeout(() => {
        log(`[ManualEntry] Detecting type for: ${identifier}`);
        SKSE_API.sendToSKSE('detectIdentifierType', JSON.stringify({ identifier }));
      }, 300); // 300ms debounce
    } else {
      // Reset to default when empty
      setDetectedType('topic');
      setCategories(['Blacklist']);
      setSelectedCategory('Blacklist');
    }

    return () => {
      if (detectionTimeoutRef.current) {
        clearTimeout(detectionTimeoutRef.current);
      }
    };
  }, [identifier]);

  // Auto-load nearby actors when modal opens
  useEffect(() => {
    if (isOpen) {
      log('[ManualEntry] Modal opened, requesting nearby actors');
      SKSE_API.requestNearbyActors();
    }
  }, [isOpen]);
  
  // Reset form when modal opens/closes
  useEffect(() => {
    if (!isOpen) {
      setIdentifier('');
      setNotes('');
      setBlockType('Soft');
      setDetectedType('topic');
      setCategories(['Blacklist']);
      setSelectedCategory('Blacklist');
      setActorFilterNames([]);
      setActorFilterFormIDs([]);
      setNewActorName('');
      setShowActorDropdown(false);
      setNearbyActors([]);
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

  if (!isOpen) return null;

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
      // FormID will be empty for manually entered actors
      setActorFilterFormIDs([...actorFilterFormIDs, '']);
    }
    setNewActorName('');
    setShowActorDropdown(false);
  };
  
  const removeActor = (index: number) => {
    setActorFilterNames(actorFilterNames.filter((_, i) => i !== index));
    setActorFilterFormIDs(actorFilterFormIDs.filter((_, i) => i !== index));
  };

  const handleCreate = () => {
    if (!identifier.trim()) {
      log('[ManualEntry] Empty identifier, aborting');
      return;
    }

    // Validate array synchronization
    if (actorFilterNames.length !== actorFilterFormIDs.length) {
      log(`[ManualEntry] ERROR: Array desync! Names: ${actorFilterNames.length}, FormIDs: ${actorFilterFormIDs.length}`);
      return;
    }

    log(`[ManualEntry] Creating entry: identifier=${identifier}, blockType=${blockType}, category=${isWhitelist ? 'Whitelist' : selectedCategory}, isWhitelist=${isWhitelist}, actors=${actorFilterNames.length}`);

    // Call createAdvancedEntry handler (supports actor filters)
    SKSE_API.sendToSKSE('createAdvancedEntry', JSON.stringify({
      identifier,
      blockType,
      category: isWhitelist ? 'Whitelist' : selectedCategory,
      notes,
      isWhitelist,
      actorFilterNames,
      actorFilterFormIDs
    }));

    // Close modal
    onClose();
  };

  return (
    <div
      className="fixed inset-0 z-50 flex items-center justify-center bg-black bg-opacity-75"
      onClick={handleBackdropClick}
      role="dialog"
      aria-modal="true"
      aria-labelledby="modal-title"
    >
      <div className="bg-gray-800 border border-gray-700 rounded-lg shadow-2xl w-full max-w-2xl flex flex-col">
        {/* Header */}
        <div className="flex items-center justify-between p-4 border-b border-gray-700">
          <h2 id="modal-title" className="text-xl font-bold text-white">
            {isWhitelist ? 'Create Whitelist Entry' : 'Create Blacklist Entry'}
          </h2>
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
            <div className="text-sm text-gray-400 mt-1">
              {detectedType === 'scene' && '🎬 Detected as Scene'}
              {detectedType === 'topic' && '💬 Detected as Topic'}
              {detectedType === 'plugin' && '📦 Detected as Plugin'}
            </div>
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
            <div className="text-sm text-gray-400 mt-1">
              Soft blocks mute audio and hide subtitles. Hard blocks prevent dialogue before it plays.
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

          {/* Actor & Faction Filtering */}
          <div>
            <label className="block text-base font-medium text-gray-300 mb-2">
              Actor & Faction Filters (Optional)
            </label>
            <div className="text-sm text-gray-400 mb-2">
              {isWhitelist 
                ? 'Leave empty to whitelist for all actors. Add specific actors/factions to only allow their dialogue.'
                : 'Leave empty to affect all actors. Add specific actors/factions to only block their dialogue.'
              }
            </div>
            
            {/* Selected actors as chips */}
            {actorFilterNames.length > 0 && (
              <div className="flex flex-wrap gap-2 mb-3">
                {actorFilterNames.map((name, index) => (
                  <div key={index} className="bg-blue-600 text-white px-3 py-1 rounded-full flex items-center gap-2 text-sm">
                    <span>{name}</span>
                    <button
                      onClick={() => removeActor(index)}
                      className="hover:text-red-300 transition-colors"
                      aria-label={`Remove ${name}`}
                    >
                      ✕
                    </button>
                  </div>
                ))}
              </div>
            )}
            
            {/* Actor input with dropdown */}
            <div className="relative" ref={dropdownRef}>
              <div className="flex gap-2">
                <input
                  type="text"
                  value={newActorName}
                  onChange={(e) => {
                    setNewActorName(e.target.value);
                    setShowActorDropdown(true);
                  }}
                  onClick={() => setShowActorDropdown(true)}
                  placeholder="Actor name or faction EditorID (e.g., WhiterunGuardFaction)..."
                  className="flex-1 px-4 py-2 text-base bg-gray-700 text-white rounded-lg border border-gray-600 focus:outline-none focus:border-blue-500"
                />
                <button
                  onClick={addActorManually}
                  disabled={!newActorName.trim()}
                  className="px-4 py-2 text-base bg-blue-600 hover:bg-blue-700 disabled:bg-gray-600 disabled:cursor-not-allowed text-white rounded-lg transition-colors"
                >
                  Add
                </button>
              </div>
              
              {/* Dropdown for nearby and recent actors */}
              {showActorDropdown && filteredActors.length > 0 && (
                <div className="absolute z-10 w-full mt-1 bg-gray-700 border border-gray-600 rounded-lg shadow-lg max-h-60 overflow-y-auto">
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
                </div>
              )}
            </div>
          </div>

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
      </div>
    </div>
  );
});

ManualEntryModal.displayName = 'ManualEntryModal';

