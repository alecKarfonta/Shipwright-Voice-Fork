#include "SavedServers.h"

#include <algorithm>
#include <sstream>

#include <libultraship/bridge/consolevariablebridge.h>
#include <nlohmann/json.hpp>
#include <ship/Context.h>
#include <ship/window/Window.h>
#include <ship/window/gui/Gui.h>

// [TIME] | [INFO] | SavedServers.cpp | Servers persisted as a JSON array cvar
// gRemote.Anchor.SavedServers = [{"name":..,"host":..,"port":..,"roomId":..}, ...]
// Storing one JSON string keeps the cvar namespace clean and survives config saves.

static const char* kCvarName = "gRemote.Anchor.SavedServers";

namespace SavedServers {

using json = nlohmann::json;

static bool ParseAll(std::vector<Entry>& out, const std::string& raw) {
    out.clear();
    if (raw.empty()) {
        return true;
    }
    json root = json::parse(raw, nullptr, false);
    if (root.is_discarded() || !root.is_array()) {
        return false;
    }
    for (auto& node : root) {
        Entry e;
        e.name = node.value("name", "");
        e.host = node.value("host", "");
        e.port = (uint16_t)node.value("port", 43383);
        e.roomId = node.value("roomId", "");
        if (!e.name.empty() && !e.host.empty()) {
            out.push_back(e);
        }
    }
    std::sort(out.begin(), out.end(), [](const Entry& a, const Entry& b) { return a.name < b.name; });
    return true;
}

static std::string Serialize(const std::vector<Entry>& entries) {
    json root = json::array();
    for (const auto& e : entries) {
        root.push_back({
            { "name", e.name },
            { "host", e.host },
            { "port", (int)e.port },
            { "roomId", e.roomId },
        });
    }
    return root.dump();
}

static void Persist(const std::vector<Entry>& entries) {
    CVarSetString(kCvarName, Serialize(entries).c_str());
    Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
}

std::vector<Entry> Load() {
    std::vector<Entry> out;
    ParseAll(out, CVarGetString(kCvarName, ""));
    return out;
}

bool Save(const Entry& entry) {
    if (entry.name.empty() || entry.host.empty()) {
        return false;
    }
    std::vector<Entry> all = Load();
    bool updated = false;
    for (auto& e : all) {
        if (e.name == entry.name) {
            e = entry;
            updated = true;
            break;
        }
    }
    if (!updated) {
        all.push_back(entry);
    }
    Persist(all);
    return true;
}

bool Remove(const std::string& name) {
    std::vector<Entry> all = Load();
    auto it = std::remove_if(all.begin(), all.end(), [&](const Entry& e) { return e.name == name; });
    bool removed = it != all.end();
    all.erase(it, all.end());
    if (removed) {
        Persist(all);
    }
    return removed;
}

bool Exists(const std::string& name) {
    std::vector<Entry> all = Load();
    return std::any_of(all.begin(), all.end(), [&](const Entry& e) { return e.name == name; });
}

} // namespace SavedServers
