#include "ARPGInventoryWindow.h"
#include "soh/Network/Anchor/Fellowship.h"
#include "ArsenalSystem.h"
#include "ARPGTheme.h"
#include "SkillUI.h"
#include "ExpeditionSystem.h"
#include "ARPG.h"
#include "ARPGSave.h"
#include "InventoryRules.h"
#include "InventoryCardStats.h"
#include "CraftingRules.h"
#include "ItemAppearance.h"
#include "InventorySystem.h"
#include "AdventureJournal.h"
#include "QuestSystem.h"
#include "ItemModifierSystem.h"
#include "AdventureItems.h"
#include "ARPGTesting.h"
#include "soh/SohGui/ImGuiUtils.h"
#include "soh/cvar_prefixes.h"
#include "soh/util.h"
#include "soh/Notification/Notification.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include <libultraship/libultraship.h>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string>
#include <vector>
#include <spdlog/fmt/fmt.h>

extern "C" {
#include "variables.h"
#include "macros.h"
extern PlayState* gPlayState;
}

namespace {
bool cardNavigationActive=false;
const ImVec4 gold(1.0f, 0.88f, 0.52f, 1);
const ImVec4 muted(0.74f, 0.79f, 0.88f, 1);
const ImVec4 positive(0.43f, 0.83f, 0.60f, 1);
const ImVec4 negative(0.94f, 0.44f, 0.41f, 1);
const ImVec4 rarities[] = { { 0.75f, 0.77f, 0.8f, 1 }, { 0.42f, 0.64f, 1, 1 },
                           { 1, 0.84f, 0.38f, 1 }, { 0.94f, 0.54f, 0.26f, 1 } };
const char* rarityNames[] = { "COMMON", "MAGIC", "RARE", "UNIQUE" };
const char* slots[] = { "SWORD", "SHIELD", "TUNIC", "BOOTS", "BOW", "HAMMER", "BOMBS", "SEEDS", "HOOKSHOT", "BOOMERANG", "DEKU NUTS", "BOMBCHUS", "DEKU STICK", "LENS", "OCARINA", "BOTTLE" };
struct DragItem { bool equipment; int index; };
bool expeditionPage=false;
constexpr const char* payloadType = "ARPG_ITEM";
constexpr const char* stonePayloadType = "ARPG_SOCKET_STONE";
ImVec4 Rarity(const ARPGItemInstance& item) { if(ARPG::Loot::Set(item)) { return {0.38f,.9f,.55f,1}; } if(ARPG::Loot::Runework(item)) { return {.75f,.61f,1,1}; } return rarities[std::min<int>(item.rarity, 3)]; }
ImU32 Color(ImVec4 c) { return ImGui::ColorConvertFloat4ToU32(c); }
ImTextureID Icon(const ARPGItemInstance& item) {
    auto shown=item;
    if(shown.slot>=ARPG_LOOT_SLOT_COUNT) { return {}; }
    if(!shown.baseEquip) { shown.baseEquip=ARPG::Loot::WeaponSlot(shown.slot)?1:std::max<int>(1,CUR_EQUIP_VALUE(shown.slot)); }
    if(shown.slot>=ARPG_SLOT_BOMBS) {
        return ARPG::Theme::Texture(static_cast<const char*>(gItemIcons[ARPG::AdventureItems::NativeItem(shown.slot)]));
    }
    return ARPG::Theme::Artwork(ARPG::ItemAppearance::Resolve(shown).artwork);
}
void DrawItemArtwork(ImDrawList* draw,const ARPGItemInstance& item,ImVec2 from,ImVec2 to,ImU32 tint) {
    if (auto texture=Icon(item)) { draw->AddImage(texture,from,to,{0,0},{1,1},tint); }
    else { draw->AddQuad({(from.x+to.x)*.5f,from.y},{to.x,(from.y+to.y)*.5f},
                        {(from.x+to.x)*.5f,to.y},{from.x,(from.y+to.y)*.5f},tint,2); }
}
std::string BaseName(ARPGItemInstance item) { item.modCount = item.uniqueId = item.setId = item.socketCount = 0; return ItemModifierSystem::ItemName(item); }
// [TIME] | [INFO] | Inventory.ItemTint | Rarity belongs to the frame; the painting retains the material colors used by the world model.
ImU32 ItemTint(const ARPGItemInstance&) { return IM_COL32_WHITE; }
// [TIME] | [INFO] | Inventory.ModifierMark | Tiny engraved symbols distinguish the actual rolls even when they share a base painting.
void ModifierMark(ImDrawList* draw, int id, ImVec2 p, float size) {
    auto pt=[&](float x,float y) { return ImVec2{p.x+x*size,p.y+y*size}; };
    static const ImU32 colors[] = { IM_COL32_WHITE, IM_COL32(243,187,126,255), IM_COL32(239,229,159,255),
        IM_COL32(255,139,71,255), IM_COL32(138,220,255,255), IM_COL32(255,245,151,255),
        IM_COL32(185,219,157,255), IM_COL32(173,213,179,255), IM_COL32(239,131,126,255),
        IM_COL32(145,232,202,255), IM_COL32(239,212,114,255), IM_COL32(174,161,255,255), IM_COL32(255,176,92,255), IM_COL32(163,226,218,255), IM_COL32(153,172,255,255), IM_COL32(248,227,137,255) };
    ImU32 c=colors[std::clamp(id,0,ARPG_MOD_COUNT_-1)];
    auto line=[&](float x,float y,float u,float v) { draw->AddLine(pt(x,y),pt(u,v),c,1.6f); };
    switch(id) {
        case ARPG_MOD_BLAST: draw->AddCircle(p,size*.24f,c,12,1.4f); line(0,-.5f,0,-.32f); line(0,.32f,0,.5f); line(-.5f,0,-.32f,0); line(.32f,0,.5f,0); break;
        case ARPG_MOD_REACH: line(-.5f,0,.5f,0); line(-.5f,0,-.2f,-.3f); line(-.5f,0,-.2f,.3f); line(.5f,0,.2f,-.3f); line(.5f,0,.2f,.3f); break;
        case ARPG_MOD_CONTROL: draw->AddCircle(pt(-.18f,-.18f),size*.25f,c,12,1.4f); draw->AddCircle(pt(.18f,.18f),size*.25f,c,12,1.4f); break;
        case ARPG_MOD_FOLLOWUP: line(-.45f,-.3f,0,-.3f); line(0,-.3f,0,.3f); line(0,.3f,.45f,.3f); line(.45f,.3f,.2f,0); line(.45f,.3f,.2f,.5f); break;
        case ARPG_MOD_FLAME: draw->AddTriangleFilled(pt(0,-.5f),pt(-.4f,.4f),pt(.4f,.4f),c); break;
        case ARPG_MOD_FROST: line(0,-.5f,0,.5f); line(-.43f,-.25f,.43f,.25f); line(-.43f,.25f,.43f,-.25f); break;
        case ARPG_MOD_STORM: line(.3f,-.5f,-.2f,0); line(-.2f,0,.2f,0); line(.2f,0,-.3f,.5f); break;
        case ARPG_MOD_AEGIS: draw->AddQuad(pt(-.4f,-.4f),pt(.4f,-.4f),pt(.3f,.2f),pt(0,.5f),c,1.5f); line(0,.5f,-.4f,-.4f); break;
        case ARPG_MOD_VITALITY: draw->AddTriangleFilled(pt(-.45f,-.15f),pt(.45f,-.15f),pt(0,.5f),c); draw->AddCircleFilled(pt(-.23f,-.18f),size*.24f,c); draw->AddCircleFilled(pt(.23f,-.18f),size*.24f,c); break;
        case ARPG_MOD_SWIFT: line(-.4f,-.35f,.3f,-.35f); line(-.2f,0,.5f,0); line(-.4f,.35f,.3f,.35f); break;
        case ARPG_MOD_LUCKY: draw->AddQuadFilled(pt(0,-.5f),pt(.32f,0),pt(0,.5f),pt(-.32f,0),c); break;
        case ARPG_MOD_MAGICAFF: draw->AddQuad(pt(0,-.5f),pt(.35f,0),pt(0,.5f),pt(-.35f,0),c,1.5f); line(0,-.5f,0,.5f); break;
        case ARPG_MOD_ARCHERY: line(-.4f,.4f,.4f,-.4f); line(.4f,-.4f,0,-.4f); line(.4f,-.4f,.4f,0); break;
        case ARPG_MOD_KEEN: line(-.5f,0,.5f,0); line(0,-.5f,0,.5f); draw->AddCircle(p,size*.3f,c,12,1.4f); break;
        default: line(-.3f,.4f,.3f,-.4f); line(-.4f,0,0,.3f); break;
    }
}
void EmptyGearMark(ImDrawList* draw,int slot,ImVec2 p,float size) {
    auto pt=[&](float x,float y) { return ImVec2{p.x+x*size,p.y+y*size}; };
    const ImU32 ink=IM_COL32(155,172,199,200);
    auto line=[&](float x,float y,float u,float v) { draw->AddLine(pt(x,y),pt(u,v),ink,1.6f); };
    if(ARPG::Loot::WeaponSlot(slot)) {
        ARPGItemInstance icon{};icon.slot=slot;icon.baseEquip=1;
        DrawItemArtwork(draw,icon,p,{p.x+size,p.y+size},IM_COL32(155,172,199,125));
    } else if(slot==0) {
        draw->AddQuad(pt(.53f,.14f),pt(.61f,.25f),pt(.4f,.65f),pt(.31f,.6f),ink,1.6f);
        line(.23f,.57f,.48f,.71f); line(.34f,.66f,.25f,.84f); line(.19f,.81f,.31f,.88f);
    } else if(slot==1) {
        ImVec2 points[]={pt(.22f,.22f),pt(.5f,.13f),pt(.78f,.22f),pt(.72f,.65f),pt(.5f,.85f),pt(.28f,.65f)};
        draw->AddPolyline(points,6,ink,ImDrawFlags_Closed,1.6f); line(.5f,.26f,.5f,.66f); line(.34f,.43f,.66f,.43f);
    } else if(slot==2) {
        ImVec2 points[]={pt(.36f,.2f),pt(.16f,.3f),pt(.1f,.48f),pt(.29f,.54f),pt(.28f,.84f),pt(.72f,.84f),pt(.71f,.54f),pt(.9f,.48f),pt(.84f,.3f),pt(.64f,.2f),pt(.57f,.34f),pt(.43f,.34f)};
        draw->AddPolyline(points,12,ink,ImDrawFlags_Closed,1.6f); line(.29f,.66f,.71f,.66f);
    } else {
        ImVec2 points[]={pt(.37f,.17f),pt(.69f,.17f),pt(.65f,.62f),pt(.81f,.72f),pt(.81f,.83f),pt(.27f,.83f),pt(.25f,.7f),pt(.36f,.57f)};
        draw->AddPolyline(points,8,ink,ImDrawFlags_Closed,1.6f); line(.37f,.29f,.68f,.29f); line(.28f,.75f,.8f,.75f);
    }
}
void ItemImage(const ARPGItemInstance& item, float size) {
    auto p=ImGui::GetCursorScreenPos();
    DrawItemArtwork(ImGui::GetWindowDrawList(),item,p,{p.x+size,p.y+size},ItemTint(item));
    ImGui::Dummy({size,size});
}

// Family and grade select separate paintings; every forge view shares this lookup.
ImTextureID GemArtwork(int material) {
    if(!ARPG::Loot::Gem(material)) { return nullptr; }
    static const char* families[]={"ruby","sapphire","topaz","emerald","amethyst","diamond"};
    static const char* grades[]={"rough","cut","perfect"};
    auto path=fmt::format("gems/{}-{}",grades[(material-1)/6],families[(material-1)%6]);
    return ARPG::Theme::Artwork(path.c_str());
}
// Engraved rune tablets and painted gems share the saved material IDs used by crafting.
void MaterialMark(ImDrawList* draw,int material,ImVec2 p,float size,ImU32 tint=IM_COL32_WHITE) {
    using namespace ARPG;
    auto pt=[&](float x,float y) { return ImVec2{p.x+x*size,p.y+y*size}; };
    if(Loot::Gem(material)) {
        if(auto art=GemArtwork(material)) {
            draw->AddImage(art,pt(-.5f,-.5f),pt(.5f,.5f),{0,0},{1,1},tint);
        } else {
            // Distinct silhouettes also keep older or incomplete art archives usable.
            static const ImVec2 outlines[6][8]={
                {{-.28f,-.34f},{.25f,-.34f},{.42f,-.1f},{.31f,.28f},{.1f,.4f},{-.23f,.33f},{-.43f,.07f},{-.4f,-.12f}},
                {{0,-.48f},{.22f,-.18f},{.34f,.13f},{.26f,.35f},{0,.44f},{-.26f,.35f},{-.34f,.13f},{-.22f,-.18f}},
                {{0,-.43f},{.09f,-.3f},{.35f,.18f},{.39f,.29f},{0,.35f},{-.39f,.29f},{-.35f,.18f},{-.09f,-.3f}},
                {{-.2f,-.44f},{.2f,-.44f},{.32f,-.31f},{.32f,.31f},{.2f,.44f},{-.2f,.44f},{-.32f,.31f},{-.32f,-.31f}},
                {{0,-.49f},{.18f,-.25f},{.27f,0},{.18f,.25f},{0,.49f},{-.18f,.25f},{-.27f,0},{-.18f,-.25f}},
                {{-.17f,-.4f},{.17f,-.4f},{.4f,-.17f},{.4f,.17f},{.17f,.4f},{-.17f,.4f},{-.4f,.17f},{-.4f,-.17f}}
            };
            static const ImVec4 colors[]={{.94f,.27f,.3f,1},{.3f,.62f,1,1},{1,.77f,.2f,1},
                {.28f,.86f,.52f,1},{.73f,.44f,.96f,1},{.76f,.94f,1,1}};
            int kind=(material-1)%6,rank=(material-1)/6;
            auto c=colors[kind];
            ImVec2 points[8];
            for(int n=0;n<8;++n) {
                float scale=rank==0?(n%3==0?.72f:1.f):1.f;
                points[n]=pt(outlines[kind][n].x*scale,outlines[kind][n].y*scale);
            }
            for(int n=0;n<8;++n) {
                float light=.45f+float((n+2)%8)*.075f;
                draw->AddTriangleFilled(p,points[n],points[(n+1)%8],Color({c.x*light,c.y*light,c.z*light,1}));
            }
            draw->AddPolyline(points,8,Color(c),ImDrawFlags_Closed,1.5f);
            if(rank==2) { draw->AddCircle(pt(-.12f,-.18f),size*.06f,IM_COL32_WHITE,4,1.5f); }
        }
    } else if(Loot::Rune(material)) {
        ImVec2 stone[]={pt(-.27f,-.4f),pt(.2f,-.43f),pt(.35f,-.22f),pt(.3f,.39f),pt(-.24f,.43f),pt(-.34f,.2f)};
        draw->AddConvexPolyFilled(stone,6,IM_COL32(97,111,140,255));
        draw->AddPolyline(stone,6,IM_COL32(201,201,187,255),ImDrawFlags_Closed,1.6f);
        draw->AddLine(pt(-.23f,-.33f),pt(-.29f,.18f),IM_COL32(217,219,208,130),2);
        ModifierMark(draw,Loot::SocketBonus(material,0).modId,p,size*.45f);
    } else if(material==Loot::Essence) {
        draw->AddQuadFilled(pt(0,-.46f),pt(.3f,0),pt(0,.46f),pt(-.3f,0),IM_COL32(249,186,74,255));
        draw->AddTriangleFilled(pt(0,-.28f),pt(.13f,.06f),pt(-.13f,.06f),IM_COL32(255,247,175,255));
        draw->AddTriangleFilled(pt(-.22f,.22f),pt(-.14f,.07f),pt(-.06f,.22f),IM_COL32(255,247,175,255));
        draw->AddTriangleFilled(pt(.06f,.22f),pt(.14f,.07f),pt(.22f,.22f),IM_COL32(255,247,175,255));
    } else {
        draw->AddCircleFilled(p,size*.32f,IM_COL32(8,15,33,230),24);
        draw->AddCircle(p,size*.32f,IM_COL32(162,176,204,210),24,2);
        draw->AddCircle(p,size*.24f,IM_COL32(68,83,117,180),24,1);
    }
}
std::string MaterialShortName(int id) {
    auto name=ARPG::Loot::MaterialName(id);
    if(ARPG::Loot::Gem(id)) { return name.substr(name.find(' ')+1); }
    if(ARPG::Loot::Rune(id)) { return name.substr(0,name.find(' ')); }
    return name;
}
void SocketMount(ImDrawList* draw,int material,ImVec2 center,float diameter,bool active=false) {
    const float radius=diameter*.5f;
    draw->AddCircleFilled(center,radius+2,IM_COL32(9,8,6,245),32);
    draw->AddCircle(center,radius,active?Color(gold):IM_COL32(153,139,106,245),32,active?2.5f:2.f);
    draw->AddCircle(center,radius-3,IM_COL32(53,46,35,255),32,1.5f);
    if(material)MaterialMark(draw,material,center,diameter-6);
}
void ItemSockets(ImDrawList* draw,const ARPGItemInstance& item,ImVec2 origin,float iconSize) {
    const float diameter=std::clamp(iconSize*.29f,16.f,24.f);
    const int count=std::min<int>(item.socketCount,ARPG_ITEM_SOCKET_COUNT);
    for(int n=0;n<count;++n)
        SocketMount(draw,item.sockets[n],{origin.x+iconSize*.5f+(n-(count-1)*.5f)*(diameter+1),origin.y+iconSize*.79f},diameter);
}
bool MaterialChoice(int id,ImVec2 size,bool selected) {
    using namespace ARPG;
    ImGui::PushID(id);
    auto p=ImGui::GetCursorScreenPos();
    bool clicked=ImGui::InvisibleButton("stone",size,ImGuiButtonFlags_EnableNav);
    auto* draw=ImGui::GetWindowDrawList();
    Theme::SlotFrame(draw,p,size,selected||ImGui::IsItemFocused(),IM_COL32(109,133,170,210));
    if(ImGui::IsItemHovered()) { draw->AddRect(p,{p.x+size.x,p.y+size.y},Color(gold),5); }
    const bool gem=Loot::Gem(id);
    if(gem) {
        const char* grades[]={"ROUGH","CUT","PERFECT"};
        Theme::Text(draw,{p.x+6,p.y+6},grades[(id-1)/6],11,Color(muted));
    }
    float nameY=gem?size.y-27.f:51.f;
    float artSize=gem?std::min({80.f,size.x-16,size.y-28}):44.f;
    MaterialMark(draw,id,{p.x+size.x*.5f,p.y+(gem?(nameY+5)*.5f:28.f)},artSize,gem&&!gARPGSave.materials[id]?IM_COL32(170,181,204,140):IM_COL32_WHITE);
    auto name=MaterialShortName(id);
    Theme::NativeText(draw,{p.x+(size.x-Theme::NativeWidth(name,21))*.5f,p.y+nameY},name,21,IM_COL32(242,236,214,255));
    auto count=fmt::format("x{}",gARPGSave.materials[id]);
    float countX=p.x+size.x-Theme::TextWidth(count,16)-7;
    draw->AddRectFilled({countX-4,p.y+3},{p.x+size.x-3,p.y+24},IM_COL32(15,24,46,225),4);
    Theme::Text(draw,{countX,p.y+5},count,16,
        gARPGSave.materials[id]?Color(gold):Color(muted));
    if(Loot::SocketMaterial(id) && gARPGSave.materials[id] && ImGui::BeginDragDropSource()) {
        ImGui::SetDragDropPayload(stonePayloadType,&id,sizeof(id));
        const auto p=ImGui::GetCursorScreenPos();
        MaterialMark(ImGui::GetWindowDrawList(),id,{p.x+24,p.y+24},46);
        ImGui::Dummy({48,48}); Theme::Wrapped("%s",Loot::MaterialName(id).c_str());
        ImGui::EndDragDropSource();
    }
    ImGui::PopID();
    return clicked;
}

void Label(const char* name) { ARPG::Theme::Label(name,21,IM_COL32(232,219,163,255)); }
void OpenSettings() {
    ARPG::Inventory::Close();
    if (auto window = Ship::Context::GetRawInstance()->GetWindow()->GetGui()->GetGuiWindow("ARPG Mod")) { window->Show(); }
}
std::string Lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return std::tolower(c); });
    return value;
}
std::string PowerText(int id, int power, bool delta = false) {
    char text[48];
    float amount = static_cast<float>(power);
    const char* unit = "%";
    if (id == ARPG_MOD_SWIFT) { amount *= 2; }
    if (id == ARPG_MOD_MAGICAFF) { amount *= 5; }
    snprintf(text, sizeof(text), (delta ? "%+.0f%s" : "%.0f%s"), amount, unit);
    return text;
}
const char* StatName(int id) {
    static const char* names[] = { "", "Melee damage", "Critical chance", "Fire hit", "Frost hit", "Light hit",
        "Ranged damage", "Hit reduction", "Heart capacity", "Movement speed", "Gear-drop chance", "Magic saving",
        "Blast radius", "Travel distance", "Setup duration", "Finisher damage" };
    static_assert(std::size(names)==ARPG_MOD_COUNT_);
    return id>=0 && id<ARPG_MOD_COUNT_?names[id]:"Unknown stat";
}
// The inspector and compact equipment tooltip use the same complete rows as the cards.
void CardStats(const ARPGItemInstance& item, bool equipped) {
    using namespace ARPG;
    const auto current=InventoryRules::Equipped(gARPGSave,item.slot);
    const bool compare=!equipped && InventoryRules::Present(current);
    Theme::Rule();
    if(ImGui::BeginTable("cardStats",compare?3:2,ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Stat",0,1.4f);
        ImGui::TableSetupColumn(equipped?"Equipped":"This item",0,1.f);
        if(compare) { ImGui::TableSetupColumn("Equipped",0,1.f); }
        ImGui::TableHeadersRow();
        bool any=false;
        const auto stats=InventoryCardStats::Build(gARPGSave,item,equipped);
        for(int i=0;i<stats.count;++i) {
            const auto [id,incoming,existing]=stats.rows[i];
            any=true;ImGui::TableNextRow();ImGui::TableNextColumn();Theme::Wrapped("%s",StatName(id));
            ImGui::TableNextColumn();Theme::Colored(compare && incoming!=existing?(incoming>existing?positive:negative):muted,"%s",PowerText(id,incoming,true).c_str());
            if(compare) { ImGui::TableNextColumn();Theme::Muted("%s",PowerText(id,existing,true).c_str()); }
        }
        if(!any) { ImGui::TableNextRow();ImGui::TableNextColumn();Theme::Muted("Bonus stats");ImGui::TableNextColumn();Theme::Muted("None"); }
        ImGui::EndTable();
    }
    if(compare) { Theme::Muted("Compared with %s",ItemModifierSystem::ItemName(current).c_str()); }
    if(item.uniqueId && Loot::Unique(item)) { Theme::Colored(gold,"%s",Loot::WeaponPowerName(item));Theme::Wrapped("%s",Loot::WeaponPowerDescription(item)); }
}
float ItemCardHeight(const ARPGItemInstance& item,bool) {
    if(!ARPG::InventoryRules::Present(item))return 108.f;
    // The grid shows this item's own stats. Lost stats belong in the hover comparison.
    const auto stats=ARPG::InventoryCardStats::Build(gARPGSave,item,true);
    return 100.f+std::max(1,stats.count)*20.f+(ARPG::Loot::Unique(item)?22.f:0.f);
}
std::string FitCardText(std::string text,float width,float fontSize) {
    if(ARPG::Theme::TextWidth(text,fontSize)<=width)return text;
    while(!text.empty() && ARPG::Theme::TextWidth(text+"...",fontSize)>width)text.pop_back();
    return text+"...";
}
void ItemDetails(const ARPGItemInstance& item, bool heading = true) {
    if (heading) {
    if (auto style = ARPG::ItemAppearance::Get(item); style.name) { ARPG::Theme::Label(style.name,25,Color(gold)); }
    ImGui::PushStyleColor(ImGuiCol_Text, Rarity(item));
    ARPG::Theme::Wrapped("%s", ItemModifierSystem::ItemName(item).c_str());
    ImGui::PopStyleColor();
    ARPG::Theme::Colored(muted, "%s  /  %s", ARPG::Loot::Quality(item), slots[item.slot]);
    }
    ImGui::Spacing();
    if(auto unique=ARPG::Loot::Unique(item); unique && ARPG::Theme::Section("Story")) { ARPG::Theme::Wrapped("%s",unique->lore); }
    if(auto set=ARPG::Loot::Set(item); set && ARPG::Theme::Section("Set bonuses")) {
        int pieces=ARPG::Loot::SetPieces(gARPGSave,item.setId,ItemModifierSystem::EquippedMask());
        ARPG::Theme::Colored(positive,"%s  %d / 4 pieces",set->name,pieces);
        for(int threshold:{2,4}) {
            ARPG::Theme::Colored(pieces>=threshold?positive:muted,"%d pieces:",threshold);
            for(auto mod:threshold==2?set->two:set->four) { ARPG::Theme::Wrapped("%s",ItemModifierSystem::DescribeModifier(mod).c_str()); }
        }
    }
    auto styleText=[](ARPG::Loot::Style style) {
        if(style.combatPower!=ARPG::Loot::CombatPower::None) {
            ARPG::Theme::Colored(gold,"%s",ARPG::Loot::PowerName(style.combatPower));
            ARPG::Theme::Wrapped("%s",ARPG::Loot::PowerDescription(style.combatPower));
            ARPG::Theme::Muted("Weapon hits: sword, hammer, stick, bow or slingshot. Requires techniques; gear changes clear preparations.");
        }
        static const char* trees[]={"Sword","Magic","Guardian"};
        if(style.tree>=0 && style.tree<3) {
            if(style.power) { ARPG::Theme::Wrapped("%s technique damage +%d%%",trees[style.tree],style.power); }
            if(style.recovery) { ARPG::Theme::Wrapped("%s recharge time -%d%%",trees[style.tree],style.recovery); }
        }
        if(style.manaOnHit) { ARPG::Theme::Wrapped("Basic sword hits restore %d magic",style.manaOnHit); }
        if(style.lifeOnKill) { ARPG::Theme::Wrapped("Kills restore %.2f hearts",style.lifeOnKill/16.f); }
    };
    if(auto unique=ARPG::Loot::Unique(item); unique && ARPG::Theme::Section("Unique power")) {
        if(item.uniqueId>=9) {
            ARPG::Theme::Colored(gold,"%s",ARPG::Loot::WeaponPowerName(item));
            ARPG::Theme::Wrapped("%s",ARPG::Loot::WeaponPowerDescription(item));
        } else styleText(unique->style);
    }
    if(auto set=ARPG::Loot::Set(item); set && ARPG::Theme::Section("Full-set power")) { styleText(set->style); }
    if(int word=ARPG::Loot::Runework(item); word && ARPG::Theme::Section("Runework power")) {
        ARPG::Theme::Colored(gold,"%s runework active",ARPG::Loot::Runeworks[word].name);
        for(auto mod:ARPG::Loot::Runeworks[word].mods) { if(mod.modId) { ARPG::Theme::Wrapped("%s",ItemModifierSystem::DescribeModifier(mod).c_str()); } }
        styleText(ARPG::Loot::Runeworks[word].style);
    }
}
}

