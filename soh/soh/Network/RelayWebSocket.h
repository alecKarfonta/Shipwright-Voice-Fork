#pragma once
#include <memory>
#include <string>
#include <vector>

namespace PublicRelay {
inline constexpr char Endpoint[] = "wss://mlapi.us/z64/relay/ws";
inline bool IsWebSocket(const std::string& host) {
    return host.starts_with("wss://") || host.starts_with("ws://");
}
bool Supported();
// Events are consumed by Network's existing worker; callbacks never touch game state.
class Socket {
  public:
    enum class Kind { Open, Data, Closed, Error };
    struct Event {
        Kind kind;
        std::string data;
    };
    Socket();
    ~Socket();
    void Start(const std::string& url);
    void Stop();
    bool Send(const std::string& bytes);
    std::vector<Event> Poll();

  private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
std::string Query(const std::string& url, const std::string& request, std::string& error);
} // namespace PublicRelay
