#pragma once

#include "../include/PCH.h"
#include "DialogueDatabase.h"
#include "SKSEMenuFramework.h"
#include <vector>
#include <string>
#include <chrono>

class STFUMenu
{
public:
    static void Initialize();
    static void Toggle();
    static bool IsOpen();
    static bool IsSkyrimNetLoaded() { return isSkyrimNetLoaded_; }
    
    // Helper functions for hotkey capture (public for main.cpp access)
    static const char* GetKeyName(uint32_t scancode);
    static bool IsCapturingHotkey() { return waitingForHotkeyInput_; }
    static void StartHotkeyCapture() { waitingForHotkeyInput_ = true; tempHotkeyCapture_ = 0; }
    static bool CaptureHotkey(uint32_t scancode);  // Returns true if capture was active
    static bool IsValidMenuHotkey(uint32_t scancode);  // Check if scancode is allowed for menu hotkey
    static bool IsTypingInTextField();  // Check if user is typing in an ImGui text field
    static void ReimportHardcodedScenes();  // Re-import all default scenes
    static bool HasOpenModal() { return showResponsesPopup_ || showManualEntryModal_; }  // Check if any modal is open
    static float GetUIScale() { return uiScale_; }  // Get UI scale factor for resolution scaling
    
private:
    static void Render();
    static void RenderHistoryTab();
    static void RenderBlacklistTab();
    static void RenderWhitelistTab();
    static void RenderSettingsTab();
    static void RenderDetailPanel();
    static void RenderBlacklistDetailPanel();
    static void RenderResponsesPopup();
    
    static void LoadSelectedEntry(int64_t entryID);
    static void ApplyBlockSettings();
    static void RefreshHistory();
    static std::vector<std::string> ParseResponsesJson(const std::string& json);
    static std::string FormatResponseText(const std::string& text);
    
    static std::string FormatTimestamp(int64_t timestamp);
    static std::string GetStatusText(DialogueDB::BlockedStatus status);
    static std::string GetDetailedStatusText(DialogueDB::BlockedStatus status);
    static ImGuiMCP::ImU32 GetStatusColor(DialogueDB::BlockedStatus status);
    static std::vector<std::string> GetFilterCategories(bool isScene);
    static int GetCategoryIndex(const std::string& category, bool isScene);
    
    // Window state
    static inline SKSEMenuFramework::Model::WindowInterface* window_ = nullptr;
    static inline bool initialized_ = false;
    static inline bool isSkyrimNetLoaded_ = false;
    static inline float uiScale_ = 1.0f;  // UI scale factor (1440p = 1.0, 1080p = 0.9)
    
    // Tab state
    static inline int currentTab_ = 0;  // 0=History, 1=Blacklist, 2=Whitelist, 3=Settings
    static inline int previousTab_ = 0;  // Track previous tab for refresh detection
    
    // Data cache
    static inline std::vector<DialogueDB::DialogueEntry> historyCache_;
    static inline std::chrono::steady_clock::time_point lastHistoryRefresh_;
    static inline std::vector<DialogueDB::BlacklistEntry> blacklistCache_;
    static inline std::vector<DialogueDB::BlacklistEntry> whitelistCache_;
    
    // Import button cooldowns (prevent spam)
    static inline std::chrono::steady_clock::time_point lastImportScenesTime_;
    static inline std::chrono::steady_clock::time_point lastImportYAMLTime_;
    
    // Selection state (History tab)
    static inline int64_t selectedEntryID_ = -1;
    static inline DialogueDB::DialogueEntry selectedEntry_;
    static inline bool hasSelection_ = false;
    
    // Multi-selection for history
    static inline std::vector<int64_t> selectedHistoryIDs_;
    static inline int lastClickedHistoryIndex_ = -1;
    
    // Selection state (Blacklist tab - unified for all targets including scenes)
    static inline int64_t selectedBlacklistID_ = -1;
    static inline DialogueDB::BlacklistEntry selectedBlacklistEntry_;
    static inline bool hasBlacklistSelection_ = false;
    
    // Multi-selection for blacklist (unified for all targets)
    static inline std::vector<int64_t> selectedBlacklistIDs_;
    static inline int lastClickedIndex_ = -1;
    
    // Selection state (Whitelist tab - unified for all targets including scenes)
    static inline int64_t selectedWhitelistID_ = -1;
    static inline DialogueDB::BlacklistEntry selectedWhitelistEntry_;
    static inline bool hasWhitelistSelection_ = false;
    
