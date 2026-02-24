#include "STFUMenu.h"
#include "Config.h"
#include "SettingsPersistence.h"
#include <spdlog/spdlog.h>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <unordered_set>
#include <algorithm>

// Color definitions
#define COLOR_ALLOWED   IM_COL32(100, 255, 100, 255)  // Green
#define COLOR_SOFT_BLOCK IM_COL32(255, 140, 0, 255)   // Orange
#define COLOR_HARD_BLOCK IM_COL32(255, 50, 50, 255)   // Red
#define COLOR_SKYRIM    IM_COL32(150, 150, 150, 255)  // Gray
#define COLOR_FILTER    IM_COL32(255, 220, 0, 255)    // Yellow
#define COLOR_TOGGLED_OFF IM_COL32(100, 200, 255, 255)  // Cyan
#define COLOR_WHITELIST IM_COL32(230, 230, 255, 255)  // Pale lavender (almost white with hint of blue)
#define COLOR_TIME      IM_COL32(180, 180, 180, 255)  // Light gray
#define COLOR_SUBTYPE   IM_COL32(70, 110, 200, 255)  // Darker Blue
#define COLOR_QUEST     IM_COL32(255, 150, 255, 255)  // Magenta
#define COLOR_TOPIC     IM_COL32(255, 255, 100, 255)  // Yellow
#define COLOR_SPEAKER   IM_COL32(200, 255, 200, 255)  // Light green
#define COLOR_TEXT      IM_COL32(255, 255, 255, 255)  // White

void STFUMenu::Initialize()
{
    if (initialized_) {
        return;
    }
    
    if (!SKSEMenuFramework::IsInstalled()) {
        spdlog::error("[STFUMenu] SKSEMenuFramework not installed!");
        return;
    }
    
    // Register menu window
    window_ = SKSEMenuFramework::AddWindow(&STFUMenu::Render, true);
    
    if (!window_) {
        spdlog::error("[STFUMenu] Failed to create menu window!");
        return;
    }
    
    window_->IsOpen = false;
    initialized_ = true;
    
    // Check if SkyrimNet.esp is loaded
    auto* dataHandler = RE::TESDataHandler::GetSingleton();
    if (dataHandler) {
        isSkyrimNetLoaded_ = dataHandler->GetModIndex("SkyrimNet.esp").has_value();
        spdlog::info("[STFUMenu] SkyrimNet.esp {}", isSkyrimNetLoaded_ ? "detected" : "not found");
    }
    
    spdlog::info("[STFUMenu] Initialized successfully");
}

void STFUMenu::Toggle()
{
    if (!initialized_ || !window_) {
        spdlog::warn("[STFUMenu::Toggle] Called but not initialized or no window");
        return;
    }
    
    bool currentState = window_->IsOpen.load();
    bool newState = !currentState;
    window_->IsOpen.store(newState);
    
    spdlog::info("[STFUMenu::Toggle] Menu toggled from {} to {}", currentState, newState);
    
    // Mark that we need to refresh all tabs on next render
    if (newState) {
        needsHistoryRefresh_ = true;
        needsBlacklistRefresh_ = true;
        needsWhitelistRefresh_ = true;
    }
}

bool STFUMenu::IsOpen()
{
    return initialized_ && window_ && window_->IsOpen;
}

void STFUMenu::Render()
{
    using namespace ImGuiMCP;
    
    // Refresh history if needed (after menu was opened)
    if (needsHistoryRefresh_) {
        try {
            RefreshHistory();
            needsHistoryRefresh_ = false;
        } catch (const std::exception& e) {
            spdlog::error("[STFUMenu] Exception during RefreshHistory in Render: {}", e.what());
            needsHistoryRefresh_ = false;  // Clear flag even on error
        }
    }
    
    // Refresh blacklist if needed (after switching to blacklist tab)
    if (needsBlacklistRefresh_) {
        try {
            auto allBlacklist = DialogueDB::GetDatabase()->GetBlacklist();
            std::vector<DialogueDB::BlacklistEntry> filteredBlacklist;
            
            std::string query = searchQuery_;
            std::transform(query.begin(), query.end(), query.begin(), ::tolower);
            
            // Apply filters when reloading
            for (const auto& entry : allBlacklist) {
                bool matches = true;
                
                // Search query filter
                if (query.length() > 0) {
                    bool foundMatch = false;
                    std::string editorID = entry.targetEditorID;
                    std::transform(editorID.begin(), editorID.end(), editorID.begin(), ::tolower);
                    if (editorID.find(query) != std::string::npos) foundMatch = true;
                    if (!foundMatch && !entry.responseText.empty()) {
                        std::string responseText = entry.responseText;
                        std::transform(responseText.begin(), responseText.end(), responseText.begin(), ::tolower);
                        if (responseText.find(query) != std::string::npos) foundMatch = true;
                    }
                    if (!foundMatch) matches = false;
                }
                
                // Target type filter
                if (matches && filterTargetType_ > 0) {
                    if (filterTargetType_ == 4) {
                        if (entry.targetType != DialogueDB::BlacklistTarget::Scene || 
                            entry.filterCategory == "BardSongs" || 
                            entry.filterCategory == "FollowerCommentary") matches = false;
                    } else if (filterTargetType_ == 5) {
                        if (entry.filterCategory != "BardSongs") matches = false;
                    } else if (filterTargetType_ == 6) {
                        if (entry.filterCategory != "FollowerCommentary") matches = false;
                    } else if (filterTargetType_ <= 3) {
                        if (static_cast<int>(entry.targetType) != filterTargetType_) matches = false;
                    }
                }
                
                // Block type filter
                if (matches) {
                    bool blockTypeMatch = false;
                    if (filterSoftBlock_ && entry.blockType == DialogueDB::BlockType::Soft) blockTypeMatch = true;
                    if (filterHardBlock_ && entry.blockType == DialogueDB::BlockType::Hard) blockTypeMatch = true;
                    if (filterSkyrimNet_ && entry.blockType == DialogueDB::BlockType::SkyrimNet) blockTypeMatch = true;
                    if (!blockTypeMatch) matches = false;
                }
                
                // Topic/Scene filter checkboxes
                if (matches && !filterTopic_ && entry.targetType == DialogueDB::BlacklistTarget::Topic) matches = false;
                if (matches && !filterScene_ && entry.targetType == DialogueDB::BlacklistTarget::Scene) matches = false;
                
                if (matches) filteredBlacklist.push_back(entry);
            }
            
            blacklistCache_ = filteredBlacklist;
            blacklistLoadedCount_ = static_cast<int>(blacklistCache_.size());
            
            // Reload selected entry from database to get updated filterCategory
            if (selectedBlacklistEntry_.id > 0) {
                for (const auto& entry : blacklistCache_) {
                    if (entry.id == selectedBlacklistEntry_.id) {
                        selectedBlacklistEntry_ = entry;
                        break;
                    }
                }
            }
            needsBlacklistRefresh_ = false;
        } catch (const std::exception& e) {
            spdlog::error("[STFUMenu] Exception during blacklist refresh in Render: {}", e.what());
            needsBlacklistRefresh_ = false;  // Clear flag even on error
        }
    }
    
    // Refresh whitelist if needed (after deletion or other operations)
    if (needsWhitelistRefresh_) {
        try {
            auto allWhitelist = DialogueDB::GetDatabase()->GetWhitelist();
            std::vector<DialogueDB::BlacklistEntry> filteredWhitelist;
            
            std::string query = whitelistSearchQuery_;
            std::transform(query.begin(), query.end(), query.begin(), ::tolower);
            
            // Apply filters when reloading
            for (const auto& entry : allWhitelist) {
                bool matches = true;
                
                // Search query filter
                if (query.length() > 0) {
                    bool foundMatch = false;
                    std::string editorID = entry.targetEditorID;
                    std::transform(editorID.begin(), editorID.end(), editorID.begin(), ::tolower);
                    if (editorID.find(query) != std::string::npos) foundMatch = true;
                    if (!foundMatch && !entry.responseText.empty()) {
                        std::string responseText = entry.responseText;
                        std::transform(responseText.begin(), responseText.end(), responseText.begin(), ::tolower);
                        if (responseText.find(query) != std::string::npos) foundMatch = true;
                    }
                    if (!foundMatch) matches = false;
                }
                
                // Target type filter
                if (matches && whitelistFilterTargetType_ > 0) {
                    if (whitelistFilterTargetType_ == 4) {
                        if (entry.targetType != DialogueDB::BlacklistTarget::Scene || 
                            entry.filterCategory == "BardSongs" || 
                            entry.filterCategory == "FollowerCommentary") matches = false;
                    } else if (whitelistFilterTargetType_ == 5) {
                        if (entry.filterCategory != "BardSongs") matches = false;
                    } else if (whitelistFilterTargetType_ == 6) {
                        if (entry.filterCategory != "FollowerCommentary") matches = false;
                    } else if (whitelistFilterTargetType_ <= 3) {
                        if (static_cast<int>(entry.targetType) != whitelistFilterTargetType_) matches = false;
                    }
                }
                
                // Topic/Scene filter checkboxes
                if (matches && !whitelistFilterTopic_ && entry.targetType == DialogueDB::BlacklistTarget::Topic) matches = false;
                if (matches && !whitelistFilterScene_ && entry.targetType == DialogueDB::BlacklistTarget::Scene) matches = false;
                
                if (matches) filteredWhitelist.push_back(entry);
            }
            
            whitelistCache_ = filteredWhitelist;
            whitelistLoadedCount_ = static_cast<int>(whitelistCache_.size());
            
            // Reload selected entry from database
            if (selectedWhitelistEntry_.id > 0) {
                for (const auto& entry : whitelistCache_) {
                    if (entry.id == selectedWhitelistEntry_.id) {
                        selectedWhitelistEntry_ = entry;
                        break;
                    }
                }
            }
            needsWhitelistRefresh_ = false;
        } catch (const std::exception& e) {
            spdlog::error("[STFUMenu] Exception during whitelist refresh in Render: {}", e.what());
            needsWhitelistRefresh_ = false;  // Clear flag even on error
        }
    }
    
    // Calculate UI scale factor based on screen resolution (once)
    static bool scaleCalculated = false;
    if (!scaleCalculated) {
        ImGuiIO* io = GetIO();
        // 1440p = 1.0 scale (design target), 1080p = 0.9 scale, higher resolutions scale proportionally
        if (io->DisplaySize.y <= 1440.0f) {
            // Gentler scaling for lower resolutions (1080p → 0.9, 1440p → 1.0)
            uiScale_ = 0.9f + (io->DisplaySize.y - 1080.0f) / 3600.0f;
        } else {
            // Proportional scaling above 1440p
            uiScale_ = io->DisplaySize.y / 1440.0f;
        }
        scaleCalculated = true;
        spdlog::info("[STFUMenu] UI scale factor: {:.2f} (resolution: {}x{})", 
            uiScale_, (int)io->DisplaySize.x, (int)io->DisplaySize.y);
    }
    
    // Set initial window size on first open and scroll to bottom
    static bool firstOpen = true;
    
    bool isCurrentlyOpen = window_->IsOpen.load();
    
    if (firstOpen && isCurrentlyOpen) {
        SetNextWindowSize(ImVec2(1800 * uiScale_, 1200 * uiScale_), ImGuiCond_FirstUseEver);
        
        // Center the window on screen
        ImGuiIO* io = GetIO();
        ImVec2 center(io->DisplaySize.x * 0.5f, io->DisplaySize.y * 0.5f);
        SetNextWindowPos(center, ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
        
        firstOpen = false;
        needScrollToBottom_ = true;
    }
    
    bool isOpen = window_->IsOpen.load();
    
    SetNextWindowBgAlpha(0.9f);
    if (!Begin("STFU - Dialogue Manager", &isOpen, ImGuiWindowFlags_None)) {
        End();
        window_->IsOpen.store(isOpen);
        return;
    }
    
    // Apply per-window font scaling (doesn't affect other mods)
    SetWindowFontScale(uiScale_);
    
    // Check for Esc key to close menu (but not if popup is open, if ESC is the menu hotkey, or if capturing hotkey)
    // ESC scancode is 0x01
    const auto& settings = Config::GetSettings();
    bool escIsMenuHotkey = (settings.menuHotkey == 0x01);
    if (!showResponsesPopup_ && !showManualEntryModal_ && !escIsMenuHotkey && !IsCapturingHotkey() && IsKeyPressed(ImGuiKey_Escape, false)) {
        isOpen = false;
    }
    
    // Tab bar at top - always visible
    if (BeginTabBar("STFUTabs", ImGuiTabBarFlags_None)) {
        if (BeginTabItem("History")) {
            currentTab_ = 0;
            EndTabItem();
        }
        
        if (BeginTabItem("Blacklist")) {
            currentTab_ = 1;
            EndTabItem();
        }
        
        if (BeginTabItem("Whitelist")) {
            currentTab_ = 2;
            EndTabItem();
        }
        
        if (BeginTabItem("Settings")) {
            currentTab_ = 3;
            EndTabItem();
        }
        
        EndTabBar();
    }
    
    // Refresh blacklist when switching to Blacklist tab
    if (currentTab_ == 1 && previousTab_ != 1) {
        // Switched to Blacklist tab - mark for refresh on next render
        needsBlacklistRefresh_ = true;
    }
    
    // Refresh whitelist when switching to Whitelist tab
    if (currentTab_ == 2 && previousTab_ != 2) {
        // Switched to Whitelist tab - refresh the unified list (includes scenes) with filters
        auto allWhitelist = DialogueDB::GetDatabase()->GetWhitelist();
        std::vector<DialogueDB::BlacklistEntry> filteredWhitelist;
        
        std::string query = whitelistSearchQuery_;
        std::transform(query.begin(), query.end(), query.begin(), ::tolower);
        
        // Apply filters when reloading
        for (const auto& entry : allWhitelist) {
            bool matches = true;
            
            // Search query filter
            if (query.length() > 0) {
                bool foundMatch = false;
                std::string editorID = entry.targetEditorID;
                std::transform(editorID.begin(), editorID.end(), editorID.begin(), ::tolower);
                if (editorID.find(query) != std::string::npos) foundMatch = true;
                if (!foundMatch && !entry.responseText.empty()) {
                    std::string responseText = entry.responseText;
                    std::transform(responseText.begin(), responseText.end(), responseText.begin(), ::tolower);
                    if (responseText.find(query) != std::string::npos) foundMatch = true;
                }
                if (!foundMatch) matches = false;
            }
            
            // Target type filter
            if (matches && whitelistFilterTargetType_ > 0) {
                if (whitelistFilterTargetType_ == 4) {
                    if (entry.targetType != DialogueDB::BlacklistTarget::Scene || 
                        entry.filterCategory == "BardSongs" || 
                        entry.filterCategory == "FollowerCommentary") matches = false;
                } else if (whitelistFilterTargetType_ == 5) {
                    if (entry.filterCategory != "BardSongs") matches = false;
                } else if (whitelistFilterTargetType_ == 6) {
                    if (entry.filterCategory != "FollowerCommentary") matches = false;
                } else if (whitelistFilterTargetType_ <= 3) {
                    if (static_cast<int>(entry.targetType) != whitelistFilterTargetType_) matches = false;
                }
            }
            
            // Topic/Scene filter checkboxes
            if (matches && !whitelistFilterTopic_ && entry.targetType == DialogueDB::BlacklistTarget::Topic) matches = false;
            if (matches && !whitelistFilterScene_ && entry.targetType == DialogueDB::BlacklistTarget::Scene) matches = false;
            
            if (matches) filteredWhitelist.push_back(entry);
        }
        
        whitelistCache_ = filteredWhitelist;
        whitelistLoadedCount_ = static_cast<int>(whitelistCache_.size());
        
        // Reload selected entry from database to get updated filterCategory
        if (selectedWhitelistEntry_.id > 0) {
            for (const auto& entry : whitelistCache_) {
                if (entry.id == selectedWhitelistEntry_.id) {
                    selectedWhitelistEntry_ = entry;
                    break;
                }
            }
        }
    }
    previousTab_ = currentTab_;
    
    // Render content in child window to prevent tabs from scrolling
    if (BeginChild("TabContent", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_None)) {
        if (currentTab_ == 0) {
            RenderHistoryTab();
        } else if (currentTab_ == 1) {
            RenderBlacklistTab();
        } else if (currentTab_ == 2) {
            RenderWhitelistTab();
        } else if (currentTab_ == 3) {
            RenderSettingsTab();
        }
    }
    EndChild();
    
    // Open popup after exiting child windows (must be at root window level)
    if (showResponsesPopup_) {
        OpenPopup("Responses");
    }
    
    // Open manual entry modal
    if (showManualEntryModal_) {
        OpenPopup("Manual Entry");
    }
    
    // Render popup windows (must be after main window content)
    RenderResponsesPopup();
    RenderManualEntryModal();
    
    End();
    window_->IsOpen.store(isOpen);
}

void STFUMenu::RenderHistoryTab()
{
    using namespace ImGuiMCP;
    
    // Auto-refresh every 2 seconds
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastHistoryRefresh_).count();
    if (elapsed >= 2) {
        try {
            RefreshHistory();
        } catch (const std::exception& e) {
            spdlog::error("[STFUMenu] Exception during auto-refresh: {}", e.what());
        }
    }
    
    // Get available space
    ImVec2 availSpace;
    GetContentRegionAvail(&availSpace);
    
    // Search bar at the top
    SetNextItemWidth(availSpace.x * 0.33f);
    bool searchChanged = InputText("##historysearch", historySearchQuery_, sizeof(historySearchQuery_));
    SameLine();
    float buttonWidth = 60.0f;
    if (Button("Search##historysearch", ImVec2(buttonWidth, 0))) {
        // Search is live, button just for visual clarity
    }
    SameLine();
    if (Button("Clear##historysearch", ImVec2(buttonWidth, 0)) && historySearchQuery_[0] != '\0') {
        historySearchQuery_[0] = '\0';
        searchChanged = true;
    }
    
    // Convert search query to lowercase for case-insensitive matching
    std::string searchLower(historySearchQuery_);
    std::transform(searchLower.begin(), searchLower.end(), searchLower.begin(), ::tolower);
    bool hasSearchQuery = !searchLower.empty();
    
    // Count matching results for display
    int matchingResults = 0;
    if (hasSearchQuery) {
        for (auto it = historyCache_.rbegin(); it != historyCache_.rend(); ++it) {
            const auto& entry = *it;
            std::string speakerLower = entry.speakerName;
            std::transform(speakerLower.begin(), speakerLower.end(), speakerLower.begin(), ::tolower);
            std::string responseLower = entry.responseText;
            std::transform(responseLower.begin(), responseLower.end(), responseLower.begin(), ::tolower);
            
            if (speakerLower.find(searchLower) != std::string::npos ||
                responseLower.find(searchLower) != std::string::npos) {
                matchingResults++;
            }
        }
        SameLine();
        Text("Results: %d", matchingResults);
    }
    
    // Top panel: History list (50% of height minus search bar space)
    float searchBarHeight = 40.0f;  // Increased to account for actual search bar height
    float historyHeight = availSpace.y * 0.50f - searchBarHeight;
    float detailHeight = availSpace.y * 0.50f - 10.0f;  // Leave space for separator
    
    // History scrollable area
    if (BeginChild("HistoryList", ImVec2(0, historyHeight), ImGuiChildFlags_Border, ImGuiWindowFlags_None)) {
        // Render in reverse order (most recent at bottom)
        int entryIndex = 0;
        for (auto it = historyCache_.rbegin(); it != historyCache_.rend(); ++it) {
            const auto& entry = *it;
            
            // Apply search filter
            if (hasSearchQuery) {
                std::string speakerLower = entry.speakerName;
                std::transform(speakerLower.begin(), speakerLower.end(), speakerLower.begin(), ::tolower);
                std::string responseLower = entry.responseText;
                std::transform(responseLower.begin(), responseLower.end(), responseLower.begin(), ::tolower);
                
                bool matches = (speakerLower.find(searchLower) != std::string::npos) ||
                               (responseLower.find(searchLower) != std::string::npos);
                
                if (!matches) {
                    continue;  // Skip entries that don't match search
                }
            }
            
            PushID(static_cast<int>(entry.id));
            
            try {
                // Format: [Time][Status][Subtype][Quest][Topic]
                std::string timeStr = FormatTimestamp(entry.timestamp);
                
                // Check if entry is whitelisted - if so, override status display
                auto db = DialogueDB::GetDatabase();
                bool isWhitelisted = false;
                if (db) {
                    if (entry.isScene && !entry.sceneEditorID.empty()) {
                        isWhitelisted = db->IsWhitelisted(DialogueDB::BlacklistTarget::Scene, 0, entry.sceneEditorID);
                    } else {
                        isWhitelisted = db->IsWhitelisted(DialogueDB::BlacklistTarget::Topic, entry.topicFormID, entry.topicEditorID);
                    }
                }
                
                std::string statusStr;
                ImU32 statusColor;
                if (isWhitelisted) {
                    statusStr = "ALLOWED";
                    statusColor = COLOR_WHITELIST;
                } else {
                    statusStr = GetStatusText(entry.blockedStatus);
                    statusColor = GetStatusColor(entry.blockedStatus);
                }
                
                std::string subtypeStr = entry.topicSubtypeName;
                std::string questStr = entry.questEditorID.empty() ? "(No Quest)" : entry.questEditorID;
                
                // Show Scene EditorID for scene dialogue, otherwise show Topic EditorID
                std::string topicStr;
                if (entry.isScene && !entry.sceneEditorID.empty()) {
                    topicStr = entry.sceneEditorID;
                } else if (entry.topicEditorID.empty()) {
                    char formIDStr[32];
                    sprintf_s(formIDStr, "0%X", entry.topicFormID);
                    topicStr = formIDStr;
                } else {
                    topicStr = entry.topicEditorID;
                }
                
                // Use invisible selectable for click detection and highlighting
                // Calculate height for both lines
                ImVec2 cursorPos;
                GetCursorScreenPos(&cursorPos);
                
                // Render colored text first to measure height
                ImVec2 startPos;
                GetCursorPos(&startPos);
                
                // First line: metadata
                TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "[%s]", timeStr.c_str());
                SameLine();
                ImVec4 statusColorVec = ImVec4(
                    ((statusColor >> IM_COL32_R_SHIFT) & 0xFF) / 255.0f,
                    ((statusColor >> IM_COL32_G_SHIFT) & 0xFF) / 255.0f,
                    ((statusColor >> IM_COL32_B_SHIFT) & 0xFF) / 255.0f,
                    ((statusColor >> IM_COL32_A_SHIFT) & 0xFF) / 255.0f
                );
                TextColored(statusColorVec, "[%s]", statusStr.c_str());
                SameLine();
                
                // Use COLOR_SUBTYPE for darker blue
                ImVec4 subtypeColorVec = ImVec4(
                    ((COLOR_SUBTYPE >> IM_COL32_R_SHIFT) & 0xFF) / 255.0f,
                    ((COLOR_SUBTYPE >> IM_COL32_G_SHIFT) & 0xFF) / 255.0f,
                    ((COLOR_SUBTYPE >> IM_COL32_B_SHIFT) & 0xFF) / 255.0f,
                    ((COLOR_SUBTYPE >> IM_COL32_A_SHIFT) & 0xFF) / 255.0f
                );
                TextColored(subtypeColorVec, "[%s]", subtypeStr.c_str());
                SameLine();
                TextColored(ImVec4(1.0f, 0.6f, 1.0f, 1.0f), "[%s]", questStr.c_str());
                SameLine();
                TextColored(ImVec4(1.0f, 1.0f, 0.4f, 1.0f), "[%s]", topicStr.c_str());
                
                // Second line: speaker and dialogue (full text, wraps if needed)
                std::string speakerLine = "  " + entry.speakerName + ": " + FormatResponseText(entry.responseText);
                Text("%s", speakerLine.c_str());
                
                // Get end position to calculate height
                ImVec2 endPos;
                GetCursorPos(&endPos);
                float itemHeight = endPos.y - startPos.y;
                
                // Draw invisible selectable over the entire entry
                SetCursorPos(startPos);
                
                // Check if this entry is selected (either single or multi)
                bool isSelected = false;
                if (!selectedHistoryIDs_.empty()) {
                    // Multi-selection mode
                    for (const auto& id : selectedHistoryIDs_) {
                        if (id == entry.id) {
                            isSelected = true;
                            break;
                        }
                    }
                } else {
                    // Single selection mode (legacy)
                    isSelected = (selectedEntryID_ == entry.id);
                }
                
                if (Selectable("##historyentry", isSelected, ImGuiSelectableFlags_None, ImVec2(0, itemHeight))) {
                    ImGuiIO* io = GetIO();
                    bool ctrlPressed = io->KeyCtrl;
                    bool shiftPressed = io->KeyShift;
                    
                    if (shiftPressed && lastClickedHistoryIndex_ >= 0) {
                        // Shift+click: Select range
                        selectedHistoryIDs_.clear();
                        
                        int start = (std::min)(lastClickedHistoryIndex_, entryIndex);
                        int end = (std::max)(lastClickedHistoryIndex_, entryIndex);
                        
                        int idx = 0;
                        for (auto it2 = historyCache_.rbegin(); it2 != historyCache_.rend(); ++it2) {
                            if (idx >= start && idx <= end) {
                                selectedHistoryIDs_.push_back(it2->id);
                            }
                            idx++;
                        }
                        
                        hasSelection_ = false;  // Multi-selection, no single entry details
                    } else if (ctrlPressed) {
                        // Ctrl+click: Toggle selection
                        
                        // If transitioning from single to multi-selection, add current selection first
                        if (selectedHistoryIDs_.empty() && selectedEntryID_ != -1) {
                            selectedHistoryIDs_.push_back(selectedEntryID_);
                        }
                        
                        bool found = false;
                        for (size_t i = 0; i < selectedHistoryIDs_.size(); ++i) {
                            if (selectedHistoryIDs_[i] == entry.id) {
                                // Already selected, remove it
                                selectedHistoryIDs_.erase(selectedHistoryIDs_.begin() + i);
                                found = true;
                                break;
                            }
                        }
                        if (!found) {
                            // Not selected, add it
                            selectedHistoryIDs_.push_back(entry.id);
                        }
                        
                        lastClickedHistoryIndex_ = entryIndex;
                        hasSelection_ = false;  // Multi-selection
                    } else {
                        // Normal click: Single selection
                        selectedHistoryIDs_.clear();
                        LoadSelectedEntry(entry.id);
                        lastClickedHistoryIndex_ = entryIndex;
                        spdlog::info("[STFUMenu] Selected entry ID: {}", entry.id);
                    }
                }
                
                // Add some spacing between entries
                Spacing();
            } catch (const std::exception& e) {
                spdlog::error("[STFUMenu] Exception rendering history entry {}: {}", entry.id, e.what());
                Text("(Error displaying entry)");
                Spacing();
            }
            
            PopID();
            entryIndex++;
        }
        
        // Auto-scroll to bottom if user was near bottom, or if menu just opened
        float scrollY = GetScrollY();
        float scrollMaxY = GetScrollMaxY();
        bool wasNearBottom = (scrollMaxY - scrollY) < 50.0f;  // Within 50 pixels of bottom
        
        if ((wasNearBottom && scrollY < scrollMaxY) || needScrollToBottom_) {
            SetScrollHereY(1.0f);
            needScrollToBottom_ = false;
        }
        
        // Handle Delete key to remove selected entries
        if (IsKeyPressed(ImGuiKey_Delete, false)) {
            if (!selectedHistoryIDs_.empty()) {
                // Delete multiple selected entries
                int deletedCount = DialogueDB::GetDatabase()->DeleteDialogueEntriesBatch(selectedHistoryIDs_);
                if (deletedCount > 0) {
                    spdlog::info("[STFUMenu] Deleted {} history entries", deletedCount);
                    selectedHistoryIDs_.clear();
                    selectedEntryID_ = -1;
                    hasSelection_ = false;
                    lastClickedHistoryIndex_ = -1;
                    needsHistoryRefresh_ = true;
                }
            } else if (selectedEntryID_ != -1) {
                // Delete single selected entry
                if (DialogueDB::GetDatabase()->DeleteDialogueEntry(selectedEntryID_)) {
                    spdlog::info("[STFUMenu] Deleted history entry {}", selectedEntryID_);
                    selectedEntryID_ = -1;
                    hasSelection_ = false;
                    lastClickedHistoryIndex_ = -1;
                    needsHistoryRefresh_ = true;
                }
            }
        }
    }
    EndChild();
    
    Separator();
    
    // Bottom panel: Detail and settings (30% of height)
    if (BeginChild("DetailPanel", ImVec2(0, detailHeight), ImGuiChildFlags_Border, ImGuiWindowFlags_None)) {
        RenderDetailPanel();
    }
    EndChild();
}

