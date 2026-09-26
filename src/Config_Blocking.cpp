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

#include "Config.h"
#include "ConfigInternal.h"
#include "EditorID.h"
#include "DialogueDatabase.h"
#include <spdlog/spdlog.h>
#include <unordered_set>
#include <algorithm>

namespace Config
{
    bool IsFilteredByMCM(uint32_t topicFormID, uint16_t topicSubtype)
    {
        // Check if this subtype has an MCM global and it's enabled
        auto subtypeGlobalIt = g_settings.mcm.subtypeGlobals.find(topicSubtype);
        if (subtypeGlobalIt != g_settings.mcm.subtypeGlobals.end() && subtypeGlobalIt->second) {
            // MCM global exists - 1.0 = block, 0.0 = allow
            if (subtypeGlobalIt->second->value >= 0.5f) {
                return true;  // Filtered by MCM
            }
        }
        return false;
    }
    
    bool HasDisabledSubtypeToggle(uint16_t topicSubtype)
    {
        // Check if this subtype has an MCM global but it's disabled
        auto subtypeGlobalIt = g_settings.mcm.subtypeGlobals.find(topicSubtype);
        if (subtypeGlobalIt != g_settings.mcm.subtypeGlobals.end() && subtypeGlobalIt->second) {
            // MCM global exists - check if it's disabled
            if (subtypeGlobalIt->second->value < 0.5f) {
                return true;  // Has toggle but it's disabled
            }
        }
        return false;
    }
    
