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

#include "../include/PCH.h"
#include "EditorID.h"
#include "DialogueDatabase.h"
#include "Config.h"
#include "PopulateTopicInfoHook.h"
#include "TopicResponseExtractor.h"
#include "SceneHook.h"
#include "DialogueLogger.h"
#include "PrismaUIMenu.h"
#include <spdlog/spdlog.h>
#include <filesystem>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <cstring>
#include "DialogueDatabaseInternal.h"

namespace DialogueDB
{
    bool Database::ShouldSoftBlock(uint32_t formID, const std::string& editorID, uint32_t actorFormID, const std::string& actorName, RE::TESObjectREFR* actorRef)
    {
        std::lock_guard<std::recursive_mutex> lock(dbMutex_);
        
        if (!db_) return false;
        
        // FIRST: Check whitelist (takes priority over blacklist)
        const char* whitelistSql = "SELECT actor_filter_formids, actor_filter_names, faction_filter_editorids FROM whitelist WHERE ((target_formid = ? OR target_formid = 0) AND target_editorid = ?) OR (target_editorid = '' AND target_formid = ?) LIMIT 1;";
        sqlite3_stmt* whitelistStmt = nullptr;
        
        if (sqlite3_prepare_v2(db_, whitelistSql, -1, &whitelistStmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(whitelistStmt, 1, formID);
            sqlite3_bind_text(whitelistStmt, 2, editorID.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(whitelistStmt, 3, formID);
            
            if (sqlite3_step(whitelistStmt) == SQLITE_ROW) {
                // Whitelist entry found - check actor filter
                const char* actorFormIDsJson = reinterpret_cast<const char*>(sqlite3_column_text(whitelistStmt, 0));
                const char* actorNamesJson = reinterpret_cast<const char*>(sqlite3_column_text(whitelistStmt, 1));
                const char* factionEditorIDsJson = reinterpret_cast<const char*>(sqlite3_column_text(whitelistStmt, 2));
                
                std::string actorFormIDsStr = actorFormIDsJson ? actorFormIDsJson : "[]";
                std::string actorNamesStr = actorNamesJson ? actorNamesJson : "[]";
                std::string factionEditorIDsStr = factionEditorIDsJson ? factionEditorIDsJson : "[]";
                
                std::vector<uint32_t> filterFormIDs = ParseActorFormIDsFromJson(actorFormIDsStr);
                std::vector<std::string> filterNames = ParseActorNamesFromJson(actorNamesStr);
                std::vector<std::string> factionFilter = ParseFactionEditorIDsFromJson(factionEditorIDsStr);
                
                // If no filter at all, whitelist applies to everyone
                // If filter exists, check if actor OR faction matches
                bool hasActorFilter = !filterFormIDs.empty() || !filterNames.empty();
                bool hasFactionFilter = !factionFilter.empty();
                
                if (!hasActorFilter && !hasFactionFilter) {
                    // No filters - whitelist applies to everyone
                    spdlog::debug("[DialogueDB] ShouldSoftBlock: Whitelisted (no filter) -> DON'T BLOCK");
                    sqlite3_finalize(whitelistStmt);
                    return false;
                } else if ((actorFormID > 0 || !actorName.empty() || actorRef) && 
                          (ActorMatchesFilter(actorFormID, actorName, filterFormIDs, filterNames) || 
                           FactionMatchesFilter(actorRef, factionFilter))) {
                    // Actor or faction matches whitelist filter
                    spdlog::debug("[DialogueDB] ShouldSoftBlock: Whitelisted for actor '{}' (0x{:08X}) OR faction -> DON'T BLOCK", actorName, actorFormID);
                    sqlite3_finalize(whitelistStmt);
                    return false;
                }
            }
            sqlite3_finalize(whitelistStmt);
        }

        // Check Actor whitelist: if this specific NPC reference is whitelisted, don't block
        if (actorFormID > 0) {
            const char* actorWlSql = "SELECT id FROM whitelist WHERE target_type = 6 AND target_formid = ? LIMIT 1;";
            sqlite3_stmt* actorWlStmt = nullptr;
            if (sqlite3_prepare_v2(db_, actorWlSql, -1, &actorWlStmt, nullptr) == SQLITE_OK) {
                sqlite3_bind_int(actorWlStmt, 1, actorFormID);
                if (sqlite3_step(actorWlStmt) == SQLITE_ROW) {
                    spdlog::debug("[DialogueDB] ShouldSoftBlock: Actor 0x{:08X} is whitelisted -> DON'T BLOCK", actorFormID);
                    sqlite3_finalize(actorWlStmt);
                    return false;
                }
                sqlite3_finalize(actorWlStmt);
            }
        }

        // Check Faction whitelist: if any of this actor's factions are whitelisted, don't block
        if (actorRef) {
            auto actorFactions = GetActorFactionEditorIDs(actorRef);
            for (const auto& factionEditorID : actorFactions) {
                const char* factionWlSql = "SELECT id FROM whitelist WHERE target_type = 7 AND target_editorid = ? LIMIT 1;";
                sqlite3_stmt* factionWlStmt = nullptr;
                if (sqlite3_prepare_v2(db_, factionWlSql, -1, &factionWlStmt, nullptr) == SQLITE_OK) {
                    sqlite3_bind_text(factionWlStmt, 1, factionEditorID.c_str(), -1, SQLITE_TRANSIENT);
                    if (sqlite3_step(factionWlStmt) == SQLITE_ROW) {
                        spdlog::debug("[DialogueDB] ShouldSoftBlock: Actor's faction '{}' is whitelisted -> DON'T BLOCK", factionEditorID);
                        sqlite3_finalize(factionWlStmt);
                        return false;
                    }
                    sqlite3_finalize(factionWlStmt);
                }
            }
        }

        // SECOND: Check blacklist (only if not whitelisted)
        const char* sql = "SELECT block_type, filter_category, actor_filter_formids, actor_filter_names, faction_filter_editorids FROM blacklist WHERE ((target_formid = ? OR target_formid = 0) AND target_editorid = ?) OR (target_editorid = '' AND target_formid = ?) LIMIT 1;";
        sqlite3_stmt* stmt = nullptr;
        
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            spdlog::error("[DialogueDB] ShouldSoftBlock: Failed to prepare statement: {}", sqlite3_errmsg(db_));
            return false;
        }
        
        sqlite3_bind_int(stmt, 1, formID);
        sqlite3_bind_text(stmt, 2, editorID.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 3, formID);
        
        bool shouldBlock = false;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            BlockType blockType = static_cast<BlockType>(sqlite3_column_int(stmt, 0));
            
            // Get filterCategory (default to "Blacklist" if not set)
            const char* filterCategoryC = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            std::string filterCategory = (filterCategoryC && filterCategoryC[0]) ? filterCategoryC : "Blacklist";
            
            // Get actor filters
            const char* actorFormIDsJson = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            const char* actorNamesJson = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            const char* factionEditorIDsJson = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
            
            std::string actorFormIDsStr = actorFormIDsJson ? actorFormIDsJson : "[]";
            std::string actorNamesStr = actorNamesJson ? actorNamesJson : "[]";
            std::string factionEditorIDsStr = factionEditorIDsJson ? factionEditorIDsJson : "[]";
            
            // Parse actor filters from database
            std::vector<uint32_t> filterFormIDs = ParseActorFormIDsFromJson(actorFormIDsStr);
            std::vector<std::string> filterNames = ParseActorNamesFromJson(actorNamesStr);
            std::vector<std::string> factionFilter = ParseFactionEditorIDsFromJson(factionEditorIDsStr);
            
            // Debug: Log what we're checking
            spdlog::debug("[DialogueDB] ShouldSoftBlock: FormID=0x{:08X}, speakerFormID=0x{:08X}, speakerName='{}', filterFormIDs.size()={}, filterNames.size()={}, factionFilter.size()={}", 
                formID, actorFormID, actorName, filterFormIDs.size(), filterNames.size(), factionFilter.size());
            
            // Check if actor OR faction matches filter (if actor info provided AND filter exists)
            bool hasActorFilter = !filterFormIDs.empty() || !filterNames.empty();
            bool hasFactionFilter = !factionFilter.empty();
            bool actorInfoProvided = actorFormID > 0 || !actorName.empty() || actorRef;
            
            if (actorInfoProvided && (hasActorFilter || hasFactionFilter)) {
                // Filters exist - check if actor/faction matches
                bool actorMatches = ActorMatchesFilter(actorFormID, actorName, filterFormIDs, filterNames);
                bool factionMatches = FactionMatchesFilter(actorRef, factionFilter);
                
                if (!actorMatches && !factionMatches) {
                    // Actor doesn't match actor filter AND doesn't match faction filter
                    spdlog::debug("[DialogueDB] ShouldSoftBlock: actor '{}' (0x{:08X}) not in actor filter and not in faction filter -> ALLOW", 
                        actorName, actorFormID);
                    sqlite3_finalize(stmt);
                    return false;
                } else {
                    spdlog::debug("[DialogueDB] ShouldSoftBlock: actor '{}' (0x{:08X}) matches actor filter or faction filter -> checking block type", 
                        actorName, actorFormID);
                }
            } else if ((hasActorFilter || hasFactionFilter) && !actorInfoProvided) {
                // Filter exists but no actor info provided - skip actor/faction-specific entries
                spdlog::debug("[DialogueDB] ShouldSoftBlock: Actor/Faction filter exists but no speaker info provided -> ALLOW");
                sqlite3_finalize(stmt);
                return false;
            } else if (!hasActorFilter && !hasFactionFilter && actorInfoProvided) {
                // No filter at all (both actor and faction filters empty) - block affects everyone
                spdlog::debug("[DialogueDB] ShouldSoftBlock: No actor or faction filter - affects all actors");
            }
            
            // SkyrimNet-only blocks should never soft block (audio/subtitles still play)
            if (blockType == BlockType::SkyrimNet) {
                shouldBlock = false;
                spdlog::debug("[DialogueDB] ShouldSoftBlock: FormID=0x{:08X}, EditorID='{}', SkyrimNet-only block -> ALLOW audio/subtitles", 
                    formID, editorID);
            } else if (!Config::IsFilterCategoryEnabled(filterCategory)) {
                // Check if this entry's filterCategory toggle is enabled
                shouldBlock = false;  // Toggle disabled, don't block
                spdlog::debug("[DialogueDB] ShouldSoftBlock: FormID=0x{:08X}, EditorID='{}', category='{}' toggle DISABLED -> ALLOW", 
                    formID, editorID, filterCategory);
            } else {
                // Toggle enabled - both hard and soft blocks always soft-block (audio + subtitles)
                shouldBlock = true;
                spdlog::debug("[DialogueDB] ShouldSoftBlock: Found {} block for FormID=0x{:08X}, EditorID='{}', category='{}' -> BLOCK audio+subtitles", 
                    blockType == BlockType::Hard ? "HARD" : "soft", formID, editorID, filterCategory);
            }
        } else {
            spdlog::debug("[DialogueDB] ShouldSoftBlock: No entry found for FormID=0x{:08X}, EditorID='{}'", formID, editorID);
        }
        
        sqlite3_finalize(stmt);

        // If topic/quest/scene didn't match, check Actor blacklist
        if (!shouldBlock && actorFormID > 0) {
            const char* actorBlSql = "SELECT block_type, filter_category FROM blacklist WHERE target_type = 6 AND target_formid = ? LIMIT 1;";
            sqlite3_stmt* actorBlStmt = nullptr;
            if (sqlite3_prepare_v2(db_, actorBlSql, -1, &actorBlStmt, nullptr) == SQLITE_OK) {
                sqlite3_bind_int(actorBlStmt, 1, actorFormID);
                if (sqlite3_step(actorBlStmt) == SQLITE_ROW) {
                    BlockType blockType = static_cast<BlockType>(sqlite3_column_int(actorBlStmt, 0));
                    const char* filterCatC = reinterpret_cast<const char*>(sqlite3_column_text(actorBlStmt, 1));
                    std::string filterCat = (filterCatC && filterCatC[0]) ? filterCatC : "Blacklist";
                    if (blockType != BlockType::SkyrimNet && Config::IsFilterCategoryEnabled(filterCat)) {
                        spdlog::debug("[DialogueDB] ShouldSoftBlock: Actor 0x{:08X} is blacklisted -> BLOCK", actorFormID);
                        shouldBlock = true;
                    }
                }
                sqlite3_finalize(actorBlStmt);
            }
        }

        // If still not blocked, check Faction blacklist
        if (!shouldBlock && actorRef) {
            auto actorFactions = GetActorFactionEditorIDs(actorRef);
            for (const auto& factionEditorID : actorFactions) {
                if (shouldBlock) break;
                const char* factionBlSql = "SELECT block_type, filter_category FROM blacklist WHERE target_type = 7 AND target_editorid = ? LIMIT 1;";
                sqlite3_stmt* factionBlStmt = nullptr;
                if (sqlite3_prepare_v2(db_, factionBlSql, -1, &factionBlStmt, nullptr) == SQLITE_OK) {
                    sqlite3_bind_text(factionBlStmt, 1, factionEditorID.c_str(), -1, SQLITE_TRANSIENT);
                    if (sqlite3_step(factionBlStmt) == SQLITE_ROW) {
                        BlockType blockType = static_cast<BlockType>(sqlite3_column_int(factionBlStmt, 0));
                        const char* filterCatC = reinterpret_cast<const char*>(sqlite3_column_text(factionBlStmt, 1));
                        std::string filterCat = (filterCatC && filterCatC[0]) ? filterCatC : "Blacklist";
                        if (blockType != BlockType::SkyrimNet && Config::IsFilterCategoryEnabled(filterCat)) {
                            spdlog::debug("[DialogueDB] ShouldSoftBlock: Actor's faction '{}' is blacklisted -> BLOCK", factionEditorID);
                            shouldBlock = true;
                        }
                    }
                    sqlite3_finalize(factionBlStmt);
                }
            }
        }

        return shouldBlock;
    }

