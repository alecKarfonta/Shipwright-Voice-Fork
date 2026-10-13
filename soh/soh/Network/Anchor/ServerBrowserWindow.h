#pragma once

// [TIME] | [INFO] | ServerBrowserWindow.h | Multiplayer server browser & manager

#include <future>
#include <string>
#include <vector>

#include <ship/window/gui/GuiWindow.h>

#include "soh/Network/Anchor/SavedServers.h"

#include "ServerBrowserModel.h"

// Result of a background LIST_ROOMS query (filled on worker thread)
struct RoomQueryResult {
    bool ok = false;
    std::string error;
    std::vector<BrowserRoom> rooms;
};

class ServerBrowserWindow : public Ship::GuiWindow {
  public:
    using GuiWindow::GuiWindow;

    void InitElement() override;
    void Draw() override;
    void DrawElement() override;
    void UpdateElement() override;

  private:
    // -- Server management state --
    char nameBuf[64] = "";
    char hostBuf[128] = "";
    int portBuf = 43383;
    char roomBuf[64] = "";
    int selectedServer = -1; // index into the saved list snapshot

    // -- Browsing state --
    char browseHostBuf[128] = "";
    int browsePort = 43383;
    char newRoomBuf[65] = "";
    char teamBuf[49] = "default";
    bool publicRoom = true;
    bool fileSelect = false, fileSelectReady = false, captured = false, focusPending = false;
    bool detailsPage = false;
    bool popupOpenLastFrame = false;
    int tab = 0;
    char searchBuf[129] = "";
    BrowserFilters filters;
    double nextRefresh = 0, releaseInputAt = 0;
    std::string selectedId;
    std::string travelError;
    bool LeaveServer(bool browse);
    void SetOpen(bool open);
    void Capture(bool active);
    void DrawNativeBrowser();
    void DrawPublicGames();
    void DrawRoomDetails(const BrowserRoom& room);
    void DrawHost();
    bool firstDraw = true;
    bool querying = false;
    bool queryFailed = false;
    std::string queryError;
    std::string queryHost;
    int queryPort = 0;
    std::vector<BrowserRoom> rooms;
    std::future<struct RoomQueryResult> queryFuture;

    void DrawServerManager();
    void DrawRoomBrowser();
    void RefreshRooms();
    void JoinRoom(const BrowserRoom& room);
    void ConnectTo(const SavedServers::Entry& entry, const std::string& roomId);
    static const char* DifficultyName(int mode);
};
