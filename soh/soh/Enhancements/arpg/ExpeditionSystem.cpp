#include "ExpeditionSystem.h"
#include "ExpeditionRules.h"
#include "RitualRules.h"
#include "WardenCombat.h"
#include "SkillSystem.h"
#include "ARPG.h"
#include "ARPGSave.h"
#include "ARPGTheme.h"
#include "EnemySystem.h"
#include "AffixSystem.h"
#include "GearVisuals.h"
#include "InventorySystem.h"
#include "ItemModifierSystem.h"
#include "soh/Enhancements/EnemyDifficulty/EnemyDifficulty.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/nametag.h"
#include "soh/Network/Anchor/Anchor.h"
#include "soh/ShipInit.hpp"
#include "soh/cvar_prefixes.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include <array>
#include <imgui.h>
extern "C" {
#include "functions.h"
#include "variables.h"
#include "macros.h"
extern PlayState* gPlayState;
}
namespace ARPG::Expeditions {
namespace {
using namespace ExpeditionRules;
std::array<Actor*,4> actors{};
Vec3f origin{};
std::array<Vec3f,3> wardSites{};
ARPG::Ritual::Channel channel{};
Relay::State relay{};
bool wardsPlaced=false;
int lastHealth=-1;
int scene=-1;
bool anchored=false;
float landmarkYaw=0,cacheOpen=0;
uint32_t nextSpawn=0;
std::string message="Choose a wilderness region and open the expedition journal.";
int targetSlot=0;
auto& Run() { return gARPGSave.expedition; }
int Member(Actor* actor) {
    if(!actor) { return -1; }
    for(int i=0;i<4;++i) { if(actors[i]==actor) { return i; } } return -1;
}
void ClearActors() {
    auto old=actors; actors={}; Warden::Reset();
    for(auto* actor:old) { if(actor && actor->update) { Actor_Kill(actor); } }
}
void ResetRitual() { wardSites={}; channel={}; relay={}; wardsPlaced=false; lastHealth=-1; }
void ResetRuntime() { Warden::Reset(); actors={}; anchored=false; scene=-1; nextSpawn=0; landmarkYaw=cacheOpen=0; ResetRitual(); }
bool Connected() {
    auto* anchor=Anchor::Instance; return anchor && anchor->isEnabled && anchor->isConnected;
}
void Fail(const char* reason) {
    ExpeditionRules::End(Run(),true); ClearActors(); anchored=false; message=reason;
}
// [TIME] | [INFO] | Expeditions.Configure | Give each route explicit counters and deterministic stats without touching native actor attack states.
void Configure(Actor* actor,int member) {
    const auto& r=Run(); const auto& modifier=Modifiers[r.mutator];
    auto* affix=AffixSystem::GetAffixData(actor); if(!affix) { return; }
    if(affix->appliedHpMult>0) { EnemyDifficulty::ApplyHealthMultiplier(actor,1.f/affix->appliedHpMult); }
    int baseHealth=actor->colChkInfo.health;
    bool warden=ExpeditionRules::Warden(r,member);
    affix->level=ItemLevel(r);
    float hp=(1.f+affix->level*.035f)*(warden?2.8f:r.wave==1?1.6f:1.f)*modifier.hp/100.f;
    EnemyDifficulty::ApplyHealthMultiplier(actor,hp);
    affix->appliedHpMult=baseHealth?actor->colChkInfo.health/float(baseHealth):1;
    affix->hpMult=std::min(65535,int(hp*256));
    affix->dmgMult=std::min(4096,int(256*OutgoingScale(r)*modifier.damage/100.f*(warden?1.3f:1.f)));
    affix->speedMult=256*modifier.speed/100;
    std::fill(std::begin(affix->elemMult),std::end(affix->elemMult),256);
    std::fill(std::begin(affix->affixIds),std::end(affix->affixIds),0);
    affix->affixCount=modifier.enchantment?1:0;
    if(modifier.enchantment) { affix->affixIds[0]=modifier.enchantment; }
    NameTag_RemoveForActorByTag(actor,"ARPGAffix");
    NameTagOptions options{}; options.tag="ARPGAffix"; options.yOffset=15; options.scale=warden?.32f:.25f;
    options.textColor=warden?Color_RGBA8{245,184,77,255}:Color_RGBA8{166,222,210,255};
    auto label=std::string(warden?(r.objective!=Gauntlet?"Sealed warden":"Warden"):(r.objective==SpiritRelay?"Spirit bearer":"Oathbound"));
    NameTag_RegisterForActorWithOptions(actor,label.c_str(),options);
}
void TickRitual(Player* player,bool inControl,bool damaged) {
    auto& r=Run(); if(r.objective!=ExpeditionRules::Ritual || !wardsPlaced) { return; }
    int nearby=-1;
    for(int i=0;i<3;++i) {
        if(!(r.sealMask&(1<<i)) && std::abs(player->actor.world.pos.y-wardSites[i].y)<30 &&
           std::hypot(player->actor.world.pos.x-wardSites[i].x,player->actor.world.pos.z-wardSites[i].z)<=ARPG::Ritual::Reach) { nearby=i; break; }
    }
    bool canHold=inControl && (player->actor.bgCheckFlags&BGCHECKFLAG_GROUND) && std::abs(player->actor.speedXZ)<1.f &&
        player->meleeWeaponState==0 && !SkillSystem::IsPerformingAction() &&
        !(player->stateFlags1&(PLAYER_STATE1_IN_WATER|PLAYER_STATE1_ON_HORSE|PLAYER_STATE1_READY_TO_FIRE|
          PLAYER_STATE1_CHARGING_SPIN_ATTACK|PLAYER_STATE1_USING_BOOMERANG|PLAYER_STATE1_DAMAGED|PLAYER_STATE1_CARRYING_ACTOR));
    if(ARPG::Ritual::Tick(r,channel,nearby,canHold,damaged)) {
        message=r.sealMask==7?"The ritual is broken! The warden is exposed and takes 50% more damage.":
                             "Ward broken. Move to another glowing ward stone and hold your ground.";
        if(r.sealMask==7 && actors[0] && actors[0]->update) {
            NameTag_RemoveForActorByTag(actors[0],"ARPGAffix");
            NameTagOptions options{}; options.tag="ARPGAffix"; options.yOffset=15; options.scale=.32f;
            options.textColor={245,202,105,255};
            NameTag_RegisterForActorWithOptions(actors[0],"Exposed warden / +50% damage taken",options);
        }
    } else if(damaged && nearby>=0) { message="Ritual interrupted. Regain your footing and try again."; }
}
Relay::Point Point(Vec3f p) { return {p.x,p.y,p.z}; }
void TickRelay(Player* player,bool inControl,bool damaged) {
    auto& r=Run(); if(r.objective!=SpiritRelay || !wardsPlaced) { return; }
    Relay::Input input{};
    input.player=Point(player->actor.world.pos); input.refuge=Point(origin);
    input.safeDrop=damaged?Point(Enemies::SafeSpiritPosition(player->actor.world.pos,origin)):input.player;
    input.active=gPlayState->pauseCtx.state==0 && gPlayState->transitionTrigger==TRANS_TRIGGER_OFF && (inControl || damaged); input.damaged=damaged;
    input.canInteract=inControl && (player->actor.bgCheckFlags&BGCHECKFLAG_GROUND) &&
        !(player->stateFlags1&(PLAYER_STATE1_IN_WATER|PLAYER_STATE1_ON_HORSE|PLAYER_STATE1_DAMAGED));
    for(int i=0;i<3;++i) { input.lanterns[i]=Point(wardSites[i]); }
    auto event=Relay::Tick(r,relay,input);
    if(event==Relay::Event::PickedUp) { message="Spirit gathered. Run to any unlit lantern; you can fight while carrying it."; }
    if(event==Relay::Event::Dropped) { message="Spirit dropped! Regain your footing, then recover it."; }
    if(event==Relay::Event::Returned) { message="An unattended spirit returned to the waystone."; }
    if(event==Relay::Event::Delivered) {
        message=r.sealMask==7?"All lanterns are lit. The warden is exposed!":"Lantern lit. Defeat another bearer and gather its spirit.";
        if(r.sealMask==7 && actors[0] && actors[0]->update) {
            NameTag_RemoveForActorByTag(actors[0],"ARPGAffix");
            NameTagOptions options{}; options.tag="ARPGAffix"; options.yOffset=15; options.scale=.32f;
            options.textColor={245,202,105,255};
            NameTag_RegisterForActorWithOptions(actors[0],"Unbound warden",options);
        }
    }
}
void Tick() {
    if(!gPlayState || !GameInteractor::IsSaveLoaded(true)) { return; }
    auto& r=Run();
    if(!Running(r)) { return; }
    if(!ItemsEnabled() || !Enemies::Enabled() || !CVarGetInteger(CVAR_ENHANCEMENT("ARPG.Enchanted"),1) || Connected()) {
        if(r.phase==Reward || r.phase==Rest) { ClearActors(); ResetRuntime(); return; }
        Fail("Expedition ended: required gameplay systems were disabled or a co-op session began."); return;
    }
    if(gSaveContext.health<=0) {
        if(r.phase!=Reward && r.phase!=Rest) { Fail("You fell. Your claimed loot and deepest clear are kept."); }
        return;
    }
    if(r.phase!=Queued && r.phase!=Combat && r.phase!=Reward && r.phase!=Rest) { return; }
    auto* player=GET_PLAYER(gPlayState);
    bool damaged=lastHealth>=0 && gSaveContext.health<lastHealth; lastHealth=gSaveContext.health;
    bool inControl=GameInteractor::IsPlayerInControl() && gPlayState->transitionTrigger==TRANS_TRIGGER_OFF;
    if(r.phase==Combat) { TickRitual(player,inControl,damaged); TickRelay(player,inControl,damaged); }
    if(!inControl) { if(r.phase==Combat) { Warden::Tick(gPlayState,actors[0],r.mutator,false); } return; }
    bool settled=r.phase==Reward || r.phase==Rest;
    if(settled) { cacheOpen=Waystone::AdvanceLid(cacheOpen,Waystone::FromExpedition(r).cache); }
    if(Enemies::CurrentRegion().population!=Enemies::Population::Wilderness) {
        if(settled) { return; } Fail("The expedition ended when you left the wilderness."); return;
    }
    if(!settled && anchored && (scene!=gPlayState->sceneNum || std::hypot(player->actor.world.pos.x-origin.x,player->actor.world.pos.z-origin.z)>1500)) {
        Fail("The expedition ended when you left the waystone's reach."); return;
    }
    if(r.phase==Combat) {
        for(int i=0;i<r.encounterSize;++i) {
            if(!(r.defeatedMask&(1<<i)) && (!actors[i] || !actors[i]->update)) {
                r.phase=Queued; r.encounterSize=r.defeatedMask=r.sealMask=0; channel={}; relay={}; ClearActors(); nextSpawn=gPlayState->gameplayFrames+100;
                message="An encounter was interrupted. The wave will retry; no clear was awarded."; break;
            }
        }
        if(r.phase==Combat && ExpeditionRules::Warden(r,0) && !(r.defeatedMask&1)) { Warden::Tick(gPlayState,actors[0],r.mutator,true); }
        else { Warden::Reset(); }
        return;
    }
    if(gPlayState->gameplayFrames<nextSpawn || !(player->actor.bgCheckFlags&BGCHECKFLAG_GROUND) ||
       (player->stateFlags1&(PLAYER_STATE1_IN_WATER|PLAYER_STATE1_ON_HORSE))) { return; }
    if(!anchored) {
        if(!settled) { Enemies::ClearPacks(); }
        if(!Enemies::FindWaystonePosition(origin)) {
            nextSpawn=gPlayState->gameplayFrames+100; message="Move into open ground so the waystone can appear beside you."; return;
        }
        scene=gPlayState->sceneNum; anchored=true;
        landmarkYaw=std::atan2(player->actor.world.pos.x-origin.x,player->actor.world.pos.z-origin.z);
        cacheOpen=settled?1.f:0.f;
        nextSpawn=gPlayState->gameplayFrames+30; return;
    }
    if(settled) { return; }
    if(r.objective!=Gauntlet && !wardsPlaced) {
        if(!Enemies::FindRitualPositions(origin,wardSites)) {
            // Retry the entire site beside Link, rather than anchoring an impossible ritual forever.
            anchored=false; nextSpawn=gPlayState->gameplayFrames+100;
            message="The objective needs three reachable sites. Move to a wider clearing."; return;
        }
        wardsPlaced=true;
    }
    ClearActors();
    auto spawned=Enemies::SpawnExpeditionWave(r.objective!=Gauntlet || r.wave==2?Enemies::PackKind::EliteGuard:r.wave==1?Enemies::PackKind::Champions:Enemies::PackKind::Warband,WaveSize(r));
    if(spawned.empty()) { nextSpawn=gPlayState->gameplayFrames+100; message="Move into open ground near the waystone. Waiting for safe space for the whole wave."; return; }
    for(size_t i=0;i<spawned.size();++i) { actors[i]=spawned[i]; Configure(actors[i],static_cast<int>(i)); }
    BeginWave(r,static_cast<int>(spawned.size())); channel={}; relay={}; lastHealth=gSaveContext.health;
    message=r.objective==SpiritRelay?"Defeat the spirit bearers. Gather their spirits and carry one to each unlit lantern.":
            r.objective==ExpeditionRules::Ritual?"Hold still beside each ward for three seconds. Damage and attacks interrupt the ritual.":
                                               "Defeat the oathbound. Stay within the waystone's reach.";
}
void Defeated(void* ref) {
    int index=Member(static_cast<Actor*>(ref));
    if(index<0 || !ExpeditionRules::Defeat(Run(),index)) { return; }
    if(Run().objective==SpiritRelay && index>0) {
        Relay::Release(Run(),relay,index,Point(Enemies::SafeSpiritPosition(static_cast<Actor*>(ref)->world.pos,origin)));
        message="A spirit is free. Approach it, then carry it to an unlit lantern.";
    }
    if(index==0) { Warden::Reset(); }
    if(Run().phase==Queued) { nextSpawn=gPlayState->gameplayFrames+100; message="Wave cleared. The next oath approaches."; }
    if(Run().phase==Reward) { message="Depth cleared. Open Expeditions in the journal to choose your cache."; }
}
}
bool Busy() { return GameInteractor::IsSaveLoaded(true) && Running(Run()); }
std::string StartRestriction() {
    if(!ItemsEnabled() || !Enemies::Enabled()) { return "Enable ARPG items, affixes and regional enemies."; }
    if(!CVarGetInteger(CVAR_ENHANCEMENT("ARPG.Enchanted"),1)) { return "Enable elemental enemy enchantments for expedition oaths."; }
    if(Connected()) { return "Expeditions currently require a solo adventure."; }
    if(!gPlayState || Enemies::CurrentRegion().population!=Enemies::Population::Wilderness) { return "Travel to a wilderness region, such as Hyrule Field or the Lost Woods."; }
    if(!Inventory::CanCraft()) { return "Wait until Link is in control."; }
    return "";
}
bool Start(int slot) {
    auto restriction=StartRestriction(); if(!restriction.empty()) { message=restriction; return false; }
    if(slot<0 || slot>=ARPG_LOOT_SLOT_COUNT) { return false; }
    int base=Loot::WeaponSlot(slot)?(ItemModifierSystem::WeaponOwned(slot)?1:0):CUR_EQUIP_VALUE(slot); if(base<1 || base>3) { message="Equip a base item in the target slot first."; return false; }
    if(!ExpeditionRules::Start(Run(),RandInt(1,UINT32_MAX),slot,base,std::max<int>(gARPGSave.level,Enemies::CurrentRegion().minLevel))) { return false; }
    ClearActors(); ResetRuntime(); message="Choose an oath and objective. Each cleared depth grants one reward cache."; return true;
}
bool Choose(int choice) {
    if(!StartRestriction().empty() || !ChooseRoute(Run(),choice)) { return false; }
    message="Resume the adventure to begin the chosen objective."; return true;
}
bool Claim(int choice) {
    if(!Inventory::CanCraft()) { return false; }
    if(!ExpeditionRules::Claim(gARPGSave,choice)) { message="Make room in your backpack or material stacks. Your cache is still waiting."; return false; }
    message="Reward claimed. Choose a new oath to push deeper, or end the expedition and keep your loot."; return true;
}
bool Continue() {
    if(!StartRestriction().empty() || !ExpeditionRules::Continue(Run())) { return false; }
    ClearActors(); ResetRuntime(); message="Choose the next oath. Enemy damage and endurance rise with depth."; return true;
}
void OnFileLoaded() { ClearActors(); ResetRuntime(); ExpeditionRules::Recover(Run()); }
void End() { ExpeditionRules::End(Run()); ClearActors(); ResetRuntime(); message="Expedition ended. Claimed loot and records are kept."; }
int Remaining() {
    const auto& r=Run(); if(r.phase!=Combat) { return 0; }
    int remaining=0; for(int i=0;i<r.encounterSize;++i) { remaining+=!(r.defeatedMask&(1<<i)); } return remaining;
}
Waystone::View GetWaystoneView() {
    bool visible=Busy() && anchored && gPlayState && scene==gPlayState->sceneNum && ItemsEnabled() &&
                 Enemies::Enabled() && CVarGetInteger(CVAR_ENHANCEMENT("ARPG.Enchanted"),1) && !Connected();
    Waystone::View view{visible,origin.x,origin.y,origin.z,landmarkYaw,cacheOpen,Waystone::FromExpedition(Run())};
    view.wards=visible && wardsPlaced && Run().objective==ExpeditionRules::Ritual;
    for(int i=0;i<3;++i) {
        auto p=wardSites[i]; view.sites[i]={p.x,p.y,p.z,channel.seal==i?float(channel.ticks)/ARPG::Ritual::HoldTicks:0.f,
                                          bool(view.state.seals&(1<<i))};
    }
    return view;
}
Relay::View GetRelayView() {
    Relay::View view{};
    view.visible=GetWaystoneView().visible && wardsPlaced && Run().objective==SpiritRelay;
    if(!view.visible) { return view; }
    view.carried=relay.carried;
    for(int i=0;i<3;++i) {
        view.lanterns[i]={Point(wardSites[i]),bool(Run().sealMask&(1<<i))};
        const auto& spirit=relay.spirits[i];
        bool carried=spirit.state==Relay::SpiritState::Carried;
        view.spirits[i]={carried?Point(GET_PLAYER(gPlayState)->actor.world.pos):spirit.position,
                         spirit.state==Relay::SpiritState::Loose || carried,carried};
    }
    return view;
}
float DamageMultiplier(Actor* actor,unsigned int flags) {
    int member=Member(actor);
    if(!Busy() || member<0) { return 1; }
    if(Protected(Run(),member)) { return 0; }
    float scale=IncomingScale(Run())*Warden::OpeningMultiplier(actor);
    if(Run().objective==ExpeditionRules::Ritual && ExpeditionRules::Warden(Run(),member) && Run().sealMask==7) { scale*=1.5f; }
    if(Run().mutator==0) { if(flags&DMG_SWORD) { scale*=.65f; } else if(flags&(DMG_RANGED|DMG_EXPLOSIVE|DMG_HAMMER)) { scale*=1.5f; } }
    if(Run().mutator==1 && (flags&(DMG_ARROW|DMG_SLINGSHOT))) scale*=1.35f;
    if(Run().mutator==2 && (flags&(DMG_ARROW_ICE|DMG_MAGIC_ICE))) scale*=1.5f;
    if(Run().mutator==3 && (flags&DMG_FIRE)) scale*=1.5f;
    if(Run().mutator==4) {
        if(flags&DMG_HAMMER) scale*=1.5f;
        if(flags&(DMG_ARROW_LIGHT|DMG_MAGIC_LIGHT)) scale*=.65f;
    }
    return scale;
}
void DrawJournal() {
    using namespace ExpeditionRules;
    auto& r=Run();
    Theme::BeginPane("expeditionJournal",{0,0});
    Theme::Wrapped("WAYSTONE EXPEDITIONS"); Theme::Rule();
    Theme::Colored({.86f,.75f,.43f,1},"Deepest clear %u   /   Depths cleared %u   /   Expeditions %u",r.bestDepth,r.clears,r.runs);
    if(Theme::Section("How expeditions work")) {
        Theme::Wrapped("Choose an oath and objective. Clear a depth for one cache, then push deeper or return with your loot. Every third depth offers unique or set gear.");
        Theme::Wrapped("Stay near the waystone. Leaving or falling ends the attempt; claimed loot is kept. Reloading during combat retries that depth.");
        Theme::Wrapped("Warden attacks mark the ground before striking. Leave the marks, then retaliate during the opening for 50%% extra damage. Marks stop while the journal is open.");
        Theme::Wrapped("The shrine's crest shows your oath; its lights track waves, wards or lanterns. The coffer opens when your cache is earned.");
    }
    ImGui::Spacing(); Theme::Rule();
    if(r.phase==Idle || r.phase==Failed) {
        Theme::Wrapped("TARGET YOUR EQUIPMENT CACHE"); const auto& slots=Loot::SlotNames;
        for(int i=0;i<ARPG_LOOT_SLOT_COUNT;++i) { if(i%3) { ImGui::SameLine(); } if(Theme::Button(slots[i],{140,36},targetSlot==i)) { targetSlot=i; } }
        auto restriction=StartRestriction(); if(!restriction.empty()) { Theme::Wrapped("%s",restriction.c_str()); }
        ImGui::BeginDisabled(!restriction.empty());
        if(Theme::Button("Begin expedition",{240,42})) { Start(targetSlot); } ImGui::EndDisabled();
    } else {
        Theme::Colored({.67f,.9f,.8f,1},"DEPTH %u   /   Item level %d",r.depth,ItemLevel(r));
        if(r.phase==Route) {
            auto routes=Routes(r); auto restriction=StartRestriction();
            if(!restriction.empty()) { Theme::Wrapped("%s",restriction.c_str()); }
            for(int i=0;i<3;++i) {
                ImGui::PushID(i); const auto& m=Modifiers[routes[i]];
                auto objective=RouteObjective(r,i);
                ImGui::BeginDisabled(!restriction.empty()); if(Theme::Button(ObjectiveNames[objective],{280,42})) { Choose(i); } ImGui::EndDisabled();
                ImGui::SameLine(); Theme::Colored({.86f,.75f,.43f,1},"%s",m.name);
                if(Theme::Section("Objective & counters")) {
                    Theme::Wrapped("%s",ObjectiveDescriptions[objective]);
                    Theme::Wrapped("%s",m.description);
                    Theme::Wrapped("%s: %s.",Warden::Attacks[routes[i]].name,Warden::Attacks[routes[i]].counter);
                }
                ImGui::PopID(); ImGui::Spacing();
            }
        } else if(r.phase==Combat || r.phase==Queued) {
            Theme::Wrapped("%s / %s",Modifiers[r.mutator].name,ObjectiveNames[r.objective]);
            if(r.phase==Queued) {
                Theme::Wrapped("Resume to summon the encounter on open ground.");
            } else if(r.objective==ExpeditionRules::Ritual) {
                Theme::Wrapped("Wards broken %d / 3   /   %d enemies remaining",BrokenSeals(r),Remaining());
                if(Theme::Section("Breaking the ritual")) {
                    Theme::Wrapped("Hold still beside each ward for three seconds. Moving, attacking or taking damage interrupts progress. Broken wards stay broken.");
                    Theme::Wrapped("Break all three to expose the warden to 50%% extra damage. Thin the guard first or use control and mobility to reach the wards early.");
                }
            } else if(r.objective==SpiritRelay) {
                Theme::Wrapped("Lanterns lit %d / 3   /   %d enemies remaining",BrokenSeals(r),Remaining());
                if(Theme::Section("Carrying spirits")) {
                    Theme::Wrapped("Defeat a spirit bearer and approach its released spirit. Carry one at a time to any unlit lantern; no channeling is required. You can move and fight while carrying.");
                    Theme::Wrapped("Taking damage drops your spirit and briefly prevents pickup. Recover it after making space. Unattended spirits return to the waystone after thirty seconds. Light all three lanterns to expose the warden.");
                }
            } else { Theme::Wrapped("Wave %d of 3   /   %d remaining",r.wave+1,Remaining()); }
            Theme::Wrapped("%s: %s.",Warden::Attacks[r.mutator].name,Warden::Attacks[r.mutator].counter);
            if(Theme::Section("Oath modifiers")) { Theme::Wrapped("%s",Modifiers[r.mutator].description); }
            if(Theme::Button("Resume encounter",{240,42})) { Inventory::Close(); }
        } else if(r.phase==Reward) {
            Theme::Wrapped("CHOOSE ONE CACHE");
            const char* labels[]={"Claim equipment","Claim runes","Claim gems"};
            for(int choice=0;choice<3;++choice) {
                ImGui::PushID(choice); auto offer=RewardFor(r,choice);
                if(InventoryRules::Valid(offer.item)) { Theme::Colored({.96f,.8f,.45f,1},"%s",ItemModifierSystem::ItemName(offer.item).c_str());
                    if(Theme::Section("Item modifiers")) {
                        for(int m=0;m<offer.item.modCount;++m) { auto& mod=offer.item.mods[m]; Theme::Wrapped("%s",ItemModifierSystem::DescribeModifier(mod).c_str()); }
                    }
                }
                for(int id=1;id<ARPG_MATERIAL_COUNT;++id) { if(offer.materials[id]) { Theme::Wrapped("%s x %d",Loot::MaterialName(id).c_str(),offer.materials[id]); } }
                if(Theme::Button(labels[choice],{240,36})) { Claim(choice); }
                Theme::Rule(); ImGui::PopID();
                if(r.phase!=Reward) { break; }
            }
        } else if(r.phase==Rest) {
            Theme::Wrapped("Cache claimed. The next depth increases enemy endurance and damage.");
            auto restriction=StartRestriction();
            if(!restriction.empty()) { Theme::Wrapped("%s",restriction.c_str()); }
            ImGui::BeginDisabled(!restriction.empty());
            if(Theme::Button("Continue to the next depth",{320,42})) { Continue(); }
            ImGui::EndDisabled();
        }
        ImGui::Spacing();
        if(r.phase!=Reward && Theme::Button("End expedition & keep claimed loot",{400,36})) { End(); }
    }
    ImGui::Spacing(); Theme::Colored({.72f,.8f,.82f,1},"%s",message.c_str()); Theme::EndPane();
}
void DrawHud() {
    if(!Busy()) { return; } const auto& r=Run();
    Theme::Colored({.75f,.89f,.65f,1},"Depth %u / %s",r.depth,Modifiers[r.mutator].name);
    if(r.phase==Queued) { Theme::Wrapped("The waystone is gathering the oathbound..."); return; }
    if(r.phase==Combat) {
        if(r.objective==ExpeditionRules::Ritual) {
            Theme::Wrapped("Wards %d / 3   /   %d foes",BrokenSeals(r),Remaining());
            if(r.sealMask==7) { Theme::Colored({1,.8f,.35f,1},"Warden exposed / +50%% damage taken"); }
            else if(channel.interrupted) { Theme::Wrapped("Interrupted - regain your footing"); }
            else if(channel.seal>=0) { Theme::Colored({.65f,.95f,.9f,1},"Breaking ward %d... %d%%",channel.seal+1,channel.ticks*100/ARPG::Ritual::HoldTicks); }

        } else if(r.objective==SpiritRelay) {
            Theme::Wrapped("Lanterns %d / 3   /   %d foes",BrokenSeals(r),Remaining());
            if(r.sealMask==7) { Theme::Colored({1,.8f,.35f,1},"Warden exposed"); }
            else if(relay.carried>=0) { Theme::Colored({.65f,.95f,.9f,1},"Spirit carried - reach an unlit lantern"); }
            else if(relay.pickupLock) { Theme::Wrapped("Spirit dropped - regain your footing"); }
            else { Theme::Wrapped("Defeat bearers and gather their spirits"); }
        } else { Theme::Wrapped("Wave %d / 3   %d remaining",r.wave+1,Remaining()); }
        auto attack=Warden::GetView();
        if(attack.phase==Warden::Phase::Warning) { Theme::Colored({1,.77f,.4f,1},"%s",Warden::Attacks[attack.oath].counter); }
        else if(attack.phase==Warden::Phase::Opening && !Protected(r,0)) { Theme::Colored({.65f,.95f,.8f,1},"Oath spent - strike now / +50%% damage"); }
        if(anchored && gPlayState) {
            const auto& pos=GET_PLAYER(gPlayState)->actor.world.pos;
            if(std::hypot(pos.x-origin.x,pos.z-origin.z)>1200) { Theme::Colored({1,.55f,.3f,1},"Return toward the waystone"); }
        }
    }
    else { Theme::Wrapped(r.phase==Reward?"Cache ready - open Expeditions in the journal":"Open Expeditions in the journal to continue"); }
}
static void Register() {
    COND_HOOK(OnPlayerUpdate,true,Tick);
    COND_HOOK(OnEnemyDefeat,true,Defeated);
    COND_HOOK(OnActorDestroy,true,[](void* actor) { int i=Member(static_cast<Actor*>(actor)); if(i>=0) { actors[i]=nullptr; if(i==0) { Warden::Reset(); } } });
    COND_HOOK(OnSceneInit,true,[](int16_t) {
        if(anchored && (Run().phase==Queued || Run().phase==Combat)) { ExpeditionRules::End(Run(),true); message="The expedition ended when you left the region."; }
        ResetRuntime();
    });
    COND_HOOK(OnLoadGame,true,[](int32_t) { OnFileLoaded(); });
    COND_HOOK(OnPlayDestroy,true,[]() { ResetRuntime(); });
    COND_HOOK(OnPlayDrawEnd,true,[]() {
        if(Busy() && anchored && gPlayState && scene==gPlayState->sceneNum) {
            auto view=GetWaystoneView();
            GearVisuals::DrawWaystone(gPlayState,view);
            if(view.visible) { Warden::Draw(gPlayState); GearVisuals::DrawRelay(gPlayState,GetRelayView()); }
        }
    });
}
static RegisterShipInitFunc init(Register);
}
