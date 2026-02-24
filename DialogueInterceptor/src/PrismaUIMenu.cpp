#include "PrismaUIMenu.h"
#include "STFUMenu.h"
#include "Config.h"
#include "TopicResponseExtractor.h"
#include <spdlog/spdlog.h>
#include <sstream>
#include <iomanip>

PRISMA_UI_API::IVPrismaUI1* PrismaUIMenu::prismaUI_ = nullptr;
PrismaView PrismaUIMenu::view_ = 0;
bool PrismaUIMenu::initialized_ = false;

void PrismaUIMenu::Initialize()
{
    if (initialized_) {
        return;
    }
    
    // Request PrismaUI API
    prismaUI_ = static_cast<PRISMA_UI_API::IVPrismaUI1*>(
        PRISMA_UI_API::RequestPluginAPI(PRISMA_UI_API::InterfaceVersion::V1)
    );
    
    if (!prismaUI_) {
        spdlog::error("[PrismaUIMenu] Failed to get PrismaUI API. Is PrismaUI installed?");
        return;
    }
    
    // Create view
    view_ = prismaUI_->CreateView("STFU/index.html", &OnDomReady);
    
    if (!view_ || !prismaUI_->IsValid(view_)) {
        spdlog::error("[PrismaUIMenu] Failed to create PrismaUI view");
        return;
    }
    
    // Register JS listeners
    prismaUI_->RegisterJSListener(view_, "requestHistory", &OnRequestHistory);
    prismaUI_->RegisterJSListener(view_, "requestBlacklist", &OnRequestBlacklist);
    prismaUI_->RegisterJSListener(view_, "closeMenu", &OnCloseMenu);
    prismaUI_->RegisterJSListener(view_, "jsLog", &OnLog);
    prismaUI_->RegisterJSListener(view_, "deleteBlacklistEntry", &OnDeleteBlacklistEntry);
    prismaUI_->RegisterJSListener(view_, "deleteBlacklistBatch", &OnDeleteBlacklistBatch);
    prismaUI_->RegisterJSListener(view_, "clearBlacklist", &OnClearBlacklist);
    prismaUI_->RegisterJSListener(view_, "refreshBlacklist", &OnRefreshBlacklist);
    prismaUI_->RegisterJSListener(view_, "updateBlacklistEntry", &OnUpdateBlacklistEntry);
    prismaUI_->RegisterJSListener(view_, "addToBlacklist", &OnAddToBlacklist);
    prismaUI_->RegisterJSListener(view_, "addToWhitelist", &OnAddToWhitelist);
    prismaUI_->RegisterJSListener(view_, "removeFromBlacklist", &OnRemoveFromBlacklist);
    prismaUI_->RegisterJSListener(view_, "toggleSubtypeFilter", &OnToggleSubtypeFilter);
    prismaUI_->RegisterJSListener(view_, "deleteHistoryEntries", &OnDeleteHistoryEntries);
    prismaUI_->RegisterJSListener(view_, "openManualEntry", &OnOpenManualEntry);
    prismaUI_->RegisterJSListener(view_, "detectIdentifierType", &OnDetectIdentifierType);
    prismaUI_->RegisterJSListener(view_, "createManualEntry", &OnCreateManualEntry);
    prismaUI_->RegisterJSListener(view_, "requestWhitelist", &OnRequestWhitelist);
    prismaUI_->RegisterJSListener(view_, "removeFromWhitelist", &OnRemoveFromWhitelist);
    prismaUI_->RegisterJSListener(view_, "updateWhitelistEntry", &OnUpdateWhitelistEntry);
    prismaUI_->RegisterJSListener(view_, "moveToBlacklist", &OnMoveToBlacklist);
    prismaUI_->RegisterJSListener(view_, "removeWhitelistBatch", &OnRemoveWhitelistBatch);
    
    // Set faster scroll speed (default is usually ~40 pixels)
    prismaUI_->SetScrollingPixelSize(view_, 100);
    
    // Hide view by default
    prismaUI_->Hide(view_);
    
    initialized_ = true;
    spdlog::info("[PrismaUIMenu] Initialized successfully");
}

void PrismaUIMenu::OnDomReady(PrismaView view)
{
    spdlog::info("[PrismaUIMenu] DOM ready for view {}", view);
    // Send initial data when DOM is ready
    SendHistoryData();
    SendBlacklistData();
    SendWhitelistData();
}

void PrismaUIMenu::OnRequestHistory(const char* data)
{
    spdlog::trace("[PrismaUIMenu] History data requested");
    SendHistoryData();
}

void PrismaUIMenu::OnRequestBlacklist(const char* data)
{
    spdlog::trace("[PrismaUIMenu] Blacklist data requested");
    SendBlacklistData();
}

void PrismaUIMenu::OnCloseMenu(const char* data)
{
    spdlog::trace("[PrismaUIMenu] Close menu requested");
    Toggle();
}

void PrismaUIMenu::OnLog(const char* data)
{
    if (data && data[0] != '\0') {
        spdlog::info("[PrismaUIMenu::JS] {}", data);
    }
}

void PrismaUIMenu::OnDeleteBlacklistEntry(const char* data)
{
    if (!data) {
        spdlog::warn("[PrismaUIMenu::OnDeleteBlacklistEntry] Null data");
        return;
    }
    
    try {
        int64_t entryId = std::stoll(data);
        spdlog::info("[PrismaUIMenu] Deleting blacklist entry: {}", entryId);
        
        auto* db = DialogueDB::GetDatabase();
        if (db && db->RemoveFromBlacklist(entryId)) {
            spdlog::info("[PrismaUIMenu] Successfully deleted blacklist entry {}", entryId);
            SendBlacklistData();  // Refresh the data
        } else {
            spdlog::error("[PrismaUIMenu] Failed to delete blacklist entry {}", entryId);
        }
    } catch (const std::exception& e) {
        spdlog::error("[PrismaUIMenu::OnDeleteBlacklistEntry] Exception: {}", e.what());
    }
}

void PrismaUIMenu::OnDeleteBlacklistBatch(const char* data)
{
    if (!data) {
        spdlog::warn("[PrismaUIMenu::OnDeleteBlacklistBatch] Null data");
        return;
    }
    
    try {
        // Parse comma-separated IDs
        std::vector<int64_t> ids;
        std::string dataStr(data);
        std::istringstream ss(dataStr);
        std::string token;
        
        while (std::getline(ss, token, ',')) {
            if (!token.empty()) {
                ids.push_back(std::stoll(token));
            }
        }
        
        spdlog::info("[PrismaUIMenu] Deleting {} blacklist entries", ids.size());
        
        auto* db = DialogueDB::GetDatabase();
        if (db) {
            int removedCount = db->RemoveFromBlacklistBatch(ids);
            spdlog::info("[PrismaUIMenu] Successfully deleted {} blacklist entries", removedCount);
            SendBlacklistData();  // Refresh the data
        }
    } catch (const std::exception& e) {
        spdlog::error("[PrismaUIMenu::OnDeleteBlacklistBatch] Exception: {}", e.what());
    }
}

void PrismaUIMenu::OnClearBlacklist(const char* data)
{
    spdlog::info("[PrismaUIMenu] Clearing all blacklist entries");
    
    try {
        auto* db = DialogueDB::GetDatabase();
        if (db) {
            int removedCount = db->ClearBlacklist();
            spdlog::info("[PrismaUIMenu] Successfully cleared {} blacklist entries", removedCount);
            SendBlacklistData();  // Refresh the data
        }
    } catch (const std::exception& e) {
        spdlog::error("[PrismaUIMenu::OnClearBlacklist] Exception: {}", e.what());
    }
}

void PrismaUIMenu::OnRefreshBlacklist(const char* data)
{
    spdlog::info("[PrismaUIMenu] Refreshing blacklist data");
    SendBlacklistData();
}

