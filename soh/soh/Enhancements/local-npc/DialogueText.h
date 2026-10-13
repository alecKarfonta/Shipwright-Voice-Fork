#pragma once
#include <algorithm>
#include <functional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace LocalNpc::DialogueText {
// Only this encoder may introduce message control bytes. Provider/user text is
// plain text, never the game's executable message format (items, SFX, events).
inline std::string Plain(const std::string& text) {
    std::string out;
    for (size_t i = 0; i < text.size();) {
        unsigned char c = text[i++];
        if (c >= 32 && c <= 126) out += c;
        else if (c < 128) out += ' ';
        else {
            const size_t start = i - 1;
            while (i < text.size() && (static_cast<unsigned char>(text[i]) & 0xC0) == 0x80) ++i;
            const auto glyph = text.substr(start, i - start);
            if (glyph == "’" || glyph == "‘") out += '\'';
            else if (glyph == "“" || glyph == "”") out += '"';
            else if (glyph == "—" || glyph == "–") out += '-';
            else if (glyph == "…") out += "...";
            else {
                // ROM glyph indices for the accents supported by the native
                // English/French/German renderer; UTF-8 bytes are never copied.
                static const std::vector<std::string> accents = {
                    "À", "î", "Â", "Ä", "Ç", "È", "É", "Ê", "Ë", "Ï", "Ô", "Ö", "Ù", "Û", "Ü", "ß",
                    "à", "á", "â", "ä", "ç", "è", "é", "ê", "ë", "ï", "ô", "ö", "ù", "û", "ü"
                };
                const auto accent = std::find(accents.begin(), accents.end(), glyph);
                out += accent == accents.end() ? '?' : char(0x80 + std::distance(accents.begin(), accent));
            }
        }
    }
    return out;
}
using Width = std::function<float(unsigned char)>;
inline std::vector<std::string> Lines(const std::string& text, const Width& width, float limit = 210) {
    std::vector<std::string> lines;
    std::string line, word;
    float used = 0;
    auto flush = [&]() { if (!line.empty()) lines.push_back(line); line.clear(); used = 0; };
    std::istringstream words(Plain(text));
    while (words >> word) {
        float size = 0; for (auto c : word) size += width(c);
        if (!line.empty() && (used + width(' ') + size > limit || line.size() + 1 + word.size() > 28)) flush();
        if (!line.empty()) { line += ' '; used += width(' '); }
        for (auto c : word) {
            if (used + width(c) > limit || line.size() >= 28) flush();
            line += c; used += width(c);
        }
    }
    flush();
    if (lines.empty()) lines.emplace_back("...");
    return lines;
}
inline std::string Label(const std::string& text, const Width& width, float limit = 210) {
    auto lines = Lines(text, width, limit);
    if (lines.size() == 1) return lines.front();
    auto line = lines.front();
    while (!line.empty()) {
        float used = 0; for (auto c : line + "...") used += width(c);
        if (used <= limit && line.size() <= 25) break;
        line.pop_back();
    }
    return line + "...";
}
inline std::string Heading(const std::string& name, const Width& width) {
    return std::string("\x08\x05\x42") + Label(name, width) + "\x05\x40\x09\x01";
}
inline std::vector<std::string> Pages(const std::string& name, const std::string& text, const Width& width) {
    auto lines = Lines(text, width);
    std::vector<std::string> pages;
    for (size_t i = 0; i < lines.size(); i += 3) {
        auto page = Heading(name, width);
        for (size_t j = i; j < std::min(i + 3, lines.size()); ++j) {
            if (j > i) page += '\x01';
            page += lines[j];
        }
        // EVENT leaves advancement to the conversation owner; campaign actors
        // never see these IDs and cannot interpret a custom reply as a reward.
        pages.push_back(page + "\x0B\x02");
    }
    return pages;
}
inline std::string Choices(const std::string& title, const std::vector<std::string>& labels, const Width& width) {
    auto message = Heading(title, width) + "\x08";
    message += labels.size() == 2 ? '\x1B' : '\x1C';
    for (size_t i = 0; i < labels.size() && i < 3; ++i) {
        if (i) message += '\x01';
        message += Label(labels[i], width, 190);
    }
    return message + "\x09\x02";
}
}
