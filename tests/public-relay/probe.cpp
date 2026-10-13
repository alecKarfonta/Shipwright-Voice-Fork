#include "RelayWebSocket.h"
#include <chrono>
#include <iostream>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <thread>
using Json = nlohmann::json;
using Clock = std::chrono::steady_clock;
struct Client {
  PublicRelay::Socket socket;
  std::string pending;
  Client(const std::string &url) {
    socket.Start(url);
    auto deadline = Clock::now() + std::chrono::seconds(12);
    while (Clock::now() < deadline) {
      for (auto &event : socket.Poll()) {
        if (event.kind == PublicRelay::Socket::Kind::Open)
          return;
        if (event.kind == PublicRelay::Socket::Kind::Error)
          throw std::runtime_error(event.data);
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    throw std::runtime_error("connect timeout");
  }
  void Send(Json message) {
    if (!socket.Send(message.dump() + std::string(1, '\0')))
      throw std::runtime_error("send failed");
  }
  Json Read(const std::string &type) {
    auto deadline = Clock::now() + std::chrono::seconds(8);
    while (Clock::now() < deadline) {
      for (auto &event : socket.Poll()) {
        if (event.kind == PublicRelay::Socket::Kind::Data)
          pending += event.data;
        else if (event.kind == PublicRelay::Socket::Kind::Error ||
                 event.kind == PublicRelay::Socket::Kind::Closed)
          throw std::runtime_error(event.data);
      }
      for (auto end = pending.find('\0'); end != std::string::npos;
           end = pending.find('\0')) {
        auto packet = Json::parse(pending.substr(0, end));
        pending.erase(0, end + 1);
        if (packet.value("type", "") == type)
          return packet;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    throw std::runtime_error("receive timeout: " + type);
  }
  void Join(const std::string &room, const char *name) {
    Send({{"type", "HANDSHAKE"},
          {"roomId", room},
          {"roomState", {{"listed", true}}},
          {"clientState",
           {{"name", name},
            {"teamId", "campaign"},
            {"seed", 0},
            {"isSaveLoaded", true},
            {"sceneNum", 85},
            {"curRoomNum", 0},
            {"campaign",
             {{"storyStage", 3},
              {"questId", 22},
              {"questState", 1},
              {"completedObjectives", 0}}},
            {"clientVersion", "public-probe"}}}});
    Read("ALL_CLIENT_STATE");
  }
};
int main(int argc, char **argv) {
  try {
    std::string url = argc > 1 ? argv[1] : PublicRelay::Endpoint;
    std::string room =
        "Relay verification " +
        std::to_string(
            std::chrono::system_clock::now().time_since_epoch().count());
    Client a(url), b(url);
    a.Join(room, "Verification A");
    b.Join(room, "Verification B");
    std::string error;
    auto directory =
        Json::parse(PublicRelay::Query(url, R"({"type":"LIST_ROOMS"})", error));
    bool listed = false;
    for (auto &entry : directory["rooms"])
      if (entry["roomId"] == room && entry["players"] == 2) {
        listed = true;
        if (entry["campaign"]["storyTitle"] != "Awaken the Forest Sage" ||
            entry["campaign"]["questTitle"] != "A Friend Beyond the Door" ||
            entry["campaign"]["player"] != "Verification A")
          throw std::runtime_error("campaign directory metadata mismatch");
      }
    if (!listed)
      throw std::runtime_error("two-player room missing");
    a.Send({{"type", "ARPG_QUEST"},
            {"version", 1},
            {"op", "share"},
            {"id", 1},
            {"state", {{"size", 1}}}});
    a.Read("ARPG_QUEST");
    b.Read("ARPG_QUEST");
    b.Send({{"type", "ARPG_QUEST"}, {"version", 1}, {"op", "join"}, {"id", 1}});
    if (b.Read("ARPG_QUEST")["quest"]["size"] != 2)
      throw std::runtime_error("quest party scaling failed");
    a.Read("ARPG_QUEST");
    a.Send({{"type", "ARPG_QUEST"},
            {"version", 1},
            {"op", "advance"},
            {"id", 1},
            {"objective", 0},
            {"seq", 1},
            {"amount", 3}});
    if (a.Read("ARPG_QUEST")["quest"]["progress"][0] != 3 ||
        b.Read("ARPG_QUEST")["quest"]["progress"][0] != 3)
      throw std::runtime_error("quest progress mismatch");
    b.Send({{"type", "ARPG_QUEST"},
            {"version", 1},
            {"op", "advance"},
            {"id", 1},
            {"objective", 0},
            {"seq", 1},
            {"amount", 3}});
    if (a.Read("ARPG_QUEST")["quest"]["progress"][0] != 6 ||
        b.Read("ARPG_QUEST")["quest"]["progress"][0] != 6)
      throw std::runtime_error("quest completion mismatch");
    std::cout
        << "PASS: verified TLS/WebSocket connection, public room discovery, "
           "campaign metadata, two-client joining, shared party size and "
           "six-kill completion.\n";
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
