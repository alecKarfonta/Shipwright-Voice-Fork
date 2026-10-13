#pragma once
#include <imgui.h>
#include <string_view>
namespace ARPG::Theme {
ImTextureID Texture(const char* path);
ImTextureID Artwork(const char* name);
float TextWidth(std::string_view text, float size);
void Text(ImDrawList* draw, ImVec2 pos, std::string_view text, float size, ImU32 color = IM_COL32(255,248,220,255));
void Heading(ImDrawList* draw, ImVec2 pos, std::string_view text, float size);
void TitleBar(std::string_view text);
float NativeWidth(std::string_view text, float size);
void NativeText(ImDrawList* draw, ImVec2 pos, std::string_view text, float size, ImU32 color = IM_COL32(255,248,220,255));
void DrawMessageBox(ImDrawList* draw, ImVec2 pos, ImVec2 size);
void Panel(ImDrawList* draw, ImVec2 pos, ImVec2 size);
void ButtonFace(ImDrawList* draw, ImVec2 center, float radius, ImU32 tint);
void SlotFrame(ImDrawList* draw, ImVec2 pos, ImVec2 size, bool selected, ImU32 color);
void Cursor(ImDrawList* draw, ImVec2 pos, ImVec2 size);
void Label(std::string_view text, float size = 21, ImU32 color = IM_COL32(255,255,235,255));
void Print(const char* format, ...);
void Wrapped(const char* format, ...);
void Muted(const char* format, ...);
void Colored(ImVec4 color, const char* format, ...);
bool Button(const char* label, ImVec2 size = {0,0}, bool selected = false);
bool IconButton(const char* label, ImTextureID icon, ImVec2 size, bool selected = false, ImU32 tint = IM_COL32_WHITE);
bool Section(const char* label);
void BeginPane(const char* id, ImVec2 size, bool message = false);
void EndPane();
void Rule();
void BeginTooltip();
void EndTooltip();
}
