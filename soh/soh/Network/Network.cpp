#include "Network.h"
#include <spdlog/spdlog.h>

// MARK: - Public

void Network::Enable(const char* host, uint16_t port) {
    if (isEnabled) {
        return;
    }

    if (receiveThread.joinable())
        receiveThread.join();
    relaySocket.reset();
    if (PublicRelay::IsWebSocket(host)) {
        if (!PublicRelay::Supported()) {
            SPDLOG_ERROR("This build does not support public relays");
            return;
        }
        relaySocket = std::make_unique<PublicRelay::Socket>();
        relaySocket->Start(host);
    } else if (SDLNet_ResolveHost(&networkAddress, host, port) == -1) {
        SPDLOG_ERROR("[Network] SDLNet_ResolveHost: {}", SDLNet_GetError());
        return;
    }

    isEnabled = true;

    receiveThread = std::thread(&Network::ReceiveFromServer, this);
}

void Network::Disable() {
    isEnabled = false;
    if (receiveThread.joinable())
        receiveThread.join();
    if (relaySocket) {
        relaySocket->Stop();
        relaySocket.reset();
    }
}

void Network::OnIncomingData(char payload[512]) {
}

void Network::OnIncomingJson(nlohmann::json payload) {
}

void Network::OnConnected() {
}

void Network::OnDisconnected() {
}

void Network::ProcessOutgoingPackets() {
}

void Network::SendDataToRemote(const char* payload) {
    SPDLOG_DEBUG("[Network] Sending data: {}", payload);
    if (relaySocket) {
        relaySocket->Send(std::string(payload) + std::string(1, '\0'));
        return;
    }
    if (networkSocket)
        SDLNet_TCP_Send(networkSocket, payload, static_cast<int>(strlen(payload) + 1));
}

void Network::SendJsonToRemote(nlohmann::json payload) {
    SendDataToRemote(payload.dump().c_str());
}

// MARK: - Private

void Network::ReceiveFromServer() {
    if (relaySocket) {
        ReceiveWebSocket();
        return;
    }
    while (isEnabled) {
        while (!isConnected && isEnabled) {
            SPDLOG_TRACE("[Network] Attempting to make connection to server...");
            networkSocket = SDLNet_TCP_Open(&networkAddress);

            if (networkSocket) {
                isConnected = true;
                receivedData.clear();
                SPDLOG_INFO("[Network] Connection to server established!");

                OnConnected();
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
        }

        SDLNet_SocketSet socketSet = SDLNet_AllocSocketSet(1);
        if (networkSocket) {
            SDLNet_TCP_AddSocket(socketSet, networkSocket);
        }

        // Listen to socket messages
        while (isConnected && networkSocket && isEnabled) {
            // we check first if socket has data, to not block in the TCP_Recv
            int socketsReady = SDLNet_CheckSockets(socketSet, 5);

            if (socketsReady == -1) {
                SPDLOG_ERROR("[Network] SDLNet_CheckSockets: {}", SDLNet_GetError());
                break;
            }

            // Always process outgoing packets
            ProcessOutgoingPackets();

            if (socketsReady == 0) {
                // No incoming data
                continue;
            }

            char remoteDataReceived[512];
            memset(remoteDataReceived, 0, sizeof(remoteDataReceived));
            int len = SDLNet_TCP_Recv(networkSocket, &remoteDataReceived, sizeof(remoteDataReceived));
            if (!len || !networkSocket || len == -1) {
                SPDLOG_ERROR("[Network] SDLNet_TCP_Recv: {}", SDLNet_GetError());
                break;
            }

            HandleRemoteData(remoteDataReceived);

            ConsumeData(std::string(remoteDataReceived, len));
        }

        if (socketSet) {
            SDLNet_FreeSocketSet(socketSet);
        }

        if (isConnected) {
            SDLNet_TCP_Close(networkSocket);
            networkSocket = nullptr;
            isConnected = false;
            receivedData.clear();
            OnDisconnected();
            SPDLOG_INFO("[Network] Ending receiving thread...");
        }
    }
}

void Network::HandleRemoteData(char payload[512]) {
    OnIncomingData(payload);
}

void Network::HandleRemoteJson(std::string payload) {
    SPDLOG_DEBUG("[Network] Received json: {}", payload);
    nlohmann::json jsonPayload;
    try {
        jsonPayload = nlohmann::json::parse(payload);
    } catch (const std::exception& e) {
        SPDLOG_ERROR("[Network] Failed to parse json: \n{}\n{}\n", payload, e.what());
        return;
    }

    try {
        OnIncomingJson(jsonPayload);
    } catch (const std::exception& e) {
        SPDLOG_ERROR("[Network] Exception handling incoming JSON: {}", e.what());
    } catch (...) { SPDLOG_ERROR("[Network] Unknown exception handling incoming JSON"); }
}

void Network::ConsumeData(const std::string& bytes) {
    receivedData += bytes;
    if (receivedData.size() > 4 * 1024 * 1024) {
        receivedData.clear();
        isEnabled = false;
        return;
    }
    for (auto end = receivedData.find('\0'); end != std::string::npos; end = receivedData.find('\0')) {
        auto packet = receivedData.substr(0, end);
        receivedData.erase(0, end + 1);
        HandleRemoteJson(packet);
    }
}
void Network::ReceiveWebSocket() {
    while (isEnabled) {
        for (auto& event : relaySocket->Poll()) {
            if (event.kind == PublicRelay::Socket::Kind::Open) {
                receivedData.clear();
                isConnected = true;
                OnConnected();
            } else if (event.kind == PublicRelay::Socket::Kind::Data) {
                if (isConnected)
                    ConsumeData(event.data);
            } else {
                if (isConnected.exchange(false)) {
                    receivedData.clear();
                    OnDisconnected();
                }
                if (event.kind == PublicRelay::Socket::Kind::Error)
                    SPDLOG_WARN("[Public relay] {}", event.data);
            }
        }
        if (isConnected)
            ProcessOutgoingPackets();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    relaySocket->Stop();
    if (isConnected.exchange(false))
        OnDisconnected();
    receivedData.clear();
}
