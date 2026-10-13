#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>
#include <nlohmann/json.hpp>

namespace LocalNpc {
struct Provider {
    std::string endpoint, model, key;
    std::string ttsEndpoint = "http://192.168.50.64:8013/tts";
    std::string voice, instruction, language = "en";
    bool speak = true;
};
struct Turn { std::string role, text; };
struct Request {
    Provider provider;
    std::string persona, context, playerText, text;
    std::vector<Turn> history;
};
struct Result {
    uint64_t serial = 0;
    std::string text, status;
    std::shared_ptr<const std::vector<uint8_t>> wav;
    bool done = false;
};

nlohmann::json Compose(const Request& request);
std::string ParseReply(const std::string& body);
bool HttpEndpoint(const std::string& url);
Provider LoadProviderDefaults(const std::vector<std::string>& paths);

// One worker, one replaceable pending request. All game/SDL state stays on the main thread.
class Client {
  public:
    Client();
    ~Client();
    uint64_t Submit(Request request);
    uint64_t Cancel();
    std::vector<Result> Poll();
  private:
    struct Job { uint64_t serial; Request request; };
    void Run();
    void Publish(Result result);
    std::string Post(const Job& job, const std::string& endpoint, const nlohmann::json& body,
                     size_t limit, long timeout, bool authorized);
    std::atomic<uint64_t> serial{0};
    std::atomic<bool> stopping{false};
    std::mutex mutex;
    std::condition_variable ready;
    std::optional<Job> pending;
    std::vector<Result> results;
    std::thread worker;
};
}
