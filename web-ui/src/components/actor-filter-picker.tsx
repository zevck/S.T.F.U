import { useState, useEffect, useRef, useMemo } from 'react';
import { SKSE_API, log } from '../lib/skse-api';
import { useHistoryStore } from '../stores/history';
import { CloseIcon } from './modal';

interface Actor {
  name: string;
  formID: string;
  lastSeen: number;  // ms since epoch
}

export interface ActorFilters {
  actorFilterNames: string[];
  actorFilterFormIDs: string[];    // Kept index-aligned with actorFilterNames
  factionFilterEditorIDs: string[];
}

interface ActorFilterPickerProps {
  filters: ActorFilters;
  onChange: (filters: ActorFilters) => void;
  label: string;
  description?: string;
  logTag: string;
}

const isRealFormID = (formID: string) => !!formID && formID !== '0x00000000' && formID !== '0x0';

// Actor & faction filter editor shared by the create and edit entry modals.
// Suggests nearby actors (requested from the game when the picker mounts) and
// actors heard in the last 30 minutes of history. Typed names that don't match
// a known actor are added as faction EditorID filters.
export const ActorFilterPicker = ({ filters, onChange, label, description, logTag }: ActorFilterPickerProps) => {
  const { actorFilterNames, actorFilterFormIDs, factionFilterEditorIDs } = filters;
  const [newActorName, setNewActorName] = useState('');
  const [showActorDropdown, setShowActorDropdown] = useState(false);
  const [nearbyActors, setNearbyActors] = useState<Actor[]>([]);
  const dropdownRef = useRef<HTMLDivElement>(null);

  const historyEntries = useHistoryStore(state => state.entries);

  // Unique speakers from history in the last 30 minutes, most recent first
  const recentActors = useMemo(() => {
    const thirtyMinutesAgo = Date.now() - (30 * 60 * 1000);
    const actorMap = new Map<string, Actor>();

    historyEntries.forEach(entry => {
      const seenMs = entry.timestamp * 1000;  // history timestamps are in seconds
      if (seenMs > thirtyMinutesAgo && entry.speaker) {
        const key = entry.speaker.toLowerCase();
        const existing = actorMap.get(key);
        if (!existing || seenMs > existing.lastSeen) {
          actorMap.set(key, { name: entry.speaker, formID: entry.speakerFormID || '', lastSeen: seenMs });
        }
      }
    });

    return Array.from(actorMap.values()).sort((a, b) => b.lastSeen - a.lastSeen);
  }, [historyEntries]);

  // Nearby actors first, then recent ones not already listed (deduped by FormID)
  const allActors = useMemo(() => {
    const actorMap = new Map<string, Actor>();
    [...nearbyActors, ...recentActors].forEach(actor => {
      const key = actor.formID.toUpperCase();
      if (isRealFormID(actor.formID) && !actorMap.has(key)) {
        actorMap.set(key, actor);
      }
    });
    return Array.from(actorMap.values());
  }, [nearbyActors, recentActors]);

  // Filter suggestions by the typed text (name or FormID), hiding already-added actors
  const filteredActors = useMemo(() => {
    const search = newActorName.trim().toLowerCase();
    return allActors.filter(actor => {
      if (actorFilterNames.includes(actor.name)) return false;
      if (!search) return true;
      const matchesName = actor.name.toLowerCase().includes(search);
      const matchesFormID = actor.formID.toLowerCase().replace('0x', '').includes(search.replace('0x', ''));
      return matchesName || matchesFormID;
    });
  }, [allActors, newActorName, actorFilterNames]);

  // Request nearby actors from the game and listen for the reply while mounted
  useEffect(() => {
    (window as any).handleNearbyActors = (data: { actors: Actor[] }) => {
      log(`[${logTag}] Received ${data.actors.length} nearby actors`);
      setNearbyActors(data.actors);
    };
    log(`[${logTag}] Requesting nearby actors`);
    SKSE_API.requestNearbyActors();

    return () => {
      delete (window as any).handleNearbyActors;
    };
  }, [logTag]);

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

  const closeInput = () => {
    setNewActorName('');
    setShowActorDropdown(false);
  };

  const addActor = (actor: Actor) => {
    if (!actorFilterNames.includes(actor.name)) {
      onChange({
        ...filters,
        actorFilterNames: [...actorFilterNames, actor.name],
        // Always add a FormID (empty if unknown) so the two arrays stay aligned
        actorFilterFormIDs: [...actorFilterFormIDs, actor.formID || ''],
      });
    }
    closeInput();
  };

  const addActorManually = () => {
    const input = newActorName.trim();
    if (!input) return;
    // A known actor name adds that actor; anything else is treated as a faction EditorID
    const matchedActor = allActors.find(a => a.name.toLowerCase() === input.toLowerCase());
    if (matchedActor) {
      addActor(matchedActor);
      return;
    }
    if (!factionFilterEditorIDs.includes(input)) {
      onChange({ ...filters, factionFilterEditorIDs: [...factionFilterEditorIDs, input] });
    }
    closeInput();
  };

  const removeActor = (index: number) => {
    onChange({
      ...filters,
      actorFilterNames: actorFilterNames.filter((_, i) => i !== index),
      actorFilterFormIDs: actorFilterFormIDs.filter((_, i) => i !== index),
    });
  };

  const removeFaction = (index: number) => {
    onChange({ ...filters, factionFilterEditorIDs: factionFilterEditorIDs.filter((_, i) => i !== index) });
  };

  const chip = (key: string, text: string, colorClass: string, onRemove: () => void) => (
    <div key={key} className={`flex items-center gap-2 px-3 py-1.5 ${colorClass} text-white rounded-full text-sm`}>
      <span>{text}</span>
      <button onClick={onRemove} className="hover:text-red-300 transition-colors" aria-label={`Remove ${text}`}>
        <CloseIcon className="w-4 h-4" />
      </button>
    </div>
  );

  return (
    <div>
      <label className="block text-base font-medium text-gray-300 mb-2">
        {label}
      </label>
      {description && <div className="text-sm text-gray-400 mb-2">{description}</div>}

      {/* Combined actor & faction chips */}
      {(actorFilterNames.length > 0 || factionFilterEditorIDs.length > 0) && (
        <div className="flex flex-wrap gap-2 mb-3">
          {actorFilterNames.map((name, index) => chip(`actor-${index}`, name, 'bg-blue-600', () => removeActor(index)))}
          {factionFilterEditorIDs.map((id, index) => chip(`faction-${index}`, id, 'bg-purple-700', () => removeFaction(index)))}
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
                Add "{newActorName}" as faction filter
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
        Actors (blue) from the dropdown. Unknown names become faction EditorID filters (purple).
      </div>
    </div>
  );
};
