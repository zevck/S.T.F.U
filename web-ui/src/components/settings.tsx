import { useState, useCallback, ReactNode } from 'react';
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

type Subtype = { id: number; name: string; tooltip?: string };

const TABS: { id: CategoryTab; label: string; icon: typeof SettingsIcon; activeClass: string }[] = [
  { id: 'master', label: 'Master Controls', icon: SettingsIcon, activeClass: 'text-purple-400 border-purple-400' },
  { id: 'combat', label: 'Combat', icon: Swords, activeClass: 'text-red-400 border-red-400' },
  { id: 'generic', label: 'Generic', icon: MessageSquare, activeClass: 'text-blue-400 border-blue-400' },
  { id: 'follower', label: 'Follower', icon: Users, activeClass: 'text-green-400 border-green-400' },
  { id: 'other', label: 'Other', icon: Sparkles, activeClass: 'text-yellow-400 border-yellow-400' },
];

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

// A tab's panel: colored title with Enable All / Disable All buttons
interface CategoryPanelProps {
  title: string;
  titleClass: string;
  onEnableAll: () => void;
  onDisableAll: () => void;
  children: ReactNode;
}

const CategoryPanel = ({ title, titleClass, onEnableAll, onDisableAll, children }: CategoryPanelProps) => (
  <div className="bg-gray-800 rounded-lg p-4">
    <div className="flex justify-between items-center mb-4">
      <h3 className={`text-lg font-semibold ${titleClass}`}>{title}</h3>
      <div className="flex gap-2">
        <button
          onClick={onEnableAll}
          className="px-3 py-1.5 text-sm bg-green-600 hover:bg-green-700 text-white rounded transition-colors"
        >
          Enable All
        </button>
        <button
          onClick={onDisableAll}
          className="px-3 py-1.5 text-sm bg-gray-700 hover:bg-gray-600 text-white rounded transition-colors"
        >
          Disable All
        </button>
      </div>
    </div>
    {children}
  </div>
);

