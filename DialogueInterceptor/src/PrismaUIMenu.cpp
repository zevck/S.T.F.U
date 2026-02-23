#include "PrismaUIMenu.h"
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
                statusStr = "SkyrimNet";
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
        json << "\"status\":\"" << statusStr << "\",";
        json << "\"responseCount\":" << responseCount << ",";
        json << "\"skyrimNetBlockable\":" << (entry.skyrimNetBlockable ? "true" : "false");
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
        json << "\"dateAdded\":" << entry.addedTimestamp;
        json << "}";
    }
    
    json << "]";
    
    return json.str();
}
