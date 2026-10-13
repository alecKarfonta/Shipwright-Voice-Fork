#include "ARPGTheme.h"
#include "soh/ShipUtils.h"
#include <libultraship/libultraship.h>
#include <fast/Fast3dGui.h>
#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <string>
extern "C" {
#include "textures/parameter_static/parameter_static.h"
#include "textures/icon_item_static/icon_item_static.h"
#include "textures/message_static/message_static.h"
}
namespace ARPG::Theme {
ImTextureID Texture(const char* path) {
    auto gui=std::dynamic_pointer_cast<Fast::Fast3dGui>(Ship::Context::GetRawInstance()->GetWindow()->GetGui());
    if(!gui || !path) { return nullptr; }
    if(!gui->HasTextureByName(path)) { gui->LoadGuiTexture(path,path); }
    return gui->GetTextureByName(path);
}
// [TIME] | [INFO] | Theme.Artwork | Original transparent paintings are packaged as raw PNG resources and cached on first use.
ImTextureID Artwork(const char* name) {
    if (!name) { return nullptr; }
    auto* context = Ship::Context::GetRawInstance();
    auto gui = std::dynamic_pointer_cast<Fast::Fast3dGui>(context->GetWindow()->GetGui());
    std::string path = std::string("textures/arpg/") + name + ".png";
    if (!gui) { return nullptr; }
    if (!gui->HasTextureByName(path)) {
        if (!context->GetResourceManager()->GetArchiveManager()->HasFile(path)) { return nullptr; }
        gui->LoadTextureFromRawImage(path, path);
    }
    return gui->GetTextureByName(path);
}
// Native message lettering carries the headings and controls; small body copy stays readable.
float TextWidth(std::string_view text,float size) {
    return ImGui::GetFont()->CalcTextSizeA(size,FLT_MAX,0,text.data(),text.data()+text.size()).x;
}
void Text(ImDrawList* draw,ImVec2 pos,std::string_view text,float size,ImU32 color) {
    draw->AddText(ImGui::GetFont(),size,pos,color,text.data(),text.data()+text.size());
}
float NativeWidth(std::string_view text,float size) {
    float width=0; for(unsigned char c:text) { width+=Ship_GetCharFontWidth(c)*size/16; } return width;
}
void NativeText(ImDrawList* draw,ImVec2 pos,std::string_view text,float size,ImU32 color) {
    for(unsigned char c:text) {
        if(c!=' ') {
            auto tex=Texture(static_cast<const char*>(Ship_GetCharFontTexture(c)));
            draw->AddImage(tex,{pos.x+1,pos.y+2},{pos.x+size+1,pos.y+size+2},{0,0},{1,1},IM_COL32(0,0,0,(color>>24)&255));
            draw->AddImage(tex,pos,{pos.x+size,pos.y+size},{0,0},{1,1},color);
        }
        pos.x+=Ship_GetCharFontWidth(c)*size/16;
    }
}
void Heading(ImDrawList* draw,ImVec2 pos,std::string_view text,float size) {
    NativeText(draw,pos,text,size,IM_COL32(248,238,194,255));
}
void TitleBar(std::string_view text) {
    auto p=ImGui::GetCursorScreenPos();
    float w=ImGui::GetContentRegionAvail().x;
    auto* draw=ImGui::GetWindowDrawList();
    draw->AddRectFilled(p,{p.x+w,p.y+32},IM_COL32(30,42,74,190));
    draw->AddLine(p,{p.x+w,p.y},IM_COL32(154,173,207,150));
    draw->AddLine({p.x,p.y+31},{p.x+w,p.y+31},IM_COL32(154,173,207,100));
    float font=std::min(21.f,21.f*(w-18)/std::max(1.f,NativeWidth(text,21)+2*text.size()));
    float x=p.x+(w-NativeWidth(text,font)-2*std::max(0,int(text.size())-1))*.5f;
    for(char c:text) {
        NativeText(draw,{x,p.y+(32-font)*.5f},std::string_view(&c,1),font,IM_COL32(248,238,194,255));
        x+=NativeWidth(std::string_view(&c,1),font)+2;
    }
    ImGui::Dummy({w,36});
}
void Panel(ImDrawList* draw,ImVec2 p,ImVec2 size) {
    ImVec2 end{p.x+size.x,p.y+size.y};
    draw->AddRectFilled({p.x+7,p.y+9},{end.x+7,end.y+9},IM_COL32(0,0,0,170),6);
    draw->AddRectFilled(p,end,IM_COL32(28,24,18,255),6);
    // A single authored endpaper, never tiled or sampled from unrelated pause-page decorations.
    if(auto art=Artwork("journal-stone")) { draw->AddImage(art,p,end,{0,0},{1,1},IM_COL32(190,156,107,255)); }
    draw->AddRect(p,end,IM_COL32(142,111,62,255),4,0,2);
    draw->AddRect({p.x+5,p.y+5},{end.x-5,end.y-5},IM_COL32(222,193,130,100),2);
}
void ButtonFace(ImDrawList* draw,ImVec2 p,float r,ImU32 tint) {
    draw->AddCircleFilled({p.x+2,p.y+3},r+2,IM_COL32(0,0,0,155));
    draw->AddImage(Texture(gButtonBackgroundTex),{p.x-r,p.y-r},{p.x+r,p.y+r},{0,0},{1,1},tint);
    draw->AddCircle(p,r,IM_COL32(225,217,166,190),32,1);
}
void Cursor(ImDrawList* draw, ImVec2 p, ImVec2 size) {
    const char* corners[]={gPauseMenuCursorTopLeftTex,gPauseMenuCursorTopRightTex,gPauseMenuCursorBottomLeftTex,gPauseMenuCursorBottomRightTex};
    float pulse=1.f+std::sin(float(ImGui::GetTime())*4)*.6f;
    for(int i=0;i<4;++i) {
        float x=i%2?p.x+size.x-13+pulse:p.x-3-pulse;
        float y=i/2?p.y+size.y-13+pulse:p.y-3-pulse;
        draw->AddImage(Texture(corners[i]),{x,y},{x+16,y+16},{0,0},{1,1},IM_COL32(255,243,96,255));
    }
}
void SlotFrame(ImDrawList* draw,ImVec2 p,ImVec2 size,bool selected,ImU32 color) {
    const ImVec2 end{p.x+size.x,p.y+size.y};
    draw->AddRectFilled(p,end,selected?IM_COL32(73,57,33,230):IM_COL32(15,14,12,205),5);
    draw->AddRect(p,end,selected?IM_COL32(236,202,124,255):color,5,0,selected?2.f:1.f);
    if(selected) { Cursor(draw,p,size); }
}
// Wrap body text while reserving its actual dimensions in ImGui.
void Label(std::string_view text,float size,ImU32 color) {
    float available=ImGui::GetContentRegionAvail().x;
    float limit=available>0?std::max(40.0f,available):380.0f, lineHeight=size*1.13f;
    ImVec2 p=ImGui::GetCursorScreenPos();
    float y=0,maxWidth=0;
    std::string line;
    auto measure=[&](std::string_view value) { return size>=20?NativeWidth(value,size):TextWidth(value,size); };
    auto flush=[&]() {
        if(size>=20) { NativeText(ImGui::GetWindowDrawList(),{p.x,p.y+y},line,size,color); }
        else { Text(ImGui::GetWindowDrawList(),{p.x,p.y+y},line,size,color); }
        maxWidth=std::max(maxWidth,measure(line)); y+=lineHeight; line.clear();
    };
    for(size_t i=0;i<text.size();) {
        if(text[i]=='\n') { flush(); ++i; continue; }
        size_t j=i; while(j<text.size() && text[j]!=' ' && text[j]!='\n') { ++j; }
        auto word=text.substr(i,j-i);
        if(!line.empty() && measure(line)+measure(word)>limit) { flush(); }
        line.append(word);
        if(j<text.size() && text[j]==' ') { line+=' '; ++j; }
        i=j;
    }
    if(!line.empty() || y==0) { flush(); }
    ImGui::Dummy({std::min(limit,maxWidth),y});
}
static void Formatted(ImU32 color,const char* format,va_list args) {
    char buffer[2048]; vsnprintf(buffer,sizeof(buffer),format,args); Label(buffer,18,color);
}
void Print(const char* format,...) { va_list args; va_start(args,format); Formatted(ImGui::GetColorU32(ImGuiCol_Text),format,args); va_end(args); }
void Wrapped(const char* format,...) { va_list args; va_start(args,format); Formatted(ImGui::GetColorU32(ImGuiCol_Text),format,args); va_end(args); }
void Muted(const char* format,...) { va_list args; va_start(args,format); Formatted(IM_COL32(188,200,220,255),format,args); va_end(args); }
void Colored(ImVec4 color,const char* format,...) { va_list args; va_start(args,format); Formatted(ImGui::ColorConvertFloat4ToU32(color),format,args); va_end(args); }
static bool Choice(const char* label,ImTextureID icon,ImVec2 size,bool selected,ImU32 iconTint) {
    float nominal=icon?22.f:23.f;
    if(size.x==0) { size.x=NativeWidth(label,nominal)+38+(icon?34:0); }
    if(size.x<0) { size.x=ImGui::GetContentRegionAvail().x; }
    size.y=std::max(34.0f,size.y);
    ImVec2 p=ImGui::GetCursorScreenPos();
    ImGui::PushID(label);
    ImGui::PushStyleColor(ImGuiCol_Button,{0,0,0,0});
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,{0,0,0,0});
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,{0,0,0,0});
    bool clicked=ImGui::Button("##journalChoice",size);
    ImGui::PopStyleColor(3);
    bool hover=ImGui::IsItemHovered() || ImGui::IsItemFocused();
    auto* draw=ImGui::GetWindowDrawList();
    auto ink=[&](ImU32 c) { return (c&0xFFFFFF)|(ImU32(((c>>24)&255)*ImGui::GetStyle().Alpha)<<24); };
    ImVec2 end{p.x+size.x,p.y+size.y};
    draw->AddRectFilled({p.x+1,p.y+3},{end.x+1,end.y+3},ink(IM_COL32(6,12,30,125)),9);
    draw->AddRectFilled(p,end,ink(selected?IM_COL32(75,92,132,245):hover?IM_COL32(62,79,117,245):IM_COL32(35,47,79,225)),9);
    draw->AddRect(p,end,ink(selected?IM_COL32(238,215,132,255):hover?IM_COL32(187,214,233,255):IM_COL32(118,139,175,200)),9,0,selected?2.f:1.f);
    draw->AddLine({p.x+12,p.y+3},{end.x-12,p.y+3},ink(IM_COL32(219,222,244,75)),1);
    float iconSize=icon?std::min(34.f,size.y-10):0;
    float room=size.x-(icon?iconSize+22:selected?28:16);
    float font=std::min(nominal,nominal*room/std::max(1.f,NativeWidth(label,nominal)));
    float labelWidth=NativeWidth(label,font);
    float groupWidth=labelWidth+(icon?iconSize+7:0);
    float x=p.x+(size.x-groupWidth)*.5f;
    if(icon) {
        draw->AddImage(icon,{x,p.y+(size.y-iconSize)*.5f},{x+iconSize,p.y+(size.y+iconSize)*.5f},{0,0},{1,1},ink(iconTint));
        x+=iconSize+7;
    } else if(selected) {
        draw->AddImage(Texture(gMessageArrowTex),{p.x+6,p.y+size.y*.5f-6},{p.x+17,p.y+size.y*.5f+6},{0,0},{1,1},ink(IM_COL32(255,234,93,255)));
    }
    NativeText(draw,{x,p.y+(size.y-font)*.5f},label,font,ink(selected?IM_COL32(255,244,157,255):IM_COL32(239,243,232,255)));
    ImGui::PopID(); return clicked;
}
bool Button(const char* label,ImVec2 size,bool selected) { return Choice(label,nullptr,size,selected,IM_COL32_WHITE); }
bool IconButton(const char* label,ImTextureID icon,ImVec2 size,bool selected,ImU32 tint) { return Choice(label,icon,size,selected,tint); }
bool Section(const char* label) {
    ImGuiID id=ImGui::GetID(label); bool open=ImGui::GetStateStorage()->GetBool(id,false);
    ImVec2 p=ImGui::GetCursorScreenPos(); float width=ImGui::GetContentRegionAvail().x;
    if(Button(label,{-1,36},open)) { open=!open; ImGui::GetStateStorage()->SetBool(id,open); }
    // [TIME] | [INFO] | Theme.Section | A small disclosure arrow makes hidden journal sections discoverable.
    auto* draw=ImGui::GetWindowDrawList(); float x=p.x+width-18,y=p.y+18;
    ImU32 ink=IM_COL32(223,211,159,220);
    if(open) { draw->AddTriangleFilled({x-4,y-2},{x+4,y-2},{x,y+3},ink); }
    else { draw->AddTriangleFilled({x-2,y-4},{x-2,y+4},{x+3,y},ink); }
    return open;
}
// Opaque reading surfaces keep the endpaper detail at the margins.
void DrawMessageBox(ImDrawList* draw,ImVec2 p,ImVec2 size) {
    ImVec2 end{p.x+size.x,p.y+size.y};
    draw->AddRectFilled(p,end,IM_COL32(24,20,16,250),10);
    if(auto art=Artwork("journal-stone")) { draw->AddImage(art,p,end,{0,0},{1,1},IM_COL32(177,145,99,190)); }
    draw->AddRectFilled(p,end,IM_COL32(12,10,8,95),10);
    draw->AddRect(p,end,IM_COL32(166,133,76,200),10);
    draw->AddLine({p.x+15,p.y+1},{end.x-15,p.y+1},IM_COL32(222,189,119,150));
}
void BeginPane(const char* id,ImVec2 size,bool message) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{14,14});
    ImGui::BeginChild(id,size,ImGuiChildFlags_AlwaysUseWindowPadding);
    DrawMessageBox(ImGui::GetWindowDrawList(),ImGui::GetWindowPos(),ImGui::GetWindowSize());
}
void EndPane() { ImGui::EndChild(); ImGui::PopStyleVar(); }
void Rule() {
    auto p=ImGui::GetCursorScreenPos(); float w=ImGui::GetContentRegionAvail().x;
    ImGui::GetWindowDrawList()->AddLine({p.x,p.y+4},{p.x+w,p.y+4},IM_COL32(165,187,218,120),1);
    ImGui::GetWindowDrawList()->AddQuadFilled({p.x+w*.5f,p.y},{p.x+w*.5f+4,p.y+4},{p.x+w*.5f,p.y+8},{p.x+w*.5f-4,p.y+4},IM_COL32(209,195,135,185));
    ImGui::Dummy({w,9});
}
void BeginTooltip() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{14,14});
    ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize,0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize,0);
    ImGui::PushStyleColor(ImGuiCol_PopupBg,{0,0,0,0});
    ImGui::SetNextWindowSizeConstraints({380,0},{440,1000});
    ImGui::BeginTooltip();
    DrawMessageBox(ImGui::GetWindowDrawList(),ImGui::GetWindowPos(),ImGui::GetWindowSize());
}
void EndTooltip() { ImGui::EndTooltip(); ImGui::PopStyleColor(); ImGui::PopStyleVar(3); }
}
