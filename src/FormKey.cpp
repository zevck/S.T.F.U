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

#include "../include/PCH.h"
#include "FormKey.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <format>
#include <mutex>
#include <unordered_map>

namespace FormKey
{
    namespace
    {
        struct LoadedPlugin
        {
            bool light = false;
            std::uint16_t index = 0;  // compile index, or the light-plugin index for light plugins
        };

        // Load-order index <-> plugin filename, built from the loaded-plugin lists once they
        // exist. The lists are fixed after data load. The plugin's type comes from the list it
        // is in, not its ESL flag, so ESL-flagged plugins loaded as full plugins (VR without ESL
        // support) are handled like the engine handles them.
        struct PluginTable
        {
            std::array<std::string, 0xFF> full;                  // compile index -> filename
            std::unordered_map<std::uint16_t, std::string> light;  // light index -> filename
            std::unordered_map<std::string, LoadedPlugin> byName;  // lowercase filename -> slot
        };

        std::string Lowercase(std::string_view a_text)
        {
            std::string lower(a_text);
            std::transform(lower.begin(), lower.end(), lower.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return lower;
        }

        const PluginTable* GetPluginTable()
        {
            static std::mutex lock;
            static PluginTable table;
            static bool built = false;

            std::lock_guard guard(lock);
            if (built) {
                return &table;
            }

            auto* dataHandler = RE::TESDataHandler::GetSingleton();
            if (!dataHandler || dataHandler->GetLoadedModCount() == 0) {
                return nullptr;  // Too early; try again on the next call
            }

            auto* mods = dataHandler->GetLoadedMods();
            for (std::uint32_t i = 0; i < dataHandler->GetLoadedModCount(); ++i) {
                const auto* file = mods[i];
                if (file && file->compileIndex < 0xFF) {
                    std::string name(file->GetFilename());
                    table.byName[Lowercase(name)] = { false, file->compileIndex };
                    table.full[file->compileIndex] = std::move(name);
                }
            }

            auto* lightMods = dataHandler->GetLoadedLightMods();
            for (std::uint32_t i = 0; i < dataHandler->GetLoadedLightModCount(); ++i) {
                const auto* file = lightMods[i];
                if (file) {
                    std::string name(file->GetFilename());
                    table.byName[Lowercase(name)] = { true, file->smallFileCompileIndex };
                    table.light[file->smallFileCompileIndex] = std::move(name);
                }
            }

            built = true;
            spdlog::info("[FormKey] Plugin table built: {} full, {} light plugins", table.byName.size() - table.light.size(), table.light.size());
            return &table;
        }
    }

    std::string Of(const RE::TESForm* a_form)
    {
        return a_form ? OfFormID(a_form->GetFormID()) : std::string{};
    }

    std::string OfFormID(std::uint32_t a_formID)
    {
        const auto index = a_formID >> 24;
        if (a_formID == 0 || index == 0xFF) {
            return {};  // Runtime-created forms have no plugin identity
        }

        const auto* table = GetPluginTable();
        if (!table) {
            return {};
        }

        // The plugin is the one whose load-order slot the FormID is in (not GetFile(0), which
        // differs for records a mod injects into another plugin's ID space)
        const std::string* plugin = nullptr;
        std::uint32_t localID = 0;
        if (index == 0xFE) {
            const auto it = table->light.find(static_cast<std::uint16_t>((a_formID >> 12) & 0xFFF));
            plugin = it != table->light.end() ? &it->second : nullptr;
            localID = a_formID & 0xFFF;
        } else {
            plugin = &table->full[index];
            localID = a_formID & 0xFFFFFF;
        }

        if (!plugin || plugin->empty()) {
            return {};
        }
        return std::format("{:06X}:{}", localID, *plugin);
    }

    bool Parse(const std::string& a_key, std::uint32_t& a_localID, std::string& a_plugin)
    {
        const auto colon = a_key.find(':');
        if (colon == std::string::npos || colon == 0 || colon + 1 >= a_key.size()) {
            return false;
        }

        std::string_view idPart(a_key.data(), colon);
        if (idPart.size() > 2 && idPart[0] == '0' && (idPart[1] == 'x' || idPart[1] == 'X')) {
            idPart.remove_prefix(2);
        }
        if (idPart.empty() || idPart.size() > 8 ||
            !std::all_of(idPart.begin(), idPart.end(), [](unsigned char c) { return std::isxdigit(c) != 0; })) {
            return false;
        }

        a_localID = static_cast<std::uint32_t>(std::stoul(std::string(idPart), nullptr, 16));
        a_plugin = a_key.substr(colon + 1);
        return true;
    }

    std::uint32_t ToFormID(const std::string& a_key)
    {
        std::uint32_t localID = 0;
        std::string plugin;
        if (!Parse(a_key, localID, plugin)) {
            return 0;
        }

        const auto* table = GetPluginTable();
        if (!table) {
            return 0;
        }

        const auto it = table->byName.find(Lowercase(plugin));
        if (it == table->byName.end()) {
            return 0;  // Plugin not loaded
        }

        const auto& slot = it->second;
        return slot.light
            ? 0xFE000000u | (static_cast<std::uint32_t>(slot.index) << 12) | (localID & 0xFFF)
            : (static_cast<std::uint32_t>(slot.index) << 24) | (localID & 0xFFFFFF);
    }

    std::uint32_t CurrentFormID(std::uint32_t a_savedFormID, const std::string& a_key)
    {
        const auto formID = a_key.empty() ? 0 : ToFormID(a_key);
        return formID ? formID : a_savedFormID;
    }

    RE::TESForm* Lookup(const std::string& a_key)
    {
        const auto formID = ToFormID(a_key);
        return formID ? RE::TESForm::LookupByID(formID) : nullptr;
    }
}
