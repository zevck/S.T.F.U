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
#include <cstdio>
#include <string>
#include <vector>
#include <spdlog/spdlog.h>

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
                default:
                    if (byte < 0x20) {
                        char buf[8];
                        snprintf(buf, sizeof(buf), "\\x%02x", static_cast<unsigned int>(byte));
                        result += buf;
                    } else {
                        result += static_cast<char>(byte);
                    }
                    break;
            }
        }

        return result;
    }

    // type: "success", "error" or "info"
    inline std::string BuildToastScript(const std::string& message, const char* type)
    {
        return "window.showToast('" + EscapeSingleQuotedJSString(message) + "', '" + type + "')";
    }

    inline std::string BuildSKSEUpdateScript(const std::string& eventName, const std::string& jsonData)
    {
        return "window.SKSE_API?.call('" + eventName + "', '" + EscapeSingleQuotedJSString(jsonData) + "')";
    }

    // ---- Minimal readers for the flat JSON objects the UI sends (JSON.stringify output) ----

    // Reads the JSON string literal starting at json[pos] (which must be '"'), unescaping
    // it into `out`. On success pos is left just past the closing quote.
    inline bool ReadJsonString(const std::string& json, size_t& pos, std::string& out)
    {
        out.clear();
        if (pos >= json.size() || json[pos] != '"') return false;
        ++pos;
        while (pos < json.size()) {
            char c = json[pos++];
            if (c == '"') return true;
            if (c != '\\') {
                out += c;
                continue;
            }
            if (pos >= json.size()) return false;
            char esc = json[pos++];
            switch (esc) {
                case 'n': out += '\n'; break;
                case 't': out += '\t'; break;
                case 'r': out += '\r'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case 'u': {
                    if (pos + 4 > json.size()) return false;
                    unsigned int cp = 0;
                    try {
                        cp = static_cast<unsigned int>(std::stoul(json.substr(pos, 4), nullptr, 16));
                    } catch (...) {
                        return false;
                    }
                    pos += 4;
                    // Encode as UTF-8 (JSON.stringify only emits \u for control characters)
                    if (cp < 0x80) {
                        out += static_cast<char>(cp);
                    } else if (cp < 0x800) {
                        out += static_cast<char>(0xC0 | (cp >> 6));
                        out += static_cast<char>(0x80 | (cp & 0x3F));
                    } else {
                        out += static_cast<char>(0xE0 | (cp >> 12));
                        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                        out += static_cast<char>(0x80 | (cp & 0x3F));
                    }
                    break;
                }
                default: out += esc; break;  // \" \\ \/
            }
        }
        return false;
    }

    // Position of the value for "key" (after any whitespace), or npos if the key is absent.
    inline size_t FindJsonValue(const std::string& json, const std::string& key)
    {
        std::string searchKey = "\"" + key + "\":";
        size_t pos = json.find(searchKey);
        if (pos == std::string::npos) return pos;
        pos += searchKey.length();
        while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
        return pos < json.size() ? pos : std::string::npos;
    }

    // Value of "key": a string's unescaped contents, or the raw token for numbers and
    // booleans ("123", "true"). Returns "" if the key is absent.
    inline std::string ExtractJsonValue(const std::string& json, const std::string& key)
    {
        size_t pos = FindJsonValue(json, key);
        if (pos == std::string::npos) return "";
        if (json[pos] == '"') {
            std::string value;
            return ReadJsonString(json, pos, value) ? value : "";
        }
        size_t end = json.find_first_of(",}", pos);
        if (end == std::string::npos) end = json.size();
        while (end > pos && (json[end - 1] == ' ' || json[end - 1] == '\t')) end--;
        return json.substr(pos, end - pos);
    }

    // Elements of the string array "key": [...]; empty if the key is absent or not an array.
    inline std::vector<std::string> ExtractJsonStringArray(const std::string& json, const std::string& key)
    {
        std::vector<std::string> result;
        size_t pos = FindJsonValue(json, key);
        if (pos == std::string::npos || json[pos] != '[') return result;
        ++pos;
        while (pos < json.size()) {
            while (pos < json.size() && (json[pos] == ' ' || json[pos] == ',' || json[pos] == '\t')) pos++;
            if (pos >= json.size() || json[pos] == ']') break;
            std::string element;
            if (!ReadJsonString(json, pos, element)) break;
            result.push_back(std::move(element));
        }
        return result;
    }

    // Parses hex FormID strings ("0x01A6A4" or "01A6A4"); empty or unparseable ones are skipped.
    inline std::vector<uint32_t> ParseHexFormIDs(const std::vector<std::string>& hexStrings)
    {
        std::vector<uint32_t> formIDs;
        for (std::string hex : hexStrings) {
            if (hex.length() >= 2 && hex[0] == '0' && (hex[1] == 'x' || hex[1] == 'X')) {
                hex = hex.substr(2);
            }
            if (hex.empty()) continue;
            try {
                formIDs.push_back(static_cast<uint32_t>(std::stoul(hex, nullptr, 16)));
            } catch (...) {
                spdlog::warn("[PrismaUIMenu] Failed to parse FormID: {}", hex);
            }
        }
        return formIDs;
    }
}
