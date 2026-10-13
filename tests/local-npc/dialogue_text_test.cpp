#include "DialogueText.h"
#include <iostream>
#include <stdexcept>

using namespace LocalNpc::DialogueText;
void Check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
int main() {
    try {
        // Deliberately use wide glyphs: names, long unbroken service output and
        // menu labels must respect both pixel width and the 120-glyph font bank.
        auto width = [](unsigned char c) { return c == 'i' ? 3.f : 11.f; };
        std::string unsafe = "Hello";
        for (int c = 0; c < 32; ++c) unsafe += char(c);
        unsafe += "# %r @ & ^ <item> — “hello” café";
        auto plain = Plain(unsafe);
        for (unsigned char c : plain) Check((c >= 32 && c <= 126) || (c >= 0x80 && c <= 0x9E), "provider text injected control bytes");
        Check(Plain("café") == "caf\x96", "native accent glyph was lost");
        Check(plain.find("%r @ & ^") != std::string::npos, "formatting-like prose was interpreted");
        Check(Plain("It’s… fine—really") == "It's... fine-really", "typographic punctuation is unreadable");
        for (const auto& text : {unsafe, std::string(3000, 'W'), std::string(3000, 'i'), std::string(), std::string("one two three")}) {
            auto pages = Pages("Very long speaker name that does not fit", text, width);
            Check(!pages.empty(), "missing readable page");
            for (const auto& page : pages) {
                Check(page.size() < 200, "decoded message buffer could overflow");
                Check(page.substr(page.size() - 2) == "\x0B\x02", "custom page loses ownership at its end");
                size_t glyphs = 0, lines = 1;
                float pixels = 0;
                for (size_t i = 0; i < page.size(); ++i) {
                    unsigned char c = page[i];
                    if (c == 5) ++i;
                    else if (c == 1) { Check(pixels <= 210, "line overflow"); pixels = 0; ++lines; }
                    else if (c >= 32) { pixels += width(c); ++glyphs; }
                    else Check(c == 8 || c == 9 || c == 11 || c == 2, "unexpected executable control code");
                }
                Check(pixels <= 210 && lines <= 4 && glyphs <= 120, "native textbox capacity exceeded");
            }
        }
        auto menu = Choices("A speaker", {"One", "Two", std::string(900, 'W')}, width);
        Check(menu.find('\x1C') != std::string::npos && menu.back() == '\x02', "three-choice menu is malformed");
        Check(Choices("Speaker", {"Yes", "No"}, width).find('\x1B') != std::string::npos, "two-choice menu is malformed");
        Check(Lines("one two three", width, 77) == std::vector<std::string>({"one two", "three"}), "word wrapping lost text");
        std::cout << "Dialogue encoding: native buffer, glyph bank, width, pagination and injection checks passed\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
