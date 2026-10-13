#include "SkillUI.h"
#include "HUDLayout.h"
#include "ItemModifierSystem.h"
#include "ARPGTheme.h"
#include "EnemySystem.h"
#include "ARPGSave.h"
#include "SkillSystem.h"
#include "SkillRules.h"
#include "SkillPatterns.h"
#include "SkillDetails.h"
#include "ARPG.h"
#include "ARPGSave.h"
#include "ARPGTesting.h"
#include "InventorySystem.h"
#include "ControllerConfig.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/cvar_prefixes.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include <imgui.h>
#include <spdlog/fmt/fmt.h>
#include <string>

extern "C" {
#include "variables.h"
extern PlayState* gPlayState;
}
namespace ARPG::SkillUI {
namespace {
bool journal = false;
int tree = ARPG_TREE_SWORD, selected = ARPG_SKILL_SWORD_CLEAVE;
const ImVec4 gold{ 1.0f, 0.88f, 0.52f, 1 }, muted{ 0.68f, 0.76f, 0.81f, 1 };
const ImVec4 colors[] = { { 0.45f, 0.87f, 0.78f, 1 }, { 0.58f, 0.65f, 1, 1 }, { 0.94f, 0.73f, 0.40f, 1 } };
const char* trees[] = { "SWORD", "MAGIC", "GUARDIAN" };
const char* chords[] = { "D Up", "D Right", "D Down", "D Left", "LS Click", "RS Click" };
const ImGuiKey keys[] = { ImGuiKey_1, ImGuiKey_2, ImGuiKey_3, ImGuiKey_4, ImGuiKey_5, ImGuiKey_6,
    ImGuiKey_Q, ImGuiKey_E, ImGuiKey_R, ImGuiKey_F, ImGuiKey_G, ImGuiKey_H, ImGuiKey_Z, ImGuiKey_X, ImGuiKey_C, ImGuiKey_V };
const char* keyNames[] = { "1", "2", "3", "4", "5", "6", "Q", "E", "R", "F", "G", "H", "Z", "X", "C", "V" };
std::string KeyCvar(int slot) { return std::string(CVAR_ENHANCEMENT("ARPG.SkillKey")) + std::to_string(slot); }
int KeyIndex(int slot) { return std::clamp(CVarGetInteger(KeyCvar(slot).c_str(), slot), 0, 15); }
void Icon(int id, ImVec2 p, float size, ImU32 color) {
    auto* draw = ImGui::GetWindowDrawList();
    auto pt=[&](float x,float y) { return ImVec2(p.x+x*size,p.y+y*size); };
    auto line=[&](float x,float y,float u,float v) { draw->AddLine(pt(x,y),pt(u,v),color,size*.07f); };
    auto arc=[&](float r,float a,float b) { draw->PathArcTo(p,size*r,a,b,18); draw->PathStroke(color,0,size*.065f); };
    auto sword=[&]() { line(-.25f,.32f,.28f,-.32f); line(-.27f,.03f,-.03f,.25f); };
    switch(id) {
        case ARPG_SKILL_SWORD_CLEAVE: sword(); arc(.43f,-1.5f,1.3f); break;
        case ARPG_SKILL_SWORD_CROSSCUT: sword(); line(-.3f,-.32f,.25f,.32f); break;
        case ARPG_SKILL_SWORD_CYCLONE: arc(.37f,-1.5f,3.9f); arc(.19f,.4f,5.5f); break;
        case ARPG_SKILL_SWORD_LUNGE:
            line(-.4f,0,.38f,0); line(.38f,0,.10f,-.25f); line(.38f,0,.10f,.25f);
            line(-.36f,-.15f,-.1f,-.15f); line(-.36f,.15f,-.1f,.15f); break;
        case ARPG_SKILL_SWORD_BLADESTORM:
            arc(.2f,0,6.28f);
            for(int i=0;i<3;++i) { float a=i*2.094f; ImVec2 q=pt(std::sin(a)*.36f,std::cos(a)*.36f);
                draw->AddTriangleFilled({q.x,q.y-size*.16f},{q.x-size*.065f,q.y+size*.09f},{q.x+size*.065f,q.y+size*.09f},color); }
            break;
        case ARPG_SKILL_SWORD_EXECUTE:
            line(0,-.42f,0,.35f); line(-.21f,-.2f,.21f,-.2f); line(0,.35f,-.18f,.15f); line(0,.35f,.18f,.15f);
            line(-.35f,.33f,-.2f,.42f); line(.35f,.33f,.2f,.42f); break;
        case ARPG_SKILL_SWORD_SHOCKWAVE:
            line(-.4f,-.05f,-.12f,.04f); line(-.12f,.04f,0,-.3f); line(0,-.3f,.13f,.26f); line(.13f,.26f,.4f,.05f); arc(.41f,.15f,2.8f); break;
        case ARPG_SKILL_SWORD_BEAM: arc(.4f,-1.4f,1.4f); arc(.25f,-1.4f,1.4f); line(-.4f,0,-.08f,0); break;
        case ARPG_SKILL_MAGIC_FLAME_BURST:
            draw->AddTriangleFilled(pt(0,-.45f),pt(-.15f,.3f),pt(.27f,.28f),color);
            draw->AddTriangleFilled(pt(-.34f,-.12f),pt(-.3f,.29f),pt(.03f,.33f),color); break;
        case ARPG_SKILL_MAGIC_FROST_NOVA:
            for(int i=0;i<6;++i) { float a=i*1.0472f; line(0,0,std::sin(a)*.42f,std::cos(a)*.42f);
                line(std::sin(a)*.27f,std::cos(a)*.27f,std::sin(a+.3f)*.36f,std::cos(a+.3f)*.36f); } break;
        case ARPG_SKILL_MAGIC_CHAIN:
            line(.15f,-.43f,-.16f,.02f); line(-.16f,.02f,.2f,-.02f); line(.2f,-.02f,-.12f,.43f);
            line(-.25f,-.27f,-.4f,0); line(.32f,.1f,.4f,.28f); break;
        case ARPG_SKILL_MAGIC_METEOR:
            draw->AddCircleFilled(pt(-.10f,.12f),size*.2f,color,8);
            line(.04f,-.02f,.34f,-.38f); line(-.15f,-.12f,.1f,-.42f); line(.13f,.19f,.42f,-.12f); break;
        case ARPG_SKILL_MAGIC_HOLY_AURA:
            draw->AddQuad(pt(0,-.43f),pt(.32f,0),pt(0,.43f),pt(-.32f,0),color,size*.065f); line(0,-.24f,0,.24f); break;
        case ARPG_SKILL_MAGIC_INFUSE: sword(); line(.18f,.15f,.4f,-.05f); line(.30f,.3f,.43f,.17f); break;
        case ARPG_SKILL_MAGIC_RECALL: arc(.35f,-2.2f,2.8f); line(-.28f,-.28f,-.4f,-.02f); line(-.28f,-.28f,.02f,-.26f); break;
        case ARPG_SKILL_GUARD_DODGE: line(-.2f,-.35f,-.02f,.08f); line(-.02f,.08f,.30f,.30f); line(.3f,.3f,-.14f,.30f); line(-.42f,0,-.27f,0); line(-.42f,.17f,-.19f,.17f); break;
        case ARPG_SKILL_GUARD_WARCRY: draw->AddTriangle(pt(-.3f,-.13f),pt(.15f,-.32f),pt(.15f,.28f),color,size*.065f); arc(.42f,-.55f,.55f); break;
        case ARPG_SKILL_GUARD_RIPOSTE:
            draw->AddQuad(pt(-.3f,-.28f),pt(.3f,-.28f),pt(.24f,.15f),pt(0,.4f),color,size*.06f); line(-.3f,-.28f,-.24f,.15f); line(-.24f,.15f,0,.4f); line(-.12f,.05f,.12f,-.12f); break;
        default: arc(.3f,0,6.28f); break;
    }
}
// [TIME] | [INFO] | SkillUI.Art | Every skill owns an original painting, shared by journal nodes and the action bar.
void Art(int id,ImVec2 center,float size,bool learned=true) {
    static constexpr const char* art[] = {
        "sword-crit", "sword-combo", "sword-shockwave", "sword-quickspin", "sword-beam", "sword-mastery",
        "magic-flame-burst", "magic-frost-nova", "magic-holy-aura", "magic-infuse", "magic-economy", "magic-recovery",
        "guard-toughness", "guard-bulwark", "guard-vengeance", "guard-bloodstone", "guard-profit", "guard-fortune",
        "sword-resolve", "magic-recall", "guard-dodge", "sword-cleave", "sword-crosscut", "sword-cyclone",
        "sword-lunge", "sword-bladestorm", "sword-execute", "magic-chain", "magic-meteor", "guard-warcry",
        "guard-riposte", "sword-tempo", "sword-finesse", "magic-resonance", "magic-channel", "guard-second-wind", "guard-iron-will"
    };
    static_assert(std::size(art) == ARPG_SKILL_COUNT_);
    if(id<0 || id>=ARPG_SKILL_COUNT_) { return; }
    auto* draw=ImGui::GetWindowDrawList();
    const auto tint=learned?IM_COL32_WHITE:IM_COL32(155,165,178,180);
    if(auto texture=Theme::Artwork((std::string("skills/")+art[id]).c_str())) {
        draw->AddImage(texture,{center.x-size*.5f,center.y-size*.5f},{center.x+size*.5f,center.y+size*.5f},{0,0},{1,1},tint);
    } else { Icon(id,center,size*.8f,tint); }
}
void Tooltip(int id) {
    if (id < 0 || id >= ARPG_SKILL_COUNT_) { return; }
    ARPG::Theme::BeginTooltip(); ImGui::PushTextWrapPos(ImGui::GetFontSize() * 25);
    Theme::Colored(colors[ARPG_SKILLS[id].tree], "%s", ARPG_SKILLS[id].name);
    Theme::Wrapped("%s", ARPG_SKILLS[id].desc);
    if (SkillRules::Castable(id)) {
        Theme::Print("Cooldown %.1fs  /  Magic %d", SkillRules::Cooldown(gARPGSave, id) / 20.0f, SkillSystem::TechniqueMagicCost(id));
        if(ItemPowers::FreeSpell(SkillSystem::ItemPowerState(),id)) Theme::Wrapped("Moonwell weave: this spell is free.");
        Theme::Print("Rune: %s", SkillRules::RuneAt(id, gARPGSave.skillRunes[id]).name);
    }
    ImGui::PopTextWrapPos(); ARPG::Theme::EndTooltip();
}
void ActionSlot(int slot, bool editing, float width) {
    int id = gARPGSave.actionSlots[slot];
    bool valid = SkillRules::Castable(id);
    ImGui::PushID(slot + 1000);
    auto pos = ImGui::GetCursorScreenPos();
    float scale=editing ? 1.0f : HUDLayout::Scale(ImGui::GetMainViewport()->WorkSize.y);
    float height = (editing ? 68 : 78)*scale;
    ImVec4 color = valid ? colors[ARPG_SKILLS[id].tree] : muted;
    bool learned = valid && SkillSystem::HasSkill(id);
    int remaining = SkillSystem::CooldownRemaining(id);
    if (ImGui::InvisibleButton("##slot", ImVec2(width, height),ImGuiButtonFlags_EnableNav)) {
        if (editing && SkillRules::Castable(selected) && SkillSystem::HasSkill(selected)) { SkillSystem::AssignSlot(slot, selected); }
        else if (!editing && learned) { SkillSystem::RequestCast(id); ImGui::SetWindowFocus(nullptr); }
    }
    auto* draw = ImGui::GetWindowDrawList();
    ImVec2 center{pos.x + width * .5f, pos.y + (editing ? 33 : 35)*scale};
    float radius = (editing ? 18 : 21)*scale;
    auto tint = learned ? (valid && ARPG_SKILLS[id].tree == ARPG_TREE_SWORD ? IM_COL32(54,170,81,255) :
                           valid && ARPG_SKILLS[id].tree == ARPG_TREE_MAGIC ? IM_COL32(77,101,236,255) : IM_COL32(227,184,39,255)) : IM_COL32(90,95,103,220);
    Theme::ButtonFace(draw, center, radius, tint);
    if (editing && (ImGui::IsItemHovered() || ImGui::IsItemFocused())) { Theme::Cursor(draw,{center.x-radius-3,center.y-radius-3},{radius*2+6,radius*2+6}); }
    if (remaining > 0) {
        float fraction = std::clamp(remaining / float(std::max(1, SkillSystem::CooldownTotal(id))), 0.0f, 1.0f);
        draw->AddCircleFilled(center, radius-1, IM_COL32(0,0,0,155));
        draw->PathArcTo(center, radius+2, -1.5708f, -1.5708f+6.28318f*fraction, 32);
        draw->PathStroke(IM_COL32(246,227,158,255),0,2.5f);
    }
    Art(valid ? id : -1,center,(editing?36:42)*scale,learned && !remaining);
    if(!editing && learned && !remaining && ItemPowers::FreeSpell(SkillSystem::ItemPowerState(),id)) {
        draw->AddCircle(center,radius+4,IM_COL32(135,232,255,255),32,2.5f);
        Theme::Text(draw,{center.x+radius-7,center.y+radius-10},"0",14*scale,IM_COL32(190,250,255,255));
    }
    const char* binding=editing || ControllerConfig::GamepadConnected()?chords[slot]:keyNames[KeyIndex(slot)];
    Theme::Text(draw,{center.x-Theme::TextWidth(binding,14*scale)*.5f,pos.y},binding,14*scale,IM_COL32(255,234,150,255));
    if (remaining > 0) {
        auto text = fmt::format("{:.1f}", remaining / 20.0f);
        Theme::Text(draw,{center.x-Theme::TextWidth(text,19*scale)/2,center.y-9*scale},text,19*scale);
    }
    const char* name = valid ? ARPG_SKILLS[id].name : "Empty";
    float font = std::min(16.0f*scale,16.0f*(width-3)/std::max(1.0f,Theme::TextWidth(name,16)));
    Theme::Text(draw,{pos.x+(width-Theme::TextWidth(name,font))/2,pos.y+height-17*scale},name,font,
                learned ? IM_COL32(255,248,213,255) : IM_COL32(147,156,165,255));
    if (ImGui::IsItemHovered()) { Tooltip(id); }
    if (editing && ImGui::BeginPopupContextItem("slotOptions")) {
        if (ImGui::MenuItem("Clear this slot")) { SkillSystem::AssignSlot(slot, ARPG_SKILL_NONE); }
        ImGui::EndPopup();
    }
    if (editing && ImGui::BeginDragDropTarget()) {
        if (const auto* payload = ImGui::AcceptDragDropPayload("ARPG_SKILL")) { if (payload->DataSize == sizeof(int)) { SkillSystem::AssignSlot(slot, *static_cast<const int*>(payload->Data)); } }
        ImGui::EndDragDropTarget();
    }
    ImGui::PopID();
}
}

bool JournalSelected() { return journal; }
void SelectJournal(bool selectedJournal) { journal = selectedJournal; }
void OpenJournal() { journal = true; Inventory::Open(); }

void DrawJournal() {
    if (!SkillsEnabled()) { Theme::Wrapped("Enable Skill Trees & XP in settings to use the skill journal."); return; }
    Theme::Colored(gold,"%u passive points    /    %u technique points",gARPGSave.passivePoints,gARPGSave.activePoints);
    float width=ImGui::GetContentRegionAvail().x;
    for(int i=0;i<ARPG_TREE_COUNT_;++i) {
        if(i) { ImGui::SameLine(); }
        auto label=fmt::format("{}   {}",trees[i],SkillRules::Investment(gARPGSave,i));
        if(Theme::Button(label.c_str(),{width/3-8,38},tree==i)) {
            tree=i; for(int id=0;id<ARPG_SKILL_COUNT_;++id) { if(ARPG_SKILLS[id].tree==tree) { selected=id; break; } }
        }
    }
    float height=std::max(200.0f,ImGui::GetContentRegionAvail().y-126);
    Theme::BeginPane("skillCatalog",{width*.55f,height});
    if (!ImGui::GetIO().WantTextInput && !ImGui::GetIO().NavActive) {
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) { selected=SkillTree::Navigate(selected,-1,0); }
        if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) { selected=SkillTree::Navigate(selected,1,0); }
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) { selected=SkillTree::Navigate(selected,0,-1); }
        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) { selected=SkillTree::Navigate(selected,0,1); }
    }
    Theme::Muted("Select a learned technique, then choose its slot below.");
    ImVec2 origin=ImGui::GetCursorScreenPos();
    float cellWidth=ImGui::GetContentRegionAvail().x/4;
    float cellHeight=std::max(110.0f,(height-62)/5);
    constexpr float artSize=56;
    auto centerOf=[&](const SkillTree::Node& node) {
        return ImVec2{origin.x+(node.column+.5f)*cellWidth,origin.y+node.row*cellHeight+35};
    };
    auto* draw=ImGui::GetWindowDrawList();
    for(const auto& node:SkillTree::nodes) {
        if(ARPG_SKILLS[node.skill].tree!=tree) { continue; }
        auto to=centerOf(node);
        for(auto parent:node.parents) {
            if(parent.skill<0) { continue; }
            auto from=centerOf(*SkillTree::Find(parent.skill));
            bool met=gARPGSave.skillRanks[parent.skill]>=parent.rank;
            auto ink=met?IM_COL32(154,183,110,170):IM_COL32(99,127,151,100);
            float join=to.y-42;
            draw->AddLine({from.x,from.y+52},{from.x,join},ink,2);
            draw->AddLine({from.x,join},{to.x,join},ink,2);
            draw->AddLine({to.x,join},{to.x,to.y-31},ink,2);
            draw->AddTriangleFilled({to.x-4,to.y-35},{to.x+4,to.y-35},{to.x,to.y-30},ink);
        }
    }
    for(const auto& node:SkillTree::nodes) {
        int id=node.skill;
        const auto& def=ARPG_SKILLS[id]; if(def.tree!=tree) { continue; }
        ImGui::PushID(id);
        ImVec2 p{origin.x+node.column*cellWidth,origin.y+node.row*cellHeight};
        ImGui::SetCursorScreenPos(p);
        int rank=SkillSystem::GetSkillRank(id);
        bool available=SkillSystem::CanLearn(id);
        if(ImGui::InvisibleButton("##skill",{cellWidth,100},ImGuiButtonFlags_EnableNav)) { selected=id; }
        auto center=centerOf(node);
        if(available && !rank) { draw->AddCircle(center,33,IM_COL32(204,192,106,160),32,1.2f); }
        Art(id,center,artSize,rank>0);
        if(selected==id || ImGui::IsItemHovered() || ImGui::IsItemFocused()) { Theme::Cursor(draw,{center.x-32,center.y-32},{64,64}); }
        float font=std::min(19.0f,19*(cellWidth-6)/std::max(1.0f,Theme::TextWidth(def.name,19)));
        Theme::Text(draw,{p.x+(cellWidth-Theme::TextWidth(def.name,font))*.5f,center.y+31},def.name,font,
                    selected==id?IM_COL32(255,242,96,255):rank?IM_COL32(255,255,235,255):IM_COL32(176,185,195,255));
        float dots=def.maxRank*10.f;
        for(int n=0;n<def.maxRank;++n) {
            float x=center.x-dots*.5f+n*10+5,y=center.y+31+font+9;
            draw->AddCircleFilled({x,y},3,n<rank?IM_COL32(151,233,124,255):IM_COL32(43,53,69,255));
            draw->AddCircle({x,y},3,IM_COL32(167,183,187,140),12);
        }
        if(!available && !rank) { Theme::Text(draw,{center.x+24,center.y-30},fmt::format("{}",SkillRules::Definition(id).level),16,IM_COL32(179,187,197,255)); }
        if(ImGui::IsItemHovered()) { Tooltip(id); }
        if(rank && SkillRules::Castable(id) && ImGui::BeginDragDropSource()) {
            ImGui::SetDragDropPayload("ARPG_SKILL",&id,sizeof(id)); Theme::Label(def.name); ImGui::EndDragDropSource();
        }
        ImGui::PopID();
    }
    ImGui::SetCursorScreenPos({origin.x,origin.y+5*cellHeight});
    ImGui::Dummy({1,1});
    Theme::EndPane(); ImGui::SameLine();
    Theme::BeginPane("skillDetails",{0,height},true);
    const auto& def = ARPG_SKILLS[selected];
    int rank = SkillSystem::GetSkillRank(selected);
    Theme::Label(def.name,30,IM_COL32(255,242,132,255));
    Theme::Muted("%s   /   Rank %d of %d",def.passive?"Passive":"Technique",rank,def.maxRank);
    Theme::Wrapped("%s", def.desc);
    ImGui::Spacing(); Theme::Rule(); ImGui::Spacing();
    if (SkillRules::Castable(selected)) {
        auto current=SkillDetails::Inspect(gARPGSave,selected), next=SkillDetails::Inspect(gARPGSave,selected,true);
        auto gearStyle=ItemModifierSystem::GetStyleBonuses();
        for(auto* stats:{&current,&next}) { stats->power*=1+gearStyle.power[def.tree]/100.f; stats->cooldown=std::max(1,int(std::lround(stats->cooldown*(1-gearStyle.recovery[def.tree]/100.f)))); }
        Theme::Muted(rank==def.maxRank?"Current rank":rank?"Current rank  >  Next rank":"First rank");
        auto stat=[&](const char* label,float value,float upgraded,const char* unit) {
            if(rank==def.maxRank || value==upgraded) { Theme::Print("%s  %.1f%s",label,value,unit); }
            else { Theme::Print("%s  %.1f%s  >  %.1f%s",label,value,unit,upgraded,unit); }
        };
        stat("Recharge",current.cooldown/20.0f,next.cooldown/20.0f,"s");
        Theme::Print("Cost  %d magic",SkillSystem::TechniqueMagicCost(selected));
        if(current.power>0) {
            stat("Strike power",current.power*100,next.power*100,"%");
            Theme::Muted("Includes synergies and gear style; before level and Momentum.");
            Theme::Print("Strike windows  %d",current.strikes);
        }
        if(current.duration) { stat(selected==ARPG_SKILL_GUARD_DODGE?"Invulnerability":"Duration",current.duration/20.0f,next.duration/20.0f,"s"); }
        if(current.healing) { stat("Healing",current.healing/16.0f,next.healing/16.0f," hearts"); }
    }
    auto gate = SkillRules::Definition(selected);
    if(rank<def.maxRank) {
        Theme::Muted("Level %d / %d    Discipline %d / %d",gARPGSave.level,gate.level,
                     SkillRules::Investment(gARPGSave,tree,selected),gate.investment);
        for(auto parent:SkillTree::Find(selected)->parents) {
            if(parent.skill<0) { continue; }
            bool met=gARPGSave.skillRanks[parent.skill]>=parent.rank;
            Theme::Colored(met?colors[0]:gold,"%s  %s %d / %d",met?"Learned":"Requires",ARPG_SKILLS[parent.skill].name,gARPGSave.skillRanks[parent.skill],parent.rank);
        }
    }
    int points = def.passive ? gARPGSave.passivePoints : gARPGSave.activePoints;
    ImGui::BeginDisabled(!SkillSystem::CanLearn(selected) || rank >= def.maxRank || points == 0);
    auto learn = rank==def.maxRank?"Mastered":rank?"Improve rank   /   1 point":"Learn   /   1 point";
    bool learnPressed=Theme::Button(learn, { -1, 34 });
    bool learnKey=!ImGui::GetIO().WantTextInput && !ImGui::GetIO().NavActive && ImGui::IsKeyPressed(ImGuiKey_Enter,false);
    if (learnPressed || learnKey) { SkillSystem::SpendPoint(selected); }
    ImGui::EndDisabled();
    if (SkillRules::Castable(selected)) {
        if(selected==ARPG_SKILL_MAGIC_INFUSE) {
            ImGui::Spacing(); Theme::Label("Choose an element",24,IM_COL32(255,233,147,255));
            int element=std::clamp(CVarGetInteger(CVAR_ENHANCEMENT("ARPG.InfusionElement"),0),0,2);
            const char* elements[]={"Fire","Frost","Light"};
            ImGui::BeginDisabled(!rank || SkillSystem::CooldownRemaining(selected) || SkillSystem::BuffRemaining(selected));
            for(int choice=0;choice<3;++choice) {
                if(choice) { ImGui::SameLine(); }
                if(Theme::Button(elements[choice],{100,32},choice==element)) {
                    CVarSetInteger(CVAR_ENHANCEMENT("ARPG.InfusionElement"),choice);CVarSave();
                }
            }
            ImGui::EndDisabled();
        }
        ImGui::Spacing(); Theme::Label("Choose a rune",25,IM_COL32(255,233,147,255));
        for (int rune = 0; rune < 3; ++rune) {
            auto variant = SkillRules::RuneAt(selected, rune);
            ImGui::PushID(rune + 100);
            bool can = SkillRules::CanRune(gARPGSave, selected, rune) && !SkillSystem::CooldownRemaining(selected) && !SkillSystem::BuffRemaining(selected);
            ImGui::BeginDisabled(!can);
            auto label = fmt::format("{}   /   Lv {}",variant.name,SkillRules::RuneLevel(rune));
            if (Theme::Button(label.c_str(), { -1, 34 }, gARPGSave.skillRunes[selected] == rune)) { SkillSystem::SelectRune(selected, rune); }
            ImGui::EndDisabled();
            Theme::Wrapped("%s", variant.description);
            ImGui::PopID();
        }

    }
    bool hasSynergy=false;
    for(auto link:SkillTree::synergies) { if(link.target==selected || link.source==selected) { hasSynergy=true; break; } }
    if(hasSynergy) {
        ImGui::Spacing(); Theme::Label("Related skills",24,IM_COL32(255,233,147,255));
        if(SkillTree::SynergyPercent(gARPGSave,selected)) { Theme::Print("Combined power bonus  +%d%%",SkillTree::SynergyPercent(gARPGSave,selected)); }
        for(auto link:SkillTree::synergies) {
            if(link.target!=selected && link.source!=selected) { continue; }
            bool incoming=link.target==selected; int related=incoming?link.source:link.target;
            ImGui::PushID(related+500);
            auto label=fmt::format("{} {}   +{}% / rank",incoming?"From":"Boosts",ARPG_SKILLS[related].name,link.percent);
            if(Theme::Button(label.c_str(),{-1,30})) { selected=related; tree=ARPG_SKILLS[related].tree; ImGui::PopID(); break; }
            ImGui::PopID();
        }
    }
    ImGui::Spacing();
    if (Theme::Section("Build and controls")) {
        Theme::Wrapped("D-pad: abilities 1-4. Left/right stick click: 5-6. LB + D-pad left/right also casts 5-6.");
        Theme::Wrapped("X / Y / RB: C items. LT: target. RT: shield. Hold LB: sprint.");
        Theme::Wrapped("Basic sword hits build up to 5 Momentum. Your next sword technique consumes all stacks. Momentum expires after 8 seconds without a hit.");
        Theme::Print("Damage per Momentum stack: +%d%%",8+3*SkillSystem::GetSkillRank(ARPG_SKILL_SWORD_FINESSE));
        Theme::Print("Basic melee multiplier: x%.2f", SkillSystem::GetMeleeDamageMult());
        for (int slot = 0; slot < ARPG_ACTION_SLOTS; ++slot) {
            ImGui::PushID(slot + 200); int key = KeyIndex(slot);
            auto keyLabel=fmt::format("{}   /   Slot {}   /   {}",keyNames[key],slot+1,chords[slot]);
            if(Theme::Button(keyLabel.c_str(),{-1,32})) {
                int previous=key; key=(key+1)%16;
                for(int other=0;other<ARPG_ACTION_SLOTS;++other) { if(other!=slot && KeyIndex(other)==key) { CVarSetInteger(KeyCvar(other).c_str(),previous); } }
                CVarSetInteger(KeyCvar(slot).c_str(),key); CVarSave();
            }
            ImGui::PopID();
        }
        if (Theme::Button("Refund all skill points")) { SkillSystem::RespecAll(); }
        Theme::Wrapped("Free Cleave and Evasion remain learned. Changing slots, ranks or builds does not erase cooldowns.");

    }
    if (Testing::Active() && Theme::Section("Testing builds")) {
        if (Theme::Button("Swordmaster")) { SkillSystem::PrepareTestBuild(0); }
        ImGui::SameLine(); if (Theme::Button("Spellblade")) { SkillSystem::PrepareTestBuild(1); }
        ImGui::SameLine(); if (Theme::Button("Warden")) { SkillSystem::PrepareTestBuild(2); }

    }
    Theme::EndPane();
    Theme::BeginPane("techniqueAssignments",{0,120},true);
    Theme::Label("ASSIGNED TECHNIQUES",17,IM_COL32(236,227,168,255));
    float slotWidth = (ImGui::GetContentRegionAvail().x - 5 * ImGui::GetStyle().ItemSpacing.x) / 6;
    for (int slot = 0; slot < ARPG_ACTION_SLOTS; ++slot) { if (slot) { ImGui::SameLine(); } ActionSlot(slot, true, slotWidth); }
    Theme::EndPane();
}

