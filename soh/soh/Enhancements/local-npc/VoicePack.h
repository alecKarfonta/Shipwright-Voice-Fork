#pragma once
// Portable recorded-dialogue pack. Used only by Client's background worker.
#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <regex>
#include <string>
#include <unordered_map>
#include <vector>
#include <nlohmann/json.hpp>

namespace LocalNpc {
struct PackQuery {
    std::string messageId, character, age, playerName;
    std::string reaction;
    bool authored = false;
    // Authored quest replies may wrap the recorded line in a title and live rewards.
    bool quest = false;
};
class VoicePack {
  public:
    static std::string Canonical(const std::string& text) {
        std::string out;
        bool space = false;
        for (unsigned char c : text) {
            if (c == '\'') continue;
            if (std::isalnum(c) || c >= 128) {
                if (space && !out.empty()) out += ' ';
                out += c < 128 ? static_cast<char>(std::tolower(c)) : static_cast<char>(c);
                space = false;
            } else space = true;
        }
        return out;
    }
    bool Load(const std::string& directory) {
        entries.clear(); byMessage.clear(); byCharacter.clear(); byReaction.clear(); root.clear();
        try {
            auto base = std::filesystem::canonical(directory);
            auto manifest = base / "manifest.json";
            if (std::filesystem::file_size(manifest) > 32 * 1024 * 1024) return false;
            std::ifstream input(manifest);
            auto data = nlohmann::json::parse(input);
            if (data.at("format") != "soh-voice-pack-v1" || data.at("language") != "en" ||
                !data.at("complete").get<bool>() || data.at("entries").size() > 20000) return false;
            root = base;
            for (const auto& j : data.at("entries")) {
                Entry e;
                e.character = j.at("character"); e.file = j.at("file");
                e.message = j.value("message_id", ""); e.age = j.value("age", "unknown");
                e.quest = j.value("scope", "") == "authored_quest";
                e.kind = j.value("kind", "speech");
                e.text = Canonical(j.at("text").get<std::string>());
                e.match = Canonical(j.value("match_text", j.at("text").get<std::string>()));
                e.bytes = j.at("bytes").get<size_t>();
                if (e.text.empty() || e.match.empty() || e.text.size() > 6000 || e.bytes > MaxBytes || !Safe(e.file))
                    throw std::runtime_error("Invalid voice pack entry");
                const auto pattern = j.value("template", "");
                if (!pattern.empty()) {
                    // Only the exporter’s numeric placeholders become wildcards. All
                    // other text is canonical literal text, never user-provided regex.
                    std::string rx;
                    size_t start = 0;
                    while (start < pattern.size()) {
                        auto open = pattern.find('{', start);
                        if (open == std::string::npos) { rx += Escape(Canonical(pattern.substr(start))); break; }
                        auto close = pattern.find('}', open);
                        if (close == std::string::npos) throw std::runtime_error("Invalid numeric template");
                        rx += Escape(Canonical(pattern.substr(start, open-start))) + " *[0-9]+( [0-9]+)* *";
                        start = close + 1;
                    }
                    e.pattern = std::make_shared<std::regex>("^("+rx+")($| )");
                }
                size_t index = entries.size();
                entries.push_back(std::move(e));
                if (j.value("scope", "") == "town_reaction")
                    byReaction[j.at("reaction").get<std::string>() + "\n" + entries.back().character].push_back(index);
                else if (entries.back().quest) byCharacter[entries.back().character].push_back(index);
                else byMessage[entries.back().message].push_back(index);
            }
            return true;
        } catch (...) { entries.clear(); byMessage.clear(); byCharacter.clear(); byReaction.clear(); root.clear(); return false; }
    }
    std::vector<std::string> Select(const PackQuery& q, const std::string& spoken) const {
        if (!q.authored || root.empty()) return {};
        auto text = Canonical(spoken);
        if (!q.reaction.empty()) {
            auto group = byReaction.find(q.reaction + "\n" + q.character);
            if (group == byReaction.end()) return {};
            for (size_t i : group->second) {
                const auto& e = entries[i];
                if ((e.age == "unknown" || e.age == q.age) && e.text == text) return {e.file};
            }
            return {};
        }
        const auto name = Canonical(q.playerName);
        if (!name.empty() && name != "link") {
            std::string padded = " " + text + " ", needle = " " + name + " ";
            size_t p = 0;
            while ((p = padded.find(needle,p)) != std::string::npos) { padded.replace(p,needle.size()," link "); p += 6; }
            text = Canonical(padded);
        }
        const auto& index = q.quest ? byCharacter : byMessage;
        auto group = index.find(q.quest ? q.character : q.messageId);
        if (group == index.end() || text.empty()) return {};
        std::vector<std::string> result;
        if (q.quest) {
            // Exact line first. Wrapped replies may contain the immutable recorded
            // sentence plus live title/reward text, which stays visible on screen.
            const Entry* best = nullptr;
            for (size_t i : group->second) {
                const auto& e=entries[i];
                if (e.age != "unknown" && e.age != q.age) continue;
                if (e.match == text) return {e.file};
                if (e.match.size() >= 20 && (" "+text+" ").find(" "+e.match+" ") != std::string::npos &&
                    (!best || e.match.size() > best->match.size())) best=&e;
            }
            return best ? std::vector<std::string>{best->file} : result;
        }
        // Native decoded pages can contain a spoken line and several choice labels.
        // Require complete ordered coverage; a changed/randomized line falls back.
        while (!text.empty() && result.size() < 24) {
            const Entry* best = nullptr;
            size_t longest = 0;
            int preference = -1;
            bool ambiguous = false;
            for (size_t i : group->second) {
                const auto& e=entries[i];
                size_t length = 0;
                for (const auto& candidate : {e.match,e.text})
                    if (text == candidate || text.rfind(candidate+" ",0)==0) length=std::max(length,candidate.size());
                if (e.pattern) {
                    std::smatch match;
                    if (std::regex_search(text,match,*e.pattern)) length=std::max(length,static_cast<size_t>(match[1].length()));
                }
                if (!length) continue;
                const int score = e.kind == "choice" && e.character == "narrator" ? 3 : e.character == q.character ? 2 : 0;
                if (length > longest || (length == longest && score > preference)) {
                    best=&e;longest=length;preference=score;ambiguous=false;
                } else if (length == longest && score == preference && best && e.character != best->character) ambiguous=true;
            }
            if (!best || ambiguous) return {};
            result.push_back(best->file);
            text=Canonical(text.substr(longest));
        }
        return text.empty() ? result : std::vector<std::string>{};
    }
    std::shared_ptr<const std::vector<uint8_t>> Audio(const std::vector<std::string>& files) const {
        if (files.empty()) return {};
        std::vector<uint8_t> pcm;
        for (const auto& file : files) {
            auto path=Safe(file);
            if (!path) return {};
            std::ifstream input(*path,std::ios::binary|std::ios::ate);
            auto size=input.tellg();
            if (size < 44 || size > static_cast<std::streamoff>(MaxBytes)) return {};
            std::vector<uint8_t> bytes(static_cast<size_t>(size));input.seekg(0);input.read(reinterpret_cast<char*>(bytes.data()),bytes.size());
            if (!input || std::memcmp(bytes.data(),"RIFF",4) || std::memcmp(bytes.data()+8,"WAVE",4)) return {};
            bool format=false;size_t offset=12, dataOffset=0, dataLength=0;
            while (offset+8 <= bytes.size()) {
                uint32_t n=U32(bytes.data()+offset+4);size_t begin=offset+8;
                if (n > bytes.size()-begin) return {};
                if (!std::memcmp(bytes.data()+offset,"fmt ",4)) {
                    if (n<16 || U16(bytes.data()+begin)!=1 || U16(bytes.data()+begin+2)!=1 ||
                        U32(bytes.data()+begin+4)!=24000 || U16(bytes.data()+begin+14)!=16) return {};
                    format=true;
                }
                if (!std::memcmp(bytes.data()+offset,"data",4)) { dataOffset=begin;dataLength=n; }
                offset=begin+n+(n&1);
            }
            if (!format || !dataLength || dataLength%2 || pcm.size()+dataLength+5760 > MaxBytes) return {};
            // Qwen references often carry a silent lead-in. Keep a short breath/
            // attack margin, but do not make the player wait through that padding.
            // Only the beginning is skipped; internal pauses and the master stay intact.
            const size_t leading = LeadingSilence(bytes.data()+dataOffset,dataLength);
            dataOffset += leading; dataLength -= leading;
            if (!pcm.empty()) pcm.insert(pcm.end(),5760,0); // 120 ms between recorded units.
            pcm.insert(pcm.end(),bytes.begin()+dataOffset,bytes.begin()+dataOffset+dataLength);
        }
        if (pcm.size()>24000*2*90) return {};
        std::vector<uint8_t> wav={'R','I','F','F'};Put32(wav,36+pcm.size());
        wav.insert(wav.end(),{'W','A','V','E','f','m','t',' '});Put32(wav,16);Put16(wav,1);Put16(wav,1);
        Put32(wav,24000);Put32(wav,48000);Put16(wav,2);Put16(wav,16);
        wav.insert(wav.end(),{'d','a','t','a'});Put32(wav,pcm.size());wav.insert(wav.end(),pcm.begin(),pcm.end());
        return std::make_shared<const std::vector<uint8_t>>(std::move(wav));
    }
  private:
    static constexpr size_t MaxBytes=8*1024*1024;
    static size_t LeadingSilence(const uint8_t* pcm,size_t bytes) {
        constexpr size_t window=240,preroll=1920; // 10 ms analysis, 80 ms retained at 24 kHz.
        const size_t samples=bytes/2;
        for(size_t start=0;start<samples;start+=window) {
            const size_t count=std::min(window,samples-start);
            uint64_t energy=0;
            for(size_t i=start;i<start+count;++i) {
                const int32_t sample=static_cast<int16_t>(U16(pcm+2*i));
                energy+=static_cast<int64_t>(sample)*sample;
            }
            // Approximately -60 dBFS RMS: deliberately conservative for whispers.
            if(energy>uint64_t(32*32)*count) return start>preroll ? 2*(start-preroll) : 0;
        }
        return 0; // Do not turn an all-quiet/empty take into a truncated WAV.
    }
    struct Entry { std::string character,file,message,age,text,match,kind;size_t bytes=0;bool quest=false;std::shared_ptr<std::regex> pattern; };
    std::filesystem::path root;
    std::vector<Entry> entries;
    std::unordered_map<std::string,std::vector<size_t>> byMessage,byCharacter,byReaction;
    std::optional<std::filesystem::path> Safe(const std::string& file) const {
        std::filesystem::path p(file);
        if (p.is_absolute() || file.find('\\')!=std::string::npos || file.find(':')!=std::string::npos) return {};
        for (const auto& part:p) if(part=="..") return {};
        auto full=std::filesystem::weakly_canonical(root/p);
        auto rel=full.lexically_relative(root);
        if (rel.empty() || *rel.begin()=="..") return {};
        return full;
    }
    static std::string Escape(const std::string& s) {
        std::string result;for(char c:s) {if(std::string(".^$|()[]*+?{}\\").find(c)!=std::string::npos)result+='\\';result+=c;}return result;
    }
    static uint16_t U16(const uint8_t* p) { return uint16_t(p[0]) | uint16_t(p[1])<<8; }
    static uint32_t U32(const uint8_t* p) { return uint32_t(U16(p)) | uint32_t(U16(p+2))<<16; }
    static void Put16(std::vector<uint8_t>& b,uint16_t n) { b.push_back(n&255);b.push_back(n>>8); }
    static void Put32(std::vector<uint8_t>& b,uint32_t n) { Put16(b,n&65535);Put16(b,n>>16); }
};
}
