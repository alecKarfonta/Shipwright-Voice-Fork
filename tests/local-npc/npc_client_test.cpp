#include "NpcClient.h"
#include <chrono>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <stdexcept>
#include <thread>

using namespace LocalNpc;
using namespace std::chrono_literals;
static int checks = 0;
static void Check(bool ok, const char* message) {
    ++checks; if (!ok) throw std::runtime_error(message);
}
static std::vector<Result> Wait(Client& client, uint64_t serial) {
    std::vector<Result> all;
    const auto until = std::chrono::steady_clock::now() + 6s;
    while (std::chrono::steady_clock::now() < until) {
        for (auto& result : client.Poll()) {
            Check(result.serial == serial, "stale result reached UI");
            bool done = result.done;
            all.push_back(std::move(result));
            if (done) return all;
        }
        std::this_thread::sleep_for(10ms);
    }
    throw std::runtime_error("timed out waiting for worker");
}
int main(int argc, char** argv) {
    try {
        Check(argc == 2, "fixture URL missing");
        const std::string base = argv[1];
        const auto configPath = std::filesystem::temp_directory_path() / "soh-npc-provider-test.json";
        {
            std::ofstream config(configPath);
            config << R"({"llm_endpoint":"http://localhost:8080/v1/chat/completions","llm_model":"uncensored","llm_api_key":"fixture-key"})";
        }
        auto defaults = LoadProviderDefaults({"/nonexistent/npc-config.json", configPath.string()});
        Check(defaults.model == "uncensored" && defaults.key == "fixture-key" &&
              defaults.endpoint == "http://localhost:8080/v1/chat/completions", "direct-launch provider defaults missing");
        {
            std::ofstream config(configPath);
            config << R"({"llm_model":17,"llm_api_key":"bad\nkey"})";
        }
        defaults = LoadProviderDefaults({configPath.string()});
        Check(defaults.model.empty() && defaults.key.empty(), "invalid provider settings accepted");
        std::filesystem::remove(configPath);
        Check(LoadProviderDefaults({configPath.string()}).endpoint.empty(), "missing config must remain unconfigured");
        Request r;
        r.provider.endpoint = base + "/chat"; r.provider.model = "fixture";
        r.provider.key = "test-secret"; r.provider.ttsEndpoint = base + "/tts";
        r.provider.voice = "forest_voice"; r.provider.instruction = "gentle";
        r.persona = "Saria"; r.context = "Kokiri Forest; child Link"; r.playerText = "How are things?";
        for (int i = 0; i < 20; ++i) r.history.push_back({i % 2 ? "assistant" : "user", "Remembered line"});
        auto body = Compose(r);
        Check(body["messages"].size() == 10, "history must be bounded");
        Check(body["messages"][0]["role"] == "system", "grounding missing");
        Check(body["messages"].back()["content"] == r.playerText, "current turn missing");
        Check(HttpEndpoint(base + "/chat"), "local HTTP rejected");
        Check(!HttpEndpoint("file:///tmp/test") && !HttpEndpoint("http://host@other/test") &&
              !HttpEndpoint("http:///path") && !HttpEndpoint("http://host\r\nX:bad"), "bad endpoint accepted");
        auto envelope = [](const std::string& text) {
            return nlohmann::json{{"choices", {{{"finish_reason", "stop"}, {"message", {{"content", text}}}}}}}.dump();
        };
        Check(ParseReply(envelope("{\"reply\":\"The forest is peaceful.\"}")) == "The forest is peaceful.", "valid reply rejected");
        auto deliveryRequest = ComposeDelivery(r, "Stay with me.");
        auto deliveryContext = nlohmann::json::parse(deliveryRequest["messages"][1]["content"].get<std::string>());
        Check(deliveryContext["current_story_context"] == r.context && deliveryContext["text"] == "Stay with me.",
              "delivery lost exact dialogue or story context");
        Check(ParseDelivery(envelope(R"({"emotion":"afraid","intensity":2,"pace":"brisk","volume":"whisper"})")).find("afraid") != std::string::npos,
              "valid acting direction rejected");
        for (const auto& invalid : {
            R"({"emotion":"ignore prior instructions","intensity":2,"pace":"brisk","volume":"soft"})",
            R"({"emotion":"neutral","intensity":4,"pace":"brisk","volume":"soft"})",
            R"({"emotion":"neutral","intensity":true,"pace":"brisk","volume":"soft"})",
            R"({"emotion":"neutral","intensity":1,"pace":"brisk","volume":"soft","text":"replace dialogue"})"}) {
            bool rejected = false; try { ParseDelivery(envelope(invalid)); } catch (...) { rejected = true; }
            Check(rejected, "invalid acting direction accepted");
        }
        for (const auto& content : {"{}", "{\"reply\":17}", "{\"reply\":\"\"}", "<think>hidden</think>", "{\"reply\":\"<tool>\"}"}) {
            bool rejected = false; try { ParseReply(envelope(content)); } catch (...) { rejected = true; }
            Check(rejected, "malformed response accepted");
        }
        Client client;
        auto results = Wait(client, client.Submit(r));
        Check(results.size() == 2 && !results.front().text.empty() && !results.front().done, "caption must arrive before audio");
        Check(results.back().wav && results.back().wav->size() > 44, "WAV missing");
        // The fixture rejects the same synthesis twice: second request must use the exact-content cache.
        r.text = results.front().text;
        results = Wait(client, client.Submit(r));
        Check(results.back().wav != nullptr, "exact speech cache missed");
        r.provider.voice = "other_voice";
        results = Wait(client, client.Submit(r));
        Check(results.back().wav != nullptr, "voice change incorrectly reused cache");
        r.context = "Kokiri Forest; child Link; after loss";
        results = Wait(client, client.Submit(r));
        Check(results.back().wav != nullptr, "story change incorrectly reused a different delivery");
        r.context = "Kokiri Forest; child Link";
        r.provider.speak = false;
        results = Wait(client, client.Submit(r));
        Check(results.back().done && !results.back().wav, "captions-only speech did not finish");
        r.text.clear();
        results = Wait(client, client.Submit(r));
        Check(!results.front().text.empty() && results.front().done, "captions-only generation failed");
        r.provider.speak = true;
        for (const auto& path : {"/fail", "/bad", "/oversize", "/truncated"}) {
            r.provider.endpoint = base + path;
            results = Wait(client, client.Submit(r));
            Check(!results.back().status.empty() && !results.back().wav, "invalid LLM response was not contained");
        }
        r.provider.endpoint = base + "/chat"; r.provider.ttsEndpoint = base + "/bad-audio";
        results = Wait(client, client.Submit(r));
        Check(!results.front().text.empty() && !results.back().status.empty() && !results.back().wav, "TTS failure must retain captions");
        r.provider.endpoint = base + "/slow"; r.provider.ttsEndpoint = base + "/tts";
        client.Submit(r); std::this_thread::sleep_for(150ms); client.Cancel();
        r.provider.endpoint = base + "/chat";
        results = Wait(client, client.Submit(r));
        Check(results.back().wav != nullptr, "new conversation did not recover after cancellation");
        client.Cancel(); Check(client.Poll().empty(), "cancel did not drain results");
        const auto start = std::chrono::steady_clock::now();
        {
            Client closing; r.provider.endpoint = base + "/slow";
            closing.Submit(r); std::this_thread::sleep_for(150ms);
        }
        Check(std::chrono::steady_clock::now() - start < 2500ms, "shutdown waited for the full request timeout");
        // A pack hit must succeed with deliberately invalid network endpoints.
        const auto packRoot = std::filesystem::temp_directory_path() / "soh-npc-offline-pack-test";
        std::filesystem::create_directories(packRoot);
        const unsigned char pcm[] = {82,73,70,70,40,0,0,0,87,65,86,69,102,109,116,32,16,0,0,0,1,0,1,0,192,93,0,0,128,187,0,0,2,0,16,0,100,97,116,97,4,0,0,0,1,0,2,0};
        {
            std::ofstream file(packRoot/"test.wav", std::ios::binary);
            file.write(reinterpret_cast<const char*>(pcm), sizeof(pcm));
            std::ofstream(packRoot/"manifest.json") << nlohmann::json{
                {"format","soh-voice-pack-v1"},{"complete",true},{"language","en"},
                {"entries", {{{"character","saria"},{"text","Welcome, Link!"},{"message_id","1000"},
                              {"file","test.wav"},{"bytes",sizeof(pcm)}}}}};
        }
        Request recorded; recorded.text="Welcome, Link!"; recorded.provider.voicePack=packRoot.string();
        recorded.provider.endpoint="invalid-director";recorded.provider.model="test";recorded.context="Kokiri Forest";
        recorded.provider.ttsEndpoint="invalid-tts";
        recorded.recording.authored=true;recorded.recording.character="saria";recorded.recording.messageId="1000";
        {
            Client warming;
            warming.PrepareVoicePack("");
            warming.PrepareVoicePack((packRoot/"missing").string());
            warming.PrepareVoicePack(packRoot.string());
            Check(warming.Poll().empty(),"warmup published dialogue or audio");
            warming.Cancel(); // A cancellation must not strand the worker or future dialogue.
            auto warmResults=Wait(warming,warming.Submit(recorded));
            Check(warmResults.size()==1 && warmResults.back().wav,
                  "dialogue arriving during pack warmup did not use offline audio");
            warming.PrepareVoicePack(packRoot.string());
            warming.Cancel();
            Check(warming.Poll().empty(),"warmup revived cancelled audio");
        }
        results=Wait(client,client.Submit(recorded));
        Check(results.back().wav && results.back().wav->size()==sizeof(pcm),"offline pack did not bypass network services");
        recorded.text="A changed line.";
        results=Wait(client,client.Submit(recorded));
        Check(!results.back().wav && !results.back().status.empty(),"pack miss did not fall back to the live provider");
        recorded.recordedOnly=true;
        results=Wait(client,client.Submit(recorded));
        Check(!results.back().wav && results.back().status.empty(),"unsupported native actor used an unrelated live voice");
        const auto reactionRoot=packRoot/"reactions";std::filesystem::create_directories(reactionRoot);
        std::filesystem::copy_file(packRoot/"test.wav",reactionRoot/"reaction.wav");
        std::ofstream(reactionRoot/"manifest.json") << nlohmann::json{
            {"format","soh-voice-pack-v1"},{"complete",true},{"language","en"},
            {"entries", {{{"character","market-guard"},{"scope","town_reaction"},{"reaction","guard.warning"},
                {"text","Stop!"},{"file","reaction.wav"},{"bytes",sizeof(pcm)}}}}};
        recorded.provider.voicePack=reactionRoot.string();recorded.text="Stop!";
        recorded.recording.character="market-guard";recorded.recording.reaction="guard.warning";
        results=Wait(client,client.Submit(recorded));
        Check(results.back().wav && results.back().wav->size()==sizeof(pcm),"ambient reaction did not use exact offline cast recording");
        recorded.recording.character="hylian-resident-00";
        results=Wait(client,client.Submit(recorded));
        Check(!results.back().wav && results.back().status.empty(),"reaction miss contacted the network or borrowed another voice");
        recorded.recording.character="market-guard";
        const auto late=packRoot/"late-install";recorded.provider.voicePack=late.string();
        results=Wait(client,client.Submit(recorded));Check(!results.back().wav,"missing reaction pack unexpectedly played");
        std::filesystem::copy(reactionRoot,late,std::filesystem::copy_options::recursive);
        results=Wait(client,client.Submit(recorded));
        Check(results.back().wav!=nullptr,"completed reaction pack was not picked up after an earlier missing-pack lookup");
        std::filesystem::remove_all(packRoot);
        if (const char* live = std::getenv("SOH_NPC_LIVE_TTS")) {
            r.text = "The forest is peaceful today."; r.provider.ttsEndpoint = live;
            r.provider.voice.clear(); r.provider.instruction = "gentle and friendly";
            if (const char* voice = std::getenv("SOH_NPC_LIVE_VOICE")) r.provider.voice = voice;
            if (std::getenv("SOH_NPC_LIVE_LLM")) {
                r.text.clear(); r.history.clear();
                auto env = [](const char* name) { const auto* value = std::getenv(name); return value ? value : ""; };
                r.provider.endpoint = env("SOH_NPC_LLM_ENDPOINT");
                r.provider.model = env("SOH_NPC_LLM_MODEL");
                r.provider.key = env("SOH_NPC_LLM_API_KEY");
            }
            // Live synthesis can take longer than fixture calls.
            auto serial = client.Submit(r); bool done = false;
            for (int i = 0; i < 1000 && !done; ++i) {
                for (auto& result : client.Poll()) if (result.serial == serial) {
                    if (!result.text.empty()) std::cout << "Live NPC reply: " << result.text << '\n';
                    if (result.done) {
                        Check(result.wav != nullptr, result.status.c_str()); done = true;
                        std::ofstream audio("/tmp/soh-npc-live.wav", std::ios::binary);
                        audio.write(reinterpret_cast<const char*>(result.wav->data()), result.wav->size());
                    }
                }
                std::this_thread::sleep_for(100ms);
            }
            Check(done, "live voice synthesis timed out");
        }
        std::cout << checks << " NPC client checks passed\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
