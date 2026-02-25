import { useState, useCallback } from 'react';
import { Upload, Settings as SettingsIcon, Swords, MessageSquare, Users, Sparkles } from 'lucide-react';
import { SKSE_API, log } from '../lib/skse-api';
import { useSettingsStore } from '../stores/settings';

// Define all subtypes organized by category
const COMBAT_SUBTYPES = {
  attackDialogue: [
    { id: 40, name: 'Accept Yield', tooltip: 'NPCs accepting your surrender' },
    { id: 26, name: 'Attack', tooltip: 'Attack dialogue - e.g. "I\'ve had enough of you!", "Die, beast!"' },
    { id: 28, name: 'Bash', tooltip: 'Bash dialogue - e.g. "Hunh!", "Gah!", "Yah!"' },
    { id: 35, name: 'Block', tooltip: 'Block dialogue - e.g. "Close...", "Easily blocked!"' },
    { id: 29, name: 'Hit', tooltip: 'Hit dialogue - e.g. "Do your worst!", "That your best?"' },
    { id: 27, name: 'Power Attack', tooltip: 'Power attack dialogue - mostly grunts' },
  ],
  combatCommentary: [
    { id: 37, name: 'Ally Killed', tooltip: 'NPCs reacting when allies die - e.g. "No!! How dare you!"' },
    { id: 65, name: 'Detect Friend Die', tooltip: 'NPCs detecting when a friend has died' },
    { id: 75, name: 'Observe Combat', tooltip: 'NPCs observing combat - e.g. "Gods! Another fight."' },
    { id: 36, name: 'Taunt', tooltip: 'Taunt dialogue - e.g. "Skyrim belongs to the Nords!"' },
  ],
  combatReactions: [
    { id: 32, name: 'Avoid Threat', tooltip: 'NPCs avoiding threats - e.g. "I need to get away!"' },
    { id: 30, name: 'Flee', tooltip: 'Flee dialogue - e.g. "Ah! It burns!", "You win!"' },
    { id: 41, name: 'Pickpocket (Combat)', tooltip: 'NPCs reacting to pickpocket attempts during combat' },
    { id: 39, name: 'Yield', tooltip: 'Yield dialogue - e.g. "I yield! I yield!"' },
  ],
  detectionAlert: [
    { id: 55, name: 'Alert Idle', tooltip: 'Alert idle - e.g. "Anybody there?", "Who\'s there?"' },
    { id: 58, name: 'Alert to Combat', tooltip: 'Alert to combat - e.g. "Now you\'re mine!"' },
    { id: 60, name: 'Alert to Normal', tooltip: 'Alert to normal - e.g. "Must have been nothing."' },
    { id: 57, name: 'Normal to Alert', tooltip: 'Normal to alert - e.g. "Did you hear something?"' },
    { id: 59, name: 'Normal to Combat', tooltip: 'Normal to combat - e.g. "You\'re dead!"' },
  ],
  lostSearch: [
    { id: 56, name: 'Lost Idle', tooltip: 'Lost idle - e.g. "I\'m going to find you."' },
    { id: 64, name: 'Lost to Combat', tooltip: 'Lost to combat - e.g. "There you are."' },
    { id: 63, name: 'Lost to Normal', tooltip: 'Lost to normal - e.g. "Must have run off."' },
  ],
  transitions: [
    { id: 31, name: 'Bleedout', tooltip: 'Bleedout dialogue - e.g. "Help me...", "I can\'t move..."' },
    { id: 62, name: 'Combat to Lost', tooltip: 'Combat to lost - e.g. "Is it safe?"' },
    { id: 61, name: 'Combat to Normal', tooltip: 'Combat to normal - e.g. "That takes care of that."' },
    { id: 33, name: 'Death', tooltip: 'Death dialogue - e.g. "Thank you.", "Free...again."' },
  ],
};

