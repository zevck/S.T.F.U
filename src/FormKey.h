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

#include <cstdint>
#include <string>

namespace RE
{
    class TESForm;
}

// A FormKey identifies a record independent of load order: its ID local to the plugin that
// defines it, plus that plugin's filename, e.g. "02707A:Skyrim.esm" or "000801:MyMod.esl".
// Runtime FormIDs change whenever the load order does (and light plugins are re-slotted), so
// anything stored in the database identifies records by FormKey.
namespace FormKey
{
    // FormKey of a loaded form. Empty for forms with no defining plugin (runtime-created forms).
    std::string Of(const RE::TESForm* a_form);

    // FormKey of a FormID valid in the current session. Empty if the form isn't loaded.
    std::string OfFormID(std::uint32_t a_formID);

    // Parses "XXXXXX:Plugin.ext" (an optional 0x prefix is accepted). Returns false if malformed.
    bool Parse(const std::string& a_key, std::uint32_t& a_localID, std::string& a_plugin);

    // The form a FormKey refers to in the current session, or nullptr if its plugin isn't loaded.
    RE::TESForm* Lookup(const std::string& a_key);

    // Current-session FormID of a FormKey, or 0 if it can't be resolved.
    std::uint32_t ToFormID(const std::string& a_key);

    // Current-session FormID of a saved record: resolved from its FormKey when it has one (the
    // FormID saved with it can be from an older load order), otherwise the saved FormID.
    std::uint32_t CurrentFormID(std::uint32_t a_savedFormID, const std::string& a_key);
}
