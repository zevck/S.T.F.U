// Settings persistence via INI file
// Saves/loads runtime toggle values so they persist across all save files

#include "SettingsPersistence.h"
#include "Config.h"
#include "DialogueDatabase.h"
#include <spdlog/spdlog.h>
#include <filesystem>
#include <Windows.h>

namespace SettingsPersistence
{
    // Default values for first-time initialization (all disabled)
    constexpr int DEFAULT_BLACKLIST_ENABLED = 0;       // Disabled
    constexpr int DEFAULT_SKYRIMNET_FILTER_ENABLED = 0; // Disabled
    constexpr int DEFAULT_BLOCK_SCENES = 0;            // Disabled
    constexpr int DEFAULT_BLOCK_BARD_SONGS = 0;        // Disabled
    constexpr int DEFAULT_BLOCK_FOLLOWER_COMMENTARY = 0; // Disabled
    constexpr int DEFAULT_PRESERVE_GRUNTS = 0;         // Disabled (filter grunts)
    constexpr int DEFAULT_SUBTYPE_ENABLED = 0;         // Disabled
    constexpr uint32_t DEFAULT_MENU_HOTKEY = 0xD2;     // Insert key
    
    static std::wstring GetIniPath()
    {
        wchar_t buffer[MAX_PATH];
        GetModuleFileNameW(nullptr, buffer, MAX_PATH);
        std::filesystem::path exePath(buffer);
        auto iniPath = exePath.parent_path() / L"Data" / L"SKSE" / L"Plugins" / L"STFU" / L"config" / L"STFU_Config.ini";
        return iniPath.wstring();
    }
    
    void SaveSettings()
    {
        const auto& settings = Config::GetSettings();
        std::wstring iniPath = GetIniPath();
        
        // Ensure directory exists before writing
        std::filesystem::create_directories(std::filesystem::path(iniPath).parent_path());
        
        // Save master toggles
        if (settings.blacklist.toggleGlobal) {
            WritePrivateProfileStringW(L"Settings", L"BlacklistEnabled", 
                settings.blacklist.toggleGlobal->value > 0.5f ? L"1" : L"0", iniPath.c_str());
        }
        if (settings.skyrimNetFilter.toggleGlobal) {
            WritePrivateProfileStringW(L"Settings", L"SkyrimNetFilterEnabled", 
                settings.skyrimNetFilter.toggleGlobal->value > 0.5f ? L"1" : L"0", iniPath.c_str());
        }
        if (settings.mcm.blockScenesGlobal) {
            WritePrivateProfileStringW(L"Settings", L"BlockScenes", 
                settings.mcm.blockScenesGlobal->value > 0.5f ? L"1" : L"0", iniPath.c_str());
        }
        if (settings.mcm.blockBardSongsGlobal) {
            WritePrivateProfileStringW(L"Settings", L"BlockBardSongs", 
                settings.mcm.blockBardSongsGlobal->value > 0.5f ? L"1" : L"0", iniPath.c_str());
        }
        if (settings.mcm.blockFollowerCommentaryGlobal) {
            WritePrivateProfileStringW(L"Settings", L"BlockFollowerCommentary", 
                settings.mcm.blockFollowerCommentaryGlobal->value > 0.5f ? L"1" : L"0", iniPath.c_str());
        }
        if (settings.mcm.preserveGruntsGlobal) {
            WritePrivateProfileStringW(L"Settings", L"PreserveGrunts", 
                settings.mcm.preserveGruntsGlobal->value > 0.5f ? L"1" : L"0", iniPath.c_str());
        }
        
        // Save menu hotkey
        wchar_t hotkeyStr[32];
        swprintf_s(hotkeyStr, L"0x%X", settings.menuHotkey);
        WritePrivateProfileStringW(L"Settings", L"MenuHotkey", hotkeyStr, iniPath.c_str());
        
        // Save subtype toggles - ONLY for subtypes that have loaded globals
        // This prevents writing unused subtypes to the INI
        for (const auto& [subtypeId, global] : settings.mcm.subtypeGlobals) {
            if (!global) continue;
            
            // Get the name for this subtype ID
            std::string name = Config::GetSubtypeName(subtypeId);
            std::wstring wideName(name.begin(), name.end());
            
            WritePrivateProfileStringW(L"Subtypes", wideName.c_str(), 
                global->value > 0.5f ? L"1" : L"0", iniPath.c_str());
        }
        
        spdlog::debug("[PERSISTENCE] Settings saved to INI: {}", std::filesystem::path(iniPath).string());
    }
    
