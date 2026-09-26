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

void PrismaUIMenu::OnOpenManualEntry(const char* data)
{
    spdlog::info("[PrismaUIMenu::OnOpenManualEntry] Manual entry requested");
    
    try {
        // Parse JSON to check if whitelist or blacklist
        std::string jsonStr(data ? data : "{}");
        bool isWhitelist = (jsonStr.find("\"isWhitelist\":true") != std::string::npos);
        
        spdlog::info("[PrismaUIMenu::OnOpenManualEntry] Opening manual entry for {}", isWhitelist ? "whitelist" : "blacklist");
        
        // Manual entry is handled entirely within the PrismaUI — no legacy modal needed.
        
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
        int detectedType = 0; // 0 = Topic, 1 = Scene, 2 = Plugin, 3 = Actor, 4 = Faction
        std::string detectedName;  // actor name or faction EditorID for display
        std::string lowerIdentifier = identifier;
        std::transform(lowerIdentifier.begin(), lowerIdentifier.end(), lowerIdentifier.begin(), ::tolower);
        
        if (lowerIdentifier.ends_with(".esp") || lowerIdentifier.ends_with(".esm") || lowerIdentifier.ends_with(".esl")) {
            detectedType = 2; // Plugin
        } else {
            // Check if it's a FormID (0x prefix, or bare hex digits like A2C94)
            bool isFormID = (identifier.find("0x") == 0) ||
                            (!identifier.empty() && identifier.find_first_not_of("0123456789abcdefABCDEF") == std::string::npos);
            
            if (isFormID) {
                // Parse FormID and look up the form
                try {
                    uint32_t formID = std::stoul(identifier, nullptr, 16);
                    auto* form = RE::TESForm::LookupByID(formID);
                    
                    if (form) {
                        // Check if it's an actor (reference)
                        auto* actor = form->As<RE::Actor>();
                        if (actor) {
                            detectedType = 3; // Actor
                            const char* name = actor->GetName();
                            detectedName = name ? name : "";
                            spdlog::info("[PrismaUIMenu::OnDetectIdentifierType] FormID detected as Actor: {} ({})", identifier, detectedName);
                        } else {
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
                                    detectedType = 5; // Unknown form type
                                    spdlog::warn("[PrismaUIMenu::OnDetectIdentifierType] FormID is neither Actor/Scene/Topic: {}", identifier);
                                }
                            }
                        }
                    } else {
                        detectedType = 5; // Form not found
                        spdlog::warn("[PrismaUIMenu::OnDetectIdentifierType] FormID not found: {}", identifier);
                    }
                } catch (...) {
                    detectedType = 0; // Default on parse error
                    spdlog::warn("[PrismaUIMenu::OnDetectIdentifierType] Failed to parse FormID: {}", identifier);
                }
            } else {
                // EditorID - try faction first, then scene, then topic
                auto* faction = RE::TESForm::LookupByEditorID<RE::TESFaction>(identifier.c_str());
                if (faction) {
                    detectedType = 4; // Faction
                    detectedName = identifier;
                    spdlog::info("[PrismaUIMenu::OnDetectIdentifierType] EditorID detected as Faction: {}", identifier);
                } else {
                    auto* scene = Config::SafeLookupForm<RE::BGSScene>(identifier.c_str());
                    if (scene) {
                        detectedType = 1; // Scene
                        spdlog::info("[PrismaUIMenu::OnDetectIdentifierType] EditorID detected as Scene: {}", identifier);
                    } else {
                        auto* topic = Config::SafeLookupForm<RE::TESTopic>(identifier.c_str());
                        if (topic) {
                            detectedType = 0; // Topic
                            spdlog::info("[PrismaUIMenu::OnDetectIdentifierType] EditorID detected as Topic: {}", identifier);
                        } else {
                            detectedType = 5; // Unknown/unresolved
                            spdlog::info("[PrismaUIMenu::OnDetectIdentifierType] EditorID unresolved: {}", identifier);
                        }
                    }
                }
            }
        }
        
        // Build categories list (matching STFUMenu::GetFilterCategories)
        std::ostringstream categoriesJson;
        categoriesJson << "[";
        
        if (detectedType == 1) {
            // Scene categories
            categoriesJson << "\"Blacklist\",\"Scene\",\"BardSongs\",\"FollowerCommentary\"";
        } else if (detectedType == 3 || detectedType == 4) {
            // Actor / Faction - just Blacklist
            categoriesJson << "\"Blacklist\"";
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
        std::string typeStr;
        if (detectedType == 1) typeStr = "scene";
        else if (detectedType == 2) typeStr = "plugin";
        else if (detectedType == 3) typeStr = "actor";
        else if (detectedType == 4) typeStr = "faction";
        else if (detectedType == 5) typeStr = "unknown";
        else typeStr = "topic";

        std::string response = "{\"type\":\"" + typeStr + "\",\"displayName\":\"" + detectedName + "\",\"categories\":" + categoriesJson.str() + "}";
        
        spdlog::info("[PrismaUIMenu::OnDetectIdentifierType] Response: {}", response);
        
        // Send response to JavaScript
        std::string script = "window.handleIdentifierDetection(" + response + ");";
        prismaUI_->Invoke(view_, script.c_str());
        
    } catch (const std::exception& e) {
        spdlog::error("[PrismaUIMenu::OnDetectIdentifierType] Exception: {}", e.what());
    }
}