void DrawHotbar() {
    if (!SkillsEnabled()) { return; }
    if (!ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(ImGuiKey_K, false)) {
        if (Inventory::IsOpen() && journal) { Inventory::Close(); } else { OpenJournal(); }
    }
    if (!GameInteractor::IsPlayerInControl() || Inventory::IsOpen()) { return; }
    if (!ImGui::GetIO().WantTextInput && !ImGui::GetIO().WantCaptureKeyboard) {
        for (int slot = 0; slot < ARPG_ACTION_SLOTS; ++slot) {
            if (ImGui::IsKeyPressed(keys[KeyIndex(slot)], false)) { SkillSystem::RequestCast(gARPGSave.actionSlots[slot]); }
        }
    }
    if (!CVarGetInteger(CVAR_ENHANCEMENT("ARPG.HUD"),1)) { return; }
    auto* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos({ viewport->WorkPos.x + viewport->WorkSize.x * .5f, viewport->WorkPos.y + viewport->WorkSize.y - 14 }, ImGuiCond_Always, { .5f, 1 });
    ImGui::SetNextWindowBgAlpha(0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize,0);
    const float hudScale = HUDLayout::Scale(viewport->WorkSize.y);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 6, 4 });
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 6 * hudScale, 3 * hudScale });
    ImGui::Begin("##ARPGActionBar", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing);
    float slotWidth = HUDLayout::SlotWidth(viewport->WorkSize.x, hudScale);
    float barWidth = slotWidth * 6 + 30 * hudScale;
    auto* draw=ImGui::GetWindowDrawList();
    auto p=ImGui::GetCursorScreenPos();
    Theme::Text(draw,{p.x,p.y},fmt::format("Lv. {}",gARPGSave.level),17*hudScale);
    uint32_t start=XpForLevel(gARPGSave.level-1),end=XpForLevel(gARPGSave.level);
    float xp=gARPGSave.level>=ARPG_MAX_LEVEL?1.0f:std::clamp((gARPGSave.xp-start)/float(std::max(1u,end-start)),0.0f,1.0f);
    draw->AddRectFilled({p.x+65*hudScale,p.y+7},{p.x+150*hudScale,p.y+12},IM_COL32(12,27,25,200));
    draw->AddRectFilled({p.x+66*hudScale,p.y+8},{p.x+66*hudScale+83*hudScale*xp,p.y+11},IM_COL32(88,214,94,255));
    draw->AddRect({p.x+65*hudScale,p.y+7},{p.x+150*hudScale,p.y+12},IM_COL32(192,212,171,230));
    for(int i=0;i<5;++i) {
        float x=p.x+barWidth*.5f-36+i*18,y=p.y+13;
        draw->AddQuadFilled({x,y-4},{x+4,y},{x,y+4},{x-4,y},i<SkillSystem::Momentum()?IM_COL32(254,224,116,255):IM_COL32(54,80,83,230));
        draw->AddQuad({x,y-4},{x+4,y},{x,y+4},{x-4,y},IM_COL32(199,207,165,210));
    }
    Theme::Text(draw,{p.x+barWidth-125*hudScale,p.y},ControllerConfig::GamepadConnected()?"Menu: Gear / Skills":"I Gear    K Skills",15*hudScale,IM_COL32(230,230,184,220));
    ImGui::Dummy({barWidth,22*hudScale});
    for (int slot = 0; slot < ARPG_ACTION_SLOTS; ++slot) { if (slot) { ImGui::SameLine(); } ActionSlot(slot, false, slotWidth); }
    for (int id : { ARPG_SKILL_MAGIC_INFUSE, ARPG_SKILL_MAGIC_HOLY_AURA, ARPG_SKILL_GUARD_RIPOSTE, ARPG_SKILL_GUARD_WARCRY, ARPG_SKILL_SWORD_BLADESTORM }) {
        if (int time = SkillSystem::BuffRemaining(id); time > 0) {
            auto text=fmt::format("{} {:.1f}s",ARPG_SKILLS[id].name,time/20.0f);
            auto at=ImGui::GetCursorScreenPos(); Theme::Text(draw,at,text,15*hudScale,ImGui::ColorConvertFloat4ToU32(colors[ARPG_SKILLS[id].tree]));
            ImGui::Dummy({Theme::TextWidth(text,15*hudScale)+10,18*hudScale}); ImGui::SameLine();
        }
    }
    ImGui::End(); ImGui::PopStyleVar(3);
}
}