namespace {
void ItemAffixes(const ARPGItemInstance& item) {
    using namespace ARPG;
    bool any=false;
    for(int i=0;i<std::min<int>(item.modCount,ARPG_ITEM_MOD_COUNT);++i) {
        Theme::Colored({.52f,.67f,1,1},"%s",ItemModifierSystem::DescribeModifier(item.mods[i]).c_str()); any=true;
    }
    for(int n=0;n<std::min<int>(item.socketCount,ARPG_ITEM_SOCKET_COUNT);++n)if(item.sockets[n]) {
        Theme::Colored({.48f,.79f,.68f,1},"%s",ItemModifierSystem::DescribeModifier(Loot::SocketBonus(item.sockets[n],item.slot)).c_str()); any=true;
    }
    if(int word=Loot::Runework(item)) {
        Theme::Colored(gold,"%s",Loot::Runeworks[word].name);
        for(auto mod:Loot::Runeworks[word].mods)if(mod.modId)Theme::Colored(gold,"%s",ItemModifierSystem::DescribeModifier(mod).c_str());
        any=true;
    }
    if(!any)Theme::Muted("No bonus stats");
}
}

// [TIME] | [INFO] | ARPGInventoryWindow.Card | Every drag refers to a fixed saved slot; moves never destroy the displaced item.
void ARPGInventoryWindow::Card(const ARPGItemInstance& original, int index, bool equipment, ImVec2 size) {
    using namespace ARPG;
    const auto item = original;
    const bool present = InventoryRules::Present(item);
    const bool statCard=size.x>=160;
    ImGui::PushID(equipment ? 1000 + index : index);
    ImGui::BeginGroup();
    ImVec2 p = ImGui::GetCursorScreenPos();
    if (ImGui::InvisibleButton("item", size, ImGuiButtonFlags_EnableNav)) {
        socketing=false;
        const bool equipSelected=equipment && !statCard && inspectItem && !inspectNative &&
            !inspectMaterial && !selectedEquipment && selectedIndex>=0 && selectedIndex<ARPG_BACKPACK_CAPACITY &&
            InventoryRules::Present(gARPGSave.backpack[selectedIndex]) && gARPGSave.backpack[selectedIndex].slot==index;
        if(equipSelected) { MoveItem(false,selectedIndex,true,index); }
        else {
            selectedIndex=index; selectedEquipment=equipment; selectedNativeSlot=-1;
            inspectNative=false; resetDetailsScroll=true; salvageArmed=-1;
            inspectItem=present; inspectMaterial=false;
        }
    }
    bool hovered = !cardNavigationActive && ImGui::IsItemHovered(ImGuiHoveredFlags_NoNavOverride);
    bool selected = inspectItem && !inspectNative && !inspectMaterial && selectedEquipment == equipment && selectedIndex == index;
    auto* draw = ImGui::GetWindowDrawList();
    ImVec2 end(p.x + size.x, p.y + size.y);
    Theme::SlotFrame(draw,p,size,selected || ImGui::IsItemFocused(),present ? Color(Rarity(item)) : IM_COL32(86,106,81,180));
    if(hovered) { draw->AddRect(p,end,IM_COL32(195,204,157,255),5,0,2); }
    const bool tile=equipment && size.y>=76 && !statCard;
    float iconSize = !equipment?std::min(size.x-16,size.y-30):tile?std::min(size.x-12,size.y-31):std::min(54.0f,size.y-12);
    ImVec2 iconPos(p.x + (equipment&&!tile ? 10 : (size.x - iconSize) * 0.5f), p.y + (size.y-iconSize-(tile || !equipment?22:0))*.5f);
    const float textX=iconPos.x+iconSize+10;
    if(statCard) { iconSize=56; iconPos={p.x+10,p.y+10}; }
    if (present) {
        DrawItemArtwork(draw,item,iconPos,{iconPos.x+iconSize,iconPos.y+iconSize},ItemTint(item));
        ItemSockets(draw,item,iconPos,iconSize);
        if (!equipment && !statCard) {
            auto unique=Loot::Unique(item);
            auto name=unique?std::string(unique->name):BaseName(item);
            if(Theme::TextWidth(name,14)>size.x-12) {
                while(!name.empty() && Theme::TextWidth(name+"...",14)>size.x-12) { name.pop_back(); }
                name+="...";
            }
            Theme::Text(draw,{p.x+(size.x-Theme::TextWidth(name,14))*.5f,end.y-23},name,14,Color(Rarity(item)));
        }
        if (tile) {
            float labelSize=std::min(18.f,18.f*(size.x-10)/Theme::NativeWidth(slots[index],18));
            Theme::NativeText(draw,{p.x+(size.x-Theme::NativeWidth(slots[index],labelSize))*.5f,end.y-25},slots[index],labelSize,Color(gold));
        } else if (equipment && !statCard) {
            Theme::NativeText(draw,{textX,p.y+7},slots[index],21,Color(gold));
            draw->PushClipRect(ImVec2(textX, p.y + 28), ImVec2(end.x - 7, end.y - 9), true);
            Theme::Text(draw,{textX,p.y+34},ItemAppearance::Resolve(item).name?ItemAppearance::Resolve(item).name:BaseName(item),16,Color(Rarity(item)));
            draw->PopClipRect();
        }
    } else if (equipment) {
        EmptyGearMark(draw,index,iconPos,iconSize);
        if(tile) { float labelSize=std::min(18.f,18.f*(size.x-10)/Theme::NativeWidth(slots[index],18)); Theme::NativeText(draw,{p.x+(size.x-Theme::NativeWidth(slots[index],labelSize))*.5f,end.y-25},slots[index],labelSize,Color(muted)); }
        else {
            Theme::NativeText(draw,{textX,p.y+7},slots[index],21,Color(gold));
            Theme::Text(draw,{textX,p.y+34},"Empty slot",15,Color(muted));
        }
    } else {
        ImVec2 center(p.x + size.x * 0.5f, p.y + size.y * 0.5f);
        draw->AddLine(ImVec2(center.x - 3, center.y), ImVec2(center.x + 3, center.y), IM_COL32(54,58,67,255));
    }
    if(present && statCard) {
        const auto stats=InventoryCardStats::Build(gARPGSave,item,true);
        draw->PushClipRect(p,end,true);
        const auto name=ItemModifierSystem::ItemName(item);
        draw->PushClipRect({p.x+76,p.y+10},{end.x-10,p.y+48},true);
        draw->AddText(ImGui::GetFont(),16,{p.x+76,p.y+10},Color(Rarity(item)),name.c_str(),nullptr,size.x-88);
        draw->PopClipRect();
        Theme::Text(draw,{p.x+76,p.y+52},FitCardText(fmt::format("{} / {}",equipment?"Equipped":Loot::Quality(item),slots[item.slot]),size.x-88,13),13,Color(gold));
        draw->AddLine({p.x+10,p.y+76},{end.x-10,p.y+76},IM_COL32(150,124,72,130));
        float y=p.y+88;
        for(int row=0;row<stats.count;++row) {
            const auto [id,power,unused]=stats.rows[row];
            if(row%2==0)draw->AddRectFilled({p.x+6,y-2},{end.x-6,y+18},IM_COL32(160,128,67,16));
            const auto value=PowerText(id,power,true);
            Theme::Text(draw,{p.x+10,y},StatName(id),13,Color(muted));
            Theme::Text(draw,{end.x-10-Theme::TextWidth(value,14),y},value,14,Color(gold));
            y+=20;
        }
        if(!stats.count) { Theme::Text(draw,{p.x+10,y},"No bonus stats",14,Color(muted)); y+=20; }
        if(Loot::Unique(item))Theme::Text(draw,{p.x+10,y},FitCardText(Loot::WeaponPowerName(item),size.x-20,13),13,Color(gold));
        draw->PopClipRect();
    }
    const bool preview=hovered || (cardNavigationActive && ImGui::IsItemFocused());
    if (present && ImGui::BeginDragDropSource()) {
        // Keep the equipment drop targets visible while an item is being carried.
        compactInspector=false;
        DragItem source{ equipment, index };
        ImGui::SetDragDropPayload(payloadType, &source, sizeof(source));
        ItemImage(item,48);
        Theme::Wrapped("%s", ItemModifierSystem::ItemName(item).c_str());
        ImGui::EndDragDropSource();
    }
    if (ImGui::BeginDragDropTarget()) {
        if(const auto* payload=ImGui::AcceptDragDropPayload(stonePayloadType))
            InsertStone(equipment,index,*static_cast<const int*>(payload->Data));
        if (const auto* payload = ImGui::AcceptDragDropPayload(payloadType)) {
            const auto source = *static_cast<const DragItem*>(payload->Data);
            MoveItem(source.equipment,source.index,equipment,index);
        }
        ImGui::EndDragDropTarget();
    }
    if (present && preview && !socketing && !ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        Theme::BeginTooltip();
        ImGui::PushTextWrapPos(400);
        ItemImage(item,56); ImGui::SameLine(); ImGui::BeginGroup();
        Theme::Colored(Rarity(item), "%s", ItemModifierSystem::ItemName(item).c_str());
        Theme::Muted("%s / Level %u", Loot::Quality(item), item.itemLevel);
        ImGui::EndGroup();
        if(item.socketCount) {
            auto p=ImGui::GetCursorScreenPos();
            for(int n=0;n<item.socketCount;++n)SocketMount(ImGui::GetWindowDrawList(),item.sockets[n],{p.x+18+n*42,p.y+18},32);
            ImGui::Dummy({float(item.socketCount)*42,40});
        }
        CardStats(item,equipment);
        auto reason=ARPG::Inventory::EquipRestriction(item);
        if(!reason.empty()) { ImGui::Spacing(); Theme::Colored(negative,"%s",reason.c_str()); }
        ImGui::PopTextWrapPos();
        Theme::EndTooltip();
    }
    ImGui::EndGroup();
    ImGui::PopID();
}