    void LoadSettings()
    {
        std::wstring iniPath = GetIniPath();
        
        // Note: We don't pre-create the INI anymore - it will be created automatically
        // when SaveSettings() is called after globals are loaded
        
        // Check if INI exists before trying to load
        bool iniExists = std::filesystem::exists(iniPath);
        if (iniExists) {
            spdlog::info("[PERSISTENCE] Loading settings from INI: {}", std::filesystem::path(iniPath).string());
        } else {
            spdlog::info("[PERSISTENCE] INI file not found, will be created on first save: {}", std::filesystem::path(iniPath).string());
        }
        
        auto& settings = const_cast<Config::Settings&>(Config::GetSettings());
        
        // Load master toggles
        if (settings.blacklist.toggleGlobal) {
            settings.blacklist.toggleGlobal->value = static_cast<float>(
                GetPrivateProfileIntW(L"Settings", L"BlacklistEnabled", DEFAULT_BLACKLIST_ENABLED, iniPath.c_str()));
        }
        if (settings.skyrimNetFilter.toggleGlobal) {
            settings.skyrimNetFilter.toggleGlobal->value = static_cast<float>(
                GetPrivateProfileIntW(L"Settings", L"SkyrimNetFilterEnabled", DEFAULT_SKYRIMNET_FILTER_ENABLED, iniPath.c_str()));
        }
        if (settings.mcm.blockScenesGlobal) {
            settings.mcm.blockScenesGlobal->value = static_cast<float>(
                GetPrivateProfileIntW(L"Settings", L"BlockScenes", DEFAULT_BLOCK_SCENES, iniPath.c_str()));
        }
        if (settings.mcm.blockBardSongsGlobal) {
            settings.mcm.blockBardSongsGlobal->value = static_cast<float>(
                GetPrivateProfileIntW(L"Settings", L"BlockBardSongs", DEFAULT_BLOCK_BARD_SONGS, iniPath.c_str()));
        }
        if (settings.mcm.blockFollowerCommentaryGlobal) {
            settings.mcm.blockFollowerCommentaryGlobal->value = static_cast<float>(
                GetPrivateProfileIntW(L"Settings", L"BlockFollowerCommentary", DEFAULT_BLOCK_FOLLOWER_COMMENTARY, iniPath.c_str()));
        }
        if (settings.mcm.preserveGruntsGlobal) {
            settings.mcm.preserveGruntsGlobal->value = static_cast<float>(
                GetPrivateProfileIntW(L"Settings", L"PreserveGrunts", DEFAULT_PRESERVE_GRUNTS, iniPath.c_str()));
        }
        
        // Load menu hotkey
        wchar_t hotkeyStr[32];
        GetPrivateProfileStringW(L"Settings", L"MenuHotkey", L"0xD2", hotkeyStr, 32, iniPath.c_str());
        settings.menuHotkey = static_cast<uint32_t>(wcstoul(hotkeyStr, nullptr, 0));
        
        spdlog::info("[PERSISTENCE] Master toggles loaded: blacklist={}, filter={}, scenes={}, bards={}, follower={}, grunts={}, hotkey=0x{:X}",
            settings.blacklist.toggleGlobal ? settings.blacklist.toggleGlobal->value : -1.0f,
            settings.skyrimNetFilter.toggleGlobal ? settings.skyrimNetFilter.toggleGlobal->value : -1.0f,
            settings.mcm.blockScenesGlobal ? settings.mcm.blockScenesGlobal->value : -1.0f,
            settings.mcm.blockBardSongsGlobal ? settings.mcm.blockBardSongsGlobal->value : -1.0f,
            settings.mcm.blockFollowerCommentaryGlobal ? settings.mcm.blockFollowerCommentaryGlobal->value : -1.0f,
            settings.mcm.preserveGruntsGlobal ? settings.mcm.preserveGruntsGlobal->value : -1.0f,
            settings.menuHotkey);
        
        // Load subtype toggles using readable names
        int loadedCount = 0;
        const auto& subtypeMap = Config::GetSubtypeNameMap();
        for (const auto& [name, subtypeId] : subtypeMap) {
            auto it = settings.mcm.subtypeGlobals.find(subtypeId);
            if (it == settings.mcm.subtypeGlobals.end() || !it->second) continue;
            
            std::wstring wideName(name.begin(), name.end());
            it->second->value = static_cast<float>(
                GetPrivateProfileIntW(L"Subtypes", wideName.c_str(), DEFAULT_SUBTYPE_ENABLED, iniPath.c_str()));
            loadedCount++;
        }
        
        spdlog::info("[PERSISTENCE] Loaded {} subtype toggles", loadedCount);
    }
    
    void Register()
    {
        // No longer using SKSE serialization - settings are stored in INI file
        spdlog::info("[PERSISTENCE] Settings persistence using INI file (no SKSE serialization needed)");
    }
}