void PrismaUIMenu::OnUpdateBlacklistEntry(const char* data)
{
    if (!data) {
        spdlog::warn("[PrismaUIMenu::OnUpdateBlacklistEntry] Null data");
        return;
    }
    
    try {
        // Parse JSON: {"id":123,"blockType":"Soft","filterCategory":"Blacklist","notes":"Comment"}
        spdlog::info("[PrismaUIMenu] Updating blacklist entry, data: {}", data);
        
        std::string jsonStr(data);
        
        // Simple JSON parsing (hacky but sufficient for our controlled data)
        auto getValue = [&jsonStr](const std::string& key) -> std::string {
            std::string searchKey = "\"" + key + "\":";
            size_t pos = jsonStr.find(searchKey);
            if (pos == std::string::npos) return "";
            
            pos += searchKey.length();
            // Skip whitespace
            while (pos < jsonStr.length() && (jsonStr[pos] == ' ' || jsonStr[pos] == '\t')) pos++;
            
            if (pos >= jsonStr.length()) return "";
            
            // Check if it's a string (starts with quote)
            if (jsonStr[pos] == '"') {
                pos++; // Skip opening quote
                size_t endPos = jsonStr.find('"', pos);
                if (endPos == std::string::npos) return "";
                return jsonStr.substr(pos, endPos - pos);
            } else {
                // It's a number or other value
                size_t endPos = jsonStr.find_first_of(",}", pos);
                if (endPos == std::string::npos) endPos = jsonStr.length();
                return jsonStr.substr(pos, endPos - pos);
            }
        };
        
        int64_t entryId = std::stoll(getValue("id"));
        std::string blockTypeStr = getValue("blockType");
        std::string filterCategory = getValue("filterCategory");
        std::string notes = getValue("notes");
        
        // Unescape notes (\n -> newline)
        size_t escapePos = 0;
        while ((escapePos = notes.find("\\n", escapePos)) != std::string::npos) {
            notes.replace(escapePos, 2, "\n");
            escapePos += 1;
        }
        
        spdlog::info("[PrismaUIMenu] Parsed: id={}, blockType={}, filterCategory={}, notes={}",
                    entryId, blockTypeStr, filterCategory, notes);
        
        auto* db = DialogueDB::GetDatabase();
        if (!db) {
            spdlog::error("[PrismaUIMenu] Database not available");
            return;
        }
        
        // Get the existing entry
        auto allEntries = db->GetBlacklist();
        DialogueDB::BlacklistEntry* existingEntry = nullptr;
        for (auto& entry : allEntries) {
            if (entry.id == entryId) {
                existingEntry = &entry;
                break;
            }
        }
        
        if (!existingEntry) {
            spdlog::error("[PrismaUIMenu] Blacklist entry {} not found", entryId);
            return;
        }
        
        // Update the fields
        if (blockTypeStr == "Soft") {
            existingEntry->blockType = DialogueDB::BlockType::Soft;
            existingEntry->blockAudio = true;
            existingEntry->blockSubtitles = true;
            existingEntry->blockSkyrimNet = true;
        } else if (blockTypeStr == "Hard") {
            existingEntry->blockType = DialogueDB::BlockType::Hard;
            existingEntry->blockAudio = true;
            existingEntry->blockSubtitles = true;
            existingEntry->blockSkyrimNet = true;
        } else if (blockTypeStr == "SkyrimNet") {
            existingEntry->blockType = DialogueDB::BlockType::SkyrimNet;
            existingEntry->blockAudio = false;
            existingEntry->blockSubtitles = false;
            existingEntry->blockSkyrimNet = true;
        }
        
        existingEntry->filterCategory = filterCategory;
        existingEntry->notes = notes;
        
        // Save to database (AddToBlacklist handles both insert and update)
        if (db->AddToBlacklist(*existingEntry)) {
            spdlog::info("[PrismaUIMenu] Successfully updated blacklist entry {}", entryId);
            // TODO: Invalidate cache - need to expose Config::InvalidateTopicCache
            SendBlacklistData();  // Refresh the UI
        } else {
            spdlog::error("[PrismaUIMenu] Failed to update blacklist entry {}", entryId);
        }
    } catch (const std::exception& e) {
        spdlog::error("[PrismaUIMenu::OnUpdateBlacklistEntry] Exception: {}", e.what());
    }
}

void PrismaUIMenu::Toggle()
{
    if (!initialized_ || !prismaUI_ || !prismaUI_->IsValid(view_)) {
        spdlog::warn("[PrismaUIMenu::Toggle] Not initialized or invalid view");
        return;
    }
    
    bool hasFocus = prismaUI_->HasFocus(view_);
    
    if (hasFocus) {
        // Unfocus and hide
        prismaUI_->Unfocus(view_);
        prismaUI_->Hide(view_);
        spdlog::info("[PrismaUIMenu] Menu closed");
    } else {
        // Show and focus
        prismaUI_->Show(view_);
        prismaUI_->Focus(view_, true, false); // Pause game, don't disable focus menu
        
        // Refresh data when opening
        SendHistoryData();
        
        spdlog::info("[PrismaUIMenu] Menu opened");
    }
}

bool PrismaUIMenu::IsOpen()
{
    if (!initialized_ || !prismaUI_ || !prismaUI_->IsValid(view_)) {
        return false;
    }
    
    return prismaUI_->HasFocus(view_);
}

void PrismaUIMenu::SendHistoryData()
{
    if (!initialized_ || !prismaUI_ || !prismaUI_->IsValid(view_)) {
        spdlog::warn("[PrismaUIMenu::SendHistoryData] Not initialized  or invalid view");
        return;
    }
    
    try {
        std::string jsonData = SerializeHistoryToJSON();
        spdlog::info("[PrismaUIMenu] Sending {} bytes of history data", jsonData.size());
        
        // Escape the JSON string for JavaScript string literal context
        std::string escapedJSON;
        for (char c : jsonData) {
            switch (c) {
                case '\'': escapedJSON += "\\'"; break;
                case '\\': escapedJSON += "\\\\"; break;
                case '\n': escapedJSON += "\\n"; break;
                case '\r': escapedJSON += "\\r"; break;
                case '\t': escapedJSON += "\\t"; break;
                default: escapedJSON += c; break;
            }
        }
        
        // Call JavaScript function with JSON data as a string
        std::string jsCode = "window.SKSE_API?.call('updateHistory', '" + escapedJSON + "')";
        
        prismaUI_->Invoke(view_, jsCode.c_str());
        
        spdlog::info("[PrismaUIMenu] Successfully invoked updateHistory");
    } catch (const std::exception& e) {
        spdlog::error("[PrismaUIMenu] Failed to send history data: {}", e.what());
    }
}

