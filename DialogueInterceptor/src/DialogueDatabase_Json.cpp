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
    // Helper functions for JSON array serialization
    std::string EscapeJsonString(const std::string& str) {
        std::string escaped;
        escaped.reserve(str.size());
        for (char c : str) {
            switch (c) {
                case '\"': escaped += "\\\""; break;
                case '\\': escaped += "\\\\"; break;
                case '\b': escaped += "\\b"; break;
                case '\f': escaped += "\\f"; break;
                case '\n': escaped += "\\n"; break;
                case '\r': escaped += "\\r"; break;
                case '\t': escaped += "\\t"; break;
                default: escaped += c; break;
            }
        }
        return escaped;
    }
    
    std::string UnescapeJsonString(const std::string& str) {
        std::string unescaped;
        unescaped.reserve(str.size());
        for (size_t i = 0; i < str.size(); ++i) {
            if (str[i] == '\\' && i + 1 < str.size()) {
                switch (str[i + 1]) {
                    case '"': unescaped += '"'; ++i; break;
                    case '\\': unescaped += '\\'; ++i; break;
                    case 'b': unescaped += '\b'; ++i; break;
                    case 'f': unescaped += '\f'; ++i; break;
                    case 'n': unescaped += '\n'; ++i; break;
                    case 'r': unescaped += '\r'; ++i; break;
                    case 't': unescaped += '\t'; ++i; break;
                    default: unescaped += str[i]; break;
                }
            } else {
                unescaped += str[i];
            }
        }
        return unescaped;
    }
    
    std::string ResponsesToJson(const std::vector<std::string>& responses) {
        if (responses.empty()) return "[]";
        
        std::ostringstream json;
        json << "[";
        for (size_t i = 0; i < responses.size(); ++i) {
            if (i > 0) json << ",";
            json << "\"" << EscapeJsonString(responses[i]) << "\"";
        }
        json << "]";
        return json.str();
    }
    
    // Serialize actor FormIDs to JSON: [0x12345, 0x67890]
    std::string ActorFormIDsToJson(const std::vector<uint32_t>& formIDs) {
        if (formIDs.empty()) return "[]";
        
        std::ostringstream json;
        json << "[";
        for (size_t i = 0; i < formIDs.size(); ++i) {
            if (i > 0) json << ",";
            json << "\"0x" << std::hex << std::setw(8) << std::setfill('0') << formIDs[i] << "\"";
        }
        json << "]";
        return json.str();
    }
    
    // Serialize actor names to JSON: ["Lydia", "Guard"]
    std::string ActorNamesToJson(const std::vector<std::string>& names) {
        return ResponsesToJson(names);  // Reuse string array serializer
    }
    
    // Serialize faction EditorIDs to JSON: ["WhiterunGuardFaction", "_W_HealerFaction"]
    std::string FactionEditorIDsToJson(const std::vector<std::string>& editorIDs) {
        return ResponsesToJson(editorIDs);  // Reuse string array serializer
    }
    
    std::vector<std::string> JsonToResponses(const std::string& json) {
        std::vector<std::string> responses;
        
        if (json.empty() || json == "[]") {
            return responses;
        }
        
        // Simple JSON array parser
        size_t pos = json.find('[');
        if (pos == std::string::npos) {
            return responses;
        }
        
        ++pos; // Skip '['
        bool inString = false;
        std::string current;
        
        while (pos < json.size()) {
            char c = json[pos];
            
            if (!inString) {
                if (c == '"') {
                    inString = true;
                    current.clear();
                } else if (c == ']') {
                    break;
                }
            } else {
                if (c == '\\' && pos + 1 < json.size()) {
                    // Escape sequence
                    current += c;
                    current += json[pos + 1];
                    ++pos;
                } else if (c == '"') {
                    // End of string
                    responses.push_back(UnescapeJsonString(current));
                    inString = false;
                } else {
                    current += c;
                }
            }
            ++pos;
        }
        
        return responses;
    }
    
    // Helper function to check if actor matches filter (ESL-safe)
    // Empty filter vectors = matches all actors
    // Returns true if actor should be filtered (blocked/whitelisted)
    bool ActorMatchesFilter(uint32_t actorFormID, const std::string& actorName,
                            const std::vector<uint32_t>& filterFormIDs, 
                            const std::vector<std::string>& filterNames)
    {
        // Empty actor filter = doesn't match by actor (faction filter might still match)
        if (filterFormIDs.empty() && filterNames.empty()) return false;
        
        // Check if actor is in the filter list
        // ESL-safe: match by name AND last 3 digits of FormID
        for (size_t i = 0; i < filterNames.size() && i < filterFormIDs.size(); ++i) {
            if (actorName == filterNames[i] && 
                (actorFormID & 0xFFF) == (filterFormIDs[i] & 0xFFF)) {
                return true;  // Actor matches filter
            }
        }
        
        return false;  // Actor not in filter
    }
    
    // Parse JSON array of actor FormIDs/Names from database TEXT column
    std::vector<uint32_t> ParseActorFormIDsFromJson(const std::string& json) {
        std::vector<uint32_t> formIDs;
        if (json.empty() || json == "[]") return formIDs;
        
        // Simple JSON array parser: ["0x12345", "0x67890"]
        // We expect hex FormIDs as strings
        size_t pos = 1;  // Skip leading '['
        std::string current;
        bool inString = false;
        
        while (pos < json.size()) {
            char c = json[pos];
            if (!inString) {
                if (c == '"') {
                    inString = true;
                    current.clear();
                } else if (c == ']') {
                    break;
                }
            } else {
                if (c == '"') {
                    // End of string - parse as hex FormID
                    if (!current.empty()) {
                        try {
                            uint32_t formID = std::stoul(current, nullptr, 0);  // auto-detect base (0x prefix)
                            formIDs.push_back(formID);
                        } catch (...) {
                            spdlog::warn("[DialogueDB] Failed to parse actor FormID: {}", current);
                        }
                    }
                    inString = false;
                } else {
                    current += c;
                }
            }
            ++pos;
        }
        
        return formIDs;
    }
    
    std::vector<std::string> ParseActorNamesFromJson(const std::string& json) {
        // Reuse existing JsonToResponses parser - it handles string arrays
        return JsonToResponses(json);
    }
    
    // Parse JSON array of faction EditorIDs from database TEXT column
    std::vector<std::string> ParseFactionEditorIDsFromJson(const std::string& json) {
        // Reuse existing JsonToResponses parser - it handles string arrays
        return JsonToResponses(json);
    }
    
    // Get faction EditorIDs from an actor reference
    std::vector<std::string> GetActorFactionEditorIDs(RE::TESObjectREFR* actorRef) {
        std::vector<std::string> factionEditorIDs;
        
        if (!actorRef) return factionEditorIDs;
        
        // Cast to Actor
        RE::Actor* actor = actorRef->As<RE::Actor>();
        if (!actor) return factionEditorIDs;
        
        // Get actor base
        RE::TESNPC* actorBase = actor->GetActorBase();
        if (!actorBase) return factionEditorIDs;
        
        // Iterate through factions
        for (auto& factionInfo : actorBase->factions) {
            if (factionInfo.faction) {
                const char* editorID = STFU::GetEditorID(factionInfo.faction);
                if (editorID && editorID[0]) {
                    factionEditorIDs.push_back(editorID);
                }
            }
        }
        
        return factionEditorIDs;
    }
    
    // Check if actor's factions match any in the filter
    bool FactionMatchesFilter(RE::TESObjectREFR* actorRef, const std::vector<std::string>& factionFilter) {
        if (factionFilter.empty()) return false;  // No faction filter
        if (!actorRef) return false;
        
        std::vector<std::string> actorFactions = GetActorFactionEditorIDs(actorRef);
        
        // Check if any of the actor's factions are in the filter
        for (const auto& actorFaction : actorFactions) {
            for (const auto& filterFaction : factionFilter) {
                if (actorFaction == filterFaction) {
                    spdlog::debug("[DialogueDB] Faction match found: actor has faction '{}'", actorFaction);
                    return true;
                }
            }
        }
        
        return false;
    }
}