    bool Database::ShouldBlockSkyrimNet(uint32_t formID, const std::string& editorID, uint32_t actorFormID, const std::string& actorName)
    {
        std::lock_guard<std::recursive_mutex> lock(dbMutex_);
        
        if (!db_) return false;
        
        // Prioritize EditorID matching (stable for ESL plugins), fall back to FormID if EditorID is empty
        const char* sql = "SELECT block_skyrimnet, block_type, filter_category, actor_filter_formids, actor_filter_names FROM blacklist WHERE ((target_formid = ? OR target_formid = 0) AND target_editorid = ?) OR (target_editorid = '' AND target_formid = ?) LIMIT 1;";
        sqlite3_stmt* stmt = nullptr;
        
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            return false;
        }
        
        sqlite3_bind_int(stmt, 1, formID);
        sqlite3_bind_text(stmt, 2, editorID.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 3, formID);
        
        bool shouldBlock = false;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            if (sqlite3_column_count(stmt) > 1) {
                BlockType blockType = static_cast<BlockType>(sqlite3_column_int(stmt, 1));
                
                // Get filterCategory (default to "Blacklist" if not set)
                const char* filterCategoryC = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
                std::string filterCategory = (filterCategoryC && filterCategoryC[0]) ? filterCategoryC : "Blacklist";
                
                // Get actor filters
                const char* actorFormIDsJson = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
                const char* actorNamesJson = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
                
                std::string actorFormIDsStr = actorFormIDsJson ? actorFormIDsJson : "[]";
                std::string actorNamesStr = actorNamesJson ? actorNamesJson : "[]";
                
                // Check if actor matches filter (if actor info provided)
                if (actorFormID > 0 && !actorName.empty()) {
                    std::vector<uint32_t> filterFormIDs = ParseActorFormIDsFromJson(actorFormIDsStr);
                    std::vector<std::string> filterNames = ParseActorNamesFromJson(actorNamesStr);
                    
                    if (!ActorMatchesFilter(actorFormID, actorName, filterFormIDs, filterNames)) {
                        // Entry doesn't apply to this actor
                        sqlite3_finalize(stmt);
                        return false;
                    }
                }
                
                // SkyrimNet-only blocks ignore filter_category - only check the flag
                if (blockType == BlockType::SkyrimNet) {
                    shouldBlock = sqlite3_column_int(stmt, 0) != 0;
                } else if (!Config::IsFilterCategoryEnabled(filterCategory)) {
                    // Check if this entry's filterCategory toggle is enabled
                    shouldBlock = false;  // Toggle disabled, don't block
                } else {
                    // Toggle enabled, check block flags
                    if (blockType == BlockType::Hard) {
                        shouldBlock = true;
                    } else {
                        shouldBlock = sqlite3_column_int(stmt, 0) != 0;
                    }
                }
            }
        }
        
        sqlite3_finalize(stmt);
        return shouldBlock;
    }
}
