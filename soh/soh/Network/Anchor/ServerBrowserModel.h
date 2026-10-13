#pragma once
#include <algorithm>
#include <cctype>
#include <string>
#include <vector>
struct BrowserRoom {
    std::string roomId;
    int players = 0, maxPlayers = 8, playersInGame = 0;
    std::vector<std::string> versions, teams;
    int difficultyMode = 0, syncItemsAndFlags = 0, pvpMode = 0;
    int storyStage = -1;
    std::string storyTitle, questTitle, chapter, campaignPlayer, campaignTeam;
    int questDone = 0, questTotal = 0, questState = 0;
};
struct BrowserFilters {
    int availability = 1; // 0 all, 1 space available, 2 full
    int rules = 0;        // 0 all, 1 co-op, 2 PvP
    int stage = -1;
};
inline std::string BrowserFold(std::string text) {
    for (auto& c : text)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return text;
}
inline bool BrowserMatches(const BrowserRoom& r, const BrowserFilters& f, const std::string& query) {
    const bool full = r.players >= r.maxPlayers;
    if ((f.availability == 1 && full) || (f.availability == 2 && !full))
        return false;
    if ((f.rules == 1 && r.pvpMode) || (f.rules == 2 && !r.pvpMode))
        return false;
    if (f.stage >= 0 && r.storyStage != f.stage)
        return false;
    std::string text = r.roomId + " " + r.storyTitle + " " + r.questTitle + " " + r.chapter;
    for (const auto& team : r.teams)
        text += " " + team;
    return BrowserFold(text).find(BrowserFold(query)) != std::string::npos;
}
inline bool BrowserOrder(const BrowserRoom& a, const BrowserRoom& b) {
    const bool af = a.players >= a.maxPlayers, bf = b.players >= b.maxPlayers;
    if (af != bf)
        return !af;
    if (a.players != b.players)
        return a.players > b.players;
    return BrowserFold(a.roomId) < BrowserFold(b.roomId);
}
