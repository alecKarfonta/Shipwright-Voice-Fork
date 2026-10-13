#include "ServerBrowserWindow.h"

#include <chrono>
#include <cstring>
#include <future>
#include <thread>
#include <algorithm>
#include <libultraship/libultraship.h>
#include "soh/Enhancements/arpg/ARPGTheme.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
extern "C" {
#include "src/overlays/gamestates/ovl_file_choose/file_choose.h"
#include "textures/title_static/title_static.h"
}

#include <SDL2/SDL_net.h>
#include <imgui.h>
#include <nlohmann/json.hpp>
#include <ship/Context.h>
#include <ship/window/Window.h>
#include <ship/window/gui/Gui.h>

#include "Anchor.h"
#include "CharacterTravel.h"
#include "soh/Network/RelayWebSocket.h"
#include "soh/Notification/Notification.h"
#include "soh/SohGui/UIWidgets.hpp"
#include "soh/util.h"

using namespace UIWidgets;

// [TIME] | [INFO] | ServerBrowserWindow.cpp | Two responsibilities:
//   1. Manage the saved-server address book (SavedServers store)
//   2. Browse live rooms on a server via the LIST_ROOMS/ROOM_LIST protocol
//      on a throwaway TCP connection, then join through the normal Anchor client

namespace {

RoomQueryResult QueryRoomListBlocking(const std::string& host, uint16_t port) {
    RoomQueryResult result;
    std::string received;
    if (PublicRelay::IsWebSocket(host)) {
        received = PublicRelay::Query(host, R"({"type":"LIST_ROOMS"})", result.error);
        if (received.empty())
            return result;
    } else {
        IPaddress addr;
        if (SDLNet_ResolveHost(&addr, host.c_str(), port) == -1) {
            result.error = SDLNet_GetError();
            return result;
        }
        TCPsocket sock = SDLNet_TCP_Open(&addr);
        if (!sock) {
            result.error = SDLNet_GetError();
            return result;
        }

        // Send LIST_ROOMS (null-delimited like all Anchor frames)
        nlohmann::json req;
        req["type"] = "LIST_ROOMS";
        std::string payload = req.dump();
        SDLNet_TCP_Send(sock, payload.c_str(), (int)payload.size() + 1);

        // Read until a full null-terminated frame arrives or we time out
        SDLNet_SocketSet set = SDLNet_AllocSocketSet(1);
        SDLNet_TCP_AddSocket(set, sock);
        char buf[2048];
        Uint32 start = SDL_GetTicks();
        bool gotFrame = false;
        while (SDL_GetTicks() - start < 3000) {
            if (SDLNet_CheckSockets(set, 200) > 0 && SDLNet_SocketReady(sock)) {
                int len = SDLNet_TCP_Recv(sock, buf, sizeof(buf));
                if (len <= 0) {
                    break;
                }
                received.append(buf, len);
                if (received.size() > 2 * 1024 * 1024)
                    break;
                if (received.find('\0') != std::string::npos) {
                    gotFrame = true;
                    break;
                }
            }
        }
        SDLNet_FreeSocketSet(set);
        SDLNet_TCP_Close(sock);

        if (!gotFrame) {
            result.error = "timed out waiting for response";
            return result;
        }
    }
    nlohmann::json resp = nlohmann::json::parse(received.substr(0, received.find('\0')), nullptr, false);
    if (resp.is_discarded() || !resp.contains("rooms") || !resp["rooms"].is_array()) {
        result.error = "unexpected response from server";
        return result;
    }
    try {
        for (auto& r : resp["rooms"]) {
            BrowserRoom room;
            room.roomId = r.value("roomId", "");
            if (room.roomId.empty()) {
                continue;
            }
            room.players = r.value("players", 0);
            room.maxPlayers = r.value("maxPlayers", 8);
            room.versions = r.value("versions", std::vector<std::string>{});
            room.playersInGame = r.value("playersInGame", 0);
            for (auto& t : r.value("teams", nlohmann::json::array())) {
                if (t.is_string()) {
                    room.teams.push_back(t.get<std::string>());
                }
            }
            if (r.contains("roomState") && r["roomState"].is_object()) {
                room.difficultyMode = r["roomState"].value("difficultyMode", 0);
                room.syncItemsAndFlags = r["roomState"].value("syncItemsAndFlags", 0);
                room.pvpMode = r["roomState"].value("pvpMode", 0);
            }
            if (r.contains("campaign") && r["campaign"].is_object()) {
                const auto& c = r["campaign"];
                room.storyStage = c.value("storyStage", -1);
                room.storyTitle = c.value("storyTitle", "");
                room.questTitle = c.value("questTitle", "");
                room.chapter = c.value("chapter", "");
                room.campaignPlayer = c.value("player", "");
                room.campaignTeam = c.value("team", "");
                room.questDone = c.value("completedObjectives", 0);
                room.questTotal = c.value("totalObjectives", 0);
                room.questState = c.value("questState", 0);
            }
            result.rooms.push_back(room);
        }
    } catch (const std::exception&) {
        result.rooms.clear();
        result.error = "Malformed room list from relay.";
        return result;
    }
    result.ok = true;
    return result;
}

} // namespace

