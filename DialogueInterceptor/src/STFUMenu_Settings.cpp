// Settings tab implementation for STFUMenu
// Separated into its own file for organization

#include "STFUMenu.h"
#include "Config.h"
#include "SettingsPersistence.h"
#include <unordered_map>
#include <vector>
#include <algorithm>

// Helper to render a toggle for a TESGlobal (1.0 = enabled, 0.0 = disabled)
static bool RenderGlobalToggle(const char* label, RE::TESGlobal* global, bool invertLogic = false)
{
    if (!global) {
        ImGuiMCP::TextDisabled("%s (not loaded)", label);
        return false;
    }
    
    bool enabled = invertLogic ? (global->value < 0.5f) : (global->value >= 0.5f);
    if (ImGuiMCP::Checkbox(label, &enabled)) {
        global->value = invertLogic ? (enabled ? 0.0f : 1.0f) : (enabled ? 1.0f : 0.0f);
        // Auto-save settings to database when changed
        SettingsPersistence::SaveSettings();
        return true;
    }
    return false;
}

// Helper to render Enable/Disable All buttons for a category
static void RenderCategoryButtons(const char* categoryID, const std::vector<RE::TESGlobal*>& globals, RE::TESGlobal* preserveGruntsGlobal = nullptr)
{
    using namespace ImGuiMCP;
    
    PushID(categoryID);  // Ensure unique button IDs per category
    
    float buttonWidth = 115.0f * STFUMenu::GetUIScale();
    
    if (Button("Enable All", ImVec2(buttonWidth, 0))) {
        for (auto* global : globals) {
            if (global) global->value = 1.0f;
        }
        // Handle Combat Grunts with inverted logic (Enable All = block grunts)
        if (preserveGruntsGlobal) {
            preserveGruntsGlobal->value = 0.0f;
        }
        SettingsPersistence::SaveSettings();
    }
    
    SameLine();
    
    if (Button("Disable All", ImVec2(buttonWidth, 0))) {
        for (auto* global : globals) {
            if (global) global->value = 0.0f;
        }
        // Handle Combat Grunts with inverted logic (Disable All = allow grunts)
        if (preserveGruntsGlobal) {
            preserveGruntsGlobal->value = 1.0f;
        }
        SettingsPersistence::SaveSettings();
    }
    
    PopID();
}

