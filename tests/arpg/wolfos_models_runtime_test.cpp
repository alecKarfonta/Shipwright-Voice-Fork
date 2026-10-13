#include "soh/Enhancements/arpg/ARPG.h"
#include "soh/Enhancements/arpg/WolfosGeometry.h"
#include "soh/Enhancements/arpg/EnemyVisuals.h"
#include "soh/Enhancements/arpg/WolfosTexture.h"
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
#include "overlays/actors/ovl_En_Wf/z_en_wf.h"
#include "objects/object_wf/object_wf.h"
void Sram_InitDebugSave(void);
void EnWf_SetupWait(EnWf*);
void EnWf_SetupSlash(EnWf*);
void EnWf_SetupBlocking(EnWf*);
void EnWf_SetupBackflipAway(EnWf*);
void EnWf_UpdateDamage(EnWf*,PlayState*);
s32 EnWf_OverrideLimbDraw(PlayState*,s32,Gfx**,Vec3f*,Vec3s*,void*);
extern PlayState* gPlayState;
extern GameState* gGameState;
}
namespace {
int phase=0,frames=0,kills=0;bool attacked=false,clawsMoved=false,tailMoved=false,blinked=false,backflipped=false;
std::array<EnWf*,6> enemies{};std::set<Gfx*> replacements;
Vec3f fixture{};Vec3s firstClaw{},firstTail{};
void HoldReviewPose(Actor* actor,PlayState* play) {
    auto* wolf=reinterpret_cast<EnWf*>(actor);
    wolf->skelAnime.curFrame=0;wolf->skelAnime.playSpeed=0;SkelAnime_Update(&wolf->skelAnime);
    // [TIME] | [INFO] | Wolfos.Review | Keep the body still but expose every eye state in the visual fixture.
    const int blink=play->gameplayFrames%100;
    wolf->eyeIndex=blink<88?0:1+(blink-88)/4;wolf->unk_4D4.y=0;
    if(std::getenv("SOH_ARPG_WOLFOS_EYES_UI"))wolf->eyeIndex=(play->gameplayFrames/80)%4;
}
nlohmann::json checks=nlohmann::json::array();std::filesystem::path output;
void Check(bool ok,const char* label) {
    if(!ok) { throw std::runtime_error(label); }
    checks.push_back(label);SPDLOG_INFO("[Wolfos model QA] PASS: {}",label);
}
void Result(const char* status,const std::string& error="") {
    std::ofstream(output)<<nlohmann::json{{"status",status},{"checks",checks},{"error",error}}.dump(2);phase=99;
}
bool Moved(Vec3s a,Vec3s b) { return a.x!=b.x || a.y!=b.y || a.z!=b.z; }
void Damage(EnWf* e,bool tail,int damage,int effect) {
    EnWf_SetupWait(e);e->colliderSpheres.base.acFlags&=~AC_BOUNCED;
    e->colliderCylinderBody.base.acFlags&=~AC_HIT;e->colliderCylinderTail.base.acFlags&=~AC_HIT;
    (tail?e->colliderCylinderTail:e->colliderCylinderBody).base.acFlags|=AC_HIT;
    e->actor.colChkInfo.damage=damage;e->actor.colChkInfo.damageEffect=effect;
    e->actor.shape.rot.y=e->actor.yawTowardsPlayer;EnWf_UpdateDamage(e,gPlayState);
}
void Tick() {
    if(phase==99) { return; }
    try {
        ++frames;
        if(phase==2 && std::getenv("SOH_ARPG_WOLFOS_COMPARE"))gSaveContext.dayTime=gSaveContext.skyboxTime=0x8000;
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
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.Creatures"),0);CVarSetInteger(CVAR_ENHANCEMENT("ARPG.RegionalWorld"),0);
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.InventoryOnPause"),1);CVarSetInteger(CVAR_WINDOW("ARPG"),0);
            ARPG::SeedRng(0xF01F);phase=1;frames=0;return;
        }
        if(phase>=2 && gPlayState && GameInteractor::IsPlayerInControl()) {
            auto* p=GET_PLAYER(gPlayState);gSaveContext.health=gSaveContext.healthCapacity;p->invincibilityTimer=20;
            if(!std::getenv("SOH_ARPG_WOLFOS_MODELS_UI")) { p->actor.world.pos=fixture;p->actor.prevPos=fixture;p->actor.velocity={};p->actor.speedXZ=0; }
        }
        if(phase==1 && frames>100 && GameInteractor::IsPlayerInControl()) {
            fixture=GET_PLAYER(gPlayState)->actor.world.pos;
            if(std::getenv("SOH_ARPG_WOLFOS_MASTER_QA"))Ship::Context::GetRawInstance()->GetResourceManager()->SetAltAssetsEnabled(false);
            for(int variant=0;variant<2;++variant) { for(int rank=0;rank<3;++rank) {
                auto* a=Actor_Spawn(&gPlayState->actorCtx,gPlayState,ACTOR_EN_WF,fixture.x+(rank-1)*140,fixture.y,fixture.z+150+variant*170,
                                    0,0x8000,0,int16_t(0xFF00|variant));
                Check(a && a->update,"Native Wolfos initializes for species and rank replacement");
                auto* e=enemies[variant*3+rank]=reinterpret_cast<EnWf*>(a);AffixSystem::Configure(a,rank);
                a->flags|=ACTOR_FLAG_DRAW_CULLING_DISABLED;
                for(int limb:ARPG::WolfosGeometry::Limbs) {
                    auto* native=static_cast<StandardLimb*>(e->skelAnime.skeleton[limb-1])->dList;
                    for(int blink=0;blink<(limb==WOLFOS_LIMB_EYES?4:1);++blink) {
                        e->eyeIndex=blink;Gfx* dl=native;ARPG_OverrideEnemyLimb(gPlayState,a,limb,&dl);
                        if(!native || !dl || native==dl) { throw std::runtime_error("Missing native replacement at limb "+std::to_string(limb)); }
                        replacements.insert(dl);Gfx* again=native;ARPG_OverrideEnemyLimb(gPlayState,a,limb,&again);
                        if(dl!=again) { throw std::runtime_error("Unstable cached skinned model"); }
                    }
                }
                e->eyeIndex=0;
            } }
            Check(replacements.size()==126,"Both species and all ranks have 19 stable limb meshes and three blink shapes");
            auto texture=std::dynamic_pointer_cast<Fast::Texture>(Ship::Context::GetRawInstance()->GetResourceManager()->GetCachedResource(ARPG::WolfosTexture::Path+7,true));
            Check(texture && texture->Width==512 && texture->Height==512 && texture->ImageDataSize==512*512*4,
                  "Authored 512px fur texture is loaded into the renderer resource cache");
            Check(enemies[0]->skelAnime.dListCount==19 && enemies[3]->skelAnime.dListCount==19,"Both native skeletons use the expected 19-matrix flex palette");
            auto* e=enemies[0];auto* native=static_cast<StandardLimb*>(e->skelAnime.skeleton[WOLFOS_LIMB_HEAD-1])->dList;Gfx* dl=native;
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.RemasteredEnemies"),0);ARPG_OverrideEnemyLimb(gPlayState,&e->actor,WOLFOS_LIMB_HEAD,&dl);
            Check(dl==native,"Model setting restores the original Wolfos head");
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.RemasteredEnemies"),1);CVarSetInteger(CVAR_ENHANCEMENT("ARPG.Enabled"),0);
            ARPG_OverrideEnemyLimb(gPlayState,&e->actor,WOLFOS_LIMB_HEAD,&dl);Check(dl==native,"Master ARPG gate preserves vanilla Wolfos");
            CVarSetInteger(CVAR_ENHANCEMENT("ARPG.Enabled"),1);Vec3f pos{};Vec3s rot{};e->unk_4D4.y=1200;
            EnWf_OverrideLimbDraw(gPlayState,WOLFOS_LIMB_HEAD,&dl,&pos,&rot,e);
            Check(rot.y==-1200 && replacements.contains(dl),"Head tracking is applied before selecting the new model");e->unk_4D4.y=0;
            dl=nullptr;ARPG_OverrideEnemyLimb(gPlayState,&e->actor,WOLFOS_LIMB_HEAD_ROOT,&dl);
            Check(!dl,"Empty flex nodes preserve the matrix-palette layout");
            if(std::getenv("SOH_ARPG_WOLFOS_MASTER_QA")) {
                auto manager=Ship::Context::GetRawInstance()->GetResourceManager();
                manager->SetAltAssetsEnabled(true);
                Check(manager->IsAltAssetsEnabled(),"Native master asset-pack overrides are enabled");
                dl=native;ARPG_OverrideEnemyLimb(gPlayState,&e->actor,WOLFOS_LIMB_HEAD,&dl);
                Check(dl==native,"Installed master takes priority while ARPG enemy remasters stay enabled");
                manager->SetAltAssetsEnabled(false);dl=native;ARPG_OverrideEnemyLimb(gPlayState,&e->actor,WOLFOS_LIMB_HEAD,&dl);
                Check(dl!=native,"Disabling alternate assets restores the existing procedural fallback");
                manager->SetAltAssetsEnabled(true);
                bool limbsLoaded=true;
                for(int limb:ARPG::WolfosGeometry::Limbs) {
                    auto* asset=static_cast<StandardLimb*>(e->skelAnime.skeleton[limb-1])->dList;
                    std::string path=reinterpret_cast<const char*>(asset);
                    if(path.starts_with("__OTR__"))path.erase(0,7);
                    auto resource=manager->LoadResourceProcess(path);
                    limbsLoaded&=resource && resource->GetInitData()->Path.starts_with("alt/objects/object_wf/") && resource->GetPointerSize()>0;
                }
                Check(limbsLoaded,"All 19 Brown Wolfos limbs load from the exported native asset pack");
                bool mapsLoaded=true;
                for(const char* suffix:{"FurTex","ToothClawTex","MouthTex","TongueTex","NostrilTex","EyeOpenTex","EyeHalfTex","EyeNarrowTex"}) {
                    auto map=std::dynamic_pointer_cast<Fast::Texture>(manager->LoadResourceProcess(std::string("objects/object_wf/gWolfosNormal")+suffix));
                    mapsLoaded&=map && map->GetInitData()->Path.starts_with("alt/") && map->Type==Fast::TextureType::RGBA32bpp && (map->Flags&TEX_FLAG_LOAD_AS_IMG);
                }
                Check(mapsLoaded,"All eight independently paintable master textures load as native high-resolution resources");
            }
            phase=2;frames=0;
            if(std::getenv("SOH_ARPG_WOLFOS_MODELS_UI")) {
                const bool compare=std::getenv("SOH_ARPG_WOLFOS_COMPARE")!=nullptr;
                for(int i=0;i<6;++i) {
                    if(i!=2 && i!=5) { Actor_Kill(&enemies[i]->actor);continue; }
                    auto& a=enemies[i]->actor;a.world.pos={fixture.x+(i==2?-85.f:85.f),fixture.y,fixture.z-80};a.home.pos=a.world.pos;
                    if(compare) {
                        a.shape.rot.y=0;a.world.rot.y=0;a.shape.yOffset=0;a.scale={.0075f,.0075f,.0075f};
                        EnWf_SetupWait(enemies[i]);HoldReviewPose(&a,gPlayState);a.update=HoldReviewPose;
                    }
                }
                if(compare) {
                    CVarSetInteger(CVAR_ENHANCEMENT("ARPG.RemasteredEnemies"),std::string(std::getenv("SOH_ARPG_WOLFOS_COMPARE"))=="native"?0:1);
                    int camera=Play_CreateSubCamera(gPlayState);Play_ChangeCameraStatus(gPlayState,0,CAM_STAT_WAIT);Play_ChangeCameraStatus(gPlayState,camera,CAM_STAT_ACTIVE);
                    Vec3f at{fixture.x,fixture.y+36,fixture.z-80},eye{fixture.x+110,fixture.y+85,fixture.z+155};
                    if(std::getenv("SOH_ARPG_WOLFOS_EYES_UI")) {
                        at={fixture.x-85,fixture.y+30,fixture.z-54};
                        eye={fixture.x-57,fixture.y+42,fixture.z+14};
                    }
                    Play_CameraSetAtEye(gPlayState,camera,&at,&eye);Play_CameraSetFov(gPlayState,camera,55);
                    GET_PLAYER(gPlayState)->actor.draw=nullptr;
                }
                std::ofstream(output)<<nlohmann::json{{"status","visual_review"},{"checks",checks}}.dump(2);
            }
            return;
        }
        if(phase==2) {
            if(std::getenv("SOH_ARPG_WOLFOS_MODELS_UI")) { return; }
            if(frames==100) {
                firstClaw=enemies[0]->colliderSpheres.elements[0].dim.worldSphere.center;
                firstTail=enemies[0]->colliderCylinderTail.dim.pos;
                EnWf_SetupSlash(enemies[0]);EnWf_SetupBlocking(enemies[1]);EnWf_SetupBackflipAway(enemies[2]);
                Check(enemies[1]->action==WOLFOS_ACTION_BLOCKING,"Native blocking pose accepts the remastered model");
            }
            if(frames>100) {
                attacked|=enemies[0]->slashStatus>0;blinked|=enemies[0]->eyeIndex>0;
                clawsMoved|=Moved(firstClaw,enemies[0]->colliderSpheres.elements[0].dim.worldSphere.center);
                tailMoved|=Moved(firstTail,enemies[0]->colliderCylinderTail.dim.pos);
                backflipped|=enemies[2]->action==WOLFOS_ACTION_BACKFLIP_AWAY && enemies[2]->skelAnime.curFrame>0;
            }
            if(frames>450) {
                Check(attacked,"Native slash activates its claw attack collider");Check(clawsMoved,"Claw collider follows animated replacement limbs");
                Check(tailMoved,"Tail weak-point collider follows the native tail matrix");Check(blinked,"Native blink animation continues");
                Check(backflipped,"Native backflip animation advances with the new skin");
                auto* e=enemies[0];int before=e->actor.colChkInfo.health;Damage(e,true,1,0);
                Check(e->actor.colChkInfo.health==before-4,"Tail strikes retain their native quadruple damage");
                Check(e->action==WOLFOS_ACTION_DAMAGED && e->actor.colorFilterTimer>0,"Tail hit enters native damage recoil and color flash");
                Damage(enemies[3],false,0,1);Check(enemies[3]->action==WOLFOS_ACTION_STUNNED,"White Wolfos retains its native stun response");
                Damage(enemies[1],false,0,14);Check(enemies[1]->fireTimer==40,"Fire hit starts native burning attachments");
                phase=3;frames=0;
            }
            return;
        }
        if(phase==3 && frames>4) {
            bool attached=true;for(const auto& v:enemies[1]->bodyPartsPos) { attached&=v.x!=0 || v.y!=0 || v.z!=0; }
            Check(attached,"All ten fire particles have initialized native joint attachments, including the left shin");
            Damage(enemies[5],false,255,0);phase=4;frames=0;return;
        }
        if(phase==4 && frames>130) {
            Check(kills==1,"Fatal hit emits one native defeat event");Check(!enemies[5],"Native death destroys the remastered actor safely");
            gPlayState->nextEntranceIndex=ENTR_LON_LON_RANCH_ENTRANCE;gPlayState->transitionTrigger=TRANS_TRIGGER_START;gPlayState->transitionType=TRANS_TYPE_INSTANT;
            phase=5;frames=0;return;
        }
        if(phase==5 && frames>100 && GameInteractor::IsPlayerInControl()) {
            Check(std::all_of(enemies.begin(),enemies.end(),[](auto* e){return e==nullptr;}),"Scene teardown releases all surviving Wolfos");Result("passed");return;
        }
        if(frames>1800) { throw std::runtime_error("Timed out in phase "+std::to_string(phase)); }
    } catch(const std::exception& e) { Result("failed",e.what());SPDLOG_ERROR("[Wolfos model QA] {}",e.what()); }
}
void Register() {
    const char* enabled=std::getenv("SOH_ARPG_WOLFOS_MODELS_QA"),*home=std::getenv("SHIP_HOME");
    if(!enabled || std::string(enabled)!="1" || !home || !std::filesystem::path(home).filename().string().starts_with("soh-arpg-qa-")) { return; }
    output=std::filesystem::path(home)/"wolfos-model-results.json";
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>(Tick);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnActorDestroy>([](void* ref) { for(auto*& e:enemies) { if(e==ref) { e=nullptr; } } });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnEnemyDefeat>([](void* ref) { for(auto* e:enemies) { if(e==ref) { ++kills; } } });
}
static RegisterShipInitFunc init(Register);
}