// [TIME] | [INFO] | Inventory.MoveItem | Drag destinations share the same atomic equip, swap and rejection rules.
bool ARPGInventoryWindow::MoveItem(bool sourceEquipment,int sourceIndex,bool targetEquipment,int targetIndex) {
    using namespace ARPG;
    if(sourceIndex<0 || sourceIndex>=(sourceEquipment?ARPG_LOOT_SLOT_COUNT:ARPG_BACKPACK_CAPACITY)) { return false; }
    if(targetIndex<0 || targetIndex>=(targetEquipment?ARPG_LOOT_SLOT_COUNT:ARPG_BACKPACK_CAPACITY))return false;
    if(sourceEquipment==targetEquipment && sourceIndex==targetIndex) { return false; }
    const auto item=targetEquipment?InventoryRules::Equipped(gARPGSave,targetIndex):gARPGSave.backpack[targetIndex];
    bool present=InventoryRules::Present(item),changed=false;
    if(targetEquipment && !sourceEquipment && gARPGSave.backpack[sourceIndex].slot==targetIndex) {
        changed=ARPG::Inventory::Equip(sourceIndex);
    } else if(!targetEquipment && sourceEquipment) {
        if(!present) { changed=ARPG::Inventory::Unequip(sourceIndex,targetIndex); }
        else if(item.slot==sourceIndex) { changed=ARPG::Inventory::Equip(targetIndex); }
    } else if(!targetEquipment && !sourceEquipment) { changed=InventoryRules::Move(gARPGSave,sourceIndex,targetIndex); }
    if(changed) { selectedEquipment=targetEquipment; selectedIndex=targetIndex; detailTab=0; salvageArmed=-1; }
    else {
        const auto& candidate=sourceEquipment?item:gARPGSave.backpack[sourceIndex];
        std::string reason=ARPG::Inventory::EquipRestriction(candidate);
        if(reason.empty()) { reason="Drop gear on its matching slot or a backpack space."; }
        Notification::Emit({.message=reason,.remainingTime=3.0f});
    }
    return changed;
}

