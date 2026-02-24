#pragma once

#include "../include/PCH.h"

namespace SceneMonitor
{
    // Initialize the scene monitor
    void Initialize();
    
    // Register a quest whose scenes should be monitored
    void RegisterBardQuest(RE::TESQuest* quest);
}
