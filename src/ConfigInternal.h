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

#include "Config.h"
#include <string>
#include <unordered_map>

// State and helpers shared between the Config_*.cpp files. Not part of the
// public Config API — include only from Config*.cpp.
namespace Config
{
    extern Settings g_settings;
    extern std::unordered_map<std::string, uint16_t> SubtypeNameMap;

    void InitializeHardcodedScenes();  // Config_Scenes.cpp
    void GenerateDefaultYAMLs();       // Config_Yaml.cpp
}