void PrismaUIMenu::OnDetectActorOrFaction(const char* data)
{
    if (!data) {
        spdlog::error("[PrismaUIMenu::OnDetectActorOrFaction] Null data received");
        return;
    }
    
    try {
        std::string jsonStr(data);
        spdlog::info("[PrismaUIMenu::OnDetectActorOrFaction] Received: {}", jsonStr);
        
        // Extract input from JSON
        size_t inputPos = jsonStr.find("\"input\":\"");
        if (inputPos == std::string::npos) {
            spdlog::error("[PrismaUIMenu::OnDetectActorOrFaction] No input found");
            return;
        }
        
        size_t inputStart = inputPos + 9;
        size_t inputEnd = jsonStr.find("\"", inputStart);
        std::string input = jsonStr.substr(inputStart, inputEnd - inputStart);
        
        if (input.empty()) {
            // Empty input - send back unknown
            std::string response = R"({"type":"unknown","name":"","formID":"","editorID":""})";
            std::string script = "window.handleActorOrFactionDetection(" + response + ");";
            prismaUI_->Invoke(view_, script.c_str());
            return;
        }
        
        std::string detectedType = "unknown";
        std::string name = "";
        std::string formID = "";
        std::string editorID = "";
        
        // Check if it's a FormID (starts with 0x)
        if (input.find("0x") == 0 || input.find("0X") == 0) {
            try {
                uint32_t actorFormID = std::stoul(input, nullptr, 16);
                auto* form = RE::TESForm::LookupByID(actorFormID);
                
                if (form) {
                    auto* actor = form->As<RE::TESNPC>();
                    if (actor) {
                        detectedType = "actor";
                        name = actor->GetFullName();
                        formID = input;
                        spdlog::info("[PrismaUIMenu::OnDetectActorOrFaction] Detected as Actor: {} ({})", name, formID);
                    }
                }
            } catch (...) {
                spdlog::warn("[PrismaUIMenu::OnDetectActorOrFaction] Failed to parse FormID: {}", input);
            }
        } else {
            // Try to look up as faction EditorID
            auto* form = RE::TESForm::LookupByEditorID(input);
            if (form) {
                auto* faction = form->As<RE::TESFaction>();
                if (faction) {
                    detectedType = "faction";
                    editorID = input;
                    name = faction->GetFullName();
                    if (name.empty()) {
                        name = editorID; // Use EditorID as fallback
                    }
                    spdlog::info("[PrismaUIMenu::OnDetectActorOrFaction] Detected as Faction: {} ({})", name, editorID);
                }
            }
        }
        
        // Build response JSON
        std::ostringstream responseJson;
        responseJson << "{\"type\":\"" << detectedType << "\",\"name\":\"" << name << "\",\"formID\":\"" << formID << "\",\"editorID\":\"" << editorID << "\"}";
        
        spdlog::info("[PrismaUIMenu::OnDetectActorOrFaction] Response: {}", responseJson.str());
        
        // Send response to JavaScript
        std::string script = "window.handleActorOrFactionDetection(" + responseJson.str() + ");";
        prismaUI_->Invoke(view_, script.c_str());
        
    } catch (const std::exception& e) {
        spdlog::error("[PrismaUIMenu::OnDetectActorOrFaction] Exception: {}", e.what());
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
        
        // Helper to extract JSON boolean value
        auto extractBool = [](const std::string& json, const std::string& key) -> bool {
            std::string searchKey = "\"" + key + "\":";
            size_t keyPos = json.find(searchKey);
            if (keyPos == std::string::npos) return false;
            size_t valueStart = keyPos + searchKey.length();
            // Skip whitespace
            while (valueStart < json.length() && std::isspace(json[valueStart])) valueStart++;
            if (valueStart >= json.length()) return false;
            // Check for true/false
            if (json.substr(valueStart, 4) == "true") return true;
            if (json.substr(valueStart, 5) == "false") return false;
            return false;
        };
        
        // Extract fields
        std::string identifier = extractString(jsonStr, "identifier");
        std::string blockTypeStr = extractString(jsonStr, "blockType");
        std::string category = extractString(jsonStr, "category");
        std::string notes = extractString(jsonStr, "notes");
        bool isWhitelist = extractBool(jsonStr, "isWhitelist");
        
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
                    // Check if it's an actor reference first
                    auto* actor = form->As<RE::Actor>();
                    if (actor) {
                        entry.targetType = DialogueDB::BlacklistTarget::Actor;
                        entry.targetFormID = actor->GetFormID();
                        const char* name = actor->GetName();
                        entry.targetEditorID = name ? name : "";  // Store display name (not EditorID)
                        spdlog::info("[PrismaUIMenu::OnCreateManualEntry] FormID is Actor: {} (Name: {})", identifier, entry.targetEditorID);
                    } else {
                    // Get EditorID from form (for non-actor types)
                    const char* editorID = STFU::GetEditorID(form);
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
                                const char* questEdID = STFU::GetEditorID(quest);
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
                    }  // end non-actor branch
                    }  // end actor else
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
                // Try faction first (EditorID lookup)
                auto* faction = RE::TESForm::LookupByEditorID<RE::TESFaction>(identifier.c_str());
                if (faction) {
                    entry.targetType = DialogueDB::BlacklistTarget::Faction;
                    entry.targetFormID = 0;
                    entry.targetEditorID = identifier;
                    spdlog::info("[PrismaUIMenu::OnCreateManualEntry] EditorID is Faction: {}", identifier);
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
                            const char* questEdID = STFU::GetEditorID(quest);
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
                }  // end faction else
            }
        }
        
        // Set block type and granular flags
        // Actor and Faction targets only support soft blocking (hard block would be game-breaking)
        bool isActorOrFaction = (entry.targetType == DialogueDB::BlacklistTarget::Actor ||
                                  entry.targetType == DialogueDB::BlacklistTarget::Faction);
        DialogueDB::BlockType blockType;
        if (!isActorOrFaction && blockTypeStr == "Hard") {
            blockType = DialogueDB::BlockType::Hard;
        } else {
            blockType = DialogueDB::BlockType::Soft;
        }
        
        entry.blockType = blockType;
        
        if (isWhitelist) {
            // Whitelist entries don't block anything
            entry.blockAudio = false;
            entry.blockSubtitles = false;
            entry.blockSkyrimNet = false;
            entry.filterCategory = "Whitelist";
        } else {
            // Blacklist entries block everything
            entry.blockAudio = true;
            entry.blockSubtitles = true;
            entry.blockSkyrimNet = true;
            entry.filterCategory = !category.empty() ? category : "Blacklist";
        }
        
        // Set timestamp
        entry.addedTimestamp = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
        
        // Add to database
        bool success = false;
        if (isWhitelist) {
            success = db->AddToWhitelist(entry);
        } else {
            success = db->AddToBlacklist(entry);
        }
        
        if (success) {
            spdlog::info("[PrismaUIMenu::OnCreateManualEntry] Added manual {} entry: {} (Type: {}, Category: {}, Plugin: {})",
                isWhitelist ? "whitelist" : "blacklist",
                identifier, 
                entry.targetType == DialogueDB::BlacklistTarget::Scene ? "Scene" : "Topic",
                entry.filterCategory,
                entry.sourcePlugin);
            
            // Refresh UI
            if (isWhitelist) {
                SendWhitelistData();
            } else {
                SendBlacklistData();
            }
            SendHistoryData();
        } else {
            spdlog::error("[PrismaUIMenu::OnCreateManualEntry] Failed to add entry: {}", identifier);
        }
        
    } catch (const std::exception& e) {
        spdlog::error("[PrismaUIMenu::OnCreateManualEntry] Exception: {}", e.what());
    }
}

