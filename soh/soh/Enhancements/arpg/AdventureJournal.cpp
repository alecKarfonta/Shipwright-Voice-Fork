#include "AdventureJournal.h"
#include "ARPGTheme.h"
#include "InventorySystem.h"
#include "InventoryRules.h"
#include "ARPGSave.h"
#include "ExplorationRules.h"
#include "ItemModifierSystem.h"
#include "ARPG.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/util.h"
#include "soh/SohGui/ImGuiUtils.h"
#include "soh/Enhancements/kaleido.h"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/cvar_prefixes.h"
#include "soh/ResourceManagerHelpers.h"
#include <libultraship/libultraship.h>
#include <fast/Fast3dGui.h>
#include <fast/resource/type/Texture.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <string>
extern "C" {
#include "../../../src/overlays/misc/ovl_kaleido_scope/z_kaleido_scope.h"
#include "textures/icon_item_field_static/icon_item_field_static.h"
#include "textures/parameter_static/parameter_static.h"
#include "textures/icon_item_static/icon_item_static.h"
#include "textures/icon_item_24_static/icon_item_24_static.h"
extern PlayState* gPlayState;
void KaleidoScope_AssignItem(PlayState* play, u16 item, u16 slot, u16 button);
void PauseMapMark_Init(PlayState* play);
void AudioOcarina_SetUiInput(int enabled, u16 buttons);
u8 KaleidoScope_GetCycleItem(u8 slot, int direction);
void KaleidoScope_GetWorldMapRect(int index, int* x, int* y, int* width, int* height);
}

namespace ARPG::AdventureJournal {
namespace {
int selectedSlot = 0, selectedSong = QUEST_SONG_LULLABY, mapFloor = -1, mapScene = -1;
int practiceNote = -1, selectedGearType=1, selectedGearValue=1, selectedWorldPoint=6;
double songStarted=0;
bool songPlaying = false, confirmSave = false;
std::string saveMessage, songMessage;
const char* buttonNames[] = { "X / C-Left", "Y / C-Down", "RB / C-Right", "D-Up", "D-Down", "D-Left", "D-Right" };
const char* noteNames[] = { "A", "C-Down", "C-Right", "C-Left", "C-Up" };
const int baseItems[4][3] = {
    { ITEM_SWORD_KOKIRI, ITEM_SWORD_MASTER, ITEM_SWORD_BGS },
    { ITEM_SHIELD_DEKU, ITEM_SHIELD_HYLIAN, ITEM_SHIELD_MIRROR },
    { ITEM_TUNIC_KOKIRI, ITEM_TUNIC_GORON, ITEM_TUNIC_ZORA },
    { ITEM_BOOTS_KOKIRI, ITEM_BOOTS_IRON, ITEM_BOOTS_HOVER }
};
bool Ready() {
    return Enabled() && gPlayState && gPlayState->pauseCtx.state == 6 && gPlayState->pauseCtx.unk_1E4 == 0;
}
std::string ItemName(int item) {
    return item == ITEM_NONE ? "Empty" : SohUtils::GetItemName(item);
}
void ItemIcon(int item, float size = 42) {
    if (item >= 0 && item < int(ARRAY_COUNT(gItemIcons))) {
        ImGui::Image(Theme::Texture(static_cast<const char*>(gItemIcons[item])), {size,size});
    } else { ImGui::Dummy({size,size}); }
}
bool CanAssign(int slot) {
    if (!Ready() || slot < 0 || slot >= 24) { return false; }
    int item = gSaveContext.inventory.items[slot];
    if (item == ITEM_NONE || item == ITEM_SOLD_OUT || !CHECK_AGE_REQ_SLOT(slot)) { return false; }
    if ((item == ITEM_ARROW_FIRE || item == ITEM_ARROW_ICE || item == ITEM_ARROW_LIGHT) &&
        gSaveContext.inventory.items[SLOT_BOW] != ITEM_BOW) { return false; }
    return true;
}
std::string SlotName(int slot) {
    if(slot<0 || slot>=24) { return "Empty"; }
    int item=gSaveContext.inventory.items[slot];
    int gearSlot=Inventory::GearSlot(slot);
    if(item!=ITEM_NONE && ItemsEnabled() && gearSlot>=0) {
        const auto& gear=InventoryRules::Equipped(gARPGSave,gearSlot);
        if(InventoryRules::Valid(gear)) {
            auto name=ItemModifierSystem::ItemName(gear);
            if(gearSlot==ARPG_SLOT_BOTTLE || slot==SLOT_ARROW_FIRE || slot==SLOT_ARROW_ICE || slot==SLOT_ARROW_LIGHT) {
                name+=" / "+ItemName(item);
            }
            return name;
        }
    }
    return ItemName(item);
}
void AssignmentButtons(bool enabled,const std::function<void(int)>& assign) {
    int count=CVarGetInteger(CVAR_ENHANCEMENT("DpadEquips"),0)?7:3;
    for(int first=0;first<count;first+=3) {
        int columns=first==0?3:4;
        if(ImGui::BeginTable(first==0?"cButtonAssignments":"dpadAssignments",columns,ImGuiTableFlags_SizingStretchSame)) {
            for(int b=first;b<first+columns;++b) {
                ImGui::TableNextColumn(); ImGui::PushID(b);
                int assignedItem=gSaveContext.equips.buttonItems[b+1];
                int assignedSlot=gSaveContext.equips.cButtonSlots[b];
                auto icon=assignedItem!=ITEM_NONE && assignedItem<int(ARRAY_COUNT(gItemIcons))?
                    Theme::Texture(static_cast<const char*>(gItemIcons[assignedItem])):nullptr;
                ImGui::BeginDisabled(!enabled);
                if(Theme::IconButton(buttonNames[b],icon,{-1,42})) { assign(b); }
                ImGui::EndDisabled();
                if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled | ImGuiHoveredFlags_DelayNormal)) {
                    Theme::BeginTooltip();
                    Theme::Wrapped("%s: %s",buttonNames[b],(assignedItem==ITEM_NONE?"Empty":
                        assignedSlot<24?SlotName(assignedSlot):ItemName(assignedItem)).c_str());
                    Theme::EndTooltip();
                }
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        if(first==3) { break; }
    }
}
} // namespace

void DrawAssignmentButtons(int slot,const std::function<void(int)>& assign) {
    AssignmentButtons(CanAssign(slot),[&](int button) {
        if(assign) { assign(button); } else { AssignItem(slot,button); }
    });
}

void DrawCSlotButtons(int slot,float width,bool active,const std::function<void(int)>& assign,
                      const std::string& restriction) {
    const char* labels[]={"X / C Left","Y / C Down","RB / C Right"};
    const bool enabled=CanAssign(slot) && restriction.empty();
    int boundSlot=slot, boundItem=slot>=0 && slot<24?gSaveContext.inventory.items[slot]:ITEM_NONE;
    // Native assignment combines elemental arrows with the bow on C buttons.
    if(boundItem==ITEM_ARROW_FIRE || boundItem==ITEM_ARROW_ICE || boundItem==ITEM_ARROW_LIGHT) {
        boundItem=boundItem==ITEM_ARROW_FIRE?ITEM_BOW_ARROW_FIRE:
            boundItem==ITEM_ARROW_ICE?ITEM_BOW_ARROW_ICE:ITEM_BOW_ARROW_LIGHT;
        if(!CVarGetInteger(CVAR_ENHANCEMENT("SeparateArrows"),0))boundSlot=SLOT_BOW;
    }
    for(int button=0;button<3;++button) {
        if(button)ImGui::SameLine(0,6);
        const bool bound=active && slot>=0 && slot<24 &&
            gSaveContext.equips.cButtonSlots[button]==boundSlot &&
            gSaveContext.equips.buttonItems[button+1]==boundItem;
        ImGui::BeginDisabled(!enabled);
        if(Theme::Button(labels[button],{(width-12)/3,32},bound)) {
            if(assign)assign(button); else AssignItem(slot,button);
        }
        ImGui::EndDisabled();
        if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled | ImGuiHoveredFlags_DelayNormal)) {
            Theme::BeginTooltip();
            Theme::Wrapped("%s: %s",buttonNames[button],ItemName(gSaveContext.equips.buttonItems[button+1]).c_str());
            if(!restriction.empty())Theme::Muted("%s",restriction.c_str());
            else if(!enabled)Theme::Muted("Unavailable at this age or not owned.");
            else Theme::Muted("%s",active?"Assign this item.":"Equip this roll and assign it.");
            Theme::EndTooltip();
        }
    }
}

