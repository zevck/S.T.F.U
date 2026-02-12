#include "Config.h"
#include <yaml-cpp/yaml.h>
#include <filesystem>

namespace Config
{
    static Settings g_settings;

    static std::string GetConfigPath()
    {
        static std::string path;
        if (path.empty()) {
            wchar_t buffer[MAX_PATH];
            GetModuleFileNameW(nullptr, buffer, MAX_PATH);
            std::filesystem::path exePath(buffer);
            path = (exePath.parent_path() / "Data" / "STFU Patcher" / "Config" / "STFU_SkyrimNetFilter.yaml").string();
        }
        return path;
    }

    static void LoadBlockedTopics()
    {
        std::string configPath = GetConfigPath();
        
        if (!std::filesystem::exists(configPath)) {
            spdlog::warn("Config file not found: {}", configPath);
            spdlog::info("No topics will be blocked - create STFU_SkyrimNetFilter.yaml to customize");
            return;
        }

        try {
            YAML::Node config = YAML::LoadFile(configPath);
            
            // Load blocked topics
            if (config["topics"] && config["topics"].IsSequence()) {
                for (const auto& entry : config["topics"]) {
                    if (entry.IsScalar()) {
                        std::string value = entry.as<std::string>();
                        
                        // Check if it's a FormID (starts with 0x) or EditorID
                        if (value.substr(0, 2) == "0x" || value.substr(0, 2) == "0X") {
                            // Parse as hex FormID
                            uint32_t formID = std::stoul(value, nullptr, 16);
                            g_settings.blacklistedFormIDs.insert(formID);
                        } else {
                            // Treat as EditorID
                            g_settings.blacklistedEditorIDs.insert(value);
                        }
                    }
                }
            }
            
            // Load blocked quests
            if (config["quests"] && config["quests"].IsSequence()) {
                for (const auto& entry : config["quests"]) {
                    if (entry.IsScalar()) {
                        std::string value = entry.as<std::string>();
                        
                        // Check if it's a FormID (starts with 0x) or EditorID
                        if (value.substr(0, 2) == "0x" || value.substr(0, 2) == "0X") {
                            // Parse as hex FormID
                            uint32_t formID = std::stoul(value, nullptr, 16);
                            g_settings.blacklistedQuestFormIDs.insert(formID);
                        } else {
                            // Treat as EditorID
                            g_settings.blacklistedQuestEditorIDs.insert(value);
                        }
                    }
                }
            }
            
            spdlog::info("Loaded {} blocked topic FormIDs and {} EditorIDs from YAML", 
                g_settings.blacklistedFormIDs.size(), 
                g_settings.blacklistedEditorIDs.size());
            spdlog::info("Loaded {} blocked quest FormIDs and {} EditorIDs from YAML", 
                g_settings.blacklistedQuestFormIDs.size(), 
                g_settings.blacklistedQuestEditorIDs.size());
        } catch (const YAML::Exception& e) {
            spdlog::error("YAML parse error: {}", e.what());
            spdlog::error("Fix STFU_SkyrimNetFilter.yaml or delete it to disable blocking");
        }
    }

    void Load()
    {
        spdlog::info("Loading STFU configuration...");
        LoadBlockedTopics();
        
        // Look up toggle global from STFU.esp by EditorID
        auto dataHandler = RE::TESDataHandler::GetSingleton();
        if (dataHandler) {
            // Try to find STFU.esp in load order
            auto stfuFile = dataHandler->LookupModByName("STFU.esp");
            if (stfuFile) {
                // Search all globals for matching EditorID
                for (auto& global : dataHandler->GetFormArray<RE::TESGlobal>()) {
                    if (global && global->GetFormEditorID()) {
                        std::string editorID = global->GetFormEditorID();
                        if (editorID == "STFU_SkyrimNetFilter") {
                            g_settings.toggleGlobal = global;
                            spdlog::info("Found STFU_SkyrimNetFilter global (FormID: 0x{:08X})", global->GetFormID());
                            break;
                        }
                    }
                }
            }
            
            if (!g_settings.toggleGlobal) {
                spdlog::warn("STFU_SkyrimNetFilter global not found - blocking will always be active");
                spdlog::warn("Add a global with EditorID 'STFU_SkyrimNetFilter' to STFU.esp for MCM toggle support");
            }
        }
        
        spdlog::info("Configuration loaded");
    }

    const Settings& GetSettings()
    {
        return g_settings;
    }
}
