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
        
        // Extract fields
        std::string identifier = ExtractJsonValue(jsonStr, "identifier");
        std::string blockTypeStr = ExtractJsonValue(jsonStr, "blockType");
        std::string category = ExtractJsonValue(jsonStr, "category");
        std::string notes = ExtractJsonValue(jsonStr, "notes");
        bool isWhitelist = ExtractJsonValue(jsonStr, "isWhitelist") == "true";
        
        if (identifier.empty()) {
            spdlog::error("[PrismaUIMenu::OnCreateAdvancedEntry] Empty identifier");
            return;
        }
        
        // Actor filters are parallel arrays: FormIDs[i] belongs to Names[i]
        std::vector<std::string> formIDStrings = ExtractJsonStringArray(jsonStr, "actorFilterFormIDs");

        // Create entry
        DialogueDB::BlacklistEntry entry;
        entry.notes = notes;
        entry.actorFilterFormIDs = ParseHexFormIDs(formIDStrings);
        entry.actorFilterNames = ExtractJsonStringArray(jsonStr, "actorFilterNames");
        entry.factionFilterEditorIDs = ExtractJsonStringArray(jsonStr, "factionFilterEditorIDs");

        spdlog::info("[PrismaUIMenu::OnCreateAdvancedEntry] Parsed {} actor names, {} actor FormIDs, {} faction EditorIDs",
                    entry.actorFilterNames.size(), entry.actorFilterFormIDs.size(), entry.factionFilterEditorIDs.size());

        // The arrays must be paired, and every FormID must have parsed
        if (formIDStrings.size() != entry.actorFilterNames.size()) {
            spdlog::error("[PrismaUIMenu::OnCreateAdvancedEntry] PAIRING ERROR: JSON has {} FormIDs but {} names. Arrays must be synchronized pairs!",
                         formIDStrings.size(), entry.actorFilterNames.size());
            return;
        }
        if (entry.actorFilterFormIDs.size() != formIDStrings.size()) {
            spdlog::error("[PrismaUIMenu::OnCreateAdvancedEntry] FORMID PARSING FAILED: Expected {} FormIDs from JSON, but parsed only {}",
                         formIDStrings.size(), entry.actorFilterFormIDs.size());
            spdlog::error("[PrismaUIMenu::OnCreateAdvancedEntry] JSON was: {}", jsonStr);
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
