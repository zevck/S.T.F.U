#pragma once

#include "../include/PCH.h"

// Handles persistent storage of runtime settings via SQLite database
namespace SettingsPersistence
{
    // Initialize persistence system
    void Register();
    
    // Load settings from database (called once at startup)
    void LoadSettings();
    
    // Save current settings to database (called when settings change)
    void SaveSettings();
}