    bool ShouldFilterFromHistory(const char* responseText, uint16_t subtype, RE::TESTopic* topic)
    {
        // Filter EnterSprintBreath (95), OutOfBreath (100), and LeaveWaterBreath (102)
        if (subtype == 95 || subtype == 100 || subtype == 102) {
            return true;
        }
        
        // Filter combat grunts by response text
        if (!responseText || responseText[0] == '\0') {
            return false;
        }
        
        std::string text = responseText;
        // Trim whitespace
        text.erase(0, text.find_first_not_of(" \t\n\r"));
        text.erase(text.find_last_not_of(" \t\n\r") + 1);
        
        // Hardcoded grunt list from vanilla Skyrim (from DialogueGeneric and unique NPC voices)
        static const std::unordered_set<std::string> knownGrunts = {
            // From DialogueGeneric
            "Agh!", "Oof!", "Nargh!", "Argh!", "Weergh!", "Yeagh!", "Yearrgh!", "Hyargh!",
            "Rrarggh!", "Grrargh!", "Aggghh!", "Nyyarrggh!", "Hsssss!", "Agh...", "Ugh...",
            "Hunh...", "Gah...", "Nuh...", "Gah!", "Unf!", "Grrh!", "Nnh!",
            "Hhyyaarargghhhh!", "Rrrraaaaarrggghhhh!", "Yyyaaaarrgghh!", "Aaaayyyaarrrrgghh!",
            "Nnyyyaarrgghh!", "Hunh!", "Gah!", "Yah!", "Rargh!",
            
            // From unique NPC voices
            "Aghh..", "Nuhn!", "Aggh!", "Arrggh!", "Wagh!", "Unh!", "Yeeeaarrggh!",
            "Hhyyaarrgghh!", "Nnnnyyaarrgghh!", "Agggh!", "Grrarrgh!", "Naggh!", "Yeeaaaggh!", "Yiiee!", "Nyargh!",
            
            // Additional grunts
            "Grrragh!", "Gaaaah!", "Aaaaah!", "Nuh!", "Yeeeaaahhh!", "Ahhhhh.....",
            "Hhhhuuuuuhhh....", "Aiiiieee!", "Ahhhhhhhh!", "Eyyaargh!", "Grraaagghh!",
            "Raarrgh!", "Huuragh!", "Grrrarrggh!", "Raaarr!", "Raaarrgh!", "Grragh!",
            "Arugh!", "Yaaaargh!", "Ffffeeeargh!", "Rlyehhhgh1", "Heyargh!", "Grrr!",
            "Uf!", "Egh!", "Yergh!"
        };
        
        // Case-insensitive lookup
        for (const auto& grunt : knownGrunts) {
            if (text.size() == grunt.size() && 
                std::equal(text.begin(), text.end(), grunt.begin(),
                [](char a, char b) { return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b)); })) {
                return true;
            }
        }
        
        return false;
    }
    
    // Check if dialogue should be soft-blocked (audio + subtitles together)
    // Checks: Database blacklist + YAML + MCM subtype filters
    bool ShouldSoftBlock(RE::TESQuest* quest, RE::TESTopic* topic, const char* speakerName, const char* responseText, uint32_t speakerFormID, RE::TESObjectREFR* speakerRef)
    {
        if (!topic) return false;
        
        // Get topic identifiers
        uint32_t topicFormID = topic->GetFormID();
        const char* topicEditorID = STFU::GetEditorID(topic);
        std::string editorIDStr = topicEditorID ? topicEditorID : "";
        
        // Check database for blacklist entry
        auto* db = DialogueDB::GetDatabase();
        if (db) {
            if (db->ShouldSoftBlock(topicFormID, editorIDStr, speakerFormID, speakerName ? speakerName : "", speakerRef)) {
                return true;
            }
            // If topic isn't directly blocked, check if it belongs to a blacklisted scene.
            // Traverse the quest's scene list to find which scene owns this topic, then
            // check if that scene is blacklisted.
            if (quest) {
                for (auto* scene : quest->scenes) {
                    if (!scene) continue;
                    bool foundInScene = false;
                    for (auto* action : scene->actions) {
                        if (!action || action->GetType() != RE::BGSSceneAction::Type::kDialogue) continue;
                        auto* da = static_cast<RE::BGSSceneActionDialogue*>(action);
                        if (da && da->topic == topic) {
                            const char* sceneEditorID = STFU::GetEditorID(scene);
                            if (sceneEditorID && db->ShouldSoftBlock(scene->GetFormID(), sceneEditorID)) {
                                return true;
                            }
                            foundInScene = true;
                            break;
                        }
                    }
                    if (foundInScene) break;
                }
            }
        }

        // WHITELIST OVERRIDE: Check if this topic/quest is explicitly whitelisted for this actor or faction.
        // This must run BEFORE the MCM subtype filter so that a whitelist entry always wins,
        // even when the dialogue's subtype is globally silenced by MCM.
        if (db) {
            if (db->IsWhitelisted(DialogueDB::BlacklistTarget::Topic, topicFormID, editorIDStr,
                                  speakerFormID, speakerName ? speakerName : "", speakerRef)) {
                return false;  // Explicitly whitelisted for this actor/faction - don't soft-block
            }
            if (quest) {
                uint32_t questFormID = quest->GetFormID();
                const char* questEditorID = STFU::GetEditorID(quest);
                std::string questEditorIDStr = questEditorID ? questEditorID : "";
                if (db->IsWhitelisted(DialogueDB::BlacklistTarget::Quest, questFormID, questEditorIDStr,
                                      speakerFormID, speakerName ? speakerName : "", speakerRef)) {
                    return false;  // Explicitly whitelisted for this actor/faction - don't soft-block
                }
            }
        }

        // Check MCM subtype filter
        uint16_t subtype = GetAccurateSubtype(topic);
        auto it = g_settings.mcm.subtypeGlobals.find(subtype);
        if (it != g_settings.mcm.subtypeGlobals.end() && it->second) {
            float value = it->second->value;
            if (value > 0.0f) {
                // PreserveGrunts override: if Block Combat Grunts is OFF (value < 0.5) and this
                // response text is a known grunt, allow it through despite the subtype being filtered.
                // This prevents awkward silence in combat when all combat subtypes are blocked.
                if (responseText &&
                    g_settings.mcm.preserveGruntsGlobal &&
                    g_settings.mcm.preserveGruntsGlobal->value < 0.5f &&
                    ShouldFilterFromHistory(responseText, subtype, topic)) {
                    // Grunt with Block Combat Grunts OFF - fall through and allow playback
                } else {
                    return true;  // Subtype is filtered
                }
            }
        }
        
        return false;
    }
}  // namespace Config
