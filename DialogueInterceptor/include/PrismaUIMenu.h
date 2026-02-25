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
    static void OnClearBlacklist(const char* data);
    static void OnRefreshBlacklist(const char* data);
    static void OnUpdateBlacklistEntry(const char* data);
    static void OnAddToBlacklist(const char* data);
    static void OnAddToWhitelist(const char* data);
    static void OnRemoveFromBlacklist(const char* data);
    static void OnToggleSubtypeFilter(const char* data);
    static void OnDeleteHistoryEntries(const char* data);
    static void OnOpenManualEntry(const char* data);
    static void OnDetectIdentifierType(const char* data);
    static void OnCreateManualEntry(const char* data);
    static void OnRequestWhitelist(const char* data);
    static void OnRemoveFromWhitelist(const char* data);
    static void OnUpdateWhitelistEntry(const char* data);
    static void OnMoveToBlacklist(const char* data);
    static void OnMoveToWhitelist(const char* data);
    static void OnRemoveWhitelistBatch(const char* data);
    static void OnImportScenes(const char* data);
    static void OnImportYAML(const char* data);
    static void OnSetCombatGruntsBlocked(const char* data);
    static void OnSetFollowerCommentaryEnabled(const char* data);
    static void OnRequestSettings(const char* data);
    static void OnSetBlacklistEnabled(const char* data);
    static void OnSetSkyrimNetEnabled(const char* data);
    static void OnSetScenesEnabled(const char* data);
    static void OnSetBardSongsEnabled(const char* data);
    static std::string SerializeHistoryToJSON();
    static std::string SerializeBlacklistToJSON();
    static std::string SerializeWhitelistToJSON();
};