int DrawBottleSelector(int slot,float width) {
    auto* state=ImGui::GetStateStorage();
    const auto key=ImGui::GetID("bottleChoice");
    slot=state->GetInt(key,slot);
    if(slot<SLOT_BOTTLE_1 || slot>SLOT_BOTTLE_4 || gSaveContext.inventory.items[slot]==ITEM_NONE)
        slot=Inventory::NativeSlot(ARPG_SLOT_BOTTLE);
    auto label=[&](int bottle) {
        return bottle<0?std::string("No bottle"):std::to_string(bottle-SLOT_BOTTLE_1+1)+": "+ItemName(gSaveContext.inventory.items[bottle]);
    };
    ImGui::SetNextItemWidth(width);
    if(ImGui::BeginCombo("##bottle",label(slot).c_str())) {
        for(int bottle=SLOT_BOTTLE_1;bottle<=SLOT_BOTTLE_4;++bottle) {
            if(gSaveContext.inventory.items[bottle]==ITEM_NONE)continue;
            if(ImGui::Selectable(label(bottle).c_str(),slot==bottle))slot=bottle;
        }
        ImGui::EndCombo();
    }
    state->SetInt(key,slot);
    return slot;
}

bool CanAssignItem(int slot) { return CanAssign(slot); }

bool DrawNativeItemCard(int slot,float width,float height,bool selected,bool controllerNavigation) {
    ImGui::PushID(2000+slot);
    ImGui::BeginGroup();
    const auto p=ImGui::GetCursorScreenPos();
    auto* draw=ImGui::GetWindowDrawList();
    bool inspect=ImGui::InvisibleButton("item",{width,height},ImGuiButtonFlags_EnableNav);
    bool preview=controllerNavigation?ImGui::IsItemFocused():ImGui::IsItemHovered(ImGuiHoveredFlags_NoNavOverride);
    Theme::SlotFrame(draw,p,{width,height},selected || ImGui::IsItemFocused(),IM_COL32(194,205,168,255));
    draw->AddImage(Theme::Texture(static_cast<const char*>(gItemIcons[gSaveContext.inventory.items[slot]])),
        {p.x+10,p.y+10},{p.x+66,p.y+66});
    const auto name=ItemName(gSaveContext.inventory.items[slot]);
    draw->AddText(ImGui::GetFont(),16,{p.x+76,p.y+10},IM_COL32(232,219,163,255),name.c_str(),nullptr,width-88);
    Theme::Text(draw,{p.x+10,p.y+82},Inventory::GearSlot(slot)==ARPG_SLOT_BOW?
        "Uses your equipped bow's bonuses":"No bonus stats",14,IM_COL32(189,201,224,255));
    if(ImGui::BeginDragDropSource()) {
        ImGui::SetDragDropPayload(NativeItemPayload,&slot,sizeof(slot));
        ItemIcon(gSaveContext.inventory.items[slot],48); Theme::Wrapped("%s",name.c_str());
        ImGui::EndDragDropSource();
    }
    if(preview && !ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        Theme::BeginTooltip();
        Theme::Colored({.91f,.86f,.64f,1},"%s",name.c_str());
        Theme::Wrapped("%s",Inventory::GearSlot(slot)==ARPG_SLOT_BOW?
            "Uses the stats of your equipped bow.":"Adventure item. No bonus stats.");
        if(!CanAssign(slot))Theme::Muted("Unavailable at this age or not owned.");
        Theme::EndTooltip();
    }
    ImGui::EndGroup();
    ImGui::PopID();
    return inspect;
}

void DrawEquipmentAssignmentButtons(int type,int value,const std::function<void(int)>& assign) {
    bool enabled=Ready() && CVarGetInteger(CVAR_ENHANCEMENT("AssignableTunicsAndBoots"),0) &&
        type>=1 && type<=3 && value>=1 && value<=3 && CHECK_OWNED_EQUIP(type,value-1) && CHECK_AGE_REQ_EQUIP(type,value);
    AssignmentButtons(enabled,assign);
}

