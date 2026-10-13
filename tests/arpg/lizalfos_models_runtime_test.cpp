#include "soh/Enhancements/arpg/ARPG.h"
#include "soh/Enhancements/arpg/LizalfosGeometry.h"
#include "soh/Enhancements/arpg/EnemyVisuals.h"
#include "soh/Enhancements/arpg/AffixSystem.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ShipInit.hpp"
#include "soh/cvar_prefixes.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <filesystem>
#include <fstream>
#include <set>
#include <cstdlib>
extern "C" {
#include "functions.h"
#include "variables.h"
#include "macros.h"
#include "overlays/actors/ovl_En_Zf/z_en_zf.h"
void Sram_InitDebugSave(void);
void func_80B45384(EnZf*);
void EnZf_SetupSlash(EnZf*);
void EnZf_SetupJumpForward(EnZf*);
void EnZf_SetupJumpBack(EnZf*);
void EnZf_UpdateDamage(EnZf*,PlayState*);
s32 EnZf_OverrideLimbDraw(PlayState*,s32,Gfx**,Vec3f*,Vec3s*,void*,Gfx**);
extern PlayState* gPlayState;
extern GameState* gGameState;
}
namespace {
int phase=0,frames=0,kills=0;bool slashed=false,trail=false,swordMoved=false,feetMoved=false,jumped=false,backJumped=false,fadeIn=false,fadeOut=false;
std::array<EnZf*,6> enemies{};std::set<Gfx*> replacements;
Vec3f fixture{},firstSword{},firstFoot{};nlohmann::json checks=nlohmann::json::array();std::filesystem::path output;
void Check(bool ok,const char* label) { if(!ok) { throw std::runtime_error(label); }checks.push_back(label);SPDLOG_INFO("[Lizalfos model QA] PASS: {}",label); }
void Result(const char* status,const std::string& error="") { std::ofstream(output)<<nlohmann::json{{"status",status},{"checks",checks},{"error",error}}.dump(2);phase=99; }
bool Moved(Vec3f a,Vec3f b) { return std::abs(a.x-b.x)+std::abs(a.y-b.y)+std::abs(a.z-b.z)>.1f; }
Gfx* Replacement(EnZf* e,int limb) {
    auto* native=static_cast<StandardLimb*>(e->skelAnime.skeleton[limb-1])->dList;Gfx* dl=native;
    ARPG_OverrideEnemyLimb(gPlayState,&e->actor,limb,&dl);return dl;
}
bool Contains(Gfx* list,Gfx wanted) {
    for(size_t i=0;i<100000;++i) {
        if(list[i].words.w0==wanted.words.w0 && list[i].words.w1==wanted.words.w1) { return true; }
        if((list[i].words.w0>>24)==G_ENDDL) { return false; }
    }
    throw std::runtime_error("Unterminated cached display list");
}
void Damage(EnZf* e,int damage,int effect) {
    func_80B45384(e);e->bodyCollider.base.acFlags|=AC_HIT;e->actor.colChkInfo.damage=damage;e->actor.colChkInfo.damageEffect=effect;
    EnZf_UpdateDamage(e,gPlayState);
}
void Tick() {
    if(phase==99) { return; }
    try {
        ++frames;
        if(phase==0 && frames>60 && gGameState) {
            CVarSetInteger(CVAR_DEVELOPER_TOOLS("DebugSaveFileMode"),2);
            gSaveContext.gameMode=GAMEMODE_NORMAL;gSaveContext.fileNum=0xFE;Sram_InitDebugSave();
            gSaveContext.gameMode=GAMEMODE_NORMAL;gSaveContext.fileNum=0xFF;gSaveContext.linkAge=0;
            gSaveContext.cutsceneIndex=0;gSaveContext.sceneLayer=0;gSaveContext.entranceIndex=ENTR_HYRULE_FIELD_PAST_BRIDGE_SPAWN;
            gSaveContext.respawnFlag=0;gSaveContext.dayTime=gSaveContext.skyboxTime=0x8000;
            gGameState->running=false;SET_NEXT_GAMESTATE(gGameState,Play_Init,PlayState);
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.Enabled"),1);CVarSetInteger(CVAR_ENHANCEMENT("ARPG.Affixes"),1);
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.Items"),1);CVarSetInteger(CVAR_ENHANCEMENT("ARPG.Enemies"),1);
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.RemasteredEnemies"),1);CVarSetInteger(CVAR_ENHANCEMENT("ARPG.Packs"),0);
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.Tactics"),0);CVarSetInteger(CVAR_ENHANCEMENT("ARPG.Enchanted"),0);
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.InventoryOnPause"),1);CVarSetInteger(CVAR_WINDOW("ARPG"),0);
            ARPG::SeedRng(0x21FA);phase=1;frames=0;return;
        }
        if(phase>=2 && gPlayState && GameInteractor::IsPlayerInControl()) {
            auto* p=GET_PLAYER(gPlayState);gSaveContext.health=gSaveContext.healthCapacity;p->invincibilityTimer=20;
            if(!std::getenv("SOH_ARPG_LIZALFOS_MODELS_UI")) { p->actor.world.pos=fixture;p->actor.prevPos=fixture;p->actor.velocity={};p->actor.speedXZ=0; }
        }
        if(phase==1 && frames>100 && GameInteractor::IsPlayerInControl()) {
            fixture=GET_PLAYER(gPlayState)->actor.world.pos;
            for(int variant=0;variant<2;++variant) { for(int rank=0;rank<3;++rank) {
                auto* a=Actor_Spawn(&gPlayState->actorCtx,gPlayState,ACTOR_EN_ZF,fixture.x+(rank-1)*85,fixture.y,fixture.z+100+variant*140,
                                    0,0x8000,0,variant?ENZF_TYPE_DINOLFOS:ENZF_TYPE_LIZALFOS_LONE);
                Check(a && a->update,"Native reptile initializes for species and rank replacement");
                auto* e=enemies[variant*3+rank]=reinterpret_cast<EnZf*>(a);AffixSystem::Configure(a,rank);a->flags|=ACTOR_FLAG_DRAW_CULLING_DISABLED;
                const auto alpha=e->alpha;
                for(int limb:ARPG::LizalfosGeometry::Limbs) {
                    auto* native=static_cast<StandardLimb*>(e->skelAnime.skeleton[limb-1])->dList;
                    for(int sheath=0;sheath<(limb==ENZF_LIMB_SWORD||limb==ENZF_LIMB_SCABBARD?2:1);++sheath) { for(int fade=0;fade<2;++fade) {
                        e->swordSheathed=sheath;e->alpha=fade?100:255;Gfx* dl=Replacement(e,limb);
                        if(!native || !dl || native==dl) { throw std::runtime_error("Missing native replacement at limb "+std::to_string(limb)); }
                        replacements.insert(dl);if(Replacement(e,limb)!=dl) { throw std::runtime_error("Unstable reptile display list"); }
                    } }
                }
                e->alpha=alpha;e->swordSheathed=0;
            } }
            Check(replacements.size()==264,"Both species and three ranks cache all limbs, weapon states and two alpha pipelines");
            auto* e=enemies[0];auto originalAlpha=e->alpha;e->alpha=255;auto* opaque=Replacement(e,ENZF_LIMB_HEAD);
            e->alpha=100;auto* faded=Replacement(e,ENZF_LIMB_HEAD);e->alpha=35;
            Check(faded!=opaque && faded==Replacement(e,ENZF_LIMB_HEAD),"Fade shares one immutable mesh for every partial alpha");
            Gfx mode{},combine{};gDPSetRenderMode(&mode,G_RM_FOG_SHADE_A,G_RM_AA_ZB_XLU_SURF2);
            gDPSetCombineLERP(&combine,SHADE,0,PRIMITIVE,0,0,0,0,ENVIRONMENT,0,0,0,COMBINED,0,0,0,COMBINED);
            Check(Contains(faded,mode) && Contains(faded,combine),"Fade display lists blend the native environment alpha instead of forcing opacity");e->alpha=originalAlpha;
            auto* native=static_cast<StandardLimb*>(e->skelAnime.skeleton[ENZF_LIMB_HEAD-1])->dList;Gfx* dl=native;
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.RemasteredEnemies"),0);ARPG_OverrideEnemyLimb(gPlayState,&e->actor,ENZF_LIMB_HEAD,&dl);
            Check(dl==native,"Model setting restores the original reptile model");
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.RemasteredEnemies"),1);CVarSetInteger(CVAR_ENHANCEMENT("ARPG.Enabled"),0);
            ARPG_OverrideEnemyLimb(gPlayState,&e->actor,ENZF_LIMB_HEAD,&dl);Check(dl==native,"Master ARPG gate preserves vanilla reptile rendering");
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.Enabled"),1);Vec3f pos{};Vec3s rot{};e->headRot=1200;dl=nullptr;
            EnZf_OverrideLimbDraw(gPlayState,ENZF_LIMB_HEAD_ROOT,&dl,&pos,&rot,e,nullptr);
            Check(rot.y==-1200 && !dl,"Head tracking remains on the empty native head root");e->headRot=0;
            for(int limb:{ENZF_LIMB_SWORD,ENZF_LIMB_SCABBARD}) {
                e->swordSheathed=0;auto* drawn=Replacement(e,limb);e->swordSheathed=1;auto* stored=Replacement(e,limb);
                dl=static_cast<StandardLimb*>(e->skelAnime.skeleton[limb-1])->dList;
                EnZf_OverrideLimbDraw(gPlayState,limb,&dl,&pos,&rot,e,nullptr);
                Check(drawn!=stored && dl==stored,"Native sheathing override selects the new empty-hand or stored-sword model");
            }
            e->swordSheathed=0;phase=2;frames=0;
            if(const char* ui=std::getenv("SOH_ARPG_LIZALFOS_MODELS_UI")) {
                for(int i=0;i<6;++i) {
                    if(i!=2 && i!=5) { Actor_Kill(&enemies[i]->actor);continue; }
                    auto& a=enemies[i]->actor;a.world.pos={fixture.x+(i==2?-85.f:85.f),fixture.y,fixture.z-80};a.home.pos=a.world.pos;
                    enemies[i]->unk_3F0=0;
                }
                std::ofstream(output)<<nlohmann::json{{"status","visual_review"},{"mode",ui},{"checks",checks}}.dump(2);
            }
            return;
        }
        if(phase==2) {
            if(const char* ui=std::getenv("SOH_ARPG_LIZALFOS_MODELS_UI")) {
                if(std::string(ui)=="fade" && enemies[5]) { enemies[5]->alpha=enemies[5]->actor.shape.shadowAlpha=115; }
                return;
            }
            fadeIn|=enemies[0]->alpha>0 && enemies[0]->alpha<255;
            if(frames==100) {
                firstSword=enemies[0]->swordCollider.dim.quad[0];firstFoot=enemies[0]->leftFootPos;
                EnZf_SetupSlash(enemies[0]);EnZf_SetupSlash(enemies[3]);EnZf_SetupJumpForward(enemies[1]);EnZf_SetupJumpBack(enemies[4]);
            }
            if(frames>100) {
                slashed|=enemies[0]->action==ENZF_ACTION_SLASH && enemies[0]->skelAnime.curFrame>=14 && enemies[0]->skelAnime.curFrame<=20;
                swordMoved|=Moved(firstSword,enemies[0]->swordCollider.dim.quad[0]);feetMoved|=Moved(firstFoot,enemies[0]->leftFootPos);
                if(auto* blure=static_cast<EffectBlure*>(Effect_GetByIndex(enemies[0]->blureIndex))) { trail|=blure->numElements>1; }
                jumped|=enemies[1]->action==ENZF_ACTION_JUMP_FORWARD && enemies[1]->actor.world.pos.y>enemies[1]->actor.floorHeight+5;
                backJumped|=enemies[4]->action==ENZF_ACTION_JUMP_BACK && enemies[4]->skelAnime.curFrame>0;
            }
            if(frames>420) {
                Check(fadeIn,"Native lone-Lizalfos entrance fades in through the translucent model pipeline");
                Check(slashed && swordMoved,"Native slash timing and sword collision quad follow the replacement weapon");
                Check(trail,"Native sword trails continue receiving animated blade positions");Check(feetMoved,"Native feet and landing attachments follow the new feet");
                Check(jumped && backJumped,"Native forward and backward jumps animate both reptile species");
                Damage(enemies[0],1,0);Check(enemies[0]->action==ENZF_ACTION_DAMAGED && enemies[0]->actor.colorFilterTimer>0,"Damage enters native recoil and color flash");
                Damage(enemies[3],0,1);Check(enemies[3]->action==ENZF_ACTION_STUNNED,"Dinolfos retains its native stun response");
                Damage(enemies[1],0,15);Check(enemies[1]->iceTimer==36,"Ice starts the native nine-point freezing effect");phase=3;frames=0;
            }
            return;
        }
        if(phase==3 && frames>4) {
            bool attached=true;for(const auto& v:enemies[1]->bodyPartsPos) { attached&=v.x!=0 || v.y!=0 || v.z!=0; }
            Check(attached,"All nine ice attachment positions are initialized on the native rig");Damage(enemies[5],255,0);phase=4;frames=0;return;
        }
        if(phase==4) {
            if(enemies[5]) { fadeOut|=enemies[5]->alpha>0 && enemies[5]->alpha<255; }
            if(frames>180) {
                Check(kills==1,"Fatal hit emits one native defeat event");Check(fadeOut && !enemies[5],"Native death fades the new model and destroys the actor");
                gPlayState->nextEntranceIndex=ENTR_LON_LON_RANCH_ENTRANCE;gPlayState->transitionTrigger=TRANS_TRIGGER_START;gPlayState->transitionType=TRANS_TYPE_INSTANT;
                phase=5;frames=0;
            }
            return;
        }
        if(phase==5 && frames>100 && GameInteractor::IsPlayerInControl()) {
            Check(std::all_of(enemies.begin(),enemies.end(),[](auto* e){return e==nullptr;}),"Scene teardown releases all surviving reptiles");Result("passed");return;
        }
        if(frames>1800) { throw std::runtime_error("Timed out in phase "+std::to_string(phase)); }
    } catch(const std::exception& e) { Result("failed",e.what());SPDLOG_ERROR("[Lizalfos model QA] {}",e.what()); }
}
void Register() {
    const char* enabled=std::getenv("SOH_ARPG_LIZALFOS_MODELS_QA"),*home=std::getenv("SHIP_HOME");
    if(!enabled || std::string(enabled)!="1" || !home || !std::filesystem::path(home).filename().string().starts_with("soh-arpg-qa-")) { return; }
    output=std::filesystem::path(home)/"lizalfos-model-results.json";
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>(Tick);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnActorDestroy>([](void* ref) { for(auto*& e:enemies) { if(e==ref) { e=nullptr; } } });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnEnemyDefeat>([](void* ref) { for(auto* e:enemies) { if(e==ref) { ++kills; } } });
}
static RegisterShipInitFunc init(Register);
}