void STFUMenu::RenderDetailPanel()
{
    using namespace ImGuiMCP;
    
    // Check if multiple entries are selected
    if (!selectedHistoryIDs_.empty() && selectedHistoryIDs_.size() > 1) {
        // Multi-selection mode - show count and bulk settings
        TextColored(ImVec4(0.8f, 0.8f, 1.0f, 1.0f), "%zu entries selected", selectedHistoryIDs_.size());
        Spacing();
        
        // Check if selection contains mixed types (topics and scenes)
        bool hasTopics = false;
        bool hasScenes = false;
        for (const auto& entryId : selectedHistoryIDs_) {
            for (const auto& entry : historyCache_) {
                if (entry.id == entryId) {
                    if (entry.isScene || entry.isBardSong) {
                        hasScenes = true;
                    } else {
                        hasTopics = true;
                    }
                    break;
                }
            }
        }
        
        // Show warning if mixed selection
        if (hasTopics && hasScenes) {
            TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), "Warning: Mixed selection (Topics and Scenes)");
            TextWrapped("Topics and Scenes have different filter categories. Select only one type to apply blocking settings.");
            return;
        }
        
        Separator();
        Spacing();
        
        Text("Block Settings:");
        Spacing();
        
        // Filter Category dropdown (disabled if SkyrimNet block or Whitelist is checked)
        Text("Type:");
        SameLine();
        if (isSkyrimNetLoaded_ && multiSkyrimNetOnly_) {
            // Show disabled dropdown with only SkyrimNet option
            const char* skyrimNetOption = "SkyrimNet";
            int skyrimNetIndex = 0;
            SetNextItemWidth(140.0f);
            BeginDisabled();
            Combo("##MultiFilterCategory", &skyrimNetIndex, &skyrimNetOption, 1);
            EndDisabled();
        } else if (multiWhiteList_) {
            // Show disabled dropdown with only Whitelist option
            const char* whitelistOption = "Whitelist";
            int whitelistIndex = 0;
            SetNextItemWidth(140.0f);
            BeginDisabled();
            Combo("##MultiFilterCategory", &whitelistIndex, &whitelistOption, 1);
            EndDisabled();
        } else {
            bool hasSceneOrBardSong = hasScenes;
        auto categories = GetFilterCategories(hasSceneOrBardSong);
        std::vector<const char*> categoryPtrs;
        for (const auto& cat : categories) {
            categoryPtrs.push_back(cat.c_str());
        }
        
        // Set default category based on selected entry types
        if (hasSceneOrBardSong) {
            // Check if all selected entries are bard songs
            bool allBardSongs = true;
            bool anyBardSongs = false;
            bool anyRegularScenes = false;
            for (const auto& entryId : selectedHistoryIDs_) {
                for (const auto& entry : historyCache_) {
                    if (entry.id == entryId) {
                        if (entry.isBardSong) {
                            anyBardSongs = true;
                        } else if (entry.isScene) {
                            anyRegularScenes = true;
                            allBardSongs = false;
                        } else {
                            allBardSongs = false;
                        }
                        break;
                    }
                }
            }
            
            // Set appropriate default
            if (allBardSongs && anyBardSongs) {
                // All are bard songs - default to BardSongs category
                multiFilterCategoryIndex_ = 2;  // BardSongs
                multiSelectedFilterCategory_ = "BardSongs";
            } else if (anyRegularScenes) {
                // Mixed or regular scenes - default to Scene category
                multiFilterCategoryIndex_ = 1;  // Scene
                multiSelectedFilterCategory_ = "Scene";
            }
            // Otherwise keep current selection
        }
        
        SetNextItemWidth(140.0f);
        if (Combo("##MultiFilterCategory", &multiFilterCategoryIndex_, categoryPtrs.data(), static_cast<int>(categoryPtrs.size()))) {
            multiSelectedFilterCategory_ = categories[multiFilterCategoryIndex_];
        }
        }
        
        Spacing();
        
        // Soft block checkbox
        Checkbox("Soft block", &multiSoftBlockEnabled_);
        if (multiSoftBlockEnabled_) {
            // Uncheck others for mutual exclusion
            multiHardBlock_ = false;
            multiSkyrimNetOnly_ = false;
            multiWhiteList_ = false;
        }
        if (IsItemHovered(ImGuiHoveredFlags_None)) {
            BeginTooltip();
            Text("Blocks audio/subtitles and SkyrimNet logging");
            EndTooltip();
        }
        

        
        // SkyrimNet block checkbox - only show if SkyrimNet.esp is loaded
        // Always disabled during multi-selection
        if (isSkyrimNetLoaded_) {
            BeginDisabled();
            Checkbox("SkyrimNet block", &multiSkyrimNetOnly_);
            if (multiSkyrimNetOnly_) {
                // Uncheck others for mutual exclusion
                multiSoftBlockEnabled_ = false;
                multiHardBlock_ = false;
                multiWhiteList_ = false;
            }
            if (IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                BeginTooltip();
                Text("SkyrimNet blocking not available during multi-selection");
                EndTooltip();
            }
            EndDisabled();
        }
        
        // Hard block checkbox
        Checkbox("Hard block", &multiHardBlock_);
        if (multiHardBlock_) {
            // Uncheck others for mutual exclusion
            multiSoftBlockEnabled_ = false;
            multiSkyrimNetOnly_ = false;
            multiWhiteList_ = false;
        }
        if (IsItemHovered(ImGuiHoveredFlags_None)) {
            BeginTooltip();
            Text("Block dialogues from playing entirely");
            EndTooltip();
        }
        
        Spacing();
        
        // Whitelist checkbox
        Checkbox("Whitelist", &multiWhiteList_);
        if (multiWhiteList_) {
            // Uncheck others for mutual exclusion
            multiSoftBlockEnabled_ = false;
            multiSkyrimNetOnly_ = false;
            multiHardBlock_ = false;
        }
        if (IsItemHovered(ImGuiHoveredFlags_None)) {
            BeginTooltip();
            Text("Remove selected entries from blacklist");
            EndTooltip();
        }
        
        Spacing();
        Separator();
        Spacing();
        
        // Apply button
        std::string buttonLabel = "Apply to " + std::to_string(selectedHistoryIDs_.size()) + " Selected";
        if (Button(buttonLabel.c_str(), ImVec2(-1, 0))) {
            try {
                auto db = DialogueDB::GetDatabase();
                
                // If whitelist is selected, don't add to blacklist
                if (multiWhiteList_) {
                    // Whitelist mode - collect IDs to remove from blacklist
                    std::vector<int64_t> idsToRemove;
                
                for (const auto& entryId : selectedHistoryIDs_) {
                    for (const auto& entry : historyCache_) {
                        if (entry.id == entryId) {
                            // Check if entry exists in blacklist
                            int64_t blacklistId = -1;
                            if ((entry.isScene || entry.isBardSong) && !entry.sceneEditorID.empty()) {
                                // All scenes (including bard songs) identified by scene EditorID
                                auto blacklist = db->GetBlacklist();
                                for (const auto& blEntry : blacklist) {
                                    if (blEntry.targetType == DialogueDB::BlacklistTarget::Scene && 
                                        blEntry.targetEditorID == entry.sceneEditorID) {
                                        blacklistId = blEntry.id;
                                        break;
                                    }
                                }
                            } else {
                                blacklistId = db->GetBlacklistEntryId(entry.topicFormID, entry.topicEditorID);
                            }
                            
                            if (blacklistId > 0) {
                                idsToRemove.push_back(blacklistId);
                            }
                            break;
                        }
                    }
                }
                
                // Batch remove - much faster than individual removes
                int removedCount = db->RemoveFromBlacklistBatch(idsToRemove);
                
                // Clear selection and refresh
                selectedHistoryIDs_.clear();
                hasSelection_ = false;
                selectedEntryID_ = -1;
                lastClickedHistoryIndex_ = -1;
                
                RefreshHistory();
                
                spdlog::info("[STFUMenu] Whitelisted {} entries (removed from blacklist)", removedCount);
            } else if (!multiSoftBlockEnabled_ && !multiHardBlock_) {
                // Nothing checked - whitelist (remove from blacklist)
                std::vector<int64_t> idsToRemove;
                
                for (const auto& entryId : selectedHistoryIDs_) {
                    for (const auto& entry : historyCache_) {
                        if (entry.id == entryId) {
                            int64_t blacklistId = -1;
                            if ((entry.isScene || entry.isBardSong) && !entry.sceneEditorID.empty()) {
                                // All scenes (including bard songs) identified by scene EditorID
                                auto blacklist = db->GetBlacklist();
                                for (const auto& blEntry : blacklist) {
                                    if (blEntry.targetType == DialogueDB::BlacklistTarget::Scene && 
                                        blEntry.targetEditorID == entry.sceneEditorID) {
                                        blacklistId = blEntry.id;
                                        break;
                                    }
                                }
                            } else {
                                blacklistId = db->GetBlacklistEntryId(entry.topicFormID, entry.topicEditorID);
                            }
                            
                            if (blacklistId > 0) {
                                idsToRemove.push_back(blacklistId);
                            }
                            break;
                        }
                    }
                }
                
                // Batch remove
                int removedCount = db->RemoveFromBlacklistBatch(idsToRemove);
                
                selectedHistoryIDs_.clear();
                hasSelection_ = false;
                selectedEntryID_ = -1;
                lastClickedHistoryIndex_ = -1;
                
                RefreshHistory();
                
                spdlog::info("[STFUMenu] Whitelisted {} entries (no block type selected)", removedCount);
            } else {
                // Blacklist mode - collect entries to add with specified settings
                
                // Determine block type
                DialogueDB::BlockType blockType;
                if (multiHardBlock_) {
                    blockType = DialogueDB::BlockType::Hard;
                } else if (multiSkyrimNetOnly_) {
                    blockType = DialogueDB::BlockType::SkyrimNet;
                } else {
                    blockType = DialogueDB::BlockType::Soft;
                }
                
                // Collect all entries to add
                std::vector<DialogueDB::BlacklistEntry> entriesToAdd;
                for (const auto& entryId : selectedHistoryIDs_) {
                    for (const auto& entry : historyCache_) {
                        if (entry.id == entryId) {
                            DialogueDB::BlacklistEntry blEntry;
                            blEntry.targetType = (entry.isScene || entry.isBardSong) ? DialogueDB::BlacklistTarget::Scene : DialogueDB::BlacklistTarget::Topic;
                            // All scenes (including bard songs) use scene ID
                            if (entry.isScene || entry.isBardSong) {
                                blEntry.targetFormID = 0;
                                blEntry.targetEditorID = entry.sceneEditorID;
                                blEntry.questEditorID = "";  // Scenes don't have quest context
                            } else {
                                blEntry.targetFormID = entry.topicFormID;
                                blEntry.targetEditorID = entry.topicEditorID;
                                blEntry.questEditorID = entry.questEditorID;  // For ESL-safe matching
                            }
                            // Use allResponses if available, otherwise wrap single responseText
                            if (!entry.allResponses.empty()) {
                                blEntry.responseText = DialogueDB::ResponsesToJson(entry.allResponses);
                            } else if (!entry.responseText.empty()) {
                                blEntry.responseText = DialogueDB::ResponsesToJson({entry.responseText});
                            }
                            blEntry.subtype = entry.topicSubtype;
                            blEntry.subtypeName = entry.topicSubtypeName;
                            blEntry.blockType = blockType;
                            // SkyrimNet-only blocks never block audio or subtitles
                            // Soft blocks always block both audio and subtitles
                            if (blockType == DialogueDB::BlockType::SkyrimNet) {
                                blEntry.blockAudio = false;
                                blEntry.blockSubtitles = false;
                            } else {
                                blEntry.blockAudio = true;
                                blEntry.blockSubtitles = true;
                            }
                            // Soft block always includes SkyrimNet blocking
                            blEntry.blockSkyrimNet = (blockType == DialogueDB::BlockType::Soft || blockType == DialogueDB::BlockType::SkyrimNet);
                            // SkyrimNet blocks use special "SkyrimNet" category, others use selected category
                            blEntry.filterCategory = (blockType == DialogueDB::BlockType::SkyrimNet) ? "SkyrimNet" : multiSelectedFilterCategory_;
                            blEntry.sourcePlugin = entry.topicSourcePlugin;  // Use topic source for blacklisting
                            
                            entriesToAdd.push_back(blEntry);
                            break;
                        }
                    }
                }
                
                // Batch add - much faster than individual adds
                int addedCount = db->AddToBlacklistBatch(entriesToAdd, false);
                
                // Clear selection and refresh
                selectedHistoryIDs_.clear();
                hasSelection_ = false;
                selectedEntryID_ = -1;
                lastClickedHistoryIndex_ = -1;
                
                RefreshHistory();
                
                spdlog::info("[STFUMenu] Bulk added {} entries to blacklist with settings (type=%d)", addedCount, static_cast<int>(blockType));
            }
            } catch (const std::exception& e) {
                spdlog::error("[STFUMenu] Exception during history multi-selection operation: {}", e.what());
            }
        }
        
        if (IsItemHovered(ImGuiHoveredFlags_None)) {
            BeginTooltip();
            Text("Add all selected entries to blacklist with the specified settings");
            EndTooltip();
        }
        
        return;
    }
    
    if (!hasSelection_) {
        TextColored(ImVec4(0.8f, 0.8f, 1.0f, 1.0f), "Multi-Selection Controls:");
        Spacing();
        TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "  Click - Select single entry");
        TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "  Ctrl+Click - Add/remove entry from selection");
        TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "  Shift+Click - Select range from last clicked to current");
        Spacing();
        Separator();
        Spacing();
        TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Select an entry above to configure blocking settings.");
        return;
    }
    
    // Get available width for split layout
    ImVec2 availSpace;
    GetContentRegionAvail(&availSpace);
    float leftWidth = availSpace.x * 0.35f;
    float rightWidth = availSpace.x * 0.65f - 10.0f;
    
    // Left side: Block settings
    if (BeginChild("BlockSettings", ImVec2(leftWidth, 0), ImGuiChildFlags_Border, ImGuiWindowFlags_None)) {
        Text("Topic: %s (0%X)", selectedEntry_.topicEditorID.c_str(), selectedEntry_.topicFormID);
        Text("Subtype: %s", selectedEntry_.topicSubtypeName.c_str());
        
        Separator();
        Spacing();
        
        Text("Block Settings:");
        Spacing();
        
        // Filter Category dropdown (disabled if SkyrimNet block or Whitelist is checked)
        Text("Type:");
        SameLine();
        if (whiteList_) {
            // Show disabled dropdown with only Whitelist option
            const char* whitelistOption = "Whitelist";
            int whitelistIndex = 0;
            SetNextItemWidth(140.0f);
            BeginDisabled();
            Combo("##FilterCategory", &whitelistIndex, &whitelistOption, 1);
            EndDisabled();
        } else if (isSkyrimNetLoaded_ && skyrimNetOnly_) {
            // Show disabled dropdown with only SkyrimNet option
            const char* skyrimNetOption = "SkyrimNet";
            int skyrimNetIndex = 0;
            SetNextItemWidth(140.0f);
            BeginDisabled();
            Combo("##FilterCategory", &skyrimNetIndex, &skyrimNetOption, 1);
            EndDisabled();
        } else {
            auto categories = GetFilterCategories(selectedEntry_.isScene);
        std::vector<const char*> categoryPtrs;
        for (const auto& cat : categories) {
            categoryPtrs.push_back(cat.c_str());
        }
        if (Combo("##FilterCategory", &filterCategoryIndex_, categoryPtrs.data(), static_cast<int>(categoryPtrs.size()))) {
            selectedFilterCategory_ = categories[filterCategoryIndex_];
        }
        }
        
        Spacing();
        
        // Soft block checkbox
        bool softBlockChanged = Checkbox("Soft block", &softBlockEnabled_);
        if (softBlockChanged && softBlockEnabled_) {
            // Uncheck others for mutual exclusion
            hardBlock_ = false;
            skyrimNetOnly_ = false;
            whiteList_ = false;
        }
        if (IsItemHovered(ImGuiHoveredFlags_None)) {
            BeginTooltip();
            Text("Blocks audio/subtitles and SkyrimNet logging");
            EndTooltip();
        }
        

        
        // SkyrimNet block checkbox - only show if SkyrimNet.esp is loaded
        if (isSkyrimNetLoaded_) {
            // Disable if not compatible (not menu dialogue)
            bool skyrimNetDisabled = !selectedEntry_.skyrimNetBlockable;
            if (skyrimNetDisabled) {
                BeginDisabled();
            }
            bool skyrimNetChanged = Checkbox("SkyrimNet block", &skyrimNetOnly_);
            if (skyrimNetChanged && skyrimNetOnly_) {
                // Uncheck others for mutual exclusion
                softBlockEnabled_ = false;
                hardBlock_ = false;
                whiteList_ = false;
            }
            if (IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                BeginTooltip();
                if (!selectedEntry_.skyrimNetBlockable) {
                    Text("SkyrimNet blocking not available for this dialogue type");
                    Text("\nSkyrimNet blocking only works for:");
                    Text("  - Menu-based dialogue topics");
                    Text("\nBackground dialogue (combat barks, idle chatter, scenes)");
                    Text("cannot be blocked for SkyrimNet only.");
                } else {
                    Text("Block dialogue from being logged by SkyrimNet");
                }
                EndTooltip();
            }
            if (skyrimNetDisabled) {
                EndDisabled();
                if (!selectedEntry_.skyrimNetBlockable) {
                    skyrimNetOnly_ = false;  // Auto-disable if not compatible
                }
            }
        }
        
        // Hard block checkbox
        bool hardBlockChanged = Checkbox("Hard block", &hardBlock_);
        if (hardBlockChanged && hardBlock_) {
            // Uncheck others for mutual exclusion
            softBlockEnabled_ = false;
            skyrimNetOnly_ = false;
            whiteList_ = false;
        }
        if (IsItemHovered(ImGuiHoveredFlags_None)) {
            BeginTooltip();
            Text("Block dialogue from playing entirely");
            EndTooltip();
        }
        
        Spacing();
        
        // Whitelist checkbox
        bool whitelistChanged = Checkbox("Whitelist", &whiteList_);
        if (whitelistChanged && whiteList_) {
            // Uncheck others for mutual exclusion
            softBlockEnabled_ = false;
            skyrimNetOnly_ = false;
            hardBlock_ = false;
        }
        if (IsItemHovered(ImGuiHoveredFlags_None)) {
            BeginTooltip();
            Text("Remove this entry from blacklist");
            EndTooltip();
        }
        
        Spacing();
        Separator();
        Spacing();
        
        // Apply button
        if (Button("Apply Changes", ImVec2(-1, 0))) {
            ApplyBlockSettings();
        }
        
        if (IsItemHovered(ImGuiHoveredFlags_None)) {
            BeginTooltip();
            Text("Add or update blacklist entry for this dialogue");
            EndTooltip();
        }
    }
    EndChild();
    
    SameLine();
    
    // Right side: Detailed info
    if (BeginChild("DetailInfo", ImVec2(rightWidth, 0), ImGuiChildFlags_Border, ImGuiWindowFlags_None)) {
        TextColored(ImVec4(0.8f, 0.8f, 1.0f, 1.0f), "Detailed Information");
        Separator();
        
        Text("Quest: %s (0%X)", 
             selectedEntry_.questName.empty() ? selectedEntry_.questEditorID.c_str() : selectedEntry_.questName.c_str(),
             selectedEntry_.questFormID);
        
        // Topic info
        if (selectedEntry_.isScene) {
            if (!selectedEntry_.sceneEditorID.empty()) {
                Text("Scene: %s", selectedEntry_.sceneEditorID.c_str());
            }
        } else {
            if (!selectedEntry_.topicEditorID.empty() && selectedEntry_.topicFormID > 0) {
                Text("Topic: %s (0%X)", selectedEntry_.topicEditorID.c_str(), selectedEntry_.topicFormID);
            } else if (!selectedEntry_.topicEditorID.empty()) {
                Text("Topic: %s", selectedEntry_.topicEditorID.c_str());
            } else if (selectedEntry_.topicFormID > 0) {
                Text("Topic: 0%X", selectedEntry_.topicFormID);
            }
        }
        
        // Subtype info
        if (selectedEntry_.topicSubtype > 0 && !selectedEntry_.topicSubtypeName.empty()) {
            Text("Subtype: %s", selectedEntry_.topicSubtypeName.c_str());
        }
        
        if (!selectedEntry_.sourcePlugin.empty()) {
            Text("Source: %s", selectedEntry_.sourcePlugin.c_str());
        }
        
        Spacing();
        
        Text("Speaker: %s", selectedEntry_.speakerName.c_str());
        Text("Speaker FormID: 0%X (Base: 0%X)", 
             selectedEntry_.speakerFormID, selectedEntry_.speakerBaseFormID);
        
        Spacing();
        
        // Check if entry is whitelisted - if so, override status display
        auto db = DialogueDB::GetDatabase();
        bool isWhitelisted = false;
        if (db) {
            if (selectedEntry_.isScene && !selectedEntry_.sceneEditorID.empty()) {
                isWhitelisted = db->IsWhitelisted(DialogueDB::BlacklistTarget::Scene, 0, selectedEntry_.sceneEditorID);
            } else {
                isWhitelisted = db->IsWhitelisted(DialogueDB::BlacklistTarget::Topic, selectedEntry_.topicFormID, selectedEntry_.topicEditorID);
            }
        }
        
        ImU32 statusColor;
        std::string detailedStatusText;
        if (isWhitelisted) {
            statusColor = COLOR_WHITELIST;
            detailedStatusText = "Whitelisted - Never blocked";
        } else {
            statusColor = GetStatusColor(selectedEntry_.blockedStatus);
            detailedStatusText = GetDetailedStatusText(selectedEntry_.blockedStatus);
        }
        
        ImVec4 statusColorVec = ImVec4(
            ((statusColor >> IM_COL32_R_SHIFT) & 0xFF) / 255.0f,
            ((statusColor >> IM_COL32_G_SHIFT) & 0xFF) / 255.0f,
            ((statusColor >> IM_COL32_B_SHIFT) & 0xFF) / 255.0f,
            ((statusColor >> IM_COL32_A_SHIFT) & 0xFF) / 255.0f
        );
        Text("Status: "); SameLine();
        TextColored(statusColorVec, "%s", detailedStatusText.c_str());
        
        Spacing();
        Separator();
        Spacing();
        
        // Show All Responses button - displays all responses captured at log time
        // Show button if we have ANY topic/scene identifier (EditorID or FormID)
        bool hasTopicOrScene = !selectedEntry_.topicEditorID.empty() || 
                               !selectedEntry_.sceneEditorID.empty() ||
                               selectedEntry_.topicFormID > 0;
        
        if (hasTopicOrScene) {
            if (Button("Show All Responses", ImVec2(-1, 0))) {
                spdlog::info("[STFUMenu] Show All Responses clicked: entry has {} allResponses, responseText='{}'", 
                    selectedEntry_.allResponses.size(), 
                    selectedEntry_.responseText.empty() ? "(empty)" : selectedEntry_.responseText.substr(0, 50));
                
                // Use allResponses from dialogue entry (captured at log time)
                std::string lookupID = selectedEntry_.isScene ? selectedEntry_.sceneEditorID : selectedEntry_.topicEditorID;
                if (lookupID.empty()) {
                    char formIDStr[32];
                    sprintf_s(formIDStr, "0%X", selectedEntry_.topicFormID);
                    lookupID = formIDStr;
                }
                
                // For scenes, aggregate responses from all topics in the scene
                if (selectedEntry_.isScene && !selectedEntry_.sceneEditorID.empty()) {
                    std::vector<std::string> aggregatedResponses;
                    std::unordered_set<std::string> seenResponses;
                    
                    // Collect all scene entries and sort by topicFormID descending (scene order)
                    std::vector<const DialogueDB::DialogueEntry*> sceneEntries;
                    for (const auto& entry : historyCache_) {
                        if (entry.isScene && entry.sceneEditorID == selectedEntry_.sceneEditorID) {
                            sceneEntries.push_back(&entry);
                        }
                    }
                    
                    // Sort by FormID descending (scenes use descending order)
                    std::sort(sceneEntries.begin(), sceneEntries.end(), 
                        [](const DialogueDB::DialogueEntry* a, const DialogueDB::DialogueEntry* b) {
                            return a->topicFormID > b->topicFormID;
                        });
                    
                    // Aggregate unique responses in correct order
                    for (const auto* entry : sceneEntries) {
                        for (const auto& response : entry->allResponses) {
                            if (seenResponses.insert(response).second) {
                                // Response was not seen before, add to aggregated list
                                aggregatedResponses.push_back(response);
                            }
                        }
                    }
                    
                    if (!aggregatedResponses.empty()) {
                        popupResponses_ = aggregatedResponses;
                        popupTitle_ = "All Scene Responses: " + lookupID;
                        showResponsesPopup_ = true;
                        spdlog::info("[STFUMenu] Showing {} unique responses from {} topics in scene", 
                            aggregatedResponses.size(), sceneEntries.size());
                    } else {
                        // Fallback to single entry responses
                        popupResponses_ = selectedEntry_.allResponses.empty() ? 
                            std::vector<std::string>{selectedEntry_.responseText} : 
                            selectedEntry_.allResponses;
                        popupTitle_ = "Scene Responses: " + lookupID;
                        showResponsesPopup_ = true;
                        spdlog::warn("[STFUMenu] No aggregated scene responses found, showing single entry");
                    }
                } else if (!selectedEntry_.allResponses.empty()) {
                    // Use captured responses from dialogue log (topic)
                    popupResponses_ = selectedEntry_.allResponses;
                    std::string typeStr = selectedEntry_.isScene ? "Scene" : "Topic";
                    popupTitle_ = "All " + typeStr + " Responses: " + lookupID;
                    showResponsesPopup_ = true;
                    spdlog::info("[STFUMenu] Showing {} captured responses in popup", popupResponses_.size());
                } else if (!selectedEntry_.responseText.empty()) {
                    // Old entry or extraction failed - show the single captured response
                    popupResponses_ = {selectedEntry_.responseText};
                    popupTitle_ = "Response - " + lookupID;
                    showResponsesPopup_ = true;
                    spdlog::info("[STFUMenu] Showing single response in popup (allResponses was empty)");
                } else {
                    // No responses available
                    popupResponses_.clear();
                    popupTitle_ = "No Responses Captured - " + lookupID;
                    showResponsesPopup_ = true;
                    spdlog::warn("[STFUMenu] No responses available for entry");
                }
            }
            if (IsItemHovered(ImGuiHoveredFlags_None)) {
                BeginTooltip();
                std::string typeStr = selectedEntry_.isScene ? "scene" : "topic";
                Text("View all responses for this %s", typeStr.c_str());
                EndTooltip();
            }
        }
        
        Spacing();
        Separator();
        Spacing();
        
        // Notes textbox (replaces Copy buttons)
        Text("Notes:");
        if (IsItemHovered(ImGuiHoveredFlags_None)) {
            BeginTooltip();
            Text("Add a note to this dialogue entry.");
            Text("This note will be saved when you add the entry to the blacklist.");
            EndTooltip();
        }
        InputTextMultiline("##HistoryNotes", historyNotes_, sizeof(historyNotes_), ImVec2(-1, 80), ImGuiInputTextFlags_None, nullptr, nullptr);
    }
    EndChild();
}