std::string PrismaUIMenu::SerializeHistoryToJSON()
{
    auto* db = DialogueDB::GetDatabase();
    if (!db) {
        return "[]";
    }
    
    // Get recent dialogue from database
    auto history = db->GetRecentDialogue(1000);  // Get last 1000 entries
    
    // Get current blacklist to recalculate statuses (matching STFUMenu::RefreshHistory)
    auto blacklist = db->GetBlacklist();
    
    // Update blocked status for all entries based on current blacklist and MCM settings
    for (auto& entry : history) {
        try {
            // Check if this is a scene or bard song entry
            if ((entry.isScene || entry.isBardSong) && !entry.sceneEditorID.empty()) {
                // Check blacklist for scene entries by scene EditorID
                bool foundInBlacklist = false;
                for (const auto& blEntry : blacklist) {
                    if (blEntry.targetType == DialogueDB::BlacklistTarget::Scene &&
                        !blEntry.targetEditorID.empty() && 
                        blEntry.targetEditorID == entry.sceneEditorID) {
                        
                        // Blacklisted entries always show their block type
                        if (blEntry.blockType == DialogueDB::BlockType::SkyrimNet) {
                            entry.blockedStatus = DialogueDB::BlockedStatus::SkyrimNetBlock;
                        } else if (blEntry.blockType == DialogueDB::BlockType::Hard) {
                            entry.blockedStatus = DialogueDB::BlockedStatus::HardBlock;
                        } else {
                            entry.blockedStatus = DialogueDB::BlockedStatus::SoftBlock;
                        }
                        foundInBlacklist = true;
                        break;
                    }
                }
                
                if (!foundInBlacklist) {
                    entry.blockedStatus = DialogueDB::BlockedStatus::Normal;
                }
            } else {
                // Regular dialogue entry - check topic blacklist
                int64_t blacklistId = db->GetBlacklistEntryId(entry.topicFormID, entry.topicEditorID);
                if (blacklistId > 0) {
                    // Find the blacklist entry
                    for (const auto& blEntry : blacklist) {
                        if (blEntry.id == blacklistId) {
                            // Blacklisted entries always show their block type
                            if (blEntry.blockType == DialogueDB::BlockType::SkyrimNet) {
                                entry.blockedStatus = DialogueDB::BlockedStatus::SkyrimNetBlock;
                            } else if (blEntry.blockType == DialogueDB::BlockType::Hard) {
                                entry.blockedStatus = DialogueDB::BlockedStatus::HardBlock;
                            } else {
                                entry.blockedStatus = DialogueDB::BlockedStatus::SoftBlock;
                            }
                            break;
                        }
                    }
                } else {
                    // Not blacklisted - check if filtered by MCM
                    if (Config::IsFilteredByMCM(entry.topicFormID, entry.topicSubtype)) {
                        entry.blockedStatus = DialogueDB::BlockedStatus::FilteredByConfig;
                    } else if (Config::HasDisabledSubtypeToggle(entry.topicSubtype)) {
                        entry.blockedStatus = DialogueDB::BlockedStatus::ToggledOff;
                    } else {
                        entry.blockedStatus = DialogueDB::BlockedStatus::Normal;
                    }
                }
            }
        } catch (const std::exception& e) {
            spdlog::error("[PrismaUIMenu] Exception processing history status: {}", e.what());
            entry.blockedStatus = DialogueDB::BlockedStatus::Normal;
        }
    }
    
    std::ostringstream json;
    json << "[";
    
    bool first = true;
    for (const auto& entry : history) {
        if (!first) json << ",";
        first = false;
        
        // Helper to escape JSON strings
        auto escapeJSON = [](const std::string& str) -> std::string {
            std::string result;
            result.reserve(str.size());
            for (char c : str) {
                switch (c) {
                    case '"': result += "\\\""; break;
                    case '\\': result += "\\\\"; break;
                    case '\b': result += "\\b"; break;
                    case '\f': result += "\\f"; break;
                    case '\n': result += "\\n"; break;
                    case '\r': result += "\\r"; break;
                    case '\t': result += "\\t"; break;
                    default:
                        if (c < 0x20) {
                            char buf[8];
                            snprintf(buf, sizeof(buf), "\\u%04x", (int)c);
                            result += buf;
                        } else {
                            result += c;
                        }
                        break;
                }
            }
            return result;
        };
        
        // Convert status enum to string
        std::string statusStr;
        switch (entry.blockedStatus) {
            case DialogueDB::BlockedStatus::Normal: 
                statusStr = "Allowed"; 
                break;
            case DialogueDB::BlockedStatus::SoftBlock: 
                statusStr = "Soft Block"; 
                break;
            case DialogueDB::BlockedStatus::HardBlock: 
                statusStr = "Hard Block"; 
                break;
            case DialogueDB::BlockedStatus::SkyrimNetBlock: 
                statusStr = "SkyrimNet Block";
                break;
            case DialogueDB::BlockedStatus::FilteredByConfig: 
                statusStr = "Filter"; 
                break;
            case DialogueDB::BlockedStatus::ToggledOff: 
                statusStr = "Toggled Off"; 
                break;
            default: 
                statusStr = "Unknown"; 
                break;
        }
        
        // Count responses
        int responseCount = static_cast<int>(entry.allResponses.size());
        if (responseCount == 0 && !entry.responseText.empty()) {
            responseCount = 1;  // At least the main response
        }
        
        json << "{";
        json << "\"id\":" << entry.id << ",";
        json << "\"timestamp\":" << entry.timestamp << ",";
        json << "\"speaker\":\"" << escapeJSON(entry.speakerName) << "\",";
        json << "\"text\":\"" << escapeJSON(entry.responseText) << "\",";
        json << "\"questName\":\"" << escapeJSON(entry.questName) << "\",";
        json << "\"questEditorID\":\"" << escapeJSON(entry.questEditorID) << "\",";
        json << "\"topicEditorID\":\"" << escapeJSON(entry.topicEditorID) << "\",";
        
        // Format FormID with one leading zero
        char formIDHex[16];
        sprintf_s(formIDHex, "%08X", entry.topicFormID);
        const char* stripped = formIDHex;
        while (*stripped == '0' && *(stripped + 1) != '\0') {
            stripped++;
        }
        json << "\"topicFormID\":\"0" << stripped << "\",";
        
        json << "\"sourcePlugin\":\"" << escapeJSON(entry.sourcePlugin) << "\",";
        json << "\"subtypeName\":\"" << escapeJSON(entry.topicSubtypeName) << "\",";
        json << "\"topicSubtype\":" << entry.topicSubtype << ",";
        json << "\"status\":\"" << statusStr << "\",";
        json << "\"responseCount\":" << responseCount << ",";
        json << "\"skyrimNetBlockable\":" << (entry.skyrimNetBlockable ? "true" : "false") << ",";
        json << "\"isScene\":" << (entry.isScene ? "true" : "false") << ",";
        json << "\"isBardSong\":" << (entry.isBardSong ? "true" : "false") << ",";
        json << "\"sceneEditorID\":\"" << escapeJSON(entry.sceneEditorID) << "\",";
        
        // Add allResponses array
        json << "\"allResponses\":";
        if (!entry.allResponses.empty()) {
            json << "[";
            for (size_t i = 0; i < entry.allResponses.size(); ++i) {
                if (i > 0) json << ",";
                json << "\"" << escapeJSON(entry.allResponses[i]) << "\"";
            }
            json << "]";
        } else {
            json << "[]";
        }
        json << "}";
    }
    
    json << "]";
    
    return json.str();
}

void PrismaUIMenu::SendBlacklistData()
{
    if (!initialized_ || !prismaUI_ || !prismaUI_->IsValid(view_)) {
        spdlog::warn("[PrismaUIMenu::SendBlacklistData] Not initialized or invalid view");
        return;
    }
    
    try {
        std::string jsonData = SerializeBlacklistToJSON();
        spdlog::info("[PrismaUIMenu] Sending {} bytes of blacklist data", jsonData.size());
        
        // Escape the JSON string for JavaScript string literal context
        std::string escapedJSON;
        for (char c : jsonData) {
            switch (c) {
                case '\'': escapedJSON += "\\'"; break;
                case '\\': escapedJSON += "\\\\"; break;
                case '\n': escapedJSON += "\\n"; break;
                case '\r': escapedJSON += "\\r"; break;
                case '\t': escapedJSON += "\\t"; break;
                default: escapedJSON += c; break;
            }
        }
        
        // Call JavaScript function with JSON data
        std::string jsCode = "window.SKSE_API?.call('updateBlacklist', '" + escapedJSON + "')";
        
        prismaUI_->Invoke(view_, jsCode.c_str());
        
        spdlog::info("[PrismaUIMenu] Successfully invoked updateBlacklist");
    } catch (const std::exception& e) {
        spdlog::error("[PrismaUIMenu] Failed to send blacklist data: {}", e.what());
    }
}

