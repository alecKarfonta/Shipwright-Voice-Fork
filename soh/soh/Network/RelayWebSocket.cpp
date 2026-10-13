#include "RelayWebSocket.h"
#include <chrono>
#include <mutex>
#include <thread>
#ifdef SOH_PUBLIC_RELAY
#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXWebSocket.h>
#endif
namespace PublicRelay {
struct Socket::Impl {
    std::mutex mutex;
    std::vector<Event> events;
    size_t queued = 0;
#ifdef SOH_PUBLIC_RELAY
    ix::WebSocket socket;
#endif
};
bool Supported() {
#ifdef SOH_PUBLIC_RELAY
    return true;
#else
    return false;
#endif
}
Socket::Socket() : impl(std::make_unique<Impl>()) {
}
Socket::~Socket() {
    Stop();
}
void Socket::Start(const std::string& url) {
#ifdef SOH_PUBLIC_RELAY
    static std::once_flag init;
    std::call_once(init, [] { ix::initNetSystem(); });
    auto& ws = impl->socket;
    ws.setUrl(url);
    ws.setPingInterval(25);
    ws.setHandshakeTimeout(8);
    const auto start = url.find("://") + 3;
    const auto authority = url.substr(start, url.find('/', start) - start);
    ws.setExtraHeaders({ { "Origin", (url.starts_with("wss://") ? "https://" : "http://") + authority } });
    ws.setMinWaitBetweenReconnectionRetries(1000);
    ws.setMaxWaitBetweenReconnectionRetries(10000);
    ws.disablePerMessageDeflate();
    ws.setOnMessageCallback([this](const ix::WebSocketMessagePtr& msg) {
        Event event;
        switch (msg->type) {
            case ix::WebSocketMessageType::Open:
                event = { Kind::Open, {} };
                break;
            case ix::WebSocketMessageType::Message:
                event = { Kind::Data, msg->str };
                break;
            case ix::WebSocketMessageType::Close:
                event = { Kind::Closed, msg->closeInfo.reason };
                break;
            case ix::WebSocketMessageType::Error:
                event = { Kind::Error, msg->errorInfo.reason };
                break;
            default:
                return;
        }
        std::lock_guard lock(impl->mutex);
        if (impl->queued + event.data.size() > 4 * 1024 * 1024 || impl->events.size() > 1024) {
            impl->events.clear();
            impl->queued = 0;
            impl->events.push_back({ Kind::Error, "Relay receive limit exceeded" });
            impl->socket.close();
            return;
        }
        impl->queued += event.data.size();
        impl->events.push_back(std::move(event));
    });
    ws.start();
#else
    impl->events.push_back({ Kind::Error, "This build does not include public relay support." });
#endif
}
void Socket::Stop() {
#ifdef SOH_PUBLIC_RELAY
    impl->socket.stop();
#endif
}
bool Socket::Send(const std::string& bytes) {
#ifdef SOH_PUBLIC_RELAY
    if (impl->socket.bufferedAmount() > 4 * 1024 * 1024) {
        impl->socket.close();
        return false;
    }
    return impl->socket.sendBinary(bytes).success;
#else
    return false;
#endif
}
std::vector<Socket::Event> Socket::Poll() {
    std::lock_guard lock(impl->mutex);
    std::vector<Event> events;
    events.swap(impl->events);
    impl->queued = 0;
    return events;
}
std::string Query(const std::string& url, const std::string& request, std::string& error) {
    Socket socket;
    socket.Start(url);
    std::string received;
    auto until = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (std::chrono::steady_clock::now() < until) {
        for (auto& e : socket.Poll()) {
            if (e.kind == Socket::Kind::Open) {
                if (!socket.Send(request + std::string(1, '\0'))) {
                    error = "Could not request public games.";
                    return {};
                }
            } else if (e.kind == Socket::Kind::Data) {
                received += e.data;
                auto end = received.find('\0');
                if (end != std::string::npos)
                    return received.substr(0, end);
                if (received.size() > 2 * 1024 * 1024) {
                    error = "Room list too large.";
                    return {};
                }
            } else {
                error = e.data.empty() ? "The relay closed the connection." : e.data;
                return {};
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    error = "The public relay did not respond. Try Refresh.";
    return {};
}
} // namespace PublicRelay
