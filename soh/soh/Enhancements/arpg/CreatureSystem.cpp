#include "CreatureSystem.h"
#include "CreatureVisuals.h"
#include "BroodSystem.h"
#include "EnemySystem.h"
#include "ExpeditionSystem.h"
#include "AffixSystem.h"
#include "ARPGTheme.h"
#include "ARPGTesting.h"
#include "soh/ActorDB.h"
#include "soh/ShipInit.hpp"
#include "soh/ObjectExtension/ObjectExtension.h"
#include "soh/ObjectExtension/ActorMaximumHealth.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/frame_interpolation.h"
#include <cmath>
#include <libultraship/bridge/consolevariablebridge.h>
extern "C" {
#include "functions.h"
#include "overlays/actors/ovl_En_Crow/z_en_crow.h"
#include "variables.h"
#include "macros.h"
extern PlayState* gPlayState;
}
namespace ARPG::Creatures {
namespace {
struct Creature { Actor actor;ColliderCylinder body;State state;int age,invulnerable;int16_t aim;float walk;MendState mend; };
struct MendRecord { uint64_t identity=0, claimant=0; int received=0; };
static ObjectExtension::Register<MendRecord> mendRegistration;
uint64_t nextMendIdentity=1;
struct Projectile { Actor actor;ColliderCylinder hitbox;int age,delay;float verticalSpeed; };
struct Technique { State state;int rally=0;uint32_t serviced=0;int16_t aim=0; };
static ObjectExtension::Register<Technique> techniqueRegistration;
int ids[2]={-1,-1},projectileId=-1;
float Distance(Vec3f a,Vec3f b){return std::hypot(a.x-b.x,a.z-b.z);}
bool Active(PlayState* play) {return play && Enabled() && GameInteractor::IsPlayerInControl() && gSaveContext.health>0 && play->transitionTrigger==TRANS_TRIGGER_OFF;}
bool Line(PlayState* play,Vec3f a,Vec3f b) {
    Vec3f hit{};CollisionPoly* poly=nullptr;int bg=BGCHECK_SCENE;
    return !BgCheck_EntityLineTest1(&play->colCtx,&a,&b,&hit,&poly,true,true,true,true,&bg);
}
bool Visible(PlayState* play,Actor* actor) {
    Vec3f a=actor->world.pos,b=GET_PLAYER(play)->actor.world.pos;a.y+=35;b.y+=35;return std::abs(a.y-b.y)<120 && Line(play,a,b);
}
bool Floor(PlayState* play,Actor* actor,Vec3f& point,float tolerance=45) {
    if(!BgCheck_PosInStaticBoundingBox(&play->colCtx,&point))return false;
    Vec3f ray=point;ray.y+=90;CollisionPoly* poly=nullptr;int bg=BGCHECK_SCENE;
    float floor=BgCheck_EntityRaycastFloor5(play,&play->colCtx,&poly,&bg,actor,&ray);
    if(!poly || bg!=BGCHECK_SCENE || floor<=BGCHECK_Y_MIN || poly->normal.y<24000 || std::abs(floor-point.y)>tolerance)return false;
    auto property=func_80041EA4(&play->colCtx,poly,bg);
    auto floorType=SurfaceType_GetFloorType(&play->colCtx,poly,bg);
    if(property==5 || property==12 || floorType==2 || floorType==3 || floorType==7 || floorType==9 || floorType==12 || SurfaceType_GetSceneExitIndex(&play->colCtx,poly,bg) || SurfaceType_IsWallDamage(&play->colCtx,poly,bg))return false;
    float water=0;WaterBox* box=nullptr;if(WaterBox_GetSurface1(play,&play->colCtx,point.x,point.z,&water,&box) && water>floor+5)return false;
    point.y=floor;return true;
}
Kind Type(const Actor* a){return a->id==ids[0]?Kind::Sentinel:Kind::Wraith;}
MendRecord* MendIdentity(Actor* actor) {
    auto& ext=ObjectExtension::GetInstance();
    if(!ext.Has<MendRecord>(actor))ext.Set<MendRecord>(actor,MendRecord{nextMendIdentity++});
    return ext.Get<MendRecord>(actor);
}
Actor* ResolveMend(const Creature* caster,PlayState* play) {
    if(!play || !caster->mend.target)return nullptr;
    // Resolve an identity in the live actor list, never retain an actor pointer
    // across updates: room unloads and actor-pool address reuse are safe.
    for(auto* a=play->actorCtx.actorLists[ACTORCAT_ENEMY].head;a;a=a->next)
        if(auto* r=ObjectExtension::GetInstance().Get<MendRecord>(a);r && r->identity==caster->mend.target)return a;
    return nullptr;
}
void EndMend(Creature* caster,PlayState* play) {
    if(auto* a=ResolveMend(caster,play)) {
        auto* r=ObjectExtension::GetInstance().Get<MendRecord>(a);
        if(r->claimant==MendIdentity(&caster->actor)->identity)r->claimant=0;
    }
    caster->mend.target=0;
}
bool MendAlly(Actor* caster,Actor* target,PlayState* play,bool starting) {
    if(target==caster || !target->update || !target->colChkInfo.health || target->category!=ACTORCAT_ENEMY)return false;
    // Wraiths cannot sustain one another. Curated native melee enemies use
    // colChkInfo health; bosses and scripted resurrecting Stalfos are excluded.
    if(!(target->id==ids[0] || (target->id==ACTOR_EN_TEST && target->params==2) ||
         target->id==ACTOR_EN_WF || (target->id==ACTOR_EN_ZF && target->params==-2)))return false;
    auto* a=AffixSystem::GetAffixData(caster);auto* b=AffixSystem::GetAffixData(target);
    if(a && b && a->packId && b->packId && a->packId!=b->packId)return false;
    auto* record=ObjectExtension::GetInstance().Get<MendRecord>(target);
    int received=record?record->received:0,health=target->colChkInfo.health,maximum=GetActorMaximumHealth(target);
    if(starting ? !NeedsMend(health,maximum,received) : !MendAmount(health,maximum,received))return false;
    if(record && record->claimant && record->claimant!=MendIdentity(caster)->identity)return false;
    Vec3f from=caster->world.pos,to=target->world.pos;from.y+=40;to.y+=35;
    return Distance(from,to)<=420 && std::abs(from.y-to.y)<100 && Line(play,from,to);
}
bool StartMend(Creature* caster,PlayState* play) {
    if(caster->mend.spent>=MendLimit || caster->state.phase!=Phase::Roam || caster->state.cooldown>1)return false;
    Actor* best=nullptr;float fraction=1;
    for(auto* a=play->actorCtx.actorLists[ACTORCAT_ENEMY].head;a;a=a->next)if(MendAlly(&caster->actor,a,play,true)) {
        float f=a->colChkInfo.health/float(GetActorMaximumHealth(a));
        if(f<fraction){best=a;fraction=f;}
    }
    if(!best)return false;
    auto* record=MendIdentity(best);
    // Match Advance's ready-on-this-tick cooldown, before it picks an attack.
    caster->state.cooldown=0;
    if(!BeginMend(caster->state,caster->mend,record->identity))return false;
    record->claimant=MendIdentity(&caster->actor)->identity;
    caster->aim=Math_Vec3f_Yaw(&caster->actor.world.pos,&best->world.pos);
    Audio_PlayActorSound2(&caster->actor,NA_SE_EN_STAL_WARAU);return true;
}
Technique* Native(Actor* a) {
    auto& ext=ObjectExtension::GetInstance();if(!ext.Has<Technique>(a)){Technique t;t.state.cooldown=100;ext.Set<Technique>(a,std::move(t));}return ext.Get<Technique>(a);
}
// Native 32-entry damage table: nuts, boomerang and hookshot stun without inventing damage.
DamageTable damageTable={{0x10,2,1,3,0x10,2,3,0x10,1,2,4,2,2,3,2,2,2,3,3,3,0,2,1,4,2,2,8,4,0,0,6,0}};
ColliderCylinderInit bodyInit={{COLTYPE_HIT0,AT_NONE,AC_ON|AC_TYPE_PLAYER,OC1_ON|OC1_TYPE_ALL,OC2_TYPE_1,COLSHAPE_CYLINDER},
    {ELEMTYPE_UNK0,{0,0,0},{0xFFCFFFFF,0,0},TOUCH_NONE,BUMP_ON|BUMP_HOOKABLE,OCELEM_ON},{32,50,0,{0,0,0}}};
ColliderCylinderInit boltInit={{COLTYPE_NONE,AT_ON|AT_TYPE_ENEMY,AC_NONE,OC1_NONE,OC2_TYPE_2,COLSHAPE_CYLINDER},
    {ELEMTYPE_UNK0,{DMG_ARROW_NORMAL,0,4},{0,0,0},TOUCH_ON|TOUCH_SFX_NONE,BUMP_NONE,OCELEM_NONE},{9,18,-9,{0,0,0}}};
void ProjectileInit(Actor* actor,PlayState* play) {
    auto* p=reinterpret_cast<Projectile*>(actor);p->age=0;p->delay=12;
    Collider_InitCylinder(play,&p->hitbox);Collider_SetCylinder(play,&p->hitbox,actor,&boltInit);
    if(actor->params==2){p->hitbox.dim.radius=27;p->hitbox.dim.height=55;p->hitbox.dim.yShift=-4;p->hitbox.info.toucher.dmgFlags=DMG_HAMMER_SWING;p->hitbox.info.toucher.damage=6;}
    actor->flags|=ACTOR_FLAG_UPDATE_CULLING_DISABLED;Actor_SetScale(actor,.01f);
}
void ProjectileDestroy(Actor* actor,PlayState* play){Collider_DestroyCylinder(play,&reinterpret_cast<Projectile*>(actor)->hitbox);}
void ProjectileUpdate(Actor* actor,PlayState* play) {
    auto* p=reinterpret_cast<Projectile*>(actor);
    if(!Enabled() || play->transitionTrigger!=TRANS_TRIGGER_OFF){Actor_Kill(actor);return;}
    if(!Active(play))return;
    if((p->hitbox.base.atFlags&(AT_HIT|AT_BOUNCED)) || ++p->age>80){Actor_Kill(actor);return;}
    if(actor->params==2) {
        if(p->age>p->delay+3){Actor_Kill(actor);return;}
        if(p->age<p->delay)return;
    } else {
        if(p->age>45){Actor_Kill(actor);return;}
        Vec3f next=actor->world.pos;next.x+=Math_SinS(actor->world.rot.y)*8;next.z+=Math_CosS(actor->world.rot.y)*8;next.y+=p->verticalSpeed;
        if(!Line(play,actor->world.pos,next)){Actor_Kill(actor);return;}actor->world.pos=next;
    }
    Collider_UpdateCylinder(actor,&p->hitbox);CollisionCheck_SetAT(play,&play->colChkCtx,&p->hitbox.base);
}
void ProjectileDraw(Actor* actor,PlayState* play) {
    auto* p=reinterpret_cast<Projectile*>(actor);
    if(actor->params==2) {
        DrawWarning(play,actor,27,2,.45f+.5f*std::min(1.f,float(p->age)/p->delay));
        if(p->age<p->delay)return;
    }
    FrameInterpolation_RecordOpenChild(actor,110);Matrix_Push();Matrix_Translate(actor->world.pos.x,actor->world.pos.y,actor->world.pos.z,MTXMODE_NEW);
    Matrix_RotateY(actor->world.rot.y*(3.14159265f/32768),MTXMODE_APPLY);
    if(actor->params==0){Matrix_Scale(.007f,.007f,.007f,MTXMODE_APPLY);DrawCreaturePart(play,15,.4f);}
    else {float scale=actor->params==2?.04f:.013f;Matrix_Scale(scale,scale,scale,MTXMODE_APPLY);Matrix_Translate(0,-2700,-300,MTXMODE_APPLY);DrawCreaturePart(play,14,1);}
    Matrix_Pop();FrameInterpolation_RecordCloseChild();
}
int BoltCount(PlayState* play) {int n=0;for(auto* a=play->actorCtx.actorLists[ACTORCAT_ITEMACTION].head;a;a=a->next)if(a->update && a->id==projectileId)++n;return n;}
void EnsureRegistered();
Actor* Bolt(Actor* source,PlayState* play,int style,Vec3f point,int16_t yaw,int delay=12) {
    EnsureRegistered();if(play->actorCtx.total>=180 || BoltCount(play)>=24)return nullptr;
    auto* a=Actor_Spawn(&play->actorCtx,play,projectileId,point.x,point.y,point.z,0,yaw,0,style);if(!a || !a->update)return nullptr;
    auto* p=reinterpret_cast<Projectile*>(a);p->delay=delay;
    if(style!=2){auto target=GET_PLAYER(play)->actor.world.pos;target.y+=30;p->verticalSpeed=std::clamp((target.y-point.y)/std::max(1.f,Distance(point,target)/8),-2.5f,2.5f);}
    // A detached snapshot survives its caster's death; no dangling parent actor pointer.
    if(auto* affix=AffixSystem::GetAffixData(source))ObjectExtension::GetInstance().Set<ARPGAffixData>(a,ARPGAffixData(*affix));
    return a;
}
void Volley(Actor* actor,PlayState* play,int style,int16_t yaw) {
    Vec3f origin=actor->world.pos;origin.y+=style==0?48:38;
    for(int side=-1;side<=1;++side)Bolt(actor,play,style,origin,int16_t(yaw+side*0x1000));
}
void GroundStrike(Actor* actor,PlayState* play,int16_t yaw) {
    for(int i=0;i<3;++i) {
        Vec3f p=actor->world.pos;p.x+=Math_SinS(yaw)*(55+i*50);p.z+=Math_CosS(yaw)*(55+i*50);
        if(!Floor(play,actor,p,50))continue;Vec3f a=actor->world.pos,b=p;a.y+=20;b.y+=20;if(!Line(play,a,b))continue;
        Bolt(actor,play,2,p,yaw,12+i*6);
    }
}
void Init(Actor* actor,PlayState* play) {
    auto* c=reinterpret_cast<Creature*>(actor);c->state=State{};c->mend=MendState{};c->age=0;c->invulnerable=0;c->walk=0;c->aim=actor->world.rot.y;
    actor->colChkInfo.health=Type(actor)==Kind::Sentinel?16:12;actor->colChkInfo.mass=80;actor->colChkInfo.damageTable=&damageTable;
    actor->gravity=-1.5f;actor->minVelocityY=-12;actor->targetMode=3;actor->naviEnemyId=0xFF;Actor_SetScale(actor,.01f);
    ActorShape_Init(&actor->shape,0,ActorShadow_DrawCircle,Type(actor)==Kind::Sentinel?40.f:25.f);
    Collider_InitCylinder(play,&c->body);Collider_SetCylinder(play,&c->body,actor,&bodyInit);
    if(Type(actor)==Kind::Wraith){c->body.dim.radius=24;c->body.dim.height=74;c->body.dim.yShift=12;}
    Actor_SetFocus(actor,Type(actor)==Kind::Sentinel?35.f:55.f);
}
void Destroy(Actor* actor,PlayState* play){auto* c=reinterpret_cast<Creature*>(actor);EndMend(c,play);Collider_DestroyCylinder(play,&c->body);}
void Update(Actor* actor,PlayState* play) {
    auto* c=reinterpret_cast<Creature*>(actor);auto kind=Type(actor);
    if(!Enabled()){EndMend(c,play);Actor_Kill(actor);return;}if(!Active(play))return;
    ++c->age;
    if(c->state.phase==Phase::Dead){if(--c->state.timer<=0)Actor_Kill(actor);return;}
    if(c->invulnerable>0)--c->invulnerable;
    if(c->body.base.acFlags&AC_HIT) {
        c->body.base.acFlags&=~AC_HIT;
        if(actor->colChkInfo.damage>0){Actor_ApplyDamage(actor);c->invulnerable=8;Audio_PlayActorSound2(actor,NA_SE_EN_STAL_DAMAGE);}
        if(actor->colChkInfo.health==0){EndMend(c,play);c->state.phase=Phase::Dead;c->state.timer=24;actor->flags&=~ACTOR_FLAG_ATTENTION_ENABLED;actor->speedXZ=0;GameInteractor_ExecuteOnEnemyDefeat(actor);Audio_PlayActorSound2(actor,NA_SE_EN_STAL_DEAD);return;}
    }
    Vec3f floor=actor->world.pos;bool supported=Floor(play,actor,floor,65);bool visible=Visible(play,actor);
    bool started=kind==Kind::Wraith && visible && supported && actor->xzDistToPlayer<480 &&
        Distance(actor->world.pos,actor->home.pos)<1000 && StartMend(c,play);
    auto before=c->state.phase;bool release=false;
    if(c->mend.target) {
        auto* target=ResolveMend(c,play);
        bool connected=target && supported && actor->xzDistToPlayer<900 && Distance(actor->world.pos,actor->home.pos)<1100 && MendAlly(actor,target,play,false);
        auto result=started?MendResult::Waiting:AdvanceMend(c->state,c->mend,true,connected);
        if(result==MendResult::Completed) {
            auto* record=MendIdentity(target);
            target->colChkInfo.health+=MendAmount(target->colChkInfo.health,GetActorMaximumHealth(target),record->received);
            ++record->received;Audio_PlayActorSound2(target,NA_SE_EV_HEALING);
        }
        if(result!=MendResult::Waiting)EndMend(c,play);
    } else release=Advance(kind,c->state,{true,visible,supported,actor->xzDistToPlayer,Distance(actor->world.pos,actor->home.pos)});
    if(before!=Phase::Windup && c->state.phase==Phase::Windup){c->aim=actor->yawTowardsPlayer;Audio_PlayActorSound2(actor,kind==Kind::Sentinel?NA_SE_EN_STAL_SAKEBI:NA_SE_EN_STAL_WARAU);}
    actor->speedXZ=0;
    if(release){if(kind==Kind::Sentinel)GroundStrike(actor,play,c->aim);else Volley(actor,play,1,c->aim);}
    if(c->state.phase==Phase::Roam && supported) {
        int16_t yaw=actor->yawTowardsPlayer;float speed=0;
        if(Distance(actor->world.pos,actor->home.pos)>850 || actor->xzDistToPlayer>900){yaw=Math_Vec3f_Yaw(&actor->world.pos,&actor->home.pos);speed=Distance(actor->world.pos,actor->home.pos)>50?1.8f:0;}
        else if(visible) {
            if(kind==Kind::Sentinel)speed=actor->xzDistToPlayer>115?1.8f:0;
            else {yaw+=actor->xzDistToPlayer<190?0x7000:actor->xzDistToPlayer>330?0:0x3800;speed=2.1f;}
        }
        Vec3f step=actor->world.pos;step.x+=Math_SinS(yaw)*50;step.z+=Math_CosS(yaw)*50;Vec3f start=actor->world.pos,end=step;start.y+=25;end.y+=25;
        if(speed>0 && Floor(play,actor,step) && Line(play,start,end)){actor->world.rot.y=yaw;actor->speedXZ=speed;}
    }
    int16_t facing=c->state.phase==Phase::Windup?c->aim:actor->yawTowardsPlayer;
    Math_SmoothStepToS(&actor->shape.rot.y,facing,4,0x700,0x80);
    Actor_MoveXZGravity(actor);Actor_UpdateBgCheckInfo(play,actor,25,35,55,5);
    if(actor->world.pos.y<BGCHECK_Y_MIN+100 || actor->world.pos.y<actor->home.pos.y-900){Actor_Kill(actor);return;}
    Collider_UpdateCylinder(actor,&c->body);
    if(!c->invulnerable)CollisionCheck_SetAC(play,&play->colChkCtx,&c->body.base);
    CollisionCheck_SetOC(play,&play->colChkCtx,&c->body.base);Actor_SetFocus(actor,kind==Kind::Sentinel?35.f:55.f);
}
void EnsureRegistered() {
    if(ids[0]>=0 || !ActorDB::Instance)return;
    for(int k=0;k<2;++k){ActorDBInit entry{k==0?"Arpg_CairnSentinel":"Arpg_LanternWraith",Name(Kind(k)),-1,ACTORCAT_ENEMY,
        ACTOR_FLAG_ATTENTION_ENABLED|ACTOR_FLAG_HOSTILE|ACTOR_FLAG_UPDATE_CULLING_DISABLED,OBJECT_GAMEPLAY_KEEP,sizeof(Creature),Init,Destroy,Update,DrawCreature,nullptr};ids[k]=ActorDB::Instance->AddEntry(entry).entry.id;}
    ActorDBInit bolt{"Arpg_CreatureProjectile","Creature projectile",-1,ACTORCAT_ITEMACTION,ACTOR_FLAG_UPDATE_CULLING_DISABLED,OBJECT_GAMEPLAY_KEEP,sizeof(Projectile),ProjectileInit,ProjectileDestroy,ProjectileUpdate,ProjectileDraw,nullptr};projectileId=ActorDB::Instance->AddEntry(bolt).entry.id;
}
Strike Classify(uint32_t flags) {
    if(flags&DMG_HAMMER)return Strike::Hammer;if(flags&DMG_EXPLOSIVE)return Strike::Blast;
    if(flags&(DMG_ARROW_LIGHT|DMG_MAGIC_LIGHT|DMG_MIR_RAY))return Strike::Light;if(flags&(DMG_ARROW_ICE|DMG_MAGIC_ICE))return Strike::Ice;
    if(flags&DMG_BOOMERANG)return Strike::Boomerang;if(flags&DMG_HOOKSHOT)return Strike::Hookshot;if(flags&DMG_DEKU_NUT)return Strike::Nut;
    if(flags&(DMG_ARROW|DMG_SLINGSHOT))return Strike::Arrow;if(flags&(DMG_SWORD|DMG_DEKU_STICK))return Strike::Blade;return Strike::Other;
}
}
bool Enabled(){return Enemies::Enabled() && CVarGetInteger(CVAR_ENHANCEMENT("ARPG.Creatures"),1);}
int ActorId(Kind kind){EnsureRegistered();return ids[int(kind)];}
bool IsCreature(const Actor* actor){return actor && ((ids[0]>=0 && actor->id==ids[0]) || (ids[1]>=0 && actor->id==ids[1]));}
Actor* Spawn(PlayState* play,Kind kind,Vec3f position) {
    if(!play || !Enabled() || play->actorCtx.total>=180 || !Floor(play,&GET_PLAYER(play)->actor,position))return nullptr;
    int id=ActorId(kind);if(id<0)return nullptr;return Actor_Spawn(&play->actorCtx,play,id,position.x,position.y,position.z,0,Math_Vec3f_Yaw(&position,&GET_PLAYER(play)->actor.world.pos),0,0);
}
State Status(const Actor* actor) {
    if(IsCreature(actor))return reinterpret_cast<const Creature*>(actor)->state;
    if(actor)if(auto* t=ObjectExtension::GetInstance().Get<Technique>(actor))return t->state;return {};
}
MendState MendStatus(const Actor* actor){return IsCreature(actor)?reinterpret_cast<const Creature*>(actor)->mend:MendState{};}
Actor* MendTarget(const Actor* actor,PlayState* play){return IsCreature(actor)?ResolveMend(reinterpret_cast<const Creature*>(actor),play):nullptr;}
float OnContact(Actor* target,Actor* source,uint32_t flags,uint8_t alignment,float damage) {
    if(!Enabled() || !target || !source || !target->update || !target->colChkInfo.health || !(alignment&AT_TYPE_PLAYER) || !Active(gPlayState) || Expeditions::DamageMultiplier(target,flags)<=0)return 1;
    auto strike=Classify(flags);bool stun=strike==Strike::Nut || strike==Strike::Hookshot || strike==Strike::Boomerang;
    if(IsCreature(target)) {
        auto* c=reinterpret_cast<Creature*>(target);float result=Receive(Type(target),c->state,strike,damage>0,stun);
        if(c->mend.target && c->state.phase!=Phase::Windup){c->state.cooldown=std::max(c->state.cooldown,80);EndMend(c,gPlayState);}
        return result;
    }
    if(auto* t=ObjectExtension::GetInstance().Get<Technique>(target);t && (damage>0 || stun) && t->state.phase==Phase::Windup){Interrupt(t->state,25);t->state.cooldown=120;}
    return 1;
}
int NeutralTechnique(Actor* actor,PlayState* play) {
    if(!Enabled() || !Active(play) || (actor->id!=ACTOR_EN_TEST && actor->id!=ACTOR_EN_WF) || !actor->colChkInfo.health)return 0;
    auto* t=Native(actor);auto& s=t->state;bool wolf=actor->id==ACTOR_EN_WF;
    if(s.phase==Phase::Stagger || s.phase==Phase::Recover)return 0;
    bool visible=Visible(play,actor);float dist=actor->xzDistToPlayer;
    bool ally=false;for(auto* a=play->actorCtx.actorLists[ACTORCAT_ENEMY].head;a;a=a->next)if(a!=actor && a->id==ACTOR_EN_WF && a->update && a->colChkInfo.health && Distance(a->world.pos,actor->world.pos)<400 && std::abs(a->world.pos.y-actor->world.pos.y)<60){ally=true;break;}
    if(s.phase==Phase::Roam) {
        if(s.cooldown || !visible || dist<140 || dist>(wolf?420:550) || (wolf && !ally))return 0;
        s.phase=Phase::Windup;s.timer=0;t->aim=actor->yawTowardsPlayer;t->serviced=play->gameplayFrames;
        Audio_PlayActorSound2(actor,wolf?NA_SE_EN_WOLFOS_CRY:NA_SE_EN_STAL_WARAU);
    }
    actor->speedXZ=0;
    if(!visible || dist>650){s.phase=Phase::Roam;s.cooldown=100;return 1;}
    if(t->serviced!=play->gameplayFrames){++s.timer;t->serviced=play->gameplayFrames;}
    Math_SmoothStepToS(&actor->shape.rot.y,t->aim,3,0x600,0x80);
    if(s.timer>=(wolf?42:34)) {
        if(wolf) {
            for(auto* a=play->actorCtx.actorLists[ACTORCAT_ENEMY].head;a;a=a->next)if(a->id==ACTOR_EN_WF && a->update && a->colChkInfo.health && Distance(a->world.pos,actor->world.pos)<400 && std::abs(a->world.pos.y-actor->world.pos.y)<60)Native(a)->rally=120;
        } else Volley(actor,play,0,t->aim);
        ++s.attacks;s.phase=Phase::Recover;s.timer=20;s.cooldown=wolf?260:160;
    }
    return 1;
}
float RallySpeed(Actor* actor){auto* t=ObjectExtension::GetInstance().Get<Technique>(actor);return Enabled() && t && t->rally>0?1.25f:1.f;}
void DrawGuide() {
    if(Testing::Active() && Theme::Section("Practice encounters")) { Theme::Muted("Temporary adventure: summon ahead, then Resume to fight.");DrawTesting(); }
    if(Theme::Section("New creatures"))for(auto kind:{Kind::Sentinel,Kind::Wraith}) {Theme::Label(Name(kind),20,IM_COL32(235,221,156,255));Theme::Wrapped("%s",Guide(kind));ImGui::Spacing();}
    if(Theme::Section("Pack techniques")) {
        Theme::Wrapped("Stalfos: at medium range, a bright charging ring warns of three bone darts. Sidestep their committed direction, block with a shield, or interrupt the charge with a hit or accepted stun.");
        Theme::Wrapped("Wolfos: with another wolf nearby, a blue-ringed howl rallies the pack for six seconds of faster movement. Interrupt the two-second howl before it finishes. A lone wolf cannot rally.");
    }
}
// [TIME] | [INFO] | CreatureSystem.SkulltulaPractice | A real ceiling and clear floor keep native descent and swaying anchored to the room.
Actor* SpawnSkulltulaPractice(PlayState* play,int variant) {
    if(!play || variant<0 || variant>2)return nullptr;
    auto* player=GET_PLAYER(play);auto origin=player->actor.world.pos;auto yaw=player->actor.shape.rot.y;
    int count=0;for(auto* a=play->actorCtx.actorLists[ACTORCAT_ENEMY].head;a;a=a->next)if(a->id==ACTOR_EN_ST && a->update)++count;
    if(count>=8)return nullptr;
    for(float distance:{180.f,240.f})for(int offset:{0,-0x1000,0x1000}) {
        Vec3f floor={origin.x+Math_SinS(yaw+offset)*distance,origin.y,origin.z+Math_CosS(yaw+offset)*distance};
        if(!Floor(play,&player->actor,floor))continue;
        auto bottom=floor;bottom.y+=60;auto top=bottom;top.y+=900;
        Vec3f ceiling{};CollisionPoly* poly=nullptr;int bg=BGCHECK_SCENE;
        if(!BgCheck_EntityLineTest1(&play->colCtx,&bottom,&top,&ceiling,&poly,false,false,true,true,&bg) || !poly || bg!=BGCHECK_SCENE || ceiling.y-floor.y<180)continue;
        auto p=floor;p.y=std::min(ceiling.y-(variant==1?90.f:65.f),floor.y+300);
        auto eye=origin;eye.y+=40;if(!Line(play,eye,bottom) || !Line(play,eye,p))continue;
        bool occupied=false;for(auto* a=play->actorCtx.actorLists[ACTORCAT_ENEMY].head;a;a=a->next)if(a->update && Distance(a->world.pos,p)<100)occupied=true;
        if(occupied)continue;
        auto* a=Actor_Spawn(&play->actorCtx,play,ACTOR_EN_ST,p.x,p.y,p.z,0,yaw+0x8000,0,variant);
        if(a && a->update)return a;
    }
    return nullptr;
}
// [TIME] | [INFO] | CreatureSystem.BubblePractice | Only path-independent native variants can be summoned in arbitrary rooms.
Actor* SpawnBubblePractice(PlayState* play,int variant) {
    if(!play || variant<0 || variant>2)return nullptr;
    auto* player=GET_PLAYER(play);auto origin=player->actor.world.pos;auto yaw=player->actor.shape.rot.y;
    int count=0;for(auto* a=play->actorCtx.actorLists[ACTORCAT_ENEMY].head;a;a=a->next)if(a->id==ACTOR_EN_BB && a->params<0 && a->update)++count;
    if(count>=6)return nullptr;
    for(int offset:{0,-0x1800,0x1800}) {
        Vec3f p={origin.x+Math_SinS(yaw+offset)*210,origin.y,origin.z+Math_CosS(yaw+offset)*210};
        if(!Floor(play,&player->actor,p))continue;
        auto eye=origin;eye.y+=40;auto target=p;target.y+=variant==2?160:80;
        if(!Line(play,eye,target))continue;
        auto bottom=p;bottom.y+=10;if(!Line(play,bottom,target))continue;
        bool occupied=false;for(auto* a=play->actorCtx.actorLists[ACTORCAT_ENEMY].head;a;a=a->next)if(a->update && Distance(a->world.pos,p)<90)occupied=true;
        if(occupied)continue;
        if(variant==2)p.y+=100;
        int param=variant==0?0xFF:variant==1?0xFE:0x3FC;
        auto* a=Actor_Spawn(&play->actorCtx,play,ACTOR_EN_BB,p.x,p.y,p.z,0,yaw+0x8000,0,param);
        if(a && a->update)return a;
    }
    return nullptr;
}
// [TIME] | [INFO] | CreatureSystem.DodongoPractice | Check the wide body and tail footprint before placing the native ground enemy.
Actor* SpawnDodongoPractice(PlayState* play) {
    if(!play)return nullptr;
    auto* player=GET_PLAYER(play);auto origin=player->actor.world.pos;auto yaw=player->actor.shape.rot.y;
    int count=0;for(auto* a=play->actorCtx.actorLists[ACTORCAT_ENEMY].head;a;a=a->next)if(a->id==ACTOR_EN_DODONGO && a->update)++count;
    if(count>=4)return nullptr;
    for(int offset:{0,-0x1800,0x1800}) {
        Vec3f p={origin.x+Math_SinS(yaw+offset)*230,origin.y,origin.z+Math_CosS(yaw+offset)*230};
        if(!Floor(play,&player->actor,p))continue;
        auto eye=origin;eye.y+=40;auto target=p;target.y+=60;
        if(!Line(play,eye,target))continue;
        auto bottom=p;bottom.y+=2;auto top=p;top.y+=80;if(!Line(play,bottom,top))continue;
        bool safe=true;
        for(int i=0;i<8;++i){auto q=p;q.x+=Math_SinS(i*0x2000)*100;q.z+=Math_CosS(i*0x2000)*100;
            if(!Floor(play,&player->actor,q,18)){safe=false;break;}q.y+=60;if(!Line(play,target,q)){safe=false;break;}}
        for(auto* a=play->actorCtx.actorLists[ACTORCAT_ENEMY].head;a;a=a->next)if(a->update && std::abs(a->world.pos.y-p.y)<80 && Distance(a->world.pos,p)<150)safe=false;
        if(!safe)continue;
        auto* actor=Actor_Spawn(&play->actorCtx,play,ACTOR_EN_DODONGO,p.x,p.y,p.z,0,yaw+0x8000,0,-1);
        if(actor && actor->update)return actor;
    }
    return nullptr;
}
// [TIME] | [INFO] | CreatureSystem.RedeadPractice | Native undead need clear floor and room for upright and coffin-rise poses.
Actor* SpawnRedeadPractice(PlayState* play,int variant) {
    if(!play || variant<0 || variant>3)return nullptr;
    auto* player=GET_PLAYER(play);auto origin=player->actor.world.pos;auto yaw=player->actor.shape.rot.y;
    int count=0;for(auto* a=play->actorCtx.actorLists[ACTORCAT_ENEMY].head;a;a=a->next)if(a->id==ACTOR_EN_RD && a->update)++count;
    if(count>=4)return nullptr;
    for(int offset:{0,-0x1800,0x1800}) {
        Vec3f p={origin.x+Math_SinS(yaw+offset)*190,origin.y,origin.z+Math_CosS(yaw+offset)*190};
        if(!Floor(play,&player->actor,p))continue;
        auto eye=origin;eye.y+=40;auto target=p;target.y+=60;if(!Line(play,eye,target))continue;
        auto bottom=p;bottom.y+=2;auto top=p;top.y+=130;if(!Line(play,bottom,top))continue;
        bool safe=true;
        for(int i=0;i<8;++i){auto q=p;q.x+=Math_SinS(i*0x2000)*45;q.z+=Math_CosS(i*0x2000)*45;
            if(!Floor(play,&player->actor,q,12)){safe=false;break;}q.y+=60;if(!Line(play,target,q)){safe=false;break;}}
        for(auto* a=play->actorCtx.actorLists[ACTORCAT_ENEMY].head;a;a=a->next)if(a->update && std::abs(a->world.pos.y-p.y)<80 && Distance(a->world.pos,p)<90)safe=false;
        if(!safe)continue;
        const int params[]={0,2,-2,-3};auto* actor=Actor_Spawn(&play->actorCtx,play,ACTOR_EN_RD,p.x,p.y,p.z,0,yaw+0x8000,0,params[variant]);
        if(actor && actor->update)return actor;
    }
    return nullptr;
}
// [TIME] | [INFO] | CreatureSystem.IronKnucklePractice | Keep enough room for the native wide axe swing and use the no-switch sentinel.
Actor* SpawnIronKnucklePractice(PlayState* play,int variant) {
    if(!play || variant<0 || variant>2 || Flags_GetClear(play,play->roomCtx.curRoom.num))return nullptr;
    auto* player=GET_PLAYER(play);auto origin=player->actor.world.pos;auto yaw=player->actor.shape.rot.y;
    int count=0;for(auto* a=play->actorCtx.actorLists[ACTORCAT_ENEMY].head;a;a=a->next)if(a->id==ACTOR_EN_IK && a->update)++count;
    if(count>=3)return nullptr;
    for(int offset:{0,-0x1800,0x1800}) {
        Vec3f p={origin.x+Math_SinS(yaw+offset)*230,origin.y,origin.z+Math_CosS(yaw+offset)*230};
        if(!Floor(play,&player->actor,p))continue;
        auto eye=origin;eye.y+=40;auto target=p;target.y+=65;if(!Line(play,eye,target))continue;
        auto bottom=p;bottom.y+=2;auto top=p;top.y+=160;if(!Line(play,bottom,top))continue;
        bool safe=true;
        for(int i=0;i<8;++i){auto q=p;q.x+=Math_SinS(i*0x2000)*100;q.z+=Math_CosS(i*0x2000)*100;
            if(!Floor(play,&player->actor,q,15)){safe=false;break;}q.y+=65;if(!Line(play,target,q)){safe=false;break;}}
        for(auto* a=play->actorCtx.actorLists[ACTORCAT_ENEMY].head;a;a=a->next)if(a->update && std::abs(a->world.pos.y-p.y)<100 && Distance(a->world.pos,p)<160)safe=false;
        if(!safe)continue;
        auto* actor=Actor_Spawn(&play->actorCtx,play,ACTOR_EN_IK,p.x,p.y,p.z,0,yaw+0x8000,0,0xFF00|(variant+1));
        if(actor && actor->update)return actor;
    }
    return nullptr;
}
// [TIME] | [INFO] | CreatureSystem.StingerPractice | Match native land, decoy-circle and water habitats before spawning their original actors.
Actor* SpawnStingerPractice(PlayState* play,int variant) {
    if(!play||variant<0||variant>2)return nullptr;
    auto* player=GET_PLAYER(play);auto origin=player->actor.world.pos;auto yaw=player->actor.shape.rot.y;
    int count=0;for(auto* a=play->actorCtx.actorLists[ACTORCAT_ENEMY].head;a;a=a->next)if(a->update&&(a->id==ACTOR_EN_EIYER||a->id==ACTOR_EN_WEIYER))++count;
    if(count+(variant==1?4:1)>8)return nullptr;
    for(float distance:{220.f,300.f,380.f})for(int offset:{0,-0x1800,0x1800}) {
        Vec3f p={origin.x+Math_SinS(yaw+offset)*distance,origin.y,origin.z+Math_CosS(yaw+offset)*distance};
        auto eye=origin;eye.y+=35;bool safe=true;
        if(variant==2){
            float surface=0;WaterBox* box=nullptr;CollisionPoly* poly=nullptr;
            if(!WaterBox_GetSurfaceImpl(play,&play->colCtx,p.x,p.z,&surface,&box)||std::abs(surface-origin.y)>100)continue;
            p.y=surface+50;float bottom=BgCheck_EntityRaycastFloor1(&play->colCtx,&poly,&p);
            if(!poly||bottom<=BGCHECK_Y_MIN||surface-bottom<45)continue;
            for(int i=0;i<8;++i){auto q=p;q.x+=Math_SinS(i*0x2000)*60;q.z+=Math_CosS(i*0x2000)*60;
                float h=0;WaterBox* b=nullptr;if(!WaterBox_GetSurfaceImpl(play,&play->colCtx,q.x,q.z,&h,&b)||std::abs(h-surface)>5){safe=false;break;}
                float floor=BgCheck_EntityRaycastFloor1(&play->colCtx,&poly,&q);if(floor<=BGCHECK_Y_MIN||h-floor<25){safe=false;break;}}
            p.y=surface+5;if(!Line(play,eye,p))continue;p.y=surface-15;
        }else{
            if(!Floor(play,&player->actor,p))continue;auto target=p;target.y+=50;auto high=p;high.y+=140;
            if(!Line(play,eye,target)||!Line(play,target,high))continue;
            for(int i=0;i<8;++i){auto q=p;q.x+=Math_SinS(i*0x2000)*(variant==1?130:65);q.z+=Math_CosS(i*0x2000)*(variant==1?130:65);
                if(!Floor(play,&player->actor,q,20)){safe=false;break;}q.y+=50;if(!Line(play,target,q)){safe=false;break;}}
        }
        for(auto* a=play->actorCtx.actorLists[ACTORCAT_ENEMY].head;a;a=a->next)if(a->update&&std::abs(a->world.pos.y-p.y)<100&&Distance(a->world.pos,p)<(variant==1?160:90))safe=false;
        if(!safe)continue;
        auto* a=Actor_Spawn(&play->actorCtx,play,variant==2?ACTOR_EN_WEIYER:ACTOR_EN_EIYER,p.x,p.y,p.z,0,yaw+0x8000,0,variant==0?10:0);
        if(a&&a->update)return a;
    }return nullptr;
}
Actor* SpawnShellbladePractice(PlayState* play,int variant) {
    if(!play||variant<0||variant>1)return nullptr;
    auto* player=GET_PLAYER(play);auto origin=player->actor.world.pos;auto yaw=player->actor.shape.rot.y;
    int count=0;for(auto* a=play->actorCtx.actorLists[ACTORCAT_ENEMY].head;a;a=a->next)if(a->update&&a->id==ACTOR_EN_SB)++count;
    if(count>=6)return nullptr;
    for(float distance:{220.f,300.f,380.f})for(int offset:{0,-0x1800,0x1800}) {
        Vec3f p={origin.x+Math_SinS(yaw+offset)*distance,origin.y,origin.z+Math_CosS(yaw+offset)*distance};
        auto eye=origin;eye.y+=35;bool safe=true;
        if(variant==1){
            float surface=0;WaterBox* box=nullptr;CollisionPoly* poly=nullptr;
            if(!WaterBox_GetSurfaceImpl(play,&play->colCtx,p.x,p.z,&surface,&box)||std::abs(surface-origin.y)>100)continue;
            p.y=surface+50;float bottom=BgCheck_EntityRaycastFloor1(&play->colCtx,&poly,&p);
            if(!poly||bottom<=BGCHECK_Y_MIN||surface-bottom<45)continue;
            for(int i=0;i<8;++i){auto q=p;q.x+=Math_SinS(i*0x2000)*60;q.z+=Math_CosS(i*0x2000)*60;
                float h=0;WaterBox* b=nullptr;if(!WaterBox_GetSurfaceImpl(play,&play->colCtx,q.x,q.z,&h,&b)||std::abs(h-surface)>5){safe=false;break;}
                float floor=BgCheck_EntityRaycastFloor1(&play->colCtx,&poly,&q);if(floor<=BGCHECK_Y_MIN||h-floor<25){safe=false;break;}}
            p.y=surface+5;if(!Line(play,eye,p))continue;p.y=surface-15;
        }else{
            if(!Floor(play,&player->actor,p))continue;auto target=p;target.y+=50;auto high=p;high.y+=140;
            if(!Line(play,eye,target)||!Line(play,target,high))continue;
            for(int i=0;i<8;++i){auto q=p;q.x+=Math_SinS(i*0x2000)*60;q.z+=Math_CosS(i*0x2000)*60;
                if(!Floor(play,&player->actor,q,20)){safe=false;break;}q.y+=50;if(!Line(play,target,q)){safe=false;break;}}
        }
        for(auto* a=play->actorCtx.actorLists[ACTORCAT_ENEMY].head;a;a=a->next)if(a->update&&std::abs(a->world.pos.y-p.y)<100&&Distance(a->world.pos,p)<90)safe=false;
        if(!safe)continue;
        auto* a=Actor_Spawn(&play->actorCtx,play,ACTOR_EN_SB,p.x,p.y,p.z,0,yaw+0x8000,0,0);
        if(a&&a->update)return a;
    }return nullptr;
}
Actor* SpawnArmosPractice(PlayState* play,int variant) {
    if(!play||variant<0||variant>1)return nullptr;
    auto* player=GET_PLAYER(play);auto origin=player->actor.world.pos;auto yaw=player->actor.shape.rot.y;
    int count=0;for(int cat:{ACTORCAT_ENEMY,ACTORCAT_BG})for(auto* a=play->actorCtx.actorLists[cat].head;a;a=a->next)if(a->update&&a->id==ACTOR_EN_AM)++count;
    if(count>=4)return nullptr;
    for(float distance:{220.f,300.f,380.f})for(int offset:{0,-0x1800,0x1800}) {
        Vec3f p={origin.x+Math_SinS(yaw+offset)*distance,origin.y,origin.z+Math_CosS(yaw+offset)*distance};
        if(!Floor(play,&player->actor,p))continue;auto eye=origin;eye.y+=40;auto target=p;target.y+=60;auto high=p;high.y+=160;
        if(!Line(play,eye,target)||!Line(play,target,high))continue;bool safe=true;
        for(int i=0;i<8;++i){auto q=p;q.x+=Math_SinS(i*0x2000)*65;q.z+=Math_CosS(i*0x2000)*65;
            if(!Floor(play,&player->actor,q,15)){safe=false;break;}q.y+=60;if(!Line(play,target,q)){safe=false;break;}}
        for(int cat:{ACTORCAT_ENEMY,ACTORCAT_BG})for(auto* a=play->actorCtx.actorLists[cat].head;a;a=a->next)if(a->update&&std::abs(a->world.pos.y-p.y)<120&&Distance(a->world.pos,p)<110)safe=false;
        if(!safe)continue;
        auto* a=Actor_Spawn(&play->actorCtx,play,ACTOR_EN_AM,p.x,p.y,p.z,0,yaw+0x8000,0,variant==0?1:0);
        if(a&&a->update)return a;
    }return nullptr;
}
Actor* SpawnPeahatPractice(PlayState* play,int variant) {
    if(!play||variant<0||variant>2)return nullptr;
    auto* player=GET_PLAYER(play);auto origin=player->actor.world.pos;auto yaw=player->actor.shape.rot.y;
    // [TIME] | [INFO] | Creatures.PeahatPractice | Hyrule Field has seven distant native adults; limit the nearby encounter without blocking practice across the entire scene.
    int adults=0,larvae=0;for(auto* a=play->actorCtx.actorLists[ACTORCAT_ENEMY].head;a;a=a->next)if(a->update&&a->id==ACTOR_EN_PEEHAT&&Distance(a->world.pos,origin)<2000.f){if(a->params>0)++larvae;else ++adults;}
    if((variant<2&&adults>=4)||(variant==2&&larvae>=10))return nullptr;
    const float radius=variant==2?45.f:210.f,headroom=variant==1?680.f:variant==0?340.f:160.f;
    for(float distance:variant==2?std::initializer_list<float>{190.f,270.f,350.f}:std::initializer_list<float>{550.f,750.f,950.f})for(int offset:{0,-0x1800,0x1800}) {
        Vec3f p={origin.x+Math_SinS(yaw+offset)*distance,origin.y,origin.z+Math_CosS(yaw+offset)*distance};
        if(!Floor(play,&player->actor,p,100))continue;auto eye=origin;eye.y+=40;auto target=p;target.y+=70;auto high=p;high.y+=headroom;
        if(!Line(play,eye,target)||!Line(play,target,high))continue;bool safe=true;
        for(int i=0;i<12;++i){auto q=p;q.x+=Math_SinS(i*0x10000/12)*radius;q.z+=Math_CosS(i*0x10000/12)*radius;
            if(!Floor(play,&player->actor,q,30)){safe=false;break;}q.y+=70;if(!Line(play,target,q)){safe=false;break;}
            auto top=q;top.y=p.y+headroom;if(!Line(play,q,top)){safe=false;break;}}
        for(auto* a=play->actorCtx.actorLists[ACTORCAT_ENEMY].head;a;a=a->next)if(a->update&&std::abs(a->world.pos.y-p.y)<200&&Distance(a->world.pos,p)<radius+90)safe=false;
        if(!safe)continue;if(variant==2)p.y+=85;
        auto* a=Actor_Spawn(&play->actorCtx,play,ACTOR_EN_PEEHAT,p.x,p.y,p.z,0,yaw+0x8000,0,variant-1);
        if(a&&a->update)return a;
    }return nullptr;
}
// [TIME] | [INFO] | Creatures.MoblinPractice | Patrol actors require authored scene paths; free placement offers the native guard and club fighter.
Actor* SpawnMoblinPractice(PlayState* play,int variant) {
    if(!play||variant<0||variant>1)return nullptr;
    auto* player=GET_PLAYER(play);if(!player)return nullptr;
    auto origin=player->actor.world.pos;auto yaw=player->actor.shape.rot.y;
    int count=0;for(auto* a=play->actorCtx.actorLists[ACTORCAT_ENEMY].head;a;a=a->next)if(a->update&&a->id==ACTOR_EN_MB&&Distance(a->world.pos,origin)<2000.f)++count;
    if(count>=4)return nullptr;
    const float radius=variant?180.f:80.f,headroom=variant?320.f:140.f;
    for(float distance:variant?std::initializer_list<float>{500.f,650.f,800.f}:std::initializer_list<float>{260.f,360.f,460.f})for(int offset:{0,-0x1800,0x1800}) {
        Vec3f p={origin.x+Math_SinS(yaw+offset)*distance,origin.y,origin.z+Math_CosS(yaw+offset)*distance};
        if(!Floor(play,&player->actor,p,50))continue;auto eye=origin;eye.y+=40;auto target=p;target.y+=70;auto high=p;high.y+=headroom;
        if(!Line(play,eye,target)||!Line(play,target,high))continue;bool safe=true;
        for(int i=0;i<12;++i){auto q=p;q.x+=Math_SinS(i*0x10000/12)*radius;q.z+=Math_CosS(i*0x10000/12)*radius;
            if(!Floor(play,&player->actor,q,20)){safe=false;break;}q.y+=70;if(!Line(play,target,q)){safe=false;break;}
            auto top=q;top.y=p.y+headroom;if(!Line(play,q,top)){safe=false;break;}}
        for(auto* a=play->actorCtx.actorLists[ACTORCAT_ENEMY].head;a;a=a->next)if(a->update&&std::abs(a->world.pos.y-p.y)<200&&Distance(a->world.pos,p)<radius+90)safe=false;
        if(!safe)continue;
        auto* a=Actor_Spawn(&play->actorCtx,play,ACTOR_EN_MB,p.x,p.y,p.z,0,Math_Vec3f_Yaw(&p,&origin),0,variant?0:-1);
        if(a&&a->update)return a;
    }return nullptr;
}
// [TIME] | [INFO] | Creatures.HandmasterPractice | Keep floor families together and give the original ceiling attack enough clear height.
Actor* SpawnHandmasterPractice(PlayState* play,int variant) {
    if(!play||variant<0||variant>1)return nullptr;
    auto* player=GET_PLAYER(play);if(!player)return nullptr;
    auto origin=player->actor.world.pos;auto yaw=player->actor.shape.rot.y;
    int count=0;for(auto* a=play->actorCtx.actorLists[ACTORCAT_ENEMY].head;a;a=a->next)if(a->update&&(a->id==ACTOR_EN_FLOORMAS||a->id==ACTOR_EN_WALLMAS)&&Distance(a->world.pos,origin)<2000.f)++count;
    if(count+(variant?1:3)>6)return nullptr;
    const float radius=80.f,headroom=variant?400.f:140.f;
    for(float distance:std::initializer_list<float>{240.f,320.f,400.f})for(int offset:{0,-0x1800,0x1800}) {
        Vec3f p={origin.x+Math_SinS(yaw+offset)*distance,origin.y,origin.z+Math_CosS(yaw+offset)*distance};
        if(!Floor(play,&player->actor,p,50))continue;auto eye=origin;eye.y+=40;auto target=p;target.y+=70;auto high=p;high.y+=headroom;
        if(!Line(play,eye,target)||!Line(play,target,high))continue;bool safe=true;
        for(int i=0;i<12;++i){auto q=p;q.x+=Math_SinS(i*0x10000/12)*radius;q.z+=Math_CosS(i*0x10000/12)*radius;
            if(!Floor(play,&player->actor,q,20)){safe=false;break;}q.y+=70;if(!Line(play,target,q)){safe=false;break;}
            auto top=q;top.y=p.y+headroom;if(!Line(play,q,top)){safe=false;break;}}
        for(auto* a=play->actorCtx.actorLists[ACTORCAT_ENEMY].head;a;a=a->next)if(a->update&&std::abs(a->world.pos.y-p.y)<200&&Distance(a->world.pos,p)<radius+90)safe=false;
        if(!safe)continue;
        auto* a=Actor_Spawn(&play->actorCtx,play,variant?ACTOR_EN_WALLMAS:ACTOR_EN_FLOORMAS,p.x,p.y,p.z,0,Math_Vec3f_Yaw(&p,&origin),0,variant?1:0);
        if(a&&a->update)return a;
    }return nullptr;
}
// [TIME] | [INFO] | Creatures.StalchildPractice | Use the original field size progression and leave daylight burrowing under native control.
Actor* SpawnStalchildPractice(PlayState* play,int variant) {
    if(!play||variant<0||variant>2)return nullptr;
    auto* player=GET_PLAYER(play);if(!player)return nullptr;
    auto origin=player->actor.world.pos;auto yaw=player->actor.shape.rot.y;
    int count=0;for(auto* a=play->actorCtx.actorLists[ACTORCAT_ENEMY].head;a;a=a->next)if(a->update&&a->id==ACTOR_EN_SKB&&Distance(a->world.pos,origin)<2000.f)++count;
    if(count>=6)return nullptr;
    const float radius=65.f+variant*15.f,headroom=120.f+variant*35.f;
    for(float distance:std::initializer_list<float>{240.f,320.f,400.f})for(int offset:{0,-0x1800,0x1800}) {
        Vec3f p={origin.x+Math_SinS(yaw+offset)*distance,origin.y,origin.z+Math_CosS(yaw+offset)*distance};
        if(!Floor(play,&player->actor,p,50))continue;auto eye=origin;eye.y+=40;auto target=p;target.y+=70;auto high=p;high.y+=headroom;
        if(!Line(play,eye,target)||!Line(play,target,high))continue;bool safe=true;
        for(int i=0;i<12;++i){auto q=p;q.x+=Math_SinS(i*0x10000/12)*radius;q.z+=Math_CosS(i*0x10000/12)*radius;
            if(!Floor(play,&player->actor,q,20)){safe=false;break;}q.y+=70;if(!Line(play,target,q)){safe=false;break;}
            auto top=q;top.y=p.y+headroom;if(!Line(play,q,top)){safe=false;break;}}
        for(auto* a=play->actorCtx.actorLists[ACTORCAT_ENEMY].head;a;a=a->next)if(a->update&&std::abs(a->world.pos.y-p.y)<200&&Distance(a->world.pos,p)<radius+90)safe=false;
        if(!safe)continue;
        auto* a=Actor_Spawn(&play->actorCtx,play,ACTOR_EN_SKB,p.x,p.y,p.z,0,Math_Vec3f_Yaw(&p,&origin),0,variant*5);
        if(a&&a->update)return a;
    }return nullptr;
}
// [TIME] | [INFO] | Creatures.BeamosPractice | High-byte sight range must be nonzero; low byte selects the original small or large armored scanner.
Actor* SpawnBeamosPractice(PlayState* play,int variant) {
    if(!play||variant<0||variant>1)return nullptr;
    auto* player=GET_PLAYER(play);if(!player)return nullptr;
    auto origin=player->actor.world.pos;auto yaw=player->actor.shape.rot.y;
    int count=0;for(auto* a=play->actorCtx.actorLists[ACTORCAT_ENEMY].head;a;a=a->next)if(a->update&&a->id==ACTOR_EN_VM&&Distance(a->world.pos,origin)<2000.f)++count;
    if(count>=4)return nullptr;
    const float radius=60.f+variant*15.f,headroom=130.f+variant*35.f;
    for(float distance:std::initializer_list<float>{240.f,320.f,400.f})for(int offset:{0,-0x1800,0x1800}) {
        Vec3f p={origin.x+Math_SinS(yaw+offset)*distance,origin.y,origin.z+Math_CosS(yaw+offset)*distance};
        if(!Floor(play,&player->actor,p,50))continue;auto eye=origin;eye.y+=40;auto target=p;target.y+=70;auto high=p;high.y+=headroom;
        if(!Line(play,eye,target)||!Line(play,target,high))continue;bool safe=true;
        for(int i=0;i<12;++i){auto q=p;q.x+=Math_SinS(i*0x10000/12)*radius;q.z+=Math_CosS(i*0x10000/12)*radius;
            if(!Floor(play,&player->actor,q,20)){safe=false;break;}q.y+=70;if(!Line(play,target,q)){safe=false;break;}
            auto top=q;top.y=p.y+headroom;if(!Line(play,q,top)){safe=false;break;}}
        for(auto* a=play->actorCtx.actorLists[ACTORCAT_ENEMY].head;a;a=a->next)if(a->update&&std::abs(a->world.pos.y-p.y)<200&&Distance(a->world.pos,p)<radius+90)safe=false;
        if(!safe)continue;
        auto* a=Actor_Spawn(&play->actorCtx,play,ACTOR_EN_VM,p.x,p.y,p.z,0,Math_Vec3f_Yaw(&p,&origin),0,(10<<8)|(variant?0:1));
        if(a&&a->update)return a;
    }return nullptr;
}
// [TIME] | [INFO] | Creatures.GuayPractice | A native home orbit needs open air and a broad floor envelope; hidden respawning birds still count toward the four-actor limit.
Actor* SpawnGuayPractice(PlayState* play,bool giant) {
 if(!play)return nullptr;auto* player=GET_PLAYER(play);if(!player)return nullptr;auto origin=player->actor.world.pos;auto yaw=player->actor.shape.rot.y;
 int count=0;for(auto* a=play->actorCtx.actorLists[ACTORCAT_ENEMY].head;a;a=a->next)if(a->update&&a->id==ACTOR_EN_CROW&&Distance(a->home.pos,origin)<2000)++count;
 if(count>=4)return nullptr;
 for(float distance:{260.f,360.f})for(int offset:{0,-0x2000,0x2000}){
  Vec3f floor={origin.x+Math_SinS(yaw+offset)*distance,origin.y,origin.z+Math_CosS(yaw+offset)*distance};if(!Floor(play,&player->actor,floor,60))continue;
  auto p=floor;p.y+=100;auto eye=origin;eye.y+=45;auto top=floor;top.y+=330;if(!Line(play,eye,p)||!Line(play,p,top))continue;bool safe=true;
  for(int j=0;j<12;++j){auto q=floor;q.x+=Math_SinS(j*0x10000/12)*330;q.z+=Math_CosS(j*0x10000/12)*330;if(!Floor(play,&player->actor,q,45)){safe=false;break;}q.y=floor.y+100;auto high=q;high.y=floor.y+330;if(!Line(play,p,q)||!Line(play,q,high)){safe=false;break;}}
  for(auto* a=play->actorCtx.actorLists[ACTORCAT_ENEMY].head;a;a=a->next)if(a->update&&Distance(a->home.pos,p)<180)safe=false;
  if(!safe)continue;auto* actor=Actor_Spawn(&play->actorCtx,play,ACTOR_EN_CROW,p.x,p.y,p.z,0,Math_Vec3f_Yaw(&p,&origin),0,giant?1:0);
  if(actor&&actor->update){if(giant){Actor_SetScale(actor,.03f);reinterpret_cast<EnCrow*>(actor)->collider.elements[0].dim.worldSphere.radius=s16(20.0f*.03f*100.0f);}return actor;}
 }return nullptr;
}
// [TIME] | [INFO] | Creatures.PoePractice | Use original roaming Poe AI; count surviving lamps and souls toward the nearby light/actor cap.
Actor* SpawnPoePractice(PlayState* play) {
    if(!play)return nullptr;
    auto* player=GET_PLAYER(play);if(!player)return nullptr;
    auto origin=player->actor.world.pos;auto yaw=player->actor.shape.rot.y;
    int count=0;for(auto& list:play->actorCtx.actorLists)for(auto* a=list.head;a;a=a->next)if(a->update&&a->id==ACTOR_EN_POH&&Distance(a->world.pos,origin)<2000.f)++count;
    if(count>=4)return nullptr;
    const float radius=100.f,headroom=125.f;
    for(float distance:std::initializer_list<float>{220.f,260.f})for(int offset:{0,-0x1800,0x1800}) {
        Vec3f p={origin.x+Math_SinS(yaw+offset)*distance,origin.y,origin.z+Math_CosS(yaw+offset)*distance};
        if(!Floor(play,&player->actor,p,50))continue;auto eye=origin;eye.y+=40;auto target=p;target.y+=70;auto high=p;high.y+=headroom;
        if(!Line(play,eye,target)||!Line(play,target,high))continue;bool safe=true;
        for(int i=0;i<12;++i){auto q=p;q.x+=Math_SinS(i*0x10000/12)*radius;q.z+=Math_CosS(i*0x10000/12)*radius;
            if(!Floor(play,&player->actor,q,20)){safe=false;break;}q.y+=70;if(!Line(play,target,q)){safe=false;break;}
            auto top=q;top.y=p.y+headroom;if(!Line(play,q,top)){safe=false;break;}}
        for(auto* a=play->actorCtx.actorLists[ACTORCAT_ENEMY].head;a;a=a->next)if(a->update&&std::abs(a->world.pos.y-p.y)<200&&Distance(a->world.pos,p)<radius+90)safe=false;
        if(!safe)continue;
        auto* a=Actor_Spawn(&play->actorCtx,play,ACTOR_EN_POH,p.x,p.y,p.z,0,Math_Vec3f_Yaw(&p,&origin),0,0);
        if(a&&a->update)return a;
    }return nullptr;
}
// [TIME] | [INFO] | Creatures.AnubisPractice | Spawn the original mirroring controller on broad ground.
Actor* SpawnAnubisPractice(PlayState* play) {
    if(!play)return nullptr;
    auto* player=GET_PLAYER(play);if(!player)return nullptr;
    auto origin=player->actor.world.pos;auto yaw=player->actor.shape.rot.y;
    int count=0;for(auto* a=play->actorCtx.actorLists[ACTORCAT_SWITCH].head;a;a=a->next)if(a->update&&a->id==ACTOR_EN_ANUBICE_TAG&&Distance(a->world.pos,origin)<2000.f)++count;
    if(count>=4)return nullptr;
    const float radius=390.f,headroom=160.f;
    for(float distance:std::initializer_list<float>{180.f,240.f})for(int offset:{0,-0x1800,0x1800}) {
        Vec3f p={origin.x+Math_SinS(yaw+offset)*distance,origin.y,origin.z+Math_CosS(yaw+offset)*distance};
        if(!Floor(play,&player->actor,p,50))continue;auto eye=origin;eye.y+=40;auto target=p;target.y+=70;auto high=p;high.y+=headroom;
        if(!Line(play,eye,target)||!Line(play,target,high))continue;bool safe=true;
        for(int i=0;i<12;++i){auto q=p;q.x+=Math_SinS(i*0x10000/12)*radius;q.z+=Math_CosS(i*0x10000/12)*radius;
            if(!Floor(play,&player->actor,q,20)){safe=false;break;}q.y+=70;if(!Line(play,target,q)){safe=false;break;}
            auto top=q;top.y=p.y+headroom;if(!Line(play,q,top)){safe=false;break;}}
        for(auto* a=play->actorCtx.actorLists[ACTORCAT_ENEMY].head;a;a=a->next)if(a->update&&std::abs(a->world.pos.y-p.y)<200&&Distance(a->world.pos,p)<radius+90)safe=false;
        for(auto* a=play->actorCtx.actorLists[ACTORCAT_SWITCH].head;a;a=a->next)if(a->update&&a->id==ACTOR_EN_ANUBICE_TAG&&Distance(a->world.pos,p)<450.f)safe=false;
        if(!safe)continue;
        auto* a=Actor_Spawn(&play->actorCtx,play,ACTOR_EN_ANUBICE_TAG,p.x,p.y,p.z,0,Math_Vec3f_Yaw(&p,&origin),0,4);
        if(a&&a->update)return a;
    }return nullptr;
}
// [TIME] | [INFO] | Creatures.MadScrubPractice | Native high-byte shot count selects one seed or a three-seed volley. Count surviving flower props as well as live enemies.
Actor* SpawnMadScrubPractice(PlayState* play,int variant) {
    if(!play||variant<0||variant>1)return nullptr;
    auto* player=GET_PLAYER(play);if(!player)return nullptr;
    auto origin=player->actor.world.pos;auto yaw=player->actor.shape.rot.y;
    int count=0;for(auto* a=play->actorCtx.actorLists[ACTORCAT_ENEMY].head;a;a=a->next)if(a->update&&a->id==ACTOR_EN_DEKUNUTS&&a->params!=10&&Distance(a->world.pos,origin)<2000.f)++count;
    int flowers=0;for(auto& list:play->actorCtx.actorLists)for(auto* a=list.head;a;a=a->next)if(a->update&&a->id==ACTOR_EN_DEKUNUTS&&a->params==10&&Distance(a->world.pos,origin)<2000.f)++flowers;
    if(count>=4||flowers>=8)return nullptr;
    const float radius=65.f,headroom=100.f;
    for(float distance:std::initializer_list<float>{240.f,320.f,400.f})for(int offset:{0,-0x1800,0x1800}) {
        Vec3f p={origin.x+Math_SinS(yaw+offset)*distance,origin.y,origin.z+Math_CosS(yaw+offset)*distance};
        if(!Floor(play,&player->actor,p,50))continue;auto eye=origin;eye.y+=40;auto target=p;target.y+=70;auto high=p;high.y+=headroom;
        if(!Line(play,eye,target)||!Line(play,target,high))continue;bool safe=true;
        for(int i=0;i<12;++i){auto q=p;q.x+=Math_SinS(i*0x10000/12)*radius;q.z+=Math_CosS(i*0x10000/12)*radius;
            if(!Floor(play,&player->actor,q,20)){safe=false;break;}q.y+=70;if(!Line(play,target,q)){safe=false;break;}
            auto top=q;top.y=p.y+headroom;if(!Line(play,q,top)){safe=false;break;}}
        for(auto* a=play->actorCtx.actorLists[ACTORCAT_ENEMY].head;a;a=a->next)if(a->update&&std::abs(a->world.pos.y-p.y)<200&&Distance(a->world.pos,p)<radius+90)safe=false;
        if(!safe)continue;
        auto* a=Actor_Spawn(&play->actorCtx,play,ACTOR_EN_DEKUNUTS,p.x,p.y,p.z,0,Math_Vec3f_Yaw(&p,&origin),0,(variant?3:1)<<8);
        if(a&&a->update)return a;
    }return nullptr;
}
void DrawTesting() {
    static const char* result="";
    if(Theme::Section("Upgraded Keese")) {
        static int variant=0;
        ImGui::SetNextItemWidth(220);
        ImGui::Combo("Variant",&variant,"Normal\0Fire\0Ice\0");
        if(Theme::Button("Summon Keese",{220,38})) {
            auto* player=GET_PLAYER(gPlayState);auto p=player->actor.world.pos;auto yaw=player->actor.shape.rot.y;
            p.x+=Math_SinS(yaw)*170;p.z+=Math_CosS(yaw)*170;
            int count=0;for(auto* a=gPlayState->actorCtx.actorLists[ACTORCAT_ENEMY].head;a;a=a->next)if(a->id==ACTOR_EN_FIREFLY && a->update)++count;
            bool grounded=Floor(gPlayState,&player->actor,p);p.y+=75;
            auto eye=player->actor.world.pos;eye.y+=60;
            if(count<8 && grounded && Line(gPlayState,eye,p)) {
                auto* a=Actor_Spawn(&gPlayState->actorCtx,gPlayState,ACTOR_EN_FIREFLY,p.x,p.y,p.z,0,yaw+0x8000,0,variant==1?0:variant==2?4:2);
                result=a?"Keese summoned ahead. Resume to watch its native flight and dive attacks.":"Keese could not be summoned here.";
            } else result="Move into open space; at most eight Keese can be present.";
        }
    }
    if(Theme::Section("Upgraded Tektites")) {
        static int variant=0;
        ImGui::SetNextItemWidth(220);
        ImGui::Combo("Species",&variant,"Red Tektite\0Blue Tektite\0");
        if(Theme::Button("Summon Tektite",{220,38})) {
            auto* player=GET_PLAYER(gPlayState);auto p=player->actor.world.pos;auto yaw=player->actor.shape.rot.y;
            p.x+=Math_SinS(yaw)*180;p.z+=Math_CosS(yaw)*180;
            int count=0;for(auto* a=gPlayState->actorCtx.actorLists[ACTORCAT_ENEMY].head;a;a=a->next)if(a->id==ACTOR_EN_TITE && a->update)++count;
            bool grounded=Floor(gPlayState,&player->actor,p);auto eye=player->actor.world.pos;eye.y+=40;auto target=p;target.y+=30;
            if(count<8 && grounded && Line(gPlayState,eye,target)) {
                auto* a=Actor_Spawn(&gPlayState->actorCtx,gPlayState,ACTOR_EN_TITE,p.x,p.y,p.z,0,yaw+0x8000,0,variant?-2:-1);
                result=a?"Tektite summoned ahead. Resume to watch its crouch, jump and landing.":"Tektite could not be summoned here.";
            } else result="Move into open space; at most eight Tektites can be present.";
        }
    }
    if(Theme::Section("Upgraded Deku Babas")) {
        static int variant=0;
        ImGui::SetNextItemWidth(220);
        ImGui::Combo("Plant",&variant,"Deku Baba\0Big Deku Baba\0");
        if(Theme::Button("Summon Deku Baba",{220,38})) {
            auto* player=GET_PLAYER(gPlayState);auto p=player->actor.world.pos;auto yaw=player->actor.shape.rot.y;
            p.x+=Math_SinS(yaw)*(variant?300:160);p.z+=Math_CosS(yaw)*(variant?300:160);
            int count=0;for(auto* a=gPlayState->actorCtx.actorLists[ACTORCAT_ENEMY].head;a;a=a->next)if(a->id==ACTOR_EN_DEKUBABA && a->update)++count;
            bool grounded=Floor(gPlayState,&player->actor,p);auto eye=player->actor.world.pos;eye.y+=40;auto target=p;target.y+=30;
            if(count<6 && grounded && Line(gPlayState,eye,target)) {
                auto* a=Actor_Spawn(&gPlayState->actorCtx,gPlayState,ACTOR_EN_DEKUBABA,p.x,p.y,p.z,0,yaw+0x8000,0,variant);
                result=a?"Deku Baba rooted ahead. Resume and approach to trigger its native lunge.":"Deku Baba could not be summoned here.";
            } else result="Move into open space; at most six Deku Babas can be present.";
        }
    }
    if(Theme::Section("Upgraded Octoroks")) {
        Theme::Muted("Face open water from shore. Keep your distance; reflect its shot with a shield.");
        if(Theme::Button("Summon Octorok",{220,38})) {
            auto* player=GET_PLAYER(gPlayState);auto origin=player->actor.world.pos;auto yaw=player->actor.shape.rot.y;
            int nearby=0,total=0;
            // [TIME] | [INFO] | CreatureSystem.OctorokPractice | Native inhabitants elsewhere in the river must not exhaust the nearby practice cap.
            for(auto* a=gPlayState->actorCtx.actorLists[ACTORCAT_ENEMY].head;a;a=a->next)if(a->id==ACTOR_EN_OKUTA && a->params==0 && a->update){++total;if(Distance(a->world.pos,origin)<700)++nearby;}
            bool spawned=false;
            for(float distance:{300.f,240.f,380.f})for(int offset:{0,-0x1000,0x1000}) {
                if(spawned || nearby>=4 || total>=16)continue;
                Vec3f p={origin.x+Math_SinS(yaw+offset)*distance,origin.y+200,origin.z+Math_CosS(yaw+offset)*distance};
                float surface=0;WaterBox* box=nullptr;CollisionPoly* poly=nullptr;
                if(!WaterBox_GetSurfaceImpl(gPlayState,&gPlayState->colCtx,p.x,p.z,&surface,&box))continue;
                p.y=surface+100;float floor=BgCheck_EntityRaycastFloor1(&gPlayState->colCtx,&poly,&p);
                if(floor<=BGCHECK_Y_MIN || surface-floor<40)continue;
                auto eye=origin;eye.y+=40;auto target=p;target.y=surface+30;
                if(!Line(gPlayState,eye,target))continue;
                p.y=surface;
                auto* a=Actor_Spawn(&gPlayState->actorCtx,gPlayState,ACTOR_EN_OKUTA,p.x,p.y,p.z,0,yaw+0x8000,0,0);
                spawned=a && a->update;
            }
            result=spawned?"Octorok summoned in the water. Resume and raise your shield.":(nearby>=4 || total>=16)?"This area already has enough Octoroks for practice.":"Face nearby deep water from shore with a clear view of the surface.";
        }
    }
    if(Theme::Section("Upgraded Skulltulas")) {
        static int variant=0;
        Theme::Muted("Find a clear room beneath a ceiling. Circle past the skull to strike its back.");
        ImGui::SetNextItemWidth(220);ImGui::Combo("Spider",&variant,"Skulltula\0Big Skulltula\0Invisible Skulltula\0");
        if(variant==2)Theme::Muted("Use the Lens of Truth to reveal this variant.");
        if(Theme::Button("Summon Skulltula",{220,38}))result=SpawnSkulltulaPractice(gPlayState,variant)?"Skulltula hanging ahead. Resume and approach.":"Find a clear floor beneath a ceiling; at most eight Skulltulas can be present.";
    }
    if(Theme::Section("Upgraded Bubbles")) {
        static int variant=0;
        ImGui::SetNextItemWidth(220);ImGui::Combo("Flying skull",&variant,"Blue Bubble\0Red Bubble\0Green Bubble\0");
        Theme::Muted(variant==2?"Strike when its green flame goes out.":"Deflect its charge to extinguish the flame, then attack.");
        if(Theme::Button("Summon Bubble",{220,38}))result=SpawnBubblePractice(gPlayState,variant)?"Bubble summoned ahead. Resume to face its native attack.":"Find open space over clear floor; at most six Bubbles can be present.";
        Theme::Muted("White and great green Bubbles follow their original scene routes.");
    }
    if(Theme::Section("Upgraded Dodongos")) {
        Theme::Muted("Circle its armored head. Strike the tail, or feed it an explosive.");
        if(Theme::Button("Summon Dodongo",{220,38}))result=SpawnDodongoPractice(gPlayState)?"Dodongo placed ahead. Resume to face its native attacks.":"Find a wide, level clearing; at most four Dodongos can be present.";
    }
    if(Theme::Section("Upgraded ReDeads & Gibdos")) {
        static int variant=0;
        ImGui::SetNextItemWidth(220);ImGui::Combo("Undead",&variant,"ReDead\0Crouching ReDead\0Gibdo\0Rising Gibdo\0");
        Theme::Muted("Your footsteps wake them. Sun's Song and the Hookshot create an opening.");
        if(Theme::Button("Summon undead",{220,38}))result=SpawnRedeadPractice(gPlayState,variant)?"Undead placed ahead. Resume and approach.":"Find open, level floor; at most four undead can be present.";
    }
    if(Theme::Section("Upgraded Iron Knuckles")) {
        static int variant=0;
        ImGui::SetNextItemWidth(220);ImGui::Combo("Armor",&variant,"Ivory\0Black\0Silver\0");
        Theme::Muted("Strike to wake it. Bait its axe, then punish the recovery. Broken armor means faster pursuit.");
        if(Theme::Button("Summon Iron Knuckle",{260,38}))result=SpawnIronKnucklePractice(gPlayState,variant)?"Iron Knuckle placed ahead. Resume and strike its armor.":"Find a wide clearing in an uncleared room; at most three Iron Knuckles can be present.";
    }
    if(Theme::Section("Upgraded Stingers")) {
        static int variant=0;
        ImGui::SetNextItemWidth(220);ImGui::Combo("Habitat",&variant,"Land wanderer\0Circling decoys\0Water swimmer\0");
        Theme::Muted(variant==2?"Face deep water. Draw it into a leap, then strike during recovery.":"Approach the moving fin to draw it out. Nuts and the boomerang interrupt its flight.");
        if(Theme::Button("Summon Stinger",{240,38}))result=SpawnStingerPractice(gPlayState,variant)?"Stinger placed ahead. Resume and approach.":variant==2?"Face open, deep water; at most eight Stingers including decoys can be present.":"Find open, level ground; the decoy circle needs extra room. Limit: eight including decoys.";
    }
    if(Theme::Section("Upgraded Shell Blades")) {
        static int habitat=0;ImGui::SetNextItemWidth(220);ImGui::Combo("Placement",&habitat,"On land\0Under water\0");
        Theme::Muted("Bait the opening. Aim into the mouth; the closed shell deflects ordinary hits.");
        if(Theme::Button("Summon Shell Blade",{260,38}))result=SpawnShellbladePractice(gPlayState,habitat)?"Shell Blade placed ahead. Resume and approach.":habitat?"Face clear, deep water. Limit: six Shell Blades.":"Find open, level ground. Limit: six Shell Blades.";
    }
    if(Theme::Section("Upgraded Armos")) {
        static int variant=0;ImGui::SetNextItemWidth(220);ImGui::Combo("Armos type",&variant,"Awakening guard\0Pushable statue\0");
        if(Theme::Button("Summon Armos",{240,38}))result=SpawnArmosPractice(gPlayState,variant)?"Armos placed ahead. Resume to test.":"Find open, level ground in an uncleared room. Limit: four Armos including statues.";
        Theme::Muted(variant?"A passive stone statue; retains native pushing and switch collision.":"Bait three hops. Stun, then strike; retreat when it begins to spin.");
    }

    if(Theme::Section("Upgraded Peahats")) {
        static int variant=0;ImGui::SetNextItemWidth(220);ImGui::Combo("Peahat type",&variant,"Hunting adult\0Brood parent\0Larva\0");
        if(Theme::Button("Summon Peahat",{240,38}))result=SpawnPeahatPractice(gPlayState,variant)?"Peahat placed ahead. Resume to test.":"Find broad open ground, such as Hyrule Field. Nearby limit: four adults or ten larvae.";
        Theme::Muted(variant==0?"Adults rise by day. Slip below the blades and strike the exposed root.":variant==1?"The high-flying parent releases up to three larvae. The brood remains capped after landing.":"Raise your shield to repel its charge; an ordinary hit also stops a larva.");
    }
    if(Theme::Section("Upgraded Guays")) {
        if(Theme::Button("Summon Guay",{240,38}))result=SpawnGuayPractice(gPlayState)?"Guay circling ahead.":"Find broad, open ground. Nearby limit: four Guays.";
        if(Theme::Button("Summon giant Guay",{300,38}))result=SpawnGuayPractice(gPlayState,true)?"Giant Guay circling ahead.":"Find broad, open ground. Nearby limit: four Guays.";
        ImGui::TextWrapped("Watch its circling flight, then meet the dive with a shield or ranged shot. Deku nuts drive it away.");
    }
    if(Theme::Section("Upgraded Poes")) {
        if(Theme::Button("Summon Poe",{240,38}))result=SpawnPoePractice(gPlayState)?"Poe summoned ahead. Resume to approach.":"Find clear, level ground. Nearby limit: four Poes, lamps or souls.";
        ImGui::TextWrapped("Watch its lantern windup. Prolonged targeting makes it vanish; strike when it returns.");
    }
    if(Theme::Section("Upgraded Anubis")) {
        if(Theme::Button("Summon Anubis",{240,38}))result=SpawnAnubisPractice(gPlayState)?"Anubis mirror anchor placed ahead. Resume and circle it.":"Find broad, level ground. Nearby limit: four mirror anchors.";
        ImGui::TextWrapped("It mirrors your movement and answers sword swings with fire. Use fire arrows or Din's Fire to defeat it.");
    }
    if(Theme::Section("Upgraded Mad Scrubs")) {
        static int variant=0;ImGui::SetNextItemWidth(220);ImGui::Combo("Seed pattern",&variant,"Single shot\0Three-shot volley\0");
        if(Theme::Button("Summon Mad Scrub",{240,38}))result=SpawnMadScrubPractice(gPlayState,variant)?"Mad Scrub placed ahead. Resume to test.":"Find clear, level ground. Limit: four Scrubs or eight burrows nearby.";
        ImGui::TextWrapped("Reflect a seed with your shield to flush it out. Chase it down before it burrows again.");
    }
    if(Theme::Section("Upgraded Beamos")) {
        static int variant=0;ImGui::SetNextItemWidth(220);ImGui::Combo("Beamos size",&variant,"Small\0Large\0");
        if(Theme::Button("Summon Beamos",{240,38}))result=SpawnBeamosPractice(gPlayState,variant)?"Beamos placed ahead. Resume to test.":"Find clear, level ground. Nearby limit: four Beamos.";
        ImGui::TextWrapped("Shoot the eye to interrupt its laser. Bombs break the armored body.");
    }
    if(Theme::Section("Upgraded Stalchildren")) {
        static int variant=0;ImGui::SetNextItemWidth(220);ImGui::Combo("Size##stalchild",&variant,"Regular\0Large\0Giant\0");
        if(Theme::Button("Summon Stalchild",{240,38}))result=SpawnStalchildPractice(gPlayState,variant)?"Stalchild placed ahead. Fight at night; daylight sends it underground.":"Find clear, level ground. Nearby limit: six Stalchildren.";
        if(Theme::Button("Set night",{150,32})&&gPlayState){gSaveContext.dayTime=gSaveContext.skyboxTime=0;gSaveContext.nightFlag=1;result="Night set for Stalchild practice.";}
        ImGui::TextWrapped("Block the swipe, then counter. Spin attacks can break its head; the body keeps fighting.");
    }
    if(Theme::Section("Upgraded handmasters")) {
        static int variant=0;ImGui::SetNextItemWidth(220);ImGui::Combo("Hand type",&variant,"Floormaster\0Wallmaster\0");
        if(Theme::Button("Summon handmaster",{240,38}))result=SpawnHandmasterPractice(gPlayState,variant)?"Handmaster placed ahead. Resume and approach.":"Find clear, level ground with enough headroom. Nearby limit: six hands including hidden siblings.";
        Theme::Muted(variant?"Watch the shadow and sidestep the drop. A catch returns you to the entrance.":"Avoid its armored lunge. Defeat the three small hands before they reunite.");
    }
    if(Theme::Section("Upgraded Moblins")) {
        static int variant=0;ImGui::SetNextItemWidth(220);ImGui::Combo("Moblin type",&variant,"Spear guard\0Club fighter\0");
        if(Theme::Button("Summon Moblin",{240,38}))result=SpawnMoblinPractice(gPlayState,variant)?"Moblin placed ahead. Resume to test.":"Find a broad, level clearing. Nearby limit: four Moblins.";
        Theme::Muted(variant?"Watch which lane the club targets. Dodge the slam and punish its recovery.":"Sidestep its spear charge. Its front deflects attacks; circle behind.");
    }
    if(Theme::Section("Experimental creatures")) {
        if(Theme::Button("Summon Cairn Sentinel",{260,38})) {
            auto p=GET_PLAYER(gPlayState)->actor.world.pos;auto yaw=GET_PLAYER(gPlayState)->actor.shape.rot.y;p.x+=Math_SinS(yaw)*170;p.z+=Math_CosS(yaw)*170;
            result=Spawn(gPlayState,Kind::Sentinel,p)?"Sentinel summoned ahead.":"No safe floor ahead, or new creatures are disabled.";
        }
        ImGui::SameLine();if(Theme::Button("Summon Lantern Wraith",{260,38})) {
            auto p=GET_PLAYER(gPlayState)->actor.world.pos;auto yaw=GET_PLAYER(gPlayState)->actor.shape.rot.y;p.x+=Math_SinS(yaw)*220;p.z+=Math_CosS(yaw)*220;
            result=Spawn(gPlayState,Kind::Wraith,p)?"Wraith summoned ahead.":"No safe floor ahead, or new creatures are disabled.";
        }
        if(Theme::Button("Summon lantern escort",{260,38})) {
            auto p=GET_PLAYER(gPlayState)->actor.world.pos;auto yaw=GET_PLAYER(gPlayState)->actor.shape.rot.y;
            auto front=p;front.x+=Math_SinS(yaw)*190;front.z+=Math_CosS(yaw)*190;
            auto rear=p;rear.x+=Math_SinS(yaw)*310+Math_CosS(yaw)*80;rear.z+=Math_CosS(yaw)*310-Math_SinS(yaw)*80;
            auto* sentinel=Spawn(gPlayState,Kind::Sentinel,front);auto* wraith=Spawn(gPlayState,Kind::Wraith,rear);
            if(sentinel && wraith) {
                sentinel->colChkInfo.health=std::max(1,GetActorMaximumHealth(sentinel)/2);
                result="A wounded Sentinel and its Wraith escort. Watch for the green healing tether.";
            } else {
                if(sentinel)Actor_Kill(sentinel);if(wraith)Actor_Kill(wraith);
                result="The escort needs more safe floor ahead.";
            }
        }
        ImGui::SameLine();
        if(Theme::Button("Clear new creatures",{260,38}))for(int cat:{ACTORCAT_ENEMY,ACTORCAT_ITEMACTION})for(auto* a=gPlayState->actorCtx.actorLists[cat].head;a;a=a->next)if(IsCreature(a) || a->id==projectileId)Actor_Kill(a);
    }
    if(*result)ImGui::TextWrapped("%s",result);
    Brood::DrawTesting();
}
static void Register() {
    COND_HOOK(OnPlayerUpdate,true,[](){
        if(!gPlayState)return;
        for(auto* a=gPlayState->actorCtx.actorLists[ACTORCAT_ENEMY].head;a;a=a->next)if(auto* t=ObjectExtension::GetInstance().Get<Technique>(a)) {
            if(!Enabled()){t->state=State{};t->rally=0;continue;}if(!Active(gPlayState))continue;
            if(t->rally>0)--t->rally;if(t->state.cooldown>0)--t->state.cooldown;
            if(t->state.phase==Phase::Windup && gPlayState->gameplayFrames-t->serviced>2){t->state.phase=Phase::Roam;t->state.cooldown=100;}
            if((t->state.phase==Phase::Stagger || t->state.phase==Phase::Recover) && --t->state.timer<=0)t->state.phase=Phase::Roam;
        }
    });
    COND_HOOK(OnPlayDrawEnd,true,[](){
        if(!gPlayState || !Enabled())return;
        for(auto* a=gPlayState->actorCtx.actorLists[ACTORCAT_ENEMY].head;a;a=a->next)if(auto* t=ObjectExtension::GetInstance().Get<Technique>(a);t && a->update && a->colChkInfo.health && a->xzDistToPlayer<900) {
            if(t->state.phase==Phase::Windup)DrawWarning(gPlayState,a,30+t->state.timer*.7f,3,.8f,a->id==ACTOR_EN_WF);
            else if(t->rally>0)DrawWarning(gPlayState,a,25,3,.4f,true);
        }
    });
}
static RegisterShipInitFunc init(Register);
}