void STFUMenu::RenderBlacklistTab()
{
    using namespace ImGuiMCP;
    
    // Load blacklist on first view (first 100 entries)
    static bool firstLoad = true;
    if (firstLoad) {
        blacklistCache_ = DialogueDB::GetDatabase()->GetBlacklist();
        blacklistLoadedCount_ = blacklistPageSize_;
        firstLoad = false;
    }
    
    // Get available space
    ImVec2 availSpace;
    GetContentRegionAvail(&availSpace);
    
    // Top panel: Compact search bar (3 lines, no header)
    if (BeginChild("SearchPanel", ImVec2(0, 151), ImGuiChildFlags_Border, ImGuiWindowFlags_None)) {
        // Line 1: Search field and Type filter
        PushItemWidth(400);
        bool searchTriggered = InputText("##SearchQuery", searchQuery_, sizeof(searchQuery_), ImGuiInputTextFlags_EnterReturnsTrue, nullptr, nullptr);
        PopItemWidth();
        SameLine();
        Text("Type:");
        SameLine();
        PushItemWidth(150);
        const char* types[] = { "All", "Topic", "Quest", "Subtype", "Scene", "Bard Song", "Follower" };
        Combo("##FilterType", &filterTargetType_, types, 7);
        PopItemWidth();
        
        // Line 2: Block type checkboxes
        Text("Filters:");
        SameLine();
        bool softChanged = Checkbox("Soft block", &filterSoftBlock_);
        SameLine();
        bool hardChanged = Checkbox("Hard block", &filterHardBlock_);
        bool skyrimNetChanged = false;
        if (isSkyrimNetLoaded_) {
            SameLine();
            skyrimNetChanged = Checkbox("SkyrimNet block", &filterSkyrimNet_);
        }
        SameLine();
        bool topicChanged = Checkbox("Topics", &filterTopic_);
        SameLine();
        bool sceneChanged = Checkbox("Scenes", &filterScene_);
        
        // Trigger search if any filter checkbox changed
        bool filterChanged = softChanged || hardChanged || skyrimNetChanged || topicChanged || sceneChanged;
        
        // Line 3: Action buttons
        if (Button("Search", ImVec2(120, 0)) || searchTriggered || filterChanged) {
            // Perform search on unified blacklist (includes scenes)
            auto allBlacklist = DialogueDB::GetDatabase()->GetBlacklist();
            
            std::vector<DialogueDB::BlacklistEntry> filteredBlacklist;
            
            std::string query = searchQuery_;
            std::transform(query.begin(), query.end(), query.begin(), ::tolower);
            
            // Filter unified blacklist
            for (const auto& entry : allBlacklist) {
                bool matches = true;
                
                // Search query (checks EditorID, FormID as hex, response text, subtype, and notes)
                if (query.length() > 0) {
                    bool foundMatch = false;
                    
                    // Check EditorID
                    std::string editorID = entry.targetEditorID;
                    std::transform(editorID.begin(), editorID.end(), editorID.begin(), ::tolower);
                    if (editorID.find(query) != std::string::npos) {
                        foundMatch = true;
                    }
                    
                    // Check response text (most user-friendly)
                    if (!foundMatch && !entry.responseText.empty()) {
                        std::string responseText = entry.responseText;
                        std::transform(responseText.begin(), responseText.end(), responseText.begin(), ::tolower);
                        if (responseText.find(query) != std::string::npos) {
                            foundMatch = true;
                        }
                    }
                    
                    // Check subtype name
                    if (!foundMatch && !entry.subtypeName.empty()) {
                        std::string subtypeName = entry.subtypeName;
                        std::transform(subtypeName.begin(), subtypeName.end(), subtypeName.begin(), ::tolower);
                        if (subtypeName.find(query) != std::string::npos) {
                            foundMatch = true;
                        }
                    }
                    
                    // Check notes field (allows user-created tags/filters)
                    if (!foundMatch && !entry.notes.empty()) {
                        std::string notes = entry.notes;
                        std::transform(notes.begin(), notes.end(), notes.begin(), ::tolower);
                        if (notes.find(query) != std::string::npos) {
                            foundMatch = true;
                        }
                    }
                    
                    // Check FormID (strip leading zeroes from both)
                    if (!foundMatch && entry.targetFormID > 0) {
                        char formIDStr[16];
                        sprintf_s(formIDStr, "%X", entry.targetFormID);
                        std::string dbFormID = formIDStr;
                        std::string searchFormID = query;
                        
                        // Remove 0x prefix
                        if (searchFormID.find("0x") == 0) searchFormID = searchFormID.substr(2);
                        
                        // Strip leading zeroes
                        auto stripZeroes = [](std::string& s) {
                            size_t pos = s.find_first_not_of('0');
                            if (pos != std::string::npos) s = s.substr(pos);
                            else s = "0";
                        };
                        stripZeroes(searchFormID);
                        stripZeroes(dbFormID);
                        
                        std::transform(searchFormID.begin(), searchFormID.end(), searchFormID.begin(), ::tolower);
                        std::transform(dbFormID.begin(), dbFormID.end(), dbFormID.begin(), ::tolower);
                        
                        if (dbFormID.find(searchFormID) != std::string::npos) {
                            foundMatch = true;
                        }
                    }
                    
                    if (!foundMatch) {
                        matches = false;
                    }
                }
                
                // Filter by target type
                if (matches && filterTargetType_ > 0) {
                    if (filterTargetType_ == 4) {
                        // Scene filter - exclude bard songs and follower commentary
                        if (entry.targetType != DialogueDB::BlacklistTarget::Scene || 
                            entry.filterCategory == "BardSongs" || 
                            entry.filterCategory == "FollowerCommentary") {
                            matches = false;
                        }
                    } else if (filterTargetType_ == 5) {
                        // Bard Song filter
                        if (entry.filterCategory != "BardSongs") {
                            matches = false;
                        }
                    } else if (filterTargetType_ == 6) {
                        // Follower Commentary filter
                        if (entry.filterCategory != "FollowerCommentary") {
                            matches = false;
                        }
                    } else if (filterTargetType_ <= 3) {
                        // Topic, Quest, Subtype
                        if (static_cast<int>(entry.targetType) != filterTargetType_) {
                            matches = false;
                        }
                    }
                }
                
                // Filter by block type
                if (matches) {
                    bool blockTypeMatch = false;
                    if (filterSoftBlock_ && entry.blockType == DialogueDB::BlockType::Soft) blockTypeMatch = true;
                    if (filterHardBlock_ && entry.blockType == DialogueDB::BlockType::Hard) blockTypeMatch = true;
                    if (filterSkyrimNet_ && entry.blockType == DialogueDB::BlockType::SkyrimNet) blockTypeMatch = true;
                    if (!blockTypeMatch) matches = false;
                }
                
                // Filter by target type (topics only)
                if (matches && !filterTopic_) {
                    if (entry.targetType == DialogueDB::BlacklistTarget::Topic) {
                        matches = false;
                    }
                }
                
                // Filter by target type (all scenes including bard songs)
                if (matches && !filterScene_) {
                    if (entry.targetType == DialogueDB::BlacklistTarget::Scene) {
                        matches = false;
                    }
                }
                
                if (matches) {
                    filteredBlacklist.push_back(entry);
                }
            }
            
            blacklistCache_ = filteredBlacklist;
            blacklistLoadedCount_ = static_cast<int>(blacklistCache_.size());
            
            spdlog::info("[STFUMenu] Blacklist search returned {} entries", blacklistCache_.size());
        }
        
        SameLine();
        
        if (Button("Reset Filters", ImVec2(120, 0))) {
            searchQuery_[0] = '\0';
            filterTargetType_ = 0;
            filterSoftBlock_ = true;
            filterHardBlock_ = true;
            filterSkyrimNet_ = true;
            filterTopic_ = true;
            filterScene_ = true;
            blacklistCache_ = DialogueDB::GetDatabase()->GetBlacklist();
            blacklistLoadedCount_ = blacklistPageSize_;
        }
        
        SameLine();
        
        if (Button("Refresh", ImVec2(120, 0))) {
            auto allBlacklist = DialogueDB::GetDatabase()->GetBlacklist();
            
            // Apply current filters to the refreshed list
            std::vector<DialogueDB::BlacklistEntry> filteredBlacklist;
            for (const auto& entry : allBlacklist) {
                bool matches = true;
                
                // Apply search query filter
                if (searchQuery_[0] != '\0') {
                    std::string query = searchQuery_;
                    std::transform(query.begin(), query.end(), query.begin(), ::tolower);
                    
                    std::string editorID = entry.targetEditorID;
                    std::transform(editorID.begin(), editorID.end(), editorID.begin(), ::tolower);
                    
                    bool foundMatch = false;
                    if (editorID.find(query) != std::string::npos) {
                        foundMatch = true;
                    }
                    
                    if (!foundMatch) {
                        std::string responseText = entry.responseText;
                        std::transform(responseText.begin(), responseText.end(), responseText.begin(), ::tolower);
                        if (responseText.find(query) != std::string::npos) {
                            foundMatch = true;
                        }
                    }
                    
                    if (!foundMatch) {
                        matches = false;
                    }
                }
                
                // Filter by target type
                if (matches && filterTargetType_ > 0) {
                    if (filterTargetType_ == 4) {
                        // Scene filter - exclude bard songs and follower commentary
                        if (entry.targetType != DialogueDB::BlacklistTarget::Scene || 
                            entry.filterCategory == "BardSongs" || 
                            entry.filterCategory == "FollowerCommentary") {
                            matches = false;
                        }
                    } else if (filterTargetType_ == 5) {
                        // Bard Song filter
                        if (entry.filterCategory != "BardSongs") {
                            matches = false;
                        }
                    } else if (filterTargetType_ == 6) {
                        // Follower Commentary filter
                        if (entry.filterCategory != "FollowerCommentary") {
                            matches = false;
                        }
                    } else if (filterTargetType_ <= 3) {
                        // Topic, Quest, Subtype
                        if (static_cast<int>(entry.targetType) != filterTargetType_) {
                            matches = false;
                        }
                    }
                }
                
                // Filter by block type
                if (matches) {
                    bool blockTypeMatch = false;
                    if (filterSoftBlock_ && entry.blockType == DialogueDB::BlockType::Soft) blockTypeMatch = true;
                    if (filterHardBlock_ && entry.blockType == DialogueDB::BlockType::Hard) blockTypeMatch = true;
                    if (filterSkyrimNet_ && entry.blockType == DialogueDB::BlockType::SkyrimNet) blockTypeMatch = true;
                    if (!blockTypeMatch) matches = false;
                }
                
                // Filter by target type (topics only)
                if (matches && !filterTopic_) {
                    if (entry.targetType == DialogueDB::BlacklistTarget::Topic) {
                        matches = false;
                    }
                }
                
                // Filter by target type (all scenes including bard songs)
                if (matches && !filterScene_) {
                    if (entry.targetType == DialogueDB::BlacklistTarget::Scene) {
                        matches = false;
                    }
                }
                
                if (matches) {
                    filteredBlacklist.push_back(entry);
                }
            }
            
            blacklistCache_ = filteredBlacklist;
            blacklistLoadedCount_ = static_cast<int>(blacklistCache_.size());
        }
        
        SameLine();
        Text("Results: %zu", blacklistCache_.size());
        
        // Manual Entry button on far right
        SameLine();
        ImVec2 availWidth;
        GetContentRegionAvail(&availWidth);
        float buttonWidth = 150.0f;
        SetCursorPosX(GetCursorPosX() + availWidth.x - buttonWidth);
        
        if (Button("Manual Entry", ImVec2(buttonWidth, 0))) {
            showManualEntryModal_ = true;
            isManualEntryForWhitelist_ = false;  // Blacklist
            manualEntryIdentifier_[0] = '\0';
            manualEntryBlockType_ = 0;
            manualEntryBlockAudio_ = true;
            manualEntryBlockSubtitles_ = true;
            manualEntryBlockSkyrimNet_ = true;
            manualEntryNotes_[0] = '\0';
            manualEntryFilterCategoryIndex_ = 0;
        }
    }
    EndChild();
    
    Separator();
    
    // Middle panel: Results list (40% of remaining height to avoid main scrollbar)
    float remainingHeight = availSpace.y - 165.0f;  // Subtract search panel height + padding
    float resultsHeight = remainingHeight * 0.40f;
    float detailHeight = remainingHeight * 0.60f - 10.0f;
    
    if (BeginChild("BlacklistResults", ImVec2(0, resultsHeight), ImGuiChildFlags_Border, ImGuiWindowFlags_None)) {
        int displayCount = 0;
        int maxDisplay = blacklistLoadedCount_;
        
        // Render blacklist entries (unified list including scenes)
        int entryIndex = 0;
        for (const auto& entry : blacklistCache_) {
            if (displayCount >= maxDisplay) break;
            displayCount++;
            
            PushID(static_cast<int>(entry.id));
            
            // Format: [Target Type][Block Type][EditorID or FormID]
            const char* targetTypeStr = "";
            ImVec4 typeColor = ImVec4(0.4f, 0.8f, 1.0f, 1.0f);  // Default cyan
            switch (entry.targetType) {
                case DialogueDB::BlacklistTarget::Topic:   targetTypeStr = "Topic"; break;
                case DialogueDB::BlacklistTarget::Quest:   targetTypeStr = "Quest"; break;
                case DialogueDB::BlacklistTarget::Subtype: targetTypeStr = "Subtype"; break;
                case DialogueDB::BlacklistTarget::Scene:
                    targetTypeStr = "Scene";
                    typeColor = ImVec4(0.4f, 1.0f, 0.8f, 1.0f);  // Teal for scenes
                    break;
                case DialogueDB::BlacklistTarget::Plugin:
                    targetTypeStr = "Plugin";
                    typeColor = ImVec4(1.0f, 0.8f, 0.4f, 1.0f);  // Orange for plugins
                    break;
            }
            
            // Map BlockType to BlockedStatus for consistent tag rendering
            DialogueDB::BlockedStatus statusForTag;
            switch (entry.blockType) {
                case DialogueDB::BlockType::Soft:
                    statusForTag = DialogueDB::BlockedStatus::SoftBlock;
                    break;
                case DialogueDB::BlockType::Hard:
                    statusForTag = DialogueDB::BlockedStatus::HardBlock;
                    break;
                case DialogueDB::BlockType::SkyrimNet:
                    statusForTag = DialogueDB::BlockedStatus::SkyrimNetBlock;
                    break;
            }
            
            // Get tag text and color (consistent with history tab)
            std::string statusStr = GetStatusText(statusForTag);
            ImU32 statusColor = GetStatusColor(statusForTag);
            ImVec4 statusColorVec = ImVec4(
                ((statusColor >> IM_COL32_R_SHIFT) & 0xFF) / 255.0f,
                ((statusColor >> IM_COL32_G_SHIFT) & 0xFF) / 255.0f,
                ((statusColor >> IM_COL32_B_SHIFT) & 0xFF) / 255.0f,
                ((statusColor >> IM_COL32_A_SHIFT) & 0xFF) / 255.0f
            );
            
            // Use invisible selectable for click detection and highlighting
            ImVec2 cursorPos;
            GetCursorScreenPos(&cursorPos);
            
            // Check if this entry is selected (either single or multi)
            bool isSelected = false;
            if (!selectedBlacklistIDs_.empty()) {
                // Multi-selection mode
                for (size_t i = 0; i < selectedBlacklistIDs_.size(); ++i) {
                    if (selectedBlacklistIDs_[i] == entry.id) {
                        isSelected = true;
                        break;
                    }
                }
            } else {
                // Single selection mode (legacy)
                isSelected = (selectedBlacklistID_ == entry.id);
            }
            
            if (Selectable("##blacklistentry", isSelected, ImGuiSelectableFlags_AllowOverlap, ImVec2(0, 0))) {
                ImGuiIO* io = GetIO();
                bool ctrlPressed = io->KeyCtrl;
                bool shiftPressed = io->KeyShift;
                
                if (shiftPressed && lastClickedIndex_ >= 0) {
                    // Shift+click: Select range
                    selectedBlacklistIDs_.clear();
                    
                    int start = (std::min)(lastClickedIndex_, entryIndex);
                    int end = (std::max)(lastClickedIndex_, entryIndex);
                    
                    int idx = 0;
                    for (const auto& e : blacklistCache_) {
                        if (idx >= start && idx <= end) {
                            selectedBlacklistIDs_.push_back(e.id);
                        }
                        idx++;
                    }
                    
                    hasBlacklistSelection_ = false;  // Multi-selection, no single entry details
                } else if (ctrlPressed) {
                    // Ctrl+click: Toggle selection
                    bool found = false;
                    for (size_t i = 0; i < selectedBlacklistIDs_.size(); ++i) {
                        if (selectedBlacklistIDs_[i] == entry.id) {
                            // Already selected, remove it
                            selectedBlacklistIDs_.erase(selectedBlacklistIDs_.begin() + i);
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        // Not selected, add it
                        selectedBlacklistIDs_.push_back(entry.id);
                    }
                    
                    lastClickedIndex_ = entryIndex;
                    hasBlacklistSelection_ = false;  // Multi-selection
                } else {
                    // Normal click: Single selection
                    selectedBlacklistIDs_.clear();
                    selectedBlacklistIDs_.push_back(entry.id);
                    
                    selectedBlacklistID_ = entry.id;
                    selectedBlacklistEntry_ = entry;
                    hasBlacklistSelection_ = true;
                    lastClickedIndex_ = entryIndex;
                    
                    // Load notes into edit buffer
                    strncpy_s(blacklistNotes_, entry.notes.c_str(), sizeof(blacklistNotes_) - 1);
                    blacklistNotes_[sizeof(blacklistNotes_) - 1] = '\0';
                    
                    spdlog::info("[STFUMenu] Selected blacklist entry ID: {}", entry.id);
                }
            }
            
            // Draw colored text on top
            SetCursorScreenPos(cursorPos);
            
            TextColored(typeColor, "[%s]", targetTypeStr);
            SameLine();
            TextColored(statusColorVec, "[%s]", statusStr.c_str());
            SameLine();
            
            // Show EditorID or FormID
            if (!entry.targetEditorID.empty()) {
                TextColored(ImVec4(1.0f, 1.0f, 0.4f, 1.0f), "%s", entry.targetEditorID.c_str());
            } else {
                TextColored(ImVec4(1.0f, 1.0f, 0.4f, 1.0f), "0%X", entry.targetFormID);
            }
            
            // Show notes if any (takes priority)
            if (!entry.notes.empty()) {
                SameLine();
                TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), " - %s", entry.notes.c_str());
            }
            // Show first response as preview if no notes (parse JSON array)
            else if (!entry.responseText.empty()) {
                auto responses = ParseResponsesJson(entry.responseText);
                if (!responses.empty()) {
                    SameLine();
                    TextColored(ImVec4(0.9f, 0.9f, 0.9f, 1.0f), " - %s", responses[0].c_str());
                }
            }
            PopID();
            entryIndex++;
        }
        
        // Check if scrolled to bottom - load more if needed
        float scrollY = GetScrollY();
        float scrollMaxY = GetScrollMaxY();
        if (scrollMaxY > 0 && (scrollMaxY - scrollY) < 50.0f) {
            // Near bottom, load more
            int totalEntries = static_cast<int>(blacklistCache_.size());
            if (blacklistLoadedCount_ < totalEntries) {
                int newCount = blacklistLoadedCount_ + blacklistPageSize_;
                blacklistLoadedCount_ = (newCount < totalEntries) ? newCount : totalEntries;
            }
        }
        
        // Handle Delete key to remove selected entries
        if (IsKeyPressed(ImGuiKey_Delete, false)) {
            if (!selectedBlacklistIDs_.empty()) {
                // Delete multiple selected entries
                int deletedCount = DialogueDB::GetDatabase()->RemoveFromBlacklistBatch(selectedBlacklistIDs_);
                if (deletedCount > 0) {
                    spdlog::info("[STFUMenu] Deleted {} blacklist entries", deletedCount);
                    selectedBlacklistIDs_.clear();
                    selectedBlacklistID_ = -1;
                    hasBlacklistSelection_ = false;
                    lastClickedIndex_ = -1;
                    needsBlacklistRefresh_ = true;
                }
            } else if (selectedBlacklistID_ != -1) {
                // Delete single selected entry
                if (DialogueDB::GetDatabase()->RemoveFromBlacklist(selectedBlacklistID_)) {
                    spdlog::info("[STFUMenu] Deleted blacklist entry {}", selectedBlacklistID_);
                    selectedBlacklistID_ = -1;
                    hasBlacklistSelection_ = false;
                    lastClickedIndex_ = -1;
                    needsBlacklistRefresh_ = true;
                }
            }
        }
    }
    EndChild();
    
    Separator();
    
    // Bottom panel: Detail panel
    if (BeginChild("BlacklistDetailPanel", ImVec2(0, detailHeight), ImGuiChildFlags_Border, ImGuiWindowFlags_None)) {
        RenderBlacklistDetailPanel();
    }
    EndChild();
}