std::string PrismaUIMenu::SerializeBlacklistToJSON()
{
    auto* db = DialogueDB::GetDatabase();
    if (!db) {
        return "[]";
    }
    
    // Get all blacklist entries
    auto blacklist = db->GetBlacklist();
    
    std::ostringstream json;
    json << "[";
    
    bool first = true;
    for (const auto& entry : blacklist) {
        if (!first) json << ",";
        first = false;
        
        // Helper to escape JSON strings
        auto escapeJSON = [](const std::string& str) -> std::string {
            std::string result;
            for (char c : str) {
                switch (c) {
                    case '"': result += "\\\""; break;
                    case '\\': result += "\\\\"; break;
                    case '\b': result += "\\b"; break;
                    case '\f': result += "\\f"; break;
                    case '\n': result += "\\n"; break;
                    case '\r': result += "\\r"; break;
                    case '\t': result += "\\t"; break;
                    default:
                        if (c < 0x20) {
                            char buf[8];
                            snprintf(buf, sizeof(buf), "\\u%04x", (int)c);
                            result += buf;
                        } else {
                            result += c;
                        }
                        break;
                }
            }
            return result;
        };
        
        json << "{";
        json << "\"id\":" << entry.id << ",";
        
        // Add target type
        std::string targetTypeStr;
        switch (entry.targetType) {
            case DialogueDB::BlacklistTarget::Topic: targetTypeStr = "Topic"; break;
            case DialogueDB::BlacklistTarget::Quest: targetTypeStr = "Quest"; break;
            case DialogueDB::BlacklistTarget::Subtype: targetTypeStr = "Subtype"; break;
            case DialogueDB::BlacklistTarget::Scene: targetTypeStr = "Scene"; break;
            case DialogueDB::BlacklistTarget::Plugin: targetTypeStr = "Plugin"; break;
            default: targetTypeStr = "Unknown"; break;
        }
        json << "\"targetType\":\"" << targetTypeStr << "\",";
        
        // Add block type
        std::string blockTypeStr;
        switch (entry.blockType) {
            case DialogueDB::BlockType::Soft: blockTypeStr = "Soft Block"; break;
            case DialogueDB::BlockType::Hard: blockTypeStr = "Hard Block"; break;
            case DialogueDB::BlockType::SkyrimNet: blockTypeStr = "SkyrimNet Block"; break;
            default: blockTypeStr = "Unknown"; break;
        }
        json << "\"blockType\":\"" << blockTypeStr << "\",";
        
        // Format FormID with one leading zero (if present)
        if (entry.targetFormID != 0) {
            char formIDHex[16];
            sprintf_s(formIDHex, "%08X", entry.targetFormID);
            const char* stripped = formIDHex;
            while (*stripped == '0' && *(stripped + 1) != '\0') {
                stripped++;
            }
            json << "\"topicFormID\":\"0" << stripped << "\",";
        } else {
            json << "\"topicFormID\":null,";
        }
        
        json << "\"topicEditorID\":\"" << escapeJSON(entry.targetEditorID) << "\",";
        json << "\"questEditorID\":\"" << escapeJSON(entry.questEditorID) << "\",";
        json << "\"sourcePlugin\":\"" << escapeJSON(entry.sourcePlugin) << "\",";
        
        // Use questEditorID or targetEditorID as quest name
        json << "\"questName\":\"" << escapeJSON(entry.questEditorID) << "\",";
        
        json << "\"filterCategory\":\"" << escapeJSON(entry.filterCategory) << "\",";
        json << "\"note\":\"" << escapeJSON(entry.notes) << "\",";
        json << "\"dateAdded\":" << entry.addedTimestamp << ",";
        
        // Add responseText (already  JSON-encoded array of responses)
        json << "\"responseText\":\"" << escapeJSON(entry.responseText) << "\"";
        json << "}";
    }
    
    json << "]";
    
    return json.str();
}

void PrismaUIMenu::OnAddToBlacklist(const char* data)
{
    if (!data) {
        spdlog::error("[PrismaUIMenu::OnAddToBlacklist] Null data received");
        return;
    }
    
    spdlog::info("[PrismaUIMenu::OnAddToBlacklist] Received data");
    
    try {
        auto* db = DialogueDB::GetDatabase();
        if (!db) {
            spdlog::error("[PrismaUIMenu::OnAddToBlacklist] Database not available");
            return;
        }
        
        // Parse JSON data: {"entries":[{...}],"blockType":"Soft|Hard|SkyrimNet"}
        std::string jsonStr(data);
        
        // Extract block type
        size_t blockTypePos = jsonStr.find("\"blockType\":\"");
        if (blockTypePos == std::string::npos) {
            spdlog::error("[PrismaUIMenu::OnAddToBlacklist] blockType not found");
            return;
        }
        
        size_t blockTypeStart = blockTypePos + 13;
        size_t blockTypeEnd = jsonStr.find("\"", blockTypeStart);
        std::string blockTypeStr = jsonStr.substr(blockTypeStart, blockTypeEnd - blockTypeStart);
        
        DialogueDB::BlockType blockType;
        if (blockTypeStr == "Hard") {
            blockType = DialogueDB::BlockType::Hard;
        } else if (blockTypeStr == "SkyrimNet") {
            blockType = DialogueDB::BlockType::SkyrimNet;
        } else {
            blockType = DialogueDB::BlockType::Soft;
        }
        
        spdlog::info("[PrismaUIMenu::OnAddToBlacklist] Block type: {}", blockTypeStr);
        
        // Helper to extract JSON string value
        auto extractString = [](const std::string& json, const std::string& key, size_t startPos = 0) -> std::string {
            std::string searchKey = "\"" + key + "\":\"";
            size_t keyPos = json.find(searchKey, startPos);
            if (keyPos == std::string::npos) return "";
            size_t valueStart = keyPos + searchKey.length();
            size_t valueEnd = json.find("\"", valueStart);
            if (valueEnd == std::string::npos) return "";
            return json.substr(valueStart, valueEnd - valueStart);
        };
        
        // Find all entry objects by looking for topicEditorID fields (each entry has one)
        std::vector<DialogueDB::BlacklistEntry> entriesToAdd;
        size_t searchPos = 0;
        
        while (true) {
            // Find next topicEditorID field
            size_t topicPos = jsonStr.find("\"topicEditorID\":\"", searchPos);
            if (topicPos == std::string::npos) break;
            
            // Find the start of this object (search backwards for {)
            size_t objStart = jsonStr.rfind("{", topicPos);
            if (objStart == std::string::npos) break;
            
            // Find the end of this object (find matching })
            // This should work because we're looking for the } that closes this specific entry
            size_t objEnd = jsonStr.find("},", topicPos);
            if (objEnd == std::string::npos) {
                // Might be the last entry, check for }]
                objEnd = jsonStr.find("}]", topicPos);
                if (objEnd == std::string::npos) break;
            }
            
            // Extract this entry's JSON substring
            std::string entryStr = jsonStr.substr(objStart, objEnd - objStart + 1);
            
            // Parse entry fields (matching STFUMenu's logic)
            DialogueDB::BlacklistEntry entry;
            
            // Check if this is a scene (matching STFUMenu line 1012)
            bool isScene = (entryStr.find("\"isScene\":true") != std::string::npos);
            bool isBardSong = (entryStr.find("\"isBardSong\":true") != std::string::npos);
            
            // Set target type based on scene status (matching STFUMenu line 1012)
            entry.targetType = (isScene || isBardSong) ? DialogueDB::BlacklistTarget::Scene : DialogueDB::BlacklistTarget::Topic;
            
            // Extract EditorIDs (scenes use sceneEditorID, topics use topicEditorID)
            if (isScene || isBardSong) {
                entry.targetEditorID = extractString(entryStr, "sceneEditorID");
                entry.targetFormID = 0;  // Scenes use EditorID, not FormID
                entry.questEditorID = "";  // Scenes don't have quest context
            } else {
                entry.targetEditorID = extractString(entryStr, "topicEditorID");
                entry.questEditorID = extractString(entryStr, "questEditorID");
                
                // Extract FormID for topics
                std::string formIDStr = extractString(entryStr, "topicFormID");
                entry.targetFormID = 0;
                if (!formIDStr.empty()) {
                    try {
                        entry.targetFormID = std::stoul(formIDStr, nullptr, 16);
                    } catch (...) {
                        spdlog::warn("[PrismaUIMenu::OnAddToBlacklist] Failed to parse FormID: {}", formIDStr);
                    }
                }
            }
            
            entry.sourcePlugin = extractString(entryStr, "sourcePlugin");
            entry.subtypeName = extractString(entryStr, "subtypeName");
            
            // Set fields (matching STFUMenu logic from lines 1032-1049)
            entry.blockType = blockType;
            entry.addedTimestamp = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count();
            
            // Set granular blocking (matching STFUMenu logic)
            if (blockType == DialogueDB::BlockType::SkyrimNet) {
                entry.blockAudio = false;
                entry.blockSubtitles = false;
                entry.blockSkyrimNet = true;
                entry.filterCategory = "SkyrimNet";
            } else {
                entry.blockAudio = true;
                entry.blockSubtitles = true;
                entry.blockSkyrimNet = (blockType == DialogueDB::BlockType::Soft);
                entry.filterCategory = entry.subtypeName.empty() ? "Blacklist" : entry.subtypeName;
            }
            
            entriesToAdd.push_back(entry);
            
            // Move to next entry
            searchPos = objEnd + 1;
        }
        
        spdlog::info("[PrismaUIMenu::OnAddToBlacklist] Parsed {} entries from JSON", entriesToAdd.size());
        
        // Use batch add for efficiency (same as STFUMenu)
        int addedCount = db->AddToBlacklistBatch(entriesToAdd, false);
        
        spdlog::info("[PrismaUIMenu::OnAddToBlacklist] Successfully added {} of {} entries", 
            addedCount, entriesToAdd.size());
        
        // Refresh UI (same as STFUMenu)
        SendBlacklistData();
        SendHistoryData();
        
    } catch (const std::exception& e) {
        spdlog::error("[PrismaUIMenu::OnAddToBlacklist] Exception: {}", e.what());
    }
}

