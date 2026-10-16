#pragma once
#include <algorithm>
#include <cstdint>
#include <deque>
#include <map>
#include <string>

namespace Mixamo::TownReactions {
enum class Role { Guard, Resident, Everyone };
enum class Event {
#define TOWN_LINE(event, id, text, role, direction) event,
#include "TownReactionLines.inc"
#undef TOWN_LINE
};
struct Line { Event event; const char *id, *text; Role role; const char* direction; };
inline constexpr Line Lines[] = {
#define TOWN_LINE(event, id, text, role, direction) {Event::event, id, text, Role::role, direction},
#include "TownReactionLines.inc"
#undef TOWN_LINE
};
inline const Line& Script(Event event) { return Lines[static_cast<int>(event)]; }
inline bool VoiceAllows(const std::string& character, Event event) {
    const auto role = Script(event).role;
#define TOWN_VOICE(id, group) if (character == id) return role == Role::Everyone || role == Role::group;
#include "TownReactionVoices.inc"
#undef TOWN_VOICE
    return false;
}
inline bool Revival(Event event) { return event == Event::Revived || event == Event::Returned; }
inline bool Crime(Event event) { return event <= Event::Away; }
inline int Priority(Event event) { return Revival(event) ? 3 : event == Event::Alarm ? 2 : 1; }
inline bool SafeTown(int living, int invaders, int darkness, bool wanted) {
    return living > 0 && invaders == 0 && darkness == 0 && !wanted;
}
struct Pending { std::string person; Event event; int town; uint64_t expires; };
// A small expiring queue avoids overlapping crowd speech and stale reactions.
struct Queue {
    uint64_t ticks = 0, nextSpeech = 0;
    std::deque<Pending> pending;
    std::map<std::string, uint64_t> cooldowns;
    void Tick() {
        ++ticks;
        pending.erase(std::remove_if(pending.begin(), pending.end(), [&](const auto& p) { return ticks >= p.expires; }), pending.end());
        for (auto i = cooldowns.begin(); i != cooldowns.end();)
            if (ticks >= i->second) i = cooldowns.erase(i); else ++i;
    }
    std::string Key(const std::string& person, Event event, int town) const {
        // Crime shouts have a town-wide cooldown; resurrection belongs to one person.
        return std::to_string(town) + ":" + (Revival(event) ? person : Script(event).id);
    }
    bool Add(const std::string& person, Event event, int town) {
        if (person.empty() || cooldowns.count(Key(person, event, town))) return false;
        for (const auto& p : pending)
            if (Key(p.person, p.event, p.town) == Key(person, event, town)) return false;
        for (auto& p : pending) if (p.person == person && p.town == town) {
            if (Priority(event) <= Priority(p.event)) return false;
            p = {person, event, town, ticks + 200}; return true;
        }
        if (pending.size() == 3) {
            auto lowest = std::min_element(pending.begin(), pending.end(), [](const auto& a, const auto& b) { return Priority(a.event) < Priority(b.event); });
            if (Priority(lowest->event) >= Priority(event)) return false;
            pending.erase(lowest);
        }
        pending.push_back({person, event, town, ticks + 200}); return true;
    }
    void Spoke(const Pending& p) {
        cooldowns[Key(p.person, p.event, p.town)] = ticks + 600;
        nextSpeech = ticks + 100;
    }
};
}