void STFUMenu::RenderBlacklistDetailPanel()
{
    using namespace ImGuiMCP;
    
    // Check if multiple entries are selected
    if (!selectedBlacklistIDs_.empty() && selectedBlacklistIDs_.size() > 1) {
        // Multi-selection mode - show controls similar to history tab
        TextColored(ImVec4(0.8f, 0.8f, 1.0f, 1.0f), "%zu entries selected", selectedBlacklistIDs_.size());
        Spacing();
        
        // Check if selection contains mixed types (topics and scenes)
        bool hasTopics = false;
        bool hasScenes = false;
        for (const auto& entryId : selectedBlacklistIDs_) {
            for (const auto& entry : blacklistCache_) {
                if (entry.id == entryId) {
                    if (entry.targetType == DialogueDB::BlacklistTarget::Scene) {
                        hasScenes = true;
                    } else if (entry.targetType == DialogueDB::BlacklistTarget::Topic) {
                        hasTopics = true;
                    }
                    break;
                }
            }
        }
        
        // Show warning if mixed selection
        if (hasTopics && hasScenes) {
            TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), "Warning: Mixed selection (Topics and Scenes)");
            TextWrapped("Topics and Scenes have different filter categories. Apply changes separately.");
            Separator();
            Spacing();
            
            // Only show remove/whitelist for mixed selections
            if (Button("Remove Selected from Blacklist", ImVec2(-1, 0))) {
                try {
                    auto db = DialogueDB::GetDatabase();
                    
                    // Use batch removal for efficiency
                    int removedCount = db->RemoveFromBlacklistBatch(selectedBlacklistIDs_);
                    
                    selectedBlacklistIDs_.clear();
                    hasBlacklistSelection_ = false;
                    selectedBlacklistID_ = -1;
                    lastClickedIndex_ = -1;
                    blacklistCache_ = db->GetBlacklist();
                    
                    spdlog::info("[STFUMenu] Bulk removed {} blacklist entries", removedCount);
                } catch (const std::exception& e) {
                    spdlog::error("[STFUMenu] Exception during blacklist multi-selection removal: {}", e.what());
                }
            }
            return;
        }
        
        // Unified selection - show full blocking controls
        bool isSceneSelection = hasScenes;
        
        // Filter Category dropdown (disabled if Whitelist or SkyrimNet block is checked)
        Text("Type:");
        SameLine();
        if (multiWhiteList_) {
            // Show disabled dropdown with only Whitelist option
            const char* whitelistOption = "Whitelist";
            int whitelistIndex = 0;
            SetNextItemWidth(140.0f);
            BeginDisabled();
            Combo("##BlacklistMultiFilterCategory", &whitelistIndex, &whitelistOption, 1);
            EndDisabled();
        } else if (isSkyrimNetLoaded_ && multiSkyrimNetOnly_) {
            // Show disabled dropdown with only SkyrimNet option
            const char* skyrimNetOption = "SkyrimNet";
            int skyrimNetIndex = 0;
            SetNextItemWidth(140.0f);
            BeginDisabled();
            Combo("##BlacklistMultiFilterCategory", &skyrimNetIndex, &skyrimNetOption, 1);
            EndDisabled();
        } else {
            auto categories = GetFilterCategories(isSceneSelection);
            std::vector<const char*> categoryPtrs;
            for (const auto& cat : categories) {
                categoryPtrs.push_back(cat.c_str());
            }
            
            SetNextItemWidth(140.0f);
            if (Combo("##BlacklistMultiFilterCategory", &multiFilterCategoryIndex_, categoryPtrs.data(), static_cast<int>(categoryPtrs.size()))) {
                multiSelectedFilterCategory_ = categories[multiFilterCategoryIndex_];
            }
            if (IsItemHovered(ImGuiHoveredFlags_None)) {
                BeginTooltip();
                Text("Select which setting toggles blocking");
                EndTooltip();
            }
        }
        
        Spacing();
        
        // Blocking controls (same as history tab)
        bool softChanged = Checkbox("Soft block", &multiSoftBlockEnabled_);
        if (softChanged && multiSoftBlockEnabled_) {
            multiHardBlock_ = false;
            multiSkyrimNetOnly_ = false;
            multiWhiteList_ = false;
        }
        if (IsItemHovered(ImGuiHoveredFlags_None)) {
            BeginTooltip();
            Text("Blocks audio/subtitles and SkyrimNet logging");
            EndTooltip();
        }

        
        // SkyrimNet block checkbox - always disabled during multi-selection
        if (isSkyrimNetLoaded_) {
            BeginDisabled();
            bool skyrimNetChanged = Checkbox("SkyrimNet block", &multiSkyrimNetOnly_);
            if (skyrimNetChanged && multiSkyrimNetOnly_) {
                multiSoftBlockEnabled_ = false;
                multiHardBlock_ = false;
                multiWhiteList_ = false;
            }
            if (IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                BeginTooltip();
                Text("SkyrimNet blocking not available during multi-selection");
                EndTooltip();
            }
            EndDisabled();
        }
        
        bool hardChanged = Checkbox("Hard block", &multiHardBlock_);
        if (hardChanged && multiHardBlock_) {
            multiSoftBlockEnabled_ = false;
            multiSkyrimNetOnly_ = false;
            multiWhiteList_ = false;
        }
        if (IsItemHovered(ImGuiHoveredFlags_None)) {
            BeginTooltip();
            Text(isSceneSelection ? "Prevent scenes from starting entirely" : "Block dialogues from playing entirely");
            EndTooltip();
        }
        
        Spacing();
        
        bool whitelistChanged = Checkbox("Whitelist", &multiWhiteList_);
        if (whitelistChanged && multiWhiteList_) {
            multiSoftBlockEnabled_ = false;
            multiSkyrimNetOnly_ = false;
            multiHardBlock_ = false;
        }
        if (IsItemHovered(ImGuiHoveredFlags_None)) {
            BeginTooltip();
            Text("Remove selected entries from blacklist");
            EndTooltip();
        }
        
        Spacing();
        Separator();
        Spacing();
        
        // Apply button
        std::string buttonLabel = "Apply to " + std::to_string(selectedBlacklistIDs_.size()) + " Selected";
        if (Button(buttonLabel.c_str(), ImVec2(-1, 0))) {
            try {
                auto db = DialogueDB::GetDatabase();
                
                if (multiWhiteList_) {
                // Remove from blacklist - use batch operation
                int removedCount = db->RemoveFromBlacklistBatch(selectedBlacklistIDs_);
                spdlog::info("[STFUMenu] Bulk whitelisted {} entries (removed from blacklist)", removedCount);
            } else {
                // Check if all block types are unchecked
                bool allBlocksUnchecked = !multiHardBlock_ && !multiSkyrimNetOnly_ && !multiSoftBlockEnabled_;
                
                if (allBlocksUnchecked) {
                    // Remove from blacklist (whitelist) - use batch operation
                    int removedCount = db->RemoveFromBlacklistBatch(selectedBlacklistIDs_);
                    spdlog::info("[STFUMenu] Bulk removed {} entries from blacklist (all blocks unchecked)", removedCount);
                } else {
                    // Update blocking settings - use batch operation for efficiency
                    // Determine block type based on checkboxes
                    DialogueDB::BlockType blockType;
                    bool blockAudio = false;
                    bool blockSubtitles = false;
                    bool blockSkyrimNet = false;
                    
                    if (multiHardBlock_) {
                        blockType = DialogueDB::BlockType::Hard;
                        blockAudio = true;
                        blockSubtitles = true;
                        blockSkyrimNet = true;
                    } else if (multiSkyrimNetOnly_) {
                        blockType = DialogueDB::BlockType::SkyrimNet;
                        blockAudio = false;
                        blockSubtitles = false;
                        blockSkyrimNet = true;
                    } else if (multiSoftBlockEnabled_) {
                        blockType = DialogueDB::BlockType::Soft;
                        // Soft block always blocks both audio and subtitles
                        blockAudio = true;
                        blockSubtitles = true;
                        blockSkyrimNet = true;
                    }
                    
                    // Collect all entries to update
                    std::vector<DialogueDB::BlacklistEntry> entriesToUpdate;
                    std::vector<uint32_t> formIDsToInvalidate;
                    
                    for (const auto& entryId : selectedBlacklistIDs_) {
                        for (auto& entry : blacklistCache_) {
                            if (entry.id == entryId) {
                                entry.blockType = blockType;
                                entry.blockAudio = blockAudio;
                                entry.blockSubtitles = blockSubtitles;
                                entry.blockSkyrimNet = blockSkyrimNet;
                                entry.filterCategory = multiSelectedFilterCategory_;
                                
                                entriesToUpdate.push_back(entry);
                                formIDsToInvalidate.push_back(entry.targetFormID);
                                break;
                            }
                        }
                    }
                    
                    // Batch update - much faster than individual updates
                    int updatedCount = db->AddToBlacklistBatch(entriesToUpdate, false);
                    
                    // Invalidate cache for all updated entries
                    for (uint32_t formID : formIDsToInvalidate) {
                        Config::InvalidateTopicCache(formID);
                    }
                    
                    spdlog::info("[STFUMenu] Bulk updated {} blacklist entries", updatedCount);
                }
            }
            
            // Clear selection and refresh
            selectedBlacklistIDs_.clear();
            hasBlacklistSelection_ = false;
            selectedBlacklistID_ = -1;
            lastClickedIndex_ = -1;
            blacklistCache_ = db->GetBlacklist();
            } catch (const std::exception& e) {
                spdlog::error("[STFUMenu] Exception during blacklist multi-selection apply: {}", e.what());
            }
        }
        
        return;
    }
    
    // Check if we have a single selection (either via hasBlacklistSelection_ or single item in array)
    bool hasSingleSelection = hasBlacklistSelection_ || selectedBlacklistIDs_.size() == 1;
    
    if (!hasSingleSelection) {
        TextColored(ImVec4(0.8f, 0.8f, 1.0f, 1.0f), "Multi-Selection Controls:");
        Spacing();
        TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "  Click - Select single entry");
        TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "  Ctrl+Click - Add/remove entry from selection");
        TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "  Shift+Click - Select range from last clicked to current");
        Spacing();
        Separator();
        Spacing();
        TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Select an entry above to view details and modify settings.");
        return;
    }
    
    // Get available width for split layout
    ImVec2 availSpace;
    GetContentRegionAvail(&availSpace);
    float leftWidth = availSpace.x * 0.50f;
    float rightWidth = availSpace.x * 0.50f - 10.0f;
    
    // Check if this is a scene entry
    bool isScene = (selectedBlacklistEntry_.targetType == DialogueDB::BlacklistTarget::Scene);
    
    if (isScene) {
        // Scene entry details
        if (BeginChild("SceneDetails", ImVec2(leftWidth, 0), ImGuiChildFlags_Border, ImGuiWindowFlags_None)) {
            Text("Scene Entry Details:");
            Separator();
            Spacing();
            
            Text("ID: %lld", selectedBlacklistEntry_.id);
            if (selectedBlacklistEntry_.targetFormID > 0) {
                Text("Editor ID: %s (0%X)", selectedBlacklistEntry_.targetEditorID.c_str(), selectedBlacklistEntry_.targetFormID);
            } else {
                Text("Editor ID: %s", selectedBlacklistEntry_.targetEditorID.c_str());
            }
            if (!selectedBlacklistEntry_.sourcePlugin.empty()) {
                Text("Source: %s", selectedBlacklistEntry_.sourcePlugin.c_str());
            }
            
            // Color-coded block type
            Text("Block Type: ");
            SameLine();
            const char* blockTypeStr = "Unknown";
            ImVec4 blockTypeColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);  // White default
            
            switch (selectedBlacklistEntry_.blockType) {
                case DialogueDB::BlockType::Soft:
                    blockTypeStr = "Soft Block";
                    blockTypeColor = ImVec4(1.0f, 0.55f, 0.0f, 1.0f);  // Orange
                    break;
                case DialogueDB::BlockType::Hard:
                    blockTypeStr = "Hard Block";
                    blockTypeColor = ImVec4(1.0f, 0.2f, 0.2f, 1.0f);  // Red
                    break;
                case DialogueDB::BlockType::SkyrimNet:
                    blockTypeStr = "SkyrimNet";
                    blockTypeColor = ImVec4(0.59f, 0.59f, 0.59f, 1.0f);  // Gray
                    break;
            }
            
            TextColored(blockTypeColor, "%s", blockTypeStr);
            
            Text("Added: %s", FormatTimestamp(selectedBlacklistEntry_.addedTimestamp).c_str());
            
            Spacing();
            
            // Show Responses button
            if (!selectedBlacklistEntry_.responseText.empty()) {
                if (Button("Show All Responses", ImVec2(-1, 0))) {
                    popupResponses_ = ParseResponsesJson(selectedBlacklistEntry_.responseText);
                    popupTitle_ = "All Responses: " + selectedBlacklistEntry_.targetEditorID;
                    showResponsesPopup_ = true;
                }
                if (IsItemHovered(ImGuiHoveredFlags_None)) {
                    BeginTooltip();
                    std::string typeStr = isScene ? "scene" : "topic";
                    Text("View all responses for this %s", typeStr.c_str());
                    EndTooltip();
                }
            } else {
                TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No responses captured yet");
                if (IsItemHovered(ImGuiHoveredFlags_None)) {
                    BeginTooltip();
                    Text("Responses will be captured when this dialogue plays in-game");
                    EndTooltip();
                }
            }
            
            Spacing();
            Separator();
            Spacing();
            
            Text("Notes:");
            InputTextMultiline("##SceneNotes", blacklistNotes_, sizeof(blacklistNotes_), ImVec2(-1, 80), ImGuiInputTextFlags_None, nullptr, nullptr);
            
            if (Button("Update Notes", ImVec2(-1, 0))) {
                selectedBlacklistEntry_.notes = blacklistNotes_;
                DialogueDB::GetDatabase()->AddToBlacklist(selectedBlacklistEntry_);
                Config::InvalidateTopicCache(selectedBlacklistEntry_.targetFormID);
                
                // Reload with filters applied
                auto allBlacklist = DialogueDB::GetDatabase()->GetBlacklist();
                std::vector<DialogueDB::BlacklistEntry> filteredBlacklist;
                std::string query = searchQuery_;
                std::transform(query.begin(), query.end(), query.begin(), ::tolower);
                
                for (const auto& entry : allBlacklist) {
                    bool matches = true;
                    if (query.length() > 0) {
                        bool foundMatch = false;
                        std::string editorID = entry.targetEditorID;
                        std::transform(editorID.begin(), editorID.end(), editorID.begin(), ::tolower);
                        if (editorID.find(query) != std::string::npos) foundMatch = true;
                        if (!foundMatch && !entry.responseText.empty()) {
                            std::string responseText = entry.responseText;
                            std::transform(responseText.begin(), responseText.end(), responseText.begin(), ::tolower);
                            if (responseText.find(query) != std::string::npos) foundMatch = true;
                        }
                        if (!foundMatch) matches = false;
                    }
                    if (matches && filterTargetType_ > 0) {
                        if (filterTargetType_ == 4) {
                            if (entry.targetType != DialogueDB::BlacklistTarget::Scene || 
                                entry.filterCategory == "BardSongs" || 
                                entry.filterCategory == "FollowerCommentary") matches = false;
                        } else if (filterTargetType_ == 5) {
                            if (entry.filterCategory != "BardSongs") matches = false;
                        } else if (filterTargetType_ == 6) {
                            if (entry.filterCategory != "FollowerCommentary") matches = false;
                        } else if (filterTargetType_ <= 3) {
                            if (static_cast<int>(entry.targetType) != filterTargetType_) matches = false;
                        }
                    }
                    if (matches) {
                        bool blockTypeMatch = false;
                        if (filterSoftBlock_ && entry.blockType == DialogueDB::BlockType::Soft) blockTypeMatch = true;
                        if (filterHardBlock_ && entry.blockType == DialogueDB::BlockType::Hard) blockTypeMatch = true;
                        if (filterSkyrimNet_ && entry.blockType == DialogueDB::BlockType::SkyrimNet) blockTypeMatch = true;
                        if (!blockTypeMatch) matches = false;
                    }
                    if (matches && !filterTopic_) {
                        if (entry.targetType == DialogueDB::BlacklistTarget::Topic) matches = false;
                    }
                    if (matches && !filterScene_) {
                        if (entry.targetType == DialogueDB::BlacklistTarget::Scene) matches = false;
                    }
                    if (matches) filteredBlacklist.push_back(entry);
                }
                blacklistCache_ = filteredBlacklist;
                blacklistLoadedCount_ = static_cast<int>(blacklistCache_.size());
                
                // Update the selected entry from cache to ensure consistency
                for (auto& entry : blacklistCache_) {
                    if (entry.id == selectedBlacklistEntry_.id) {
                        selectedBlacklistEntry_ = entry;
                        break;
                    }
                }
                
                spdlog::info("[STFUMenu] Updated scene entry notes");
            }
        }
        EndChild();
        
        SameLine();
        
        // Scene actions
        if (BeginChild("SceneActions", ImVec2(rightWidth, 0), ImGuiChildFlags_Border, ImGuiWindowFlags_None)) {
            Text("Actions:");
            Separator();
            Spacing();
            
            // Blocking behavior controls for scenes
            bool isHardBlock = (selectedBlacklistEntry_.blockType == DialogueDB::BlockType::Hard);
            bool isSoftBlock = (selectedBlacklistEntry_.blockType == DialogueDB::BlockType::Soft);
            bool isSkyrimNetOnly = (selectedBlacklistEntry_.blockType == DialogueDB::BlockType::SkyrimNet);
            
            // Track changes
            bool softBlockChanged = false, skyrimNetChanged = false, hardBlockChanged = false;
            
            if (Checkbox("Soft block", &isSoftBlock)) {
                if (isSoftBlock) {
                    // Uncheck others for mutual exclusion
                    isSkyrimNetOnly = false;
                    isHardBlock = false;
                }
                softBlockChanged = true;
            }
            if (IsItemHovered(ImGuiHoveredFlags_None)) {
                BeginTooltip();
                Text("Blocks audio/subtitles and SkyrimNet logging");
                EndTooltip();
            }
            
            // Only show SkyrimNet checkbox if entry was originally created as SkyrimNet block
            if (selectedBlacklistEntry_.filterCategory == "SkyrimNet") {
                SameLine();
                if (Checkbox("SkyrimNet", &isSkyrimNetOnly)) {
                    if (isSkyrimNetOnly) {
                        // Uncheck others for mutual exclusion
                        isSoftBlock = false;
                        isHardBlock = false;
                    }
                    skyrimNetChanged = true;
                }
                if (IsItemHovered(ImGuiHoveredFlags_None)) {
                    BeginTooltip();
                    Text("Block scene from being logged by SkyrimNet (audio/subtitles play normally)");
                    EndTooltip();
                }
            }
            
            SameLine();
            if (Checkbox("Hard block", &isHardBlock)) {
                if (isHardBlock) {
                    // Uncheck others for mutual exclusion
                    isSoftBlock = false;
                    isSkyrimNetOnly = false;
                }
                hardBlockChanged = true;
            }
            if (IsItemHovered(ImGuiHoveredFlags_None)) {
                BeginTooltip();
                Text("Prevent scene from starting entirely");
                EndTooltip();
            }
            
            // Filter Category dropdown (inline with checkboxes) - only show for non-SkyrimNet blocks
            bool isSkyrimNetBlock = (selectedBlacklistEntry_.blockType == DialogueDB::BlockType::SkyrimNet);
            if (!isSkyrimNetBlock) {
                SameLine();
                int categoryIndex = GetCategoryIndex(selectedBlacklistEntry_.filterCategory, true);
                auto categories = GetFilterCategories(true);
                std::vector<const char*> categoryPtrs;
                for (const auto& cat : categories) {
                    categoryPtrs.push_back(cat.c_str());
                }
                SetNextItemWidth(280.0f);
                if (Combo("##FilterCategoryScene", &categoryIndex, categoryPtrs.data(), static_cast<int>(categoryPtrs.size()))) {
                    selectedBlacklistEntry_.filterCategory = categories[categoryIndex];
                    DialogueDB::GetDatabase()->AddToBlacklist(selectedBlacklistEntry_);
                    
                    // Re-apply current search/filter instead of loading full list
                    // This preserves the user's search results after category change
                    auto allBlacklist = DialogueDB::GetDatabase()->GetBlacklist();
                    std::vector<DialogueDB::BlacklistEntry> filteredBlacklist;
                    
                    std::string query = searchQuery_;
                    std::transform(query.begin(), query.end(), query.begin(), ::tolower);
                    
                    // Re-apply the same filters that were used in the search
                    for (const auto& entry : allBlacklist) {
                        bool matches = true;
                        
                        // Search query filter
                        if (query.length() > 0) {
                            bool foundMatch = false;
                            std::string editorID = entry.targetEditorID;
                            std::transform(editorID.begin(), editorID.end(), editorID.begin(), ::tolower);
                            if (editorID.find(query) != std::string::npos) foundMatch = true;
                            
                            if (!foundMatch && !entry.responseText.empty()) {
                                std::string responseText = entry.responseText;
                                std::transform(responseText.begin(), responseText.end(), responseText.begin(), ::tolower);
                                if (responseText.find(query) != std::string::npos) foundMatch = true;
                            }
                            
                            if (!foundMatch && !entry.subtypeName.empty()) {
                                std::string subtypeName = entry.subtypeName;
                                std::transform(subtypeName.begin(), subtypeName.end(), subtypeName.begin(), ::tolower);
                                if (subtypeName.find(query) != std::string::npos) foundMatch = true;
                            }
                            
                            if (!foundMatch && !entry.notes.empty()) {
                                std::string notes = entry.notes;
                                std::transform(notes.begin(), notes.end(), notes.begin(), ::tolower);
                                if (notes.find(query) != std::string::npos) foundMatch = true;
                            }
                            
                            if (!foundMatch && entry.targetFormID > 0) {
                                char formIDStr[16];
                                sprintf_s(formIDStr, "%X", entry.targetFormID);
                                std::string dbFormID = formIDStr;
                                std::string searchFormID = query;
                                if (searchFormID.find("0x") == 0) searchFormID = searchFormID.substr(2);
                                auto stripZeroes = [](std::string& s) {
                                    size_t pos = s.find_first_not_of('0');
                                    if (pos != std::string::npos) s = s.substr(pos);
                                    else s = "0";
                                };
                                stripZeroes(searchFormID);
                                stripZeroes(dbFormID);
                                std::transform(searchFormID.begin(), searchFormID.end(), searchFormID.begin(), ::tolower);
                                std::transform(dbFormID.begin(), dbFormID.end(), dbFormID.begin(), ::tolower);
                                if (dbFormID.find(searchFormID) != std::string::npos) foundMatch = true;
                            }
                            
                            if (!foundMatch) matches = false;
                        }
                        
                        // Apply target type filter
                        if (matches && filterTargetType_ > 0) {
                            if (filterTargetType_ == 4 && (entry.targetType != DialogueDB::BlacklistTarget::Scene || 
                                entry.filterCategory == "BardSongs" || entry.filterCategory == "FollowerCommentary")) {
                                matches = false;
                            } else if (filterTargetType_ == 5 && entry.filterCategory != "BardSongs") {
                                matches = false;
                            } else if (filterTargetType_ == 6 && entry.filterCategory != "FollowerCommentary") {
                                matches = false;
                            } else if (filterTargetType_ <= 3 && static_cast<int>(entry.targetType) != filterTargetType_) {
                                matches = false;
                            }
                        }
                        
                        // Apply block type filter
                        if (matches) {
                            bool blockTypeMatch = false;
                            if (filterSoftBlock_ && entry.blockType == DialogueDB::BlockType::Soft) blockTypeMatch = true;
                            if (filterHardBlock_ && entry.blockType == DialogueDB::BlockType::Hard) blockTypeMatch = true;
                            if (filterSkyrimNet_ && entry.blockType == DialogueDB::BlockType::SkyrimNet) blockTypeMatch = true;
                            if (!blockTypeMatch) matches = false;
                        }
                        
                        // Apply topic/scene filters
                        if (matches && !filterTopic_ && entry.targetType == DialogueDB::BlacklistTarget::Topic) {
                            matches = false;
                        }
                        if (matches && !filterScene_ && entry.targetType == DialogueDB::BlacklistTarget::Scene) {
                            matches = false;
                        }
                        
                        if (matches) {
                            filteredBlacklist.push_back(entry);
                        }
                    }
                    
                    blacklistCache_ = filteredBlacklist;
                    
                    // Update the selected entry in the cache
                    for (auto& entry : blacklistCache_) {
                        if (entry.id == selectedBlacklistEntry_.id) {
                            selectedBlacklistEntry_ = entry;
                            break;
                        }
                    }
                    
                    spdlog::info("[STFUMenu] Updated filter category to: {} (reapplied search filters)", selectedBlacklistEntry_.filterCategory);
                }
                if (IsItemHovered(ImGuiHoveredFlags_None)) {
                    BeginTooltip();
                    Text("Select which setting toggles blocking");
                    EndTooltip();
                }
            }
            
            // Update block type based on checkbox changes
            if (softBlockChanged || skyrimNetChanged || hardBlockChanged) {
                DialogueDB::BlockType newBlockType;
                bool blockAudio = false;
                bool blockSubtitles = false;
                bool blockSkyrimNet = false;
                
                if (isHardBlock) {
                    newBlockType = DialogueDB::BlockType::Hard;
                    blockAudio = true;
                    blockSubtitles = true;
                    blockSkyrimNet = true;
                } else if (isSkyrimNetOnly && !isSoftBlock) {
                    // SkyrimNet-only block
                    newBlockType = DialogueDB::BlockType::SkyrimNet;
                    blockAudio = false;
                    blockSubtitles = false;
                    blockSkyrimNet = true;
                    // Auto-set filter category to SkyrimNet
                    selectedBlacklistEntry_.filterCategory = "SkyrimNet";
                } else if (isSoftBlock) {
                    // Soft block always blocks audio, subtitles, and SkyrimNet
                    newBlockType = DialogueDB::BlockType::Soft;
                    blockAudio = true;
                    blockSubtitles = true;
                    blockSkyrimNet = true;
                } else {
                    // Nothing checked - default to soft block
                    newBlockType = DialogueDB::BlockType::Soft;
                    blockAudio = true;
                    blockSubtitles = true;
                    blockSkyrimNet = true;
                    isSoftBlock = true;  // Update UI state
                }
                
                // Update granular flags in entry
                selectedBlacklistEntry_.blockType = newBlockType;
                selectedBlacklistEntry_.blockAudio = blockAudio;
                selectedBlacklistEntry_.blockSubtitles = blockSubtitles;
                selectedBlacklistEntry_.blockSkyrimNet = blockSkyrimNet;
                
                // Save to database (AddToBlacklist does UPDATE if entry exists)
                DialogueDB::GetDatabase()->AddToBlacklist(selectedBlacklistEntry_);
                
                // Reload with filters applied
                auto allBlacklist = DialogueDB::GetDatabase()->GetBlacklist();
                std::vector<DialogueDB::BlacklistEntry> filteredBlacklist;
                std::string query = searchQuery_;
                std::transform(query.begin(), query.end(), query.begin(), ::tolower);
                
                for (const auto& entry : allBlacklist) {
                    bool matches = true;
                    
                    // Search query
                    if (query.length() > 0) {
                        bool foundMatch = false;
                        std::string editorID = entry.targetEditorID;
                        std::transform(editorID.begin(), editorID.end(), editorID.begin(), ::tolower);
                        if (editorID.find(query) != std::string::npos) foundMatch = true;
                        if (!foundMatch && !entry.responseText.empty()) {
                            std::string responseText = entry.responseText;
                            std::transform(responseText.begin(), responseText.end(), responseText.begin(), ::tolower);
                            if (responseText.find(query) != std::string::npos) foundMatch = true;
                        }
                        if (!foundMatch) matches = false;
                    }
                    
                    // Filter by target type
                    if (matches && filterTargetType_ > 0) {
                        if (filterTargetType_ == 4) {
                            if (entry.targetType != DialogueDB::BlacklistTarget::Scene || 
                                entry.filterCategory == "BardSongs" || 
                                entry.filterCategory == "FollowerCommentary") {
                                matches = false;
                            }
                        } else if (filterTargetType_ == 5) {
                            if (entry.filterCategory != "BardSongs") matches = false;
                        } else if (filterTargetType_ == 6) {
                            if (entry.filterCategory != "FollowerCommentary") matches = false;
                        } else if (filterTargetType_ <= 3) {
                            if (static_cast<int>(entry.targetType) != filterTargetType_) matches = false;
                        }
                    }
                    
                    // Filter by block type
                    if (matches) {
                        bool blockTypeMatch = false;
                        if (filterSoftBlock_ && entry.blockType == DialogueDB::BlockType::Soft) blockTypeMatch = true;
                        if (filterHardBlock_ && entry.blockType == DialogueDB::BlockType::Hard) blockTypeMatch = true;
                        if (filterSkyrimNet_ && entry.blockType == DialogueDB::BlockType::SkyrimNet) blockTypeMatch = true;
                        if (!blockTypeMatch) matches = false;
                    }
                    
                    // Filter by target type (topics only)
                    if (matches && !filterTopic_) {
                        if (entry.targetType == DialogueDB::BlacklistTarget::Topic) matches = false;
                    }
                    
                    // Filter by target type (all scenes including bard songs)
                    if (matches && !filterScene_) {
                        if (entry.targetType == DialogueDB::BlacklistTarget::Scene) matches = false;
                    }
                    
                    if (matches) filteredBlacklist.push_back(entry);
                }
                
                blacklistCache_ = filteredBlacklist;
                blacklistLoadedCount_ = static_cast<int>(blacklistCache_.size());
                
                // Update the selected entry from cache to ensure consistency
                for (auto& entry : blacklistCache_) {
                    if (entry.id == selectedBlacklistEntry_.id) {
                        selectedBlacklistEntry_ = entry;
                        break;
                    }
                }
                
                spdlog::info("[STFUMenu] Updated scene entry: blockType={}, audio={}, subtitles={}, skyrimNet={}", 
                             static_cast<int>(newBlockType), blockAudio, blockSubtitles, blockSkyrimNet);
            }
            
            Spacing();
            
            if (Button("Remove from Blacklist", ImVec2(-1, 0))) {
                DialogueDB::GetDatabase()->RemoveFromBlacklist(selectedBlacklistEntry_.id);
                hasBlacklistSelection_ = false;
                selectedBlacklistID_ = -1;
                blacklistCache_ = DialogueDB::GetDatabase()->GetBlacklist();
                spdlog::info("[STFUMenu] Removed scene entry ID: {}", selectedBlacklistEntry_.id);
            }
            
            if (Button("Add to Whitelist", ImVec2(-1, 0))) {
                try {
                    auto db = DialogueDB::GetDatabase();
                    // Create whitelist entry from blacklist entry
                    DialogueDB::BlacklistEntry whitelistEntry = selectedBlacklistEntry_;
                    whitelistEntry.id = 0;  // Reset ID for new entry
                    whitelistEntry.filterCategory = "Whitelist";
                    whitelistEntry.addedTimestamp = std::chrono::duration_cast<std::chrono::seconds>(
                        std::chrono::system_clock::now().time_since_epoch()
                    ).count();
                    
                    if (db->AddToWhitelist(whitelistEntry)) {
                        // Remove from blacklist after successful whitelist addition
                        db->RemoveFromBlacklist(selectedBlacklistEntry_.id);
                        hasBlacklistSelection_ = false;
                        selectedBlacklistID_ = -1;
                        blacklistCache_ = db->GetBlacklist();
                        spdlog::info("[STFUMenu] Added scene to whitelist and removed from blacklist: {}", selectedBlacklistEntry_.targetEditorID);
                    } else {
                        spdlog::error("[STFUMenu] Failed to add scene to whitelist: {}", selectedBlacklistEntry_.targetEditorID);
                    }
                } catch (const std::exception& e) {
                    spdlog::error("[STFUMenu] Exception adding scene to whitelist: {}", e.what());
                }
            }
            if (IsItemHovered(ImGuiHoveredFlags_None)) {
                BeginTooltip();
                Text("Add this scene to the whitelist.");
                Text("It will be removed from the blacklist.");
                EndTooltip();
            }
            
            Spacing();
            Separator();
            Spacing();
            
            // Explanation of blocking behavior for scenes
            Text("Block Type Information:");
            Spacing();
            
            TextColored(ImVec4(1.0f, 0.65f, 0.0f, 1.0f), "Soft Block:");
            TextWrapped("Scene continues to play. Audio and subtitles are blocked based on checkboxes.");
            
            Spacing();
            
            TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "Hard Block:");
            TextWrapped("Prevents scene from starting entirely. No dialogue, no packages, no scripts are executed.");
        }
        EndChild();
    } else {
        // Regular blacklist entry details
        if (BeginChild("BlacklistDetails", ImVec2(leftWidth, 0), ImGuiChildFlags_Border, ImGuiWindowFlags_None)) {
            Text("Blacklist Entry Details:");
            Separator();
            Spacing();
            
            Text("ID: %lld", selectedBlacklistEntry_.id);
            
            // Condense Form ID and Editor ID into one line
            if (!selectedBlacklistEntry_.targetEditorID.empty() && selectedBlacklistEntry_.targetType != DialogueDB::BlacklistTarget::Subtype) {
                Text("Topic: %s (0%X)", selectedBlacklistEntry_.targetEditorID.c_str(), selectedBlacklistEntry_.targetFormID);
            } else if (!selectedBlacklistEntry_.targetEditorID.empty()) {
                Text("Topic: %s", selectedBlacklistEntry_.targetEditorID.c_str());
            }
            
            // Block type and timestamp BEFORE response text
            Text("Block Type: ");
            SameLine();
            const char* blockTypeStr = "Unknown";
            ImVec4 blockTypeColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);  // White default
            
            switch (selectedBlacklistEntry_.blockType) {
                case DialogueDB::BlockType::Soft:
                    blockTypeStr = "Soft Block";
                    blockTypeColor = ImVec4(1.0f, 0.55f, 0.0f, 1.0f);  // Orange
                    break;
                case DialogueDB::BlockType::Hard:
                    blockTypeStr = "Hard Block";
                    blockTypeColor = ImVec4(1.0f, 0.2f, 0.2f, 1.0f);  // Red
                    break;
                case DialogueDB::BlockType::SkyrimNet:
                    blockTypeStr = "SkyrimNet Only";
                    blockTypeColor = ImVec4(0.59f, 0.59f, 0.59f, 1.0f);  // Gray
                    break;
            }
            
            TextColored(blockTypeColor, "%s", blockTypeStr);
            
            Text("Added: %s", FormatTimestamp(selectedBlacklistEntry_.addedTimestamp).c_str());
            
            Spacing();
            
            // Show Responses button
            if (!selectedBlacklistEntry_.responseText.empty()) {
                if (Button("Show All Responses", ImVec2(-1, 0))) {
                    popupResponses_ = ParseResponsesJson(selectedBlacklistEntry_.responseText);
                    popupTitle_ = "All Responses: " + selectedBlacklistEntry_.targetEditorID;
                    showResponsesPopup_ = true;
                }
                if (IsItemHovered(ImGuiHoveredFlags_None)) {
                    BeginTooltip();
                    Text("View all responses for this topic");
                    EndTooltip();
                }
            } else {
                TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No responses captured yet");
                if (IsItemHovered(ImGuiHoveredFlags_None)) {
                    BeginTooltip();
                    Text("Responses will be captured when this dialogue plays in-game");
                    EndTooltip();
                }
            }
            
            if (selectedBlacklistEntry_.subtype > 0) {
                Spacing();
                Text("Subtype: %s", 
                     selectedBlacklistEntry_.subtypeName.empty() ? "Unknown" : selectedBlacklistEntry_.subtypeName.c_str());
            }
            
            Spacing();
            Separator();
            Spacing();
            
            Text("Notes:");
            InputTextMultiline("##BlacklistNotes", blacklistNotes_, sizeof(blacklistNotes_), ImVec2(-1, 80), ImGuiInputTextFlags_None, nullptr, nullptr);
            
            if (Button("Update Notes", ImVec2(-1, 0))) {
                selectedBlacklistEntry_.notes = blacklistNotes_;
                DialogueDB::GetDatabase()->AddToBlacklist(selectedBlacklistEntry_);
                
                // Reload with filters applied
                auto allBlacklist = DialogueDB::GetDatabase()->GetBlacklist();
                std::vector<DialogueDB::BlacklistEntry> filteredBlacklist;
                std::string query = searchQuery_;
                std::transform(query.begin(), query.end(), query.begin(), ::tolower);
                
                for (const auto& entry : allBlacklist) {
                    bool matches = true;
                    if (query.length() > 0) {
                        bool foundMatch = false;
                        std::string editorID = entry.targetEditorID;
                        std::transform(editorID.begin(), editorID.end(), editorID.begin(), ::tolower);
                        if (editorID.find(query) != std::string::npos) foundMatch = true;
                        if (!foundMatch && !entry.responseText.empty()) {
                            std::string responseText = entry.responseText;
                            std::transform(responseText.begin(), responseText.end(), responseText.begin(), ::tolower);
                            if (responseText.find(query) != std::string::npos) foundMatch = true;
                        }
                        if (!foundMatch) matches = false;
                    }
                    if (matches && filterTargetType_ > 0) {
                        if (filterTargetType_ == 4) {
                            if (entry.targetType != DialogueDB::BlacklistTarget::Scene || 
                                entry.filterCategory == "BardSongs" || 
                                entry.filterCategory == "FollowerCommentary") matches = false;
                        } else if (filterTargetType_ == 5) {
                            if (entry.filterCategory != "BardSongs") matches = false;
                        } else if (filterTargetType_ == 6) {
                            if (entry.filterCategory != "FollowerCommentary") matches = false;
                        } else if (filterTargetType_ <= 3) {
                            if (static_cast<int>(entry.targetType) != filterTargetType_) matches = false;
                        }
                    }
                    if (matches) {
                        bool blockTypeMatch = false;
                        if (filterSoftBlock_ && entry.blockType == DialogueDB::BlockType::Soft) blockTypeMatch = true;
                        if (filterHardBlock_ && entry.blockType == DialogueDB::BlockType::Hard) blockTypeMatch = true;
                        if (filterSkyrimNet_ && entry.blockType == DialogueDB::BlockType::SkyrimNet) blockTypeMatch = true;
                        if (!blockTypeMatch) matches = false;
                    }
                    if (matches && !filterTopic_) {
                        if (entry.targetType == DialogueDB::BlacklistTarget::Topic) matches = false;
                    }
                    if (matches && !filterScene_) {
                        if (entry.targetType == DialogueDB::BlacklistTarget::Scene) matches = false;
                    }
                    if (matches) filteredBlacklist.push_back(entry);
                }
                blacklistCache_ = filteredBlacklist;
                blacklistLoadedCount_ = static_cast<int>(blacklistCache_.size());
                
                // Update the selected entry from cache to ensure consistency
                for (auto& entry : blacklistCache_) {
                    if (entry.id == selectedBlacklistEntry_.id) {
                        selectedBlacklistEntry_ = entry;
                        break;
                    }
                }
                
                spdlog::info("[STFUMenu] Updated blacklist entry notes");
            }
        }
        EndChild();
        
        SameLine();
        
        // Regular blacklist actions
        if (BeginChild("BlacklistActions", ImVec2(rightWidth, 0), ImGuiChildFlags_Border, ImGuiWindowFlags_None)) {
            Text("Actions:");
            Separator();
            Spacing();
            
            // Blocking behavior controls
            bool isHardBlock = (selectedBlacklistEntry_.blockType == DialogueDB::BlockType::Hard);
            bool isSoftBlock = (selectedBlacklistEntry_.blockType == DialogueDB::BlockType::Soft);
            bool isSkyrimNetOnly = (selectedBlacklistEntry_.blockType == DialogueDB::BlockType::SkyrimNet);
            
            // Track changes
            bool softBlockChanged = false, skyrimNetChanged = false, hardBlockChanged = false;
            
            if (Checkbox("Soft block", &isSoftBlock)) {
                if (isSoftBlock) {
                    // Uncheck others for mutual exclusion
                    isSkyrimNetOnly = false;
                    isHardBlock = false;
                }
                softBlockChanged = true;
            }
            if (IsItemHovered(ImGuiHoveredFlags_None)) {
                BeginTooltip();
                Text("Blocks audio/subtitles and SkyrimNet logging");
                EndTooltip();
            }
            
            // Only show SkyrimNet checkbox if entry was originally created as SkyrimNet block
            if (selectedBlacklistEntry_.filterCategory == "SkyrimNet") {
                SameLine();
                if (Checkbox("SkyrimNet", &isSkyrimNetOnly)) {
                    if (isSkyrimNetOnly) {
                        // Uncheck others for mutual exclusion
                        isSoftBlock = false;
                        isHardBlock = false;
                    }
                    skyrimNetChanged = true;
                }
                if (IsItemHovered(ImGuiHoveredFlags_None)) {
                    BeginTooltip();
                    Text("Block dialogue from being logged by SkyrimNet (audio/subtitles play normally)");
                    EndTooltip();
                }
            }
            
            SameLine();
            if (Checkbox("Hard block", &isHardBlock)) {
                if (isHardBlock) {
                    // Uncheck others for mutual exclusion
                    isSoftBlock = false;
                    isSkyrimNetOnly = false;
                }
                hardBlockChanged = true;
            }
            if (IsItemHovered(ImGuiHoveredFlags_None)) {
                BeginTooltip();
                Text("Block dialogue from playing entirely");
                EndTooltip();
            }
            
            // Filter Category dropdown (inline with checkboxes) - only show for non-SkyrimNet blocks
            bool isSkyrimNetBlock = (selectedBlacklistEntry_.blockType == DialogueDB::BlockType::SkyrimNet);
            if (!isSkyrimNetBlock) {
                SameLine();
                int categoryIndex = GetCategoryIndex(selectedBlacklistEntry_.filterCategory, false);
                auto categories = GetFilterCategories(false);
                std::vector<const char*> categoryPtrs;
                for (const auto& cat : categories) {
                    categoryPtrs.push_back(cat.c_str());
                }
                SetNextItemWidth(280.0f);
                if (Combo("##FilterCategoryBlacklist", &categoryIndex, categoryPtrs.data(), static_cast<int>(categoryPtrs.size()))) {
                    selectedBlacklistEntry_.filterCategory = categories[categoryIndex];
                    DialogueDB::GetDatabase()->AddToBlacklist(selectedBlacklistEntry_);
                    
                    // Re-apply current search/filter instead of loading full list
                    // This preserves the user's search results after category change
                    auto allBlacklist = DialogueDB::GetDatabase()->GetBlacklist();
                    std::vector<DialogueDB::BlacklistEntry> filteredBlacklist;
                    
                    std::string query = searchQuery_;
                    std::transform(query.begin(), query.end(), query.begin(), ::tolower);
                    
                    // Re-apply the same filters that were used in the search
                    for (const auto& entry : allBlacklist) {
                        bool matches = true;
                        
                        // Search query filter
                        if (query.length() > 0) {
                            bool foundMatch = false;
                            std::string editorID = entry.targetEditorID;
                            std::transform(editorID.begin(), editorID.end(), editorID.begin(), ::tolower);
                            if (editorID.find(query) != std::string::npos) foundMatch = true;
                            
                            if (!foundMatch && !entry.responseText.empty()) {
                                std::string responseText = entry.responseText;
                                std::transform(responseText.begin(), responseText.end(), responseText.begin(), ::tolower);
                                if (responseText.find(query) != std::string::npos) foundMatch = true;
                            }
                            
                            if (!foundMatch && !entry.subtypeName.empty()) {
                                std::string subtypeName = entry.subtypeName;
                                std::transform(subtypeName.begin(), subtypeName.end(), subtypeName.begin(), ::tolower);
                                if (subtypeName.find(query) != std::string::npos) foundMatch = true;
                            }
                            
                            if (!foundMatch && !entry.notes.empty()) {
                                std::string notes = entry.notes;
                                std::transform(notes.begin(), notes.end(), notes.begin(), ::tolower);
                                if (notes.find(query) != std::string::npos) foundMatch = true;
                            }
                            
                            if (!foundMatch && entry.targetFormID > 0) {
                                char formIDStr[16];
                                sprintf_s(formIDStr, "%X", entry.targetFormID);
                                std::string dbFormID = formIDStr;
                                std::string searchFormID = query;
                                if (searchFormID.find("0x") == 0) searchFormID = searchFormID.substr(2);
                                auto stripZeroes = [](std::string& s) {
                                    size_t pos = s.find_first_not_of('0');
                                    if (pos != std::string::npos) s = s.substr(pos);
                                    else s = "0";
                                };
                                stripZeroes(searchFormID);
                                stripZeroes(dbFormID);
                                std::transform(searchFormID.begin(), searchFormID.end(), searchFormID.begin(), ::tolower);
                                std::transform(dbFormID.begin(), dbFormID.end(), dbFormID.begin(), ::tolower);
                                if (dbFormID.find(searchFormID) != std::string::npos) foundMatch = true;
                            }
                            
                            if (!foundMatch) matches = false;
                        }
                        
                        // Apply target type filter
                        if (matches && filterTargetType_ > 0) {
                            if (filterTargetType_ == 4 && (entry.targetType != DialogueDB::BlacklistTarget::Scene || 
                                entry.filterCategory == "BardSongs" || entry.filterCategory == "FollowerCommentary")) {
                                matches = false;
                            } else if (filterTargetType_ == 5 && entry.filterCategory != "BardSongs") {
                                matches = false;
                            } else if (filterTargetType_ == 6 && entry.filterCategory != "FollowerCommentary") {
                                matches = false;
                            } else if (filterTargetType_ <= 3 && static_cast<int>(entry.targetType) != filterTargetType_) {
                                matches = false;
                            }
                        }
                        
                        // Apply block type filter
                        if (matches) {
                            bool blockTypeMatch = false;
                            if (filterSoftBlock_ && entry.blockType == DialogueDB::BlockType::Soft) blockTypeMatch = true;
                            if (filterHardBlock_ && entry.blockType == DialogueDB::BlockType::Hard) blockTypeMatch = true;
                            if (filterSkyrimNet_ && entry.blockType == DialogueDB::BlockType::SkyrimNet) blockTypeMatch = true;
                            if (!blockTypeMatch) matches = false;
                        }
                        
                        // Apply topic/scene filters
                        if (matches && !filterTopic_ && entry.targetType == DialogueDB::BlacklistTarget::Topic) {
                            matches = false;
                        }
                        if (matches && !filterScene_ && entry.targetType == DialogueDB::BlacklistTarget::Scene) {
                            matches = false;
                        }
                        
                        if (matches) {
                            filteredBlacklist.push_back(entry);
                        }
                    }
                    
                    blacklistCache_ = filteredBlacklist;
                    
                    // Update the selected entry in the cache
                    for (auto& entry : blacklistCache_) {
                        if (entry.id == selectedBlacklistEntry_.id) {
                            selectedBlacklistEntry_ = entry;
                            break;
                        }
                    }
                    
                    spdlog::info("[STFUMenu] Updated filter category to: {} (reapplied search filters)", selectedBlacklistEntry_.filterCategory);
                }
                if (IsItemHovered(ImGuiHoveredFlags_None)) {
                    BeginTooltip();
                    Text("Select which setting toggles blocking");
                    EndTooltip();
                }
            }
            
            // Update block type based on checkbox changes
            if (softBlockChanged || skyrimNetChanged || hardBlockChanged) {
                DialogueDB::BlockType newBlockType;
                bool blockAudio = false;
                bool blockSubtitles = false;
                bool blockSkyrimNet = false;
                
                if (isHardBlock) {
                    newBlockType = DialogueDB::BlockType::Hard;
                    blockAudio = true;
                    blockSubtitles = true;
                    blockSkyrimNet = true;
                } else if (isSkyrimNetOnly && !isSoftBlock) {
                    // SkyrimNet-only block
                    newBlockType = DialogueDB::BlockType::SkyrimNet;
                    blockAudio = false;
                    blockSubtitles = false;
                    blockSkyrimNet = true;
                    // Auto-set filter category to SkyrimNet
                    selectedBlacklistEntry_.filterCategory = "SkyrimNet";
                } else if (isSoftBlock) {
                    // Soft block always blocks audio, subtitles, and SkyrimNet
                    newBlockType = DialogueDB::BlockType::Soft;
                    blockAudio = true;
                    blockSubtitles = true;
                    blockSkyrimNet = true;
                } else {
                    // Nothing checked - default to soft block
                    newBlockType = DialogueDB::BlockType::Soft;
                    blockAudio = true;
                    blockSubtitles = true;
                    blockSkyrimNet = true;
                    isSoftBlock = true;  // Update UI state
                }
                
                // Update granular flags in entry
                selectedBlacklistEntry_.blockType = newBlockType;
                selectedBlacklistEntry_.blockAudio = blockAudio;
                selectedBlacklistEntry_.blockSubtitles = blockSubtitles;
                selectedBlacklistEntry_.blockSkyrimNet = blockSkyrimNet;
                
                // Save to database (AddToBlacklist does UPDATE if entry exists)
                DialogueDB::GetDatabase()->AddToBlacklist(selectedBlacklistEntry_);
                
                // Reload with filters applied
                auto allBlacklist = DialogueDB::GetDatabase()->GetBlacklist();
                std::vector<DialogueDB::BlacklistEntry> filteredBlacklist;
                std::string query = searchQuery_;
                std::transform(query.begin(), query.end(), query.begin(), ::tolower);
                
                for (const auto& entry : allBlacklist) {
                    bool matches = true;
                    if (query.length() > 0) {
                        bool foundMatch = false;
                        std::string editorID = entry.targetEditorID;
                        std::transform(editorID.begin(), editorID.end(), editorID.begin(), ::tolower);
                        if (editorID.find(query) != std::string::npos) foundMatch = true;
                        if (!foundMatch && !entry.responseText.empty()) {
                            std::string responseText = entry.responseText;
                            std::transform(responseText.begin(), responseText.end(), responseText.begin(), ::tolower);
                            if (responseText.find(query) != std::string::npos) foundMatch = true;
                        }
                        if (!foundMatch) matches = false;
                    }
                    if (matches && filterTargetType_ > 0) {
                        if (filterTargetType_ == 4) {
                            if (entry.targetType != DialogueDB::BlacklistTarget::Scene || 
                                entry.filterCategory == "BardSongs" || 
                                entry.filterCategory == "FollowerCommentary") matches = false;
                        } else if (filterTargetType_ == 5) {
                            if (entry.filterCategory != "BardSongs") matches = false;
                        } else if (filterTargetType_ == 6) {
                            if (entry.filterCategory != "FollowerCommentary") matches = false;
                        } else if (filterTargetType_ <= 3) {
                            if (static_cast<int>(entry.targetType) != filterTargetType_) matches = false;
                        }
                    }
                    if (matches) {
                        bool blockTypeMatch = false;
                        if (filterSoftBlock_ && entry.blockType == DialogueDB::BlockType::Soft) blockTypeMatch = true;
                        if (filterHardBlock_ && entry.blockType == DialogueDB::BlockType::Hard) blockTypeMatch = true;
                        if (filterSkyrimNet_ && entry.blockType == DialogueDB::BlockType::SkyrimNet) blockTypeMatch = true;
                        if (!blockTypeMatch) matches = false;
                    }
                    if (matches && !filterTopic_) {
                        if (entry.targetType == DialogueDB::BlacklistTarget::Topic) matches = false;
                    }
                    if (matches && !filterScene_) {
                        if (entry.targetType == DialogueDB::BlacklistTarget::Scene) matches = false;
                    }
                    if (matches) filteredBlacklist.push_back(entry);
                }
                blacklistCache_ = filteredBlacklist;
                blacklistLoadedCount_ = static_cast<int>(blacklistCache_.size());
                
                // Update the selected entry from cache to ensure consistency
                for (auto& entry : blacklistCache_) {
                    if (entry.id == selectedBlacklistEntry_.id) {
                        selectedBlacklistEntry_ = entry;
                        break;
                    }
                }
                
                spdlog::info("[STFUMenu] Updated blacklist entry: blockType={}, audio={}, subtitles={}, skyrimNet={}", 
                             static_cast<int>(newBlockType), blockAudio, blockSubtitles, blockSkyrimNet);
            }
            
            Spacing();
            
            if (Button("Remove from Blacklist", ImVec2(-1, 0))) {
                DialogueDB::GetDatabase()->RemoveFromBlacklist(selectedBlacklistEntry_.id);
                hasBlacklistSelection_ = false;
                selectedBlacklistID_ = -1;
                blacklistCache_ = DialogueDB::GetDatabase()->GetBlacklist();
                spdlog::info("[STFUMenu] Removed blacklist entry ID: {}", selectedBlacklistEntry_.id);
            }
            
            if (Button("Add to Whitelist", ImVec2(-1, 0))) {
                try {
                    auto db = DialogueDB::GetDatabase();
                    // Create whitelist entry from blacklist entry
                    DialogueDB::BlacklistEntry whitelistEntry = selectedBlacklistEntry_;
                    whitelistEntry.id = 0;  // Reset ID for new entry
                    whitelistEntry.filterCategory = "Whitelist";
                    whitelistEntry.addedTimestamp = std::chrono::duration_cast<std::chrono::seconds>(
                        std::chrono::system_clock::now().time_since_epoch()
                    ).count();
                    
                    if (db->AddToWhitelist(whitelistEntry)) {
                        // Remove from blacklist after successful whitelist addition
                        db->RemoveFromBlacklist(selectedBlacklistEntry_.id);
                        hasBlacklistSelection_ = false;
                        selectedBlacklistID_ = -1;
                        blacklistCache_ = db->GetBlacklist();
                        spdlog::info("[STFUMenu] Added to whitelist and removed from blacklist: {}", selectedBlacklistEntry_.targetEditorID);
                    } else {
                        spdlog::error("[STFUMenu] Failed to add to whitelist: {}", selectedBlacklistEntry_.targetEditorID);
                    }
                } catch (const std::exception& e) {
                    spdlog::error("[STFUMenu] Exception adding to whitelist: {}", e.what());
                }
            }
            if (IsItemHovered(ImGuiHoveredFlags_None)) {
                BeginTooltip();
                Text("Add this entry to the whitelist.");
                Text("It will be removed from the blacklist.");
                EndTooltip();
            }
            
            Spacing();
            Separator();
            Spacing();
            
            Text("Block Type Information:");
            Spacing();
            
            TextColored(ImVec4(1.0f, 0.65f, 0.0f, 1.0f), "Soft Block:");
            TextWrapped("Removes audio, subtitles, and animations. Dialogue still appears in history and can be sent to SkyrimNet.");
            
            Spacing();
            
            TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "Hard Block:");
            if (isScene) {
                TextWrapped("Prevents entire scene from starting. Nothing is logged or sent to SkyrimNet.");
            } else {
                TextWrapped("Prevents dialogue from playing. Nothing is logged or sent to SkyrimNet.");
            }
            
            Spacing();
            
            TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "SkyrimNet Only:");
            TextWrapped("Blocks only SkyrimNet logging. Audio and subtitles play normally.");
        }
        EndChild();
    }
}

