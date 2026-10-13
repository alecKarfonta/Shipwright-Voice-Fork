#include "ARPGTesting.h"
#include "ARPG.h"
#include "ARPGSave.h"
#include "AffixData.h"
#include "AffixSystem.h"
#include "ARPGRules.h"
#include "ItemModifierSystem.h"
#include "InventoryRules.h"
#include "CraftingRules.h"
#include "InventorySystem.h"
#include "EnemySystem.h"
#include "ExpeditionSystem.h"
#include "SkillUI.h"
#include <libultraship/libultraship.h>
#include "SkillSystem.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ShipInit.hpp"
#include <imgui.h>
#include <cstdlib>
#include <string_view>
#include <spdlog/spdlog.h>

extern "C" {
#include "functions.h"
#include "variables.h"
#include "macros.h"
extern PlayState* gPlayState;
}

namespace ARPG::Testing {

bool Active() {
    const char* sandbox = std::getenv("SOH_ARPG_SANDBOX");
    return sandbox != nullptr && std::string_view(sandbox) == "1" &&
           gSaveContext.fileNum == 0xFF && GameInteractor::IsSaveLoaded(true);
}

static void Refill() {
    gSaveContext.health = gSaveContext.healthCapacity;
    gSaveContext.magic = static_cast<int8_t>(gSaveContext.magicCapacity);
    gSaveContext.magicTarget = gSaveContext.magic;
    gSaveContext.magicState = MAGIC_STATE_IDLE;
    SkillSystem::ResetRuntime();
}

static void GivePoints() {
    gARPGSave.passivePoints = 49;
    gARPGSave.activePoints = 20;
}

// [TIME] | [INFO] | Testing.Waystone | Preview shrine states only in the isolated temporary adventure; existing expedition actors are cleaned first.
static void PreviewWaystone(int oath,int cache,int objective=0,bool warden=false) {
    if(!Active() || !Expeditions::StartRestriction().empty()) { return; }
    Expeditions::End();
    if(!Expeditions::Start(0)) { return; }
    auto& run=gARPGSave.expedition;
    run.mutator=std::clamp(oath,0,4); run.wave=warden?2:0; run.objective=objective;
    run.phase=cache==1?ExpeditionRules::Reward:cache==2?ExpeditionRules::Rest:ExpeditionRules::Queued;
}

void DrawControls() {
    if (!Active() || !ImGui::CollapsingHeader("Testing shortcuts", ImGuiTreeNodeFlags_DefaultOpen)) { return; }
    ImGui::TextWrapped("Temporary adventure: your normal save is separate. Restart testing for a fresh character.");
    ImGui::BeginDisabled(!GameInteractor::IsPlayerInControl());
    if (ImGui::Button("Refill health + magic / reset cooldowns")) { Refill(); }
    if (ImGui::Button("Swordmaster build")) { SkillSystem::PrepareTestBuild(0); }
    ImGui::SameLine(); if (ImGui::Button("Spellblade build")) { SkillSystem::PrepareTestBuild(1); }
    ImGui::SameLine(); if (ImGui::Button("Warden build")) { SkillSystem::PrepareTestBuild(2); }
    if (ImGui::Button("Refill skill points")) { GivePoints(); }
    ImGui::SameLine();
    ImGui::BeginDisabled(!ItemsEnabled());
    if (ImGui::Button("Roll boss gear for all slots")) {
        for (int slot = 0; slot < ARPG_LOOT_SLOT_COUNT; ++slot) {
            if (Loot::WeaponSlot(slot)?ItemModifierSystem::WeaponOwned(slot):CUR_EQUIP_VALUE(slot)!=0) { ItemModifierSystem::GiveItemDrop(ItemModifierSystem::RollItemDrop(ARPG_TIER_BOSS, slot)); }
        }
    }
    ImGui::EndDisabled();
    if(ImGui::CollapsingHeader("Adventure relics")) {
        static int selectedRelic=19;
        if(ImGui::BeginCombo("Relic",Loot::Uniques[selectedRelic].name)) {
            for(int id=19;id<=Loot::UniqueCount;++id)if(ImGui::Selectable(Loot::Uniques[id].name,id==selectedRelic))selectedRelic=id;
            ImGui::EndCombo();
        }
        ImGui::TextWrapped("%s",Loot::WeaponPowerDescription([](int id){ARPGItemInstance item{};item.uniqueId=id;return item;}(selectedRelic)));
        ImGui::BeginDisabled(InventoryRules::FreeSlot(gARPGSave)<0);
        if(ImGui::Button("Give selected relic")) {
            ARPGItemInstance item{};item.slot=Loot::Uniques[selectedRelic].slot;item.baseEquip=1;item.itemLevel=35;
            Loot::IdentifyUnique(item,selectedRelic);ItemModifierSystem::GiveItemDrop(item);
        }
        ImGui::EndDisabled();
    }
    if(ImGui::CollapsingHeader("Relic wardrobe")) {
        static int wardrobe=0;
        const char* names[]={"Cinderwake / Briar Oath","Moonwell / Dawnglass","Briarwatch set","Cinderbound set","Tidecaller set"};
        ImGui::Combo("Collection",&wardrobe,names,5);
        if(ImGui::Button("Wear and drop matching relics")) { PreviewRelics(wardrobe); }
    }
    Enemies::DrawTesting();
    static int selectedEnemy = 0;
    const char* enemies[] = { "Stalfos", "Wolfos", "White Wolfos", "Lizalfos", "Dinolfos", "Iron Knuckle", "Deku Baba" };
    static const s16 ids[] = { ACTOR_EN_TEST, ACTOR_EN_WF, ACTOR_EN_WF, ACTOR_EN_ZF, ACTOR_EN_ZF, ACTOR_EN_IK, ACTOR_EN_DEKUBABA };
    static const s16 params[] = { 2, int16_t(0xFF00), int16_t(0xFF01), -1, -2, 2, 0 };
    ImGui::Combo("Test enemy", &selectedEnemy, enemies, 7);
    static int selectedRank = 0;
    const char* ranks[] = { "Random", "Ordinary", "Champion", "Elite" };
    ImGui::Combo("Enemy rank", &selectedRank, ranks, 4);
    if (ImGui::Button("Spawn enemy ahead")) {
        auto* player = GET_PLAYER(gPlayState);
        const auto& pos = player->actor.world.pos;
        auto* enemy = Actor_Spawn(&gPlayState->actorCtx, gPlayState, ids[selectedEnemy],
                    pos.x + Math_SinS(player->actor.shape.rot.y) * 150.0f, pos.y,
                    pos.z + Math_CosS(player->actor.shape.rot.y) * 150.0f, 0, player->actor.shape.rot.y + 0x8000, 0, params[selectedEnemy]);
        if (enemy && selectedRank > 0 && AffixesEnabled()) { AffixSystem::Configure(enemy, selectedRank - 1); }
    }
    if(ImGui::CollapsingHeader("Oath shrine preview")) {
        static int oath=0;
        const char* names[]={"Iron procession","Wild hunt","Cinder oath","Winter vigil","Storm covenant"};
        ImGui::Combo("Shrine oath",&oath,names,5);
        auto restriction=Expeditions::StartRestriction();
        if(!restriction.empty()) { ImGui::TextWrapped("%s",restriction.c_str()); }
        ImGui::BeginDisabled(!restriction.empty());
        if(ImGui::Button("Preview encounter shrine")) { PreviewWaystone(oath,0); }
        ImGui::SameLine(); if(ImGui::Button("Test ritual encounter")) { PreviewWaystone(oath,0,ExpeditionRules::Ritual); }
        if(ImGui::Button("Test spirit relay")) { PreviewWaystone(oath,0,ExpeditionRules::SpiritRelay); }
        if(ImGui::Button("Test warden attacks")) { PreviewWaystone(oath,0,ExpeditionRules::Gauntlet,true); }
        if(ImGui::Button("Preview earned coffer")) { PreviewWaystone(oath,1); }
        ImGui::SameLine(); if(ImGui::Button("Preview empty coffer")) { PreviewWaystone(oath,2); }
        ImGui::EndDisabled();
    }
    if (ImGui::Button("Switch child / adult")) {
        gPlayState->linkAgeOnLoad = LINK_IS_ADULT ? LINK_AGE_CHILD : LINK_AGE_ADULT;
        gPlayState->nextEntranceIndex = gSaveContext.entranceIndex;
        gPlayState->transitionTrigger = TRANS_TRIGGER_START;
        gPlayState->transitionType = TRANS_TYPE_INSTANT;
    }
    ImGui::SameLine();
    if (ImGui::Button("Choose scene (debug warp)")) {
        gSaveContext.respawnFlag = 0;
        gSaveContext.cutsceneIndex = 0;
        gPlayState->state.running = false;
        SET_NEXT_GAMESTATE(&gPlayState->state, Select_Init, SelectContext);
    }
    ImGui::EndDisabled();
}

static const ARPGItemMod pairs[4][2] = {
    {{ARPG_MOD_FLAME,24},{ARPG_MOD_BRUTAL,26}},
    {{ARPG_MOD_AEGIS,5},{ARPG_MOD_VITALITY,18}},
    {{ARPG_MOD_MAGICAFF,3},{ARPG_MOD_VITALITY,20}},
    {{ARPG_MOD_SWIFT,12},{ARPG_MOD_LUCKY,25}}
};

static bool gearPreviewRequested = false;
static int basePreview = 0;
static int relicPreview = -1;
void PreviewGear() {
    if (!Active()) { return; }
    gearPreviewRequested = true;
    basePreview = 0; relicPreview=-1;
    Inventory::Close();
}
void PreviewBaseGear() {
    if (!Active()) { return; }
    relicPreview=-1; basePreview=basePreview%3+1;
    gearPreviewRequested=true;
    Inventory::Close();
}
void PreviewRelics(int wardrobe) {
    if(!Active() || wardrobe<0 || wardrobe>4) { return; }
    relicPreview=wardrobe; basePreview=0; gearPreviewRequested=true; Inventory::Close();
}
// [TIME] | [INFO] | Testing.CraftingKit | Only the temporary adventure can conjure materials and catalog examples.
void GiveCraftingKit() {
    if(!Active() || !ItemsEnabled()) { return; }
    for(int id=1;id<ARPG_MATERIAL_COUNT;++id) { gARPGSave.materials[id]=std::max<uint16_t>(gARPGSave.materials[id],id==Loot::Essence?200:6); }
    auto base=[](int slot) { ARPGItemInstance item{}; item.slot=slot; item.itemLevel=35;
        item.baseEquip=slot==ARPG_SLOT_SWORD?(LINK_IS_CHILD?1:2):slot==ARPG_SLOT_SHIELD?2:1; return item; };
    for(int slot=0;slot<ARPG_LOOT_SLOT_COUNT;++slot) { auto item=base(slot); item.socketCount=slot==0?2:3; InventoryRules::Collect(gARPGSave,item); }
    for(int id=1;id<=Loot::UniqueCount;++id) { auto item=base(Loot::Uniques[id].slot); Loot::IdentifyUnique(item,id); item.socketCount=1; InventoryRules::Collect(gARPGSave,item); }
    for(int id=1;id<=Loot::SetCount;++id) { for(int slot=0;slot<4;++slot) { auto item=base(slot); Loot::IdentifySet(item,id); InventoryRules::Collect(gARPGSave,item); } }
    if(GameInteractor::IsPlayerInControl()) {
        auto* player=GET_PLAYER(gPlayState);
        for(int i=0;i<3;++i) { Actor source{}; source.world.pos=player->actor.world.pos;
            source.world.pos.x+=Math_SinS(player->actor.shape.rot.y)*120+(i-1)*50;
            source.world.pos.z+=Math_CosS(player->actor.shape.rot.y)*120;
            ItemModifierSystem::SpawnMaterialDrop(&source,i==0?13:i==1?19:20);
        }
    }
}

// [TIME] | [INFO] | Testing.PreviewGear | Equip a matching set and place four physical pickups ahead, restricted to the temporary adventure.
static void PrepareGearPreview() {
    auto* player = GET_PLAYER(gPlayState);
    for (int slot = 0; slot < ARPG_SLOT_COUNT_; ++slot) {
        ARPGItemInstance item{};
        item.slot = slot; item.baseEquip = CUR_EQUIP_VALUE(slot);
        if (slot == ARPG_SLOT_SWORD) { item.baseEquip = LINK_IS_CHILD ? 1 : 2; }
        else if (LINK_IS_CHILD && slot != ARPG_SLOT_SHIELD) { item.baseEquip = 1; }
        else if (item.baseEquip == 0 || (LINK_IS_CHILD && item.baseEquip == 3)) { item.baseEquip = 1; }
        item.rarity = ARPG_RARITY_RARE; item.itemLevel = 20; item.modCount = 2;
        item.mods[0] = pairs[slot][0]; item.mods[1] = pairs[slot][1];
        if(basePreview) {
            item.baseEquip=LINK_IS_CHILD?1:basePreview;
            if(!LINK_IS_CHILD && slot==ARPG_SLOT_SWORD && basePreview==1) { item.baseEquip=2; }
            item.modCount=0; item.mods[0]={}; item.mods[1]={}; item.rarity=ARPG_RARITY_COMMON;
        }
        if(relicPreview>=0) {
            if(relicPreview<2) { Loot::IdentifyUnique(item,slot*2+1+relicPreview); }
            else { Loot::IdentifySet(item,relicPreview-1); }
        }
        // [TIME] | [INFO] | Testing.RelicWardrobe | Reuse catalog examples while cycling collections, so repeated previews do not fill the backpack.
        auto matches=[&](const ARPGItemInstance& existing) {
            return relicPreview>=0 && InventoryRules::Present(existing) && existing.slot==item.slot &&
                existing.baseEquip==item.baseEquip && existing.uniqueId==item.uniqueId && existing.setId==item.setId;
        };
        if(!matches(gARPGSave.equipment[slot])) {
            int index=-1;
            for(int bag=0;bag<ARPG_BACKPACK_CAPACITY;++bag) { if(matches(gARPGSave.backpack[bag])) { index=bag; break; } }
            if(index<0) { index=InventoryRules::FreeSlot(gARPGSave); if(index>=0 && !InventoryRules::Collect(gARPGSave,item)) { index=-1; } }
            if(index>=0) { Inventory::Equip(index); }
        }
        Actor source{};
        float side = (slot - 1.5f) * 85.0f;
        float sn = Math_SinS(player->actor.shape.rot.y), cs = Math_CosS(player->actor.shape.rot.y);
        source.world.pos = player->actor.world.pos;
        source.world.pos.x += sn * 190 + cs * side;
        source.world.pos.z += cs * 190 - sn * side;
        source.world.pos.y += 20;
        ItemModifierSystem::SpawnItemDrop(&source,item);
    }
    auto* camera = GET_ACTIVE_CAM(gPlayState);
    player->yaw = player->actor.world.rot.y = player->actor.shape.rot.y =
        Math_Vec3f_Yaw(&player->actor.world.pos, &camera->eye);
}

// [TIME] | [INFO] | Testing.Initialize | Only an explicitly launched temporary debug adventure receives the testing preset.
static void RegisterTesting() {
    static bool prepared = false;
    COND_HOOK(OnLoadGame, true, [](int32_t) { prepared = false; gearPreviewRequested = false; basePreview = 0; relicPreview=-1; });
    COND_HOOK(OnPlayerUpdate, true, []() {
        if (gearPreviewRequested && Active() && GameInteractor::IsPlayerInControl()) {
            gearPreviewRequested = false;
            PrepareGearPreview();
        }
        if (prepared || !Active() || !GameInteractor::IsPlayerInControl() || !(GET_PLAYER(gPlayState)->actor.bgCheckFlags & BGCHECKFLAG_GROUND)) { return; }
        prepared = true;
        if (gARPGSave.level < 20) {
            ARPG::Rules::GrantXp(gARPGSave, ARPG::Rules::XpThreshold(19) - gARPGSave.xp);
        }
        GivePoints();
        SkillSystem::PrepareTestBuild(0);
        if (ItemsEnabled() && InventoryRules::Count(gARPGSave) == 0 && gARPGSave.itemsFound == 0) {
            for (int round = 0; round < 4; ++round) {
                for (int slot = 0; slot < ARPG_SLOT_COUNT_; ++slot) {
                    if (CUR_EQUIP_VALUE(slot) == 0) { continue; }
                    int index = InventoryRules::FreeSlot(gARPGSave);
                    auto item = ItemModifierSystem::RollItemDrop(round == 0 ? ARPG_TIER_CHAMPION : ARPG_TIER_BOSS, slot);
                    if (round == 0) { item.baseEquip = CUR_EQUIP_VALUE(slot); }
                    // [TIME] | [INFO] | Testing.Gallery | Four ordinary modifier pairs showcase the paintings only in the temporary adventure.
                    if (round == 1) {
                        item.baseEquip = CUR_EQUIP_VALUE(slot);
                        item.rarity = ARPG_RARITY_RARE;
                        item.itemLevel = 20;
                        item.modCount = 2;
                        item.mods[0] = pairs[slot][0]; item.mods[1] = pairs[slot][1];
                        item.mods[2] = {};
                    }
                    InventoryRules::Collect(gARPGSave, item);
                    if (round == 0) { Inventory::Equip(index); }
                }
            }
        }
        if (auto window = Ship::Context::GetRawInstance()->GetWindow()->GetGui()->GetGuiWindow("ARPG Mod")) { window->Hide(); }
        Refill();
        if(const char* forge=std::getenv("SOH_ARPG_FORGE_PREVIEW"); forge && std::string_view(forge)=="1") {
            GiveCraftingKit(); Inventory::Open(); return;
        }
        const char* gearPreview = std::getenv("SOH_ARPG_GEAR_PREVIEW");
        if (gearPreview && std::string_view(gearPreview) == "relics") { PreviewRelics(0); }
        else if (gearPreview && std::string_view(gearPreview) == "base") { PreviewBaseGear(); }
        else if (gearPreview && std::string_view(gearPreview) == "1") { PreviewGear(); }
        else { SkillUI::OpenJournal(); }
        SPDLOG_INFO("[ARPG testing] Ready: temporary adult/child adventure, level {}, 49 passive and 20 active points, intro skipped.", gARPGSave.level);
    });
}
static RegisterShipInitFunc testingInit(RegisterTesting);

}