    // Multi-selection for whitelist (unified for all targets)
    static inline std::vector<int64_t> selectedWhitelistIDs_;
    static inline int lastClickedWhitelistIndex_ = -1;
    
    // UI state for detail panel (single selection)
    static inline bool hardBlock_ = false;
    static inline bool softBlockEnabled_ = false;
    static inline bool softBlockAudio_ = true;         // Default checked for convenience
    static inline bool softBlockSubtitles_ = true;     // Default checked for convenience
    static inline bool skyrimNetOnly_ = false;         // Separate SkyrimNet block option
    static inline bool whiteList_ = false;
    static inline int filterCategoryIndex_ = 0;       // Index for filterCategory dropdown
    static inline std::string selectedFilterCategory_ = "Blacklist";  // Current filterCategory selection
    
    // UI state for multi-selection
    static inline bool multiSoftBlockEnabled_ = false;
    static inline bool multiSoftBlockAudio_ = true;
    static inline bool multiSoftBlockSubtitles_ = true;
    static inline bool multiSkyrimNetOnly_ = false;    // Separate SkyrimNet block option
    static inline bool multiHardBlock_ = false;
    static inline bool multiWhiteList_ = false;
    static inline int multiFilterCategoryIndex_ = 0;
    static inline std::string multiSelectedFilterCategory_ = "Blacklist";
    
    // Scroll state
    static inline float historyScrollY_ = 0.0f;
    static inline bool autoScrollToBottom_ = true;
    static inline bool needScrollToBottom_ = false;
    
    // History search state
    static inline char historySearchQuery_[256] = "";
    
    // Blacklist search state
    static inline char searchQuery_[256] = "";
    static inline int filterTargetType_ = 0;  // 0=All, 1=Topic, 2=Quest, 3=Subtype, 4=Scene, 5=Bard Song
    static inline bool filterSoftBlock_ = true;
    static inline bool filterHardBlock_ = true;
    static inline bool filterSkyrimNet_ = true;
    static inline bool filterTopic_ = true;
    static inline bool filterScene_ = true;
    static inline char blacklistNotes_[512] = "";
    static inline char historyNotes_[512] = "";  // Notes buffer for History tab
    static inline int blacklistPageSize_ = 100;
    static inline int blacklistLoadedCount_ = 0;
    
    // Whitelist search state
    static inline char whitelistSearchQuery_[256] = "";
    static inline int whitelistFilterTargetType_ = 0;  // 0=All, 1=Topic, 2=Quest, 3=Subtype, 4=Scene, 5=Bard Song
    static inline bool whitelistFilterSoftBlock_ = true;
    static inline bool whitelistFilterHardBlock_ = true;
    static inline bool whitelistFilterSkyrimNet_ = true;
    static inline bool whitelistFilterTopic_ = true;
    static inline bool whitelistFilterScene_ = true;
    static inline char whitelistNotes_[512] = "";
    static inline int whitelistPageSize_ = 100;
    static inline int whitelistLoadedCount_ = 0;
    
    // Responses popup state
    static inline bool showResponsesPopup_ = false;
    static inline std::vector<std::string> popupResponses_;
    static inline std::string popupTitle_;
    
    // Hotkey capture state
    static inline bool waitingForHotkeyInput_ = false;
    static inline uint32_t tempHotkeyCapture_ = 0;
    
    // Concurrent access guard
    static inline bool isRefreshingHistory_ = false;
    static inline bool needsHistoryRefresh_ = false;
    static inline bool needsBlacklistRefresh_ = false;
    static inline bool needsWhitelistRefresh_ = false;
    
    // Manual entry modal state
    static inline bool showManualEntryModal_ = false;
    static inline bool isManualEntryForWhitelist_ = false;  // true = whitelist, false = blacklist
    static inline char manualEntryIdentifier_[256] = "";   // EditorID, FormID, or Plugin name
    static inline int manualEntryBlockType_ = 0;           // 0=Soft, 1=Hard
    static inline bool manualEntryBlockAudio_ = true;
    static inline bool manualEntryBlockSubtitles_ = true;
    static inline bool manualEntryBlockSkyrimNet_ = true;
    static inline char manualEntryNotes_[512] = "";
    static inline int manualEntryFilterCategoryIndex_ = 0;
    static void RenderManualEntryModal();};