void ARPGInventoryWindow::CSlots() {
    using namespace ARPG;
    Theme::Label("C ITEMS",18,Color(gold));
    ARPGItemInstance selected{};
    if(inspectItem && !inspectMaterial && !inspectNative && selectedIndex>=0 &&
       selectedIndex<(selectedEquipment?ARPG_LOOT_SLOT_COUNT:ARPG_BACKPACK_CAPACITY))
        selected=selectedEquipment?InventoryRules::Equipped(gARPGSave,selectedIndex):gARPGSave.backpack[selectedIndex];
    const bool hasGear=InventoryRules::Present(selected);
    const bool native=inspectItem && inspectNative && selectedNativeSlot>=0 && selectedNativeSlot<24;
    if(hasGear && Loot::WeaponSlot(selected.slot)) {
        if(ARPG::Inventory::GearSlot(selectedNativeSlot)!=selected.slot)
            selectedNativeSlot=ARPG::Inventory::NativeSlot(selected.slot);
        if(selected.slot==ARPG_SLOT_BOTTLE)
            selectedNativeSlot=AdventureJournal::DrawBottleSelector(selectedNativeSlot,ImGui::GetContentRegionAvail().x);
    }
    const bool assignableGear=hasGear && (Loot::WeaponSlot(selected.slot) ||
        (selected.slot>=ARPG_SLOT_SHIELD && selected.slot<=ARPG_SLOT_BOOTS &&
         CVarGetInteger(CVAR_ENHANCEMENT("AssignableTunicsAndBoots"),0)));
    const bool canAssign=native?AdventureJournal::CanAssignItem(selectedNativeSlot):
        assignableGear && ARPG::Inventory::EquipRestriction(selected).empty();
    auto assignGear=[&](bool equipped,int index,int button,int nativeSlot=-1) {
        if(index<0 || index>=(equipped?ARPG_LOOT_SLOT_COUNT:ARPG_BACKPACK_CAPACITY))return;
        auto item=equipped?InventoryRules::Equipped(gARPGSave,index):gARPGSave.backpack[index];
        if(ARPG::Inventory::Assign(equipped,index,button,nativeSlot)) {
            selectedEquipment=true; selectedIndex=item.slot; selectedNativeSlot=nativeSlot;
            inspectItem=true; inspectNative=false; inspectMaterial=false; salvageArmed=-1;
        } else {
            auto reason=ARPG::Inventory::EquipRestriction(item);
            Notification::Emit({.message=reason.empty()?"This item cannot go on a C button.":reason,.remainingTime=3.f});
        }
    };
    auto assignNative=[&](int slot,int button) {
        if(AdventureJournal::AssignItem(slot,button)) {
            selectedNativeSlot=slot; inspectNative=true; inspectItem=true; inspectMaterial=false;
        } else Notification::Emit({.message="This item is unavailable at your current age or is not owned.",.remainingTime=3.f});
    };
    const char* labels[]={"X / C Left","Y / C Down","RB / C Right"};
    const float width=(ImGui::GetContentRegionAvail().x-16)/3;
    for(int button=0;button<3;++button) {
        if(button)ImGui::SameLine(0,8);
        ImGui::PushID(5000+button);
        const auto p=ImGui::GetCursorScreenPos();
        if(ImGui::InvisibleButton("cSlot",{width,88},ImGuiButtonFlags_EnableNav) && canAssign) {
            if(native)assignNative(selectedNativeSlot,button);
            else assignGear(selectedEquipment,selectedIndex,button,selectedNativeSlot);
        }
        const bool hovered=ImGui::IsItemHovered();
        auto* draw=ImGui::GetWindowDrawList();
        Theme::SlotFrame(draw,p,{width,88},hovered || ImGui::IsItemFocused(),
            canAssign?IM_COL32(203,182,108,230):IM_COL32(122,140,160,220));
        Theme::Text(draw,{p.x+(width-Theme::TextWidth(labels[button],14))*.5f,p.y+6},labels[button],14,Color(gold));
        const int item=gSaveContext.equips.buttonItems[button+1];
        if(item>=0 && item<int(ARRAY_COUNT(gItemIcons)) && item!=ITEM_NONE) {
            const float x=p.x+(width-36)*.5f;
            draw->AddImage(Theme::Texture(static_cast<const char*>(gItemIcons[item])),{x,p.y+25},{x+36,p.y+61});
        }
        auto name=FitCardText(item==ITEM_NONE?"Empty":SohUtils::GetItemName(item),width-12,13);
        Theme::Text(draw,{p.x+(width-Theme::TextWidth(name,13))*.5f,p.y+68},name,13,Color(muted));
        if(ImGui::BeginDragDropTarget()) {
            if(const auto* payload=ImGui::AcceptDragDropPayload(payloadType)) {
                const auto source=*static_cast<const DragItem*>(payload->Data);
                int bottle=!inspectNative && source.equipment==selectedEquipment && source.index==selectedIndex?selectedNativeSlot:-1;
                assignGear(source.equipment,source.index,button,bottle);
            }
            if(const auto* payload=ImGui::AcceptDragDropPayload(AdventureJournal::NativeItemPayload))
                assignNative(*static_cast<const int*>(payload->Data),button);
            ImGui::EndDragDropTarget();
        }
        if(hovered && !ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            Theme::BeginTooltip();
            Theme::Wrapped("%s: %s",labels[button],item==ITEM_NONE?"Empty":SohUtils::GetItemName(item).c_str());
            if(canAssign)Theme::Wrapped("Assign %s",native?SohUtils::GetItemName(gSaveContext.inventory.items[selectedNativeSlot]).c_str():ItemModifierSystem::ItemName(selected).c_str());
            else Theme::Muted("Select a usable item, or drop one here.");
            Theme::EndTooltip();
        }
        ImGui::PopID();
    }
    ImGui::Spacing();
}

bool ARPGInventoryWindow::InsertStone(bool equipped,int index,int material,int socket) {
    using namespace ARPG;
    if(!ARPG::Inventory::CanCraft() || !Crafting::Socket(gARPGSave,equipped,index,material,socket)) {
        socketNotice="Choose an empty socket and a stone you own.";
        Notification::Emit({.message=socketNotice,.remainingTime=3.f});
        return false;
    }
    selectedEquipment=equipped; selectedIndex=index;
    inspectItem=true; inspectNative=false; inspectMaterial=false;
    const auto& item=equipped?InventoryRules::Equipped(gARPGSave,index):gARPGSave.backpack[index];
    socketNotice=Loot::MaterialName(material)+" inserted.";
    if(int word=Loot::Runework(item))socketNotice=std::string(Loot::Runeworks[word].name)+" awakened.";
    selectedSocket=-1;
    for(int n=0;n<item.socketCount;++n)if(!item.sockets[n]) { selectedSocket=n; break; }
    return true;
}

void ARPGInventoryWindow::SocketArtwork(const ARPGItemInstance& item,float size,bool interactive) {
    using namespace ARPG;
    const auto start=ImGui::GetCursorScreenPos();
    auto* draw=ImGui::GetWindowDrawList();
    ImGui::BeginGroup();
    Theme::SlotFrame(draw,start,{size,size},false,Color(Rarity(item)));
    DrawItemArtwork(draw,item,{start.x+8,start.y+5},{start.x+size-8,start.y+size-8},IM_COL32_WHITE);
    ImGui::Dummy({size,size});
    const int count=std::min<int>(item.socketCount,ARPG_ITEM_SOCKET_COUNT);
    const float diameter=std::min(42.f,(size-16)/std::max(1,count)-4);
    for(int n=0;n<count;++n) {
        ImGui::PushID(7000+n);
        const ImVec2 center{start.x+size*.5f+(n-(count-1)*.5f)*(diameter+5),start.y+size*.73f};
        ImGui::SetCursorScreenPos({center.x-diameter*.5f,center.y-diameter*.5f});
        if(interactive && ImGui::InvisibleButton("socket",{diameter,diameter},ImGuiButtonFlags_EnableNav)) {
            selectedSocket=n; socketing=true; socketNotice.clear(); focusSocketStone=!item.sockets[n];
            forgeMode=false; stashTab=0; inspectItem=true; inspectMaterial=false; inspectNative=false;
        }
        const bool preview=interactive && (cardNavigationActive?ImGui::IsItemFocused():ImGui::IsItemHovered(ImGuiHoveredFlags_NoNavOverride));
        const bool active=preview || (socketing && selectedSocket==n);
        SocketMount(draw,item.sockets[n],center,diameter,active);
        if(interactive && ImGui::BeginDragDropTarget()) {
            if(const auto* payload=ImGui::AcceptDragDropPayload(stonePayloadType))
                InsertStone(selectedEquipment,selectedIndex,*static_cast<const int*>(payload->Data),n);
            ImGui::EndDragDropTarget();
        }
        if(preview && !ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            Theme::BeginTooltip();
            Theme::Wrapped("Socket %d: %s",n+1,item.sockets[n]?Loot::MaterialName(item.sockets[n]).c_str():"Empty");
            if(item.sockets[n])Theme::Colored(positive,"%s",ItemModifierSystem::DescribeModifier(Loot::SocketBonus(item.sockets[n],item.slot)).c_str());
            else Theme::Muted("Select to choose a stone, or drop one here.");
            Theme::EndTooltip();
        }
        ImGui::PopID();
    }
    ImGui::SetCursorScreenPos(start); ImGui::Dummy({size,size});
    ImGui::EndGroup();
}

void ARPGInventoryWindow::SocketWorkbench() {
    using namespace ARPG;
    if(selectedIndex<0 || selectedIndex>=(selectedEquipment?ARPG_LOOT_SLOT_COUNT:ARPG_BACKPACK_CAPACITY)) { socketing=false; return; }
    const auto item=selectedEquipment?InventoryRules::Equipped(gARPGSave,selectedIndex):gARPGSave.backpack[selectedIndex];
    if(!InventoryRules::Valid(item)) { socketing=false; return; }
    if(Theme::Button("Back to gear",{-1,34})) { socketing=false; return; }
    ImGui::Spacing();
    Theme::Colored(Rarity(item),"%s",ItemModifierSystem::ItemName(item).c_str());
    Theme::Muted("%s / Level %u",Loot::Quality(item),item.itemLevel);
    const float available=ImGui::GetContentRegionAvail().x;
    SocketArtwork(item,std::min(180.f,available*.43f),true);
    ImGui::SameLine(); ImGui::BeginGroup();
    ItemAffixes(item);
    ImGui::Spacing();
    Theme::Muted("%d / %d sockets filled",Loot::Filled(item),item.socketCount);
    ImGui::EndGroup();
    Theme::Rule();
    const bool empty=selectedSocket>=0 && selectedSocket<item.socketCount && !item.sockets[selectedSocket];
    if(empty)Theme::Label(fmt::format("Socket {}  /  Choose a stone",selectedSocket+1),22,Color(gold));
    else Theme::Muted("Select an empty circle. Filled stones can be recovered at the forge.");
    if(!socketNotice.empty())Theme::Colored(positive,"%s",socketNotice.c_str());
    const float tabWidth=(available-8)*.5f;
    if(Theme::Button("Gems",{tabWidth,34},forgeTab==0)) { forgeTab=0; focusSocketStone=true; }
    ImGui::SameLine();
    if(Theme::Button("Runes",{tabWidth,34},forgeTab==1)) { forgeTab=1; focusSocketStone=true; }
    ImGui::Spacing();
    ImGui::BeginChild("socketStones",{0,0});
    const int columns=std::max(1,int((ImGui::GetContentRegionAvail().x+8)/135));
    const float cell=(ImGui::GetContentRegionAvail().x-8*(columns-1))/columns;
    const int first=forgeTab?Loot::RuneFirst:1,last=forgeTab?Loot::RuneLast:18;
    int shown=0;
    for(int id=first;id<=last;++id) {
        if(!gARPGSave.materials[id])continue;
        if(shown++%columns)ImGui::SameLine(0,8);
        ImGui::BeginGroup();
        if(focusSocketStone) { ImGui::SetKeyboardFocusHere(); focusSocketStone=false; }
        const bool chosen=MaterialChoice(id,{cell,94},selectedMaterial==id);
        if((cardNavigationActive?ImGui::IsItemFocused():ImGui::IsItemHovered(ImGuiHoveredFlags_NoNavOverride)) && !ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            selectedMaterial=id;
            Theme::BeginTooltip();
            Theme::Wrapped("%s",Loot::MaterialName(id).c_str());
            Theme::Colored(positive,"%s",ItemModifierSystem::DescribeModifier(Loot::SocketBonus(id,item.slot)).c_str());
            if(empty)Theme::Muted("A / Enter: insert in socket %d",selectedSocket+1);
            Theme::EndTooltip();
        }
        if(chosen && empty)InsertStone(selectedEquipment,selectedIndex,id,selectedSocket);
        ImGui::EndGroup();
    }
    if(!shown) { Theme::Muted(forgeTab?"No runes in your satchel.":"No gems in your satchel."); focusSocketStone=false; }
    ImGui::EndChild();
}