void PrismaUIMenu::OnAddToWhitelist(const char* data)
{
    if (!data) {
        spdlog::error("[PrismaUIMenu::OnAddToWhitelist] Null data received");
        return;
    }
    
    spdlog::info("[PrismaUIMenu::OnAddToWhitelist] Received (NOT IMPLEMENTED): {}", data);
    // TODO: Implement whitelist functionality
}

void PrismaUIMenu::OnRemoveFromBlacklist(const char* data)
{
    if (!data) {
        spdlog::error("[PrismaUIMenu::OnRemoveFromBlacklist] Null data received");
        return;
    }
    
    spdlog::info("[PrismaUIMenu::OnRemoveFromBlacklist] Received: {}", data);
    
    try {
        auto* db = DialogueDB::GetDatabase();
        if (!db) {
            spdlog::error("[PrismaUIMenu::OnRemoveFromBlacklist] Database not available");
            return;
        }
        
        // Parse JSON: {"topicFormID":"...", "topicEditorID":"...", "sceneEditorID":"...", "isScene":bool}
        std::string jsonStr(data);
        
        // Helper to extract JSON string value
        auto extractString = [](const std::string& json, const std::string& key) -> std::string {
            std::string searchKey = "\"" + key + "\":\"";
            size_t keyPos = json.find(searchKey);
            if (keyPos == std::string::npos) return "";
            size_t valueStart = keyPos + searchKey.length();
            size_t valueEnd = json.find("\"", valueStart);
            if (valueEnd == std::string::npos) return "";
            return json.substr(valueStart, valueEnd - valueStart);
        };
        
        // Extract fields
        std::string topicFormIDStr = extractString(jsonStr, "topicFormID");
        std::string topicEditorID = extractString(jsonStr, "topicEditorID");
        std::string sceneEditorID = extractString(jsonStr, "sceneEditorID");
        bool isScene = (jsonStr.find("\"isScene\":true") != std::string::npos);
        
        spdlog::info("[PrismaUIMenu::OnRemoveFromBlacklist] Looking for: topicEditorID='{}', topicFormIDStr='{}', sceneEditorID='{}', isScene={}", 
            topicEditorID, topicFormIDStr, sceneEditorID, isScene);
        
        // Find the blacklist entry
        int64_t blacklistId = -1;
        
        if (isScene && !sceneEditorID.empty()) {
            // Look for scene entry
            auto blacklist = db->GetBlacklist();
            for (const auto& entry : blacklist) {
                if (entry.targetType == DialogueDB::BlacklistTarget::Scene &&
                    entry.targetEditorID == sceneEditorID) {
                    blacklistId = entry.id;
                    break;
                }
            }
        } else if (!topicEditorID.empty() || !topicFormIDStr.empty()) {
            // Look for topic entry - parse FormID
            uint32_t topicFormID = 0;
            if (!topicFormIDStr.empty()) {
                try {
                    topicFormID = std::stoul(topicFormIDStr, nullptr, 16);
                    spdlog::info("[PrismaUIMenu::OnRemoveFromBlacklist] Parsed topicFormID: 0x{:08X}", topicFormID);
                } catch (const std::exception& e) {
                    spdlog::warn("[PrismaUIMenu::OnRemoveFromBlacklist] Failed to parse FormID '{}': {}", topicFormIDStr, e.what());
                }
            }
            
            spdlog::info("[PrismaUIMenu::OnRemoveFromBlacklist] Calling GetBlacklistEntryId with FormID=0x{:08X}, EditorID='{}'", topicFormID, topicEditorID);
            blacklistId = db->GetBlacklistEntryId(topicFormID, topicEditorID);
            spdlog::info("[PrismaUIMenu::OnRemoveFromBlacklist] GetBlacklistEntryId returned: {}", blacklistId);
        } else {
            spdlog::warn("[PrismaUIMenu::OnRemoveFromBlacklist] No valid identifiers provided (topicEditorID and topicFormIDStr both empty)");
        }
        
        if (blacklistId > 0) {
            if (db->RemoveFromBlacklist(blacklistId)) {
                spdlog::info("[PrismaUIMenu::OnRemoveFromBlacklist] Successfully removed entry ID: {}", blacklistId);
                
                // Refresh displays
                SendBlacklistData();
                SendHistoryData();
            } else {
                spdlog::error("[PrismaUIMenu::OnRemoveFromBlacklist] Failed to remove entry ID: {}", blacklistId);
            }
        } else {
            spdlog::warn("[PrismaUIMenu::OnRemoveFromBlacklist] Entry not found in blacklist");
        }
        
    } catch (const std::exception& e) {
        spdlog::error("[PrismaUIMenu::OnRemoveFromBlacklist] Exception: {}", e.what());
    }
}
void PrismaUIMenu::OnToggleSubtypeFilter(const char* data)
{
    if (!data) {
        spdlog::error("[PrismaUIMenu::OnToggleSubtypeFilter] Null data received");
        return;
    }
    
    try {
        spdlog::info("[PrismaUIMenu::OnToggleSubtypeFilter] Received data: {}", data);
        
        std::string jsonStr(data);
        
        // Parse JSON to extract topicSubtype
        // Expected format: {"topicSubtype": 5}
        size_t subtypePos = jsonStr.find("\"topicSubtype\"");
        if (subtypePos == std::string::npos) {
            spdlog::error("[PrismaUIMenu::OnToggleSubtypeFilter] 'topicSubtype' field not found in JSON");
            return;
        }
        
        // Find the colon after "topicSubtype"
        size_t colonPos = jsonStr.find(":", subtypePos);
        if (colonPos == std::string::npos) {
            spdlog::error("[PrismaUIMenu::OnToggleSubtypeFilter] Malformed JSON - no colon after topicSubtype");
            return;
        }
        
        // Extract the numeric value
        size_t numStart = colonPos + 1;
        while (numStart < jsonStr.length() && (jsonStr[numStart] == ' ' || jsonStr[numStart] == '\t')) {
            numStart++;
        }
        
        size_t numEnd = numStart;
        while (numEnd < jsonStr.length() && jsonStr[numEnd] >= '0' && jsonStr[numEnd] <= '9') {
            numEnd++;
        }
        
        if (numStart >= numEnd) {
            spdlog::error("[PrismaUIMenu::OnToggleSubtypeFilter] Could not parse topicSubtype value");
            return;
        }
        
        std::string subtypeStr = jsonStr.substr(numStart, numEnd - numStart);
        uint16_t topicSubtype = static_cast<uint16_t>(std::stoi(subtypeStr));
        
        spdlog::info("[PrismaUIMenu::OnToggleSubtypeFilter] Toggling subtype filter for subtype {}", topicSubtype);
        
        // Call Config function to toggle the subtype filter
        bool success = Config::ToggleSubtypeFilter(topicSubtype);
        
        if (success) {
            spdlog::info("[PrismaUIMenu::OnToggleSubtypeFilter] Successfully toggled subtype {} filter", topicSubtype);
            
            // Refresh history to show updated statuses
            std::string historyJSON = SerializeHistoryToJSON();
            if (prismaUI_ && view_) {
                std::string jsCall = "window.SKSE_API.call('updateHistory', '" + historyJSON + "')";
                prismaUI_->Invoke(view_, jsCall.c_str());
            }
        } else {
            spdlog::warn("[PrismaUIMenu::OnToggleSubtypeFilter] Failed to toggle subtype {} filter (no MCM global?)", topicSubtype);
        }
        
    } catch (const std::exception& e) {
        spdlog::error("[PrismaUIMenu::OnToggleSubtypeFilter] Exception: {}", e.what());
    }
}
void PrismaUIMenu::OnDeleteHistoryEntries(const char* data)
{
    if (!data) {
        spdlog::error("[PrismaUIMenu::OnDeleteHistoryEntries] Null data received");
        return;
    }
    
    try {
        spdlog::info("[PrismaUIMenu::OnDeleteHistoryEntries] Received data: {}", data);
        
        std::string jsonStr(data);
        std::vector<int> entryIds;
        
        // Parse JSON to extract entryIds array
        // Expected format: {"entryIds": [1, 2, 3, ...]}
        size_t entryIdsPos = jsonStr.find("\"entryIds\"");
        if (entryIdsPos == std::string::npos) {
            spdlog::error("[PrismaUIMenu::OnDeleteHistoryEntries] 'entryIds' field not found in JSON");
            return;
        }
        
        // Find the array start bracket
        size_t arrayStart = jsonStr.find("[", entryIdsPos);
        if (arrayStart == std::string::npos) {
            spdlog::error("[PrismaUIMenu::OnDeleteHistoryEntries] Array start bracket not found");
            return;
        }
        
        // Find the array end bracket
        size_t arrayEnd = jsonStr.find("]", arrayStart);
        if (arrayEnd == std::string::npos) {
            spdlog::error("[PrismaUIMenu::OnDeleteHistoryEntries] Array end bracket not found");
            return;
        }
        
        // Extract the array content
        std::string arrayContent = jsonStr.substr(arrayStart + 1, arrayEnd - arrayStart - 1);
        
        // Parse comma-separated integers
        size_t pos = 0;
        while (pos < arrayContent.length()) {
            // Skip whitespace
            while (pos < arrayContent.length() && (arrayContent[pos] == ' ' || arrayContent[pos] == '\t')) {
                pos++;
            }
            
            // Find the number
            size_t numStart = pos;
            while (pos < arrayContent.length() && arrayContent[pos] >= '0' && arrayContent[pos] <= '9') {
                pos++;
            }
            
            if (pos > numStart) {
                std::string numStr = arrayContent.substr(numStart, pos - numStart);
                int entryId = std::stoi(numStr);
                entryIds.push_back(entryId);
            }
            
            // Skip comma and whitespace
            while (pos < arrayContent.length() && (arrayContent[pos] == ',' || arrayContent[pos] == ' ' || arrayContent[pos] == '\t')) {
                pos++;
            }
        }
        
        spdlog::info("[PrismaUIMenu::OnDeleteHistoryEntries] Deleting {} entries", entryIds.size());
        
        // Convert to int64_t for database function
        std::vector<int64_t> entryIds64;
        entryIds64.reserve(entryIds.size());
        for (int id : entryIds) {
            entryIds64.push_back(static_cast<int64_t>(id));
        }
        
        // Delete entries from database
        auto* db = DialogueDB::GetDatabase();
        if (!db) {
            spdlog::error("[PrismaUIMenu::OnDeleteHistoryEntries] Database not available");
            return;
        }
        
        int deletedCount = db->DeleteDialogueEntriesBatch(entryIds64);
        
        spdlog::info("[PrismaUIMenu::OnDeleteHistoryEntries] Successfully deleted {} entries", deletedCount);
        
        // Refresh history display using SendHistoryData for proper escaping
        SendHistoryData();
        
    } catch (const std::exception& e) {
        spdlog::error("[PrismaUIMenu::OnDeleteHistoryEntries] Exception: {}", e.what());
    }
}