int DrawItemGrid(int selected,const std::function<void(int)>& dropTarget) {
    if(!Ready()) { Theme::Wrapped("Opening gear..."); return -1; }
    Theme::Label("Items",18,IM_COL32(232,219,163,255));
    float cell=std::min(84.f,(ImGui::GetContentRegionAvail().x-50)/6);
    int clickedSlot=-1;
    for (int slot=0; slot<24; ++slot) {
        if (slot%6) { ImGui::SameLine(); }
        ImGui::PushID(slot);
        auto pos=ImGui::GetCursorScreenPos();
        int item=gSaveContext.inventory.items[slot];
        ImGui::PushStyleColor(ImGuiCol_Button,{0,0,0,0});
        bool clicked=ImGui::Button("##item",{cell,cell});
        if(dropTarget) { dropTarget(slot); }
        ImGui::PopStyleColor();
        Theme::SlotFrame(ImGui::GetWindowDrawList(),pos,{cell,cell},selected==slot,IM_COL32(194,205,168,255));
        if (item!=ITEM_NONE) {
            auto tint=CanAssign(slot)?IM_COL32_WHITE:IM_COL32(120,120,120,255);
            ImGui::GetWindowDrawList()->AddImage(Theme::Texture(static_cast<const char*>(gItemIcons[item])),
                {pos.x+8,pos.y+8},{pos.x+cell-8,pos.y+cell-8},{0,0},{1,1},tint);
            if (slot==SLOT_STICK || slot==SLOT_NUT || slot==SLOT_BOMB || slot==SLOT_BOW ||
                slot==SLOT_SLINGSHOT || slot==SLOT_BOMBCHU || slot==SLOT_BEAN) {
                Theme::Text(ImGui::GetWindowDrawList(),{pos.x+4,pos.y+cell-23},
                    std::to_string(gSaveContext.inventory.ammo[slot]),20);
            }
        }
        int gearSlot=Inventory::GearSlot(slot);
        if(item!=ITEM_NONE && ItemsEnabled() && gearSlot>=0) {
            const auto& gear=InventoryRules::Equipped(gARPGSave,gearSlot);
            if(InventoryRules::Valid(gear) && (gear.modCount || gear.uniqueId || gear.socketCount)) {
                ImGui::GetWindowDrawList()->AddCircleFilled({pos.x+cell-8,pos.y+8},3,IM_COL32(255,214,97,255));
            }
        }
        if (clicked) { clickedSlot=slot; }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
            Theme::BeginTooltip(); Theme::Wrapped("%s",SlotName(slot).c_str()); Theme::EndTooltip();
        }
        ImGui::PopID();
    }
    return clickedSlot;
}

void DrawItemDetails(int selectedSlot,bool header,const std::function<void(int)>& assign,bool showAssignments) {
    if(!Ready() || selectedSlot<0 || selectedSlot>=24) { return; }
    if(header) {
        ItemIcon(gSaveContext.inventory.items[selectedSlot],64);
        ImGui::SameLine(); ImGui::BeginGroup();
        Theme::Wrapped("%s",SlotName(selectedSlot).c_str());
        ImGui::EndGroup();
        if(showAssignments) { ImGui::Spacing(); Theme::Label("Assign",18,IM_COL32(232,219,163,255)); }
    } else if((selectedSlot>=SLOT_BOTTLE_1 && selectedSlot<=SLOT_BOTTLE_4) ||
              selectedSlot==SLOT_ARROW_FIRE || selectedSlot==SLOT_ARROW_ICE || selectedSlot==SLOT_ARROW_LIGHT ||
              gSaveContext.inventory.items[selectedSlot]==ITEM_LONGSHOT ||
              gSaveContext.inventory.items[selectedSlot]==ITEM_OCARINA_TIME) {
        Theme::Muted("%s",ItemName(gSaveContext.inventory.items[selectedSlot]).c_str());
    }
    if(showAssignments)DrawAssignmentButtons(selectedSlot,assign);
    if (!CanAssign(selectedSlot)) {
        int item=gSaveContext.inventory.items[selectedSlot];
        Theme::Muted("%s",item==ITEM_NONE?"This slot is empty.":
            !CHECK_AGE_REQ_SLOT(selectedSlot)?"Unavailable at this age.":
            item==ITEM_SOLD_OUT?"This item is sold out.":"Requires the bow.");
    }
    const int previous=KaleidoScope_GetCycleItem(selectedSlot,-1), next=KaleidoScope_GetCycleItem(selectedSlot,1);
    if(((previous!=ITEM_NONE && previous!=gSaveContext.inventory.items[selectedSlot]) ||
        (next!=ITEM_NONE && next!=gSaveContext.inventory.items[selectedSlot])) && Theme::Section("Item variants")) {
    for(int direction : {-1,1}) {
        int next=KaleidoScope_GetCycleItem(selectedSlot,direction);
        if(next==ITEM_NONE || next==gSaveContext.inventory.items[selectedSlot]) { continue; }
        ImGui::BeginDisabled(!Ready());
        if(Theme::Button((std::string(direction<0?"Previous: ":"Next: ")+ItemName(next)).c_str(),{-1,36})) {
            int old=gSaveContext.inventory.items[selectedSlot];
            gSaveContext.inventory.items[selectedSlot]=next;
            for(int b=1;b<8;++b) {
                if(gSaveContext.equips.cButtonSlots[b-1]!=selectedSlot || gSaveContext.equips.buttonItems[b]!=old) { continue; }
                gSaveContext.equips.buttonItems[b]=CHECK_AGE_REQ_ITEM(next)?next:ITEM_NONE;
                if(gSaveContext.equips.buttonItems[b]==ITEM_NONE) { gSaveContext.equips.cButtonSlots[b-1]=SLOT_NONE; }
                else { Interface_LoadItemIcon1(gPlayState,b); }
            }
        }
        ImGui::EndDisabled();
    }
    }
}

