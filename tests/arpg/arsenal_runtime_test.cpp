#include "soh/Enhancements/arpg/ARPG.h"
#include "soh/Enhancements/arpg/ARPGBridge.h"
#include "soh/Enhancements/arpg/ARPGSave.h"
#include "soh/Enhancements/arpg/ArsenalSystem.h"
#include "soh/Enhancements/arpg/SkillSystem.h"
#include "soh/Enhancements/arpg/SkillCombat.h"
#include "soh/Enhancements/arpg/ItemModifierSystem.h"
#include "soh/Enhancements/arpg/ExpeditionSystem.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/ObjectExtension/ObjectExtension.h"
#include "soh/ShipInit.hpp"
#include <libultraship/bridge/consolevariablebridge.h>
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <cstdlib>
extern "C" {
#include "functions.h"
#include "variables.h"
#include "macros.h"
#include "overlays/actors/ovl_En_Test/z_en_test.h"
#include "overlays/actors/ovl_Boss_Goma/z_boss_goma.h"
#include "overlays/actors/ovl_Boss_Dodongo/z_boss_dodongo.h"
void Sram_InitDebugSave(void);
void CollisionCheck_ApplyDamage(PlayState*,CollisionCheckContext*,Collider*,ColliderInfo*);
void BossGoma_UpdateHit(BossGoma*,PlayState*);
void BossGoma_FloorStunned(BossGoma*,PlayState*);
void BossGoma_FloorIdle(BossGoma*,PlayState*);
void BossDodongo_UpdateDamage(BossDodongo*,PlayState*);
void BossDodongo_Vulnerable(BossDodongo*,PlayState*);
void BossDodongo_Walk(BossDodongo*,PlayState*);
extern PlayState* gPlayState;extern GameState* gGameState;
}
namespace {
using namespace ARPG;
int phase=0,frames=0,faultChecks=0,renderFrames=0;
bool visualOnly=false;
Actor* foe=nullptr;Vec3f fixture{};Actor arrow{},boom{},hook{},bomb{},nut{};
nlohmann::json checks=nlohmann::json::array();std::filesystem::path output;
void Check(bool ok,const std::string& label) {
    if(!ok)throw std::runtime_error(label);checks.push_back(label);SPDLOG_INFO("[Arsenal QA] PASS: {}",label);
    std::ofstream(output)<<nlohmann::json{{"status","running"},{"checks",checks},{"phase",phase}}.dump(2);
}
void Result(const char* status,const std::string& error="") {phase=99;std::ofstream(output)<<nlohmann::json{{"status",status},{"checks",checks},{"error",error},{"fault_collision_checks",faultChecks},{"render_frames",renderFrames}}.dump(2);}
void Noop(Actor*,PlayState*){}
void Next(int value){phase=value;frames=0;}
void Warp(int entrance) {
    Arsenal::Reset(gPlayState);SkillCombat::CancelAll(gPlayState);SkillSystem::ResetRuntime();foe=nullptr;
    gSaveContext.entranceIndex=entrance;gSaveContext.cutsceneIndex=0;gSaveContext.sceneLayer=0;gSaveContext.respawnFlag=0;
    gGameState->running=false;SET_NEXT_GAMESTATE(gGameState,Play_Init,PlayState);
}
int Hit(Actor* target,Actor* source,uint32_t flags,bool hard=false,uint8_t alignment=AT_TYPE_PLAYER) {
    auto backup=target->colChkInfo;Collider a{},b{};ColliderInfo strike{},surface{};
    a.actor=source;a.atFlags=alignment;b.actor=target;b.acFlags=AC_HIT|(hard?AC_HARD:0);
    strike.toucher.dmgFlags=flags;strike.toucher.damage=2;surface.acHit=&a;surface.acHitInfo=&strike;surface.bumperFlags=BUMP_HIT;
    target->colChkInfo.damage=0;CollisionCheck_ApplyDamage(gPlayState,&gPlayState->colChkCtx,&b,&surface);
    int damage=target->colChkInfo.damage;target->colChkInfo=backup;return damage;
}
void Gear(int unique) {
    for(int slot=0;slot<4;++slot){gARPGSave.equipment[slot]={};gARPGSave.equipment[slot].slot=slot;}
    if(unique) {int slot=Loot::Uniques[unique].slot;auto& item=gARPGSave.equipment[slot];item.baseEquip=CUR_EQUIP_VALUE(slot);item.itemLevel=35;Loot::IdentifyUnique(item,unique);item.modCount=0;}
    SkillSystem::ResetRuntime();gSaveContext.magic=gSaveContext.magicTarget=48;gSaveContext.magicCapacity=96;gSaveContext.isMagicAcquired=1;gSaveContext.magicState=MAGIC_STATE_IDLE;
}
void CreditArrow(){gPlayState->gameplayFrames+=8;Hit(foe,&arrow,DMG_ARROW_NORMAL);}
void Charge(){CreditArrow();CreditArrow();CreditArrow();}
void Tick() {
    if(phase==99)return;
    try {
        ++frames;
        if(phase==0 && frames>60 && gGameState) {
            CVarSetInteger("gDeveloperTools.DebugSaveFileMode",2);gSaveContext.gameMode=GAMEMODE_NORMAL;gSaveContext.fileNum=0xFE;Sram_InitDebugSave();
            gSaveContext.gameMode=GAMEMODE_NORMAL;gSaveContext.fileNum=0xFF;gSaveContext.linkAge=LINK_AGE_ADULT;
            gSaveContext.dayTime=gSaveContext.skyboxTime=0x8000;
            for(const char* key:{"gEnhancements.ARPG.Enabled","gEnhancements.ARPG.Items","gEnhancements.ARPG.Skills"})CVarSetInteger(key,1);
            for(const char* key:{"gEnhancements.ARPG.Enemies","gEnhancements.ARPG.Packs","gEnhancements.ARPG.Affixes","gEnhancements.ARPG.Enchanted","gEnhancements.ARPG.Tactics","gEnhancements.ARPG.RegionalWorld","gOpenWindows.ARPG"})CVarSetInteger(key,0);
            SeedRng(0xA25E);Warp(ENTR_HYRULE_FIELD_PAST_BRIDGE_SPAWN);Next(1);return;
        }
        if(phase<10 && gPlayState && GameInteractor::IsPlayerInControl()){auto* p=GET_PLAYER(gPlayState);p->invincibilityTimer=40;p->actor.speedXZ=0;}
        if(phase==6 && gPlayState) {
            auto* p=GET_PLAYER(gPlayState);p->actor.world.pos=p->actor.prevPos=fixture;p->actor.velocity={};
        }
        if(phase==1 && frames>100 && GameInteractor::IsPlayerInControl()) {
            auto* player=GET_PLAYER(gPlayState);fixture=player->actor.world.pos;player->actor.shape.rot.y=0;
            foe=Actor_Spawn(&gPlayState->actorCtx,gPlayState,ACTOR_EN_TEST,fixture.x,fixture.y,fixture.z+70,0,0x8000,0,STALFOS_TYPE_2);
            Check(foe && foe->update,"Native Stalfos initializes with its original damage table");foe->update=Noop;foe->colChkInfo.health=200;
            arrow.id=ACTOR_EN_ARROW;boom.id=ACTOR_EN_BOOM;hook.id=ACTOR_ARMS_HOOK;bomb.id=ACTOR_EN_BOM;nut.id=ACTOR_EN_M_FIRE1;
            gARPGSave.level=1;for(auto& rank:gARPGSave.skillRanks)rank=0;
            for(int id:{ARPG_SKILL_SWORD_CLEAVE,ARPG_SKILL_MAGIC_FROST_NOVA,ARPG_SKILL_MAGIC_INFUSE,ARPG_SKILL_MAGIC_CHAIN,ARPG_SKILL_GUARD_DODGE})gARPGSave.skillRanks[id]=1;
            Gear(0);int normal=Hit(foe,&arrow,DMG_ARROW_NORMAL);
            Check(Hit(foe,&boom,DMG_BOOMERANG)==0 && Arsenal::GetStatus(foe).marked==80,"A native zero-damage boomerang stun arms a mark without inventing damage");
            Check(Hit(foe,&arrow,DMG_ARROW_NORMAL)>normal && !Arsenal::GetStatus(foe).marked,"A real arrow collision consumes the mark for a stronger finisher");
            ++gPlayState->gameplayFrames;Check(Hit(foe,&arrow,DMG_ARROW_NORMAL)==normal,"The finisher cannot be spent twice");
            ++gPlayState->gameplayFrames;Hit(foe,&boom,DMG_BOOMERANG,true);Check(!Arsenal::GetStatus(foe).marked,"Native hard shield contact cannot mark its wearer");
            Hit(foe,&hook,DMG_HOOKSHOT);Check(Arsenal::GetStatus(foe).tethered && Arsenal::MovementMultiplier(foe)==.75f,"Hookshot creates a timed tether on an accepted native stun");
            int tether=Hit(foe,&bomb,DMG_EXPLOSIVE);++gPlayState->gameplayFrames;int plain=Hit(foe,&bomb,DMG_EXPLOSIVE);
            Check(tether>plain && !Arsenal::GetStatus(foe).tethered && Arsenal::MovementMultiplier(foe)==.6f,"Bombs consume a tether and leave concussion movement control");
            auto pause=gPlayState->pauseCtx.state;gPlayState->pauseCtx.state=6;
            for(int i=0;i<50;++i)GameInteractor_ExecuteOnPlayerUpdate();Check(Arsenal::GetStatus(foe).concussed==40,"Paused native updates preserve weapon debuff durations");gPlayState->pauseCtx.state=pause;
            CVarSetInteger("gEnhancements.ARPG.Items",0);GameInteractor_ExecuteOnPlayerUpdate();Check(Arsenal::MovementMultiplier(foe)==1,"Disabling items clears native arsenal effects");
            CVarSetInteger("gEnhancements.ARPG.Items",1);Check(!Arsenal::GetStatus(foe).concussed,"Re-enabling items cannot revive old debuffs");
            ++gPlayState->gameplayFrames;Hit(foe,&boom,DMG_BOOMERANG,false,AT_TYPE_ENEMY);Check(!Arsenal::GetStatus(foe).marked,"Enemy-aligned projectiles cannot create player setups");
            auto category=foe->category;foe->category=ACTORCAT_NPC;Hit(foe,&boom,DMG_BOOMERANG);foe->category=category;
            Check(!Arsenal::GetStatus(foe).marked,"Friendly NPC categories cannot receive arsenal setup effects");
            Hit(foe,&nut,DMG_DEKU_NUT);Check(Arsenal::GetStatus(foe).concussed==30,"Native Deku-nut flash creates its shorter disruption window");
            Arsenal::Reset(gPlayState);player->actor.bgCheckFlags|=BGCHECKFLAG_GROUND;
            Check(Hit(foe,&player->actor,DMG_HAMMER_SWING)>0 && Arsenal::LiveFaults(gPlayState)==1,"Native hammer damage launches one ground-supported fault line");
            Hit(foe,&player->actor,DMG_HAMMER_SWING);Check(Arsenal::LiveFaults(gPlayState)==1,"Repeated hammer contacts cannot duplicate the fault line");Next(2);return;
        }
        if(phase==2 && visualOnly) {
            if(GameInteractor::IsPlayerInControl() && frames%40==0)Hit(foe,&GET_PLAYER(gPlayState)->actor,DMG_HAMMER_SWING);
            if(GameInteractor::IsPlayerInControl() && frames%160==1)Hit(foe,&boom,DMG_BOOMERANG);
            return;
        }
        if(phase==2 && frames>35) {
            Check(faultChecks>=3 && !Arsenal::LiveFaults(gPlayState),"All three fault nodes use native collision and expire without leaked actors");
            Gear(2);Charge();Check(SkillSystem::ItemPowerState().weaveHits==3,"Bow hits can charge Moonwell weave without requiring a sword attack");
            auto timer=SkillSystem::ItemPowerState().weave;auto pause=gPlayState->pauseCtx.state;gPlayState->pauseCtx.state=6;
            for(int i=0;i<20;++i)GameInteractor_ExecuteOnPlayerUpdate();Check(SkillSystem::ItemPowerState().weave==timer,"Paused game freezes prepared item powers");gPlayState->pauseCtx.state=pause;
            gSaveContext.magic=0;gSaveContext.isMagicAcquired=0;Check(!SkillSystem::TryCastActive(gPlayState,ARPG_SKILL_MAGIC_FROST_NOVA),"A free spell still requires native magic acquisition");gSaveContext.isMagicAcquired=1;
            Check(SkillSystem::TechniqueMagicCost(ARPG_SKILL_MAGIC_FROST_NOVA)==0 && SkillSystem::TechniqueMagicCost(ARPG_SKILL_MAGIC_INFUSE)>0,"Free magic applies only to the four offensive spells");
            auto total=gPlayState->actorCtx.total;gPlayState->actorCtx.total=180;
            Check(!SkillSystem::TryCastActive(gPlayState,ARPG_SKILL_MAGIC_FROST_NOVA) && SkillSystem::ItemPowerState().weaveHits==3,"Failed actor allocation preserves the free spell preparation");gPlayState->actorCtx.total=total;
            Check(SkillSystem::TryCastActive(gPlayState,ARPG_SKILL_MAGIC_FROST_NOVA) && !SkillSystem::ItemPowerState().weaveHits && gSaveContext.magic==0 && gSaveContext.magicState==MAGIC_STATE_IDLE,"Successful free cast at zero magic consumes exactly one preparation without a meter transaction");Next(3);return;
        }
        if(phase==3 && frames>35) {
            SkillCombat::CancelAll(gPlayState);Gear(1);Check(SkillSystem::TryCastActive(gPlayState,ARPG_SKILL_SWORD_CLEAVE),"Cinderwake primes a follow-up through a real technique cast");Next(4);return;
        }
        if(phase==4 && frames>15) {
            int before=SkillSystem::CooldownRemaining(ARPG_SKILL_SWORD_CLEAVE);CreditArrow();
            Check(SkillSystem::CooldownRemaining(ARPG_SKILL_SWORD_CLEAVE)==std::max(0,before-20) && !SkillSystem::ItemPowerState().furnace,"An arrow follow-up spends Furnace rhythm to shorten sword recharge once");
            Gear(3);Check(SkillSystem::TryCastActive(gPlayState,ARPG_SKILL_GUARD_DODGE),"Guardian dodge primes Briar reprisal");Next(5);return;
        }
        if(phase==5 && frames>12) {
            gSaveContext.health=gSaveContext.healthCapacity;gSaveContext.isDoubleDefenseAcquired=0;
            int health=gSaveContext.health;Health_ChangeBy(gPlayState,-12);int lost=health-gSaveContext.health;
            Check(lost>0 && SkillSystem::ItemPowerState().healing==std::min(8,lost),"Briar reprisal stores actual post-defense health loss with a half-heart cap");
            health=gSaveContext.health;CreditArrow();Check(gSaveContext.health-health==std::min(8,lost) && !SkillSystem::ItemPowerState().healing,"A ranged counter restores the stored loss exactly once");
            auto recharge=SkillSystem::ItemPowerState().recharge;gARPGSave.equipment[1]={};SkillSystem::ItemPowerState();
            Check(!SkillSystem::ItemPowerState().powers && SkillSystem::ItemPowerState().recharge==recharge,"Unequipping clears powers without erasing reprisal recharge");
            Gear(2);Charge();gARPGSave.equipment[0].sockets[0]=19;Check(!SkillSystem::ItemPowerState().weaveHits,"Changing sockets discards a prepared free spell");
            Gear(0);Actor_Kill(foe);foe=nullptr;
            auto* p=GET_PLAYER(gPlayState);p->actor.world.pos=p->actor.prevPos=fixture;p->actor.velocity={};p->actor.bgCheckFlags|=BGCHECKFLAG_GROUND;
            CVarSetInteger("gEnhancements.ARPG.Enemies",1);
            CVarSetInteger("gEnhancements.ARPG.Affixes",1);CVarSetInteger("gEnhancements.ARPG.Enchanted",1);
            Check(Expeditions::Start(0),"An arsenal fixture can start a real expedition");
            auto& run=gARPGSave.expedition;int route=-1;
            for(int i=0;i<3;++i)if(ExpeditionRules::RouteObjective(run,i)==ExpeditionRules::Ritual)route=i;
            Check(route>=0 && Expeditions::Choose(route),"The ritual route uses normal expedition initialization");run.mutator=0;
            Next(6);return;
        }
        if(phase==6 && frames>160 && gARPGSave.expedition.phase==ExpeditionRules::Combat) {
            Actor* warden=nullptr;Actor* guard=nullptr;
            for(auto* a=gPlayState->actorCtx.actorLists[ACTORCAT_ENEMY].head;a;a=a->next) {
                float scale=Expeditions::DamageMultiplier(a,DMG_SLASH_KOKIRI);
                if(scale==0)warden=a;else if(scale!=1)guard=a;
            }
            Check(warden && guard,"Native oath encounter contains a sealed warden and susceptible guard");
            for(uint32_t flags:{uint32_t(DMG_HAMMER_SWING),uint32_t(DMG_EXPLOSIVE),uint32_t(DMG_ARROW_NORMAL),uint32_t(DMG_ARROW_FIRE),uint32_t(DMG_ARROW_ICE)})
                Check(Hit(warden,&GET_PLAYER(gPlayState)->actor,flags)==0,"Sealed warden rejects an arsenal damage category");
            Hit(warden,&boom,DMG_BOOMERANG);Hit(warden,&hook,DMG_HOOKSHOT);
            Check(!Arsenal::GetStatus(warden).marked && !Arsenal::GetStatus(warden).tethered,"Protected warden cannot receive arsenal setup debuffs");
            auto& run=gARPGSave.expedition;
            for(int oath=0;oath<5;++oath) {
                run.mutator=oath;
                uint32_t weak[]={DMG_HAMMER_SWING,DMG_ARROW_NORMAL,DMG_ARROW_ICE,DMG_ARROW_FIRE,DMG_HAMMER_SWING};
                Check(Expeditions::DamageMultiplier(guard,weak[oath])>Expeditions::DamageMultiplier(guard,DMG_SLASH_KOKIRI),"Oath "+std::to_string(oath)+" rewards its advertised weapon counter");
            }
            Check(Expeditions::DamageMultiplier(guard,DMG_ARROW_LIGHT)<Expeditions::DamageMultiplier(guard,DMG_SLASH_KOKIRI),"Storm oath resists light attacks");
            run.sealMask=7;Check(Hit(warden,&GET_PLAYER(gPlayState)->actor,DMG_HAMMER_SWING)>0,"Breaking ritual protection restores native hammer damage to the warden");
            Expeditions::End();CVarSetInteger("gEnhancements.ARPG.Enemies",0);
            CVarSetInteger("gEnhancements.ARPG.Affixes",0);CVarSetInteger("gEnhancements.ARPG.Enchanted",0);
            Warp(ENTR_DEKU_TREE_BOSS_ENTRANCE);Next(10);return;
        }
        if(phase==10 && frames>100 && gPlayState && gPlayState->sceneNum==SCENE_DEKU_TREE_BOSS) {
            BossGoma* boss=nullptr;for(auto* a=gPlayState->actorCtx.actorLists[ACTORCAT_BOSS].head;a;a=a->next)if(a->id==ACTOR_BOSS_GOMA)boss=reinterpret_cast<BossGoma*>(a);
            Check(boss!=nullptr,"Gohma loads in her native boss room");boss->actor.colChkInfo.health=30;boss->invincibilityFrames=0;boss->actionFunc=BossGoma_FloorStunned;boss->eyeClosedTimer=1;
            ColliderInfo strike{};strike.toucher.dmgFlags=DMG_HAMMER_SWING;boss->collider.elements[0].info.acHitInfo=&strike;boss->collider.elements[0].info.bumperFlags|=BUMP_HIT;
            BossGoma_UpdateHit(boss,gPlayState);Check(boss->actor.colChkInfo.health==30,"Closed Gohma eye rejects the new hammer attack");
            boss->eyeClosedTimer=0;boss->collider.elements[0].info.bumperFlags|=BUMP_HIT;BossGoma_UpdateHit(boss,gPlayState);
            Check(boss->actor.colChkInfo.health==26,"The native stunned-eye branch accepts hammer damage");
            boss->invincibilityFrames=0;boss->actionFunc=BossGoma_FloorStunned;strike.toucher.dmgFlags=DMG_SLINGSHOT;boss->collider.elements[0].info.bumperFlags|=BUMP_HIT;BossGoma_UpdateHit(boss,gPlayState);
            Check(boss->actor.colChkInfo.health==25,"Slingshot seeds chip the exposed eye through the native boss branch");
            boss->invincibilityFrames=0;boss->actionFunc=BossGoma_FloorIdle;boss->patienceTimer=0;strike.toucher.dmgFlags=DMG_HAMMER_SWING;boss->collider.elements[0].info.bumperFlags|=BUMP_HIT;BossGoma_UpdateHit(boss,gPlayState);
            Check(boss->actor.colChkInfo.health==25,"Unstunned Gohma cannot be damaged by the added weapon path");
            ARPGItemInstance hammer{};hammer.slot=ARPG_SLOT_HAMMER;hammer.baseEquip=1;hammer.itemLevel=35;hammer.modCount=1;hammer.mods[0]={ARPG_MOD_BRUTAL,100};gARPGSave.weapons[1]=hammer;
            Collider source{};source.actor=&GET_PLAYER(gPlayState)->actor;boss->collider.elements[0].info.acHit=&source;
            boss->invincibilityFrames=0;boss->actionFunc=BossGoma_FloorStunned;boss->eyeClosedTimer=0;boss->collider.elements[0].info.bumperFlags|=BUMP_HIT;
            BossGoma_UpdateHit(boss,gPlayState);Check(boss->actor.colChkInfo.health==17,"A rolled hammer amplifies damage inside Gohma's actual vulnerable phase");
            gARPGSave.weapons[1]={};boss->collider.elements[0].info.acHit=nullptr;
            boss->collider.elements[0].info.acHitInfo=nullptr;boss->collider.elements[0].info.bumperFlags&=~BUMP_HIT;
            Warp(ENTR_DODONGOS_CAVERN_BOSS_ENTRANCE);Next(11);return;
        }
        if(phase==11 && frames>100 && gPlayState && gPlayState->sceneNum==SCENE_DODONGOS_CAVERN_BOSS) {
            BossDodongo* boss=nullptr;for(auto* a=gPlayState->actorCtx.actorLists[ACTORCAT_BOSS].head;a;a=a->next)if(a->id==ACTOR_BOSS_DODONGO)boss=reinterpret_cast<BossDodongo*>(a);
            Check(boss!=nullptr,"King Dodongo loads in his native boss room");boss->health=30;boss->unk_1C0=0;boss->actionFunc=BossDodongo_Walk;
            ColliderInfo strike{};strike.toucher.dmgFlags=DMG_HAMMER_SWING;boss->collider.elements[0].info.acHitInfo=&strike;boss->collider.elements[0].info.bumperFlags|=BUMP_HIT;
            BossDodongo_UpdateDamage(boss,gPlayState);Check(boss->health==30,"Armored King Dodongo walking phase rejects hammer damage");
            boss->actionFunc=BossDodongo_Vulnerable;boss->collider.elements[0].info.bumperFlags|=BUMP_HIT;BossDodongo_UpdateDamage(boss,gPlayState);
            Check(boss->health==26,"The bomb-created vulnerable phase accepts hammer punishment");
            boss->unk_1C0=0;boss->actionFunc=BossDodongo_Vulnerable;CVarSetInteger("gEnhancements.ARPG.Items",0);boss->collider.elements[0].info.bumperFlags|=BUMP_HIT;BossDodongo_UpdateDamage(boss,gPlayState);
            Check(boss->health==24,"Disabling item extensions restores the boss's original two-damage hammer hit");CVarSetInteger("gEnhancements.ARPG.Items",1);
            ARPGItemInstance hammer{};hammer.slot=ARPG_SLOT_HAMMER;hammer.baseEquip=1;hammer.itemLevel=35;hammer.modCount=1;hammer.mods[0]={ARPG_MOD_BRUTAL,100};gARPGSave.weapons[1]=hammer;
            Collider source{};source.actor=&GET_PLAYER(gPlayState)->actor;boss->collider.elements[0].info.acHit=&source;
            boss->unk_1C0=0;boss->actionFunc=BossDodongo_Vulnerable;boss->collider.elements[0].info.bumperFlags|=BUMP_HIT;
            BossDodongo_UpdateDamage(boss,gPlayState);Check(boss->health==16,"A rolled hammer amplifies damage inside King Dodongo's actual vulnerable phase");
            gARPGSave.weapons[1]={};boss->collider.elements[0].info.acHit=nullptr;
            boss->collider.elements[0].info.acHitInfo=nullptr;boss->collider.elements[0].info.bumperFlags&=~BUMP_HIT;Result("passed");return;
        }
        if(frames>1200)throw std::runtime_error("Timed out at arsenal phase "+std::to_string(phase)+" run="+std::to_string(gARPGSave.expedition.phase)+" control="+std::to_string(GameInteractor::IsPlayerInControl())+" ground="+std::to_string(GET_PLAYER(gPlayState)->actor.bgCheckFlags)+" shrine="+std::to_string(Expeditions::GetWaystoneView().visible));
    }catch(const std::exception& e){Result("failed",e.what());}
}
void ProbeFault(void* ref) {
    auto* source=static_cast<Actor*>(ref);if(phase!=2 || !Arsenal::IsFault(source))return;
    try {
        auto& ctx=gPlayState->colChkCtx;
        for(int i=0;i<ctx.colATCount;++i) {
            if(ctx.colAT[i]->actor!=source || ctx.colAT[i]->shape!=COLSHAPE_CYLINDER)continue;
            auto* c=reinterpret_cast<ColliderCylinder*>(ctx.colAT[i]);
            static std::array<bool,3> tested{};int node=c->dim.pos.z<fixture.z+90?0:c->dim.pos.z<fixture.z+150?1:2;if(tested[node])continue;
            Actor probe{};probe.id=ACTOR_EN_TEST;probe.category=ACTORCAT_ENEMY;probe.update=Noop;probe.colChkInfo.health=200;
            probe.world.pos={float(c->dim.pos.x),float(c->dim.pos.y),float(c->dim.pos.z)};
            ColliderCylinder receiver{};ColliderCylinderInit init={{COLTYPE_HIT0,AT_NONE,AC_ON|AC_TYPE_PLAYER,OC1_NONE,OC2_TYPE_1,COLSHAPE_CYLINDER},
                {ELEMTYPE_UNK0,{0,0,0},{0xFFFFFFFF,0,0},TOUCH_NONE,BUMP_ON,OCELEM_NONE},{20,60,0,{0,0,0}}};
            Collider_InitCylinder(gPlayState,&receiver);Collider_SetCylinder(gPlayState,&receiver,&probe,&init);Collider_UpdateCylinder(&probe,&receiver);
            auto saved=ctx;ctx.colATCount=1;ctx.colAT[0]=&c->base;ctx.colACCount=1;ctx.colAC[0]=&receiver.base;
            CollisionCheck_AT(gPlayState,&ctx);CollisionCheck_Damage(gPlayState,&ctx);
            Check((receiver.base.acFlags&AC_HIT) && probe.colChkInfo.damage>0,"Fault node "+std::to_string(node)+" intersects the native narrow phase and damage pipeline");
            Check(!Arsenal::Collision(gPlayState,&c->base,&receiver.base),"Fault rejects a repeated limb/volume hit on its victim");
            ObjectExtension::GetInstance().Free(&probe);probe.category=ACTORCAT_NPC;
            Check(!Arsenal::Collision(gPlayState,&c->base,&receiver.base),"Fault refuses friendly NPC categories");probe.category=ACTORCAT_ENEMY;
            receiver.base.actor=foe;Check(!Arsenal::Collision(gPlayState,&c->base,&receiver.base),"Fault excludes the original hammer victim");
            ctx=saved;c->base.at=nullptr;c->info.atHit=nullptr;c->info.atHitInfo=nullptr;c->base.atFlags&=~AT_HIT;c->info.toucherFlags&=~TOUCH_HIT;
            Collider_DestroyCylinder(gPlayState,&receiver);ObjectExtension::GetInstance().Free(&probe);tested[node]=true;++faultChecks;
        }
    }catch(const std::exception& e){Result("failed",e.what());}
}
void Register(){const char* enabled=std::getenv("SOH_ARPG_ARSENAL_QA"),*home=std::getenv("SHIP_HOME");if(!enabled || std::string(enabled)!="1" || !home || !std::filesystem::path(home).filename().string().starts_with("soh-arpg-qa-"))return;
    visualOnly=std::getenv("SOH_ARPG_ARSENAL_UI") && std::string(std::getenv("SOH_ARPG_ARSENAL_UI"))=="1";
    output=std::filesystem::path(home)/"arsenal-results.json";GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>(Tick);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnActorUpdate>(ProbeFault);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayDrawEnd>([](){if(phase==2)++renderFrames;});
}
static RegisterShipInitFunc init(Register);
}