namespace {
namespace Theme = ARPG::Theme;
constexpr int DirectoryInputBlock = 0x444952;
const char* StoryNames[] = { "Inside the Great Deku Tree", "Dodongo's Cavern",       "Inside Jabu-Jabu's Belly",
                             "Awaken the Forest Sage",     "Awaken the Fire Sage",   "Awaken the Water Sage",
                             "Awaken the Shadow Sage",     "Awaken the Spirit Sage", "Break the Castle's Barrier",
                             "Defeat Ganondorf and Ganon", "Hyrule Saved",           "Randomized Adventure" };
void NativePanel(ImDrawList* draw, ImVec2 p, ImVec2 size) {
    const char* tiles[] = { gFileSelWindow1Tex,  gFileSelWindow2Tex,  gFileSelWindow3Tex,  gFileSelWindow4Tex,
                            gFileSelWindow5Tex,  gFileSelWindow6Tex,  gFileSelWindow7Tex,  gFileSelWindow8Tex,
                            gFileSelWindow9Tex,  gFileSelWindow10Tex, gFileSelWindow11Tex, gFileSelWindow12Tex,
                            gFileSelWindow13Tex, gFileSelWindow14Tex, gFileSelWindow15Tex, gFileSelWindow16Tex,
                            gFileSelWindow17Tex, gFileSelWindow18Tex, gFileSelWindow19Tex, gFileSelWindow20Tex };
    for (int col = 0; col < 4; ++col)
        for (int row = 0; row < 5; ++row) {
            float x = col * 64.f / 240.f, end = std::min(1.f, (col + 1) * 64.f / 240.f);
            draw->AddImage(Theme::Texture(tiles[row * 4 + col]), { p.x + x * size.x, p.y + row * size.y / 5 },
                           { p.x + end * size.x, p.y + (row + 1) * size.y / 5 }, { 0, 0 }, { 1, 1 },
                           IM_COL32(100, 150, 255, 255));
        }
}
bool NativeButton(const char* id, const std::string& label, ImVec2 size, bool selected = false) {
    if (size.x < 0)
        size.x = ImGui::GetContentRegionAvail().x;
    auto p = ImGui::GetCursorScreenPos();
    ImGui::PushID(id);
    ImGui::PushStyleColor(ImGuiCol_Button, { 0, 0, 0, 0 });
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 0, 0, 0, 0 });
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, { 0, 0, 0, 0 });
    bool clicked = ImGui::Button("##native", size);
    ImGui::PopStyleColor(3);
    auto* draw = ImGui::GetWindowDrawList();
    bool focus = ImGui::IsItemHovered() || ImGui::IsItemFocused();
    auto tint = ImGui::GetColorU32(selected ? ImVec4(.62f, .78f, 1, 1) : ImVec4(.39f, .59f, 1, 1));
    draw->AddImage(Theme::Texture(gFileSelNameBoxTex), p, { p.x + size.x, p.y + size.y }, { 0, 0 }, { 1, 1 }, tint);
    if (focus || selected)
        draw->AddImage(Theme::Texture(gFileSelBigButtonHighlightTex), { p.x - 3, p.y - 3 },
                       { p.x + size.x + 3, p.y + size.y + 3 }, { 0, 0 }, { 1, 1 },
                       ImGui::GetColorU32(ImVec4(.8f, .94f, 1, focus ? .8f : .35f)));
    if (!label.empty()) {
        float font = std::min(22.f, 22.f * (size.x - 22) / std::max(1.f, Theme::NativeWidth(label, 22)));
        Theme::NativeText(draw, { p.x + (size.x - Theme::NativeWidth(label, font)) / 2, p.y + (size.y - font) / 2 },
                          label, font, ImGui::GetColorU32(selected ? ImVec4(1, .97f, .55f, 1) : ImVec4(1, 1, 1, 1)));
    }
    ImGui::PopID();
    return clicked;
}
void Ellipsis(ImDrawList* draw, ImVec2 pos, std::string text, float width, float size, ImU32 color) {
    if (Theme::TextWidth(text, size) > width) {
        while (!text.empty() && Theme::TextWidth(text + "...", size) > width) {
            size_t end = text.size() - 1;
            while (end > 0 && (static_cast<unsigned char>(text[end]) & 0xc0) == 0x80)
                --end;
            text.erase(end);
        }
        text += "...";
    }
    Theme::Text(draw, pos, text, size, color);
}
} // namespace

