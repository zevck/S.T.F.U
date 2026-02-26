import { memo, useState, useEffect, useRef, useMemo } from 'react';
import { SKSE_API, log } from '../lib/skse-api';
import { useHistoryStore } from '../stores/history';
import { DialogueEntry } from '../types';

interface AdvancedEntryModalProps {
  isOpen: boolean;
  onClose: () => void;
  prefillEntry?: DialogueEntry | null;
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

export const AdvancedEntryModal = memo(({ isOpen, onClose, prefillEntry = null }: AdvancedEntryModalProps) => {
  const [identifier, setIdentifier] = useState('');
  const [isWhitelist, setIsWhitelist] = useState(false);
  const [blockType, setBlockType] = useState<'Soft' | 'Hard' | 'SkyrimNet'>('Soft');
  const [notes, setNotes] = useState('');
  const [detectedType, setDetectedType] = useState<'topic' | 'scene' | 'plugin'>('topic');
  const [categories, setCategories] = useState<string[]>(['Blacklist']);
  const [selectedCategory, setSelectedCategory] = useState('Blacklist');
  const [actorFilterNames, setActorFilterNames] = useState<string[]>([]);
  const [actorFilterFormIDs, setActorFilterFormIDs] = useState<string[]>([]);
  const [newActorFormID, setNewActorFormID] = useState('');
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
  
  // Filter actors based on FormID input
  const filteredActors = useMemo(() => {
    const search = newActorFormID.trim().toUpperCase().replace(/^0X/, '');
    if (!search) return allActors;
    
    return allActors.filter(actor => {
      const formID = actor.formID.toUpperCase().replace(/^0X/, '');
      const name = actor.name.toUpperCase();
      return formID.includes(search) || name.includes(search);
    });
  }, [allActors, newActorFormID]);
  
  // Set up global handler for detection results
  useEffect(() => {
    (window as any).handleIdentifierDetection = (result: DetectionResult) => {
      log(`[AdvancedEntry] Detection result: type=${result.type}, categories=${result.categories.length}`);
      setDetectedType(result.type);
      setCategories(result.categories);
      
      if (!result.categories.includes(selectedCategory)) {
        setSelectedCategory('Blacklist');
      }
    };
    
    // Set up handler for nearby actors
    (window as any).handleNearbyActors = (data: { actors: Actor[] }) => {
      log(`[AdvancedEntry] Received ${data.actors.length} nearby actors`);
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
        log(`[AdvancedEntry] Detecting type for: ${identifier}`);
        SKSE_API.sendToSKSE('detectIdentifierType', JSON.stringify({ identifier }));
      }, 300);
    } else {
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
  
  // Prefill form when entry provided or when modal opens with prefillEntry
  useEffect(() => {
    if (isOpen && prefillEntry) {
      // Priority: 1) EditorID (topic or scene), 2) FormID as fallback
      let identifier = '';
      if (prefillEntry.isScene) {
        identifier = prefillEntry.sceneEditorID || prefillEntry.topicFormID || '';
      } else {
        identifier = prefillEntry.topicEditorID || prefillEntry.topicFormID || '';
      }
      log(`[AdvancedEntry] Prefilling from history entry: ${identifier}`);
      setIdentifier(identifier);
      setNotes('');
      setActorFilterNames([]);
      setActorFilterFormIDs([]);
      // Don't auto-add speaker - let user choose actors manually
    }
  }, [prefillEntry, isOpen]);

  // Auto-load nearby actors when modal opens
  useEffect(() => {
    if (isOpen) {
      log('[AdvancedEntry] Modal opened, requesting nearby actors');
      SKSE_API.requestNearbyActors();
    }
  }, [isOpen]);

  // Reset form when modal opens/closes
  useEffect(() => {
    if (!isOpen) {
      setIdentifier('');
      setIsWhitelist(false);
      setNotes('');
      setBlockType('Soft');
      setDetectedType('topic');
      setCategories(['Blacklist']);
      setSelectedCategory('Blacklist');
      setActorFilterNames([]);
      setActorFilterFormIDs([]);
      setNewActorFormID('');
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
    // Check if FormID already exists
    if (!actorFilterFormIDs.includes(actor.formID)) {
      setActorFilterNames([...actorFilterNames, actor.name]);
      setActorFilterFormIDs([...actorFilterFormIDs, actor.formID]);
    }
    setNewActorFormID('');
    setShowActorDropdown(false);
  };
  
  // Normalize FormID to 0x format (supports A2C94, 0xA2C94, 000A2C94, etc.)
  const normalizeFormID = (input: string): string | null => {
    const trimmed = input.trim();
    if (!trimmed) return null;
    
    // Remove 0x prefix if present
    const hexString = trimmed.replace(/^0x/i, '');
    
    // Check if it's a valid hex string
    if (!/^[0-9a-fA-F]+$/.test(hexString)) {
      return null; // Invalid hex
    }
    
    // Pad to 8 digits and add 0x prefix
    const padded = hexString.toUpperCase().padStart(8, '0');
    return `0x${padded}`;
  };
  
  const addActorByFormID = () => {
    const input = newActorFormID.trim();
    if (!input) return;
    
    // Normalize FormID format
    const normalizedFormID = normalizeFormID(input);
    if (!normalizedFormID) {
      log(`[AdvancedEntry] Invalid FormID format: ${input}`);
      return;
    }
    
    // Check if FormID already added (compare normalized versions)
    const existingNormalized = actorFilterFormIDs.map(f => normalizeFormID(f) || f);
    if (existingNormalized.includes(normalizedFormID)) {
      setNewActorFormID('');
      return;
    }
    
    // Try to find actor in available actors (compare normalized)
    const actor = allActors.find(a => {
      const actorNormalized = normalizeFormID(a.formID);
      return actorNormalized === normalizedFormID;
    });
    
    if (actor) {
      addActor(actor);
    } else {
      // Add with FormID only, name will be looked up by backend
      setActorFilterFormIDs([...actorFilterFormIDs, normalizedFormID]);
      setActorFilterNames([...actorFilterNames, `Unknown (${normalizedFormID})`]);
      setNewActorFormID('');
      setShowActorDropdown(false);
    }
  };
  
  const removeActor = (index: number) => {
    setActorFilterNames(actorFilterNames.filter((_, i) => i !== index));
    setActorFilterFormIDs(actorFilterFormIDs.filter((_, i) => i !== index));
  };

  const handleCreate = () => {
    if (!identifier.trim()) {
      log('[AdvancedEntry] Empty identifier, aborting');
      return;
    }

    // Validate array synchronization
    if (actorFilterNames.length !== actorFilterFormIDs.length) {
      log(`[AdvancedEntry] ERROR: Array desync! Names: ${actorFilterNames.length}, FormIDs: ${actorFilterFormIDs.length}`);
      return;
    }

    log(`[AdvancedEntry] Creating entry: identifier=${identifier}, blockType=${blockType}, category=${isWhitelist ? 'Whitelist' : selectedCategory}, isWhitelist=${isWhitelist}, actors=${actorFilterNames.length}`);

    // Call createAdvancedEntry handler
    SKSE_API.sendToSKSE('createAdvancedEntry', JSON.stringify({
      identifier,
      blockType,
      category: isWhitelist ? 'Whitelist' : selectedCategory,
      notes,
      isWhitelist,
      actorFilterNames,
      actorFilterFormIDs
    }));

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
      <div className="bg-gray-800 border border-gray-700 rounded-lg shadow-2xl w-full max-w-3xl flex flex-col max-h-[90vh]">
        {/* Header */}
        <div className="flex items-center justify-between p-4 border-b border-gray-700">
          <h2 id="modal-title" className="text-xl font-bold text-white">
            {isWhitelist ? 'Advanced Whitelist Entry' : 'Advanced Blacklist Entry'}
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

        {/* Content - Scrollable */}
        <div className="p-6 space-y-4 overflow-y-auto">
          {/* Identifier Input */}
          <div>
            <label className="block text-base font-medium text-gray-300 mb-2">
              Identifier <span className="text-red-400">*</span>
            </label>
            <input
              type="text"
              value={identifier}
              onChange={(e) => setIdentifier(e.target.value)}
              placeholder={isWhitelist ? "EditorID, FormID, or Plugin.esp" : "EditorID or FormID (e.g., GenericGreeting or 02707A)"}
              className="w-full px-4 py-2.5 text-base bg-gray-700 text-white rounded-lg border border-gray-600 focus:outline-none focus:border-blue-500"
              autoFocus
            />
            <div className="text-sm text-gray-400 mt-1">
              {detectedType === 'scene' && '🎬 Detected as Scene'}
              {detectedType === 'topic' && '💬 Detected as Topic'}
              {detectedType === 'plugin' && '📦 Detected as Plugin'}
            </div>
          </div>

          {/* Actions */}
          <div>
            <label className="block text-base font-medium text-gray-300 mb-2">
              Actions <span className="text-red-400">*</span>
            </label>
            <div className="flex flex-wrap gap-4">
              {/* Block Type Radio Buttons */}
              <label className="flex items-center gap-2 cursor-pointer">
                <input
                  type="radio"
                  name="actionType"
                  checked={!isWhitelist && blockType === 'Soft'}
                  onChange={() => {
                    setIsWhitelist(false);
                    setBlockType('Soft');
                  }}
                  className="w-5 h-5 text-blue-600 focus:ring-blue-500 cursor-pointer"
                />
                <span className="text-base text-white">Soft Block</span>
              </label>
              <label className="flex items-center gap-2 cursor-pointer">
                <input
                  type="radio"
                  name="actionType"
                  checked={!isWhitelist && blockType === 'Hard'}
                  onChange={() => {
                    setIsWhitelist(false);
                    setBlockType('Hard');
                  }}
                  className="w-5 h-5 text-blue-600 focus:ring-blue-500 cursor-pointer"
                />
                <span className="text-base text-white">Hard Block</span>
              </label>
              {/* Only show SkyrimNet option if prefill entry is SkyrimNet blockable */}
              {prefillEntry?.skyrimNetBlockable && (
                <label className="flex items-center gap-2 cursor-pointer">
                  <input
                    type="radio"
                    name="actionType"
                    checked={!isWhitelist && blockType === 'SkyrimNet'}
                    onChange={() => {
                      setIsWhitelist(false);
                      setBlockType('SkyrimNet');
                    }}
                    className="w-5 h-5 text-blue-600 focus:ring-blue-500 cursor-pointer"
                  />
                  <span className="text-base text-white">SkyrimNet Only</span>
                </label>
              )}
              
              {/* Whitelist Radio Button */}
              <label className="flex items-center gap-2 cursor-pointer">
                <input
                  type="radio"
                  name="actionType"
                  checked={isWhitelist}
                  onChange={() => setIsWhitelist(true)}
                  className="w-5 h-5 text-blue-600 focus:ring-blue-500 cursor-pointer"
                />
                <span className="text-base text-white">Whitelist</span>
              </label>
            </div>
            <div className="text-sm text-gray-400 mt-1">
              {isWhitelist 
                ? 'Whitelist entries allow dialogue to play regardless of other filters'
                : `Soft blocks mute audio/subtitles. Hard blocks prevent dialogue.${prefillEntry?.skyrimNetBlockable ? ' SkyrimNet blocks only from AI.' : ''}`
              }
            </div>
          </div>

          {/* Filter Category - only for blacklist */}
          {!isWhitelist && (
          <div>
            <label className="block text-base font-medium text-gray-300 mb-2">
              Filter Category <span className="text-red-400">*</span>
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

          {/* Actor Filters */}
          <div>
            <label className="block text-base font-medium text-gray-300 mb-2">
              Actor Filters (Optional)
            </label>
            <div className="text-sm text-gray-400 mb-2">
              Enter actor FormID (e.g., 0x00013478). Multiple NPCs can share names, so FormID ensures accuracy.
            </div>
            
            {/* Selected Actors */}
            {actorFilterFormIDs.length > 0 && (
              <div className="flex flex-wrap gap-2 mb-3">
                {actorFilterNames.map((name, index) => (
                  <div key={index} className="bg-blue-600 text-white px-3 py-1 rounded-full flex items-center gap-2 text-sm">
                    <span>{name}</span>
                    <span className="text-xs text-blue-200">({actorFilterFormIDs[index]})</span>
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
            
            {/* Actor Input - Only input field and Add button, no Get Nearby button */}
            <div className="flex gap-2 mb-2">
              <input
                type="text"
                value={newActorFormID}
                onChange={(e) => {
                  setNewActorFormID(e.target.value);
                  setShowActorDropdown(true);
                }}
                onClick={() => setShowActorDropdown(true)}
                placeholder="Enter FormID (e.g., 0x00013478) or select..."
                className="flex-1 px-4 py-2 text-base bg-gray-700 text-white rounded-lg border border-gray-600 focus:outline-none focus:border-blue-500"
              />
              <button
                onClick={addActorByFormID}
                disabled={!newActorFormID.trim()}
                className="px-4 py-2 text-base bg-blue-600 hover:bg-blue-700 disabled:bg-gray-600 disabled:cursor-not-allowed text-white rounded-lg transition-colors"
              >
                Add
              </button>
            </div>
            
            {/* Dropdown for actors - relative container */}
            <div className="relative" ref={dropdownRef}>
              {showActorDropdown && filteredActors.length > 0 && (
                <div className="absolute z-10 w-full bg-gray-700 border border-gray-600 rounded-lg shadow-lg max-h-60 overflow-y-auto">
                  <div className="px-3 py-2 text-xs text-gray-400 border-b border-gray-600 sticky top-0 bg-gray-700">
                    {nearbyActors.length > 0 && 'Nearby & Recent Actors'}
                    {nearbyActors.length === 0 && 'Recent Actors (Last 30 min)'}
                  </div>
                  {filteredActors.map((actor, index) => {
                    const isNearby = nearbyActors.some(n => n.formID.toUpperCase() === actor.formID.toUpperCase());
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
            
            <div className="text-sm text-gray-400 mt-2">
              {actorFilterFormIDs.length === 0 
                ? 'Empty = affects all actors. Add FormIDs to make this entry actor-specific.'
                : `This entry will only affect actors with FormIDs: ${actorFilterFormIDs.join(', ')}`
              }
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
              placeholder="Add notes about why this entry was created..."
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

AdvancedEntryModal.displayName = 'AdvancedEntryModal';