void ARPGInventoryWindow::Inspector() {
    using namespace ARPG;
    if (selectedIndex < 0 || selectedIndex >= (selectedEquipment ? ARPG_LOOT_SLOT_COUNT : ARPG_BACKPACK_CAPACITY)) { return; }
    auto item = selectedEquipment ? InventoryRules::Equipped(gARPGSave,selectedIndex) : gARPGSave.backpack[selectedIndex];
    if (!InventoryRules::Present(item)) {
        Theme::Muted("Select an item to inspect it.");
        return;
    }
    SocketArtwork(item,128,true);
    ImGui::SameLine(); ImGui::BeginGroup();
    Theme::Colored(Rarity(item),"%s",ItemModifierSystem::ItemName(item).c_str());
    Theme::Muted("%s / Lv. %u",Loot::Quality(item),item.itemLevel);
    ImGui::Spacing(); ItemAffixes(item);
    ImGui::EndGroup();
    if(item.socketCount)Theme::Muted("Select an empty circle to socket a gem or rune.");
    ImGui::Spacing();

    auto reason=ARPG::Inventory::EquipRestriction(item);
    if(selectedEquipment) {
        ImGui::BeginDisabled(InventoryRules::FreeSlot(gARPGSave)<0);
        if(Theme::Button("Unequip",{-1,36})) {
            int destination=InventoryRules::FreeSlot(gARPGSave);
            if(ARPG::Inventory::Unequip(selectedIndex,destination)) { selectedEquipment=false; selectedIndex=destination; }
        }
        ImGui::EndDisabled();
        if(InventoryRules::FreeSlot(gARPGSave)<0) { Theme::Muted("Backpack full"); }
    } else {
        ImGui::BeginDisabled(!reason.empty());
        if(Theme::Button("Equip",{-1,36}) && ARPG::Inventory::Equip(selectedIndex)) { selectedEquipment=true; selectedIndex=item.slot; }
        ImGui::EndDisabled();
        if(!reason.empty()) { ImGui::PushStyleColor(ImGuiCol_Text,negative); Theme::Wrapped("%s",reason.c_str()); ImGui::PopStyleColor(); }
    }
    if(Loot::WeaponSlot(item.slot)) {
        if(ARPG::Inventory::GearSlot(selectedNativeSlot)!=item.slot)selectedNativeSlot=ARPG::Inventory::NativeSlot(item.slot);
        AdventureJournal::DrawItemDetails(selectedNativeSlot,false,{},false);
    }
    ImGui::Spacing();
    if((item.uniqueId || item.setId || Loot::Runework(item)) && Theme::Section("Lore & powers")) { ItemDetails(item,false); }

}