const GENERIC_SUBTYPES = {
  behaviors: [
    { id: 98, name: 'Actor Collide', tooltip: 'NPCs reacting when you bump into them' },
    { id: 70, name: 'Barter Exit', tooltip: 'Merchants\' closing remarks when ending trade' },
    { id: 76, name: 'Notice Corpse', tooltip: 'NPCs reacting to seeing dead bodies' },
    { id: 89, name: 'Pursue Idle Topic', tooltip: 'Guards pursuing criminals' },
    { id: 77, name: 'Time To Go', tooltip: 'NPCs telling you to leave' },
  ],
  crimeStealth: [
    { id: 42, name: 'Assault', tooltip: 'Civilian NPCs reacting to witnessing assault' },
    { id: 44, name: 'Assault NC', tooltip: 'Assault reactions outside crime system' },
    { id: 43, name: 'Murder', tooltip: 'Civilian NPCs reacting to witnessing murder' },
    { id: 45, name: 'Murder NC', tooltip: 'Murder reactions outside crime system' },
    { id: 46, name: 'Pickpocket NC', tooltip: 'Pickpocket comments outside crime system' },
    { id: 88, name: 'Pickpocket Topic', tooltip: 'Dialogue about pickpocketing attempts' },
    { id: 38, name: 'Steal', tooltip: 'NPCs reacting to you stealing items' },
    { id: 47, name: 'Steal From NC', tooltip: 'NPCs commenting on theft outside crime system' },
    { id: 49, name: 'Trespass', tooltip: 'NPCs warning about entering restricted areas' },
    { id: 48, name: 'Trespass Against NC', tooltip: 'Trespass warnings outside crime system' },
    { id: 50, name: 'Werewolf Transform Crime', tooltip: 'NPCs reacting to werewolf transformations' },
  ],
  objectInteractions: [
    { id: 85, name: 'Destroy Object', tooltip: 'NPCs reacting to you destroying objects' },
    { id: 84, name: 'Knock Over Object', tooltip: 'NPCs commenting when you knock things over' },
    { id: 87, name: 'Locked Object', tooltip: 'NPCs commenting on locked containers/doors' },
    { id: 86, name: 'Stand On Furniture', tooltip: 'NPCs commenting when you stand on furniture' },
    { id: 82, name: 'Z-Key Object', tooltip: 'NPCs commenting when you pick up objects' },
  ],
  playerActions: [
    { id: 91, name: 'Player Cast Projectile', tooltip: 'NPCs reacting to projectile spells' },
    { id: 92, name: 'Player Cast Self', tooltip: 'NPCs reacting to self-cast spells' },
    { id: 99, name: 'Player In Iron Sights', tooltip: 'NPCs reacting to being aimed at' },
    { id: 93, name: 'Player Shout', tooltip: 'NPCs reacting to you using shouts' },
    { id: 81, name: 'Shoot Bow', tooltip: 'NPCs reacting to you shooting arrows near them' },
    { id: 80, name: 'Swing Melee Weapon', tooltip: 'NPCs reacting to you swinging weapons' },
  ],
  social: [
    { id: 78, name: 'Goodbye', tooltip: 'Farewells when ending conversations' },
    { id: 79, name: 'Hello', tooltip: 'Greetings from NPCs' },
    { id: 94, name: 'Idle', tooltip: 'Random idle chatter NPCs make' },
    { id: 74, name: 'Training Exit', tooltip: 'Trainer dialogue when exiting training' },
  ],
};

const FOLLOWER_SUBTYPES = {
  commands: [
    { id: 16, name: 'Agree', tooltip: 'Follower agreeing to requests or commands' },
    { id: 18, name: 'Exit Favor State', tooltip: 'Follower dialogue when ending favor/command mode' },
    { id: 19, name: 'Moral Refusal', tooltip: 'Follower refuses requests that conflict with morals' },
    { id: 13, name: 'Refuse', tooltip: 'Follower refusing general requests' },
    { id: 15, name: 'Show', tooltip: 'Follower ready for a command' },
  ],
};

const OTHER_SUBTYPES = {
  voicePowers: [
    { id: 53, name: 'Voice Power End (Long)', tooltip: 'Long shout ending dialogue' },
    { id: 52, name: 'Voice Power End (Short)', tooltip: 'Short shout ending dialogue' },
    { id: 54, name: 'Voice Power Start (Long)', tooltip: 'Long shout starting dialogue' },
    { id: 51, name: 'Voice Power Start (Short)', tooltip: 'Short shout starting dialogue' },
  ],
};