export const Settings = () => {
  // Category navigation state
  const [activeCategory, setActiveCategory] = useState<CategoryTab>('master');

  // Read settings from global store (updated by C++ in real-time)
  const blacklistEnabled = useSettingsStore(state => state.blacklistEnabled);
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

  // Toggles every subtype in the list whose current state differs from `enabled`
  const setAll = useCallback((categorySubtypes: Subtype[], enabled: boolean) => {
    categorySubtypes.forEach(subtype => {
      if (!!subtypes[subtype.id] !== enabled) {
        SKSE_API.toggleSubtypeFilter(subtype.id);
      }
    });
    log(`[Settings] Requested ${enabled ? 'enable' : 'disable'} all for category`);
  }, [subtypes]);

  const setAllCombat = useCallback((enabled: boolean) => {
    setAll(Object.values(COMBAT_SUBTYPES).flat(), enabled);
    if (combatGruntsBlocked !== enabled) {
      SKSE_API.setCombatGruntsBlocked(enabled);
    }
  }, [setAll, combatGruntsBlocked]);

  const setAllFollower = useCallback((enabled: boolean) => {
    setAll(Object.values(FOLLOWER_SUBTYPES).flat(), enabled);
    if (followerCommentaryEnabled !== enabled) {
      SKSE_API.setFollowerCommentaryEnabled(enabled);
    }
  }, [setAll, followerCommentaryEnabled]);

  const handleImportScenes = useCallback(() => {
    SKSE_API.importScenes();
    log('[Settings] Import scenes requested');
  }, []);

  const handleImportYAML = useCallback(() => {
    SKSE_API.importYAML();
    log('[Settings] Import YAML requested');
  }, []);

  // A titled group of subtype toggles
  const subtypeGroup = (title: string, list: Subtype[]) => (
    <div>
      <SubcategoryHeader title={title} />
      <div className="space-y-2">
        {list.map(subtype => (
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
  );

  return (
    <div className="flex flex-col h-full">
      {/* Header */}
      <div className="p-4 pb-0">
        <h2 className="text-2xl font-bold text-purple-400 mb-1">STFU Settings</h2>
        <p className="text-sm text-gray-400">Changes take effect immediately</p>
      </div>

      {/* Category Tabs */}
      <div className="flex gap-2 px-4 pt-4 border-b border-gray-700">
        {TABS.map(({ id, label, icon: Icon, activeClass }) => (
          <button
            key={id}
            onClick={() => setActiveCategory(id)}
            className={`flex items-center gap-2 px-4 py-2 rounded-t-lg transition-colors border-b-2 ${
              activeCategory === id
                ? `bg-gray-700 ${activeClass}`
                : 'bg-transparent text-gray-400 hover:text-gray-300 border-transparent'
            }`}
          >
            <Icon size={16} />
            {label}
          </button>
        ))}
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
                  title="Import from Blacklist, Whitelist, and SubtypeOverrides YAMLs"
                >
                  <Upload size={18} />
                  Import from YAML
                </button>
              </div>
              <div className="text-sm text-gray-400 mt-3 space-y-1">
                <p><strong>Import Scenes:</strong> Restores default ambient scenes, bard songs, and follower commentary</p>
                <p><strong>Import YAML:</strong> Loads entries from blacklist/whitelist/overrides YAML files</p>
              </div>
            </div>
          </div>
        )}

        {/* Combat Dialogue Tab */}
        {activeCategory === 'combat' && (
          <CategoryPanel
            title="Combat Dialogue Filters"
            titleClass="text-red-400"
            onEnableAll={() => setAllCombat(true)}
            onDisableAll={() => setAllCombat(false)}
          >
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
              <div className="space-y-4">
                {subtypeGroup('Attack Dialogue', COMBAT_SUBTYPES.attackDialogue)}
                {subtypeGroup('Combat Reactions', COMBAT_SUBTYPES.combatReactions)}
              </div>
              <div className="space-y-4">
                {subtypeGroup('Combat Commentary', COMBAT_SUBTYPES.combatCommentary)}
                {subtypeGroup('Detection & Alert', COMBAT_SUBTYPES.detectionAlert)}
              </div>
              <div className="space-y-4">
                {subtypeGroup('Lost/Search', COMBAT_SUBTYPES.lostSearch)}
                {subtypeGroup('Transitions', COMBAT_SUBTYPES.transitions)}
              </div>
            </div>
          </CategoryPanel>
        )}

        {/* Generic Dialogue Tab */}
        {activeCategory === 'generic' && (
          <CategoryPanel
            title="Generic Dialogue Filters"
            titleClass="text-blue-400"
            onEnableAll={() => setAll(Object.values(GENERIC_SUBTYPES).flat(), true)}
            onDisableAll={() => setAll(Object.values(GENERIC_SUBTYPES).flat(), false)}
          >
            {/* Three-column layout */}
            <div className="grid grid-cols-3 gap-6">
              <div className="space-y-4">
                {subtypeGroup('Behaviors', GENERIC_SUBTYPES.behaviors)}
                {subtypeGroup('Object Interactions', GENERIC_SUBTYPES.objectInteractions)}
              </div>
              <div className="space-y-4">
                {subtypeGroup('Player Actions', GENERIC_SUBTYPES.playerActions)}
                {subtypeGroup('Social', GENERIC_SUBTYPES.social)}
              </div>
              {/* Column 3 - Crime & Stealth (full column) */}
              {subtypeGroup('Crime & Stealth', GENERIC_SUBTYPES.crimeStealth)}
            </div>
          </CategoryPanel>
        )}

        {/* Follower Dialogue Tab */}
        {activeCategory === 'follower' && (
          <CategoryPanel
            title="Follower Dialogue Filters"
            titleClass="text-green-400"
            onEnableAll={() => setAllFollower(true)}
            onDisableAll={() => setAllFollower(false)}
          >
            <div className="grid grid-cols-2 gap-8">
              {subtypeGroup('Commands', FOLLOWER_SUBTYPES.commands)}

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
          </CategoryPanel>
        )}

        {/* Other Dialogue Tab */}
        {activeCategory === 'other' && (
          <CategoryPanel
            title="Other Dialogue Filters"
            titleClass="text-yellow-400"
            onEnableAll={() => setAll(Object.values(OTHER_SUBTYPES).flat(), true)}
            onDisableAll={() => setAll(Object.values(OTHER_SUBTYPES).flat(), false)}
          >
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
          </CategoryPanel>
        )}
      </div>
    </div>
  );
};