void ServerBrowserWindow::Capture(bool active) {
    if (captured == active)
        return;
    captured = active;
    auto* context = Ship::Context::GetRawInstance();
    context->GetWindow()->GetGui()->SetGameModalActive(active);
    if (active) {
        context->GetControlDeck()->BlockGameInput(DirectoryInputBlock);
        context->GetWindow()->SetMouseCapture(false);
        context->GetWindow()->SetCursorVisibility(true);
    } else {
        context->GetControlDeck()->UnblockGameInput(DirectoryInputBlock);
        context->GetWindow()->GetMouseStateManager()->UpdateMouseCapture();
    }
}
void ServerBrowserWindow::SetOpen(bool open) {
    SetVisibility(open);
    focusPending = open;
    if (open) {
        Capture(true);
        tab = 0;
        detailsPage = false;
        nextRefresh = 0;
    } else
        releaseInputAt = ImGui::GetTime() + .18;
}
void ServerBrowserWindow::InitElement() {
    snprintf(browseHostBuf, sizeof(browseHostBuf), "%s", PublicRelay::Endpoint);
    browsePort = 443;
    snprintf(teamBuf, sizeof(teamBuf), "%s", CVarGetString(CVAR_REMOTE_ANCHOR("TeamId"), "default"));
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPresentFileSelect>([this]() {
        fileSelect = true;
        fileSelectReady = true;
        SetOpen(true);
    });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnFileChooseMain>([this](void* state) {
        auto* menu = static_cast<FileChooseContext*>(state);
        fileSelect = true;
        fileSelectReady = menu->menuMode == FS_MENU_MODE_CONFIG && menu->configMode == CM_MAIN_MENU;
        if (fileSelectReady && !IsVisible() && (menu->state.input[0].press.button & BTN_Z))
            SetOpen(true);
        if (captured)
            menu->state.input[0] = {};
    });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnExitGame>([](int32_t) {
        if (Anchor::Instance->isEnabled) {
            CVarClear(CVAR_REMOTE_ANCHOR("Enabled"));
            Anchor::Instance->Disable();
        }
    });
    auto leave = [this]() {
        fileSelect = fileSelectReady = false;
        Hide();
        Capture(false);
    };
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnLoadGame>([leave](int32_t) { leave(); });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnZTitleInit>([leave](void*) { leave(); });
}
void ServerBrowserWindow::Draw() {
    if (!IsVisible()) {
        if (captured && ImGui::GetTime() >= releaseInputAt)
            Capture(false);
        if (!fileSelectReady)
            return;
        auto* vp = ImGui::GetMainViewport();
        ImGui::SetNextWindowViewport(vp->ID);
        ImGui::SetNextWindowPos({ vp->WorkPos.x + vp->WorkSize.x / 2, vp->WorkPos.y + vp->WorkSize.y - 54 },
                                ImGuiCond_Always, { .5f, 0 });
        ImGui::SetNextWindowSize({ 240, 46 });
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 4, 4 });
        if (ImGui::Begin("##PublicGamesEntry", nullptr,
                         ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking |
                             ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoNavFocus)) {
            if (NativeButton("open", "Public Games [Z]", { -1, 34 }))
                SetOpen(true);
        }
        ImGui::End();
        ImGui::PopStyleVar();
        return;
    }
    Capture(true);
    if (!popupOpenLastFrame && !ImGui::GetIO().WantTextInput &&
        (ImGui::IsKeyPressed(ImGuiKey_Escape, false) || ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false))) {
        if (detailsPage)
            detailsPage = false;
        else {
            SetOpen(false);
            return;
        }
    }
    DrawNativeBrowser();
    popupOpenLastFrame = ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
}
void ServerBrowserWindow::DrawElement() {
    DrawRoomBrowser();
}
void ServerBrowserWindow::DrawNativeBrowser() {
    auto* vp = ImGui::GetMainViewport();
    const float w = std::min(1180.f, vp->WorkSize.x - 20), h = std::min(850.f, vp->WorkSize.y - 20);
    ImGui::SetNextWindowViewport(vp->ID);
    ImGui::SetNextWindowPos({ vp->WorkPos.x + vp->WorkSize.x / 2, vp->WorkPos.y + vp->WorkSize.y / 2 },
                            ImGuiCond_Always, { .5f, .5f });
    ImGui::SetNextWindowSize({ w, h });
    if (focusPending)
        ImGui::SetNextWindowFocus();
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 26, 22 });
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { 8, 5 });
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 8, 7 });
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 11);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, { 0, 0, 0, 0 });
    ImGui::PushStyleColor(ImGuiCol_ChildBg, { .025f, .06f, .16f, .78f });
    ImGui::PushStyleColor(ImGuiCol_FrameBg, { .04f, .10f, .26f, .95f });
    ImGui::PushStyleColor(ImGuiCol_Text, { .95f, .97f, 1, 1 });
    ImGui::PushStyleColor(ImGuiCol_TextDisabled, { .65f, .78f, .94f, 1 });
    ImGui::PushStyleColor(ImGuiCol_Border, { .42f, .62f, .94f, .6f });
    if (ImGui::Begin("Hyrule Public Games", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking |
                         ImGuiWindowFlags_NoMove)) {
        auto* draw = ImGui::GetWindowDrawList();
        draw->PushClipRectFullScreen();
        draw->AddRectFilled(vp->Pos, { vp->Pos.x + vp->Size.x, vp->Pos.y + vp->Size.y }, IM_COL32(2, 5, 18, 220));
        NativePanel(draw, ImGui::GetWindowPos(), ImGui::GetWindowSize());
        draw->PopClipRect();
        auto p = ImGui::GetCursorScreenPos();
        Theme::NativeText(draw, { p.x + 4, p.y + 5 }, "Hyrule Reborn", w < 700 ? 25 : 31);
        ImGui::SetCursorPosX(w - 26 - 160);
        if (NativeButton("back", fileSelect ? "Characters" : "Back", { 160, 36 }))
            SetOpen(false);
        float width = ImGui::GetContentRegionAvail().x;
        const char* tabs[] = { "Public Games", "Host / Join", "Connection" };
        if (!ImGui::GetIO().WantTextInput) {
            if (ImGui::IsKeyPressed(ImGuiKey_GamepadL1, false) || ImGui::IsKeyPressed(ImGuiKey_LeftBracket, false)) {
                tab = (tab + 2) % 3;
                detailsPage = false;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_GamepadR1, false) || ImGui::IsKeyPressed(ImGuiKey_RightBracket, false)) {
                tab = (tab + 1) % 3;
                detailsPage = false;
            }
        }
        for (int i = 0; i < 3; ++i) {
            if (i)
                ImGui::SameLine();
            if (NativeButton(tabs[i], tabs[i], { (width - 16) / 3, 36 }, tab == i)) {
                tab = i;
                detailsPage = false;
            }
            if (focusPending && i == 0)
                ImGui::SetKeyboardFocusHere(-1);
        }
        focusPending = false;
        if (firstDraw || (tab == 0 && ImGui::GetTime() >= nextRefresh)) {
            firstDraw = false;
            RefreshRooms();
        }
        DrawRoomBrowser();
        ImGui::TextDisabled("A / Enter  Select     B / Esc  Back     LB / RB  Tabs");
    }
    ImGui::End();
    ImGui::PopStyleColor(6);
    ImGui::PopStyleVar(4);
}
void ServerBrowserWindow::DrawRoomBrowser() {
    ImGui::TextWrapped("%s", CharacterTravel::Summary().c_str());
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Saved on this device. Use the same save slot in every party.\nShare campaign quests from the journal; rewards stay personal.");
    ImGui::BeginChild("directoryBody", { 0, -24 }, ImGuiChildFlags_AlwaysUseWindowPadding);
    if (!travelError.empty()) ImGui::TextWrapped("%s", travelError.c_str());
    if (Anchor::Instance->isEnabled) {
        Theme::Label(Anchor::Instance->isConnected ? "Party joined" : "Connecting to party...", 25);
        ImGui::TextWrapped("%s", CVarGetString(CVAR_REMOTE_ANCHOR("RoomId"), ""));
        ImGui::Text("%zu / 8 players | Team: %s", Anchor::Instance->clients.size(),
                    CVarGetString(CVAR_REMOTE_ANCHOR("TeamId"), "default"));
        ImGui::TextWrapped(Anchor::Instance->isConnected
                               ? "Gear, skills, currency and progress stay with this character."
                               : "Waiting for the relay. You can cancel and choose another party.");
        ImGui::BeginDisabled(!Anchor::Instance->isConnected);
        if (NativeButton("continue", fileSelect ? "Choose Character" : "Return to Adventure", { -1, 40 }))
            SetOpen(false);
        ImGui::EndDisabled();
        const bool loaded = Anchor::Instance->IsSaveLoaded();
        if (NativeButton("disconnect", loaded ? "Save & Switch Server" : "Choose Another Party", { -1, 36 }))
            LeaveServer(true);
        if (loaded && NativeButton("solo", "Save & Play Solo", { -1, 36 }))
            LeaveServer(false);
    } else if (tab == 0)
        DrawPublicGames();
    else if (tab == 1)
        DrawHost();
    else {
        Theme::Label("Relay Connection", 25);
        ImGui::TextWrapped("Official service: mlapi.us | Public games refresh every 15 seconds.");
        ImGui::BeginDisabled(querying);
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##relay", browseHostBuf, sizeof(browseHostBuf));
        if (!PublicRelay::IsWebSocket(browseHostBuf))
            ImGui::InputInt("TCP port", &browsePort);
        browsePort = std::clamp(browsePort, 1, 65535);
        if (NativeButton("official", "Official Relay", { -1, 36 })) {
            snprintf(browseHostBuf, sizeof(browseHostBuf), "%s", PublicRelay::Endpoint);
            browsePort = 443;
            RefreshRooms();
        }
        if (NativeButton("query", "Use This Relay", { -1, 36 })) {
            RefreshRooms();
            tab = 0;
        }
        ImGui::EndDisabled();
        if (queryFailed) {
            ImGui::TextWrapped("Connection detail: %s", queryError.c_str());
        }
        if (ImGui::CollapsingHeader("Saved relays"))
            DrawServerManager();
    }
    ImGui::EndChild();
}
void ServerBrowserWindow::DrawPublicGames() {
    if (queryHost != browseHostBuf || queryPort != browsePort)
        RefreshRooms();
    auto selected = std::find_if(rooms.begin(), rooms.end(), [&](const auto& r) { return r.roomId == selectedId; });
    if (detailsPage && selected != rooms.end()) {
        if (NativeButton("list", "Back to Games", { -1, 34 }))
            detailsPage = false;
        DrawRoomDetails(*selected);
        return;
    }
    detailsPage = false;
    float width = ImGui::GetContentRegionAvail().x;
    ImGui::SetNextItemWidth(width - 104);
    ImGui::InputTextWithHint("##find", "Search games, teams or quests", searchBuf, sizeof(searchBuf));
    ImGui::SameLine();
    ImGui::BeginDisabled(querying);
    if (NativeButton("refresh", "Refresh", { 96, ImGui::GetFrameHeight() }))
        RefreshRooms();
    ImGui::EndDisabled();
    ImGui::SetNextItemWidth((width - 16) * .28f);
    ImGui::Combo("##spaces", &filters.availability, "All parties\0Open places\0Full parties\0");
    ImGui::SameLine();
    ImGui::SetNextItemWidth((width - 16) * .23f);
    ImGui::Combo("##rules", &filters.rules, "Any rules\0Co-op\0PvP\0");
    ImGui::SameLine();
    ImGui::SetNextItemWidth((width - 16) * .49f);
    if (ImGui::BeginCombo("##story", filters.stage < 0 ? "Any campaign stage" : StoryNames[filters.stage])) {
        if (ImGui::Selectable("Any campaign stage", filters.stage < 0))
            filters.stage = -1;
        for (int i = 0; i < int(std::size(StoryNames)); ++i)
            if (ImGui::Selectable(StoryNames[i], filters.stage == i))
                filters.stage = i;
        ImGui::EndCombo();
    }
    if (selected != rooms.end() && !BrowserMatches(*selected, filters, searchBuf))
        selectedId.clear();
    if (queryFailed) {
        ImGui::TextWrapped("The relay is temporarily unavailable. Refresh to retry.");
        if (!rooms.empty())
            ImGui::TextDisabled("Showing the last successful list. Refresh before joining.");
    }
    int shown = 0, players = 0;
    for (const auto& room : rooms) {
        players += room.players;
        if (BrowserMatches(room, filters, searchBuf))
            ++shown;
    }
    ImGui::TextDisabled("%d parties shown | %d players online%s", shown, players, querying ? " | Refreshing..." : "");
    const bool wide = width >= 820;
    const float listWidth = wide ? width * .56f : width;
    ImGui::BeginChild("gamesList", { listWidth, 0 }, ImGuiChildFlags_None);
    if (!shown) {
        ImGui::Spacing();
        Theme::Label(queryFailed && rooms.empty() ? "Unable to reach the relay"
                     : querying && rooms.empty()  ? "Calling the adventurers..."
                     : rooms.empty()              ? "An adventure awaits"
                                                  : "No matching parties",
                     23);
        ImGui::TextWrapped(queryFailed && rooms.empty()
                               ? "You can retry or choose Select Save to continue your adventure."
                           : rooms.empty() ? "Host a public game and welcome the first adventurers to your party."
                                           : "Try another quest, campaign stage or party filter.");
        if (!rooms.empty() && NativeButton("clear", "Clear Filters", { -1, 36 })) {
            filters = {};
            filters.availability = 0;
            searchBuf[0] = 0;
        }
        if (NativeButton("hostEmpty", "Host a Game", { -1, 40 }))
            tab = 1;
    }
    for (const auto& room : rooms) {
        if (!BrowserMatches(room, filters, searchBuf))
            continue;
        auto p = ImGui::GetCursorScreenPos();
        float cw = ImGui::GetContentRegionAvail().x;
        if (NativeButton(room.roomId.c_str(), "", { cw, 91 }, room.roomId == selectedId)) {
            selectedId = room.roomId;
            if (!room.teams.empty())
                snprintf(teamBuf, sizeof(teamBuf), "%s",
                         (!room.campaignTeam.empty() ? room.campaignTeam : room.teams.front()).c_str());
            if (!wide)
                detailsPage = true;
        }
        auto* draw = ImGui::GetWindowDrawList();
        Ellipsis(draw, { p.x + 12, p.y + 9 }, room.roomId, cw - 116, 20, IM_COL32(255, 247, 192, 255));
        const std::string count = std::to_string(room.players) + " / " + std::to_string(room.maxPlayers);
        Theme::Text(draw, { p.x + cw - 77, p.y + 9 }, count, 20, IM_COL32(221, 242, 255, 255));
        Ellipsis(draw, { p.x + 12, p.y + 35 },
                 room.storyTitle.empty() ? (room.playersInGame ? "Campaign not reported" : "Choosing a save")
                                         : room.storyTitle,
                 cw - 24, 17, IM_COL32(228, 239, 255, 255));
        const std::string line = room.questTitle.empty()
                                     ? std::to_string(room.playersInGame) + " in game  |  " +
                                           (room.pvpMode ? "PvP" : "Co-op") + "  |  " +
                                           (room.players >= room.maxPlayers ? "Full" : "Places open")
                                     : "Quest: " + room.questTitle + "  (" + std::to_string(room.questDone) + "/" +
                                           std::to_string(room.questTotal) + ")";
        Ellipsis(draw, { p.x + 12, p.y + 62 }, line, cw - 24, 16, IM_COL32(184, 212, 242, 255));
    }
    ImGui::EndChild();
    if (wide) {
        ImGui::SameLine();
        ImGui::BeginChild("partyDetails", { 0, 0 }, ImGuiChildFlags_AlwaysUseWindowPadding);
        selected = std::find_if(rooms.begin(), rooms.end(), [&](const auto& r) { return r.roomId == selectedId; });
        if (selected != rooms.end())
            DrawRoomDetails(*selected);
        else {
            Theme::Label("Choose Your Party", 24);
            ImGui::TextWrapped("Select a game to see its campaign, team and rules before joining.");
        }
        ImGui::EndChild();
    }
}
void ServerBrowserWindow::DrawRoomDetails(const BrowserRoom& room) {
    ImGui::BeginChild(
        "questSummary",
        { 0, std::max(80.f, ImGui::GetContentRegionAvail().y - (ImGui::GetContentRegionAvail().x < 400 ? 174 : 146)) },
        ImGuiChildFlags_None);
    ImGui::TextWrapped("%s", room.roomId.c_str());
    ImGui::TextWrapped("Main campaign: %s",
                       room.storyTitle.empty()
                           ? (room.playersInGame ? "Not reported by this game version" : "Waiting for a save")
                           : room.storyTitle.c_str());
    ImGui::Text("%d / %d players | %d in game", room.players, room.maxPlayers, room.playersInGame);
    if (!room.campaignPlayer.empty())
        ImGui::TextWrapped("From %s | Team %s", room.campaignPlayer.c_str(), room.campaignTeam.c_str());
    if (!room.questTitle.empty()) {
        ImGui::Spacing();
        ImGui::TextDisabled("TRACKED QUEST");
        ImGui::TextWrapped("%s", room.questTitle.c_str());
        ImGui::TextWrapped("%s | %d / %d objectives%s", room.chapter.c_str(), room.questDone, room.questTotal,
                           room.questState == 2 ? " | Ready to turn in" : "");
    }
    ImGui::Separator();
    ImGui::TextWrapped("%s | Difficulty: %s", room.pvpMode ? "PvP enabled" : "Co-op",
                       DifficultyName(room.difficultyMode));
    ImGui::TextWrapped("Your character: gear and story stay personal.");
    if (room.syncItemsAndFlags)
        ImGui::TextWrapped("This room offers legacy save sharing; your portable character stays protected.");
    bool different = std::any_of(room.versions.begin(), room.versions.end(),
                                 [](const auto& v) { return v != Anchor::Instance->clientVersion; });
    if (different)
        ImGui::TextWrapped("Different build. Matching builds are recommended.");
    if (!room.versions.empty())
        ImGui::TextWrapped("Build: %s", room.versions.front().c_str());
    ImGui::EndChild();
    ImGui::Separator();
    ImGui::TextDisabled("Your name / Team");
    char player[49];
    snprintf(player, sizeof(player), "%s", CVarGetString(CVAR_REMOTE_ANCHOR("Name"), ""));
    float half = (ImGui::GetContentRegionAvail().x - 8) / 2;
    ImGui::SetNextItemWidth(half);
    if (ImGui::InputTextWithHint("##player", "Player name", player, sizeof(player))) {
        CVarSetString(CVAR_REMOTE_ANCHOR("Name"), player);
        Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(half);
    if (ImGui::BeginCombo("##joinTeam", teamBuf)) {
        for (const auto& team : room.teams)
            if (ImGui::Selectable(team.c_str(), team == teamBuf))
                snprintf(teamBuf, sizeof(teamBuf), "%s", team.c_str());
        ImGui::EndCombo();
    }
    ImGui::TextWrapped("Same team + seed shares quest progress.");
    ImGui::BeginDisabled(!player[0] || !teamBuf[0] || room.players >= room.maxPlayers || queryFailed || querying);
    if (NativeButton("join",
                     room.players >= room.maxPlayers ? "Party Full"
                     : !player[0]                    ? "Enter Your Name"
                                                     : "Join Party",
                     { -1, 40 }))
        JoinRoom(room);
    ImGui::EndDisabled();
}
void ServerBrowserWindow::DrawHost() {
    Theme::Label("Gather an Adventuring Party", 25);
    char player[49];
    snprintf(player, sizeof(player), "%s", CVarGetString(CVAR_REMOTE_ANCHOR("Name"), ""));
    ImGui::TextDisabled("Player name / Team");
    const float half = (ImGui::GetContentRegionAvail().x - 8) / 2;
    ImGui::SetNextItemWidth(half);
    if (ImGui::InputTextWithHint("##hostPlayer", "Player name", player, sizeof(player))) {
        CVarSetString(CVAR_REMOTE_ANCHOR("Name"), player);
        Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(half);
    ImGui::InputTextWithHint("##hostTeam", "Team", teamBuf, sizeof(teamBuf));
    ImGui::TextDisabled("Room name");
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##roomName", "e.g. The Forest Watch", newRoomBuf, sizeof(newRoomBuf));
    ImGui::Checkbox("List this game publicly", &publicRoom);
    ImGui::BeginDisabled(!player[0] || !newRoomBuf[0] || !teamBuf[0]);
    if (NativeButton("create", "Create / Join Named Party", { -1, 42 })) {
        CVarSetInteger(CVAR_REMOTE_ANCHOR("RoomSettings.Listed"), publicRoom ? 1 : 0);
        CVarSetInteger(CVAR_REMOTE_ANCHOR("RoomSettings.PvpMode"), 0);
        CVarSetInteger(CVAR_REMOTE_ANCHOR("RoomSettings.SyncItemsAndFlags"), 0);
        ConnectTo({ "", browseHostBuf, (uint16_t)browsePort, newRoomBuf }, newRoomBuf);
    }
    ImGui::EndDisabled();
    ImGui::TextWrapped(publicRoom
                           ? "Your party will appear in Public Games. Other adventurers can join by selecting it."
                           : "Unlisted games can be joined by name. Unlisted does not add a password.");
    ImGui::TextWrapped("New parties start with co-op rules and personal items/story. Quest sharing works within your "
                       "team and seed. Joining an existing room keeps that room's rules.");
}

// ---------------------------------------------------------------------------
// Server management (address book)
// ---------------------------------------------------------------------------

void ServerBrowserWindow::DrawServerManager() {
    ImGui::SeparatorText("My Servers");

    std::vector<SavedServers::Entry> servers = SavedServers::Load();

    if (servers.empty()) {
        ImGui::TextDisabled("No saved servers yet - add one below");
    } else {
        if (ImGui::BeginTable("##savedServers", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Host", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Room");
            ImGui::TableSetupColumn("##actions", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFontSize() * 13.0f);
            ImGui::TableHeadersRow();

            for (size_t i = 0; i < servers.size(); i++) {
                auto& s = servers[i];
                ImGui::PushID(s.name.c_str());
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                if (ImGui::Selectable(s.name.c_str(), selectedServer == (int)i, ImGuiSelectableFlags_SpanAllColumns)) {
                    selectedServer = (int)i;
                }
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(s.host.c_str());
                ImGui::SameLine();
                ImGui::TextDisabled(":%d", s.port);
                ImGui::TableNextColumn();
                if (!s.roomId.empty()) {
                    ImGui::TextUnformatted(s.roomId.c_str());
                } else {
                    ImGui::TextDisabled("-");
                }
                ImGui::TableNextColumn();
                ImGui::BeginDisabled(Anchor::Instance->isEnabled);
                if (ImGui::Button("Connect")) {
                    ConnectTo(s, s.roomId);
                }
                ImGui::EndDisabled();
                ImGui::SameLine();
                if (ImGui::Button("Browse")) {
                    strncpy(browseHostBuf, s.host.c_str(), sizeof(browseHostBuf) - 1);
                    browsePort = s.port;
                    RefreshRooms();
                }
                ImGui::SameLine();
                if (ImGui::Button("X##del")) {
                    SavedServers::Remove(s.name);
                    if (selectedServer == (int)i) {
                        selectedServer = -1;
                    }
                }
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
    }

    // Add / edit form
    ImGui::Spacing();
    if (ImGui::CollapsingHeader("Add / Edit Server")) {
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        ImGui::InputTextWithHint("##name", "Name (e.g. Friday Night Crew)", nameBuf, sizeof(nameBuf));
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        ImGui::InputTextWithHint("##host", "Host (IP or hostname)", hostBuf, sizeof(hostBuf));
        ImGui::SetNextItemWidth(ImGui::GetFontSize() * 6);
        ImGui::InputInt("##port", &portBuf);
        if (portBuf <= 0 || portBuf > 65535) {
            portBuf = 43383;
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        ImGui::InputTextWithHint("##room", "Default room (optional)", roomBuf, sizeof(roomBuf));

        bool valid = strlen(nameBuf) > 0 && strlen(hostBuf) > 0;
        ImGui::BeginDisabled(!valid);
        if (ImGui::Button("Save Server")) {
            SavedServers::Entry e;
            e.name = nameBuf;
            e.host = hostBuf;
            e.port = (uint16_t)portBuf;
            e.roomId = roomBuf;
            SavedServers::Save(e);
            memset(nameBuf, 0, sizeof(nameBuf));
            memset(hostBuf, 0, sizeof(hostBuf));
            memset(roomBuf, 0, sizeof(roomBuf));
            portBuf = 43383;
        }
        ImGui::EndDisabled();
    }
}

// ---------------------------------------------------------------------------
// Room browser
// ---------------------------------------------------------------------------

void ServerBrowserWindow::UpdateElement() {
    // Collect finished background queries (safe cvar/ImGui work stays on main thread)
    if (querying && queryFuture.valid() && queryFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        RoomQueryResult res;
        try {
            res = queryFuture.get();
        } catch (const std::exception& error) { res.error = error.what(); } catch (...) {
            res.error = "Unable to query the relay.";
        }
        if (res.ok) {
            rooms = std::move(res.rooms);
            std::sort(rooms.begin(), rooms.end(), BrowserOrder);
            if (std::none_of(rooms.begin(), rooms.end(), [&](const auto& room) { return room.roomId == selectedId; })) {
                selectedId.clear();
                detailsPage = false;
            }
        }
        queryFailed = !res.ok;
        queryError = std::move(res.error);
        querying = false;
    }
}

void ServerBrowserWindow::RefreshRooms() {
    if (querying)
        return;
    nextRefresh = ImGui::GetTime() + 15;
    queryFailed = false;
    queryError.clear();
    if (strlen(browseHostBuf) == 0) {
        queryFailed = true;
        queryError = "no host entered";
        return;
    }
    if (queryHost != browseHostBuf || queryPort != browsePort) {
        rooms.clear();
        selectedId.clear();
        detailsPage = false;
    }
    queryHost = browseHostBuf;
    queryPort = browsePort;
    querying = true;
    std::string host = browseHostBuf;
    int port = browsePort;
    queryFuture =
        std::async(std::launch::async, [host, port]() { return QueryRoomListBlocking(host, (uint16_t)port); });
}

void ServerBrowserWindow::JoinRoom(const BrowserRoom& room) {
    SavedServers::Entry e;
    e.host = browseHostBuf;
    e.port = (uint16_t)browsePort;
    ConnectTo(e, room.roomId);
}

bool ServerBrowserWindow::LeaveServer(bool browse) {
    if (!CharacterTravel::Checkpoint(travelError)) return false;
    CVarClear(CVAR_REMOTE_ANCHOR("Enabled"));
    Anchor::Instance->Disable();
    Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
    if (browse) {
        tab = 0;
        detailsPage = false;
        RefreshRooms();
    } else SetOpen(false);
    return true;
}

void ServerBrowserWindow::ConnectTo(const SavedServers::Entry& entry, const std::string& roomId) {
    if (Anchor::Instance->isEnabled)
        return;
    if (std::string(CVarGetString(CVAR_REMOTE_ANCHOR("Name"), "")).empty()) {
        Notification::Emit({ .message = "Enter a player name before joining." });
        return;
    }
    if (!CharacterTravel::Checkpoint(travelError)) return;
    CVarSetInteger(CVAR_REMOTE_ANCHOR("PortableCharacters"), 1);
    CVarSetString(CVAR_REMOTE_ANCHOR("TeamId"), teamBuf[0] ? teamBuf : "default");
    // Configure cvars then enable Anchor exactly like the existing menu flow
    CVarSetString(CVAR_REMOTE_ANCHOR("Host"), entry.host.c_str());
    CVarSetInteger(CVAR_REMOTE_ANCHOR("Port"), entry.port);
    if (!roomId.empty()) {
        CVarSetString(CVAR_REMOTE_ANCHOR("RoomId"), roomId.c_str());
    }
    CVarSetInteger(CVAR_REMOTE_ANCHOR("Enabled"), 1);
    Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();

    Anchor::Instance->Enable();
}

const char* ServerBrowserWindow::DifficultyName(int mode) {
    switch (mode) {
        case 1:
            return "HP Scaling";
        case 2:
            return "Full (HP+Dmg+Aggro)";
        default:
            return "Off";
    }
}
