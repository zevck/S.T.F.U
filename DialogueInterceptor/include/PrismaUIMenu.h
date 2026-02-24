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
    static std::string SerializeHistoryToJSON();
    static std::string SerializeBlacklistToJSON();
};