void STFUMenu::LoadSelectedEntry(int64_t entryID)
{
    selectedEntryID_ = entryID;
    hasSelection_ = false;
    
    // Find entry in cache
    for (const auto& entry : historyCache_) {
        if (entry.id == entryID) {
            selectedEntry_ = entry;
            hasSelection_ = true;
            
            auto db = DialogueDB::GetDatabase();
            
            // Check unified blacklist - handle both scenes and regular dialogue
            int64_t blacklistId = -1;
            
            if (entry.isScene && !entry.sceneEditorID.empty()) {
                // Scene - look up by EditorID in unified blacklist
                auto blacklist = db->GetBlacklist();
                for (const auto& blEntry : blacklist) {
                    if (blEntry.targetType == DialogueDB::BlacklistTarget::Scene && 
                        blEntry.targetEditorID == entry.sceneEditorID) {
                        blacklistId = blEntry.id;
                        break;
                    }
                }
            } else {
                // Regular dialogue - look up by FormID/EditorID
                blacklistId = db->GetBlacklistEntryId(entry.topicFormID, entry.topicEditorID);
            }
            
            if (blacklistId > 0) {
                // Entry is blacklisted - get the actual block type from unified blacklist
                auto blacklist = db->GetBlacklist();
                for (const auto& blEntry : blacklist) {
                    if (blEntry.id == blacklistId) {
                        // Load notes into history notes buffer
                        strncpy_s(historyNotes_, blEntry.notes.c_str(), sizeof(historyNotes_) - 1);
                        historyNotes_[sizeof(historyNotes_) - 1] = '\0';
                        
                        // Load filterCategory
                        selectedFilterCategory_ = blEntry.filterCategory.empty() ? "Blacklist" : blEntry.filterCategory;
                        // For SkyrimNet category, just use index 0 (dropdown will be disabled anyway)
                        filterCategoryIndex_ = (selectedFilterCategory_ == "SkyrimNet") ? 0 : GetCategoryIndex(selectedFilterCategory_, entry.isScene);
                        
                        // Set UI state based on actual blacklist entry
                        if (blEntry.blockType == DialogueDB::BlockType::Hard) {
                            hardBlock_ = true;
                            softBlockEnabled_ = false;
                            skyrimNetOnly_ = false;
                            softBlockAudio_ = blEntry.blockAudio;
                            softBlockSubtitles_ = blEntry.blockSubtitles;
                        } else if (blEntry.blockType == DialogueDB::BlockType::SkyrimNet) {
                            hardBlock_ = false;
                            softBlockEnabled_ = false;
                            skyrimNetOnly_ = true;  // SkyrimNet-only block
                            softBlockAudio_ = false;
                            softBlockSubtitles_ = false;
                        } else {  // Soft
                            hardBlock_ = false;
                            softBlockEnabled_ = true;   // Soft block IS checked
                            skyrimNetOnly_ = false;
                            softBlockAudio_ = blEntry.blockAudio;
                            softBlockSubtitles_ = blEntry.blockSubtitles;
                        }
                        break;
                    }
                }
            } else {
                // Not blacklisted - default settings (ready to block)
                // Clear notes buffer
                historyNotes_[0] = '\0';
                
                // Reset filterCategory to default based on entry type
                if (entry.isBardSong) {
                    selectedFilterCategory_ = "BardSongs";
                    filterCategoryIndex_ = 2;  // BardSongs is third in scene categories
                } else if (entry.isScene) {
                    selectedFilterCategory_ = "Scene";
                    filterCategoryIndex_ = 1;  // Scene is second in scene categories
                } else {
                    selectedFilterCategory_ = "Blacklist";
                    filterCategoryIndex_ = 0;
                }
                
                hardBlock_ = false;
                softBlockEnabled_ = false;  // Soft block unchecked
                skyrimNetOnly_ = false;     // SkyrimNet unchecked
                softBlockAudio_ = true;     // Sub-options checked by default
                softBlockSubtitles_ = true;
            }
            
            break;
        }
    }
}

