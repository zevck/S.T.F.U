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

#include <cstdio>
#include <string>

// JSON/JS string helpers shared by the PrismaUIMenu_*.cpp translation units.
namespace PrismaUIMenuDetail
{
    inline std::string escapeJSON(const std::string& str)
    {
        std::string result;
        result.reserve(str.size());

        for (unsigned char byte : str) {
            switch (byte) {
                case '"': result += "\\\""; break;
                case '\\': result += "\\\\"; break;
                case '\b': result += "\\b"; break;
                case '\f': result += "\\f"; break;
                case '\n': result += "\\n"; break;
                case '\r': result += "\\r"; break;
                case '\t': result += "\\t"; break;
                default:
                    if (byte < 0x20) {
                        char buf[8];
                        snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned int>(byte));
                        result += buf;
                    } else {
                        result += static_cast<char>(byte);
                    }
                    break;
            }
        }

        return result;
    }

    inline std::string EscapeSingleQuotedJSString(const std::string& str)
    {
        std::string result;
        result.reserve(str.size());

        for (unsigned char byte : str) {
            switch (byte) {
                case '\'': result += "\\'"; break;
                case '\\': result += "\\\\"; break;
                case '\n': result += "\\n"; break;
                case '\r': result += "\\r"; break;
                case '\t': result += "\\t"; break;
                default: result += static_cast<char>(byte); break;
            }
        }

        return result;
    }

    inline std::string BuildSKSEUpdateScript(const std::string& eventName, const std::string& jsonData)
    {
        return "window.SKSE_API?.call('" + eventName + "', '" + EscapeSingleQuotedJSString(jsonData) + "')";
    }
}
