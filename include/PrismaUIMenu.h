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

#pragma once
#include "../include/PCH.h"
#include "PrismaUI_API.h"
#include "DialogueDatabase.h"

class PrismaUIMenu
{
public:
    static void Initialize();
    static void Toggle();
    static bool IsOpen();
    static void SendHistoryData();
    static void SendBlacklistData();
    static void SendWhitelistData();
    static void SendSettingsData();
    
private:
    static PRISMA_UI_API::IVPrismaUI1* prismaUI_;
    static PrismaView view_;
    static bool initialized_;
    
    static void OnDomReady(PrismaView view);
    static void OnRequestHistory(const char* data);
    static void OnRequestBlacklist(const char* data);
    static void OnCloseMenu(const char* data);
    static void OnLog(const char* data);
    static void OnDeleteBlacklistEntry(const char* data);
    static void OnDeleteBlacklistBatch(const char* data);
    static void OnRefreshBlacklist(const char* data);
    static void OnUpdateBlacklistEntry(const char* data);
    static void OnToggleSubtypeFilter(const char* data);
    static void OnDeleteHistoryEntries(const char* data);
    static void OnDetectIdentifierType(const char* data);
    static void OnCreateAdvancedEntry(const char* data);
    static void OnRequestWhitelist(const char* data);
    static void OnRemoveFromWhitelist(const char* data);
    static void OnUpdateWhitelistEntryAdvanced(const char* data);
    static void OnRemoveWhitelistBatch(const char* data);
    static void OnImportScenes(const char* data);
    static void OnImportYAML(const char* data);
    static void OnSetCombatGruntsBlocked(const char* data);
    static void OnSetFollowerCommentaryEnabled(const char* data);
    static void OnRequestSettings(const char* data);
    static void OnSetBlacklistEnabled(const char* data);
    static void OnSetScenesEnabled(const char* data);
    static void OnSetBardSongsEnabled(const char* data);
    static void OnGetNearbyActors(const char* data);
    static std::string SerializeHistoryToJSON();
    static std::string SerializeBlacklistToJSON();
    static std::string SerializeWhitelistToJSON();
};