namespace {
void Items() {
    float width=ImGui::GetContentRegionAvail().x;
    Theme::BeginPane("adventureItems",{std::min(760.f,width*.66f),0},true);
    int clicked=DrawItemGrid(selectedSlot);
    if(clicked>=0) { selectedSlot=clicked; }
    Theme::EndPane(); ImGui::SameLine();
    Theme::BeginPane("itemAssignment",{0,0},true);
    DrawItemDetails(selectedSlot);
    Theme::EndPane();
}
void BaseGear() {
    Theme::BeginPane("baseEquipment",{0,0},true);
    Theme::Label("ADVENTURE EQUIPMENT");
    Theme::Wrapped("Select owned equipment to wear it. Rolled bonuses apply only when their base equipment matches.");
    if (ImGui::BeginTable("baseEquipmentTable",3,ImGuiTableFlags_SizingStretchSame)) {
        for(int type=0;type<4;++type) {
            for(int value=1;value<=3;++value) {
                ImGui::TableNextColumn(); ImGui::PushID(type*4+value);
                int item=baseItems[type][value-1];
                if(type==0 && value==3 && !gSaveContext.bgsFlag && CHECK_OWNED_EQUIP_ALT(EQUIP_TYPE_SWORD,EQUIP_INV_SWORD_BROKENGIANTKNIFE)) { item=ITEM_SWORD_KNIFE; }
                bool owned=CHECK_OWNED_EQUIP(type,value-1), age=CHECK_AGE_REQ_EQUIP(type,value);
                ItemIcon(item,56);
                ImGui::BeginDisabled(!Ready() || !owned || !age);
                if(Theme::Button(ItemName(item).c_str(),{-1,36},CUR_EQUIP_VALUE(type)==value)) { EquipBase(type,value); selectedGearType=type; selectedGearValue=value; }
                ImGui::EndDisabled();
                Theme::Muted("%s",!owned?"Not acquired":!age?"Unavailable at this age":CUR_EQUIP_VALUE(type)==value?"Equipped":"Available");
                ImGui::PopID();
            }
        }
        ImGui::EndTable();
    }
    if(CVarGetInteger(CVAR_ENHANCEMENT("AssignableTunicsAndBoots"),0) && selectedGearType>0) {
        Theme::Wrapped("Assign %s",ItemName(baseItems[selectedGearType][selectedGearValue-1]).c_str());
        int count=CVarGetInteger(CVAR_ENHANCEMENT("DpadEquips"),0)?7:3;
        for(int b=0;b<count;++b) {
            if(b) { ImGui::SameLine(); }
            if(Theme::Button(buttonNames[b],{(ImGui::GetContentRegionAvail().x)/(count-b)-10,34})) {
                AssignEquipment(selectedGearType,selectedGearValue,b);
            }
        }
    }
    if(CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD) && Theme::Button("Remove shield",{250,36})) {
        Inventory_ChangeEquipment(EQUIP_TYPE_SHIELD,EQUIP_VALUE_SHIELD_NONE);
        Player_SetEquipmentData(gPlayState,GET_PLAYER(gPlayState));
    }
    Theme::Rule(); Theme::Label("PERMANENT UPGRADES");
    const char* upgrades[]={"Quiver","Bomb bag","Strength","Diving scale","Wallet","Bullet bag","Deku sticks","Deku nuts"};
    for(int i=0;i<8;++i) {
        if(i==UPG_STRENGTH || i==UPG_SCALE) { Theme::Print("%s: level %d",upgrades[i],CUR_UPG_VALUE(i)); }
        else { Theme::Print("%s: %d",upgrades[i],CUR_CAPACITY(i)); }
    }
    Theme::EndPane();
}
int selectedReward=QUEST_MEDALLION_FOREST;
void Reward(int quest,ImVec2 p,float size) {
    auto entry=questMapping.find(quest); if(entry==questMapping.end()) { return; }
    bool owned=CHECK_QUEST_ITEM(quest);
    ImGui::SetCursorScreenPos(p); ImGui::PushID(quest);
    if(ImGui::InvisibleButton("reward",{size,size},ImGuiButtonFlags_EnableNav)) { selectedReward=quest; }
    auto* draw=ImGui::GetWindowDrawList();
    draw->AddCircleFilled({p.x+size*.5f,p.y+size*.5f},size*.49f,IM_COL32(17,26,51,135),32);
    draw->AddCircle({p.x+size*.5f,p.y+size*.5f},size*.49f,owned?IM_COL32(197,187,142,150):IM_COL32(107,126,162,100),32);
    draw->AddImage(Theme::Texture(entry->second.texturePath.c_str()),{p.x+4,p.y+4},{p.x+size-4,p.y+size-4},{0,0},{1,1},
        owned?IM_COL32_WHITE:IM_COL32(85,97,126,140));
    if(selectedReward==quest || ImGui::IsItemFocused()) { Theme::Cursor(draw,p,{size,size}); }
    if(ImGui::IsItemHovered() || ImGui::IsItemFocused()) {
        Theme::BeginTooltip(); Theme::Print("%s",SohUtils::GetQuestItemName(quest).c_str());
        Theme::Muted("%s",owned?"Collected":"Not yet found"); Theme::EndTooltip();
    }
    ImGui::PopID();
}
void Quest() {
    float width=ImGui::GetContentRegionAvail().x;
    Theme::BeginPane("questStatus",{width*.43f,0},true);
    Theme::Label("QUEST STATUS");
    ImGui::Image(Theme::Texture(gQuestIconHeartContainerTex),{26,26}); ImGui::SameLine();
    Theme::Print("%.0f / %.0f hearts   -   %u / 4 pieces",gSaveContext.health/16.f,gSaveContext.healthCapacity/16.f,(gSaveContext.inventory.questItems>>28)&0xF);
    ImGui::Image(Theme::Texture(gQuestIconGoldSkulltulaTex),{26,26}); ImGui::SameLine();
    Theme::Print("%d Skulltulas + %d wild   -   %d Rupees",gSaveContext.inventory.gsTokens,Exploration::SkullCount(gARPGSave),gSaveContext.rupees);
    if(gARPGSave.skullRewardTier<32) {
        Theme::Muted("Next bounty: %d / %d tokens",gSaveContext.inventory.gsTokens+Exploration::SkullCount(gARPGSave),(gARPGSave.skullRewardTier+1)*5);
    }
    Theme::Muted("Magic %d   /   Deaths %d",gSaveContext.magic,gSaveContext.deaths);
    Theme::Rule(); Theme::Label("SAGES' MEDALLIONS",22,IM_COL32(242,223,157,255));
    float available=ImGui::GetContentRegionAvail().x;
    auto origin=ImGui::GetCursorScreenPos();
    float radius=std::clamp(available*.25f,55.f,89.f),icon=std::clamp(available*.15f,36.f,56.f);
    float height=radius*2+icon+12;
    ImVec2 center{origin.x+available*.5f,origin.y+height*.5f};
    ImGui::Dummy({available,height}); auto afterRing=ImGui::GetCursorScreenPos();
    auto* draw=ImGui::GetWindowDrawList();
    draw->AddCircle(center,radius,IM_COL32(170,186,214,110),64,1.5f);
    draw->AddCircle(center,radius-7,IM_COL32(103,128,172,70),64);
    float unit=std::min(22.f,radius*.3f);
    auto triangle=[&](float x,float y) { draw->AddTriangleFilled({center.x+x,center.y+y-unit},{center.x+x-unit,center.y+y+unit*.7f},{center.x+x+unit,center.y+y+unit*.7f},IM_COL32(225,194,110,220)); };
    triangle(0,-unit*.7f); triangle(-unit,unit); triangle(unit,unit);
    for(int q=0;q<6;++q) {
        float angle=-1.5707963f+q*1.0471976f;
        Reward(q,{center.x+std::cos(angle)*radius-icon*.5f,center.y+std::sin(angle)*radius-icon*.5f},icon);
    }
    ImGui::SetCursorScreenPos(afterRing);
    Theme::Label(SohUtils::GetQuestItemName(selectedReward),20,IM_COL32(249,237,190,255));
    Theme::Rule(); Theme::Label("SPIRITUAL STONES",22,IM_COL32(242,223,157,255));
    origin=ImGui::GetCursorScreenPos(); icon=std::min(60.f,(available-30)/3);
    ImGui::Dummy({available,icon+6}); auto afterStones=ImGui::GetCursorScreenPos();
    for(int n=0;n<3;++n) { Reward(QUEST_KOKIRI_EMERALD+n,{origin.x+(available-3*icon-24)*.5f+n*(icon+12),origin.y},icon); }
    ImGui::SetCursorScreenPos(afterStones); Theme::Rule();
    for(int q=QUEST_STONE_OF_AGONY;q<=QUEST_GERUDO_CARD;++q) {
        auto entry=questMapping.find(q); if(entry==questMapping.end()) { continue; }
        ImGui::Image(Theme::Texture(entry->second.texturePath.c_str()),{28,28}); ImGui::SameLine();
        Theme::Print("%s%s",CHECK_QUEST_ITEM(q)?"":"Unfound: ",SohUtils::GetQuestItemName(q).c_str());
    }
    if(IS_RANDO) {
        Theme::Rule(); Theme::Label("RANDOMIZER COLLECTION");
        for(const auto& [name,achieved] : Rando::Context::GetInstance()->GetKaleido()->GetStatus(gPlayState)) {
            Theme::Wrapped("%s %s",achieved?"[+]":"[ ]",name.c_str());
        }
    }
    Theme::EndPane(); ImGui::SameLine();
    Theme::BeginPane("questSongs",{0,0},true);
    Theme::Label("OCARINA SONGS");
    int songColumns=ImGui::GetContentRegionAvail().x<420?1:2;
    if(ImGui::BeginTable("songs",songColumns,ImGuiTableFlags_SizingStretchSame)) {
        for(int q=QUEST_SONG_MINUET;q<=QUEST_SONG_STORMS;++q) {
            ImGui::TableNextColumn(); ImGui::BeginDisabled(!CHECK_QUEST_ITEM(q));
            auto color=songMapping.find(static_cast<QuestItem>(q));
            ImU32 tint=color==songMapping.end()?IM_COL32_WHITE:ImGui::ColorConvertFloat4ToU32(color->second.color);
            if(Theme::IconButton(SohUtils::GetQuestItemName(q).c_str(),Theme::Texture(gSongNoteTex),{-1,40},selectedSong==q,tint)) { StopSong(); selectedSong=q; }
            ImGui::EndDisabled();
        }
        ImGui::EndTable();
    }
    Theme::Rule();
    if(CHECK_QUEST_ITEM(selectedSong)) {
        int song=gOcarinaSongItemMap[selectedSong-QUEST_SONG_MINUET];
        auto& notes=gOcarinaSongButtons[song];
        Theme::Label(SohUtils::GetQuestItemName(selectedSong),24,IM_COL32(247,231,165,255));
        static const char* noteTextures[]={gOcarinaBtnIconATex,gOcarinaBtnIconCDownTex,gOcarinaBtnIconCRightTex,gOcarinaBtnIconCLeftTex,gOcarinaBtnIconCUpTex};
        auto staff=ImGui::GetCursorScreenPos();
        float step=std::min(44.f,ImGui::GetContentRegionAvail().x/std::max(1,int(notes.numButtons)));
        auto* draw=ImGui::GetWindowDrawList();
        draw->AddLine({staff.x,staff.y+20},{staff.x+step*notes.numButtons,staff.y+20},IM_COL32(162,184,220,100),2);
        for(int n=0;n<notes.numButtons;++n) {
            int note=notes.buttonsIndex[n]; float size=std::min(30.f,step-5);
            ImVec2 center{staff.x+step*(n+.5f),staff.y+20};
            draw->AddCircleFilled(center,size*.6f,IM_COL32(19,30,61,230),24);
            draw->AddImage(Theme::Texture(noteTextures[note]),{center.x-size*.5f,center.y-size*.5f},{center.x+size*.5f,center.y+size*.5f},{0,0},{1,1},
                note==0?IM_COL32(116,176,255,255):IM_COL32(255,228,92,255));
        }
        ImGui::Dummy({step*notes.numButtons,44});
        float songButtonWidth=(ImGui::GetContentRegionAvail().x-10)*.5f;
        if(Theme::Button(songPlaying?"Stop playback":"Listen",{songButtonWidth,36})) {
            if(songPlaying) { StopSong(); }
            else { StopSong(); AudioOcarina_SetInstrument(OCARINA_INSTRUMENT_DEFAULT); AudioOcarina_SetPlaybackSong(song+1,1); songPlaying=true; songStarted=ImGui::GetTime(); }
        }
        ImGui::SameLine();
        if(Theme::Button("Practice",{songButtonWidth,36})) { StopSong(); practiceNote=0; songMessage="Play with A and arrow keys, the controller, or hold the note buttons.";
            AudioOcarina_SetUiInput(1,0);
            AudioOcarina_SetInstrument(OCARINA_INSTRUMENT_DEFAULT);
            AudioOcarina_Start((1<<song)+0x8000); songStarted=ImGui::GetTime(); }
        if(songPlaying && ImGui::GetTime()-songStarted>.2 && AudioOcarina_GetPlaybackStaff()->state==0) { StopSong(); }
        if(practiceNote>=0) {
            const ImGuiKey keys[]={ImGuiKey_A,ImGuiKey_DownArrow,ImGuiKey_RightArrow,ImGuiKey_LeftArrow,ImGuiKey_UpArrow};
            const ImGuiKey pad[]={ImGuiKey_GamepadFaceDown,ImGuiKey_GamepadRStickDown,ImGuiKey_GamepadRStickRight,ImGuiKey_GamepadRStickLeft,ImGuiKey_GamepadRStickUp};
            const u16 masks[]={BTN_A,BTN_CDOWN,BTN_CRIGHT,BTN_CLEFT,BTN_CUP};
            u16 buttons=0;
            float noteWidth=(ImGui::GetContentRegionAvail().x-40)/5;
            for(int n=0;n<5;++n) {
                if(n) { ImGui::SameLine(); }
                Theme::Button(noteNames[n],{noteWidth,34});
                if(ImGui::IsItemActive() || ImGui::IsKeyDown(keys[n]) || ImGui::IsKeyDown(pad[n])) { buttons|=masks[n]; }
            }
            AudioOcarina_SetUiInput(1,buttons);
            auto* staff=AudioOcarina_GetPlayingStaff();
            if(staff->state==song) { StopSong(); songMessage="Well played!"; }
            else if(ImGui::GetTime()-songStarted>.2 && staff->state==0xFF) { StopSong(); songMessage="Try again. Select Practice to start from the first note."; }
        }
        Theme::Wrapped("%s",songMessage.c_str());
    } else { Theme::Wrapped("Learn a song to listen to it and practice its notes."); }
    Theme::EndPane();
}
ImTextureID WorldTexture() {
    auto gui=std::dynamic_pointer_cast<Fast::Fast3dGui>(Ship::Context::GetRawInstance()->GetWindow()->GetGui());
    if(!gui->HasTextureByName("arpgWorldMap")) { gui->LoadGuiTexture("arpgWorldMap",gWorldMapImageTex,gWorldMapImageTLUT); }
    return gui->GetTextureByName("arpgWorldMap");
}
void WorldMap() {
    Theme::Wrapped("Current location: %s",SohUtils::GetSceneName(gPlayState->sceneNum).c_str());
    const char* names[]={"Haunted Wasteland","Gerudo Fortress","Gerudo Valley","Lake Hylia","Lon Lon Ranch","Market",
        "Hyrule Field","Death Mountain","Kakariko Village","Lost Woods","Kokiri Forest","Zora's Domain"};
    static const char* clouds[]={gWorldMapCloud16Tex,gWorldMapCloud15Tex,gWorldMapCloud14Tex,gWorldMapCloud13Tex,
        gWorldMapCloud12Tex,gWorldMapCloud11Tex,gWorldMapCloud10Tex,gWorldMapCloud9Tex,gWorldMapCloud8Tex,
        gWorldMapCloud7Tex,gWorldMapCloud6Tex,gWorldMapCloud5Tex,gWorldMapCloud4Tex,gWorldMapCloud3Tex,gWorldMapCloud2Tex,gWorldMapCloud1Tex};
    static const int flags[]={5,0,19,14,15,1,2,16,18,3,7,8,9,12,11,6};
    float available=ImGui::GetContentRegionAvail().x;
    ImGui::BeginGroup();
    auto p=ImGui::GetCursorScreenPos();
    float scale=std::min(available*.72f/216.f,std::max(1.f,(ImGui::GetContentRegionAvail().y-95)/128));
    auto* draw=ImGui::GetWindowDrawList();
    bool mirror=CVarGetInteger(CVAR_ENHANCEMENT("MirroredWorld"),0);
    draw->AddImage(WorldTexture(),p,{p.x+216*scale,p.y+128*scale},{mirror?1.f:0.f,0},{mirror?0.f:1.f,1});
    for(int i=0;i<28;++i) {
        int x,y,w,h; KaleidoScope_GetWorldMapRect(i,&x,&y,&w,&h);
        if(mirror) { x=216-x-w; }
        ImVec2 a{p.x+x*scale,p.y+y*scale},b{a.x+w*scale,a.y+h*scale};
        if(i<16) {
            if(!(gSaveContext.worldMapAreaData & gBitFlags[flags[i]])) { draw->AddImage(Theme::Texture(clouds[i]),a,b); }
        } else {
            int point=i-16;
            if(!gPlayState->pauseCtx.worldMapPoints[point]) { continue; }
            bool goal=gPlayState->pauseCtx.worldMapPoints[point]==2;
            draw->AddCircleFilled({(a.x+b.x)/2,(a.y+b.y)/2},3*scale,goal?IM_COL32(255,222,76,255):IM_COL32(85,155,255,255));
            if(selectedWorldPoint==point) { draw->AddCircle({(a.x+b.x)/2,(a.y+b.y)/2},5*scale,IM_COL32_WHITE,0,2); }
            if(ImGui::IsMouseHoveringRect(a,b) && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) { selectedWorldPoint=point; }
            if(gPlayState->pauseCtx.tradeQuestLocation==point) { draw->AddTriangleFilled({a.x,a.y-10},{b.x,a.y-10},{(a.x+b.x)/2,a.y-2},IM_COL32(100,255,160,255)); }
            if(ImGui::IsMouseHoveringRect(a,b)) { Theme::BeginTooltip(); Theme::Print("%s",names[point]); Theme::EndTooltip(); }
        }
    }
    ImGui::Dummy({216*scale,128*scale});
    Theme::Print("Blue: known. Gold: quest. Green: trade.");
    if(gPlayState->pauseCtx.tradeQuestLocation<12) { Theme::Print("Trade: %s",names[gPlayState->pauseCtx.tradeQuestLocation]); }
    ImGui::EndGroup(); ImGui::SameLine();
    ImGui::BeginGroup();
    static const int skullAreas[]={21,20,19,18,11,14,10,15,16,13,12,17};
    for(int point=0;point<12;++point) {
        if(!gPlayState->pauseCtx.worldMapPoints[point]) { continue; }
        if(Theme::Button(names[point],{available*.25f,32},selectedWorldPoint==point)) { selectedWorldPoint=point; }
    }
    if(gPlayState->pauseCtx.worldMapPoints[selectedWorldPoint]) {
        int area=skullAreas[selectedWorldPoint];
        Theme::Print("Skulltulas: %s",GET_GS_FLAGS(area)==gAreaGsFlags[area]?"Complete":"Incomplete");
    }
    ImGui::EndGroup();
}
bool InDungeon() {
    int scene=gPlayState->sceneNum;
    return (scene>=SCENE_DEKU_TREE && scene<=SCENE_ICE_CAVERN) ||
           (scene>=SCENE_DEKU_TREE_BOSS && scene<=SCENE_SHADOW_TEMPLE_BOSS);
}
ImTextureID DungeonTexture(int half) {
    auto& ctx=gPlayState->interfaceCtx;
    auto gui=std::dynamic_pointer_cast<Fast::Fast3dGui>(Ship::Context::GetRawInstance()->GetWindow()->GetGui());
    std::string key="arpgDungeonMap"+std::to_string(half);
    if(!gui->HasTextureByName(key)) {
        auto source=std::dynamic_pointer_cast<Fast::Texture>(Ship::Context::GetRawInstance()->GetResourceManager()->LoadResource(ctx.mapSegmentName[half]));
        if(!source) { return nullptr; }
        // Decode the live room palette, including unexplored rooms and the current room.
        // Full RGBA replacements bypass room palettes; HD CI4 replacements still
        // need decoding from the engine's copied indices.
        const size_t pixelCount=size_t(source->Width)*source->Height;
        if((source->Flags & TEX_FLAG_LOAD_AS_RAW) && source->ImageDataSize>=pixelCount*4) {
            gui->LoadGuiTexture(key,*source);
        } else {
            if(!ctx.mapSegment[half] || source->ImageDataSize<(pixelCount+1)/2) { return nullptr; }
            Fast::Texture rgba;
            rgba.Type=Fast::TextureType::RGBA32bpp; rgba.Width=source->Width; rgba.Height=source->Height;
            rgba.ImageDataSize=rgba.Width*rgba.Height*4; rgba.ImageData=new uint8_t[rgba.ImageDataSize];
            const auto* pixels=reinterpret_cast<const uint8_t*>(ctx.mapSegment[half]);
            for(int i=0;i<rgba.Width*rgba.Height;++i) {
                int index=(pixels[i/2] >> (i%2?0:4))&15;
                unsigned color=(ctx.mapPalette[index*2]<<8)|ctx.mapPalette[index*2+1];
                rgba.ImageData[i*4]=((color>>11)&31)*255/31;
                rgba.ImageData[i*4+1]=((color>>6)&31)*255/31;
                rgba.ImageData[i*4+2]=((color>>1)&31)*255/31;
                rgba.ImageData[i*4+3]=(color&1)?255:0;
            }
            gui->LoadGuiTexture(key,rgba);
        }
    }
    return gui->GetTextureByName(key);
}
void SelectFloor(int floor) {
    mapFloor=floor; mapScene=gPlayState->sceneNum;
    auto& pause=gPlayState->pauseCtx;
    pause.dungeonMapSlot=pause.cursorPoint[PAUSE_MAP]=floor+3;
    R_MAP_TEX_INDEX=R_MAP_TEX_INDEX_BASE+gMapData->floorTexIndexOffset[gSaveContext.mapIndex][floor];
    KaleidoScope_UpdateDungeonMap(gPlayState);
    // Native drawing normally supplies the current-room color and marker table.
    gPlayState->interfaceCtx.mapPalette[28]=0x06;
    gPlayState->interfaceCtx.mapPalette[29]=0x3F;
    PauseMapMark_Init(gPlayState);
    auto gui=std::dynamic_pointer_cast<Fast::Fast3dGui>(Ship::Context::GetRawInstance()->GetWindow()->GetGui());
    gui->UnloadTexture("arpgDungeonMap0"); gui->UnloadTexture("arpgDungeonMap1");
}
void DungeonMap() {
    int dungeon=gSaveContext.mapIndex;
    if(dungeon<0 || dungeon>=10 || !gMapData) { return; }
    if(mapScene!=gPlayState->sceneNum || mapFloor<0) { SelectFloor(std::clamp<int>(VREG(30),0,7)); }
    Theme::Wrapped("%s",SohUtils::GetSceneName(gPlayState->sceneNum).c_str());
    Theme::Print("Map: %s   Compass: %s   Boss key: %s   Small keys: %d",
        CHECK_DUNGEON_ITEM(DUNGEON_MAP,dungeon)?"Yes":"No",CHECK_DUNGEON_ITEM(DUNGEON_COMPASS,dungeon)?"Yes":"No",
        CHECK_DUNGEON_ITEM(DUNGEON_KEY_BOSS,dungeon)?"Yes":"No",std::max<int>(0,gSaveContext.inventory.dungeonKeys[dungeon]));
    const char* floors[]={"","8F","7F","6F","5F","4F","3F","2F","1F","B1","B2","B3","B4","B5","B6","B7","B8"};
    bool first=true;
    for(int f=0;f<8;++f) {
        int id=gMapData->floorID[dungeon][f];
        if(!id || id>16 || (!(gSaveContext.sceneFlags[dungeon].floors & gBitFlags[f]) && !CHECK_DUNGEON_ITEM(DUNGEON_MAP,dungeon))) { continue; }
        if(!first) { ImGui::SameLine(); } first=false;
        if(Theme::Button(floors[id],{65,34},mapFloor==f)) { SelectFloor(f); }
    }
    if(GET_GS_FLAGS(dungeon)==gAreaGsFlags[dungeon]) { Theme::Print("All original Gold Skulltulas collected."); }
    Theme::Print("Current floor: %s",floors[std::clamp<int>(gMapData->floorID[dungeon][std::clamp<int>(VREG(30),0,7)],0,16)]);
    auto p=ImGui::GetCursorScreenPos();
    float scale=std::min(ImGui::GetContentRegionAvail().x/96.f,ImGui::GetContentRegionAvail().y/85.f);
    auto* draw=ImGui::GetWindowDrawList();
    bool mirror=CVarGetInteger(CVAR_ENHANCEMENT("MirroredWorld"),0);
    draw->AddRectFilled(p,{p.x+96*scale,p.y+85*scale},IM_COL32(165,180,157,255));
    for(int half=0;half<2;++half) {
        int side=mirror?1-half:half;
        draw->AddImage(DungeonTexture(half),{p.x+side*48*scale,p.y},{p.x+(side+1)*48*scale,p.y+85*scale},{mirror?1.f:0.f,0},{mirror?0.f:1.f,1});
    }
    if(CHECK_DUNGEON_ITEM(DUNGEON_COMPASS,dungeon) && gLoadedPauseMarkDataTable) {
        auto* marks=gLoadedPauseMarkDataTable[R_MAP_TEX_INDEX>>1];
        for(int m=0;m<3 && marks[m].markType!=PAUSE_MAP_MARK_NONE;++m) {
            for(int i=0;i<marks[m].count;++i) {
                auto& point=marks[m].points[i];
                bool chest=marks[m].markType==PAUSE_MAP_MARK_CHEST;
                if(chest && (Flags_GetTreasure(gPlayState,point.chestFlag) || gPlayState->sceneNum>=SCENE_DEKU_TREE_BOSS)) { continue; }
                float x=mirror?96-point.x:point.x;
                ImVec2 c{p.x+x*scale,p.y+(7-point.y)*scale};
                draw->AddImage(Theme::Texture(chest?gMapChestIconTex:gMapBossIconTex),{c.x-4*scale,c.y-4*scale},{c.x+4*scale,c.y+4*scale});
            }
        }
    }
    ImGui::Dummy({96*scale,85*scale});
}
void Map() {
    Theme::BeginPane("adventureMap",{0,0},true);
    if(InDungeon()) { DungeonMap(); } else { WorldMap(); }
    Theme::EndPane();
}
void SavePage() {
    Theme::BeginPane("saveAdventure",{0,0},true);
    Theme::Label("SAVE ADVENTURE");
    Theme::Wrapped("Save your adventure, equipment, skills and expedition progress to the current file.");
    if(gSaveContext.fileNum>2) { Theme::Wrapped("This temporary adventure has no save slot."); }
    ImGui::BeginDisabled(!Ready() || gSaveContext.fileNum>2 || !GameInteractor_Should(VB_BE_ABLE_TO_SAVE,true));
    if(Theme::Button(confirmSave?"Confirm save":"Save game",{280,44})) {
        if(confirmSave) { saveMessage=Save()?"Save requested.":"Saving is unavailable."; confirmSave=false; }
        else { confirmSave=true; }
    }
    ImGui::EndDisabled();
    if(confirmSave && Theme::Button("Cancel",{280,36})) { confirmSave=false; }
    Theme::Wrapped("%s",saveMessage.c_str());
    Theme::EndPane();
}
} // namespace