void PrismaUIMenu::OnOpenManualEntry(const char* data)
{
    spdlog::info("[PrismaUIMenu::OnOpenManualEntry] Manual entry requested");
    
    try {
        // Parse JSON to check if whitelist or blacklist
        std::string jsonStr(data ? data : "{}");
        bool isWhitelist = (jsonStr.find("\"isWhitelist\":true") != std::string::npos);
        
        spdlog::info("[PrismaUIMenu::OnOpenManualEntry] Opening manual entry for {}", isWhitelist ? "whitelist" : "blacklist");
        
        // Call STFUMenu to open the manual entry modal
        STFUMenu::OpenManualEntry(isWhitelist);
        
    } catch (const std::exception& e) {
        spdlog::error("[PrismaUIMenu::OnOpenManualEntry] Exception: {}", e.what());
    }
}

void PrismaUIMenu::OnDetectIdentifierType(const char* data)
{
    if (!data) {
        spdlog::error("[PrismaUIMenu::OnDetectIdentifierType] Null data received");
        return;
    }
    
    try {
        std::string jsonStr(data);
        spdlog::info("[PrismaUIMenu::OnDetectIdentifierType] Received: {}", jsonStr);
        
        // Extract identifier from JSON
        size_t identifierPos = jsonStr.find("\"identifier\":\"");
        if (identifierPos == std::string::npos) {
            spdlog::error("[PrismaUIMenu::OnDetectIdentifierType] No identifier found");
            return;
        }
        
        size_t identifierStart = identifierPos + 14;
        size_t identifierEnd = jsonStr.find("\"", identifierStart);
        std::string identifier = jsonStr.substr(identifierStart, identifierEnd - identifierStart);
        
        if (identifier.empty()) {
            // Empty identifier - send back default (topic)
            std::string response = R"({"type":"topic","categories":["Blacklist"]})";
            std::string script = "window.handleIdentifierDetection(" + response + ");";
            prismaUI_->Invoke(view_, script.c_str());
            return;
        }
        
        // Detect type (matching STFUMenu logic from lines 5224-5243)
        int detectedType = 0; // 0 = Topic, 1 = Scene, 2 = Plugin
        std::string lowerIdentifier = identifier;
        std::transform(lowerIdentifier.begin(), lowerIdentifier.end(), lowerIdentifier.begin(), ::tolower);
        
        if (lowerIdentifier.ends_with(".esp") || lowerIdentifier.ends_with(".esm") || lowerIdentifier.ends_with(".esl")) {
            detectedType = 2; // Plugin
        } else {
            // Check if it's a FormID (0x format)
            bool isFormID = (identifier.find("0x") == 0);
            
            if (isFormID) {
                // Parse FormID and look up the form
                try {
                    uint32_t formID = std::stoul(identifier, nullptr, 16);
                    auto* form = RE::TESForm::LookupByID(formID);
                    
                    if (form) {
                        // Check if it's a scene
                        auto* scene = form->As<RE::BGSScene>();
                        if (scene) {
                            detectedType = 1; // Scene
                            spdlog::info("[PrismaUIMenu::OnDetectIdentifierType] FormID detected as Scene: {}", identifier);
                        } else {
                            // Check if it's a topic
                            auto* topic = form->As<RE::TESTopic>();
                            if (topic) {
                                detectedType = 0; // Topic
                                spdlog::info("[PrismaUIMenu::OnDetectIdentifierType] FormID detected as Topic: {}", identifier);
                            } else {
                                detectedType = 0; // Default to topic if unknown
                                spdlog::warn("[PrismaUIMenu::OnDetectIdentifierType] FormID is neither Scene nor Topic: {}", identifier);
                            }
                        }
                    } else {
                        detectedType = 0; // Default to topic if form not found
                        spdlog::warn("[PrismaUIMenu::OnDetectIdentifierType] FormID not found: {}", identifier);
                    }
                } catch (...) {
                    detectedType = 0; // Default on parse error
                    spdlog::warn("[PrismaUIMenu::OnDetectIdentifierType] Failed to parse FormID: {}", identifier);
                }
            } else {
                // EditorID - try to detect if it's a scene
                auto* scene = Config::SafeLookupForm<RE::BGSScene>(identifier.c_str());
                if (scene) {
                    detectedType = 1; // Scene
                    spdlog::info("[PrismaUIMenu::OnDetectIdentifierType] EditorID detected as Scene: {}", identifier);
                } else {
                    detectedType = 0; // Topic (default)
                    spdlog::info("[PrismaUIMenu::OnDetectIdentifierType] EditorID detected as Topic: {}", identifier);
                }
            }
        }
        
        // Build categories list (matching STFUMenu::GetFilterCategories)
        std::ostringstream categoriesJson;
        categoriesJson << "[";
        
        if (detectedType == 1) {
            // Scene categories
            categoriesJson << "\"Blacklist\",\"Scene\",\"BardSongs\",\"FollowerCommentary\"";
        } else {
            // Topic categories
            categoriesJson << "\"Blacklist\",";
            categoriesJson << "\"AcceptYield\",\"ActorCollideWithActor\",\"Agree\",\"AlertIdle\",\"AlertToCombat\",\"AlertToNormal\",";
            categoriesJson << "\"AllyKilled\",\"Assault\",\"AssaultNC\",\"Attack\",\"AvoidThreat\",\"BarterExit\",\"Bash\",";
            categoriesJson << "\"Bleedout\",\"Block\",\"CombatToLost\",\"CombatToNormal\",\"Death\",\"DestroyObject\",";
            categoriesJson << "\"DetectFriendDie\",\"ExitFavorState\",\"Flee\",\"Goodbye\",\"Hello\",\"Hit\",\"Idle\",";
            categoriesJson << "\"KnockOverObject\",\"LockedObject\",\"LostIdle\",\"LostToCombat\",\"LostToNormal\",";
            categoriesJson << "\"MoralRefusal\",\"Murder\",\"MurderNC\",\"NormalToAlert\",\"NormalToCombat\",\"NoticeCorpse\",";
            categoriesJson << "\"ObserveCombat\",\"PickpocketCombat\",\"PickpocketNC\",\"PickpocketTopic\",";
            categoriesJson << "\"PlayerCastProjectileSpell\",\"PlayerCastSelfSpell\",\"PlayerInIronSights\",\"PlayerShout\",";
            categoriesJson << "\"PowerAttack\",\"PursueIdleTopic\",\"Refuse\",\"ShootBow\",\"Show\",\"StandOnFurniture\",\"Steal\",";
            categoriesJson << "\"StealFromNC\",\"SwingMeleeWeapon\",\"Taunt\",\"TimeToGo\",\"TrainingExit\",\"Trespass\",";
            categoriesJson << "\"TrespassAgainstNC\",\"VoicePowerEndLong\",\"VoicePowerEndShort\",\"VoicePowerStartLong\",";
            categoriesJson << "\"VoicePowerStartShort\",\"WerewolfTransformCrime\",\"Yield\",\"ZKeyObject\"";
        }
        
        categoriesJson << "]";
        
        // Build response JSON
        std::string typeStr = (detectedType == 1) ? "scene" : (detectedType == 2) ? "plugin" : "topic";
        std::string response = "{\"type\":\"" + typeStr + "\",\"categories\":" + categoriesJson.str() + "}";
        
        spdlog::info("[PrismaUIMenu::OnDetectIdentifierType] Response: {}", response);
        
        // Send response to JavaScript
        std::string script = "window.handleIdentifierDetection(" + response + ");";
        prismaUI_->Invoke(view_, script.c_str());
        
    } catch (const std::exception& e) {
        spdlog::error("[PrismaUIMenu::OnDetectIdentifierType] Exception: {}", e.what());
    }
}

