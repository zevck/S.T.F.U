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
    prismaUI_->RegisterJSListener(view_, "refreshBlacklist", &OnRefreshBlacklist);
    prismaUI_->RegisterJSListener(view_, "toggleSubtypeFilter", &OnToggleSubtypeFilter);
    prismaUI_->RegisterJSListener(view_, "deleteHistoryEntries", &OnDeleteHistoryEntries);
    prismaUI_->RegisterJSListener(view_, "detectIdentifierType", &OnDetectIdentifierType);
    prismaUI_->RegisterJSListener(view_, "createAdvancedEntry", &OnCreateAdvancedEntry);
    prismaUI_->RegisterJSListener(view_, "updateBlacklistEntryAdvanced", &OnUpdateBlacklistEntry);  // Reuse existing handler
    prismaUI_->RegisterJSListener(view_, "requestWhitelist", &OnRequestWhitelist);
    prismaUI_->RegisterJSListener(view_, "removeFromWhitelist", &OnRemoveFromWhitelist);
    prismaUI_->RegisterJSListener(view_, "updateWhitelistEntryAdvanced", &OnUpdateWhitelistEntryAdvanced);
    prismaUI_->RegisterJSListener(view_, "removeWhitelistBatch", &OnRemoveWhitelistBatch);
    prismaUI_->RegisterJSListener(view_, "importScenes", &OnImportScenes);
    prismaUI_->RegisterJSListener(view_, "importYAML", &OnImportYAML);
    prismaUI_->RegisterJSListener(view_, "getNearbyActors", &OnGetNearbyActors);
    prismaUI_->RegisterJSListener(view_, "setCombatGruntsBlocked", &OnSetCombatGruntsBlocked);
    prismaUI_->RegisterJSListener(view_, "setFollowerCommentaryEnabled", &OnSetFollowerCommentaryEnabled);
    prismaUI_->RegisterJSListener(view_, "requestSettings", &OnRequestSettings);
    prismaUI_->RegisterJSListener(view_, "setBlacklistEnabled", &OnSetBlacklistEnabled);
    prismaUI_->RegisterJSListener(view_, "setScenesEnabled", &OnSetScenesEnabled);
    prismaUI_->RegisterJSListener(view_, "setBardSongsEnabled", &OnSetBardSongsEnabled);
    
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
    SendSettingsData();
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
        spdlog::debug("[PrismaUIMenu] Menu closed");
    } else {
        // Show and focus
        prismaUI_->Show(view_);
        prismaUI_->Focus(view_, true, false); // Pause game, don't disable focus menu
        
        // Refresh all tab data when opening so whichever tab is active is current
        SendHistoryData();
        SendBlacklistData();
        SendWhitelistData();
        
        spdlog::debug("[PrismaUIMenu] Menu opened");
    }
}

bool PrismaUIMenu::IsOpen()
{
    if (!initialized_ || !prismaUI_ || !prismaUI_->IsValid(view_)) {
        return false;
    }
    
    return prismaUI_->HasFocus(view_);
}