void STFUMenu::ApplyBlockSettings()
{
    if (!hasSelection_) {
        spdlog::warn("[STFUMenu] ApplyBlockSettings called with no selection");
        return;
    }
    
    spdlog::info("[STFUMenu] ApplyBlockSettings called: hardBlock={}, softBlockEnabled={}, skyrimNetOnly={}, whiteList={}, softBlockAudio={}, softBlockSubtitles={}",
        hardBlock_, softBlockEnabled_, skyrimNetOnly_, whiteList_, softBlockAudio_, softBlockSubtitles_);
    
    auto db = DialogueDB::GetDatabase();
    
    // Check if all blocks are unchecked (unblocking)
    // Sub-options only matter when Soft block checkbox is checked
    bool nothingChecked = !hardBlock_ && !softBlockEnabled_ && !skyrimNetOnly_ && !whiteList_;
    
    spdlog::info("[STFUMenu] nothingChecked={}, isScene={}", nothingChecked, selectedEntry_.isScene);
    
    // Handle both scene and regular dialogue with unified blacklist
    if (nothingChecked) {
        try {
            // UNBLOCK: Remove from blacklist AND whitelist
            int64_t blacklistId = -1;
            int64_t whitelistId = -1;
            
            if ((selectedEntry_.isScene || selectedEntry_.isBardSong) && !selectedEntry_.sceneEditorID.empty()) {
                // All scenes (including bard songs) - look up by scene EditorID in unified blacklist
                auto blacklist = db->GetBlacklist();
                for (const auto& blEntry : blacklist) {
                    if (blEntry.targetType == DialogueDB::BlacklistTarget::Scene && 
                        blEntry.targetEditorID == selectedEntry_.sceneEditorID) {
                        blacklistId = blEntry.id;
                        break;
                    }
                }
                // Check whitelist too
                whitelistId = db->GetWhitelistEntryId(0, selectedEntry_.sceneEditorID);
            } else {
                // Regular dialogue - look up by FormID/EditorID
                blacklistId = db->GetBlacklistEntryId(selectedEntry_.topicFormID, selectedEntry_.topicEditorID);
                whitelistId = db->GetWhitelistEntryId(selectedEntry_.topicFormID, selectedEntry_.topicEditorID);
            }
            
            // Remove from blacklist if found
            if (blacklistId > 0) {
                if (db->RemoveFromBlacklist(blacklistId)) {
                    spdlog::info("[STFUMenu] Removed from blacklist: {}", 
                        (selectedEntry_.isScene || selectedEntry_.isBardSong) ? selectedEntry_.sceneEditorID : selectedEntry_.topicEditorID);
                    selectedEntry_.blockedStatus = DialogueDB::BlockedStatus::Normal;
                } else {
                    spdlog::error("[STFUMenu] Failed to remove from blacklist");
                }
            }
            
            // Remove from whitelist if found
            if (whitelistId > 0) {
                if (db->RemoveFromWhitelist(whitelistId)) {
                    spdlog::info("[STFUMenu] Removed from whitelist: {}", 
                        (selectedEntry_.isScene || selectedEntry_.isBardSong) ? selectedEntry_.sceneEditorID : selectedEntry_.topicEditorID);
                } else {
                    spdlog::error("[STFUMenu] Failed to remove from whitelist");
                }
            }
            
            if (blacklistId <= 0 && whitelistId <= 0) {
                spdlog::warn("[STFUMenu] Entry not found in blacklist or whitelist");
            }
        } catch (const std::exception& e) {
            spdlog::error("[STFUMenu] Exception during unblock operation: {}", e.what());
        }
        
        // Refresh history to update all entries' blocked status
        RefreshHistory();
        
        // Update the selected entry in the cache
        for (auto& historyEntry : historyCache_) {
            if (historyEntry.id == selectedEntryID_) {
                selectedEntry_ = historyEntry;
                break;
            }
        }
        
        return;
    } else if (hardBlock_ || softBlockEnabled_ || skyrimNetOnly_) {
        // BLOCK: Add/update unified blacklist entry
        // Determine block type
        DialogueDB::BlockType blockType;
        if (hardBlock_) {
            blockType = DialogueDB::BlockType::Hard;
        } else if (skyrimNetOnly_) {
            blockType = DialogueDB::BlockType::SkyrimNet;
        } else {  // softBlockEnabled_ must be true
            blockType = DialogueDB::BlockType::Soft;
        }
        
        // Create blacklist entry
        DialogueDB::BlacklistEntry entry;
        
        if ((selectedEntry_.isScene || selectedEntry_.isBardSong) && !selectedEntry_.sceneEditorID.empty()) {
            // First, remove from whitelist if present (for scenes, use EditorID lookup)
            try {
                int64_t whitelistId = db->GetWhitelistEntryId(0, selectedEntry_.sceneEditorID);
                if (whitelistId > 0) {
                    if (db->RemoveFromWhitelist(whitelistId)) {
                        spdlog::info("[STFUMenu] Removed scene from whitelist before blocking: {}", selectedEntry_.sceneEditorID);
                    } else {
                        spdlog::error("[STFUMenu] Failed to remove scene from whitelist: {}", selectedEntry_.sceneEditorID);
                    }
                }
            } catch (const std::exception& e) {
                spdlog::error("[STFUMenu] Exception checking/removing scene whitelist entry: {}", e.what());
            }
            
            // All scenes (including bard songs) - add to unified blacklist with target_type=Scene
            entry.targetType = DialogueDB::BlacklistTarget::Scene;
            entry.targetFormID = 0;  // Scenes identified by EditorID
            entry.targetEditorID = selectedEntry_.sceneEditorID;
            entry.blockType = blockType;
            entry.addedTimestamp = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count();
            entry.notes = historyNotes_;  // Use notes from textbox
            entry.responseText = "";  // Scenes don't have response text
            entry.subtype = 14;  // Scene subtype
            entry.subtypeName = "Scene";
            // SkyrimNet blocks use special "SkyrimNet" category, others use selected category
            entry.filterCategory = (blockType == DialogueDB::BlockType::SkyrimNet) ? "SkyrimNet" : selectedFilterCategory_;
            entry.sourcePlugin = selectedEntry_.topicSourcePlugin;  // Use topic source for blacklisting
            
            // Save granular blocking options
            // SkyrimNet-only blocks never block audio or subtitles
            // Soft blocks always block both audio and subtitles
            if (blockType == DialogueDB::BlockType::SkyrimNet) {
                entry.blockAudio = false;
                entry.blockSubtitles = false;
            } else {
                entry.blockAudio = true;
                entry.blockSubtitles = true;
            }
            // Soft block always includes SkyrimNet blocking
            entry.blockSkyrimNet = (blockType == DialogueDB::BlockType::Soft || blockType == DialogueDB::BlockType::SkyrimNet);
        
        try {
            if (!db->AddToBlacklist(entry)) {
                spdlog::error("[STFUMenu] Failed to add scene to blacklist: {}", selectedEntry_.sceneEditorID);
                return;
            }
        } catch (const std::exception& e) {
            spdlog::error("[STFUMenu] Exception adding scene to blacklist: {}", e.what());
            return;
        }
        
        spdlog::info("[STFUMenu] Added scene to blacklist: {} (Type: {})", 
                     selectedEntry_.sceneEditorID, static_cast<int>(blockType));
        
        // Update selected entry status
        if (blockType == DialogueDB::BlockType::Hard) {
            selectedEntry_.blockedStatus = DialogueDB::BlockedStatus::HardBlock;
        } else if (blockType == DialogueDB::BlockType::SkyrimNet) {
            selectedEntry_.blockedStatus = DialogueDB::BlockedStatus::SkyrimNetBlock;
        } else {
            selectedEntry_.blockedStatus = DialogueDB::BlockedStatus::SoftBlock;
        }
        
        RefreshHistory();
            for (auto& historyEntry : historyCache_) {
                if (historyEntry.id == selectedEntryID_) {
                    selectedEntry_ = historyEntry;
                    break;
                }
            }
            return;
        }
        
        // If we get here, it's regular dialogue, not a scene
        // Continue to regular dialogue handling below
    }
    
    // Regular dialogue handling (not a scene)
    if (nothingChecked) {
        try {
            // UNBLOCK: Remove from blacklist
            int64_t entryId = db->GetBlacklistEntryId(selectedEntry_.topicFormID, selectedEntry_.topicEditorID);
            if (entryId > 0) {
                if (db->RemoveFromBlacklist(entryId)) {
                    spdlog::info("[STFUMenu] Removed from blacklist: {}", selectedEntry_.topicEditorID);
                } else {
                    spdlog::error("[STFUMenu] Failed to remove from blacklist: {}", selectedEntry_.topicEditorID);
                    return;
                }
            } else {
                spdlog::warn("[STFUMenu] Entry not found in blacklist: {}", selectedEntry_.topicEditorID);
            }
        } catch (const std::exception& e) {
            spdlog::error("[STFUMenu] Exception during dialogue unblock: {}", e.what());
            return;
        }
        
        // Refresh history to update all entries
        RefreshHistory();
        
        // Update the selected entry in the cache
        for (auto& historyEntry : historyCache_) {
            if (historyEntry.id == selectedEntryID_) {
                selectedEntry_ = historyEntry;
                break;
            }
        }
        return;
    } else if (hardBlock_ || softBlockEnabled_ || skyrimNetOnly_) {
        // BLOCK: Add/update blacklist entry
        // First, remove from whitelist if present
        try {
            int64_t whitelistId = db->GetWhitelistEntryId(selectedEntry_.topicFormID, selectedEntry_.topicEditorID);
            if (whitelistId > 0) {
                if (db->RemoveFromWhitelist(whitelistId)) {
                    spdlog::info("[STFUMenu] Removed from whitelist before blocking: {}", selectedEntry_.topicEditorID);
                } else {
                    spdlog::error("[STFUMenu] Failed to remove from whitelist: {}", selectedEntry_.topicEditorID);
                }
            }
        } catch (const std::exception& e) {
            spdlog::error("[STFUMenu] Exception checking/removing whitelist entry: {}", e.what());
        }
        
        // Determine block type based on which option is checked
        DialogueDB::BlockType blockType;
        if (hardBlock_) {
            blockType = DialogueDB::BlockType::Hard;
        } else if (skyrimNetOnly_) {
            blockType = DialogueDB::BlockType::SkyrimNet;
        } else {  // softBlockEnabled_ must be true
            blockType = DialogueDB::BlockType::Soft;
        }
        
        // Create blacklist entry
        DialogueDB::BlacklistEntry entry;
        entry.targetType = DialogueDB::BlacklistTarget::Topic;
        entry.targetFormID = selectedEntry_.topicFormID;
        entry.targetEditorID = selectedEntry_.topicEditorID;
        entry.questEditorID = selectedEntry_.questEditorID;  // For ESL-safe matching
        entry.blockType = blockType;
        entry.addedTimestamp = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
        entry.notes = historyNotes_;  // Use notes from textbox
        // Use allResponses if available, otherwise wrap single responseText in JSON
        if (!selectedEntry_.allResponses.empty()) {
            entry.responseText = DialogueDB::ResponsesToJson(selectedEntry_.allResponses);
        } else if (!selectedEntry_.responseText.empty()) {
            entry.responseText = DialogueDB::ResponsesToJson({selectedEntry_.responseText});
        }
        entry.subtype = selectedEntry_.topicSubtype;  // Store subtype for filtering
        entry.subtypeName = selectedEntry_.topicSubtypeName;  // Store subtype name for display
        // SkyrimNet blocks use special "SkyrimNet" category, others use selected category
        entry.filterCategory = (blockType == DialogueDB::BlockType::SkyrimNet) ? "SkyrimNet" : selectedFilterCategory_;
        entry.sourcePlugin = selectedEntry_.topicSourcePlugin;  // Use topic source for blacklisting
        
        // Save granular blocking options
        // SkyrimNet-only blocks never block audio or subtitles
        // Soft blocks always block both audio and subtitles
        if (blockType == DialogueDB::BlockType::SkyrimNet) {
            entry.blockAudio = false;
            entry.blockSubtitles = false;
        } else {
            entry.blockAudio = true;
            entry.blockSubtitles = true;
        }
        // Soft block always includes SkyrimNet blocking
        entry.blockSkyrimNet = (blockType == DialogueDB::BlockType::Soft || blockType == DialogueDB::BlockType::SkyrimNet);
        
        spdlog::info("[STFUMenu] Attempting to add to blacklist: EditorID='{}', FormID=0x{:08X}, TargetType={}, BlockType={}, Audio={}, Subtitles={}, SkyrimNet={}",
            entry.targetEditorID, entry.targetFormID, static_cast<int>(entry.targetType), static_cast<int>(entry.blockType),
            entry.blockAudio, entry.blockSubtitles, entry.blockSkyrimNet);
        
        try {
            if (!db->AddToBlacklist(entry)) {
                spdlog::error("[STFUMenu] Failed to add to blacklist: {}", selectedEntry_.topicEditorID);
                return;
            }
        } catch (const std::exception& e) {
            spdlog::error("[STFUMenu] Exception adding dialogue to blacklist: {}", e.what());
            return;
        }
        
        spdlog::info("[STFUMenu] Added to blacklist: {} (Type: {})", 
                     selectedEntry_.topicEditorID, static_cast<int>(blockType));
    } else if (whiteList_) {
        // WHITELIST: Add entry to whitelist
        try {
            auto db = DialogueDB::GetDatabase();
            
            // First, remove from blacklist if present
            int64_t blacklistId = -1;
            if ((selectedEntry_.isScene || selectedEntry_.isBardSong) && !selectedEntry_.sceneEditorID.empty()) {
                // For scenes, lookup by EditorID
                blacklistId = db->GetBlacklistEntryId(0, selectedEntry_.sceneEditorID);
            } else {
                // For regular dialogue, lookup by FormID and EditorID
                blacklistId = db->GetBlacklistEntryId(selectedEntry_.topicFormID, selectedEntry_.topicEditorID);
            }
            
            if (blacklistId > 0) {
                if (db->RemoveFromBlacklist(blacklistId)) {
                    spdlog::info("[STFUMenu] Removed from blacklist before whitelisting: {}", 
                        selectedEntry_.isScene ? selectedEntry_.sceneEditorID : selectedEntry_.topicEditorID);
                } else {
                    spdlog::error("[STFUMenu] Failed to remove from blacklist: {}", 
                        selectedEntry_.isScene ? selectedEntry_.sceneEditorID : selectedEntry_.topicEditorID);
                }
            }
            
            // Create whitelist entry
            DialogueDB::BlacklistEntry entry;
            
            if ((selectedEntry_.isScene || selectedEntry_.isBardSong) && !selectedEntry_.sceneEditorID.empty()) {
                // Scene/Bard Song entry
                entry.targetType = DialogueDB::BlacklistTarget::Scene;
                entry.targetFormID = 0;
                entry.targetEditorID = selectedEntry_.sceneEditorID;
                entry.questEditorID = "";  // Scenes don't have quest context
                entry.blockType = DialogueDB::BlockType::Soft;  // Default to soft for whitelist
                entry.subtype = 14;  // Scene subtype
                entry.subtypeName = "Scene";
                entry.sourcePlugin = selectedEntry_.topicSourcePlugin;
            } else {
                // Regular dialogue entry
                entry.targetType = DialogueDB::BlacklistTarget::Topic;
                entry.targetFormID = selectedEntry_.topicFormID;
                entry.targetEditorID = selectedEntry_.topicEditorID;
                entry.questEditorID = selectedEntry_.questEditorID;  // For ESL-safe matching
                entry.blockType = DialogueDB::BlockType::Soft;  // Default to soft for whitelist
                entry.subtype = selectedEntry_.topicSubtype;
                entry.subtypeName = selectedEntry_.topicSubtypeName;
                entry.sourcePlugin = selectedEntry_.topicSourcePlugin;
                
                // Use allResponses if available
                if (!selectedEntry_.allResponses.empty()) {
                    entry.responseText = DialogueDB::ResponsesToJson(selectedEntry_.allResponses);
                } else if (!selectedEntry_.responseText.empty()) {
                    entry.responseText = DialogueDB::ResponsesToJson({selectedEntry_.responseText});
                }
            }
            
            entry.addedTimestamp = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count();
            entry.notes = historyNotes_;
            entry.filterCategory = "Whitelist";
            entry.blockAudio = false;
            entry.blockSubtitles = false;
            entry.blockSkyrimNet = false;
            
            if (db->AddToWhitelist(entry)) {
                spdlog::info("[STFUMenu] Added to whitelist: {}", entry.targetEditorID);
                RefreshHistory();
                
                // Update the selected entry in the cache
                for (auto& historyEntry : historyCache_) {
                    if (historyEntry.id == selectedEntryID_) {
                        selectedEntry_ = historyEntry;
                        break;
                    }
                }
            } else {
                spdlog::error("[STFUMenu] Failed to add to whitelist: {}", entry.targetEditorID);
            }
        } catch (const std::exception& e) {
            spdlog::error("[STFUMenu] Exception adding to whitelist: {}", e.what());
        }
        return;
    }
    
    // Refresh history to update all entries with the same topic
    RefreshHistory();
    
    // Also refresh blacklist and whitelist caches to show updated data immediately
    blacklistCache_ = DialogueDB::GetDatabase()->GetBlacklist();
    whitelistCache_ = DialogueDB::GetDatabase()->GetWhitelist();
    
   // Update the selected entry in the cache
    for (auto& historyEntry : historyCache_) {
        if (historyEntry.id == selectedEntryID_) {
            selectedEntry_ = historyEntry;
            break;
        }
    }
}

