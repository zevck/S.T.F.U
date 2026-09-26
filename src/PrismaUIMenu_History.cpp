/*
 * STFU - a Skyrim SKSE plugin for silencing and filtering NPC dialogue.
 * Copyright (C) 2026 Zevick
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "PrismaUIMenu.h"
#include "EditorID.h"
#include "Config.h"
#include "SettingsPersistence.h"
#include "TopicResponseExtractor.h"
#include <spdlog/spdlog.h>
#include <sstream>
#include <iomanip>
#include "PrismaUIMenuJson.h"

using namespace PrismaUIMenuDetail;

void PrismaUIMenu::OnRequestHistory(const char* data)
{
    spdlog::trace("[PrismaUIMenu] History data requested");
    SendHistoryData();
}

void PrismaUIMenu::SendHistoryData()
{
    if (!initialized_ || !prismaUI_ || !prismaUI_->IsValid(view_)) {
        spdlog::warn("[PrismaUIMenu::SendHistoryData] Not initialized  or invalid view");
        return;
    }
    
    // Flush pending write queue so recently-logged dialogue is visible immediately
    auto* db = DialogueDB::GetDatabase();
    if (db) {
        db->FlushQueue();
    }
    
    try {
        std::string jsonData = SerializeHistoryToJSON();
        spdlog::info("[PrismaUIMenu] Sending {} bytes of history data", jsonData.size());
        
        std::string jsCode = BuildSKSEUpdateScript("updateHistory", jsonData);
        prismaUI_->Invoke(view_, jsCode.c_str());
        
        spdlog::debug("[PrismaUIMenu] Successfully invoked updateHistory");
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
    
    // Get current blacklist and whitelist to recalculate statuses
    auto blacklist = db->GetBlacklist();
    auto whitelist = db->GetWhitelist();
    
    // Update blocked status for all entries based on whitelist, blacklist, and MCM settings
    for (auto& entry : history) {
        try {
            bool foundInWhitelist = false;
            
            // First check whitelist - whitelisted entries always show as "Allowed" regardless of other settings
            if ((entry.isScene || entry.isBardSong) && !entry.sceneEditorID.empty()) {
                // Check whitelist for scene entries by scene EditorID
                for (const auto& wlEntry : whitelist) {
                    if (wlEntry.targetType == DialogueDB::BlacklistTarget::Scene &&
                        !wlEntry.targetEditorID.empty() && 
                        wlEntry.targetEditorID == entry.sceneEditorID) {
                        entry.blockedStatus = DialogueDB::BlockedStatus::Whitelisted; // Whitelisted status
                        foundInWhitelist = true;
                        break;
                    }
                }
            } else {
                // Regular dialogue entry - check topic whitelist with actor filtering
                for (const auto& wlEntry : whitelist) {
                    if (wlEntry.targetType == DialogueDB::BlacklistTarget::Topic &&
                        DialogueDB::EntryMatchesTarget(wlEntry, entry.topicFormID, entry.topicEditorID)) {

                        // Whitelist entry found - check actor filter
                        bool hasActorFilter = !wlEntry.actorFilterFormIDs.empty() || !wlEntry.actorFilterNames.empty();
                        bool actorFilterMatches = DialogueDB::EntryActorFilterMatches(wlEntry, entry.speakerFormID, entry.speakerName);
                        
                        // Check faction filter using base form (always in memory, unlike placed references)
                        bool hasFactionFilter = !wlEntry.factionFilterEditorIDs.empty();
                        bool factionFilterMatches = false;
                        if (hasFactionFilter && entry.speakerBaseFormID != 0) {
                            auto* npcBase = RE::TESForm::LookupByID<RE::TESNPC>(entry.speakerBaseFormID);
                            if (npcBase) {
                                for (auto& factionInfo : npcBase->factions) {
                                    if (factionFilterMatches) break;
                                    if (!factionInfo.faction) continue;
                                    const char* editorID = STFU::GetEditorID(factionInfo.faction);
                                    if (!editorID || !editorID[0]) continue;
                                    for (const auto& filterFaction : wlEntry.factionFilterEditorIDs) {
                                        if (filterFaction == editorID) {
                                            factionFilterMatches = true;
                                            break;
                                        }
                                    }
                                }
                            }
                        }
                        
                        // Determine if this entry matches: no filters = everyone, else actor OR faction must match
                        bool filterMatches = false;
                        if (!hasActorFilter && !hasFactionFilter) {
                            filterMatches = true;
                        } else if (hasActorFilter && actorFilterMatches) {
                            filterMatches = true;
                        } else if (hasFactionFilter && factionFilterMatches) {
                            filterMatches = true;
                        }
                        
                        if (filterMatches) {
                            entry.blockedStatus = DialogueDB::BlockedStatus::Whitelisted;
                            foundInWhitelist = true;
                            break;  // Only break if actor matches and we set Whitelisted status
                        }
                    }
                }

                // Check Actor whitelist (target_type=6) - always check regardless of topic whitelist
                if (entry.speakerFormID != 0) {
                    for (const auto& wlEntry : whitelist) {
                        if (wlEntry.targetType == DialogueDB::BlacklistTarget::Actor &&
                            DialogueDB::EntryMatchesActor(wlEntry, entry.speakerFormID)) {
                            entry.blockedStatus = DialogueDB::BlockedStatus::Whitelisted;
                            entry.isActorWhitelisted = true;
                            foundInWhitelist = true;
                            break;
                        }
                    }
                }

                // Check Faction whitelist (target_type=7) - always check regardless of actor whitelist
                if (entry.speakerBaseFormID != 0) {
                    auto* npcBase = RE::TESForm::LookupByID<RE::TESNPC>(entry.speakerBaseFormID);
                    if (npcBase) {
                        bool foundFactionWl = false;
                        for (const auto& wlEntry : whitelist) {
                            if (foundFactionWl) break;
                            if (wlEntry.targetType == DialogueDB::BlacklistTarget::Faction &&
                                (!wlEntry.targetEditorID.empty() || wlEntry.targetFormID != 0)) {
                                for (auto& factionInfo : npcBase->factions) {
                                    if (!factionInfo.faction) continue;
                                    bool match = (wlEntry.targetFormID != 0 && factionInfo.faction->GetFormID() == wlEntry.targetFormID);
                                    if (!match && !wlEntry.targetEditorID.empty()) {
                                        const char* edID = STFU::GetEditorID(factionInfo.faction);
                                        match = (edID && wlEntry.targetEditorID == edID);
                                    }
                                    if (match) {
                                        entry.blockedStatus = DialogueDB::BlockedStatus::Whitelisted;
                                        entry.whitelistFactionEditorID = !wlEntry.targetEditorID.empty()
                                            ? wlEntry.targetEditorID
                                            : "";
                                        foundFactionWl = true;
                                        foundInWhitelist = true;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }
            }
            
            // If not whitelisted, check blacklist and filters
            if (!foundInWhitelist) {
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
                                // Check actor filter - if entry has actor filters, verify speaker matches
                                bool hasActorFilter = !blEntry.actorFilterFormIDs.empty() || !blEntry.actorFilterNames.empty();
                                bool actorFilterMatches = DialogueDB::EntryActorFilterMatches(blEntry, entry.speakerFormID, entry.speakerName);
                                
                                // Check faction filter using base form (always in memory, unlike placed references)
                                bool hasFactionFilter = !blEntry.factionFilterEditorIDs.empty();
                                bool factionFilterMatches = false;
                                std::string matchedFactionEditorID;
                                if (hasFactionFilter && entry.speakerBaseFormID != 0) {
                                    auto* npcBase = RE::TESForm::LookupByID<RE::TESNPC>(entry.speakerBaseFormID);
                                    if (npcBase) {
                                        for (auto& factionInfo : npcBase->factions) {
                                            if (factionFilterMatches) break;
                                            if (!factionInfo.faction) continue;
                                            const char* editorID = STFU::GetEditorID(factionInfo.faction);
                                            if (!editorID || !editorID[0]) continue;
                                            for (const auto& filterFaction : blEntry.factionFilterEditorIDs) {
                                                if (filterFaction == editorID) {
                                                    factionFilterMatches = true;
                                                    matchedFactionEditorID = filterFaction;
                                                    break;
                                                }
                                            }
                                        }
                                    }
                                }
                                
                                // Determine if this entry matches: no filters = block everyone, else actor OR faction must match
                                bool filterMatches = false;
                                if (!hasActorFilter && !hasFactionFilter) {
                                    filterMatches = true;
                                } else if (hasActorFilter && actorFilterMatches) {
                                    filterMatches = true;
                                } else if (hasFactionFilter && factionFilterMatches) {
                                    filterMatches = true;
                                }
                                
                                if (filterMatches) {
                                    // Blacklisted entries show their block type
                                    if (blEntry.blockType == DialogueDB::BlockType::SkyrimNet) {
                                        entry.blockedStatus = DialogueDB::BlockedStatus::SkyrimNetBlock;
                                    } else if (blEntry.blockType == DialogueDB::BlockType::Hard) {
                                        entry.blockedStatus = DialogueDB::BlockedStatus::HardBlock;
                                    } else {
                                        entry.blockedStatus = DialogueDB::BlockedStatus::SoftBlock;
                                    }
                                    // Record faction that triggered filter (for UI indicator)
                                    if (hasFactionFilter && factionFilterMatches) {
                                        entry.blockingFactionEditorID = matchedFactionEditorID;
                                    }
                                } else {
                                    // Actor doesn't match filter - treat as if not blacklisted
                                    if (Config::IsFilteredByMCM(entry.topicFormID, entry.topicSubtype)) {
                                        entry.blockedStatus = DialogueDB::BlockedStatus::FilteredByConfig;
                                    } else if (Config::HasDisabledSubtypeToggle(entry.topicSubtype)) {
                                        entry.blockedStatus = DialogueDB::BlockedStatus::ToggledOff;
                                    } else {
                                        entry.blockedStatus = DialogueDB::BlockedStatus::Normal;
                                    }
                                }
                                break;
                            }
                        }
                    } else {
                        // Not blacklisted by topic - check Actor/Faction blacklist
                        // Run both checks independently so both flags are captured even when both match
                        bool foundActorFactionBl = false;
                        if (entry.speakerFormID != 0) {
                            for (const auto& blEntry : blacklist) {
                                if (blEntry.targetType == DialogueDB::BlacklistTarget::Actor &&
                                    DialogueDB::EntryMatchesActor(blEntry, entry.speakerFormID)) {
                                    // Actor blocks are always soft
                                    if (blEntry.blockType == DialogueDB::BlockType::SkyrimNet) {
                                        entry.blockedStatus = DialogueDB::BlockedStatus::SkyrimNetBlock;
                                    } else {
                                        entry.blockedStatus = DialogueDB::BlockedStatus::SoftBlock;
                                    }
                                    entry.isActorBlocked = true;
                                    foundActorFactionBl = true;
                                    break;
                                }
                            }
                        }
                        // Always check faction regardless of whether actor matched
                        bool foundFactionBl = false;
                        if (entry.speakerBaseFormID != 0) {
                            auto* npcBase = RE::TESForm::LookupByID<RE::TESNPC>(entry.speakerBaseFormID);
                            if (npcBase) {
                                for (const auto& blEntry : blacklist) {
                                    if (foundFactionBl) break;
                                    if (blEntry.targetType == DialogueDB::BlacklistTarget::Faction &&
                                        (!blEntry.targetEditorID.empty() || blEntry.targetFormID != 0)) {
                                        for (auto& factionInfo : npcBase->factions) {
                                            if (!factionInfo.faction) continue;
                                            bool match = (blEntry.targetFormID != 0 && factionInfo.faction->GetFormID() == blEntry.targetFormID);
                                            if (!match && !blEntry.targetEditorID.empty()) {
                                                const char* edID = STFU::GetEditorID(factionInfo.faction);
                                                match = (edID && blEntry.targetEditorID == edID);
                                            }
                                            if (match) {
                                                // Faction blocks are always soft
                                                if (blEntry.blockType == DialogueDB::BlockType::SkyrimNet) {
                                                    entry.blockedStatus = DialogueDB::BlockedStatus::SkyrimNetBlock;
                                                } else {
                                                    entry.blockedStatus = DialogueDB::BlockedStatus::SoftBlock;
                                                }
                                                // Capture which faction entry caused the block (for UI)
                                                entry.blockingFactionEditorID = !blEntry.targetEditorID.empty()
                                                    ? blEntry.targetEditorID
                                                    : "";
                                                foundFactionBl = true;
                                                foundActorFactionBl = true;
                                                break;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                        if (!foundActorFactionBl) {
                            // Check if filtered by MCM
                            if (Config::IsFilteredByMCM(entry.topicFormID, entry.topicSubtype)) {
                                entry.blockedStatus = DialogueDB::BlockedStatus::FilteredByConfig;
                            } else if (Config::HasDisabledSubtypeToggle(entry.topicSubtype)) {
                                entry.blockedStatus = DialogueDB::BlockedStatus::ToggledOff;
                            } else {
                                entry.blockedStatus = DialogueDB::BlockedStatus::Normal;
                            }
                        }
                    }
                }
            }
            // Even when whitelisted, detect actor/faction blacklist for UI indicator flags
            if (foundInWhitelist && !entry.isScene && !entry.isBardSong) {
                // Check actor blacklist indicator
                if (entry.speakerFormID != 0) {
                    for (const auto& blEntry : blacklist) {
                        if (blEntry.targetType == DialogueDB::BlacklistTarget::Actor &&
                            DialogueDB::EntryMatchesActor(blEntry, entry.speakerFormID)) {
                            entry.isActorBlocked = true;
                            break;
                        }
                    }
                }
                // Check faction blacklist indicator
                if (entry.speakerBaseFormID != 0 && entry.blockingFactionEditorID.empty()) {
                    auto* npcBase = RE::TESForm::LookupByID<RE::TESNPC>(entry.speakerBaseFormID);
                    if (npcBase) {
                        bool found = false;
                        for (const auto& blEntry : blacklist) {
                            if (found) break;
                            if (blEntry.targetType == DialogueDB::BlacklistTarget::Faction &&
                                (!blEntry.targetEditorID.empty() || blEntry.targetFormID != 0)) {
                                for (auto& factionInfo : npcBase->factions) {
                                    if (!factionInfo.faction) continue;
                                    bool match = (blEntry.targetFormID != 0 && factionInfo.faction->GetFormID() == blEntry.targetFormID);
                                    if (!match && !blEntry.targetEditorID.empty()) {
                                        const char* edID = STFU::GetEditorID(factionInfo.faction);
                                        match = (edID && blEntry.targetEditorID == edID);
                                    }
                                    if (match) {
                                        entry.blockingFactionEditorID = !blEntry.targetEditorID.empty()
                                            ? blEntry.targetEditorID
                                            : "";
                                        found = true;
                                        break;
                                    }
                                }
                            }
                        }
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
            case DialogueDB::BlockedStatus::Whitelisted: 
                statusStr = "Whitelist"; 
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
        
        // Add speaker FormID for actor filtering
        char speakerFormIDHex[20];
        sprintf_s(speakerFormIDHex, "0x%08X", entry.speakerFormID);
        json << "\"speakerFormID\":\"" << speakerFormIDHex << "\",";
        
        json << "\"text\":\"" << escapeJSON(entry.responseText) << "\",";
        json << "\"questName\":\"" << escapeJSON(entry.questName) << "\",";
        json << "\"questEditorID\":\"" << escapeJSON(entry.questEditorID) << "\",";
        json << "\"topicEditorID\":\"" << escapeJSON(entry.topicEditorID) << "\",";
        
        // Always emit FormID as "0x" + 8 fixed hex chars to match speakerFormID.
        // The previous "strip leading zeros then prepend one 0" logic produced 9-char
        // strings (e.g. "067400023") for FormIDs with a high byte >= 0x10, which then
        // overflowed Config::ParseFormIdentifier's 8-char limit and was silently
        // stored as an EditorID with FormID=0 — breaking blacklist matching.
        char formIDHex[16];
        sprintf_s(formIDHex, "0x%08X", entry.topicFormID);
        json << "\"topicFormID\":\"" << formIDHex << "\",";
        
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
        json << ",";
        json << "\"isActorBlocked\":" << (entry.isActorBlocked ? "true" : "false") << ",";
        json << "\"blockingFactionEditorID\":\"" << escapeJSON(entry.blockingFactionEditorID) << "\",";
        json << "\"isActorWhitelisted\":" << (entry.isActorWhitelisted ? "true" : "false") << ",";
        json << "\"whitelistFactionEditorID\":\"" << escapeJSON(entry.whitelistFactionEditorID) << "\",";
        json << "\"isSubtypeFiltered\":" << (Config::IsFilteredByMCM(entry.topicFormID, entry.topicSubtype) ? "true" : "false") << ",";
        json << "\"isSubtypeToggledOff\":" << (Config::HasDisabledSubtypeToggle(entry.topicSubtype) ? "true" : "false") << "";
        json << "}";
    }
    
    json << "]";
    
    return json.str();
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
