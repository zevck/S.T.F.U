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

#include "../include/PCH.h"

namespace STFU
{
    // Resolve a form's EditorID.
    //
    // On Skyrim AE the engine discards EditorIDs for most form types, so the
    // TESForm::GetFormEditorID() vfunc returns "" (empty) for them — scenes,
    // topics, quests, factions, etc. STFU keys a lot of its logic on those
    // EditorIDs, so it needs them restored.
    //
    // We used to depend on Native EditorID Fix for this, but it conflicts with
    // several mods. SkyrimNet migrated to powerofthree's Tweaks, so STFU follows.
    // po3's Tweaks restores EditorIDs and exposes an exported GetFormEditorID(formID);
    // we use that as the source of truth, resolved once and cached. If po3 isn't
    // loaded (or has no entry for the form) we fall back to the vanilla vfunc, which
    // still returns the real value for form types the engine keeps.
    //
    // Returns a const char* (never a dangling pointer): po3's strings are interned
    // for the process lifetime, and the vanilla fallback returns engine-owned
    // storage — same transient-use contract the old GetFormEditorID() calls had.
    // May return nullptr when there is genuinely no EditorID (matches vanilla).
    inline const char* GetEditorID(const RE::TESForm* a_form)
    {
        if (!a_form) return nullptr;

        using GetFormEditorID_t = const char* (*)(std::uint32_t);

        // Resolve po3_Tweaks' exported function exactly once. Function-local static
        // init is thread-safe (C++11 magic statics) — these calls run on the engine's
        // worker threads, so that guarantee matters.
        static const GetFormEditorID_t po3GetEditorID = []() -> GetFormEditorID_t {
            if (auto* tweaks = GetModuleHandleW(L"po3_Tweaks")) {
                return reinterpret_cast<GetFormEditorID_t>(GetProcAddress(tweaks, "GetFormEditorID"));
            }
            return nullptr;
        }();

        if (po3GetEditorID) {
            if (const char* id = po3GetEditorID(a_form->formID); id && id[0] != '\0') {
                return id;
            }
        }

        // Fallback: vanilla vfunc (works for forms the engine keeps, or if po3 absent).
        return a_form->GetFormEditorID();
    }
}