void STFUMenu::RenderWhitelistTab()
{
    using namespace ImGuiMCP;
    
    // Load whitelist on first view (first 100 entries)
    static bool firstLoad = true;
    if (firstLoad) {
        whitelistCache_ = DialogueDB::GetDatabase()->GetWhitelist();
        whitelistLoadedCount_ = whitelistPageSize_;
        firstLoad = false;
    }
    
    // Get available space
    ImVec2 availSpace;
    GetContentRegionAvail(&availSpace);
    
    // Top panel: Compact search bar
    if (BeginChild("WhitelistSearchPanel", ImVec2(0, 151), ImGuiChildFlags_Border, ImGuiWindowFlags_None)) {
        // Line 1: Search field and Type filter
        PushItemWidth(400);
        bool searchTriggered = InputText("##WhitelistSearchQuery", whitelistSearchQuery_, sizeof(whitelistSearchQuery_), ImGuiInputTextFlags_EnterReturnsTrue, nullptr, nullptr);
        PopItemWidth();
        SameLine();
        Text("Type:");
        SameLine();
        PushItemWidth(150);
        const char* types[] = { "All", "Topic", "Quest", "Subtype", "Scene", "Bard Song", "Follower" };
        Combo("##WhitelistFilterType", &whitelistFilterTargetType_, types, 7);
        PopItemWidth();
        
        // Line 2: Filter checkboxes
        Text("Filters:");
        SameLine();
        bool topicChanged = Checkbox("Topics##WL", &whitelistFilterTopic_);
        SameLine();
        bool sceneChanged = Checkbox("Scenes##WL", &whitelistFilterScene_);
        
        bool filterChanged = topicChanged || sceneChanged;
        
        // Line 3: Action buttons
        if (Button("Search##WL", ImVec2(120, 0)) || searchTriggered || filterChanged) {
            auto allWhitelist = DialogueDB::GetDatabase()->GetWhitelist();
            std::vector<DialogueDB::BlacklistEntry> filteredWhitelist;
            
            std::string query = whitelistSearchQuery_;
            std::transform(query.begin(), query.end(), query.begin(), ::tolower);
            
            for (const auto& entry : allWhitelist) {
                bool matches = true;
                
                // Search query
                if (query.length() > 0) {
                    bool foundMatch = false;
                    
                    std::string editorID = entry.targetEditorID;
                    std::transform(editorID.begin(), editorID.end(), editorID.begin(), ::tolower);
                    if (editorID.find(query) != std::string::npos) foundMatch = true;
                    
                    if (!foundMatch && !entry.responseText.empty()) {
                        std::string responseText = entry.responseText;
                        std::transform(responseText.begin(), responseText.end(), responseText.begin(), ::tolower);
                        if (responseText.find(query) != std::string::npos) foundMatch = true;
                    }
                    
                    if (!foundMatch && !entry.subtypeName.empty()) {
                        std::string subtypeName = entry.subtypeName;
                        std::transform(subtypeName.begin(), subtypeName.end(), subtypeName.begin(), ::tolower);
                        if (subtypeName.find(query) != std::string::npos) foundMatch = true;
                    }
                    
                    if (!foundMatch && !entry.notes.empty()) {
                        std::string notes = entry.notes;
                        std::transform(notes.begin(), notes.end(), notes.begin(), ::tolower);
                        if (notes.find(query) != std::string::npos) foundMatch = true;
                    }
                    
                    if (!foundMatch && entry.targetFormID > 0) {
                        char formIDStr[16];
                        sprintf_s(formIDStr, "%X", entry.targetFormID);
                        std::string dbFormID = formIDStr;
                        std::string searchFormID = query;
                        
                        if (searchFormID.find("0x") == 0) searchFormID = searchFormID.substr(2);
                        
                        auto stripZeroes = [](std::string& s) {
                            size_t pos = s.find_first_not_of('0');
                            if (pos != std::string::npos) s = s.substr(pos);
                            else s = "0";
                        };
                        stripZeroes(searchFormID);
                        stripZeroes(dbFormID);
                        
                        std::transform(searchFormID.begin(), searchFormID.end(), searchFormID.begin(), ::tolower);
                        std::transform(dbFormID.begin(), dbFormID.end(), dbFormID.begin(), ::tolower);
                        
                        if (dbFormID.find(searchFormID) != std::string::npos) foundMatch = true;
                    }
                    
                    if (!foundMatch) matches = false;
                }
                
                // Filter by target type
                if (matches && whitelistFilterTargetType_ > 0) {
                    if (whitelistFilterTargetType_ == 4) {
                        // Scene filter - exclude bard songs and follower commentary
                        if (entry.targetType != DialogueDB::BlacklistTarget::Scene || 
                            entry.filterCategory == "BardSongs" || 
                            entry.filterCategory == "FollowerCommentary") {
                            matches = false;
                        }
                    } else if (whitelistFilterTargetType_ == 5) {
                        // Bard Song filter
                        if (entry.filterCategory != "BardSongs") {
                            matches = false;
                        }
                    } else if (whitelistFilterTargetType_ == 6) {
                        // Follower Commentary filter
                        if (entry.filterCategory != "FollowerCommentary") {
                            matches = false;
                        }
                    } else if (whitelistFilterTargetType_ <= 3) {
                        if (static_cast<int>(entry.targetType) != whitelistFilterTargetType_) {
                            matches = false;
                        }
                    }
                }
                
                // Filter by target type (topics)
                if (matches && !whitelistFilterTopic_) {
                    if (entry.targetType == DialogueDB::BlacklistTarget::Topic) {
                        matches = false;
                    }
                }
                
                // Filter by target type (scenes)
                if (matches && !whitelistFilterScene_) {
                    if (entry.targetType == DialogueDB::BlacklistTarget::Scene) {
                        matches = false;
                    }
                }
                
                if (matches) {
                    filteredWhitelist.push_back(entry);
                }
            }
            
            whitelistCache_ = filteredWhitelist;
            whitelistLoadedCount_ = static_cast<int>(whitelistCache_.size());
            
            spdlog::info("[STFUMenu] Whitelist search returned {} entries", whitelistCache_.size());
        }
        
        SameLine();
        
        if (Button("Reset Filters##WL", ImVec2(120, 0))) {
            whitelistSearchQuery_[0] = '\0';
            whitelistFilterTargetType_ = 0;
            whitelistFilterTopic_ = true;
            whitelistFilterScene_ = true;
            whitelistCache_ = DialogueDB::GetDatabase()->GetWhitelist();
            whitelistLoadedCount_ = whitelistPageSize_;
        }
        
        SameLine();
        
        if (Button("Refresh##WL", ImVec2(120, 0))) {
            auto allWhitelist = DialogueDB::GetDatabase()->GetWhitelist();
            
            // Apply current filters to the refreshed list
            std::vector<DialogueDB::BlacklistEntry> filteredWhitelist;
            std::string query = whitelistSearchQuery_;
            std::transform(query.begin(), query.end(), query.begin(), ::tolower);
            
            for (const auto& entry : allWhitelist) {
                bool matches = true;
                
                // Search query
                if (query.length() > 0) {
                    bool foundMatch = false;
                    
                    std::string editorID = entry.targetEditorID;
                    std::transform(editorID.begin(), editorID.end(), editorID.begin(), ::tolower);
                    if (editorID.find(query) != std::string::npos) foundMatch = true;
                    
                    if (!foundMatch && !entry.responseText.empty()) {
                        std::string responseText = entry.responseText;
                        std::transform(responseText.begin(), responseText.end(), responseText.begin(), ::tolower);
                        if (responseText.find(query) != std::string::npos) foundMatch = true;
                    }
                    
                    if (!foundMatch) matches = false;
                }
                
                // Filter by target type
                if (matches && whitelistFilterTargetType_ > 0) {
                    if (whitelistFilterTargetType_ == 4) {
                        // Scene filter - exclude bard songs and follower commentary
                        if (entry.targetType != DialogueDB::BlacklistTarget::Scene || 
                            entry.filterCategory == "BardSongs" || 
                            entry.filterCategory == "FollowerCommentary") {
                            matches = false;
                        }
                    } else if (whitelistFilterTargetType_ == 5) {
                        // Bard Song filter
                        if (entry.filterCategory != "BardSongs") {
                            matches = false;
                        }
                    } else if (whitelistFilterTargetType_ == 6) {
                        // Follower Commentary filter
                        if (entry.filterCategory != "FollowerCommentary") {
                            matches = false;
                        }
                    } else if (whitelistFilterTargetType_ <= 3) {
                        if (static_cast<int>(entry.targetType) != whitelistFilterTargetType_) {
                            matches = false;
                        }
                    }
                }
                
                // Filter by target type (topics)
                if (matches && !whitelistFilterTopic_) {
                    if (entry.targetType == DialogueDB::BlacklistTarget::Topic) {
                        matches = false;
                    }
                }
                
                // Filter by target type (scenes)
                if (matches && !whitelistFilterScene_) {
                    if (entry.targetType == DialogueDB::BlacklistTarget::Scene) {
                        matches = false;
                    }
                }
                
                if (matches) {
                    filteredWhitelist.push_back(entry);
                }
            }
            
            whitelistCache_ = filteredWhitelist;
            whitelistLoadedCount_ = static_cast<int>(whitelistCache_.size());
        }
        
        SameLine();
        Text("Results: %zu", whitelistCache_.size());
        
        // Manual Entry button on far right
        SameLine();
        ImVec2 availWidthWL;
        GetContentRegionAvail(&availWidthWL);
        float buttonWidthWL = 150.0f;
        SetCursorPosX(GetCursorPosX() + availWidthWL.x - buttonWidthWL);
        
        if (Button("Manual Entry##WL", ImVec2(buttonWidthWL, 0))) {
            showManualEntryModal_ = true;
            isManualEntryForWhitelist_ = true;  // Whitelist
            manualEntryIdentifier_[0] = '\0';
            manualEntryBlockType_ = 0;
            manualEntryBlockAudio_ = false;  // Whitelist defaults to not blocking
            manualEntryBlockSubtitles_ = false;
            manualEntryBlockSkyrimNet_ = false;
            manualEntryNotes_[0] = '\0';
            manualEntryFilterCategoryIndex_ = 0;
        }
    }
    EndChild();
    
    Separator();
    
    // Middle panel: Results list
    float remainingHeight = availSpace.y - 165.0f;
    float resultsHeight = remainingHeight * 0.40f;
    float detailHeight = remainingHeight * 0.60f - 10.0f;
    
    if (BeginChild("WhitelistResults", ImVec2(0, resultsHeight), ImGuiChildFlags_Border, ImGuiWindowFlags_None)) {
        int displayCount = 0;
        int maxDisplay = whitelistLoadedCount_;
        
        int entryIndex = 0;
        for (const auto& entry : whitelistCache_) {
            if (displayCount >= maxDisplay) break;
            displayCount++;
            
            PushID(static_cast<int>(entry.id));
            
            // Format entry display
            const char* targetTypeStr = "";
            ImVec4 typeColor = ImVec4(0.4f, 0.8f, 1.0f, 1.0f);
            switch (entry.targetType) {
                case DialogueDB::BlacklistTarget::Topic:   targetTypeStr = "Topic"; break;
                case DialogueDB::BlacklistTarget::Quest:   targetTypeStr = "Quest"; break;
                case DialogueDB::BlacklistTarget::Subtype: targetTypeStr = "Subtype"; break;
                case DialogueDB::BlacklistTarget::Scene:
                    targetTypeStr = "Scene";
                    typeColor = ImVec4(0.4f, 1.0f, 0.8f, 1.0f);
                    break;
                case DialogueDB::BlacklistTarget::Plugin:
                    targetTypeStr = "Plugin";
                    typeColor = ImVec4(1.0f, 0.8f, 0.4f, 1.0f);
                    break;
            }
            
            // Whitelisted entries always show "ALLOWED" with pale lavender color
            std::string statusStr = "ALLOWED";
            ImU32 statusColor = COLOR_WHITELIST;
            ImVec4 statusColorVec = ImVec4(
                ((statusColor >> IM_COL32_R_SHIFT) & 0xFF) / 255.0f,
                ((statusColor >> IM_COL32_G_SHIFT) & 0xFF) / 255.0f,
                ((statusColor >> IM_COL32_B_SHIFT) & 0xFF) / 255.0f,
                ((statusColor >> IM_COL32_A_SHIFT) & 0xFF) / 255.0f
            );
            
            ImVec2 cursorPos;
            GetCursorScreenPos(&cursorPos);
            
            // Check if selected
            bool isSelected = false;
            if (!selectedWhitelistIDs_.empty()) {
                for (size_t i = 0; i < selectedWhitelistIDs_.size(); ++i) {
                    if (selectedWhitelistIDs_[i] == entry.id) {
                        isSelected = true;
                        break;
                    }
                }
            } else {
                isSelected = (selectedWhitelistID_ == entry.id);
            }
            
            if (Selectable("##whitelistentry", isSelected, ImGuiSelectableFlags_AllowOverlap, ImVec2(0, 0))) {
                ImGuiIO* io = GetIO();
                bool ctrlPressed = io->KeyCtrl;
                bool shiftPressed = io->KeyShift;
                
                if (shiftPressed && lastClickedWhitelistIndex_ >= 0) {
                    // Shift+click: Select range
                    selectedWhitelistIDs_.clear();
                    
                    int start = (std::min)(lastClickedWhitelistIndex_, entryIndex);
                    int end = (std::max)(lastClickedWhitelistIndex_, entryIndex);
                    
                    int idx = 0;
                    for (const auto& e : whitelistCache_) {
                        if (idx >= start && idx <= end) {
                            selectedWhitelistIDs_.push_back(e.id);
                        }
                        idx++;
                    }
                    
                    hasWhitelistSelection_ = false;
                } else if (ctrlPressed) {
                    // Ctrl+click: Toggle selection
                    bool found = false;
                    for (size_t i = 0; i < selectedWhitelistIDs_.size(); ++i) {
                        if (selectedWhitelistIDs_[i] == entry.id) {
                            selectedWhitelistIDs_.erase(selectedWhitelistIDs_.begin() + i);
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        selectedWhitelistIDs_.push_back(entry.id);
                    }
                    
                    lastClickedWhitelistIndex_ = entryIndex;
                    hasWhitelistSelection_ = false;
                } else {
                    // Normal click: Single selection
                    selectedWhitelistIDs_.clear();
                    selectedWhitelistIDs_.push_back(entry.id);
                    
                    selectedWhitelistID_ = entry.id;
                    selectedWhitelistEntry_ = entry;
                    hasWhitelistSelection_ = true;
                    lastClickedWhitelistIndex_ = entryIndex;
                    
                    strncpy_s(whitelistNotes_, entry.notes.c_str(), sizeof(whitelistNotes_) - 1);
                    whitelistNotes_[sizeof(whitelistNotes_) - 1] = '\0';
                    
                    spdlog::info("[STFUMenu] Selected whitelist entry ID: {}", entry.id);
                }
            }
            
            // Draw colored text
            SetCursorScreenPos(cursorPos);
            
            TextColored(typeColor, "[%s]", targetTypeStr);
            SameLine();
            TextColored(statusColorVec, "[%s]", statusStr.c_str());
            SameLine();
            
            if (!entry.targetEditorID.empty()) {
                TextColored(ImVec4(1.0f, 1.0f, 0.4f, 1.0f), "%s", entry.targetEditorID.c_str());
            } else {
                TextColored(ImVec4(1.0f, 1.0f, 0.4f, 1.0f), "0%X", entry.targetFormID);
            }
            
            if (!entry.notes.empty()) {
                SameLine();
                TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), " - %s", entry.notes.c_str());
            } else if (!entry.responseText.empty()) {
                auto responses = ParseResponsesJson(entry.responseText);
                if (!responses.empty()) {
                    SameLine();
                    TextColored(ImVec4(0.9f, 0.9f, 0.9f, 1.0f), " - %s", responses[0].c_str());
                }
            }
            PopID();
            entryIndex++;
        }
        
        // Auto-load more on scroll
        float scrollY = GetScrollY();
        float scrollMaxY = GetScrollMaxY();
        if (scrollMaxY > 0 && (scrollMaxY - scrollY) < 50.0f) {
            int totalEntries = static_cast<int>(whitelistCache_.size());
            if (whitelistLoadedCount_ < totalEntries) {
                int newCount = whitelistLoadedCount_ + whitelistPageSize_;
                whitelistLoadedCount_ = (newCount < totalEntries) ? newCount : totalEntries;
            }
        }
        
        // Handle Delete key to remove selected entries
        if (IsKeyPressed(ImGuiKey_Delete, false)) {
            if (!selectedWhitelistIDs_.empty()) {
                // Delete multiple selected entries
                int deletedCount = DialogueDB::GetDatabase()->RemoveFromWhitelistBatch(selectedWhitelistIDs_);
                if (deletedCount > 0) {
                    spdlog::info("[STFUMenu] Deleted {} whitelist entries", deletedCount);
                    selectedWhitelistIDs_.clear();
                    selectedWhitelistID_ = -1;
                    hasWhitelistSelection_ = false;
                    lastClickedWhitelistIndex_ = -1;
                    needsWhitelistRefresh_ = true;
                }
            } else if (selectedWhitelistID_ != -1) {
                // Delete single selected entry
                if (DialogueDB::GetDatabase()->RemoveFromWhitelist(selectedWhitelistID_)) {
                    spdlog::info("[STFUMenu] Deleted whitelist entry {}", selectedWhitelistID_);
                    selectedWhitelistID_ = -1;
                    hasWhitelistSelection_ = false;
                    lastClickedWhitelistIndex_ = -1;
                    needsWhitelistRefresh_ = true;
                }
            }
        }
    }
    EndChild();
    
    Separator();
    
    // Bottom panel: Detail panel (simplified - just show message for now)
    if (BeginChild("WhitelistDetailPanel", ImVec2(0, detailHeight), ImGuiChildFlags_Border, ImGuiWindowFlags_None)) {
        if (selectedWhitelistIDs_.empty()) {
            TextColored(ImVec4(0.8f, 0.8f, 1.0f, 1.0f), "Multi-Selection Controls:");
            Spacing();
            TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "  Click - Select single entry");
            TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "  Ctrl+Click - Add/remove entry from selection");
            TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "  Shift+Click - Select range from last clicked to current");
            Spacing();
            Separator();
            Spacing();
            TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Select an entry above to view details and modify settings.");
        } else if (selectedWhitelistIDs_.size() > 1) {
            TextColored(ImVec4(0.8f, 0.8f, 1.0f, 1.0f), "%zu entries selected", selectedWhitelistIDs_.size());
            Spacing();
            
            if (Button("Remove Selected from Whitelist", ImVec2(-1, 0))) {
                try {
                    auto db = DialogueDB::GetDatabase();
                    int removedCount = 0;
                    
                    for (size_t i = 0; i < selectedWhitelistIDs_.size(); ++i) {
                        if (db->RemoveFromWhitelist(selectedWhitelistIDs_[i])) {
                            removedCount++;
                        }
                    }
                    
                    selectedWhitelistIDs_.clear();
                    hasWhitelistSelection_ = false;
                    selectedWhitelistID_ = -1;
                    lastClickedWhitelistIndex_ = -1;
                    whitelistCache_ = db->GetWhitelist();
                    
                    spdlog::info("[STFUMenu] Bulk removed {} whitelist entries", removedCount);
                } catch (const std::exception& e) {
                    spdlog::error("[STFUMenu] Exception during whitelist multi-selection removal: {}", e.what());
                }
            }
        } else if (hasWhitelistSelection_) {
            // Get available width for split layout
            ImVec2 availSpace;
            GetContentRegionAvail(&availSpace);
            float leftWidth = availSpace.x * 0.50f;
            float rightWidth = availSpace.x * 0.50f - 10.0f;
            
            // Check if this is a scene entry
            bool isScene = (selectedWhitelistEntry_.targetType == DialogueDB::BlacklistTarget::Scene);
            
            if (BeginChild("WhitelistDetails", ImVec2(leftWidth, 0), ImGuiChildFlags_Border, ImGuiWindowFlags_None)) {
                Text(isScene ? "Scene Whitelist Entry:" : "Whitelist Entry Details:");
                Separator();
                Spacing();
                
                Text("ID: %lld", selectedWhitelistEntry_.id);
                
                // Show Editor ID with FormID if available
                if (selectedWhitelistEntry_.targetFormID > 0) {
                    if (isScene) {
                        Text("Scene ID: %s (0%X)", selectedWhitelistEntry_.targetEditorID.c_str(), selectedWhitelistEntry_.targetFormID);
                    } else {
                        Text("Topic: %s (0%X)", selectedWhitelistEntry_.targetEditorID.c_str(), selectedWhitelistEntry_.targetFormID);
                    }
                } else {
                    if (isScene) {
                        Text("Scene ID: %s", selectedWhitelistEntry_.targetEditorID.c_str());
                    } else {
                        Text("Topic: %s", selectedWhitelistEntry_.targetEditorID.c_str());
                    }
                }
                
                // Show source plugin if available
                if (!selectedWhitelistEntry_.sourcePlugin.empty()) {
                    Text("Source: %s", selectedWhitelistEntry_.sourcePlugin.c_str());
                }
                
                // Show type
                Text("Type: ");
                SameLine();
                if (isScene) {
                    TextColored(ImVec4(0.4f, 0.8f, 0.8f, 1.0f), "Scene");
                } else {
                    TextColored(ImVec4(0.8f, 0.8f, 0.4f, 1.0f), "Topic");
                }
                
                // Show added timestamp
                Text("Added: %s", FormatTimestamp(selectedWhitelistEntry_.addedTimestamp).c_str());
                
                Spacing();
                
                // Show Responses button
                if (!selectedWhitelistEntry_.responseText.empty()) {
                    if (Button("Show All Responses", ImVec2(-1, 0))) {
                        popupResponses_ = ParseResponsesJson(selectedWhitelistEntry_.responseText);
                        popupTitle_ = "All Responses: " + selectedWhitelistEntry_.targetEditorID;
                        showResponsesPopup_ = true;
                    }
                    if (IsItemHovered(ImGuiHoveredFlags_None)) {
                        BeginTooltip();
                        std::string typeStr = isScene ? "scene" : "topic";
                        Text("View all responses for this %s", typeStr.c_str());
                        EndTooltip();
                    }
                } else {
                    TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No responses captured yet");
                    if (IsItemHovered(ImGuiHoveredFlags_None)) {
                        BeginTooltip();
                        Text("Responses will be captured when this dialogue plays in-game");
                        EndTooltip();
                    }
                }
                
                // Show subtype for non-scene entries
                if (!isScene && selectedWhitelistEntry_.subtype > 0) {
                    Spacing();
                    Text("Subtype: %s", 
                         selectedWhitelistEntry_.subtypeName.empty() ? "Unknown" : selectedWhitelistEntry_.subtypeName.c_str());
                }
                
                Spacing();
                Separator();
                Spacing();
                
                Text("Notes:");
                InputTextMultiline("##WhitelistNotes", whitelistNotes_, sizeof(whitelistNotes_), ImVec2(-1, 80), ImGuiInputTextFlags_None, nullptr, nullptr);
                
                if (Button("Update Notes", ImVec2(-1, 0))) {
                    selectedWhitelistEntry_.notes = whitelistNotes_;
                    DialogueDB::GetDatabase()->AddToWhitelist(selectedWhitelistEntry_);
                    
                    // Reload with filters applied
                    auto allWhitelist = DialogueDB::GetDatabase()->GetWhitelist();
                    std::vector<DialogueDB::BlacklistEntry> filteredWhitelist;
                    std::string query = whitelistSearchQuery_;
                    std::transform(query.begin(), query.end(), query.begin(), ::tolower);
                    
                    for (const auto& entry : allWhitelist) {
                        bool matches = true;
                        if (query.length() > 0) {
                            bool foundMatch = false;
                            std::string editorID = entry.targetEditorID;
                            std::transform(editorID.begin(), editorID.end(), editorID.begin(), ::tolower);
                            if (editorID.find(query) != std::string::npos) foundMatch = true;
                            if (!foundMatch && !entry.responseText.empty()) {
                                std::string responseText = entry.responseText;
                                std::transform(responseText.begin(), responseText.end(), responseText.begin(), ::tolower);
                                if (responseText.find(query) != std::string::npos) foundMatch = true;
                            }
                            if (!foundMatch) matches = false;
                        }
                        if (matches && whitelistFilterTargetType_ > 0) {
                            if (whitelistFilterTargetType_ == 4) {
                                if (entry.targetType != DialogueDB::BlacklistTarget::Scene || 
                                    entry.filterCategory == "BardSongs" || 
                                    entry.filterCategory == "FollowerCommentary") matches = false;
                            } else if (whitelistFilterTargetType_ == 5) {
                                if (entry.filterCategory != "BardSongs") matches = false;
                            } else if (whitelistFilterTargetType_ == 6) {
                                if (entry.filterCategory != "FollowerCommentary") matches = false;
                            } else if (whitelistFilterTargetType_ <= 3) {
                                if (static_cast<int>(entry.targetType) != whitelistFilterTargetType_) matches = false;
                            }
                        }
                        if (matches && !whitelistFilterTopic_) {
                            if (entry.targetType == DialogueDB::BlacklistTarget::Topic) matches = false;
                        }
                        if (matches && !whitelistFilterScene_) {
                            if (entry.targetType == DialogueDB::BlacklistTarget::Scene) matches = false;
                        }
                        if (matches) filteredWhitelist.push_back(entry);
                    }
                    whitelistCache_ = filteredWhitelist;
                    whitelistLoadedCount_ = static_cast<int>(whitelistCache_.size());
                    spdlog::info("[STFUMenu] Updated whitelist entry notes");
                }
            }
            EndChild();
            
            SameLine();
            
            // Whitelist actions panel
            if (BeginChild("WhitelistActions", ImVec2(rightWidth, 0), ImGuiChildFlags_Border, ImGuiWindowFlags_None)) {
                Text("Actions:");
                Separator();
                Spacing();
                
                TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Whitelisted Entry");
                Spacing();
                TextWrapped("This entry will never be blocked regardless of other filters or settings.");
                Spacing();
                Separator();
                Spacing();
                
                if (Button("Move to Blacklist", ImVec2(-1, 0))) {
                    try {
                        auto db = DialogueDB::GetDatabase();
                        // Create blacklist entry from whitelist entry
                        DialogueDB::BlacklistEntry blacklistEntry = selectedWhitelistEntry_;
                        blacklistEntry.id = 0;  // Reset ID for new entry
                        blacklistEntry.blockType = DialogueDB::BlockType::Soft;
                        blacklistEntry.filterCategory = "Blacklist";
                        blacklistEntry.blockAudio = true;
                        blacklistEntry.blockSubtitles = true;
                        blacklistEntry.blockSkyrimNet = true;
                        blacklistEntry.addedTimestamp = std::chrono::duration_cast<std::chrono::seconds>(
                            std::chrono::system_clock::now().time_since_epoch()
                        ).count();
                        
                        if (db->AddToBlacklist(blacklistEntry)) {
                            // Invalidate cache so changes take effect immediately
                            Config::InvalidateTopicCache(blacklistEntry.targetFormID);
                            // Remove from whitelist after successful blacklist addition
                            db->RemoveFromWhitelist(selectedWhitelistEntry_.id);
                            hasWhitelistSelection_ = false;
                            selectedWhitelistID_ = -1;
                            
                            // Reload whitelist with filters applied
                            auto allWhitelist = db->GetWhitelist();
                            std::vector<DialogueDB::BlacklistEntry> filteredWhitelist;
                            std::string query = whitelistSearchQuery_;
                            std::transform(query.begin(), query.end(), query.begin(), ::tolower);
                            
                            for (const auto& entry : allWhitelist) {
                                bool matches = true;
                                if (query.length() > 0) {
                                    bool foundMatch = false;
                                    std::string editorID = entry.targetEditorID;
                                    std::transform(editorID.begin(), editorID.end(), editorID.begin(), ::tolower);
                                    if (editorID.find(query) != std::string::npos) foundMatch = true;
                                    if (!foundMatch && !entry.responseText.empty()) {
                                        std::string responseText = entry.responseText;
                                        std::transform(responseText.begin(), responseText.end(), responseText.begin(), ::tolower);
                                        if (responseText.find(query) != std::string::npos) foundMatch = true;
                                    }
                                    if (!foundMatch) matches = false;
                                }
                                if (matches && whitelistFilterTargetType_ > 0) {
                                    if (whitelistFilterTargetType_ == 4) {
                                        if (entry.targetType != DialogueDB::BlacklistTarget::Scene || 
                                            entry.filterCategory == "BardSongs" || 
                                            entry.filterCategory == "FollowerCommentary") matches = false;
                                    } else if (whitelistFilterTargetType_ == 5) {
                                        if (entry.filterCategory != "BardSongs") matches = false;
                                    } else if (whitelistFilterTargetType_ == 6) {
                                        if (entry.filterCategory != "FollowerCommentary") matches = false;
                                    } else if (whitelistFilterTargetType_ <= 3) {
                                        if (static_cast<int>(entry.targetType) != whitelistFilterTargetType_) matches = false;
                                    }
                                }
                                if (matches && !whitelistFilterTopic_) {
                                    if (entry.targetType == DialogueDB::BlacklistTarget::Topic) matches = false;
                                }
                                if (matches && !whitelistFilterScene_) {
                                    if (entry.targetType == DialogueDB::BlacklistTarget::Scene) matches = false;
                                }
                                if (matches) filteredWhitelist.push_back(entry);
                            }
                            whitelistCache_ = filteredWhitelist;
                            whitelistLoadedCount_ = static_cast<int>(whitelistCache_.size());
                            
                            // Reload blacklist without filters (user is on whitelist tab)
                            blacklistCache_ = db->GetBlacklist();
                            spdlog::info("[STFUMenu] Moved entry to blacklist and removed from whitelist: {}", selectedWhitelistEntry_.targetEditorID);
                        } else {
                            spdlog::error("[STFUMenu] Failed to add entry to blacklist: {}", selectedWhitelistEntry_.targetEditorID);
                        }
                    } catch (const std::exception& e) {
                        spdlog::error("[STFUMenu] Exception moving entry to blacklist: {}", e.what());
                    }
                }
                if (IsItemHovered(ImGuiHoveredFlags_None)) {
                    BeginTooltip();
                    Text("Move this entry to the blacklist with Soft block.");
                    Text("It will be removed from the whitelist.");
                    EndTooltip();
                }
                
                Spacing();
                
                if (Button("Remove from Whitelist", ImVec2(-1, 0))) {
                    try {
                        auto db = DialogueDB::GetDatabase();
                        if (db->RemoveFromWhitelist(selectedWhitelistEntry_.id)) {
                            spdlog::info("[STFUMenu] Removed whitelist entry: {}", selectedWhitelistEntry_.targetEditorID);
                            selectedWhitelistIDs_.clear();
                            hasWhitelistSelection_ = false;
                            selectedWhitelistID_ = -1;
                            
                            // Reload with filters applied
                            auto allWhitelist = db->GetWhitelist();
                            std::vector<DialogueDB::BlacklistEntry> filteredWhitelist;
                            std::string query = whitelistSearchQuery_;
                            std::transform(query.begin(), query.end(), query.begin(), ::tolower);
                            
                            for (const auto& entry : allWhitelist) {
                                bool matches = true;
                                if (query.length() > 0) {
                                    bool foundMatch = false;
                                    std::string editorID = entry.targetEditorID;
                                    std::transform(editorID.begin(), editorID.end(), editorID.begin(), ::tolower);
                                    if (editorID.find(query) != std::string::npos) foundMatch = true;
                                    if (!foundMatch && !entry.responseText.empty()) {
                                        std::string responseText = entry.responseText;
                                        std::transform(responseText.begin(), responseText.end(), responseText.begin(), ::tolower);
                                        if (responseText.find(query) != std::string::npos) foundMatch = true;
                                    }
                                    if (!foundMatch) matches = false;
                                }
                                if (matches && whitelistFilterTargetType_ > 0) {
                                    if (whitelistFilterTargetType_ == 4) {
                                        if (entry.targetType != DialogueDB::BlacklistTarget::Scene || 
                                            entry.filterCategory == "BardSongs" || 
                                            entry.filterCategory == "FollowerCommentary") matches = false;
                                    } else if (whitelistFilterTargetType_ == 5) {
                                        if (entry.filterCategory != "BardSongs") matches = false;
                                    } else if (whitelistFilterTargetType_ == 6) {
                                        if (entry.filterCategory != "FollowerCommentary") matches = false;
                                    } else if (whitelistFilterTargetType_ <= 3) {
                                        if (static_cast<int>(entry.targetType) != whitelistFilterTargetType_) matches = false;
                                    }
                                }
                                if (matches && !whitelistFilterTopic_) {
                                    if (entry.targetType == DialogueDB::BlacklistTarget::Topic) matches = false;
                                }
                                if (matches && !whitelistFilterScene_) {
                                    if (entry.targetType == DialogueDB::BlacklistTarget::Scene) matches = false;
                                }
                                if (matches) filteredWhitelist.push_back(entry);
                            }
                            whitelistCache_ = filteredWhitelist;
                            whitelistLoadedCount_ = static_cast<int>(whitelistCache_.size());
                        }
                    } catch (const std::exception& e) {
                        spdlog::error("[STFUMenu] Exception during whitelist removal: {}", e.what());
                    }
                }
                if (IsItemHovered(ImGuiHoveredFlags_None)) {
                    BeginTooltip();
                    Text("Remove this entry from whitelist without adding to blacklist.");
                    EndTooltip();
                }
            }
            EndChild();
        }
    }
    EndChild();
}

void STFUMenu::RefreshHistory()
{
    // Prevent concurrent calls to avoid database corruption
    if (isRefreshingHistory_) {
        spdlog::debug("[STFUMenu] RefreshHistory already running, skipping");
        return;
    }
    
    isRefreshingHistory_ = true;
    
    try {
        auto db = DialogueDB::GetDatabase();
        
        // Flush any pending entries from the queue before retrieving history
        db->FlushQueue();
        
        historyCache_ = db->GetRecentDialogue(100);
    
    // Get unified blacklist for checking entries (including scenes)
    auto blacklist = db->GetBlacklist();
    
    // Update blocked status for all entries based on current blacklist and MCM settings
    for (auto& entry : historyCache_) {
        try {
            // Check if this is a scene or bard song entry
            if ((entry.isScene || entry.isBardSong) && !entry.sceneEditorID.empty()) {
                // Check unified blacklist for all scene entries (including bard songs) by scene EditorID
                bool foundInBlacklist = false;
                for (const auto& blEntry : blacklist) {
                    try {
                        if (blEntry.targetType == DialogueDB::BlacklistTarget::Scene &&
                            !blEntry.targetEditorID.empty() && 
                            blEntry.targetEditorID == entry.sceneEditorID) {
                            
                            // Check if this entry's filterCategory toggle is enabled
                            std::string filterCategory = blEntry.filterCategory.empty() ? "Blacklist" : blEntry.filterCategory;
                            
                            // SkyrimNet blocks always check the global SkyrimNet filter, not the category toggle
                            if (blEntry.blockType == DialogueDB::BlockType::SkyrimNet) {
                                bool skyrimnetEnabled = Config::IsFilterCategoryEnabled("SkyrimNet");
                                if (skyrimnetEnabled) {
                                    entry.blockedStatus = DialogueDB::BlockedStatus::SkyrimNetBlock;
                                } else {
                                    entry.blockedStatus = DialogueDB::BlockedStatus::ToggledOff;
                                }
                            } else {
                                bool categoryEnabled = Config::IsFilterCategoryEnabled(filterCategory);
                                
                                if (!categoryEnabled) {
                                    // Toggle disabled for this category, mark as ToggledOff (blue ALLOWED)
                                    entry.blockedStatus = DialogueDB::BlockedStatus::ToggledOff;
                                } else {
                                    // Toggle enabled, update status based on blacklist entry
                                    switch (blEntry.blockType) {
                                        case DialogueDB::BlockType::Hard:
                                            entry.blockedStatus = DialogueDB::BlockedStatus::HardBlock;
                                            break;
                                        default:
                                            entry.blockedStatus = DialogueDB::BlockedStatus::SoftBlock;
                                            break;
                                    }
                                }
                            }
                            foundInBlacklist = true;
                            break;
                        }
                    } catch (const std::exception& e) {
                        spdlog::error("[STFUMenu] Exception comparing scene editorIDs: {}", e.what());
                        continue;
                    }
                }
                
                if (!foundInBlacklist) {
                    entry.blockedStatus = DialogueDB::BlockedStatus::Normal;
                }
            } else {
                // Regular dialogue entry - check regular blacklist
                int64_t blacklistId = db->GetBlacklistEntryId(entry.topicFormID, entry.topicEditorID);
                if (blacklistId > 0) {
                    // Entry is blacklisted - get the actual block type and filterCategory
                    auto blacklist = db->GetBlacklist();
                    for (const auto& blEntry : blacklist) {
                        if (blEntry.id == blacklistId) {
                            // Check if this entry's filterCategory toggle is enabled
                            std::string filterCategory = blEntry.filterCategory.empty() ? "Blacklist" : blEntry.filterCategory;
                            
                            // SkyrimNet blocks always check the global SkyrimNet filter, not the category toggle
                            if (blEntry.blockType == DialogueDB::BlockType::SkyrimNet) {
                                bool skyrimnetEnabled = Config::IsFilterCategoryEnabled("SkyrimNet");
                                if (skyrimnetEnabled) {
                                    entry.blockedStatus = DialogueDB::BlockedStatus::SkyrimNetBlock;
                                } else {
                                    entry.blockedStatus = DialogueDB::BlockedStatus::ToggledOff;
                                }
                            } else {
                                bool categoryEnabled = Config::IsFilterCategoryEnabled(filterCategory);
                                
                                if (!categoryEnabled) {
                                    // Toggle disabled for this category, mark as ToggledOff (blue ALLOWED)
                                    entry.blockedStatus = DialogueDB::BlockedStatus::ToggledOff;
                                } else {
                                    // Toggle enabled, update status based on blacklist entry
                                    switch (blEntry.blockType) {
                                        case DialogueDB::BlockType::Hard:
                                            entry.blockedStatus = DialogueDB::BlockedStatus::HardBlock;
                                            break;
                                        default:
                                            entry.blockedStatus = DialogueDB::BlockedStatus::SoftBlock;
                                            break;
                                    }
                                }
                            }
                            break;
                        }
                    }
                } else {
                    // Not blacklisted - check if filtered by MCM or has disabled toggle
                    if (Config::IsFilteredByMCM(entry.topicFormID, entry.topicSubtype)) {
                        entry.blockedStatus = DialogueDB::BlockedStatus::FilteredByConfig;
                    } else if (Config::HasDisabledSubtypeToggle(entry.topicSubtype)) {
                        // Has a subtype toggle but it's disabled - show as ToggledOff (cyan)
                        entry.blockedStatus = DialogueDB::BlockedStatus::ToggledOff;
                    } else {
                        entry.blockedStatus = DialogueDB::BlockedStatus::Normal;
                    }
                }
            }
        } catch (const std::exception& e) {
            spdlog::error("[STFUMenu] Exception processing history entry: {}", e.what());
            entry.blockedStatus = DialogueDB::BlockedStatus::Normal;
        }
    }
    
    lastHistoryRefresh_ = std::chrono::steady_clock::now();
    
    spdlog::debug("[STFUMenu] Refreshed history: {} entries", historyCache_.size());
    
    } catch (const std::exception& e) {
        spdlog::error("[STFUMenu] Exception in RefreshHistory: {}", e.what());
    }
    
    // Always clear the flag, even if there was an exception
    isRefreshingHistory_ = false;
}

