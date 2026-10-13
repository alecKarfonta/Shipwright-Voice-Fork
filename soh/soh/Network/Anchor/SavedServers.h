#pragma once

// [TIME] | [INFO] | SavedServers.h | cvar-backed address book of multiplayer servers

#include <string>
#include <vector>

namespace SavedServers {

struct Entry {
    std::string name;   // display name, e.g. "Alec's Server"
    std::string host;   // IP or hostname
    uint16_t port;      // TCP port (Anchor default 43383)
    std::string roomId; // last-used room on this server (optional convenience)
};

// Load all saved servers (sorted by name). Reads from CVars on every call so
// external edits to the config file are picked up.
std::vector<Entry> Load();

// Insert or update (matched by name). Returns false if name is empty.
bool Save(const Entry& entry);

// Remove a saved server by name. Returns false if not found.
bool Remove(const std::string& name);

// True if an entry with this name exists.
bool Exists(const std::string& name);

} // namespace SavedServers