void ARPGInventoryWindow::MaterialInspector() {
    using namespace ARPG;
    if(!Loot::Gem(selectedMaterial) && !Loot::Rune(selectedMaterial)) { return; }
    float width=ImGui::GetContentRegionAvail().x;
    auto p=ImGui::GetCursorScreenPos();
    MaterialMark(ImGui::GetWindowDrawList(),selectedMaterial,{p.x+width*.5f,p.y+56},112);
    ImGui::Dummy({width,116});
    Theme::Label(Loot::MaterialName(selectedMaterial),27,Color(gold));
    Theme::Muted("%d in your satchel",gARPGSave.materials[selectedMaterial]);
    ImGui::Spacing();
    const float actionWidth=Loot::Gem(selectedMaterial)?(width-10)*.5f:-1.f;
    if(Theme::Button("Socket stone",{actionWidth,42})) {
        forgeMode=true; forgeOperation=0; forgeTab=Loot::Rune(selectedMaterial)?1:0;
        compactInspector=false; forgeNotice.clear();
    }
    if(Loot::Gem(selectedMaterial)) {
        ImGui::SameLine();
        if(Theme::Button("Refine gem",{actionWidth,42})) {
            forgeMode=true; forgeOperation=3; gemRank=(selectedMaterial-1)/6;
            compactInspector=false; forgeNotice.clear();
        }
    }
    ImGui::Spacing(); Theme::Rule(); ImGui::Spacing();
    Theme::Label("SOCKET BONUSES",21,Color(gold));
    if(ImGui::BeginTable("stoneBonuses",2,ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Gear",0,.28f); ImGui::TableSetupColumn("Power",0,.72f);
        for(int slot=0;slot<ARPG_LOOT_SLOT_COUNT;++slot) {
            ImGui::TableNextRow(); ImGui::TableNextColumn(); Theme::Label(slots[slot],17,Color(muted));
            ImGui::TableNextColumn(); Theme::Colored(positive,"%s",ItemModifierSystem::DescribeModifier(Loot::SocketBonus(selectedMaterial,slot)).c_str());
        }
        ImGui::EndTable();
    }
}

// A dedicated workbench gives each transaction a visible input, result and cost.
// CraftingRules remains the single source of truth for previews and atomic mutations.
void ARPGInventoryWindow::Forge() {
    using namespace ARPG;
    const float width=ImGui::GetContentRegionAvail().x;
    const bool wide=width>=1180;
    auto forgeTop=ImGui::GetCursorScreenPos();
    if(width>=900) { Theme::Heading(ImGui::GetWindowDrawList(),{forgeTop.x+width*.5f-Theme::NativeWidth("THE FORGE",29)*.5f,forgeTop.y+3},"THE FORGE",29); }
    if(Theme::Button("Back to equipment",{200,38})) {
        forgeMode=false; compactInspector=false; forgeChooseItem=false; forgeChooseOperation=false; salvageArmed=-1; forgeNotice.clear(); return;
    }
    ImGui::SameLine();
    auto balance=fmt::format("{} Essence",gARPGSave.materials[Loot::Essence]);
    auto balancePos=ImGui::GetCursorScreenPos();
    balancePos.x+=std::max(0.f,ImGui::GetContentRegionAvail().x-Theme::NativeWidth(balance,23)-40);
    MaterialMark(ImGui::GetWindowDrawList(),Loot::Essence,{balancePos.x+12,balancePos.y+18},30);
    Theme::NativeText(ImGui::GetWindowDrawList(),{balancePos.x+34,balancePos.y+6},balance,23,Color(gold));
    ImGui::Dummy({1,38});

    const char* operations[]={"Insert stone","Add socket","Extract","Refine gems","Runeworks","Salvage"};
    const int columns=width>=900?6:3;
    const float tabWidth=(width-10.f*(columns-1))/columns;
    const bool compactActions=width<700;
    if(compactActions && Theme::Button(fmt::format("{}   /   Change action",operations[forgeOperation]).c_str(),{-1,42},true)) { forgeChooseOperation=!forgeChooseOperation; }
    if(!compactActions || forgeChooseOperation) {
    for(int op=0;op<6;++op) {
        if(op%columns) { ImGui::SameLine(); }
        if(Theme::Button(operations[op],{tabWidth,42},forgeOperation==op)) {
            forgeOperation=op; salvageArmed=-1; forgeNotice.clear(); forgeChooseItem=false; forgeChooseOperation=false;
            if(op==3 && !Loot::Gem(selectedMaterial)) { selectedMaterial=1+gemRank*6; }
            if(op==0) { forgeTab=Loot::Rune(selectedMaterial)?1:0; }
        }
    }
    ImGui::Spacing();
    }
    const bool gearOperation=forgeOperation<=2 || forgeOperation==5;
    auto validSelection=[&]() { return !selectedEquipment && selectedIndex>=0 && selectedIndex<ARPG_BACKPACK_CAPACITY && InventoryRules::Valid(gARPGSave.backpack[selectedIndex]); };
    auto chooseGear=[&]() {
        Label("BACKPACK GEAR");
        Theme::Muted("Choose a piece to work on.");
        ImGui::Spacing();
        const float available=ImGui::GetContentRegionAvail().x;
        const int cols=std::max(2,int((available+8)/84));
        const float cell=(available-8.f*(cols-1))/cols;
        int count=0;
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,{8,8});
        for(int i=0;i<ARPG_BACKPACK_CAPACITY;++i) {
            const auto& item=gARPGSave.backpack[i];
            if(!InventoryRules::Valid(item)) { continue; }
            if(count++%cols) { ImGui::SameLine(); }
            ImGui::PushID(i);
            auto p=ImGui::GetCursorScreenPos();
            if(ImGui::InvisibleButton("forgeGear",{cell,cell},ImGuiButtonFlags_EnableNav)) {
                selectedIndex=i; selectedEquipment=false; salvageArmed=-1; forgeNotice.clear(); forgeChooseItem=false;
            }
            auto* draw=ImGui::GetWindowDrawList();
            Theme::SlotFrame(draw,p,{cell,cell},(!selectedEquipment&&selectedIndex==i)||ImGui::IsItemFocused(),Color(Rarity(item)));
            DrawItemArtwork(draw,item,{p.x+7,p.y+5},{p.x+cell-7,p.y+cell-9},IM_COL32_WHITE);
            for(int n=0;n<item.socketCount;++n) {
                auto center=ImVec2{p.x+cell*.5f+(n-(item.socketCount-1)*.5f)*13,p.y+cell-7};
                draw->AddCircleFilled(center,3,item.sockets[n]?Color(positive):IM_COL32(15,22,41,255),12);
                draw->AddCircle(center,3,Color(gold),12);
            }
            if(ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal | ImGuiHoveredFlags_NoNavOverride)) {
                Theme::BeginTooltip(); Theme::Label(ItemModifierSystem::ItemName(item),23,Color(Rarity(item)));
                Theme::Muted("%s / %d sockets",Loot::Quality(item),item.socketCount); Theme::EndTooltip();
            }
            ImGui::PopID();
        }
        ImGui::PopStyleVar();
        if(!count) { Theme::Wrapped("Your backpack is empty. Unequip a piece of gear or collect some loot first."); }
    };
    const float paneHeight=ImGui::GetContentRegionAvail().y;
    if(wide && gearOperation) {
        Theme::BeginPane("forgeGearPicker",{210,paneHeight}); chooseGear(); Theme::EndPane(); ImGui::SameLine();
    }
    auto item=validSelection()?gARPGSave.backpack[selectedIndex]:ARPGItemInstance{};
    auto gearSummary=[&](bool pedestal) {
        if(!validSelection()) {
            Label("CHOOSE YOUR GEAR");
            Theme::Wrapped(selectedEquipment?"Move equipped gear into the backpack first.":"Choose a backpack item to place on the forge.");
            return;
        }
        float icon=pedestal?140.f:76.f;
        auto p=ImGui::GetCursorScreenPos();
        if(pedestal) { p.x+=(ImGui::GetContentRegionAvail().x-icon)*.5f; }
        ImGui::SetCursorScreenPos(p);
        SocketArtwork(item,icon,true);
        if(!pedestal) { ImGui::SameLine(); ImGui::BeginGroup(); }
        Theme::Label(ItemModifierSystem::ItemName(item),25,Color(Rarity(item)));
        Theme::Muted("%s / %s",Loot::Quality(item),slots[item.slot]);
        if(!pedestal) { ImGui::EndGroup(); }
        ImGui::Spacing();
        Label("SOCKETS");
        for(int n=0;n<item.socketCount;++n) {
            if(n) { ImGui::SameLine(0,10); }
            auto pos=ImGui::GetCursorScreenPos();
            MaterialMark(ImGui::GetWindowDrawList(),item.sockets[n],{pos.x+25,pos.y+25},46);
            ImGui::Dummy({50,50});
            if(ImGui::IsItemHovered()) { Theme::BeginTooltip(); Theme::Wrapped("%d. %s",n+1,Loot::MaterialName(item.sockets[n]).c_str()); Theme::EndTooltip(); }
        }
        if(!item.socketCount) { Theme::Muted("No sockets yet"); }
        else { Theme::Muted("%d of %d sockets filled",Loot::Filled(item),item.socketCount); }
        if(int word=Loot::Runework(item)) { ImGui::Spacing(); Theme::Colored(gold,"%s awakened",Loot::Runeworks[word].name); }
    };
    if(wide && gearOperation) {
        Theme::BeginPane("forgePedestal",{260,paneHeight}); gearSummary(true); Theme::EndPane(); ImGui::SameLine();
    }
    Theme::BeginPane("forgeWorkbench",{0,paneHeight});
    if(!wide && gearOperation) {
        if(forgeChooseItem) {
            if(Theme::Button("Return to workbench",{-1,38})) { forgeChooseItem=false; }
            chooseGear(); Theme::EndPane(); return;
        }
        // A selectable gear strip keeps the full item name and socket art together on small screens.
        auto p=ImGui::GetCursorScreenPos();
        float available=ImGui::GetContentRegionAvail().x;
        ImGui::BeginGroup();
        ImGui::Dummy({68,68}); ImGui::SameLine(); ImGui::BeginGroup();
        Theme::Label(validSelection()?ItemModifierSystem::ItemName(item):"Choose backpack gear",23,validSelection()?Color(Rarity(item)):Color(gold));
        Theme::Muted("Select to change gear");
        ImGui::EndGroup(); ImGui::EndGroup();
        float stripHeight=std::max(76.f,ImGui::GetItemRectSize().y+8);
        ImGui::SetCursorScreenPos(p);
        if(ImGui::InvisibleButton("forgeCurrentGear",{available,stripHeight},ImGuiButtonFlags_EnableNav)) { forgeChooseItem=true; salvageArmed=-1; }
        auto* draw=ImGui::GetWindowDrawList();
        draw->AddRect(p,{p.x+available,p.y+stripHeight},ImGui::IsItemHovered()||ImGui::IsItemFocused()?Color(gold):IM_COL32(127,148,184,200),5);
        if(validSelection()) {
            DrawItemArtwork(draw,item,{p.x+3,p.y+3},{p.x+63,p.y+63},IM_COL32_WHITE);
            for(int n=0;n<item.socketCount;++n) { MaterialMark(draw,item.sockets[n],{p.x+12+n*21,p.y+stripHeight-9},20); }
        } else { EmptyGearMark(draw,0,{p.x+5,p.y+5},58); }
        ImGui::Spacing();
    }
    const bool canChange=ARPG::Inventory::CanCraft();
    auto action=[&](const char* label,bool allowed,const std::string& reason,auto transaction,const std::string& notice) {
        if(!canChange) { Theme::Muted("Crafting is available when Link can change gear."); }
        else if(!allowed) { Theme::Muted("%s",reason.c_str()); }
        ImGui::Spacing();
        ImGui::BeginDisabled(!canChange || !allowed);
        if(Theme::Button(label,{-1,46}) && transaction()) { forgeNotice=notice; salvageArmed=-1; }
        ImGui::EndDisabled();
    };
    auto materials=[&](bool gemsOnly) {
        const float w=ImGui::GetContentRegionAvail().x;
        if(!gemsOnly) {
            if(Theme::Button("Gems",{(w-10)*.5f,36},forgeTab==0)) { forgeTab=0; selectedMaterial=1+gemRank*6; forgeNotice.clear(); }
            ImGui::SameLine();
            if(Theme::Button("Runes",{(w-10)*.5f,36},forgeTab==1)) { forgeTab=1; selectedMaterial=Loot::RuneFirst; forgeNotice.clear(); }
        }
        if(gemsOnly || forgeTab==0) {
            const char* grades[]={"Rough","Cut","Perfect"};
            for(int rank=0;rank<3;++rank) {
                if(rank) { ImGui::SameLine(); }
                if(Theme::IconButton(grades[rank],GemArtwork(1+rank*6+(selectedMaterial-1)%6),{(w-20)/3,40},gemRank==rank)) {
                    gemRank=rank; selectedMaterial=1+rank*6+(selectedMaterial-1)%6; forgeNotice.clear();
                }
            }
        }
        ImGui::Spacing();
        int cols=w>=630?6:3;
        float cell=(w-10.f*(cols-1))/cols;
        for(int n=0;n<6;++n) {
            if(n%cols) { ImGui::SameLine(); }
            int id=(gemsOnly || forgeTab==0)?1+gemRank*6+n:Loot::RuneFirst+n;
            if(MaterialChoice(id,{cell,Loot::Gem(id)?106.f:80.f},selectedMaterial==id)) { selectedMaterial=id; forgeNotice.clear(); }
        }
        ImGui::Spacing();
    };
    if(forgeOperation==0) {
        Label("CHOOSE A STONE");
        materials(false);
        Theme::Label(Loot::MaterialName(selectedMaterial),25,Color(gold));
        if(validSelection()) {
            Theme::Colored(positive,"%s",ItemModifierSystem::DescribeModifier(Loot::SocketBonus(selectedMaterial,item.slot)).c_str());
            auto probe=gARPGSave;
            bool allowed=Crafting::Socket(probe,selectedIndex,selectedMaterial);
            std::string reason=!gARPGSave.materials[selectedMaterial]?"You do not have this stone.":
                !item.socketCount?"Add a socket to this item first.":"All sockets are filled. Extract the stones to replace them.";
            Theme::Muted("Cost: 1 stone / next empty socket");
            action("Insert stone",allowed,reason,[&]() { return Crafting::Socket(gARPGSave,selectedIndex,selectedMaterial); },"Stone inserted.");
        } else { Theme::Muted("Select backpack gear to preview its bonus."); }
    } else if(forgeOperation==1 && validSelection()) {
        Theme::Label("Make room for a new stone",27,Color(gold));
        Theme::Wrapped("Adds one empty socket to this piece of gear.");
        ImGui::Spacing();
        Theme::Label(fmt::format("{}  >  {} sockets",item.socketCount,std::min<int>(item.socketCount+1,Loot::MaxSockets(item))),30,Color(positive));
        Theme::Muted("This item can hold up to %d sockets.",Loot::MaxSockets(item));
        ImGui::Spacing(); Theme::Rule(); ImGui::Spacing();
        Theme::Label(fmt::format("{} Essence",Crafting::SocketCost(item)),27,Color(gold));
        auto probe=gARPGSave;
        bool allowed=Crafting::AddSocket(probe,selectedIndex);
        std::string reason=Loot::Filled(item)?"Extract the stones before adding a socket.":item.socketCount>=Loot::MaxSockets(item)?"This item already has its maximum sockets.":"You need more Essence.";
        action("Add one socket",allowed,reason,[&]() { return Crafting::AddSocket(gARPGSave,selectedIndex); },"New socket added.");
    } else if(forgeOperation==2 && validSelection()) {
        Theme::Label("Recover your stones",27,Color(gold));
        Theme::Wrapped("Returns every stone to your materials. The gear and its sockets stay intact.");
        ImGui::Spacing();
        for(int n=0;n<item.socketCount;++n) {
            if(!item.sockets[n]) { continue; }
            auto p=ImGui::GetCursorScreenPos();
            MaterialMark(ImGui::GetWindowDrawList(),item.sockets[n],{p.x+22,p.y+22},40);
            ImGui::Dummy({48,44}); ImGui::SameLine(); Theme::Label(Loot::MaterialName(item.sockets[n]),23);
        }
        ImGui::Spacing(); Theme::Rule(); ImGui::Spacing();
        Theme::Label("8 Essence",27,Color(gold));
        auto probe=gARPGSave;
        bool allowed=Crafting::Extract(probe,selectedIndex);
        std::string reason=!Loot::Filled(item)?"There are no stones to extract.":gARPGSave.materials[Loot::Essence]<8?"You need more Essence.":"Your materials cannot hold all of these stones.";
        action("Extract all stones",allowed,reason,[&]() { return Crafting::Extract(gARPGSave,selectedIndex); },"Stones returned to your materials.");
    } else if(forgeOperation==3) {
        Label("GEMCUTTING");
        Theme::Muted("Combine three matching gems into one higher grade.");
        materials(true);
        auto p=ImGui::GetCursorScreenPos();
        MaterialMark(ImGui::GetWindowDrawList(),selectedMaterial,{p.x+48,p.y+55},96);
        if(selectedMaterial<=12) {
            Theme::NativeText(ImGui::GetWindowDrawList(),{p.x+101,p.y+40},"x3   >",26,Color(gold));
            MaterialMark(ImGui::GetWindowDrawList(),selectedMaterial+6,{p.x+227,p.y+55},112);
        }
        ImGui::Dummy({292,116});
        Theme::Label(selectedMaterial<=12?Loot::MaterialName(selectedMaterial+6):"Perfect gem",27,Color(gold));
        auto probe=gARPGSave;
        bool allowed=Crafting::UpgradeGem(probe,selectedMaterial);
        std::string reason=selectedMaterial>12?"This gem is already the highest grade.":gARPGSave.materials[selectedMaterial]<3?"You need three of this gem.":"Your higher-grade gem stack is full.";
        action("Combine three gems",allowed,reason,[&]() { return Crafting::UpgradeGem(gARPGSave,selectedMaterial); },"Gem refined.");
    } else if(forgeOperation==4) {
        const float space=ImGui::GetContentRegionAvail().x;
        const bool split=space>=750;
        if(split) { ImGui::BeginChild("recipeList",{230,0}); }
        if(!split && Theme::Button(fmt::format("{}  /  Choose recipe",Loot::Runeworks[recipeIndex].name).c_str(),{-1,42},true)) { forgeChooseRecipe=!forgeChooseRecipe; }
        if(split || forgeChooseRecipe) {
        Label("RUNEWORK RECIPES");
        for(int i=1;i<int(std::size(Loot::Runeworks));++i) {
            if(!split && (i-1)%2) { ImGui::SameLine(); }
            if(Theme::Button(Loot::Runeworks[i].name,{split?-1.f:(space-10)*.5f,40},recipeIndex==i)) { recipeIndex=i; forgeChooseRecipe=false; }
        }
        }
        if(split) { ImGui::EndChild(); ImGui::SameLine(); ImGui::BeginChild("recipePreview",{0,0}); }
        else { ImGui::Spacing(); Theme::Rule(); }
        const auto& recipe=Loot::Runeworks[recipeIndex];
        Theme::Label(recipe.name,32,Color(gold));
        std::string bases;
        for(int slot=0;slot<ARPG_LOOT_SLOT_COUNT;++slot) { if(recipe.slotMask&(1<<slot)) { if(!bases.empty()) { bases+=" / "; } bases+=slots[slot]; } }
        Theme::Muted("Common %s / exactly %d sockets",bases.c_str(),recipe.count);
        Theme::Wrapped("Insert these runes from left to right.");
        ImGui::Spacing();
        float runeWidth=std::min(130.f,(ImGui::GetContentRegionAvail().x-20)/3);
        for(int n=0;n<recipe.count;++n) {
            if(n) { ImGui::SameLine(); }
            ImGui::PushID(n);
            if(MaterialChoice(recipe.runes[n],{runeWidth,80},false)) {
                selectedMaterial=recipe.runes[n]; forgeTab=1; forgeOperation=0; forgeNotice.clear();
            }
            ImGui::PopID();
        }
        ImGui::Spacing(); Label("AWAKENED POWER");
        for(auto mod:recipe.mods) { if(mod.modId) { Theme::Colored(positive,"%s",ItemModifierSystem::DescribeModifier(mod).c_str()); } }
        const char* trees[]={"Sword","Magic","Guardian"};
        if(recipe.style.power) { Theme::Wrapped("%s technique damage +%d%%",trees[recipe.style.tree],recipe.style.power); }
        if(recipe.style.recovery) { Theme::Wrapped("%s recharge -%d%%",trees[recipe.style.tree],recipe.style.recovery); }
        if(recipe.style.manaOnHit) { Theme::Wrapped("Hits restore %d magic",recipe.style.manaOnHit); }
        if(recipe.style.lifeOnKill) { Theme::Wrapped("Kills restore %.2f hearts",recipe.style.lifeOnKill/16.f); }
        if(recipe.style.combatPower!=Loot::CombatPower::None && Theme::Section(Loot::PowerName(recipe.style.combatPower))) { Theme::Wrapped("%s",Loot::PowerDescription(recipe.style.combatPower)); }
        if(Theme::Button("Prepare these runes",{-1,44})) {
            forgeOperation=0; forgeTab=1; selectedMaterial=recipe.runes[0]; forgeNotice.clear();
        }
        if(split) { ImGui::EndChild(); }
    } else if(forgeOperation==5 && validSelection()) {
        Theme::Label("Reclaim Forge Essence",27,Color(gold));
        Theme::Wrapped("Destroy this piece of gear to recover its Essence.");
        ImGui::Spacing();
        auto p=ImGui::GetCursorScreenPos();
        MaterialMark(ImGui::GetWindowDrawList(),Loot::Essence,{p.x+40,p.y+40},78);
        ImGui::Dummy({90,82}); ImGui::SameLine();
        Theme::Label(fmt::format("+{} Essence",Crafting::SalvageValue(item)),30,Color(gold));
        ImGui::Spacing(); Theme::Rule(); ImGui::Spacing();
        auto probe=gARPGSave;
        bool allowed=Crafting::Salvage(probe,selectedIndex);
        if(salvageArmed==selectedIndex) {
            Theme::Colored(negative,"Destroy %s?",ItemModifierSystem::ItemName(item).c_str());
            Theme::Wrapped("This cannot be undone.");
            action("Confirm salvage",allowed,"Extract the stones first, or make room for Essence.",[&]() { return Crafting::Salvage(gARPGSave,selectedIndex); },"Gear salvaged. Essence recovered.");
            if(Theme::Button("Keep this item",{-1,40})) { salvageArmed=-1; }
        } else {
            action("Salvage this item",allowed,Loot::Filled(item)?"Extract the stones before salvaging this gear.":"Your Essence stack is full.",[&]() { salvageArmed=selectedIndex; return false; },"");
        }
    }
    if(gearOperation && !validSelection() && forgeOperation!=0 && forgeNotice.empty()) { Theme::Muted("Choose backpack gear to begin."); }
    if(!forgeNotice.empty()) { ImGui::Spacing(); Theme::Colored(positive,"%s",forgeNotice.c_str()); }
    Theme::EndPane();
}