std::string STFUMenu::FormatTimestamp(int64_t timestamp)
{
    std::time_t time = static_cast<std::time_t>(timestamp);
    std::tm tm;
    localtime_s(&tm, &time);
    
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

std::string STFUMenu::GetStatusText(DialogueDB::BlockedStatus status)
{
    switch (status) {
        case DialogueDB::BlockedStatus::SoftBlock:
            return "BLOCKED";
        case DialogueDB::BlockedStatus::HardBlock:
            return "BLOCKED";
        case DialogueDB::BlockedStatus::SkyrimNetBlock:
            return "BLOCKED";
        case DialogueDB::BlockedStatus::FilteredByConfig:
            return "BLOCKED";
        case DialogueDB::BlockedStatus::ToggledOff:
            return "ALLOWED";
        default:
            return "ALLOWED";
    }
}

std::string STFUMenu::GetDetailedStatusText(DialogueDB::BlockedStatus status)
{
    switch (status) {
        case DialogueDB::BlockedStatus::SoftBlock:
            return "Soft blocked by blacklist";
        case DialogueDB::BlockedStatus::HardBlock:
            return "Hard blocked by blacklist";
        case DialogueDB::BlockedStatus::SkyrimNetBlock:
            return "SkyrimNet blocked by blacklist";
        case DialogueDB::BlockedStatus::FilteredByConfig:
            return "Soft blocked by filter";
        case DialogueDB::BlockedStatus::ToggledOff:
            return "Allowed (toggle disabled)";
        default:
            return "Allowed";
    }
}

ImGuiMCP::ImU32 STFUMenu::GetStatusColor(DialogueDB::BlockedStatus status)
{
    switch (status) {
        case DialogueDB::BlockedStatus::SoftBlock:
            return COLOR_SOFT_BLOCK;
        case DialogueDB::BlockedStatus::HardBlock:
            return COLOR_HARD_BLOCK;
        case DialogueDB::BlockedStatus::SkyrimNetBlock:
            return COLOR_SKYRIM;
        case DialogueDB::BlockedStatus::FilteredByConfig:
            return COLOR_FILTER;
        case DialogueDB::BlockedStatus::ToggledOff:
           return COLOR_TOGGLED_OFF;
        default:
            return COLOR_ALLOWED;
    }
}

std::vector<std::string> STFUMenu::GetFilterCategories(bool isScene)
{
    std::vector<std::string> categories;
    
    if (isScene) {
        // Scenes can be in Blacklist, Scene, BardSongs, or FollowerCommentary categories
        categories.push_back("Blacklist");
        categories.push_back("Scene");
        categories.push_back("BardSongs");
        categories.push_back("FollowerCommentary");
    } else {
        // Regular dialogue can be in Blacklist or any subtype category
        categories.push_back("Blacklist");
        
        // Add all subtype filter names (alphabetically sorted)
        std::vector<std::string> subtypes = {
            "AcceptYield", "ActorCollideWithActor", "Agree", "AlertIdle", "AlertToCombat", "AlertToNormal",
            "AllyKilled", "Assault", "AssaultNC", "Attack", "AvoidThreat", "BarterExit", "Bash",
            "Bleedout", "Block", "CombatToLost", "CombatToNormal", "Death", "DestroyObject",
            "DetectFriendDie", "ExitFavorState", "Flee", "Goodbye", "Hello", "Hit", "Idle",
            "KnockOverObject", "LockedObject", "LostIdle", "LostToCombat", "LostToNormal",
            "MoralRefusal", "Murder", "MurderNC", "NormalToAlert", "NormalToCombat", "NoticeCorpse",
            "ObserveCombat", "PickpocketCombat", "PickpocketNC", "PickpocketTopic",
            "PlayerCastProjectileSpell", "PlayerCastSelfSpell", "PlayerInIronSights", "PlayerShout",
            "PowerAttack", "PursueIdleTopic", "Refuse", "ShootBow", "Show", "StandOnFurniture", "Steal",
            "StealFromNC", "SwingMeleeWeapon", "Taunt", "TimeToGo", "TrainingExit", "Trespass",
            "TrespassAgainstNC", "VoicePowerEndLong", "VoicePowerEndShort", "VoicePowerStartLong",
            "VoicePowerStartShort", "WerewolfTransformCrime", "Yield", "ZKeyObject"
        };
        
        for (const auto& subtype : subtypes) {
            categories.push_back(subtype);
        }
    }
    
    return categories;
}

int STFUMenu::GetCategoryIndex(const std::string& category, bool isScene)
{
    auto categories = GetFilterCategories(isScene);
    for (size_t i = 0; i < categories.size(); i++) {
        if (categories[i] == category) {
            return static_cast<int>(i);
        }
    }
    return 0;  // Default to "Blacklist" if not found
}

std::vector<std::string> STFUMenu::ParseResponsesJson(const std::string& json)
{
    // Wrap the DialogueDatabase JSON parser
    return DialogueDB::JsonToResponses(json);
}

std::string STFUMenu::FormatResponseText(const std::string& text)
{
    // Check if text is empty or only whitespace
    if (text.empty() || std::all_of(text.begin(), text.end(), [](unsigned char c) { return std::isspace(c); })) {
        return "*Response has no text*";
    }
    return text;
}

void STFUMenu::RenderResponsesPopup()
{
    using namespace ImGuiMCP;
    
    SetNextWindowSize(ImVec2(600, 650), ImGuiCond_FirstUseEver);
    
    if (BeginPopupModal("Responses", &showResponsesPopup_, ImGuiWindowFlags_None)) {
        // ESC key closes popup
        if (IsKeyPressed(ImGuiKey_Escape)) {
            CloseCurrentPopup();
            showResponsesPopup_ = false;
        }
        
        Text("%s", popupTitle_.c_str());
        Separator();
        Spacing();
        
        if (BeginChild("ResponsesList", ImVec2(0, 0), ImGuiChildFlags_Border, ImGuiWindowFlags_None)) {
            if (popupResponses_.empty()) {
                TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No responses have been captured yet.");
                TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Responses are captured when the dialogue is played in-game.");
            } else {
                for (size_t i = 0; i < popupResponses_.size(); i++) {
                    PushID(static_cast<int>(i));
                    
                    // Alternate text colors: white for even indices, light gray for odd
                    ImVec4 textColor = (i % 2 == 0) ? ImVec4(1.0f, 1.0f, 1.0f, 1.0f) : ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
                    
                    PushStyleColor(ImGuiCol_Text, textColor);
                    PushTextWrapPos(0.0f);
                    
                    Text("%zu. ", i + 1);
                    SameLine();
                    std::string formattedText = FormatResponseText(popupResponses_[i]);
                    Text("\"%s\"", formattedText.c_str());
                    
                    PopTextWrapPos();
                    PopStyleColor();
                    
                    if (i < popupResponses_.size() - 1) {
                        Spacing();
                    }
                    PopID();
                }
            }
        }
        EndChild();
        
        EndPopup();
    }
}

const char* STFUMenu::GetKeyName(uint32_t scancode)
{
    // Map DirectInput scancodes to readable key names
    // Reference: https://docs.microsoft.com/en-us/previous-versions/visualstudio/visual-studio-6.0/aa299374(v=vs.60)
    switch (scancode) {
        // Function keys
        case 0x3B: return "F1";
        case 0x3C: return "F2";
        case 0x3D: return "F3";
        case 0x3E: return "F4";
        case 0x3F: return "F5";
        case 0x40: return "F6";
        case 0x41: return "F7";
        case 0x42: return "F8";
        case 0x43: return "F9";
        case 0x44: return "F10";
        case 0x57: return "F11";
        case 0x58: return "F12";
        
        // Navigation keys
        case 0xC7: return "Home";
        case 0xCF: return "End";
        case 0xD2: return "Insert";
        case 0xD3: return "Delete";
        case 0xC9: return "Page Up";
        case 0xD1: return "Page Down";
        
        // Arrow keys
        case 0xC8: return "Up Arrow";
        case 0xD0: return "Down Arrow";
        case 0xCB: return "Left Arrow";
        case 0xCD: return "Right Arrow";
        
        // Number row
        case 0x02: return "1";
        case 0x03: return "2";
        case 0x04: return "3";
        case 0x05: return "4";
        case 0x06: return "5";
        case 0x07: return "6";
        case 0x08: return "7";
        case 0x09: return "8";
        case 0x0A: return "9";
        case 0x0B: return "0";
        case 0x0C: return "Minus";
        case 0x0D: return "Equals";
        
        // Numpad
        case 0x52: return "Numpad 0";
        case 0x4F: return "Numpad 1";
        case 0x50: return "Numpad 2";
        case 0x51: return "Numpad 3";
        case 0x4B: return "Numpad 4";
        case 0x4C: return "Numpad 5";
        case 0x4D: return "Numpad 6";
        case 0x47: return "Numpad 7";
        case 0x48: return "Numpad 8";
        case 0x49: return "Numpad 9";
        case 0x4E: return "Numpad +";
        case 0x4A: return "Numpad -";
        case 0x37: return "Numpad *";
        case 0xB5: return "Numpad /";
        case 0x53: return "Numpad .";
        case 0x9C: return "Numpad Enter";
        
        // Special keys
        case 0x01: return "Escape";
        case 0x0F: return "Tab";
        case 0x1C: return "Enter";
        case 0x39: return "Spacebar";
        case 0x0E: return "Backspace";
        case 0x1D: return "Left Ctrl";
        case 0x9D: return "Right Ctrl";
        case 0x2A: return "Left Shift";
        case 0x36: return "Right Shift";
        case 0x38: return "Left Alt";
        case 0xB8: return "Right Alt";
        
        // Brackets and punctuation
        case 0x1A: return "[";
        case 0x1B: return "]";
        case 0x2B: return "\\";
        case 0x27: return ";";
        case 0x28: return "'";
        case 0x33: return ",";
        case 0x34: return ".";
        case 0x35: return "/";
        case 0x29: return "`";
        
        // Letter keys
        case 0x1E: return "A";
        case 0x30: return "B";
        case 0x2E: return "C";
        case 0x20: return "D";
        case 0x12: return "E";
        case 0x21: return "F";
        case 0x22: return "G";
        case 0x23: return "H";
        case 0x17: return "I";
        case 0x24: return "J";
        case 0x25: return "K";
        case 0x26: return "L";
        case 0x32: return "M";
        case 0x31: return "N";
        case 0x18: return "O";
        case 0x19: return "P";
        case 0x10: return "Q";
        case 0x13: return "R";
        case 0x1F: return "S";
        case 0x14: return "T";
        case 0x16: return "U";
        case 0x2F: return "V";
        case 0x11: return "W";
        case 0x2D: return "X";
        case 0x15: return "Y";
        case 0x2C: return "Z";
        
        default:
            static char buffer[32];
            snprintf(buffer, sizeof(buffer), "Key 0x%02X", scancode);
            return buffer;
    }
}

bool STFUMenu::IsTypingInTextField()
{
    // Check if ImGui is capturing keyboard input (user is typing in a text field)
    auto* io = ImGuiMCP::GetIO();
    return io && io->WantCaptureKeyboard;
}

bool STFUMenu::IsValidMenuHotkey(uint32_t scancode)
{
    // Only reject keys that would interfere with menu functionality:
    
    // Reject mouse buttons - they come through as 0-7, not with 0x100 offset
    if (scancode <= 0x07) {
        return false;
    }
    
    // Reject mouse wheel (264-265 / 0x108-0x109) - also check non-offset values
    if (scancode >= 0x108 && scancode <= 0x109) {
        return false;
    }
    
    // Reject Enter (would interfere with confirmations)
    if (scancode == 0x1C) {  // Enter
        return false;
    }
    
    // Reject Ctrl and Shift (used for multi-selection)
    if (scancode == 0x1D ||  // Left Ctrl
        scancode == 0x2A ||  // Left Shift
        scancode == 0x36) {  // Right Shift
        return false;
    }
    
    // Allow everything else (including letters, numbers, function keys, gamepad, etc.)
    return true;
}

bool STFUMenu::CaptureHotkey(uint32_t scancode)
{
    if (!waitingForHotkeyInput_) {
        return false;
    }
    
    // Ignore escape to cancel capture
    if (scancode == 0x01) {  // Escape key
        spdlog::info("[STFU Menu] Hotkey capture cancelled");
        waitingForHotkeyInput_ = false;
        tempHotkeyCapture_ = 0;
        return true;
    }
    
    // Ignore invalid keys and stay in capture mode
    if (!IsValidMenuHotkey(scancode)) {
        spdlog::debug("[STFU Menu] Ignoring invalid key during capture: {} (0x{:X}) - Mouse clicks, scroll wheel, Enter, Ctrl, and Shift are not allowed", GetKeyName(scancode), scancode);
        return true;  // Ignore and continue waiting
    }
    
    // Capture the key
    tempHotkeyCapture_ = scancode;
    waitingForHotkeyInput_ = false;
    
    spdlog::info("[STFU Menu] Captured hotkey: {} (0x{:X})", GetKeyName(scancode), scancode);
    
    // Save to settings (using const_cast like LoadSettings does)
    auto& settings = const_cast<Config::Settings&>(Config::GetSettings());
    settings.menuHotkey = scancode;
    SettingsPersistence::SaveSettings();
    
    return true;
}

void STFUMenu::ReimportHardcodedScenes()
{
    spdlog::info("[STFU Menu] Re-importing hardcoded scenes...");
    
    auto* db = DialogueDB::GetDatabase();
    if (!db) {
        spdlog::error("[STFU Menu] Database not available for scene import");
        return;
    }
    
    // Import ambient scenes with "Scene" category
    auto scenesList = Config::GetHardcodedScenesList();
    db->ImportHardcodedScenes(scenesList, "Scene");
    spdlog::info("[STFU Menu] Imported {} ambient scenes", scenesList.size());
    
    // Import bard song quests as scenes with "BardSongs" category
    auto bardSongs = Config::GetBardSongQuestsList();
    std::vector<std::string> bardScenes;
    for (const auto& quest : bardSongs) {
        bardScenes.push_back(quest);
    }
    db->ImportHardcodedScenes(bardScenes, "BardSongs");
    spdlog::info("[STFU Menu] Imported {} bard song quests", bardScenes.size());
    
    // Import follower commentary scenes with "FollowerCommentary" category
    auto followerScenes = Config::GetFollowerCommentaryScenesList();
    db->ImportHardcodedScenes(followerScenes, "FollowerCommentary");
    spdlog::info("[STFU Menu] Imported {} follower commentary scenes", followerScenes.size());
    
    spdlog::info("[STFU Menu] Scene re-import complete!");
}
void STFUMenu::RenderManualEntryModal()
{
    using namespace ImGuiMCP;
    
    if (!showManualEntryModal_) {
        return;
    }
    
    SetNextWindowSize(ImVec2(600, 600), ImGuiCond_FirstUseEver);
    if (BeginPopupModal("Manual Entry", &showManualEntryModal_, ImGuiWindowFlags_None)) {
        // ESC key closes popup
        if (IsKeyPressed(ImGuiKey_Escape)) {
            CloseCurrentPopup();
            showManualEntryModal_ = false;
        }
        
        Text(isManualEntryForWhitelist_ ? "Create Whitelist Entry" : "Create Blacklist Entry");
        Separator();
        Spacing();
        
        // Identifier field (EditorID, FormID, or Plugin name)
        Text("Identifier:");
        PushItemWidth(-1);
        InputText("##ManualIdentifier", manualEntryIdentifier_, sizeof(manualEntryIdentifier_));
        PopItemWidth();
        if (isManualEntryForWhitelist_) {
            TextWrapped("EditorID, FormID, or Plugin.esp");
        } else {
            TextWrapped("EditorID or FormID");
        }
        
        Spacing();
        
        // Blocking options (only for blacklist)
        if (!isManualEntryForWhitelist_) {
            Text("Block Type:");
            const char* blockTypes[] = { "Soft Block", "Hard Block" };
            Combo("##ManualBlockType", &manualEntryBlockType_, blockTypes, 2);
            
            Spacing();
            
            // Filter category (for blacklist only) - detect type on the fly to show appropriate categories
            Text("Filter Category:");
            
            // Auto-detect type to determine which categories to show
            int detectedTypeForCategories = 0; // Default to Topic
            std::string identifierCheck = manualEntryIdentifier_;
            if (!identifierCheck.empty()) {
                std::string lowerCheck = identifierCheck;
                std::transform(lowerCheck.begin(), lowerCheck.end(), lowerCheck.begin(), ::tolower);
                
                if (!lowerCheck.ends_with(".esp") && !lowerCheck.ends_with(".esm") && !lowerCheck.ends_with(".esl")) {
                    // Not a plugin, try to detect if it's a scene
                    auto* scene = Config::SafeLookupForm<RE::BGSScene>(identifierCheck.c_str());
                    if (scene) {
                        detectedTypeForCategories = 1; // Scene
                    }
                }
            }
            
            bool isSceneForCategories = (detectedTypeForCategories == 1);
            auto categories = GetFilterCategories(isSceneForCategories);
            std::vector<const char*> categoryPtrs;
            for (const auto& cat : categories) {
                categoryPtrs.push_back(cat.c_str());
            }
            
            // Clamp the index to valid range when categories change
            if (manualEntryFilterCategoryIndex_ >= static_cast<int>(categoryPtrs.size())) {
                manualEntryFilterCategoryIndex_ = 0;
            }
            
            Combo("##ManualFilterCategory", &manualEntryFilterCategoryIndex_, categoryPtrs.data(), static_cast<int>(categoryPtrs.size()));
            
            Spacing();
        }
        
        // Notes field
        Text("Notes:");
        PushItemWidth(-1);
        InputTextMultiline("##ManualNotes", manualEntryNotes_, sizeof(manualEntryNotes_), ImVec2(-1, 80), ImGuiInputTextFlags_None, nullptr, nullptr);
        PopItemWidth();
        
        Spacing();
        Separator();
        Spacing();
        
        // Action buttons
        if (Button("Create", ImVec2(120, 0))) {
            // Parse identifier and create entry
            std::string identifier = manualEntryIdentifier_;
            
            if (identifier.empty()) {
                spdlog::error("[STFUMenu] Empty identifier for manual entry");
            } else {
                auto db = DialogueDB::GetDatabase();
                if (db) {
                    // Auto-detect type based on identifier
                    int detectedType = 0; // Default to Topic
                    std::string lowerIdentifier = identifier;
                    std::transform(lowerIdentifier.begin(), lowerIdentifier.end(), lowerIdentifier.begin(), ::tolower);
                    
                    if (lowerIdentifier.ends_with(".esp") || lowerIdentifier.ends_with(".esm") || lowerIdentifier.ends_with(".esl")) {
                        detectedType = 2; // Plugin
                    } else {
                        // Try to detect if it's a scene by looking it up in game data
                        auto* scene = Config::SafeLookupForm<RE::BGSScene>(identifier.c_str());
                        if (scene) {
                            detectedType = 1; // Scene
                        } else {
                            detectedType = 0; // Topic (default)
                        }
                    }
                    
                    DialogueDB::BlacklistEntry entry;
                    entry.notes = manualEntryNotes_;
                    
                    // Determine target type based on auto-detection
                    if (detectedType == 0) {
                        entry.targetType = DialogueDB::BlacklistTarget::Topic;
                    } else if (detectedType == 1) {
                        entry.targetType = DialogueDB::BlacklistTarget::Scene;
                        entry.subtype = 14;  // Scene subtype
                        entry.subtypeName = "Scene";
                    } else {
                        entry.targetType = DialogueDB::BlacklistTarget::Plugin;
                    }
                    
                    // Parse FormID if it's in 0x format or FormKey format
                    if (identifier.find("0x") == 0 || identifier.find(":") != std::string::npos) {
                        // Try to parse as FormID
                        if (identifier.find("0x") == 0) {
                            // 0x format
                            entry.targetFormID = std::stoul(identifier, nullptr, 16);
                        } else if (identifier.find(":") != std::string::npos) {
                            // FormKey format (handled by database enrichment)
                            entry.targetFormID = 0;
                        }
                    }
                    
                    // Set EditorID (plugins use this field for plugin name)
                    entry.targetEditorID = identifier;
                    
                    // Set block type and flags
                    if (!isManualEntryForWhitelist_) {
                        entry.blockType = (manualEntryBlockType_ == 0) ? DialogueDB::BlockType::Soft : DialogueDB::BlockType::Hard;
                        // Soft block always blocks both audio and subtitles
                        if (entry.blockType == DialogueDB::BlockType::Soft) {
                            entry.blockAudio = true;
                            entry.blockSubtitles = true;
                            entry.blockSkyrimNet = true;
                        } else {
                            // Hard block
                            entry.blockAudio = true;
                            entry.blockSubtitles = true;
                            entry.blockSkyrimNet = true;
                        }
                        
                        // Set filter category based on detected type
                        bool isScene = (detectedType == 1);
                        auto categories = GetFilterCategories(isScene);
                        if (manualEntryFilterCategoryIndex_ >= 0 && manualEntryFilterCategoryIndex_ < categories.size()) {
                            entry.filterCategory = categories[manualEntryFilterCategoryIndex_];
                        } else {
                            entry.filterCategory = "Blacklist";
                        }
                    } else {
                        entry.blockType = DialogueDB::BlockType::Soft;
                        entry.blockAudio = false;
                        entry.blockSubtitles = false;
                        entry.blockSkyrimNet = false;
                        entry.filterCategory = "Whitelist";
                    }
                    
                    // Set timestamp
                    entry.addedTimestamp = std::chrono::duration_cast<std::chrono::seconds>(
                        std::chrono::system_clock::now().time_since_epoch()
                    ).count();
                    
                    // Add to database
                    bool success = false;
                    if (isManualEntryForWhitelist_) {
                        success = db->AddToWhitelist(entry);
                        if (success) {
                            spdlog::info("[STFUMenu] Added manual whitelist entry: {}", identifier);
                            
                            // Reload with filters applied
                            auto allWhitelist = db->GetWhitelist();
                            std::vector<DialogueDB::BlacklistEntry> filteredWhitelist;
                            std::string query = whitelistSearchQuery_;
                            std::transform(query.begin(), query.end(), query.begin(), ::tolower);
                            
                            for (const auto& e : allWhitelist) {
                                bool matches = true;
                                if (query.length() > 0) {
                                    bool foundMatch = false;
                                    std::string editorID = e.targetEditorID;
                                    std::transform(editorID.begin(), editorID.end(), editorID.begin(), ::tolower);
                                    if (editorID.find(query) != std::string::npos) foundMatch = true;
                                    if (!foundMatch && !e.responseText.empty()) {
                                        std::string responseText = e.responseText;
                                        std::transform(responseText.begin(), responseText.end(), responseText.begin(), ::tolower);
                                        if (responseText.find(query) != std::string::npos) foundMatch = true;
                                    }
                                    if (!foundMatch) matches = false;
                                }
                                if (matches && whitelistFilterTargetType_ > 0) {
                                    if (whitelistFilterTargetType_ == 4) {
                                        if (e.targetType != DialogueDB::BlacklistTarget::Scene || 
                                            e.filterCategory == "BardSongs" || 
                                            e.filterCategory == "FollowerCommentary") matches = false;
                                    } else if (whitelistFilterTargetType_ == 5) {
                                        if (e.filterCategory != "BardSongs") matches = false;
                                    } else if (whitelistFilterTargetType_ == 6) {
                                        if (e.filterCategory != "FollowerCommentary") matches = false;
                                    } else if (whitelistFilterTargetType_ <= 3) {
                                        if (static_cast<int>(e.targetType) != whitelistFilterTargetType_) matches = false;
                                    }
                                }
                                if (matches && !whitelistFilterTopic_) {
                                    if (e.targetType == DialogueDB::BlacklistTarget::Topic) matches = false;
                                }
                                if (matches && !whitelistFilterScene_) {
                                    if (e.targetType == DialogueDB::BlacklistTarget::Scene) matches = false;
                                }
                                if (matches) filteredWhitelist.push_back(e);
                            }
                            whitelistCache_ = filteredWhitelist;
                            whitelistLoadedCount_ = static_cast<int>(whitelistCache_.size());
                        }
                    } else {
                        success = db->AddToBlacklist(entry);
                        if (success) {
                            spdlog::info("[STFUMenu] Added manual blacklist entry: {}", identifier);
                            
                            // Reload with filters applied
                            auto allBlacklist = db->GetBlacklist();
                            std::vector<DialogueDB::BlacklistEntry> filteredBlacklist;
                            std::string query = searchQuery_;
                            std::transform(query.begin(), query.end(), query.begin(), ::tolower);
                            
                            for (const auto& e : allBlacklist) {
                                bool matches = true;
                                if (query.length() > 0) {
                                    bool foundMatch = false;
                                    std::string editorID = e.targetEditorID;
                                    std::transform(editorID.begin(), editorID.end(), editorID.begin(), ::tolower);
                                    if (editorID.find(query) != std::string::npos) foundMatch = true;
                                    if (!foundMatch && !e.responseText.empty()) {
                                        std::string responseText = e.responseText;
                                        std::transform(responseText.begin(), responseText.end(), responseText.begin(), ::tolower);
                                        if (responseText.find(query) != std::string::npos) foundMatch = true;
                                    }
                                    if (!foundMatch) matches = false;
                                }
                                if (matches && filterTargetType_ > 0) {
                                    if (filterTargetType_ == 4) {
                                        if (e.targetType != DialogueDB::BlacklistTarget::Scene || 
                                            e.filterCategory == "BardSongs" || 
                                            e.filterCategory == "FollowerCommentary") matches = false;
                                    } else if (filterTargetType_ == 5) {
                                        if (e.filterCategory != "BardSongs") matches = false;
                                    } else if (filterTargetType_ == 6) {
                                        if (e.filterCategory != "FollowerCommentary") matches = false;
                                    } else if (filterTargetType_ <= 3) {
                                        if (static_cast<int>(e.targetType) != filterTargetType_) matches = false;
                                    }
                                }
                                if (matches) {
                                    bool blockTypeMatch = false;
                                    if (filterSoftBlock_ && e.blockType == DialogueDB::BlockType::Soft) blockTypeMatch = true;
                                    if (filterHardBlock_ && e.blockType == DialogueDB::BlockType::Hard) blockTypeMatch = true;
                                    if (filterSkyrimNet_ && e.blockType == DialogueDB::BlockType::SkyrimNet) blockTypeMatch = true;
                                    if (!blockTypeMatch) matches = false;
                                }
                                if (matches && !filterTopic_) {
                                    if (e.targetType == DialogueDB::BlacklistTarget::Topic) matches = false;
                                }
                                if (matches && !filterScene_) {
                                    if (e.targetType == DialogueDB::BlacklistTarget::Scene) matches = false;
                                }
                                if (matches) filteredBlacklist.push_back(e);
                            }
                            blacklistCache_ = filteredBlacklist;
                            blacklistLoadedCount_ = static_cast<int>(blacklistCache_.size());
                        }
                    }
                    
                    if (!success) {
                        spdlog::error("[STFUMenu] Failed to add manual entry: {}", identifier);
                    }
                }
                
                showManualEntryModal_ = false;
            }
        }
        
        SameLine();
        
        if (Button("Cancel", ImVec2(120, 0))) {
            showManualEntryModal_ = false;
        }
        
        EndPopup();
    }
}