type CategoryTab = 'master' | 'combat' | 'generic' | 'follower' | 'other';

interface SubcategoryHeaderProps {
  title: string;
}

const SubcategoryHeader = ({ title }: SubcategoryHeaderProps) => (
  <div className="text-base font-semibold text-blue-400 mb-2 mt-4 first:mt-0">{title}</div>
);

interface ToggleProps {
  label: string;
  checked: boolean;
  onChange: (checked: boolean) => void;
  tooltip?: string;
}

const Toggle = ({ label, checked, onChange, tooltip }: ToggleProps) => (
  <label className="flex items-center gap-2 cursor-pointer group" title={tooltip}>
    <input
      type="checkbox"
      checked={checked}
      onChange={(e) => onChange(e.target.checked)}
      className="w-5 h-5 rounded border-gray-600 bg-gray-700 text-blue-600 focus:ring-blue-500 cursor-pointer"
    />
    <span className="text-base text-white group-hover:text-blue-300 transition-colors">{label}</span>
  </label>
);

export const Settings = () => {
  // Category navigation state
  const [activeCategory, setActiveCategory] = useState<CategoryTab>('master');
  
  // Read settings from global store (updated by C++ in real-time)
  const blacklistEnabled = useSettingsStore(state => state.blacklistEnabled);
  const skyrimNetEnabled = useSettingsStore(state => state.skyrimNetEnabled);
  const scenesEnabled = useSettingsStore(state => state.scenesEnabled);
  const bardSongsEnabled = useSettingsStore(state => state.bardSongsEnabled);
  const combatGruntsBlocked = useSettingsStore(state => state.combatGruntsBlocked);
  const followerCommentaryEnabled = useSettingsStore(state => state.followerCommentaryEnabled);
  const subtypes = useSettingsStore(state => state.subtypes);

  const toggleSubtype = useCallback((subtypeId: number) => {
    // C++ will toggle the global and send back updated settings
    SKSE_API.toggleSubtypeFilter(subtypeId);
    log(`[Settings] Requested toggle for subtype ${subtypeId}`);
  }, []);

  const handleEnableAll = useCallback((categorySubtypes: { id: number; name: string; tooltip?: string }[]) => {
    // Toggle each subtype that's currently disabled
    categorySubtypes.forEach(subtype => {
      // Only toggle if currently disabled (false/undefined in store)
      if (!subtypes[subtype.id]) {
        SKSE_API.toggleSubtypeFilter(subtype.id);
      }
    });
    log(`[Settings] Requested enable all for category`);
  }, [subtypes]);

  const handleDisableAll = useCallback((categorySubtypes: { id: number; name: string; tooltip?: string }[]) => {
    // Toggle each subtype that's currently enabled
    categorySubtypes.forEach(subtype => {
      // Only toggle if currently enabled (true in store)
      if (subtypes[subtype.id]) {
        SKSE_API.toggleSubtypeFilter(subtype.id);
      }
    });
    log(`[Settings] Requested disable all for category`);
  }, [subtypes]);

  const handleEnableAllCombat = useCallback(() => {
    handleEnableAll(Object.values(COMBAT_SUBTYPES).flat());
    if (!combatGruntsBlocked) {
      SKSE_API.setCombatGruntsBlocked(true);
    }
  }, [handleEnableAll, combatGruntsBlocked]);

  const handleDisableAllCombat = useCallback(() => {
    handleDisableAll(Object.values(COMBAT_SUBTYPES).flat());
    if (combatGruntsBlocked) {
      SKSE_API.setCombatGruntsBlocked(false);
    }
  }, [handleDisableAll, combatGruntsBlocked]);

  const handleEnableAllFollower = useCallback(() => {
    handleEnableAll(Object.values(FOLLOWER_SUBTYPES).flat());
    if (!followerCommentaryEnabled) {
      SKSE_API.setFollowerCommentaryEnabled(true);
    }
  }, [handleEnableAll, followerCommentaryEnabled]);

  const handleDisableAllFollower = useCallback(() => {
    handleDisableAll(Object.values(FOLLOWER_SUBTYPES).flat());
    if (followerCommentaryEnabled) {
      SKSE_API.setFollowerCommentaryEnabled(false);
    }
  }, [handleDisableAll, followerCommentaryEnabled]);

  const handleImportScenes = useCallback(() => {
    SKSE_API.importScenes();
    log('[Settings] Import scenes requested');
  }, []);

  const handleImportYAML = useCallback(() => {
    SKSE_API.importYAML();
    log('[Settings] Import YAML requested');
  }, []);

  return (
    <div className="flex flex-col h-full">
      {/* Header */}
      <div className="p-4 pb-0">
        <h2 className="text-2xl font-bold text-purple-400 mb-1">STFU Settings</h2>
        <p className="text-sm text-gray-400">Changes take effect immediately</p>
      </div>

      {/* Category Tabs */}
      <div className="flex gap-2 px-4 pt-4 border-b border-gray-700">
        <button
          onClick={() => setActiveCategory('master')}
          className={`flex items-center gap-2 px-4 py-2 rounded-t-lg transition-colors border-b-2 ${
            activeCategory === 'master'
              ? 'bg-gray-700 text-purple-400 border-purple-400'
              : 'bg-transparent text-gray-400 hover:text-gray-300 border-transparent'
          }`}
        >
          <SettingsIcon size={16} />
          Master Controls
        </button>
        <button
          onClick={() => setActiveCategory('combat')}
          className={`flex items-center gap-2 px-4 py-2 rounded-t-lg transition-colors border-b-2 ${
            activeCategory === 'combat'
              ? 'bg-gray-700 text-red-400 border-red-400'
              : 'bg-transparent text-gray-400 hover:text-gray-300 border-transparent'
          }`}
        >
          <Swords size={16} />
          Combat
        </button>
        <button
          onClick={() => setActiveCategory('generic')}
          className={`flex items-center gap-2 px-4 py-2 rounded-t-lg transition-colors border-b-2 ${
            activeCategory === 'generic'
              ? 'bg-gray-700 text-blue-400 border-blue-400'
              : 'bg-transparent text-gray-400 hover:text-gray-300 border-transparent'
          }`}
        >
          <MessageSquare size={16} />
          Generic
        </button>
        <button
          onClick={() => setActiveCategory('follower')}
          className={`flex items-center gap-2 px-4 py-2 rounded-t-lg transition-colors border-b-2 ${
            activeCategory === 'follower'
              ? 'bg-gray-700 text-green-400 border-green-400'
              : 'bg-transparent text-gray-400 hover:text-gray-300 border-transparent'
          }`}
        >
          <Users size={16} />
          Follower
        </button>
        <button
          onClick={() => setActiveCategory('other')}
          className={`flex items-center gap-2 px-4 py-2 rounded-t-lg transition-colors border-b-2 ${
            activeCategory === 'other'
              ? 'bg-gray-700 text-yellow-400 border-yellow-400'
              : 'bg-transparent text-gray-400 hover:text-gray-300 border-transparent'
          }`}
        >
          <Sparkles size={16} />
          Other
        </button>
      </div>

      {/* Tab Content */}
      <div className="flex-1 overflow-y-auto p-4">
        {/* Master Controls Tab */}
        {activeCategory === 'master' && (
          <div className="space-y-4">
            <div className="bg-gray-800 rounded-lg p-4">
              <h3 className="text-lg font-semibold text-purple-400 mb-4">Global Filters</h3>
              <div className="grid grid-cols-2 gap-x-8 gap-y-3">
                <Toggle
                  label="Enable Blacklist Filter"
                  checked={blacklistEnabled}
                  onChange={(checked) => SKSE_API.setBlacklistEnabled(checked)}
                  tooltip="Controls dialogue marked with 'Blacklist' category only"
                />
                <Toggle
                  label="Enable SkyrimNet Filter"
                  checked={skyrimNetEnabled}
                  onChange={(checked) => SKSE_API.setSkyrimNetEnabled(checked)}
                  tooltip="Block dialogue from being logged by SkyrimNet"
                />
                <Toggle
                  label="Block Ambient Scenes"
                  checked={scenesEnabled}
                  onChange={(checked) => SKSE_API.setScenesEnabled(checked)}
                  tooltip="Block hardcoded ambient scenes (e.g., inn banter)"
                />
                <Toggle
                  label="Block Bard Songs"
                  checked={bardSongsEnabled}
                  onChange={(checked) => SKSE_API.setBardSongsEnabled(checked)}
                  tooltip="Block bard performances at inns"
                />
              </div>
            </div>

            <div className="bg-gray-800 rounded-lg p-4">
              <h3 className="text-lg font-semibold text-purple-400 mb-4">Import & Restore</h3>
              <div className="grid grid-cols-2 gap-4">
                <button
                  onClick={handleImportScenes}
                  className="px-4 py-3 text-base bg-blue-600 hover:bg-blue-700 text-white rounded-lg flex items-center justify-center gap-2 transition-colors"
                  title="Restore default ambient scenes, bard songs, and follower commentary to blacklist"
                >
                  <Upload size={18} />
                  Import Scenes
                </button>
                <button
                  onClick={handleImportYAML}
                  className="px-4 py-3 text-base bg-purple-600 hover:bg-purple-700 text-white rounded-lg flex items-center justify-center gap-2 transition-colors"
                  title="Import from Blacklist, Whitelist, SubtypeOverrides, and SkyrimNetFilter YAMLs"
                >
                  <Upload size={18} />
                  Import from YAML
                </button>
              </div>
              <div className="text-sm text-gray-400 mt-3 space-y-1">
                <p><strong>Import Scenes:</strong> Restores default ambient scenes, bard songs, and follower commentary</p>
                <p><strong>Import YAML:</strong> Loads entries from blacklist/whitelist/overrides/filter YAML files</p>
              </div>
            </div>
          </div>
        )}

        {/* Combat Dialogue Tab */}
        {activeCategory === 'combat' && (
          <div className="bg-gray-800 rounded-lg p-4">
            <div className="flex justify-between items-center mb-4">
              <h3 className="text-lg font-semibold text-red-400">Combat Dialogue Filters</h3>
              <div className="flex gap-2">
                <button
                  onClick={handleEnableAllCombat}
                  className="px-3 py-1.5 text-sm bg-green-600 hover:bg-green-700 text-white rounded transition-colors"
                >
                  Enable All
                </button>
                <button
                  onClick={handleDisableAllCombat}
                  className="px-3 py-1.5 text-sm bg-gray-700 hover:bg-gray-600 text-white rounded transition-colors"
                >
                  Disable All
                </button>
              </div>
            </div>

            {/* Combat Grunts */}
            <div className="mb-4 pb-4 border-b border-gray-700">
              <SubcategoryHeader title="Combat Grunts" />
              <Toggle
                label="Block Combat Grunts"
                checked={combatGruntsBlocked}
                onChange={(checked) => SKSE_API.setCombatGruntsBlocked(checked)}
                tooltip="Combat grunts - e.g. 'Agh!', 'Oof!', 'Nargh!'"
              />
            </div>

            {/* Three-column layout */}
            <div className="grid grid-cols-3 gap-6">
              {/* Column 1 */}
              <div className="space-y-4">
                <div>
                  <SubcategoryHeader title="Attack Dialogue" />
                  <div className="space-y-2">
                    {COMBAT_SUBTYPES.attackDialogue.map(subtype => (
                      <Toggle
                        key={subtype.id}
                        label={subtype.name}
                        checked={subtypes[subtype.id] || false}
                        onChange={() => toggleSubtype(subtype.id)}
                        tooltip={subtype.tooltip}
                      />
                    ))}
                  </div>
                </div>

                <div>
                  <SubcategoryHeader title="Combat Reactions" />
                  <div className="space-y-2">
                    {COMBAT_SUBTYPES.combatReactions.map(subtype => (
                      <Toggle
                        key={subtype.id}
                        label={subtype.name}
                        checked={subtypes[subtype.id] || false}
                        onChange={() => toggleSubtype(subtype.id)}
                        tooltip={subtype.tooltip}
                      />
                    ))}
                  </div>
                </div>
              </div>

              {/* Column 2 */}
              <div className="space-y-4">
                <div>
                  <SubcategoryHeader title="Combat Commentary" />
                  <div className="space-y-2">
                    {COMBAT_SUBTYPES.combatCommentary.map(subtype => (
                      <Toggle
                        key={subtype.id}
                        label={subtype.name}
                        checked={subtypes[subtype.id] || false}
                        onChange={() => toggleSubtype(subtype.id)}
                        tooltip={subtype.tooltip}
                      />
                    ))}
                  </div>
                </div>

                <div>
                  <SubcategoryHeader title="Detection & Alert" />
                  <div className="space-y-2">
                    {COMBAT_SUBTYPES.detectionAlert.map(subtype => (
                      <Toggle
                        key={subtype.id}
                        label={subtype.name}
                        checked={subtypes[subtype.id] || false}
                        onChange={() => toggleSubtype(subtype.id)}
                        tooltip={subtype.tooltip}
                      />
                    ))}
                  </div>
                </div>
              </div>

              {/* Column 3 */}
              <div className="space-y-4">
                <div>
                  <SubcategoryHeader title="Lost/Search" />
                  <div className="space-y-2">
                    {COMBAT_SUBTYPES.lostSearch.map(subtype => (
                      <Toggle
                        key={subtype.id}
                        label={subtype.name}
                        checked={subtypes[subtype.id] || false}
                        onChange={() => toggleSubtype(subtype.id)}
                        tooltip={subtype.tooltip}
                      />
                    ))}
                  </div>
                </div>

                <div>
                  <SubcategoryHeader title="Transitions" />
                  <div className="space-y-2">
                    {COMBAT_SUBTYPES.transitions.map(subtype => (
                      <Toggle
                        key={subtype.id}
                        label={subtype.name}
                        checked={subtypes[subtype.id] || false}
                        onChange={() => toggleSubtype(subtype.id)}
                        tooltip={subtype.tooltip}
                      />
                    ))}
                  </div>
                </div>
              </div>
            </div>
          </div>
        )}

        {/* Generic Dialogue Tab */}
        {activeCategory === 'generic' && (
          <div className="bg-gray-800 rounded-lg p-4">
            <div className="flex justify-between items-center mb-4">
              <h3 className="text-lg font-semibold text-blue-400">Generic Dialogue Filters</h3>
              <div className="flex gap-2">
                <button
                  onClick={() => handleEnableAll(Object.values(GENERIC_SUBTYPES).flat())}
                  className="px-3 py-1.5 text-sm bg-green-600 hover:bg-green-700 text-white rounded transition-colors"
                >
                  Enable All
                </button>
                <button
                  onClick={() => handleDisableAll(Object.values(GENERIC_SUBTYPES).flat())}
                  className="px-3 py-1.5 text-sm bg-gray-700 hover:bg-gray-600 text-white rounded transition-colors"
                >
                  Disable All
                </button>
              </div>
            </div>

            {/* Three-column layout */}
            <div className="grid grid-cols-3 gap-6">
              {/* Column 1 */}
              <div className="space-y-4">
                <div>
                  <SubcategoryHeader title="Behaviors" />
                  <div className="space-y-2">
                    {GENERIC_SUBTYPES.behaviors.map(subtype => (
                      <Toggle
                        key={subtype.id}
                        label={subtype.name}
                        checked={subtypes[subtype.id] || false}
                        onChange={() => toggleSubtype(subtype.id)}
                        tooltip={subtype.tooltip}
                      />
                    ))}
                  </div>
                </div>

                <div>
                  <SubcategoryHeader title="Object Interactions" />
                  <div className="space-y-2">
                    {GENERIC_SUBTYPES.objectInteractions.map(subtype => (
                      <Toggle
                        key={subtype.id}
                        label={subtype.name}
                        checked={subtypes[subtype.id] || false}
                        onChange={() => toggleSubtype(subtype.id)}
                        tooltip={subtype.tooltip}
                      />
                    ))}
                  </div>
                </div>
              </div>

              {/* Column 2 */}
              <div className="space-y-4">
                <div>
                  <SubcategoryHeader title="Player Actions" />
                  <div className="space-y-2">
                    {GENERIC_SUBTYPES.playerActions.map(subtype => (
                      <Toggle
                        key={subtype.id}
                        label={subtype.name}
                        checked={subtypes[subtype.id] || false}
                        onChange={() => toggleSubtype(subtype.id)}
                        tooltip={subtype.tooltip}
                      />
                    ))}
                  </div>
                </div>

                <div>
                  <SubcategoryHeader title="Social" />
                  <div className="space-y-2">
                    {GENERIC_SUBTYPES.social.map(subtype => (
                      <Toggle
                        key={subtype.id}
                        label={subtype.name}
                        checked={subtypes[subtype.id] || false}
                        onChange={() => toggleSubtype(subtype.id)}
                        tooltip={subtype.tooltip}
                      />
                    ))}
                  </div>
                </div>
              </div>

              {/* Column 3 - Crime & Stealth (full column) */}
              <div>
                <SubcategoryHeader title="Crime & Stealth" />
                <div className="space-y-2">
                  {GENERIC_SUBTYPES.crimeStealth.map(subtype => (
                    <Toggle
                      key={subtype.id}
                      label={subtype.name}
                      checked={subtypes[subtype.id] || false}
                      onChange={() => toggleSubtype(subtype.id)}
                      tooltip={subtype.tooltip}
                    />
                  ))}
                </div>
              </div>
            </div>
          </div>
        )}

        {/* Follower Dialogue Tab */}
        {activeCategory === 'follower' && (
          <div className="bg-gray-800 rounded-lg p-4">
            <div className="flex justify-between items-center mb-4">
              <h3 className="text-lg font-semibold text-green-400">Follower Dialogue Filters</h3>
              <div className="flex gap-2">
                <button
                  onClick={handleEnableAllFollower}
                  className="px-3 py-1.5 text-sm bg-green-600 hover:bg-green-700 text-white rounded transition-colors"
                >
                  Enable All
                </button>
                <button
                  onClick={handleDisableAllFollower}
                  className="px-3 py-1.5 text-sm bg-gray-700 hover:bg-gray-600 text-white rounded transition-colors"
                >
                  Disable All
                </button>
              </div>
            </div>

            <div className="grid grid-cols-2 gap-8">
              <div>
                <SubcategoryHeader title="Commands" />
                <div className="space-y-2">
                  {FOLLOWER_SUBTYPES.commands.map(subtype => (
                    <Toggle
                      key={subtype.id}
                      label={subtype.name}
                      checked={subtypes[subtype.id] || false}
                      onChange={() => toggleSubtype(subtype.id)}
                      tooltip={subtype.tooltip}
                    />
                  ))}
                </div>
              </div>

              <div>
                <SubcategoryHeader title="Follower Commentary" />
                <div className="space-y-2">
                  <Toggle
                    label="Block Follower Commentary"
                    checked={followerCommentaryEnabled}
                    onChange={(checked) => SKSE_API.setFollowerCommentaryEnabled(checked)}
                    tooltip="Follower commentary scenes - triggered when entering dungeons or areas"
                  />
                </div>
              </div>
            </div>
          </div>
        )}

        {/* Other Dialogue Tab */}
        {activeCategory === 'other' && (
          <div className="bg-gray-800 rounded-lg p-4">
            <div className="flex justify-between items-center mb-4">
              <h3 className="text-lg font-semibold text-yellow-400">Other Dialogue Filters</h3>
              <div className="flex gap-2">
                <button
                  onClick={() => handleEnableAll(Object.values(OTHER_SUBTYPES).flat())}
                  className="px-3 py-1.5 text-sm bg-green-600 hover:bg-green-700 text-white rounded transition-colors"
                >
                  Enable All
                </button>
                <button
                  onClick={() => handleDisableAll(Object.values(OTHER_SUBTYPES).flat())}
                  className="px-3 py-1.5 text-sm bg-gray-700 hover:bg-gray-600 text-white rounded transition-colors"
                >
                  Disable All
                </button>
              </div>
            </div>

            <SubcategoryHeader title="Voice Powers" />
            <div className="grid grid-cols-2 gap-x-8 gap-y-2">
              {OTHER_SUBTYPES.voicePowers.map(subtype => (
                <Toggle
                  key={subtype.id}
                  label={subtype.name}
                  checked={subtypes[subtype.id] || false}
                  onChange={() => toggleSubtype(subtype.id)}
                  tooltip={subtype.tooltip}
                />
              ))}
            </div>
          </div>
        )}
      </div>
    </div>
  );
};
