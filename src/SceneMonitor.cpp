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

#include "SceneMonitor.h"
#include "EditorID.h"
#include "Config.h"
#include "DialogueDatabase.h"
#include <spdlog/spdlog.h>

namespace SceneMonitor
{
    void Initialize()
    {
        // Add the bard song scenes to the blacklist once per database. Gated on a meta flag,
        // not on the blacklist's contents, so scenes the user removes stay removed.
        auto* db = DialogueDB::GetDatabase();
        if (!db) {
            spdlog::error("[SceneMonitor] Database not available");
            return;
        }
        if (db->GetMetaFlag("bard_scenes_initialized")) {
            spdlog::info("[SceneMonitor] Bard scenes already initialized, skipping auto-population");
            return;
        }

        // Insert-only: a DB upgraded from before this flag existed may already have edited bard rows
        const auto scenes = Config::GetBardSongScenesList();
        const int added = db->ImportHardcodedScenes(scenes, "BardSongs", true);
        db->SetMetaFlag("bard_scenes_initialized", true);
        spdlog::info("[SceneMonitor] Added {} of {} bard song scenes to the blacklist (first run)", added, scenes.size());
    }
}