void PrismaUIMenu::OnCreateManualEntry(const char* data)
{
    if (!data) {
        spdlog::error("[PrismaUIMenu::OnCreateManualEntry] Null data received");
        return;
    }
    
    try {
        std::string jsonStr(data);
        spdlog::info("[PrismaUIMenu::OnCreateManualEntry] Received: {}", jsonStr);
        
        auto* db = DialogueDB::GetDatabase();
        if (!db) {
            spdlog::error("[PrismaUIMenu::OnCreateManualEntry] Database not available");
            return;
        }
        
        // Helper to extract JSON string value
        auto extractString = [](const std::string& json, const std::string& key) -> std::string {
            std::string searchKey = "\"" + key + "\":\"";
            size_t keyPos = json.find(searchKey);
            if (keyPos == std::string::npos) return "";
            size_t valueStart = keyPos + searchKey.length();
            size_t valueEnd = json.find("\"", valueStart);
            if (valueEnd == std::string::npos) return "";
            return json.substr(valueStart, valueEnd - valueStart);
        };
        
        // Extract fields
        std::string identifier = extractString(jsonStr, "identifier");
        std::string blockTypeStr = extractString(jsonStr, "blockType");
        std::string category = extractString(jsonStr, "category");
        std::string notes = extractString(jsonStr, "notes");
        
        if (identifier.empty()) {
            spdlog::error("[PrismaUIMenu::OnCreateManualEntry] Empty identifier");
            return;
        }
        
        // Create entry
        DialogueDB::BlacklistEntry entry;
        entry.notes = notes;
        
        // Parse identifier using Config helper (supports 0x format, FormKey format, or EditorID)
        auto [parsedFormID, parsedEditorID] = Config::ParseFormIdentifier(identifier);
        bool isFormIDInput = (parsedFormID != 0);
        
        if (isFormIDInput) {
            // FormID was entered - parse and look up the form
            try {
                entry.targetFormID = parsedFormID;
                
                auto* form = RE::TESForm::LookupByID(parsedFormID);
                if (form) {
                    // Get EditorID from form
                    const char* editorID = form->GetFormEditorID();
                    entry.targetEditorID = editorID ? editorID : "";
                    
                    // Check if it's a scene
                    auto* scene = form->As<RE::BGSScene>();
                    if (scene) {
                        // Scene
                        entry.targetType = DialogueDB::BlacklistTarget::Scene;
                        entry.subtype = 14;
                        entry.subtypeName = "Scene";
                        
                        // Get source plugin
                        auto* file = scene->GetFile(0);
                        entry.sourcePlugin = file ? file->GetFilename() : "";
                        
                        // Extract all responses for this scene
                        auto responses = TopicResponseExtractor::ExtractAllResponsesForScene(entry.targetEditorID);
                        if (!responses.empty()) {
                            // Store as JSON array
                            std::ostringstream responsesJson;
                            responsesJson << "[";
                            for (size_t i = 0; i < responses.size(); ++i) {
                                if (i > 0) responsesJson << ",";
                                // Basic JSON escaping
                                std::string escaped = responses[i];
                                size_t pos = 0;
                                while ((pos = escaped.find("\\", pos)) != std::string::npos) {
                                    escaped.replace(pos, 1, "\\\\");
                                    pos += 2;
                                }
                                pos = 0;
                                while ((pos = escaped.find("\"", pos)) != std::string::npos) {
                                    escaped.replace(pos, 1, "\\\"");
                                    pos += 2;
                                }
                                pos = 0;
                                while ((pos = escaped.find("\n", pos)) != std::string::npos) {
                                    escaped.replace(pos, 1, "\\n");
                                    pos += 2;
                                }
                                responsesJson << "\"" << escaped << "\"";
                            }
                            responsesJson << "]";
                            entry.responseText = responsesJson.str();
                        }
                        
                        spdlog::info("[PrismaUIMenu::OnCreateManualEntry] FormID is Scene: {} (EditorID: {})", identifier, entry.targetEditorID);
                    } else {
                        // Try as topic
                        auto* topic = form->As<RE::TESTopic>();
                        if (topic) {
                            entry.targetType = DialogueDB::BlacklistTarget::Topic;
                            
                            // Get quest context
                            RE::TESQuest* quest = topic->ownerQuest;
                            if (quest) {
                                const char* questEdID = quest->GetFormEditorID();
                                entry.questEditorID = questEdID ? questEdID : "";
                            }
                            
                            // Get subtype
                            uint16_t subtype = Config::GetAccurateSubtype(topic);
                            entry.subtype = subtype;
                            entry.subtypeName = Config::GetSubtypeName(subtype);
                            
                            // Get source plugin
                            auto* file = topic->GetFile(0);
                            entry.sourcePlugin = file ? file->GetFilename() : "";
                            
                            // Extract all responses
                            auto responses = TopicResponseExtractor::ExtractAllResponsesForTopic(parsedFormID);
                            if (!responses.empty()) {
                                // Store as JSON array in response_text field
                                std::ostringstream responsesJson;
                                responsesJson << "[";
                                for (size_t i = 0; i < responses.size(); ++i) {
                                    if (i > 0) responsesJson << ",";
                                    // Escape JSON string
                                    std::string escaped = responses[i];
                                    // Basic JSON escaping
                                    size_t pos = 0;
                                    while ((pos = escaped.find("\\", pos)) != std::string::npos) {
                                        escaped.replace(pos, 1, "\\\\");
                                        pos += 2;
                                    }
                                    pos = 0;
                                    while ((pos = escaped.find("\"", pos)) != std::string::npos) {
                                        escaped.replace(pos, 1, "\\\"");
                                        pos += 2;
                                    }
                                    pos = 0;
                                    while ((pos = escaped.find("\n", pos)) != std::string::npos) {
                                        escaped.replace(pos, 1, "\\n");
                                        pos += 2;
                                    }
                                    responsesJson << "\"" << escaped << "\"";
                                }
                                responsesJson << "]";
                                entry.responseText = responsesJson.str();
                            }
                            
                            spdlog::info("[PrismaUIMenu::OnCreateManualEntry] FormID is Topic: {} (EditorID: {}, Quest: {}, Subtype: {})",
                                identifier, entry.targetEditorID, entry.questEditorID, entry.subtypeName);
                        } else {
                            // Unknown form type - default to Topic
                            entry.targetType = DialogueDB::BlacklistTarget::Topic;
                            spdlog::warn("[PrismaUIMenu::OnCreateManualEntry] FormID type unknown, defaulting to Topic: {}", identifier);
                        }
                    }
                } else {
                    // Form not found - still create entry but without enrichment
                    entry.targetType = DialogueDB::BlacklistTarget::Topic;
                    entry.targetEditorID = "";
                    spdlog::warn("[PrismaUIMenu::OnCreateManualEntry] FormID not found in game data: {}", identifier);
                }
            } catch (...) {
                spdlog::error("[PrismaUIMenu::OnCreateManualEntry] Failed to parse FormID: {}", identifier);
                entry.targetType = DialogueDB::BlacklistTarget::Topic;
                entry.targetFormID = 0;
                entry.targetEditorID = identifier;
            }
        } else {
            // EditorID was entered
            entry.targetEditorID = identifier;
            
            // Check for plugin
            std::string lowerIdentifier = identifier;
            std::transform(lowerIdentifier.begin(), lowerIdentifier.end(), lowerIdentifier.begin(), ::tolower);
            
            if (lowerIdentifier.ends_with(".esp") || lowerIdentifier.ends_with(".esm") || lowerIdentifier.ends_with(".esl")) {
                // Plugin
                entry.targetType = DialogueDB::BlacklistTarget::Plugin;
                entry.targetFormID = 0;
                spdlog::info("[PrismaUIMenu::OnCreateManualEntry] EditorID is Plugin: {}", identifier);
            } else {
                // Try to look up as scene first
                auto* scene = Config::SafeLookupForm<RE::BGSScene>(identifier.c_str());
                if (scene) {
                    // Scene
                    entry.targetType = DialogueDB::BlacklistTarget::Scene;
                    entry.targetFormID = scene->GetFormID();
                    entry.subtype = 14;
                    entry.subtypeName = "Scene";
                    
                    // Get source plugin
                    auto* file = scene->GetFile(0);
                    entry.sourcePlugin = file ? file->GetFilename() : "";
                    
                    // Extract all responses for this scene
                    auto responses = TopicResponseExtractor::ExtractAllResponsesForScene(identifier);
                    if (!responses.empty()) {
                        // Store as JSON array
                        std::ostringstream responsesJson;
                        responsesJson << "[";
                        for (size_t i = 0; i < responses.size(); ++i) {
                            if (i > 0) responsesJson << ",";
                            // Basic JSON escaping
                            std::string escaped = responses[i];
                            size_t pos = 0;
                            while ((pos = escaped.find("\\", pos)) != std::string::npos) {
                                escaped.replace(pos, 1, "\\\\");
                                pos += 2;
                            }
                            pos = 0;
                            while ((pos = escaped.find("\"", pos)) != std::string::npos) {
                                escaped.replace(pos, 1, "\\\"");
                                pos += 2;
                            }
                            pos = 0;
                            while ((pos = escaped.find("\n", pos)) != std::string::npos) {
                                escaped.replace(pos, 1, "\\n");
                                pos += 2;
                            }
                            responsesJson << "\"" << escaped << "\"";
                        }
                        responsesJson << "]";
                        entry.responseText = responsesJson.str();
                    }
                    
                    spdlog::info("[PrismaUIMenu::OnCreateManualEntry] EditorID is Scene: {} (FormID: 0x{:08X})", identifier, entry.targetFormID);
                } else {
                    // Try as topic
                    auto* topic = RE::TESForm::LookupByEditorID<RE::TESTopic>(identifier.c_str());
                    if (topic) {
                        entry.targetType = DialogueDB::BlacklistTarget::Topic;
                        entry.targetFormID = topic->GetFormID();
                        
                        // Get quest context
                        RE::TESQuest* quest = topic->ownerQuest;
                        if (quest) {
                            const char* questEdID = quest->GetFormEditorID();
                            entry.questEditorID = questEdID ? questEdID : "";
                        }
                        
                        // Get subtype
                        uint16_t subtype = Config::GetAccurateSubtype(topic);
                        entry.subtype = subtype;
                        entry.subtypeName = Config::GetSubtypeName(subtype);
                        
                        // Get source plugin
                        auto* file = topic->GetFile(0);
                        entry.sourcePlugin = file ? file->GetFilename() : "";
                        
                        // Extract all responses
                        auto responses = TopicResponseExtractor::ExtractAllResponsesForTopic(identifier);
                        if (!responses.empty()) {
                            // Store as JSON array
                            std::ostringstream responsesJson;
                            responsesJson << "[";
                            for (size_t i = 0; i < responses.size(); ++i) {
                                if (i > 0) responsesJson << ",";
                                // Basic JSON escaping
                                std::string escaped = responses[i];
                                size_t pos = 0;
                                while ((pos = escaped.find("\\", pos)) != std::string::npos) {
                                    escaped.replace(pos, 1, "\\\\");
                                    pos += 2;
                                }
                                pos = 0;
                                while ((pos = escaped.find("\"", pos)) != std::string::npos) {
                                    escaped.replace(pos, 1, "\\\"");
                                    pos += 2;
                                }
                                pos = 0;
                                while ((pos = escaped.find("\n", pos)) != std::string::npos) {
                                    escaped.replace(pos, 1, "\\n");
                                    pos += 2;
                                }
                                responsesJson << "\"" << escaped << "\"";
                            }
                            responsesJson << "]";
                            entry.responseText = responsesJson.str();
                        }
                        
                        spdlog::info("[PrismaUIMenu::OnCreateManualEntry] EditorID is Topic: {} (FormID: 0x{:08X}, Quest: {}, Subtype: {})",
                            identifier, entry.targetFormID, entry.questEditorID, entry.subtypeName);
                    } else {
                        // Not found - default to Topic without enrichment
                        entry.targetType = DialogueDB::BlacklistTarget::Topic;
                        entry.targetFormID = 0;
                        spdlog::warn("[PrismaUIMenu::OnCreateManualEntry] EditorID not found in game data: {}", identifier);
                    }
                }
            }
        }
        
        // Set block type and granular flags
        DialogueDB::BlockType blockType;
        if (blockTypeStr == "Hard") {
            blockType = DialogueDB::BlockType::Hard;
        } else {
            blockType = DialogueDB::BlockType::Soft;
        }
        
        entry.blockType = blockType;
        entry.blockAudio = true;
        entry.blockSubtitles = true;
        entry.blockSkyrimNet = true;
        
        // Set filter category
        entry.filterCategory = !category.empty() ? category : "Blacklist";
        
        // Set timestamp
        entry.addedTimestamp = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
        
        // Add to database
        bool success = db->AddToBlacklist(entry);
        
        if (success) {
            spdlog::info("[PrismaUIMenu::OnCreateManualEntry] Added manual entry: {} (Type: {}, Category: {}, Plugin: {})",
                identifier, 
                entry.targetType == DialogueDB::BlacklistTarget::Scene ? "Scene" : "Topic",
                entry.filterCategory,
                entry.sourcePlugin);
            
            // Refresh UI
            SendBlacklistData();
            SendHistoryData();
        } else {
            spdlog::error("[PrismaUIMenu::OnCreateManualEntry] Failed to add entry: {}", identifier);
        }
        
    } catch (const std::exception& e) {
        spdlog::error("[PrismaUIMenu::OnCreateManualEntry] Exception: {}", e.what());
    }
}