void PrismaUIMenu::OnCreateAdvancedEntry(const char* data)
{
    if (!data) {
        spdlog::error("[PrismaUIMenu::OnCreateAdvancedEntry] Null data received");
        return;
    }
    
    try {
        std::string jsonStr(data);
        spdlog::info("[PrismaUIMenu::OnCreateAdvancedEntry] Received: {}", jsonStr);
        
        auto* db = DialogueDB::GetDatabase();
        if (!db) {
            spdlog::error("[PrismaUIMenu::OnCreateAdvancedEntry] Database not available");
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
        
        // Helper to extract JSON boolean value
        auto extractBool = [](const std::string& json, const std::string& key) -> bool {
            std::string searchKey = "\"" + key + "\":";
            size_t keyPos = json.find(searchKey);
            if (keyPos == std::string::npos) return false;
            size_t valueStart = keyPos + searchKey.length();
            while (valueStart < json.length() && std::isspace(json[valueStart])) valueStart++;
            if (valueStart >= json.length()) return false;
            if (json.substr(valueStart, 4) == "true") return true;
            if (json.substr(valueStart, 5) == "false") return false;
            return false;
        };
        
        // Parse actor filters - handles both "0x01A6A4" and "01A6A4" formats
        auto parseActorFormIDs = [](const std::string& json) -> std::vector<uint32_t> {
            std::vector<uint32_t> formIDs;
            size_t arrayStart = json.find("\"actorFilterFormIDs\":[]");
            if (arrayStart != std::string::npos) return formIDs; // Empty array, return immediately
            
            arrayStart = json.find("\"actorFilterFormIDs\":[");
            if (arrayStart == std::string::npos) return formIDs;
            
            // Find opening [ bracket
            size_t pos = json.find("[", arrayStart);
            if (pos == std::string::npos) return formIDs;
            pos++; // Move past [
            
            // Parse each quoted string in the array
            while (pos < json.length()) {
                // Skip whitespace
                while (pos < json.length() && (json[pos] == ' ' || json[pos] == ',')) pos++;
                
                // Check for end of array
                if (pos >= json.length() || json[pos] == ']') break;
                
                // Expect opening quote
                if (json[pos] != '\"') break;
                pos++; // Skip opening quote
                
                // Find closing quote
                size_t hexEnd = json.find("\"", pos);
                if (hexEnd == std::string::npos) break;
                
                // Extract hex string (with or without 0x prefix)
                std::string hexStr = json.substr(pos, hexEnd - pos);
                
                // Remove 0x or 0X prefix if present
                if (hexStr.length() >= 2 && hexStr[0] == '0' && (hexStr[1] == 'x' || hexStr[1] == 'X')) {
                    hexStr = hexStr.substr(2);
                }
                
                // Parse as hex
                try {
                    if (!hexStr.empty()) {
                        uint32_t formID = std::stoul(hexStr, nullptr, 16);
                        formIDs.push_back(formID);
                    }
                } catch (...) {
                    spdlog::warn("[PrismaUIMenu::parseActorFormIDs] Failed to parse hex: {}", hexStr);
                }
                
                // Move past closing quote
                pos = hexEnd + 1;
            }
            return formIDs;
        };
        
        auto parseActorNames = [](const std::string& json) -> std::vector<std::string> {
            std::vector<std::string> names;
            size_t arrayStart = json.find("\"actorFilterNames\":[]");
            if (arrayStart != std::string::npos) return names; // Empty array, return immediately
            
            arrayStart = json.find("\"actorFilterNames\":[");
            if (arrayStart == std::string::npos) return names;
            
            // Find the opening bracket
            size_t bracketPos = json.find("[", arrayStart);
            if (bracketPos == std::string::npos) return names;
            
            size_t pos = json.find("\"", bracketPos);
            while (pos != std::string::npos && pos < json.size()) {
                if (json[pos] != '\"') break;
                pos++;
                size_t nameEnd = json.find("\"", pos);
                if (nameEnd == std::string::npos) break;
                names.push_back(json.substr(pos, nameEnd - pos));
                pos = json.find("\",\"", nameEnd);
                if (pos == std::string::npos) break;
                pos += 3;
            }
            return names;
        };
        
        auto parseFactionEditorIDs = [](const std::string& json) -> std::vector<std::string> {
            std::vector<std::string> editorIDs;
            size_t arrayStart = json.find("\"factionFilterEditorIDs\":[]");
            if (arrayStart != std::string::npos) return editorIDs; // Empty array, return immediately
            
            arrayStart = json.find("\"factionFilterEditorIDs\":[");
            if (arrayStart == std::string::npos) return editorIDs;
            
            // Find the opening bracket
            size_t bracketPos = json.find("[", arrayStart);
            if (bracketPos == std::string::npos) return editorIDs;
            
            size_t pos = json.find("\"", bracketPos);
            while (pos != std::string::npos && pos < json.size()) {
                if (json[pos] != '\"') break;
                pos++;
                size_t idEnd = json.find("\"", pos);
                if (idEnd == std::string::npos) break;
                editorIDs.push_back(json.substr(pos, idEnd - pos));
                pos = json.find("\",\"", idEnd);
                if (pos == std::string::npos) break;
                pos += 3;
            }
            return editorIDs;
        };
        
        // Extract fields
        std::string identifier = extractString(jsonStr, "identifier");
        std::string blockTypeStr = extractString(jsonStr, "blockType");
        std::string category = extractString(jsonStr, "category");
        std::string notes = extractString(jsonStr, "notes");
        bool isWhitelist = extractBool(jsonStr, "isWhitelist");
        
        if (identifier.empty()) {
            spdlog::error("[PrismaUIMenu::OnCreateAdvancedEntry] Empty identifier");
            return;
        }
        
        // Helper to count JSON array elements
        auto countJsonArrayElements = [](const std::string& json, const std::string& key) -> size_t {
            size_t arrayStart = json.find("\"" + key + "\":[");
            if (arrayStart == std::string::npos) return 0;
            
            size_t pos = json.find("[", arrayStart);
            if (pos == std::string::npos) return 0;
            pos++;
            
            size_t count = 0;
            bool inQuote = false;
            int depth = 0;
            
            while (pos < json.length()) {
                char c = json[pos];
                if (c == '\"' && (pos == 0 || json[pos-1] != '\\')) {
                    inQuote = !inQuote;
                    if (inQuote) count++; // Opening quote = new element
                } else if (!inQuote) {
                    if (c == '[') depth++;
                    else if (c == ']') {
                        if (depth == 0) break; // End of our array
                        depth--;
                    }
                }
                pos++;
            }
            return count;
        };
        
        // Count expected elements before parsing
        size_t expectedFormIDCount = countJsonArrayElements(jsonStr, "actorFilterFormIDs");
        size_t expectedNameCount = countJsonArrayElements(jsonStr, "actorFilterNames");
        
        // CRITICAL VALIDATION #1: Arrays must be paired (same count in JSON)
        if (expectedFormIDCount != expectedNameCount) {
            spdlog::error("[PrismaUIMenu::OnCreateAdvancedEntry] PAIRING ERROR: JSON has {} FormIDs but {} names. Arrays must be synchronized pairs!",
                         expectedFormIDCount, expectedNameCount);
            return;
        }
        
        // Create entry
        DialogueDB::BlacklistEntry entry;
        entry.notes = notes;
        entry.actorFilterFormIDs = parseActorFormIDs(jsonStr);
        entry.actorFilterNames = parseActorNames(jsonStr);
        entry.factionFilterEditorIDs = parseFactionEditorIDs(jsonStr);
        
        spdlog::info("[PrismaUIMenu::OnCreateAdvancedEntry] Parsed {} actor names, {} actor FormIDs, {} faction EditorIDs",
                    entry.actorFilterNames.size(), entry.actorFilterFormIDs.size(), entry.factionFilterEditorIDs.size());
        
        // CRITICAL VALIDATION #2: Verify FormID parsing matched expected count
        if (entry.actorFilterFormIDs.size() != expectedFormIDCount) {
            spdlog::error("[PrismaUIMenu::OnCreateAdvancedEntry] FORMID PARSING FAILED: Expected {} FormIDs from JSON, but parsed only {}",
                         expectedFormIDCount, entry.actorFilterFormIDs.size());
            spdlog::error("[PrismaUIMenu::OnCreateAdvancedEntry] JSON was: {}", jsonStr);
            return;
        }
        
        // CRITICAL VALIDATION #3: Verify name parsing matched expected count
        if (entry.actorFilterNames.size() != expectedNameCount) {
            spdlog::error("[PrismaUIMenu::OnCreateAdvancedEntry] NAME PARSING FAILED: Expected {} actor names from JSON, but parsed only {}",
                         expectedNameCount, entry.actorFilterNames.size());
            spdlog::error("[PrismaUIMenu::OnCreateAdvancedEntry] JSON was: {}", jsonStr);
            return;
        }
        
        // CRITICAL VALIDATION #4: Final pairing check - both arrays must have identical size
        if (entry.actorFilterFormIDs.size() != entry.actorFilterNames.size()) {
            spdlog::error("[PrismaUIMenu::OnCreateAdvancedEntry] PAIRING DESYNC: Parsed {} FormIDs but {} names. Pairs are broken!",
                         entry.actorFilterFormIDs.size(), entry.actorFilterNames.size());
            spdlog::error("[PrismaUIMenu::OnCreateAdvancedEntry] This should never happen if validations #2 and #3 passed!");
            return;
        }
        
        // Log successful pairing
        if (!entry.actorFilterFormIDs.empty()) {
            spdlog::info("[PrismaUIMenu::OnCreateAdvancedEntry] Validated {} actor filter pairs:", entry.actorFilterFormIDs.size());
            for (size_t i = 0; i < entry.actorFilterFormIDs.size(); i++) {
                spdlog::info("[PrismaUIMenu::OnCreateAdvancedEntry]   Pair {}: FormID=0x{:08X}, Name='{}'", 
                            i, entry.actorFilterFormIDs[i], entry.actorFilterNames[i]);
            }
        }
        
        // Parse identifier using Config helper
        auto [parsedFormID, parsedEditorID] = Config::ParseFormIdentifier(identifier);
        bool isFormIDInput = (parsedFormID != 0);
        
        if (isFormIDInput) {
            // FormID was entered - parse and look up the form
            try {
                entry.targetFormID = parsedFormID;
                
                auto* form = RE::TESForm::LookupByID(parsedFormID);
                if (form) {
                    const char* editorID = STFU::GetEditorID(form);
                    entry.targetEditorID = editorID ? editorID : "";
                    
                    auto* actor = form->As<RE::Actor>();
                    if (actor) {
                        entry.targetType = DialogueDB::BlacklistTarget::Actor;
                        const char* name = actor->GetName();
                        entry.targetEditorID = name ? name : "";
                        spdlog::info("[PrismaUIMenu::OnCreateAdvancedEntry] FormID is Actor: {} (0x{:08X})",
                            entry.targetEditorID, parsedFormID);
                    } else {
                    auto* scene = form->As<RE::BGSScene>();
                    if (scene) {
                        entry.targetType = DialogueDB::BlacklistTarget::Scene;
                        entry.subtype = 14;
                        entry.subtypeName = "Scene";
                        auto* file = scene->GetFile(0);
                        entry.sourcePlugin = file ? file->GetFilename() : "";
                        
                        auto responses = TopicResponseExtractor::ExtractAllResponsesForScene(entry.targetEditorID);
                        if (!responses.empty()) {
                            std::ostringstream responsesJson;
                            responsesJson << "[";
                            for (size_t i = 0; i < responses.size(); ++i) {
                                if (i > 0) responsesJson << ",";
                                std::string escaped = responses[i];
                                size_t p = 0;
                                while ((p = escaped.find("\\", p)) != std::string::npos) { escaped.replace(p, 1, "\\\\"); p += 2; }
                                p = 0;
                                while ((p = escaped.find("\"", p)) != std::string::npos) { escaped.replace(p, 1, "\\\""); p += 2; }
                                p = 0;
                                while ((p = escaped.find("\n", p)) != std::string::npos) { escaped.replace(p, 1, "\\n"); p += 2; }
                                responsesJson << "\"" << escaped << "\"";
                            }
                            responsesJson << "]";
                            entry.responseText = responsesJson.str();
                        }
                    } else {
                        auto* topic = form->As<RE::TESTopic>();
                        if (topic) {
                            entry.targetType = DialogueDB::BlacklistTarget::Topic;
                            RE::TESQuest* quest = topic->ownerQuest;
                            if (quest) {
                                const char* questEdID = STFU::GetEditorID(quest);
                                entry.questEditorID = questEdID ? questEdID : "";
                            }
                            uint16_t subtype = Config::GetAccurateSubtype(topic);
                            entry.subtype = subtype;
                            entry.subtypeName = Config::GetSubtypeName(subtype);
                            auto* file = topic->GetFile(0);
                            entry.sourcePlugin = file ? file->GetFilename() : "";
                            
                            auto responses = TopicResponseExtractor::ExtractAllResponsesForTopic(entry.targetFormID);
                            if (!responses.empty()) {
                                std::ostringstream responsesJson;
                                responsesJson << "[";
                                for (size_t i = 0; i < responses.size(); ++i) {
                                    if (i > 0) responsesJson << ",";
                                    std::string escaped = responses[i];
                                    size_t p = 0;
                                    while ((p = escaped.find("\\", p)) != std::string::npos) { escaped.replace(p, 1, "\\\\"); p += 2; }
                                    p = 0;
                                    while ((p = escaped.find("\"", p)) != std::string::npos) { escaped.replace(p, 1, "\\\""); p += 2; }
                                    p = 0;
                                    while ((p = escaped.find("\n", p)) != std::string::npos) { escaped.replace(p, 1, "\\n"); p += 2; }
                                    responsesJson << "\"" << escaped << "\"";
                                }
                                responsesJson << "]";
                                entry.responseText = responsesJson.str();
                            }
                        }
                    }
                    }  // end actor else
                }
            } catch (...) {
                spdlog::error("[PrismaUIMenu::OnCreateAdvancedEntry] Failed to parse FormID: {}", identifier);
                return;
            }
        } else if (!parsedEditorID.empty()) {
            // EditorID was entered
            entry.targetEditorID = parsedEditorID;
            
            // Check plugin blocking
            if (parsedEditorID.ends_with(".esp") || parsedEditorID.ends_with(".esm") || parsedEditorID.ends_with(".esl")) {
                entry.targetType = DialogueDB::BlacklistTarget::Plugin;
                entry.sourcePlugin = parsedEditorID;
            } else {
                // Try faction first (EditorID lookup)
                auto* faction = RE::TESForm::LookupByEditorID<RE::TESFaction>(parsedEditorID.c_str());
                if (faction) {
                    entry.targetType = DialogueDB::BlacklistTarget::Faction;
                    entry.targetFormID = faction->GetFormID();  // Store FormID for reliable matching
                    entry.targetEditorID = parsedEditorID;
                    spdlog::info("[PrismaUIMenu::OnCreateAdvancedEntry] EditorID is Faction: {}", parsedEditorID);
                } else {
                // Try to find scene first
                auto* handler = RE::TESDataHandler::GetSingleton();
                if (handler) {
                    bool found = false;
                    for (auto* scene : handler->GetFormArray<RE::BGSScene>()) {
                        if (scene) {
                            const char* edID = STFU::GetEditorID(scene);
                            if (edID && parsedEditorID == edID) {
                                entry.targetType = DialogueDB::BlacklistTarget::Scene;
                                entry.targetFormID = scene->GetFormID();
                                entry.subtype = 14;
                                entry.subtypeName = "Scene";
                                auto* file = scene->GetFile(0);
                                entry.sourcePlugin = file ? file->GetFilename() : "";
                                found = true;
                                break;
                            }
                        }
                    }
                    
                    if (!found) {
                        // Try topic
                        for (auto* topic : handler->GetFormArray<RE::TESTopic>()) {
                            if (topic) {
                                const char* edID = STFU::GetEditorID(topic);
                                if (edID && parsedEditorID == edID) {
                                    entry.targetType = DialogueDB::BlacklistTarget::Topic;
                                    entry.targetFormID = topic->GetFormID();
                                    RE::TESQuest* quest = topic->ownerQuest;
                                    if (quest) {
                                        const char* questEdID = STFU::GetEditorID(quest);
                                        entry.questEditorID = questEdID ? questEdID : "";
                                    }
                                    uint16_t subtype = Config::GetAccurateSubtype(topic);
                                    entry.subtype = subtype;
                                    entry.subtypeName = Config::GetSubtypeName(subtype);
                                    auto* file = topic->GetFile(0);
                                    entry.sourcePlugin = file ? file->GetFilename() : "";
                                    found = true;
                                    break;
                                }
                            }
                        }
                    }
                }
                }  // end faction else
            }
        }
        
        // Set block type
        // Actor and Faction targets only support soft blocking (hard block would be game-breaking)
        bool isActorOrFaction = (entry.targetType == DialogueDB::BlacklistTarget::Actor ||
                                  entry.targetType == DialogueDB::BlacklistTarget::Faction);
        if (!isActorOrFaction && blockTypeStr == "Soft") {
            entry.blockType = DialogueDB::BlockType::Soft;
            entry.blockAudio = true;
            entry.blockSubtitles = true;
            entry.blockSkyrimNet = true;
            entry.filterCategory = !category.empty() ? category : "Blacklist";
        } else if (!isActorOrFaction && blockTypeStr == "Hard") {
            entry.blockType = DialogueDB::BlockType::Hard;
            entry.blockAudio = true;
            entry.blockSubtitles = true;
            entry.blockSkyrimNet = true;
            entry.filterCategory = !category.empty() ? category : "Blacklist";
        } else if (blockTypeStr == "SkyrimNet") {
            entry.blockType = DialogueDB::BlockType::SkyrimNet;
            entry.blockAudio = false;
            entry.blockSubtitles = false;
            entry.blockSkyrimNet = true;
            entry.filterCategory = !category.empty() ? category : "Blacklist";
        } else {
            // Actor/Faction (or default fallback): always Soft
            entry.blockType = DialogueDB::BlockType::Soft;
            entry.blockAudio = true;
            entry.blockSubtitles = true;
            entry.blockSkyrimNet = true;
            entry.filterCategory = !category.empty() ? category : "Blacklist";
        }
        
        // Set timestamp
        entry.addedTimestamp = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
        
        // Add to database
        bool success = false;
        if (isWhitelist) {
            success = db->AddToWhitelist(entry);
        } else {
            success = db->AddToBlacklist(entry);
        }
        
        if (success) {
            spdlog::info("[PrismaUIMenu::OnCreateAdvancedEntry] Added advanced {} entry: {} with {} actor filters",
                isWhitelist ? "whitelist" : "blacklist",
                identifier,
                entry.actorFilterNames.size());
            
            // Refresh UI
            if (isWhitelist) {
                SendWhitelistData();
            } else {
                SendBlacklistData();
            }
            SendHistoryData();
        } else {
            spdlog::error("[PrismaUIMenu::OnCreateAdvancedEntry] Failed to add entry: {}", identifier);
        }
        
    } catch (const std::exception& e) {
        spdlog::error("[PrismaUIMenu::OnCreateAdvancedEntry] Exception: {}", e.what());
    }
}

void PrismaUIMenu::OnGetNearbyActors(const char* data)
{
    try {
        spdlog::info("[PrismaUIMenu::OnGetNearbyActors] Getting nearby actors");
        
        auto player = RE::PlayerCharacter::GetSingleton();
        if (!player) {
            spdlog::error("[PrismaUIMenu::OnGetNearbyActors] Player not found");
            return;
        }
        
        auto processLists = RE::ProcessLists::GetSingleton();
        if (!processLists) {
            spdlog::error("[PrismaUIMenu::OnGetNearbyActors] ProcessLists not found");
            return;
        }
        
        std::ostringstream json;
        json << "{\"actors\":[";
        
        bool first = true;
        const float maxDistance = 4096.0f; // ~68 units (about 4-5 cell widths)
        
        // Iterate through high process actors (loaded and active)
        for (auto& actorHandle : processLists->highActorHandles) {
            auto actorPtr = actorHandle.get();
            if (!actorPtr) continue;
            
            auto actor = actorPtr.get();
            if (!actor || actor == player) continue;
            
            // Get distance
            float distance = player->GetPosition().GetDistance(actor->GetPosition());
            if (distance > maxDistance) continue;
            
            // Get base form
            auto base = actor->GetActorBase();
            if (!base) continue;
            
            // Get name
            const char* name = actor->GetDisplayFullName();
            if (!name || strlen(name) == 0) {
                name = base->GetFullName();
                if (!name || strlen(name) == 0) {
                    continue; // Skip unnamed actors
                }
            }
            
            // Get FormID
            RE::FormID formID = actor->GetFormID();
            
            if (!first) json << ",";
            first = false;
            
            json << "{";
            json << "\"name\":\"";
            // Escape quotes in name
            for (const char* p = name; *p; ++p) {
                if (*p == '\"') json << "\\\"";
                else if (*p == '\\') json << "\\\\";
                else json << *p;
            }
            json << "\",";
            
            // Format FormID as "0x" + fixed 8 hex chars, consistent with every
            // other FormID emitted to the UI (speakerFormID, actorFilterFormIDs,
            // topicFormID). The old strip-leading-zeros form produced variable
            // width for no benefit — JS parses either, but uniform width keeps
            // string-equality comparisons on the UI side reliable.
            char formIDHex[16];
            sprintf_s(formIDHex, "0x%08X", formID);
            json << "\"formID\":\"" << formIDHex << "\",";
            json << "\"distance\":" << static_cast<int>(distance);
            json << "}";
        }
        
        json << "]}";
        
        std::string jsonData = json.str();
        spdlog::info("[PrismaUIMenu::OnGetNearbyActors] Response: {}", jsonData);
        
        // Send to frontend
        std::string script = "window.handleNearbyActors(" + jsonData + ");";
        prismaUI_->Invoke(view_, script.c_str());
        
    } catch (const std::exception& e) {
        spdlog::error("[PrismaUIMenu::OnGetNearbyActors] Exception: {}", e.what());
    }
}