void STFUMenu::RenderSettingsTab()
{
    using namespace ImGuiMCP;
    
    const auto& settings = Config::GetSettings();
    const float columnWidth = 285.0f * GetUIScale();  // Scale column widths (1080p=0.9, 1440p=1.0)
    
    TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "STFU Settings");
    Text("Changes take effect immediately.");
    Separator();
    Spacing();
    
    // Master Controls Section
    if (CollapsingHeader("Master Controls", ImGuiTreeNodeFlags_DefaultOpen)) {
        Indent();
        
        // Two-column layout: toggles on left, hotkey/import on right
        const float rightColumnX = 350.0f * GetUIScale();
        const float rightButtonWidth = 200.0f * GetUIScale();
        
        // Left column: Master toggles
        BeginGroup();
        RenderGlobalToggle("Enable Blacklist Filter", settings.blacklist.toggleGlobal);
        if (IsItemHovered()) {
            BeginTooltip();
            Text("Controls dialogue marked with 'Blacklist' category only");
            Text("Other categories (Scenes, Subtypes, etc.) have separate toggles");
            EndTooltip();
        }
        
        if (isSkyrimNetLoaded_) {
            RenderGlobalToggle("Enable SkyrimNet Filter", settings.skyrimNetFilter.toggleGlobal);
            if (IsItemHovered()) {
                BeginTooltip();
                Text("Block dialogue from being logged by SkyrimNet");
                EndTooltip();
            }
        }
        
        RenderGlobalToggle("Block Ambient Scenes", settings.mcm.blockScenesGlobal);
        if (IsItemHovered()) {
            BeginTooltip();
            Text("Block hardcoded ambient scenes (e.g., inn banter)");
            EndTooltip();
        }
        
        RenderGlobalToggle("Block Bard Songs", settings.mcm.blockBardSongsGlobal);
        if (IsItemHovered()) {
            BeginTooltip();
            Text("Block bard performances at inns");
            EndTooltip();
        }
        EndGroup();
        
        // Right column: Hotkey and Import buttons
        SameLine(rightColumnX);
        BeginGroup();
        
        // Menu Hotkey Customization
        // Center the "Hotkey:" text
        ImVec2 textSize;
        CalcTextSize(&textSize, "Hotkey:", nullptr, false, -1.0f);
        float centerX = (rightButtonWidth - textSize.x) / 2.0f;
        SetCursorPosX(GetCursorPosX() + centerX);
        Text("Hotkey:");
        
        const char* currentKeyName = GetKeyName(settings.menuHotkey);
        
        if (waitingForHotkeyInput_) {
            PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.5f, 0.0f, 1.0f));  // Orange when waiting
            PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.6f, 0.1f, 1.0f));
            PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.7f, 0.4f, 0.0f, 1.0f));
            if (Button("Press any key...", ImVec2(rightButtonWidth, 0))) {
                // Button is just for display, actual capture happens in CaptureHotkey
            }
            PopStyleColor(3);
            if (IsItemHovered()) {
                BeginTooltip();
                Text("Press Escape to cancel. Invalid keys (mouse, Enter, Ctrl, Shift) are ignored.");
                EndTooltip();
            }
        } else {
            char buttonLabel[128];
            snprintf(buttonLabel, sizeof(buttonLabel), "%s", currentKeyName);
            if (Button(buttonLabel, ImVec2(rightButtonWidth, 0))) {
                StartHotkeyCapture();
                spdlog::info("[STFU Menu] Started hotkey capture mode");
            }
            if (IsItemHovered()) {
                BeginTooltip();
                Text("Click to change the menu hotkey");
                EndTooltip();
            }
        }
        

        
        // Import Hardcoded Scenes Button (with cooldown)
        auto now = std::chrono::steady_clock::now();
        auto timeSinceLastImportScenes = std::chrono::duration_cast<std::chrono::seconds>(now - lastImportScenesTime_).count();
        bool canImportScenes = timeSinceLastImportScenes >= 5;  // 5 second cooldown
        
        if (!canImportScenes) BeginDisabled();
        if (Button("Import Scenes", ImVec2(rightButtonWidth, 0))) {
            ReimportHardcodedScenes();
            lastImportScenesTime_ = std::chrono::steady_clock::now();
        }
        if (!canImportScenes) EndDisabled();
        
        if (IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            BeginTooltip();
            Text("Restore default ambient scenes, bard songs, and follower commentary to blacklist");
            if (!canImportScenes) {
                int remaining = 5 - static_cast<int>(timeSinceLastImportScenes);
                Text("Cooldown: %d seconds remaining", remaining);
            }
            EndTooltip();
        }
        
        Spacing();
        
        // Import YAML Button (with cooldown)
        auto timeSinceLastImportYAML = std::chrono::duration_cast<std::chrono::seconds>(now - lastImportYAMLTime_).count();
        bool canImportYAML = timeSinceLastImportYAML >= 5;  // 5 second cooldown
        
        if (!canImportYAML) BeginDisabled();
        if (Button("Import from YAML", ImVec2(rightButtonWidth, 0))) {
            Config::ImportYAMLToDatabase();
            lastImportYAMLTime_ = std::chrono::steady_clock::now();
        }
        if (!canImportYAML) EndDisabled();
        
        if (IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            BeginTooltip();
            if (isSkyrimNetLoaded_) {
                Text("Import from Blacklist, Whitelist, SubtypeOverrides, and SkyrimNetFilter YAMLs");
            } else {
                Text("Import from Blacklist, Whitelist, and SubtypeOverrides YAMLs");
            }
            Text("Blacklist/Whitelist: topics, scenes, quests; Whitelist: plugins");
            Text("Subtype Overrides: Blacklist topics with specific filter categories");
            if (isSkyrimNetLoaded_) {
                Text("SkyrimNet Filter: Block topics from SkyrimNet logging. Only supports menu topics");
            }
            if (!canImportYAML) {
                int remaining = 5 - static_cast<int>(timeSinceLastImportYAML);
                Text("Cooldown: %d seconds remaining", remaining);
            }
            EndTooltip();
        }
        
        EndGroup();
        
        Unindent();
        Spacing();
    }
    
    // Subtype Filters Section - Organized by Category
    if (CollapsingHeader("Dialogue Subtype Filters", ImGuiTreeNodeFlags_DefaultOpen)) {
        Indent();
        
        Text("Enable/disable specific dialogue subtypes:");
        Spacing();
        
        // Helper lambda to collect globals from subtype lists
        auto collectGlobals = [&](std::vector<RE::TESGlobal*>& globalsList, const std::vector<std::pair<uint16_t, const char*>>& subtypes) {
            for (const auto& [subtype, name] : subtypes) {
                auto it = settings.mcm.subtypeGlobals.find(subtype);
                if (it != settings.mcm.subtypeGlobals.end()) {
                    globalsList.push_back(it->second);
                }
            }
        };
        
        // Helper to add section headers
        auto AddHeaderOption = [](const char* header) {
            ImGuiMCP::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "%s", header);
            ImGuiMCP::Separator();
        };
        
        // Tooltip map for all dialogue subtypes
        static std::unordered_map<uint16_t, const char*> tooltips = {
            // Combat subtypes
            {26, "Attack dialogue - e.g. 'I've had enough of you!', 'Die, beast!', 'You can't win this!'"},
            {27, "Power attack dialogue - Only grunts by default, mods might add dialogue"},
            {28, "Bash dialogue - e.g. 'Hunh!', 'Gah!', 'Yah!', 'Rargh!' (vanilla is only grunts, mods may add dialogue)"},
            {29, "Hit dialogue - e.g. 'Do your worst!', 'That your best? Huh?', 'Damn you!'"},
            {30, "Flee dialogue - e.g. 'Ah! It burns!', 'You win! I submit!', 'Aaaiiee!'"},
            {31, "Bleedout dialogue - e.g. 'Help me...', 'I can't move...', 'Someone...'"},
            {32, "Avoid threat dialogue - e.g. 'I need to get away!', 'Too dangerous!', 'Back off!'"},
            {33, "Death dialogue - e.g. 'Thank you.', 'Free...again.', 'At last.'"},
            {35, "Block dialogue - e.g. 'Close...', 'Easily blocked!', 'Oho! Not so fast!'"},
            {36, "Taunt dialogue - e.g. 'Skyrim belongs to the Nords!', 'I'll rip your heart out!', 'Prepare to die!'"},
            {37, "Ally killed dialogue - e.g. 'No!! How dare you!', 'Why, why?!', 'Gods, no!'"},
            {39, "Yield dialogue - e.g. 'I yield! I yield!', 'Please, mercy!', 'I give up!'"},
            {40, "Accept yield dialogue - e.g. 'I'll let you live. This time.', 'You're not worth it.', 'All right, you've had enough.'"},
            {41, "Pickpocket combat - NPCs reacting to pickpocket attempts during combat"},
            {55, "Alert idle dialogue - e.g. 'Anybody there?', 'Hello? Who's there?', 'I know I heard something.'"},
            {56, "Lost idle dialogue - e.g. 'I'm going to find you.', 'You afraid to fight me?'"},
            {57, "Normal to alert transition - e.g. 'Did you hear something?', 'Hello? Who's there?', 'Is... is someone there?'"},
            {58, "Alert to combat transition - e.g. 'Now you're mine!', 'Time to end this little game!', 'I knew it!'"},
            {59, "Normal to combat transition - e.g. 'You're dead!', 'By Ysmir, you won't leave here alive!'"},
            {60, "Alert to normal transition - e.g. 'Must have been nothing.', 'Mind's playing tricks on me...'"},
            {61, "Combat to normal transition - e.g. 'That takes care of that.', 'All over now.'"},
            {62, "Combat to lost transition - e.g. 'Is it safe?', 'Where are you hiding?'"},
            {63, "Lost to normal transition - e.g. 'Oh well. Must have run off.', 'Nobody here anymore.'"},
            {64, "Lost to combat transition - e.g. 'There you are.', 'I knew I'd find you!'"},
            {65, "Detect friend die - NPCs reacting when they detect a friend has died"},
            {75, "Observe combat dialogue - e.g. 'Gods! Another fight.', 'Somebody help!', 'I'm getting out of here!'"},
            // Generic subtypes
            {38, "Steal dialogue - NPCs reacting to you stealing items"},
            {42, "Assault dialogue - Civilian NPCs reacting to witnessing assault"},
            {43, "Murder dialogue - Civilian NPCs reacting to witnessing murder"},
            {44, "Assault non-crime - Assault reactions outside crime system"},
            {45, "Murder non-crime - Murder reactions outside crime system"},
            {46, "Pickpocket non-crime - Pickpocket comments outside crime system"},
            {47, "Steal from non-crime - NPCs commenting on theft outside crime system"},
            {48, "Trespass against non-crime - Trespass warnings outside crime system"},
            {49, "Trespass dialogue - NPCs warning you about entering restricted areas"},
            {50, "Werewolf transform crime - NPCs reacting to werewolf transformations"},
            {70, "Barter exit - Merchants' closing remarks when ending trade"},
            {74, "Training exit - Trainer dialogue when exiting training menu"},
            {76, "Notice corpse - NPCs reacting to seeing dead bodies"},
            {77, "Time to go - NPCs telling you to leave or wrapping up"},
            {78, "Goodbye dialogue - Farewells when ending conversations"},
            {79, "Hello dialogue - Greetings from NPCs"},
            {80, "Swing melee weapon - NPCs reacting to you swinging weapons near them"},
            {81, "Shoot bow - NPCs reacting to you shooting arrows near them"},
            {82, "ZKeyObject - NPCs commenting when you pick up objects through the physics system"},
            {84, "Knock over object - NPCs commenting when you knock things over"},
            {85, "Destroy object - NPCs reacting to you destroying objects"},
            {86, "Stand on furniture - NPCs commenting when you stand on furniture"},
            {87, "Locked object - NPCs commenting when you try to open locked containers/doors"},
            {88, "Pickpocket topic - Dialogue about pickpocketing attempts"},
            {89, "Pursue idle topic - Guards pursuing criminals"},
            {91, "Cast projectile spell - NPCs reacting to you casting projectile spells near them"},
            {92, "Cast self spell - NPCs reacting to you casting spells on yourself"},
            {93, "Player shout - NPCs reacting to you using shouts"},
            {94, "Idle chatter - Random comments NPCs make to themselves"},
            {98, "Actor collide - NPCs reacting when you bump into them"},
            {99, "Player in iron sights - NPCs reacting to you aiming at them with ranged weapons"},
            // Follower subtypes
            {13, "Refuse - Follower refusing general requests"},
            {15, "Show - Follower ready for a command"},
            {16, "Agree - Follower agreeing to requests or commands"},
            {18, "Exit favor state - Follower dialogue when ending favor/command mode"},
            {19, "Moral refusal - Follower refuses requests that conflict with their morals"},
            // Other subtypes
            {51, "Voice power start short - Short shout starting dialogue"},
            {52, "Voice power end short - Short shout ending dialogue"},
            {53, "Voice power end long - Long shout ending dialogue"},
            {54, "Voice power start long - Long shout starting dialogue"}
        };
        
        // Helper to render toggle with tooltip
        auto RenderToggleWithTooltip = [&](const char* label, uint16_t subtype, const std::vector<std::pair<uint16_t, const char*>>& subtypes) {
            auto it = settings.mcm.subtypeGlobals.find(subtype);
            if (it != settings.mcm.subtypeGlobals.end()) {
                RenderGlobalToggle(label, it->second);
                if (ImGuiMCP::IsItemHovered()) {
                    auto tooltipIt = tooltips.find(subtype);
                    if (tooltipIt != tooltips.end()) {
                        ImGuiMCP::BeginTooltip();
                        ImGuiMCP::Text("%s", tooltipIt->second);
                        ImGuiMCP::EndTooltip();
                    }
                }
            }
        };
        
        // ==== COMBAT DIALOGUE ====
        std::vector<RE::TESGlobal*> combatGlobals;
        
        // Organize by subcategory
        std::vector<std::pair<uint16_t, const char*>> attackDialogue = {
            {40, "Accept Yield"}, {26, "Attack"}, {28, "Bash"}, {35, "Block"}, {29, "Hit"}, {27, "Power Attack"}
        };
        std::vector<std::pair<uint16_t, const char*>> combatCommentary = {
            {37, "Ally Killed"}, {65, "Detect Friend Die"}, {75, "Observe Combat"}, {36, "Taunt"}
        };
        std::vector<std::pair<uint16_t, const char*>> combatReactions = {
            {32, "Avoid Threat"}, {30, "Flee"}, {41, "Pickpocket (Combat)"}, {39, "Yield"}
        };
        std::vector<std::pair<uint16_t, const char*>> detectionAlert = {
            {55, "Alert Idle"}, {58, "Alert to Combat"}, {60, "Alert to Normal"},
            {57, "Normal to Alert"}, {59, "Normal to Combat"}
        };
        std::vector<std::pair<uint16_t, const char*>> lostSearch = {
            {56, "Lost Idle"}, {64, "Lost to Combat"}, {63, "Lost to Normal"}
        };
        std::vector<std::pair<uint16_t, const char*>> transitions = {
            {31, "Bleedout"}, {62, "Combat to Lost"}, {61, "Combat to Normal"}, {33, "Death"}
        };
        
        // Collect all combat globals
        collectGlobals(combatGlobals, attackDialogue);
        collectGlobals(combatGlobals, combatCommentary);
        collectGlobals(combatGlobals, combatReactions);
        collectGlobals(combatGlobals, detectionAlert);
        collectGlobals(combatGlobals, lostSearch);
        collectGlobals(combatGlobals, transitions);
        
        if (CollapsingHeader("Combat Dialogue", ImGuiTreeNodeFlags_None)) {
            Indent();
            
            RenderCategoryButtons("Combat", combatGlobals, settings.mcm.preserveGruntsGlobal);
            Spacing();
            
            // Combat Grunts (at top)
            AddHeaderOption("Combat Grunts");
            RenderGlobalToggle("Block Combat Grunts", settings.mcm.preserveGruntsGlobal, true);
            if (IsItemHovered()) {
                BeginTooltip();
                Text("Combat grunts - e.g. 'Agh!', 'Oof!', 'Nargh!', 'Argh!', 'Yeagh!'");
                EndTooltip();
            }
            Spacing();
            
            // Helper to render a subcategory header spanning 2 columns
            auto RenderSubcategoryHeader = [](const char* title, int startCol) {
                TableNextRow();
                TableSetColumnIndex(startCol);
                PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 1.0f, 1.0f));
                Text("%s", title);
                PopStyleColor();
            };
            
            // Helper to render toggles in a subcategory (fills 2 columns starting at startCol)
            auto RenderSubcategoryToggles = [&](const std::vector<std::pair<uint16_t, const char*>>& items, int startCol) {
                int index = 0;
                for (const auto& [subtype, name] : items) {
                    if (index % 2 == 0) {
                        TableNextRow();
                        TableSetColumnIndex(startCol);
                    } else {
                        TableSetColumnIndex(startCol + 1);
                    }
                    RenderToggleWithTooltip(name, subtype, items);
                    index++;
                }
            };
            
            // 4-column layout: 2 subcategories side-by-side
            if (BeginTable("CombatToggles", 4, ImGuiTableFlags_None)) {
                TableSetupColumn("Col1", ImGuiTableColumnFlags_WidthFixed, columnWidth);
                TableSetupColumn("Col2", ImGuiTableColumnFlags_WidthFixed, columnWidth);
                TableSetupColumn("Col3", ImGuiTableColumnFlags_WidthFixed, columnWidth);
                TableSetupColumn("Col4", ImGuiTableColumnFlags_WidthFixed, columnWidth);
                
                // Row 1: Attack Dialogue | Combat Commentary
                RenderSubcategoryHeader("Attack Dialogue", 0);
                TableSetColumnIndex(2);
                PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 1.0f, 1.0f));
                Text("Combat Commentary");
                PopStyleColor();
                
                // Render both subcategories in parallel
                int attackIndex = 0, commentaryIndex = 0;
                int maxRows = attackDialogue.size() > combatCommentary.size() ? (int)attackDialogue.size() : (int)combatCommentary.size();
                maxRows = (maxRows + 1) / 2;  // Convert to row count (2 items per row)
                
                for (int row = 0; row < maxRows; row++) {
                    TableNextRow();
                    
                    // Attack Dialogue (left side, cols 0-1)
                    if (attackIndex < attackDialogue.size()) {
                        TableSetColumnIndex(0);
                        RenderToggleWithTooltip(attackDialogue[attackIndex].second, 
                            attackDialogue[attackIndex].first, attackDialogue);
                        attackIndex++;
                    }
                    if (attackIndex < attackDialogue.size()) {
                        TableSetColumnIndex(1);
                        RenderToggleWithTooltip(attackDialogue[attackIndex].second, 
                            attackDialogue[attackIndex].first, attackDialogue);
                        attackIndex++;
                    }
                    
                    // Combat Commentary (right side, cols 2-3)
                    if (commentaryIndex < combatCommentary.size()) {
                        TableSetColumnIndex(2);
                        RenderToggleWithTooltip(combatCommentary[commentaryIndex].second, 
                            combatCommentary[commentaryIndex].first, combatCommentary);
                        commentaryIndex++;
                    }
                    if (commentaryIndex < combatCommentary.size()) {
                        TableSetColumnIndex(3);
                        RenderToggleWithTooltip(combatCommentary[commentaryIndex].second, 
                            combatCommentary[commentaryIndex].first, combatCommentary);
                        commentaryIndex++;
                    }
                }
                
                // Spacing row
                TableNextRow();
                
                // Row 2: Combat Reactions | Detection & Alert
                RenderSubcategoryHeader("Combat Reactions", 0);
                TableSetColumnIndex(2);
                PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 1.0f, 1.0f));
                Text("Detection & Alert");
                PopStyleColor();
                
                int reactionsIndex = 0, detectionIndex = 0;
                maxRows = combatReactions.size() > detectionAlert.size() ? (int)combatReactions.size() : (int)detectionAlert.size();
                maxRows = (maxRows + 1) / 2;
                
                for (int row = 0; row < maxRows; row++) {
                    TableNextRow();
                    
                    // Combat Reactions (left side, cols 0-1)
                    if (reactionsIndex < combatReactions.size()) {
                        TableSetColumnIndex(0);
                        RenderToggleWithTooltip(combatReactions[reactionsIndex].second, 
                            combatReactions[reactionsIndex].first, combatReactions);
                        reactionsIndex++;
                    }
                    if (reactionsIndex < combatReactions.size()) {
                        TableSetColumnIndex(1);
                        RenderToggleWithTooltip(combatReactions[reactionsIndex].second, 
                            combatReactions[reactionsIndex].first, combatReactions);
                        reactionsIndex++;
                    }
                    
                    // Detection & Alert (right side, cols 2-3)
                    if (detectionIndex < detectionAlert.size()) {
                        TableSetColumnIndex(2);
                        RenderToggleWithTooltip(detectionAlert[detectionIndex].second, 
                            detectionAlert[detectionIndex].first, detectionAlert);
                        detectionIndex++;
                    }
                    if (detectionIndex < detectionAlert.size()) {
                        TableSetColumnIndex(3);
                        RenderToggleWithTooltip(detectionAlert[detectionIndex].second, 
                            detectionAlert[detectionIndex].first, detectionAlert);
                        detectionIndex++;
                    }
                }
                
                // Spacing row
                TableNextRow();
                
                // Row 3: Lost/Search | Transitions
                RenderSubcategoryHeader("Lost/Search", 0);
                TableSetColumnIndex(2);
                PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 1.0f, 1.0f));
                Text("Transitions");
                PopStyleColor();
                
                int lostIndex = 0, transitionsIndex = 0;
                maxRows = lostSearch.size() > transitions.size() ? (int)lostSearch.size() : (int)transitions.size();
                maxRows = (maxRows + 1) / 2;
                
                for (int row = 0; row < maxRows; row++) {
                    TableNextRow();
                    
                    // Lost/Search (left side, cols 0-1)
                    if (lostIndex < lostSearch.size()) {
                        TableSetColumnIndex(0);
                        RenderToggleWithTooltip(lostSearch[lostIndex].second, 
                            lostSearch[lostIndex].first, lostSearch);
                        lostIndex++;
                    }
                    if (lostIndex < lostSearch.size()) {
                        TableSetColumnIndex(1);
                        RenderToggleWithTooltip(lostSearch[lostIndex].second, 
                            lostSearch[lostIndex].first, lostSearch);
                        lostIndex++;
                    }
                    
                    // Transitions (right side, cols 2-3)
                    if (transitionsIndex < transitions.size()) {
                        TableSetColumnIndex(2);
                        RenderToggleWithTooltip(transitions[transitionsIndex].second, 
                            transitions[transitionsIndex].first, transitions);
                        transitionsIndex++;
                    }
                    if (transitionsIndex < transitions.size()) {
                        TableSetColumnIndex(3);
                        RenderToggleWithTooltip(transitions[transitionsIndex].second, 
                            transitions[transitionsIndex].first, transitions);
                        transitionsIndex++;
                    }
                }
                
                EndTable();
            }
            
            Unindent();
        }
        
        Spacing();
        
        // ==== GENERIC DIALOGUE ====
        std::vector<RE::TESGlobal*> genericGlobals;
        
        // Organize by subcategory (alphabetically)
        std::vector<std::pair<uint16_t, const char*>> behaviors = {
            {98, "Actor Collide"}, {70, "Barter Exit"}, {76, "Notice Corpse"},
            {89, "Pursue Idle Topic"}, {77, "Time To Go"}
        };
        std::vector<std::pair<uint16_t, const char*>> crimeStealth = {
            {42, "Assault"}, {44, "Assault NC"}, {43, "Murder"}, {45, "Murder NC"},
            {46, "Pickpocket NC"}, {88, "Pickpocket Topic"}, {38, "Steal"}, {47, "Steal From NC"},
            {49, "Trespass"}, {48, "Trespass Against NC"}, {50, "Werewolf Transform Crime"}
        };
        std::vector<std::pair<uint16_t, const char*>> objectInteractions = {
            {85, "Destroy Object"}, {84, "Knock Over Object"}, {87, "Locked Object"},
            {86, "Stand On Furniture"}, {82, "Z-Key Object"}
        };
        std::vector<std::pair<uint16_t, const char*>> playerActions = {
            {91, "Player Cast Projectile"}, {92, "Player Cast Self"}, {99, "Player In Iron Sights"},
            {93, "Player Shout"}, {81, "Shoot Bow"}, {80, "Swing Melee Weapon"}
        };
        std::vector<std::pair<uint16_t, const char*>> social = {
            {78, "Goodbye"}, {79, "Hello"}, {94, "Idle"}, {74, "Training Exit"}
        };
        
        // Collect all generic globals
        collectGlobals(genericGlobals, behaviors);
        collectGlobals(genericGlobals, crimeStealth);
        collectGlobals(genericGlobals, objectInteractions);
        collectGlobals(genericGlobals, playerActions);
        collectGlobals(genericGlobals, social);
        
        if (CollapsingHeader("Generic Dialogue", ImGuiTreeNodeFlags_None)) {
            Indent();
            
            RenderCategoryButtons("Generic", genericGlobals);
            Spacing();
            
            // 4-column layout for Generic subcategories
            if (BeginTable("GenericToggles", 4, ImGuiTableFlags_None)) {
                TableSetupColumn("Col1", ImGuiTableColumnFlags_WidthFixed, columnWidth);
                TableSetupColumn("Col2", ImGuiTableColumnFlags_WidthFixed, columnWidth);
                TableSetupColumn("Col3", ImGuiTableColumnFlags_WidthFixed, columnWidth);
                TableSetupColumn("Col4", ImGuiTableColumnFlags_WidthFixed, columnWidth);
                
                // Row 1: Behaviors | Social
                TableNextRow();
                TableSetColumnIndex(0);
                PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 1.0f, 1.0f));
                Text("Behaviors");
                PopStyleColor();
                TableSetColumnIndex(2);
                PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 1.0f, 1.0f));
                Text("Social");
                PopStyleColor();
                
                int behaviorsIndex = 0, socialIndex = 0;
                int maxRows = behaviors.size() > social.size() ? (int)behaviors.size() : (int)social.size();
                maxRows = (maxRows + 1) / 2;
                
                for (int row = 0; row < maxRows; row++) {
                    TableNextRow();
                    
                    // Behaviors (left side, cols 0-1)
                    if (behaviorsIndex < behaviors.size()) {
                        TableSetColumnIndex(0);
                        RenderToggleWithTooltip(behaviors[behaviorsIndex].second, 
                            behaviors[behaviorsIndex].first, behaviors);
                        behaviorsIndex++;
                    }
                    if (behaviorsIndex < behaviors.size()) {
                        TableSetColumnIndex(1);
                        RenderToggleWithTooltip(behaviors[behaviorsIndex].second, 
                            behaviors[behaviorsIndex].first, behaviors);
                        behaviorsIndex++;
                    }
                    
                    // Social (right side, cols 2-3)
                    if (socialIndex < social.size()) {
                        TableSetColumnIndex(2);
                        RenderToggleWithTooltip(social[socialIndex].second, 
                            social[socialIndex].first, social);
                        socialIndex++;
                    }
                    if (socialIndex < social.size()) {
                        TableSetColumnIndex(3);
                        RenderToggleWithTooltip(social[socialIndex].second, 
                            social[socialIndex].first, social);
                        socialIndex++;
                    }
                }
                
                // Spacing row
                TableNextRow();
                
                // Row 2: Object Interactions | Player Actions
                TableNextRow();
                TableSetColumnIndex(0);
                PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 1.0f, 1.0f));
                Text("Object Interactions");
                PopStyleColor();
                TableSetColumnIndex(2);
                PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 1.0f, 1.0f));
                Text("Player Actions");
                PopStyleColor();
                
                int objectsIndex = 0, playerIndex = 0;
                maxRows = objectInteractions.size() > playerActions.size() ? (int)objectInteractions.size() : (int)playerActions.size();
                maxRows = (maxRows + 1) / 2;
                
                for (int row = 0; row < maxRows; row++) {
                    TableNextRow();
                    
                    // Object Interactions (left side, cols 0-1)
                    if (objectsIndex < objectInteractions.size()) {
                        TableSetColumnIndex(0);
                        RenderToggleWithTooltip(objectInteractions[objectsIndex].second, 
                            objectInteractions[objectsIndex].first, objectInteractions);
                        objectsIndex++;
                    }
                    if (objectsIndex < objectInteractions.size()) {
                        TableSetColumnIndex(1);
                        RenderToggleWithTooltip(objectInteractions[objectsIndex].second, 
                            objectInteractions[objectsIndex].first, objectInteractions);
                        objectsIndex++;
                    }
                    
                    // Player Actions (right side, cols 2-3)
                    if (playerIndex < playerActions.size()) {
                        TableSetColumnIndex(2);
                        RenderToggleWithTooltip(playerActions[playerIndex].second, 
                            playerActions[playerIndex].first, playerActions);
                        playerIndex++;
                    }
                    if (playerIndex < playerActions.size()) {
                        TableSetColumnIndex(3);
                        RenderToggleWithTooltip(playerActions[playerIndex].second, 
                            playerActions[playerIndex].first, playerActions);
                        playerIndex++;
                    }
                }
                
                EndTable();
            }
            Spacing();
            
            // Crime & Stealth (separate, 2 columns)
            AddHeaderOption("Crime & Stealth");
            if (BeginTable("CrimeToggles", 2, ImGuiTableFlags_None)) {
                TableSetupColumn("Column1", ImGuiTableColumnFlags_WidthFixed, columnWidth);
                TableSetupColumn("Column2", ImGuiTableColumnFlags_WidthFixed, columnWidth);
                
                int index = 0;
                for (const auto& [subtype, name] : crimeStealth) {
                    if (index % 2 == 0) {
                        TableNextRow();
                        TableSetColumnIndex(0);
                    } else {
                        TableSetColumnIndex(1);
                    }
                    RenderToggleWithTooltip(name, subtype, crimeStealth);
                    index++;
                }
                EndTable();
            }
            
            Unindent();
        }
        
        Spacing();
        
        // ==== FOLLOWER DIALOGUE ====
        std::vector<RE::TESGlobal*> followerGlobals;
        
        std::vector<std::pair<uint16_t, const char*>> commands = {
            {16, "Agree"}, {18, "Exit Favor State"}, {19, "Moral Refusal"},
            {13, "Refuse"}, {15, "Show"}
        };
        
        collectGlobals(followerGlobals, commands);
        
        // Add follower commentary global
        if (settings.mcm.blockFollowerCommentaryGlobal) {
            followerGlobals.push_back(settings.mcm.blockFollowerCommentaryGlobal);
        }
        
        if (CollapsingHeader("Follower Dialogue", ImGuiTreeNodeFlags_None)) {
            Indent();
            
            RenderCategoryButtons("Follower", followerGlobals);
            Spacing();
            
            // 4-column layout: Commands | Follower Commentary
            if (BeginTable("FollowerToggles", 4, ImGuiTableFlags_None)) {
                TableSetupColumn("Col1", ImGuiTableColumnFlags_WidthFixed, columnWidth);
                TableSetupColumn("Col2", ImGuiTableColumnFlags_WidthFixed, columnWidth);
                TableSetupColumn("Col3", ImGuiTableColumnFlags_WidthFixed, columnWidth);
                TableSetupColumn("Col4", ImGuiTableColumnFlags_WidthFixed, columnWidth);
                
                // Headers
                TableNextRow();
                TableSetColumnIndex(0);
                PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 1.0f, 1.0f));
                Text("Commands");
                PopStyleColor();
                TableSetColumnIndex(2);
                PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 1.0f, 1.0f));
                Text("Follower Commentary");
                PopStyleColor();
                
                // Render Commands in left columns (0-1)
                int commandIndex = 0;
                int maxRows = ((int)commands.size() + 1) / 2;
                
                for (int row = 0; row < maxRows; row++) {
                    TableNextRow();
                    
                    // Commands (left side, cols 0-1)
                    if (commandIndex < commands.size()) {
                        TableSetColumnIndex(0);
                        RenderToggleWithTooltip(commands[commandIndex].second, 
                            commands[commandIndex].first, commands);
                        commandIndex++;
                    }
                    if (commandIndex < commands.size()) {
                        TableSetColumnIndex(1);
                        RenderToggleWithTooltip(commands[commandIndex].second, 
                            commands[commandIndex].first, commands);
                        commandIndex++;
                    }
                    
                    // Follower Commentary toggle in right column (only first row)
                    if (row == 0) {
                        TableSetColumnIndex(2);
                        RenderGlobalToggle("Block Follower Commentary", settings.mcm.blockFollowerCommentaryGlobal);
                        if (IsItemHovered()) {
                            BeginTooltip();
                            Text("Follower commentary scenes - triggered when entering dungeons or areas");
                            EndTooltip();
                        }
                    }
                }
                
                EndTable();
            }
            
            Unindent();
        }
        
        Spacing();
        
        // ==== OTHER DIALOGUE ====
        std::vector<RE::TESGlobal*> otherGlobals;
        
        std::vector<std::pair<uint16_t, const char*>> voicePowers = {
            {53, "Voice Power End (Long)"}, {52, "Voice Power End (Short)"},
            {54, "Voice Power Start (Long)"}, {51, "Voice Power Start (Short)"}
        };
        
        collectGlobals(otherGlobals, voicePowers);
        
        if (CollapsingHeader("Other Dialogue", ImGuiTreeNodeFlags_None)) {
            Indent();
            
            RenderCategoryButtons("Other", otherGlobals);
            Spacing();
            
            // Voice Powers
            AddHeaderOption("Voice Powers");
            if (BeginTable("VoicePowersToggles", 2, ImGuiTableFlags_None)) {
                TableSetupColumn("Column1", ImGuiTableColumnFlags_WidthFixed, columnWidth);
                TableSetupColumn("Column2", ImGuiTableColumnFlags_WidthFixed, columnWidth);
                
                int index = 0;
                for (const auto& [subtype, name] : voicePowers) {
                    if (index % 2 == 0) {
                        TableNextRow();
                        TableSetColumnIndex(0);
                    } else {
                        TableSetColumnIndex(1);
                    }
                    RenderToggleWithTooltip(name, subtype, voicePowers);
                    index++;
                }
                EndTable();
            }
            
            Unindent();
        }
        
        Unindent();
    }
}