bool AssignItem(int slot,int button) {
    if(button<0 || button>=7 || (button>=3 && !CVarGetInteger(CVAR_ENHANCEMENT("DpadEquips"),0)) || !CanAssign(slot)) { return false; }
    int item=gSaveContext.inventory.items[slot];
    if(!GameInteractor_Should(VB_EQUIP_ITEM_TO_C_BUTTON,true,gPlayState,slot,item)) { return false; }
    KaleidoScope_AssignItem(gPlayState,item,slot,button);
    return true;
}
bool AssignEquipment(int type,int value,int button) {
    if(!Ready() || !CVarGetInteger(CVAR_ENHANCEMENT("AssignableTunicsAndBoots"),0) || type<1 || type>3 ||
       value<1 || value>3 || button<0 || button>=7 || (button>=3 && !CVarGetInteger(CVAR_ENHANCEMENT("DpadEquips"),0)) ||
       !CHECK_OWNED_EQUIP(type,value-1) || !CHECK_AGE_REQ_EQUIP(type,value)) { return false; }
    int slot=(type==1?SLOT_SHIELD_DEKU:type==2?SLOT_TUNIC_KOKIRI:SLOT_BOOTS_KOKIRI)+value-1;
    KaleidoScope_AssignItem(gPlayState,baseItems[type][value-1],slot,button);
    return true;
}
bool EquipBase(int type,int value) {
    if(!Ready() || type<0 || type>=4 || value<1 || value>3 || !CHECK_OWNED_EQUIP(type,value-1) || !CHECK_AGE_REQ_EQUIP(type,value)) { return false; }
    Inventory_ChangeEquipment(type,value);
    if(type==EQUIP_TYPE_SWORD) {
        int item=baseItems[type][value-1];
        if(value==3) {
            if(gSaveContext.bgsFlag) { gSaveContext.swordHealth=8; }
            else if(CHECK_OWNED_EQUIP_ALT(EQUIP_TYPE_SWORD,EQUIP_INV_SWORD_BROKENGIANTKNIFE)) { item=ITEM_SWORD_KNIFE; }
        }
        gSaveContext.equips.buttonItems[0]=item;
        gSaveContext.infTable[29]=0;
        Flags_UnsetInfTable(INFTABLE_SWORDLESS);
        Interface_LoadItemIcon1(gPlayState,0);
    }
    Player_SetEquipmentData(gPlayState,GET_PLAYER(gPlayState));
    return true;
}
bool Save() {
    if(!Ready() || gSaveContext.fileNum>2 || !GameInteractor_Should(VB_BE_ABLE_TO_SAVE,true)) { return false; }
    Play_PerformSave(gPlayState);
    return true;
}
void StopSong() {
    if(songPlaying || practiceNote>=0) { AudioOcarina_SetUiInput(0,0); AudioOcarina_SetInstrument(OCARINA_INSTRUMENT_OFF); }
    songPlaying=false; practiceNote=-1; songMessage.clear();
}
void CancelSave() { confirmSave=false; }
void Reset() {
    StopSong(); mapFloor=mapScene=-1; confirmSave=false; saveMessage.clear();
}
void Draw(Page page) {
    if(!Ready()) { Theme::Wrapped("Opening journal..."); return; }
    if(page!=Page::Quest) { StopSong(); }
    if(page!=Page::Save) { confirmSave=false; }
    switch(page) {
        case Page::Items: Items(); break;
        case Page::BaseGear: BaseGear(); break;
        case Page::Quest: Quest(); break;
        case Page::Map: Map(); break;
        case Page::Save: SavePage(); break;
    }
}
} // namespace ARPG::AdventureJournal