// [TIME] | [INFO] | Inventory.Submenus | Keep crafting, character totals and testing out of the everyday loot view.
void ARPGInventoryWindow::Character() {
    using namespace ARPG;
    Theme::BeginPane("characterBonuses",{std::min(700.f,ImGui::GetContentRegionAvail().x),0},true);
    if(Theme::Button("Back to equipment",{240,36})) { utilityPage=0; }
    ImGui::Spacing(); Label("ACTIVE GEAR BONUSES");
    bool bonuses = false;
    ImGui::BeginTable("gearBonuses", 2, ImGuiTableFlags_SizingStretchProp);
    ImGui::TableSetupColumn("Stat", 0, 0.66f);
    ImGui::TableSetupColumn("Bonus", 0, 0.34f);
    for (int id = 1; id < ARPG_MOD_COUNT_; ++id) {
        int power = ItemModifierSystem::GetModPower(id);
        if (power <= 0) { continue; }
        bonuses = true;
        ImGui::TableNextRow(); ImGui::TableNextColumn();
        ImGui::PushStyleColor(ImGuiCol_Text, muted); ARPG::Theme::Wrapped("%s", StatName(id)); ImGui::PopStyleColor();
        ImGui::TableNextColumn(); ARPG::Theme::Colored(positive, "%s", PowerText(id, power).c_str());
    }
    ImGui::EndTable();
    auto style=ItemModifierSystem::GetStyleBonuses();
    const char* treeNames[]={"Sword","Magic","Guardian"};
    for(int tree=0;tree<3;++tree) {
        if(style.power[tree]) { Theme::Colored(positive,"%s technique damage +%d%%",treeNames[tree],style.power[tree]); }
        if(style.recovery[tree]) { Theme::Colored(positive,"%s recharge -%d%%",treeNames[tree],style.recovery[tree]); }
    }
    if(style.manaOnHit) { Theme::Colored(positive,"Magic on hit +%d",style.manaOnHit); }
    if(style.lifeOnKill) { Theme::Colored(positive,"Hearts on kill +%.2f",style.lifeOnKill/16.f); }
    if (!bonuses) { ARPG::Theme::Wrapped("Equip rolled gear to activate its modifiers."); }

    Theme::EndPane();
}
void ARPGInventoryWindow::More() {
    using namespace ARPG;
    Theme::BeginPane("journalMore",{0,0},true);
    Label("ADVENTURE");
    if(Theme::Button("Character bonuses",{360,44})) { utilityPage=1; }
    if(Theme::Button("Forge & materials",{360,44})) { utilityPage=0; forgeMode=true; }
    if(Theme::Button("Gear",{360,44})) { SelectPage(0); }
    if(Theme::Button("Quest status & songs",{360,44})) { utilityPage=5; campaignPage=false; }
    if(Theme::Button("Map",{360,44})) { utilityPage=6; }
    if(Theme::Button("Save game",{360,44})) { utilityPage=7; }
    if(Theme::Button("Fellowship & chat",{360,44})) { SelectPage(8); }
    if(Theme::Button("Arsenal & enemy counters",{360,44})) { utilityPage=8; }
    ImGui::Spacing(); Theme::Rule(); ImGui::Spacing();
    Label("OPTIONS");
    if(Theme::Button("Settings & testing",{360,44})) { OpenSettings(); }
    if(Testing::Active()) {
        ImGui::Spacing(); Label("TEST ADVENTURE");
        if(Theme::Button("Preview matching gear",{360,40})) { Testing::PreviewGear(); }
        if(Theme::Button("Preview starting equipment",{360,40})) { Testing::PreviewBaseGear(); }
        if(Theme::Button("Forge test kit",{360,40})) { Testing::GiveCraftingKit(); utilityPage=0; forgeMode=true; }
    }
    Theme::EndPane();
}

int ARPGInventoryWindow::CurrentPage() const {
    if(utilityPage==9) { return 8; }
    if(utilityPage==3 || utilityPage==4) { return 0; }
    if(utilityPage==5) { return 4; }
    if(utilityPage==6) { return 5; }
    if(utilityPage) { return 7; }
    if(expeditionPage) { return 6; }
    return ARPG::SkillUI::JournalSelected()?3:0;
}
void ARPGInventoryWindow::SelectPage(int page) {
    if(page==1 || page==2) page=0; // Items and equipment share Gear.
    utilityPage=page==1?3:page==2?4:page==4?5:page==5?6:page==7?2:page==8?9:0;
    expeditionPage=page==6;
    ARPG::SkillUI::SelectJournal(page==3);
    socketing=false;
    forgeMode=false; compactInspector=false; showFilters=false; forgeChooseItem=false; forgeChooseOperation=false; forgeNotice.clear(); salvageArmed=-1;
}
void ARPGInventoryWindow::DrawElement() {
    using namespace ARPG;
    const auto& input=ImGui::GetIO();
    if(input.MouseDelta.x!=0 || input.MouseDelta.y!=0 || ImGui::IsMouseClicked(ImGuiMouseButton_Left))cardNavigationActive=false;
    for(auto key:{ImGuiKey_Tab,ImGuiKey_LeftArrow,ImGuiKey_RightArrow,ImGuiKey_UpArrow,ImGuiKey_DownArrow,
                  ImGuiKey_GamepadDpadLeft,ImGuiKey_GamepadDpadRight,ImGuiKey_GamepadDpadUp,ImGuiKey_GamepadDpadDown,
                  ImGuiKey_GamepadLStickLeft,ImGuiKey_GamepadLStickRight,ImGuiKey_GamepadLStickUp,ImGuiKey_GamepadLStickDown})
        if(ImGui::IsKeyPressed(key))cardNavigationActive=true;
    if(utilityPage==3 || utilityPage==4) SelectPage(0);
    if(forgeMode && !utilityPage && !expeditionPage && !SkillUI::JournalSelected()) {
        AdventureJournal::StopSong(); AdventureJournal::CancelSave();
        ImGui::BeginChild("forgePage",{0,std::max(100.f,ImGui::GetContentRegionAvail().y-25)});
        Forge();
        ImGui::EndChild();
        auto footer=ImGui::GetCursorScreenPos();
        auto width=ImGui::GetContentRegionAvail().x;
        ImGui::GetWindowDrawList()->AddRectFilled({footer.x-4,footer.y-2},{footer.x+width+4,footer.y+21},IM_COL32(16,24,47,230),4);
        Theme::Text(ImGui::GetWindowDrawList(),footer,"Esc  Gear     [ / ]  Journal pages",16,IM_COL32(215,224,241,255));
        ImGui::Dummy({width,20});
        return;
    }
    auto* draw=ImGui::GetWindowDrawList();
    auto start=ImGui::GetCursorScreenPos();
    float widthAvailable=ImGui::GetContentRegionAvail().x;
    bool compact=ImGui::GetWindowWidth()<860;
    const char* title=utilityPage==9?"FELLOWSHIP":utilityPage==3?"ADVENTURE ITEMS":utilityPage==4?"EQUIPMENT":utilityPage==5?"HYRULE REBORN":utilityPage==6?"ATLAS OF HYRULE":utilityPage==7?"SAVE ADVENTURE":utilityPage==8?"ARSENAL":utilityPage==1?"CHARACTER":utilityPage==2?"ADVENTURE JOURNAL":expeditionPage?"EXPEDITIONS":SkillUI::JournalSelected()?"SKILL JOURNAL":forgeMode?"THE FORGE":"GEAR";
    float titleSize=25.f;
    Theme::Heading(draw,start,title,titleSize);
    float actionWidth=compact?78.f:106.f;
    ImGui::SetCursorScreenPos({start.x+widthAvailable-actionWidth*2-8,start.y+5});
    if(Theme::Button("Save",{actionWidth,34},utilityPage==7)) { utilityPage=7; expeditionPage=false; SkillUI::SelectJournal(false); }
    ImGui::SameLine(0,8);
    if(Theme::Button("Resume",{actionWidth,34})) { ARPG::Inventory::Close(); }
    ImGui::SetCursorScreenPos({start.x,start.y+48});
    const char* pages[]={"Gear","Gear","Gear","Skills","Quests","Map","Expeditions","More","Fellowship"};
    const int pageItems[]={ITEM_SWORD_MASTER,ITEM_BOMB,ITEM_SHIELD_HYLIAN,ITEM_SWORD_KOKIRI,
        ITEM_MEDALLION_FOREST,ITEM_DUNGEON_MAP,ITEM_COMPASS,ITEM_STONE_OF_AGONY,ITEM_MEDALLION_LIGHT};
    const int visiblePages[]={0,3,4,5,6,8,7};
    int columns=widthAvailable>=850?7:3;
    float tabWidth=(widthAvailable-static_cast<float>((columns-1)*8))/static_cast<float>(columns);
    static bool compactPages=false;
    const bool pageDrawer=widthAvailable<850;
    if(pageDrawer) {
        auto current=CurrentPage();
        auto icon=current==3?Theme::Artwork("skills/sword-cleave"):Theme::Texture(static_cast<const char*>(gItemIcons[pageItems[current]]));
        if(Theme::IconButton(fmt::format("{}  /  Change page",pages[current]).c_str(),icon,{-1,40},compactPages)) { compactPages=!compactPages; }
    }
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,{8,7});
    if(!pageDrawer || compactPages) {
    for(int tabIndex=0;tabIndex<7;++tabIndex) {
        int i=visiblePages[tabIndex];
        if(tabIndex%columns) { ImGui::SameLine(); }
        auto icon=widthAvailable<1080?nullptr:i==3?Theme::Artwork("skills/sword-cleave"):Theme::Texture(static_cast<const char*>(gItemIcons[pageItems[i]]));
        if(Theme::IconButton(pages[i],icon,{tabWidth,38},utilityPage!=7 && CurrentPage()==i)) { SelectPage(i); compactPages=false; }
    }
    }
    ImGui::PopStyleVar();
    ImGui::Spacing();
    // Keep navigation outside the scrolling page. Every page owns only the available body area.
    ImGui::BeginChild("pausePage",{0,std::max(100.f,ImGui::GetContentRegionAvail().y-28)},ImGuiChildFlags_None);
    if (utilityPage!=5) { AdventureJournal::StopSong(); }
    if (utilityPage!=7) { AdventureJournal::CancelSave(); }
    if (utilityPage==5) {
        float tab=(ImGui::GetContentRegionAvail().x-8)/2;
        if(Theme::Button("Campaign quests",{tab,34},campaignPage))campaignPage=true;
        ImGui::SameLine();
        if(Theme::Button("Quest items & songs",{tab,34},!campaignPage))campaignPage=false;
        ImGui::Spacing();
        if(campaignPage) { AdventureJournal::StopSong(); Quests::DrawJournal(); }
        else AdventureJournal::Draw(AdventureJournal::Page::Quest);
    }
    else if (utilityPage>=3 && utilityPage<=7) { AdventureJournal::Draw(static_cast<AdventureJournal::Page>(utilityPage-3)); }
    else if (utilityPage==9) { Fellowship::DrawJournal(); }
    else if (utilityPage==1) { Character(); }
    else if (utilityPage==2) { More(); }
    else if (utilityPage==8) {
        Theme::BeginPane("arsenalGuide",{0,0},true);
        if(Theme::Button("Back to More",{240,36})) { utilityPage=2; }
        ImGui::Spacing(); Arsenal::DrawGuide(); Theme::EndPane();
    }
    else if (expeditionPage) { Expeditions::DrawJournal(); }
    else if (SkillUI::JournalSelected()) { SkillUI::DrawJournal(); }
    else {
    float width = ImGui::GetContentRegionAvail().x;
    float height=ImGui::GetContentRegionAvail().y;
    compactInspector=false;
    if(compact && compactInspector) {
        Theme::BeginPane("compactDetails",{0,0},true);
        if(resetDetailsScroll) { ImGui::SetScrollY(0); resetDetailsScroll=false; }
        if(Theme::Button("Back to inventory",{220,34})) { compactInspector=false; inspectItem=false; forgeMode=false; }
        ImGui::Spacing();
        if(inspectMaterial) { MaterialInspector(); }
        else if(inspectNative) { AdventureJournal::DrawItemDetails(selectedNativeSlot); } else { Inspector(); }
        Theme::EndPane();
    } else {
        const float stashWidth=compact?width:(width-12)*.53f;
        {
            Theme::BeginPane("backpack",{stashWidth,compact?std::max(360.f,height*.6f):height});
            { Theme::Label(fmt::format("Backpack   {} / {}",InventoryRules::Count(gARPGSave),ARPG_BACKPACK_CAPACITY),18,Color(gold)); }
            const char* categories[]={"Gear","Gems","Runes"};
            float categoryWidth=(ImGui::GetContentRegionAvail().x-20)/3;
            for(int tab=0;tab<3;++tab) {
                if(tab) { ImGui::SameLine(); }
                if(Theme::Button(categories[tab],{categoryWidth,34},stashTab==tab)) {
                    stashTab=tab; socketing=false;
                    if(tab) {
                        if(tab==1 && !Loot::Gem(selectedMaterial)) { selectedMaterial=1+gemRank*6; }
                        if(tab==2 && !Loot::Rune(selectedMaterial)) { selectedMaterial=Loot::RuneFirst; }
                        inspectNative=false; inspectMaterial=true; inspectItem=false;
                    } else { inspectMaterial=false; inspectItem=false; }
                }
            }
            ImGui::Spacing();
            if(stashTab==0) {
                const char* filters[]={"All","Swords","Shields","Tunics","Boots","Bows","Hammers","Bombs","Seeds","Hookshots","Boomerangs","Nuts","Bombchus","Sticks","Lenses","Ocarinas","Bottles"};
                static_assert(sizeof(filters)/sizeof(filters[0])==ARPG_LOOT_SLOT_COUNT+1);
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x-146);
                ImGui::InputTextWithHint("##search","Search gear",search,sizeof(search));
                ImGui::SameLine(); ImGui::SetNextItemWidth(136);
                showFilters=ImGui::BeginCombo("##gearFilter",filters[filter+1],ImGuiComboFlags_HeightLarge);
                if(showFilters) {
                    for(int n=0;n<=ARPG_LOOT_SLOT_COUNT;++n) {
                        if(ImGui::Selectable(filters[n],filter==n-1)) { filter=n-1; }
                        if(filter==n-1) { ImGui::SetItemDefaultFocus(); }
                    }
                    ImGui::EndCombo();
                }
                ImGui::Spacing();
                ImGui::BeginChild("grid",{0,std::max(80.f,ImGui::GetContentRegionAvail().y-46)});
                float available=ImGui::GetContentRegionAvail().x;
                int columns=std::clamp(int((available+8)/304),1,3);
                float cell=(available-8.f*(columns-1))/columns;
                struct Entry { ARPGItemInstance item; int index; bool equipped; int nativeSlot=-1; };
                std::vector<Entry> entries;
                int lastOccupied=-1;
                for(int i=0;i<ARPG_BACKPACK_CAPACITY;++i) { if(InventoryRules::Present(gARPGSave.backpack[i])) { lastOccupied=i; } }
                // Keep one empty row for rearranging, instead of pages of empty slots.
                int shownSlots=std::min(ARPG_BACKPACK_CAPACITY,std::max(columns*3,((lastOccupied+columns)/columns+1)*columns));
                const auto query=Lower(search);
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,{8,8});
                // Native adventure items use the same gear cards and stats as dropped loot.
                for(int slot=ARPG_SLOT_BOW;slot<ARPG_LOOT_SLOT_COUNT;++slot) {
                    const auto active=InventoryRules::Equipped(gARPGSave,slot);
                    if(!InventoryRules::Present(active) || (filter>=0 && active.slot!=filter) ||
                       (!query.empty() && Lower(ItemModifierSystem::ItemName(active)).find(query)==std::string::npos))continue;
                    entries.push_back({active,slot,true});
                }
                // Spells, trade items and elemental arrows have native inventory
                // slots but no separate loot roll. Keep them in this same Gear list.
                for(int slot=0;slot<24;++slot) {
                    const int item=gSaveContext.inventory.items[slot], gearSlot=ARPG::Inventory::GearSlot(slot);
                    if(item==ITEM_NONE || item==ITEM_SOLD_OUT ||
                       (gearSlot>=0 && slot!=SLOT_ARROW_FIRE && slot!=SLOT_ARROW_ICE && slot!=SLOT_ARROW_LIGHT) ||
                       (filter>=0 && gearSlot!=filter) ||
                       (!query.empty() && Lower(SohUtils::GetItemName(item)).find(query)==std::string::npos))continue;
                    entries.push_back({{},slot,false,slot});
                }
                for(int i=0;i<shownSlots;++i) {
                    const auto& item=gARPGSave.backpack[i];
                    if((filter>=0 && (!InventoryRules::Present(item) || item.slot!=filter)) ||
                       (!query.empty() && (!InventoryRules::Present(item) || Lower(ItemModifierSystem::ItemName(item)+" "+(ItemAppearance::Get(item).name?ItemAppearance::Get(item).name:"")).find(query)==std::string::npos))) { continue; }
                    entries.push_back({item,i,false});
                }
                // Reserve only the rows needed by the items themselves; comparisons are transient.
                for(size_t row=0;row<entries.size();row+=columns) {
                    float cardHeight=108;
                    const auto stop=std::min(entries.size(),row+columns);
                    for(size_t i=row;i<stop;++i)cardHeight=std::max(cardHeight,ItemCardHeight(entries[i].item,entries[i].equipped));
                    for(size_t i=row;i<stop;++i) {
                        if(i!=row)ImGui::SameLine();
                        const auto& entry=entries[i];
                        if(entry.nativeSlot>=0) {
                            if(AdventureJournal::DrawNativeItemCard(entry.nativeSlot,cell,cardHeight,
                                inspectNative && selectedNativeSlot==entry.nativeSlot,cardNavigationActive)) {
                                socketing=false; selectedNativeSlot=entry.nativeSlot; inspectNative=true; inspectItem=true; inspectMaterial=false;
                            }
                        }
                        else Card(entry.item,entry.index,entry.equipped,{cell,cardHeight});
                    }
                }
                ImGui::PopStyleVar();
                if(entries.empty()) {
                    Theme::Muted("No matching gear");
                    if(Theme::Button("Clear filters",{-1,34})) { filter=-1; search[0]=0; }
                }
                ImGui::EndChild();
                float action=(ImGui::GetContentRegionAvail().x-10)*.5f;
                if(Theme::Button("Forge",{action,36})) { forgeMode=true; compactInspector=false; salvageArmed=-1; forgeNotice.clear(); }
                ImGui::SameLine();
                if(Theme::Button("Character",{action,36})) { utilityPage=1; }
            } else {
                ImGui::BeginChild("materialSatchel",{0,std::max(80.f,ImGui::GetContentRegionAvail().y-(compact?0:50))});
                float available=ImGui::GetContentRegionAvail().x;
                int columns=available>=500?6:3;
                float cell=(available-8.f*(columns-1))/columns;
                const char* grades[]={"ROUGH","CUT","PERFECT"};
                for(int rank=0;rank<(stashTab==1?3:1);++rank) {
                    if(stashTab==1) { Theme::Label(grades[rank],20,Color(gold)); }
                    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,{8,8});
                    for(int kind=0;kind<6;++kind) {
                        if(kind%columns) { ImGui::SameLine(); }
                        int id=stashTab==1?1+rank*6+kind:Loot::RuneFirst+kind;
                        if(MaterialChoice(id,{cell,stashTab==1?96.f:80.f},selectedMaterial==id)) {
                            selectedMaterial=id; if(stashTab==1) { gemRank=rank; }
                            inspectMaterial=true; inspectItem=true; compactInspector=compact;
                        }
                    }
                    ImGui::PopStyleVar();
                }
                ImGui::EndChild();
                if(!compact && Theme::Button("Open the forge",{-1,42})) { forgeMode=true; forgeOperation=stashTab==1?3:0; forgeTab=stashTab==1?0:1; compactInspector=false; forgeNotice.clear(); }
            }
            Theme::EndPane();
        }
        if(!compact) { ImGui::SameLine(0,12); }
        {
            Theme::BeginPane("equipment",{0,compact?std::max(560.f,height):height});
            Theme::Label(socketing?"Sockets":"Equipped gear",20,Color(gold));
            ImGui::BeginChild("loadoutContent",{0,0});
            if(socketing) { SocketWorkbench(); } else {
            auto contentStart=ImGui::GetCursorScreenPos();
            float available=ImGui::GetContentRegionAvail().x;
            float unit=std::min((available-32)/3.f,100.f),gap=16;
            float tall=std::max(105.f,unit*1.28f);
            auto origin=contentStart;origin.x+=(available-unit*3-gap*2)*.5f;
            // Four worn slots in the familiar paper-doll arrangement.
            auto* paper=ImGui::GetWindowDrawList();
            ImVec2 center{origin.x+unit*1.5f+gap,origin.y+tall};
            if(auto crest=Theme::Artwork("journal-crest"))paper->AddImage(crest,
                {center.x-66,center.y-95},{center.x+66,center.y+65},{0,0},{1,1},IM_COL32(181,146,77,48));
            auto equip=[&](int slot,float x,float y,float h) {
                ImGui::SetCursorScreenPos({origin.x+x,origin.y+y});
                Card(InventoryRules::Equipped(gARPGSave,slot),slot,true,{unit,h});
            };
            equip(ARPG_SLOT_SWORD,0,30,tall*1.25f);
            equip(ARPG_SLOT_TUNIC,unit+gap,0,tall);
            equip(ARPG_SLOT_SHIELD,2*(unit+gap),30,tall*1.25f);
            equip(ARPG_SLOT_BOOTS,unit+gap,tall+12,tall*.78f);
            ImGui::SetCursorScreenPos({contentStart.x,origin.y+tall*1.78f+28});
            Theme::Rule();
            if(stashTab==0) { CSlots(); Theme::Rule(); }
            if(inspectMaterial) { MaterialInspector(); }
            else if(inspectNative) { AdventureJournal::DrawItemDetails(selectedNativeSlot,true,{},false); }
            else if(inspectItem) { Inspector(); }
            else { Theme::Muted("Select gear, then choose its slot. Or drag it onto a slot."); }
            }
            ImGui::EndChild();
            Theme::EndPane();
        }
    }
    }
    ImGui::EndChild();
    auto footer=ImGui::GetCursorScreenPos();
    draw->AddRectFilled({footer.x-4,footer.y-2},{footer.x+widthAvailable+4,footer.y+20},IM_COL32(16,24,47,195),4);
    if(!compact && CurrentPage()==0) { Theme::Text(draw,ImGui::GetCursorScreenPos(),"Hover to compare    Select or drag to equip",15.f,IM_COL32(191,204,174,255)); }
    {
        const char* controls=compact?"[ / ] Pages     Esc Back":"[ / ] or LB / RB  Pages     Esc  Back";
        auto hp=ImGui::GetCursorScreenPos();
        float font=compact?14.f:15.f;
        Theme::Text(draw,{hp.x+widthAvailable-Theme::TextWidth(controls,font),hp.y},controls,font,IM_COL32(191,204,174,255));
    }

}

// The journal owns the pause interface; the engine retains only its pause lifecycle.
void ARPGInventoryWindow::Draw() {
    ARPG::Inventory::ObservePause();
    if(ARPG::Quests::ConsumeJournalRequest()) {SelectPage(4);campaignPage=true;ARPG::Inventory::Open();}
    if (ARPG::SkillUI::JournalSelected()) { expeditionPage=false; utilityPage=0; }
    if (ARPG::ItemsEnabled() && !ImGui::GetIO().WantTextInput && !showFilters && ImGui::IsKeyPressed(ImGuiKey_I, false)) {
        if (IsVisible() && !ARPG::SkillUI::JournalSelected() && !expeditionPage && !utilityPage && !forgeMode) { ARPG::Inventory::Close(); } else { SelectPage(0); ARPG::Inventory::Open(); }
    }
    if (!IsVisible() || !ARPG::Enabled()) { return; }
    if(!ImGui::GetIO().WantTextInput && (ImGui::IsKeyPressed(ImGuiKey_Escape,false) || ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight,false))) {
        if(showFilters) { showFilters=false; } // ImGui closes the dropdown; keep its parent page open.
        else if(socketing) { socketing=false; }
        else if(compactInspector) { compactInspector=false; inspectItem=false; }
        else if(utilityPage==8) { utilityPage=2; }
        else if(utilityPage || forgeMode || expeditionPage || ARPG::SkillUI::JournalSelected()) { SelectPage(0); }
        else if(inspectItem) { inspectItem=false; inspectNative=false; resetDetailsScroll=true; }
        else { ARPG::Inventory::Close(); return; }
    }
    if(!ImGui::GetIO().WantTextInput && !showFilters) {
        if(ImGui::IsKeyPressed(ImGuiKey_LeftBracket,false) || ImGui::IsKeyPressed(ImGuiKey_GamepadL1,false)) { const int order[]={0,3,4,5,6,8,7}; auto at=std::find(std::begin(order),std::end(order),CurrentPage()); SelectPage(order[(int(at-std::begin(order))+6)%7]); }
        if(ImGui::IsKeyPressed(ImGuiKey_RightBracket,false) || ImGui::IsKeyPressed(ImGuiKey_GamepadR1,false)) { const int order[]={0,3,4,5,6,8,7}; auto at=std::find(std::begin(order),std::end(order),CurrentPage()); SelectPage(order[(int(at-std::begin(order))+1)%7]); }
    }
    if(ImGui::IsKeyPressed(ImGuiKey_GamepadStart,false)) { ARPG::Inventory::Close(); return; }
    if (GameInteractor::IsPlayerInControl() && gPlayState->pauseCtx.state == 0) { ARPG::Inventory::Open(); }
    auto* viewport = ImGui::GetMainViewport();
    float width = std::min(1540.0f, viewport->WorkSize.x - 24);
    float height = std::min(1000.0f, viewport->WorkSize.y - 24);
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x * 0.5f,
                                 viewport->WorkPos.y + viewport->WorkSize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18, 18));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10, 6));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 6));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 1);
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 11);
    ImGui::PushStyleColor(ImGuiCol_ScrollbarBg,{0,0,0,0});
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab,{.7f,.75f,.9f,.6f});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.5f, 0.61f, 0.8f, .8f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.94f, 0.94f, 0.86f, 1));
    ImGui::PushStyleColor(ImGuiCol_TextDisabled, muted);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.2f, 0.34f, 1));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.24f, 0.33f, 0.47f, 1));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.35f, 0.42f, 0.57f, 1));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.07f, 0.11f, 0.2f, .9f));
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.23f, 0.33f, 0.47f, 1));
    ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.62f, 0.61f, 0.45f, 1));
    ImGui::PushStyleColor(ImGuiCol_NavCursor, ImVec4(.95f,.82f,.5f,1));
    if (ImGui::Begin("Loot Inventory", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoMove)) {
        auto* draw = ImGui::GetWindowDrawList();
        draw->PushClipRectFullScreen();
        draw->AddRectFilled(viewport->Pos, {viewport->Pos.x+viewport->Size.x,viewport->Pos.y+viewport->Size.y},IM_COL32(5,12,9,210));
        draw->PopClipRect();
        ARPG::Theme::Panel(draw, ImGui::GetWindowPos(), ImGui::GetWindowSize());
        DrawElement();
    }
    ImGui::End();
    ImGui::PopStyleColor(14);
    ImGui::PopStyleVar(7);
}
