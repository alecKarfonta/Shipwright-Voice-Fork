#include "NativeEffects.h"
#include "AdventureRelics.h"
#include "AdventureItemRules.h"
#include "ARPG.h"
#include "ARPGSave.h"
#include "ArsenalSystem.h"
#include "CreatureSystem.h"
#include "EnemySystem.h"
#include "ExpeditionSystem.h"
#include "InventoryRules.h"
#include "ItemModifierSystem.h"
#include "SkillCombat.h"
#include "soh/ShipInit.hpp"
#include "soh/ObjectExtension/ObjectExtension.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/EnemyDifficulty/EnemyDifficulty.h"
#include "soh/frame_interpolation.h"
#include <array>
#include <cmath>
#include <cstring>
extern "C" {
#include "functions.h"
#include "variables.h"
#include "macros.h"
#include "overlays/actors/ovl_En_M_Thunder/z_en_m_thunder.h"
#include "overlays/actors/ovl_En_Boom/z_en_boom.h"
#include "overlays/actors/ovl_En_Bom_Chu/z_en_bom_chu.h"
#include "overlays/actors/ovl_En_Arrow/z_en_arrow.h"
extern PlayState* gPlayState;
}
namespace ARPG::Relics {
namespace {
enum class Kind { Ice, Cable, Blast, Fire, Beam, Gust, Rain, Wisp, Thorns, Anchor };
struct Target {
    uint32_t id=0, lastContact=UINT32_MAX, pointFrame=UINT32_MAX;
    Actor* pointSource=nullptr;
    Vec3f point{};
    int wet=0, weakRecovery=0, pull=0;
    bool heavy=false;
};
struct Projectile { uint32_t orbitTarget=0; int orbit=0; bool used=false; };
struct Effect {
    Kind kind=Kind::Ice; int age=0, duration=16, delay=0, count=1; float radius=70, power=1;
    Vec3f origin{},end{},velocity{};
    std::array<ColliderCylinder,8> colliders{};
    std::array<uint32_t,32> hits{};
    uint32_t exclude=0; int hitCount=0;
};
static ObjectExtension::Register<Target> targetRegistration;
static ObjectExtension::Register<Projectile> projectileRegistration;
static ObjectExtension::Register<Effect> effectRegistration;
Preparation prep;
uint32_t nextIdentity=1;
bool rainPending=false;
int pendingAge=0;
bool cinderReady=false,guardHeld=false;
uint64_t loadout=0;
Vec3f lastPlayerPosition{};
bool Active() { return ItemsEnabled() && gPlayState && gSaveContext.health>0 && gPlayState->transitionTrigger==TRANS_TRIGGER_OFF; }
bool Playing() { return Active() && !GameInteractor::IsGameplayPaused() && GameInteractor::IsPlayerInControl(); }
Target* Data(Actor* actor) {
    auto& ext=ObjectExtension::GetInstance();
    if(!ext.Has<Target>(actor)) {Target t;t.id=nextIdentity++;if(!nextIdentity)++nextIdentity;ext.Set<Target>(actor,std::move(t));}
    return ext.Get<Target>(actor);
}
Projectile* Shot(Actor* actor) {
    auto& ext=ObjectExtension::GetInstance();if(!ext.Has<Projectile>(actor))ext.Set<Projectile>(actor,Projectile{});return ext.Get<Projectile>(actor);
}
bool Enemy(Actor* a) { return a && a->update && a->colChkInfo.health && EnemyDifficulty::IsEnemyActor(a) && !Enemies::IsWatchman(a) && a->category!=ACTORCAT_BOSS; }
bool Movable(Actor* a) { return Enemy(a) && (Arsenal::FamilyFor(a)!=Arsenal::Family::Other || Creatures::IsCreature(a)); }
void EachEnemy(const auto& f) { if(gPlayState)for(auto* a=gPlayState->actorCtx.actorLists[ACTORCAT_ENEMY].head;a;a=a->next)if(Enemy(a))f(a); }
Actor* Find(uint32_t id) { Actor* found=nullptr;if(id)EachEnemy([&](Actor* a){auto* d=ObjectExtension::GetInstance().Get<Target>(a);if(d && d->id==id)found=a;});return found; }
const ARPGItemInstance* Equipped(int slot,int id=0) {
    if(!Active())return nullptr;
    const auto& item=InventoryRules::Equipped(gARPGSave,slot);
    return InventoryRules::Valid(item) && ItemModifierSystem::IsEquipped(item) && (!id || AdventureItems::IsUnique(&item,id))?&item:nullptr;
}
float Radius(const ARPGItemInstance* item,float base) { return ToolRadius(item?Loot::Power(*item,ARPG_MOD_REACH):0,base); }
float Distance(Vec3f a,Vec3f b) { return std::sqrt(SQ(a.x-b.x)+SQ(a.y-b.y)+SQ(a.z-b.z)); }
bool Clear(Vec3f a,Vec3f b,float lift=20) {
    a.y+=lift;b.y+=lift;Vec3f hit{};CollisionPoly* poly=nullptr;int bg=BGCHECK_SCENE;
    return !BgCheck_EntityLineTest1(&gPlayState->colCtx,&a,&b,&hit,&poly,true,true,true,true,&bg);
}
bool Step(Actor* a,Vec3f toward,float amount,bool away=false) {
    if(!Movable(a) || !(a->bgCheckFlags&BGCHECKFLAG_GROUND))return false;
    Vec3f p=a->world.pos;float dx=toward.x-p.x,dz=toward.z-p.z,length=std::sqrt(dx*dx+dz*dz);
    if(length<1 || (!away && length<65))return false;
    float scale=(away?amount:std::min(amount,std::max(0.f,length-60)))/length*(away?-1:1);p.x+=dx*scale;p.z+=dz*scale;
    if(!BgCheck_PosInStaticBoundingBox(&gPlayState->colCtx,&p) || !Clear(a->world.pos,p))return false;
    Vec3f ray=p;ray.y+=50;CollisionPoly* poly=nullptr;int bg=BGCHECK_SCENE;
    float floor=BgCheck_EntityRaycastFloor5(gPlayState,&gPlayState->colCtx,&poly,&bg,a,&ray);
    if(!poly || bg!=BGCHECK_SCENE || poly->normal.y<24000 || floor<=BGCHECK_Y_MIN || std::abs(floor-p.y)>25 ||
       SurfaceType_GetSceneExitIndex(&gPlayState->colCtx,poly,bg) || SurfaceType_IsWallDamage(&gPlayState->colCtx,poly,bg))return false;
    auto type=SurfaceType_GetFloorType(&gPlayState->colCtx,poly,bg);auto prop=func_80041EA4(&gPlayState->colCtx,poly,bg);
    if(type==2 || type==3 || type==7 || type==9 || type==12 || prop==FLOOR_PROPERTY_5 || prop==FLOOR_PROPERTY_12)return false;
    float water=0;WaterBox* box=nullptr;if(WaterBox_GetSurface1(gPlayState,&gPlayState->colCtx,p.x,p.z,&water,&box) && water>floor+5)return false;
    p.y=floor;a->world.pos=p;return true;
}
Vec3f WeakPoint(Actor* target) {
    float phase=gPlayState->gameplayFrames*.055f+Data(target)->id;
    float facing=Actor_WorldYawTowardActor(target,&GET_PLAYER(gPlayState)->actor)*(3.14159265f/32768);
    Vec3f p=target->world.pos;
    float height=std::clamp(target->focus.pos.y-p.y,25.f,75.f);
    p.x+=std::sin(facing)*18+std::cos(facing)*std::sin(phase)*13;
    p.z+=std::cos(facing)*18-std::sin(facing)*std::sin(phase)*13;p.y+=height*.65f+std::cos(phase)*8;
    return p;
}
// Small, native-rendered geometry keeps the silhouettes readable without new texture archives.
struct Mesh {
    std::array<Vtx,720> v{};int count=0;
    void Tri(Vec3f a,Vec3f b,Vec3f c,std::array<uint8_t,4> color) {
        if(count+3>int(v.size()))return;
        for(auto p:{a,b,c}) {auto& q=v[count++].v;q.ob[0]=p.x*4;q.ob[1]=p.y*4;q.ob[2]=p.z*4;std::memcpy(q.cn,color.data(),4);}
    }
    void Line(Vec3f a,Vec3f b,float w,std::array<uint8_t,4> color) {
        Tri({a.x-w,a.y,a.z},{a.x+w,a.y,a.z},{b.x+w,b.y,b.z},color);Tri({a.x-w,a.y,a.z},{b.x+w,b.y,b.z},{b.x-w,b.y,b.z},color);
    }
    void Ring(float r,float y,std::array<uint8_t,4> color) {
        for(int i=0;i<24;++i) {float a=i*.2617994f,b=(i+1)*.2617994f;Tri({std::sin(a)*r,y,std::cos(a)*r},{std::sin(b)*r,y,std::cos(b)*r},{std::sin(b)*(r-3),y,std::cos(b)*(r-3)},color);Tri({std::sin(a)*r,y,std::cos(a)*r},{std::sin(b)*(r-3),y,std::cos(b)*(r-3)},{std::sin(a)*(r-3),y,std::cos(a)*(r-3)},color);}
    }
};
void DrawMesh(Mesh& mesh,Vec3f p,const void* identity,int part=91) {
    if(!mesh.count)return;auto* play=gPlayState;GraphicsContext* __gfxCtx=play->state.gfxCtx;
    auto* v=static_cast<Vtx*>(Graph_Alloc(__gfxCtx,sizeof(Vtx)*mesh.count));if(!v)return;std::memcpy(v,mesh.v.data(),sizeof(Vtx)*mesh.count);
    FrameInterpolation_RecordOpenChild(identity,part);Gfx_SetupDL_25Xlu(__gfxCtx);gDPPipeSync(POLY_XLU_DISP++);
    gSPClearGeometryMode(POLY_XLU_DISP++,G_LIGHTING|G_CULL_BOTH|G_TEXTURE_GEN|G_TEXTURE_GEN_LINEAR|G_FOG);
    gSPSetGeometryMode(POLY_XLU_DISP++,G_ZBUFFER|G_SHADE|G_SHADING_SMOOTH);
    gDPSetCycleType(POLY_XLU_DISP++,G_CYC_1CYCLE);gDPSetRenderMode(POLY_XLU_DISP++,G_RM_AA_ZB_XLU_SURF,G_RM_AA_ZB_XLU_SURF2);
    NativeEffects::Feather(v,mesh.count,.72f);
    POLY_XLU_DISP=NativeEffects::Bind(POLY_XLU_DISP,NativeEffects::Texture::Trail,play->gameplayFrames);
    Matrix_Push();Matrix_Translate(p.x,p.y,p.z,MTXMODE_NEW);Matrix_Scale(.25f,.25f,.25f,MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++,MATRIX_NEWMTX(__gfxCtx),G_MTX_NOPUSH|G_MTX_LOAD|G_MTX_MODELVIEW);
    for(int i=0;i<mesh.count;i+=24){int n=std::min(24,mesh.count-i);gSPVertex(POLY_XLU_DISP++,reinterpret_cast<uintptr_t>(v+i),n,0);for(int j=0;j<n;j+=3)gSP1Triangle(POLY_XLU_DISP++,j,j+1,j+2,0);}
    Matrix_Pop();FrameInterpolation_RecordCloseChild();
}
void DrawEffect(Actor* actor,PlayState*) {
    auto* fx=ObjectExtension::GetInstance().Get<Effect>(actor);if(!fx || !Active() || fx->age<fx->delay)return;
    using namespace NativeEffects;
    NativeEffects::Mesh m(gPlayState);int age=fx->age-fx->delay;
    float fade=std::clamp((fx->duration-fx->age)/6.f,0.f,1.f);
    if(fx->kind==Kind::Cable) {
        Point end{fx->end.x-fx->origin.x,fx->end.y-fx->origin.y+25,fx->end.z-fx->origin.z};
        m.Line({0,25,0},end,5,Fade(ice,fade),Texture::Lightning);
    } else if(fx->kind==Kind::Rain) {
        for(int i=0;i<20;++i) {
            float a=i*2.4f,r=std::sqrt((i+.5f)/20)*fx->radius,y=140-std::fmod(age*9.f+i*23.f,130.f);
            Point p{std::sin(a)*r,y,std::cos(a)*r};m.Line(p,{p.x-2,p.y-14,p.z},.8f,Fade(ice,fade*.65f));
        }
    } else if(fx->kind==Kind::Beam) {
        m.Line({-fx->velocity.x,-fx->velocity.y,-fx->velocity.z},{fx->velocity.x*.5f,fx->velocity.y*.5f,fx->velocity.z*.5f},4,Fade(gold,fade));
        m.Sprite(Texture::Glow,{0,0,0},9,9,Fade(gold,fade));
    } else if(fx->kind==Kind::Wisp) {
        m.Sprite(Texture::Fire,{0,12,0},8,16,Fade(ice,fade));
    } else {
        bool frozen=fx->kind==Kind::Ice,gust=fx->kind==Kind::Gust,thorn=fx->kind==Kind::Thorns;
        auto color=frozen?ice:gust?steel:thorn?green:fire;
        float r=fx->radius*std::min(1.f,(age+2)/8.f);
        m.Arc(r,6,3,0,2*pi,Fade(color,fade*.7f));
        for(int i=0;i<7;++i) {
            float a=i*2.4f;Point p{std::sin(a)*r*.75f,10.f+age*.65f,std::cos(a)*r*.75f};
            m.Sprite(frozen || gust || thorn?Texture::Dust:Texture::Fire,p,9,14,Fade(color,fade*.75f));
        }
    }
    m.Draw(gPlayState,actor->world.pos,0,actor);

}
void DestroyEffect(Actor* actor,PlayState* play) {
    if(auto* fx=ObjectExtension::GetInstance().Get<Effect>(actor))for(auto& c:fx->colliders)Collider_DestroyCylinder(play,&c);
    auto* native=reinterpret_cast<EnMThunder*>(actor);Collider_DestroyCylinder(play,&native->collider);LightContext_RemoveLight(play,&play->lightCtx,native->lightNode);
}
Actor* Spawn(Kind kind,Vec3f origin,float radius,int duration=16,Actor* exclude=nullptr,Vec3f end={},int delay=0);
void UpdateEffect(Actor* actor,PlayState* play) {
    auto* fx=ObjectExtension::GetInstance().Get<Effect>(actor);
    if(!fx || !Active() || fx->age>=fx->duration) {Actor_Kill(actor);return;}
    if(!Playing())return;
    if(fx->kind==Kind::Rain) {
        EachEnemy([&](Actor* a){if(Distance(a->world.pos,fx->origin)<fx->radius && Clear(fx->origin,a->world.pos))Data(a)->wet=40;});
    } else if(fx->kind==Kind::Wisp) {
        Actor* target=nullptr;float closest=fx->radius;
        EachEnemy([&](Actor* a){float d=Distance(a->focus.pos,actor->world.pos);if(Arsenal::GetStatus(a).marked && d<closest && Clear(actor->world.pos,a->focus.pos,0)){target=a;closest=d;}});
        if(target) {
            Vec3f p=target->focus.pos;float d=Distance(p,actor->world.pos);
            if(d<30){Spawn(Kind::Fire,actor->world.pos,85);Actor_Kill(actor);return;}
            float f=std::min(1.f,7.f/std::max(d,1.f));actor->world.pos.x+=(p.x-actor->world.pos.x)*f;actor->world.pos.y+=(p.y-actor->world.pos.y)*f;actor->world.pos.z+=(p.z-actor->world.pos.z)*f;
        }
    } else if(fx->kind==Kind::Beam) {
        Vec3f p{actor->world.pos.x+fx->velocity.x,actor->world.pos.y+fx->velocity.y,actor->world.pos.z+fx->velocity.z};
        if(!Clear(actor->world.pos,p,0)){Actor_Kill(actor);return;}actor->world.pos=p;
    }
    if(fx->kind!=Kind::Rain && fx->kind!=Kind::Wisp && fx->age>=fx->delay && (fx->kind==Kind::Beam || fx->age<fx->delay+4)) {
        for(int i=0;i<fx->count;++i) {
            auto& c=fx->colliders[i];Vec3f p=actor->world.pos;
            if(fx->kind==Kind::Cable){float f=(i+.5f)/fx->count;p={fx->origin.x+(fx->end.x-fx->origin.x)*f,fx->origin.y+(fx->end.y-fx->origin.y)*f,fx->origin.z+(fx->end.z-fx->origin.z)*f};}
            c.dim.pos={s16(p.x),s16(p.y),s16(p.z)};c.base.atFlags&=~(AT_HIT|AT_BOUNCED);c.info.toucherFlags&=~(TOUCH_HIT|TOUCH_DREW_HITMARK);CollisionCheck_SetAT(play,&play->colChkCtx,&c.base);
        }
        if(fx->age==fx->delay && (fx->kind==Kind::Blast || fx->kind==Kind::Fire)) {Vec3f v{},a{0,.2f,0};EffectSsBomb2_SpawnLayered(play,&actor->world.pos,&v,&a,70,12);Audio_PlayActorSound2(actor,NA_SE_IT_BOMB_EXPLOSION);}
    }
    ++fx->age;
}
Actor* Spawn(Kind kind,Vec3f origin,float radius,int duration,Actor* exclude,Vec3f end,int delay) {
    if(!Active() || gPlayState->actorCtx.total>=180 || LiveCount()>=8 || (GET_PLAYER(gPlayState)->stateFlags2&PLAYER_STATE2_SPIN_ATTACKING))return nullptr;
    auto* a=Actor_Spawn(&gPlayState->actorCtx,gPlayState,ACTOR_EN_M_THUNDER,origin.x,origin.y,origin.z,0,0,0,1);if(!a || !a->update)return nullptr;
    a->world.pos=origin;
    if(kind==Kind::Rain)for(auto* old=gPlayState->actorCtx.actorLists[ACTORCAT_ITEMACTION].head;old;old=old->next)if(auto* previous=ObjectExtension::GetInstance().Get<Effect>(old);previous && previous->kind==Kind::Rain)Actor_Kill(old);
    Effect fx;fx.kind=kind;fx.origin=origin;fx.end=end;fx.radius=radius;fx.duration=duration;fx.delay=delay;fx.exclude=exclude?Data(exclude)->id:0;fx.count=kind==Kind::Cable?8:1;
    ObjectExtension::GetInstance().Set<Effect>(a,std::move(fx));auto* data=ObjectExtension::GetInstance().Get<Effect>(a);
    uint32_t flags=kind==Kind::Ice?DMG_ARROW_ICE:kind==Kind::Cable || kind==Kind::Beam?DMG_ARROW_LIGHT:kind==Kind::Fire?DMG_ARROW_FIRE:kind==Kind::Thorns || kind==Kind::Gust?DMG_BOOMERANG:kind==Kind::Anchor?DMG_HAMMER_SWING:DMG_EXPLOSIVE;
    ColliderCylinderInit init={{COLTYPE_NONE,AT_ON|AT_TYPE_PLAYER,AC_NONE,OC1_NONE,OC2_TYPE_1,COLSHAPE_CYLINDER},
        {ELEMTYPE_UNK0,{flags,0,2},{0,0,0},TOUCH_ON|TOUCH_SFX_NONE,BUMP_NONE,OCELEM_NONE},{s16(radius),90,-20,{0,0,0}}};
    for(auto& c:data->colliders){Collider_InitCylinder(gPlayState,&c);Collider_SetCylinder(gPlayState,&c,a,&init);}
    a->update=UpdateEffect;a->draw=DrawEffect;a->destroy=DestroyEffect;a->flags|=ACTOR_FLAG_UPDATE_CULLING_DISABLED|ACTOR_FLAG_DRAW_CULLING_DISABLED;
    return a;
}
void Conduct(Actor* target) {
    int arcs=0;
    EachEnemy([&](Actor* other){if(other==target || arcs>=3 || !Wet(other) || Distance(other->world.pos,target->world.pos)>170 || !Clear(target->world.pos,other->world.pos))return;
        if(Spawn(Kind::Cable,target->world.pos,24,12,target,other->world.pos))++arcs;});
}
void Update() {
    if(!Active()){Reset(gPlayState);return;}
    // Preparations belong to the equipped loadout. Launched actors retain their own snapshot.
    uint64_t key=1469598103934665603ULL;
    for(int slot=0;slot<ARPG_LOOT_SLOT_COUNT;++slot){const auto& item=InventoryRules::Equipped(gARPGSave,slot);key=(key^uint64_t(item.uniqueId | (item.baseEquip<<8) | (ItemModifierSystem::IsEquipped(item)<<16)))*1099511628211ULL;}
    if(loadout!=key){prep.guard=prep.light=prep.anchor=prep.cinder=prep.steady=0;prep.airborneShot=prep.cinderFinal=false;rainPending=false;loadout=key;}
    if(!Playing())return;
    auto* player=GET_PLAYER(gPlayState);bool grounded=player->actor.bgCheckFlags&BGCHECKFLAG_GROUND;
    bool shielding=(player->stateFlags1&PLAYER_STATE1_SHIELDING)!=0;
    if(shielding && !guardHeld && Equipped(ARPG_SLOT_SHIELD,24))prep.guard=6;
    if(!shielding)prep.guard=0;guardHeld=shielding;
    prep.Tick(true,grounded,Distance(lastPlayerPosition,player->actor.world.pos)<.7f,Equipped(ARPG_SLOT_BOOTS,26));lastPlayerPosition=player->actor.world.pos;
    if(!Equipped(ARPG_SLOT_SHIELD,24))prep.light=0;
    if(!Equipped(ARPG_SLOT_BOOTS,27))prep.airborneShot=false;
    if(prep.cinder && !player->meleeWeaponState)cinderReady=true;
    if(player->heldItemAction!=PLAYER_IA_DEKU_STICK){prep.cinder=0;prep.cinderFinal=false;}
    if(prep.steady && player->hoverBootsTimer && !grounded){player->linearVelocity=0;player->actor.speedXZ=0;player->actor.velocity.x=player->actor.velocity.z=0;}
    if(grounded && prep.airborneShot){Spawn(Kind::Gust,player->actor.world.pos,150);prep.airborneShot=false;}
    if(rainPending){auto* item=Equipped(ARPG_SLOT_OCARINA,28);if(item && !prep.rainRecovery && Spawn(Kind::Rain,player->actor.world.pos,Radius(item,190),AdventureItems::Duration(item,160))){prep.rainRecovery=200;rainPending=false;}else if(++pendingAge>20)rainPending=false;}
    EachEnemy([&](Actor* a){auto* d=ObjectExtension::GetInstance().Get<Target>(a);if(!d)return;if(d->wet)--d->wet;if(d->weakRecovery)--d->weakRecovery;
        if(d->pull){--d->pull;if(!Movable(a) || !Clear(player->actor.world.pos,a->world.pos) || Expeditions::DamageMultiplier(a,DMG_HOOKSHOT)<=0){d->pull=0;return;}
            if(d->heavy){if(Actor_WorldDistXZToActor(a,&player->actor)>75)SkillCombat::TryStep(gPlayState,8,Actor_WorldYawTowardActor(&player->actor,a));else d->pull=0;}
            else Step(a,player->actor.world.pos,9);
        }
    });
}
void Draw() {
    if(!Active())return;auto* player=GET_PLAYER(gPlayState);
    EachEnemy([&](Actor* a){auto* d=ObjectExtension::GetInstance().Get<Target>(a);if(d && d->wet){Mesh m;m.Ring(24,2,{88,160,242,150});DrawMesh(m,a->world.pos,a,93);}});
    if(Equipped(ARPG_SLOT_LENS,25) && gPlayState->actorCtx.lensActive && gSaveContext.magic>0 && Movable(player->focusActor)) {
        auto* target=player->focusActor;if(!Data(target)->weakRecovery){Mesh m;std::array<uint8_t,4> c{250,124,242,245};m.Line({-11,0,0},{0,11,0},2,c);m.Line({0,11,0},{11,0,0},2,c);m.Line({11,0,0},{0,-11,0},2,c);m.Line({0,-11,0},{-11,0,0},2,c);DrawMesh(m,WeakPoint(target),target,94);}
    }
    if(prep.light || prep.anchor>=40 || prep.cinder){Mesh m;m.Ring(26,4,prep.light?std::array<uint8_t,4>{255,237,146,205}:prep.cinder?std::array<uint8_t,4>{255,144,48,205}:std::array<uint8_t,4>{215,182,109,205});DrawMesh(m,player->actor.world.pos,player,95);}
}
}
int LiveCount(){int n=0;if(gPlayState)for(auto* a=gPlayState->actorCtx.actorLists[ACTORCAT_ITEMACTION].head;a;a=a->next)n+=a->update && ObjectExtension::GetInstance().Has<Effect>(a);return n;}
int Wet(Actor* a){auto* d=a?ObjectExtension::GetInstance().Get<Target>(a):nullptr;return Active() && d?d->wet:0;}
Preparation State(){return prep;}
void Reset(PlayState* play){prep={};rainPending=false;pendingAge=0;loadout=0;EachEnemy([](Actor* a){if(auto* d=ObjectExtension::GetInstance().Get<Target>(a)){d->wet=d->pull=d->weakRecovery=0;d->pointFrame=UINT32_MAX;}});if(play)for(auto* a=play->actorCtx.actorLists[ACTORCAT_ITEMACTION].head;a;a=a->next)if(ObjectExtension::GetInstance().Has<Effect>(a))Actor_Kill(a);}
bool Collision(PlayState*,void* atRef,void* acRef,void* infoRef,void*,void* posRef) {
    auto* at=static_cast<Collider*>(atRef);auto* ac=static_cast<Collider*>(acRef);auto* info=static_cast<ColliderInfo*>(infoRef);
    if(!at->actor || !ac->actor)return true;
    auto* fx=ObjectExtension::GetInstance().Get<Effect>(at->actor);
    if(fx){if(!Playing() || !Enemy(ac->actor) || !Clear(fx->kind==Kind::Beam?at->actor->world.pos:fx->origin,ac->actor->world.pos) || Expeditions::DamageMultiplier(ac->actor,info->toucher.dmgFlags)<=0)return false;
        if(ac->acFlags&AC_HARD)return true;
        uint32_t id=Data(ac->actor)->id;if(id==fx->exclude || fx->hitCount>=int(fx->hits.size()))return false;
        for(int i=0;i<fx->hitCount;++i)if(fx->hits[i]==id)return false;fx->hits[fx->hitCount++]=id;
    }
    if(Playing() && !(ac->acFlags&AC_HARD) && (at->atFlags&AT_TYPE_PLAYER) && Enemy(ac->actor) && at->actor->id==ACTOR_EN_ARROW && (info->toucher.dmgFlags&(DMG_ARROW|DMG_SLINGSHOT))) {
        auto* d=Data(ac->actor);d->point=*static_cast<Vec3f*>(posRef);d->pointSource=at->actor;d->pointFrame=gPlayState->gameplayFrames;
    }
    return true;
}
float OnContact(Actor* target,Actor* source,uint32_t flags,uint8_t alignment,float damage) {
    if(!Playing() || !Enemy(target) || !source || !(alignment&AT_TYPE_PLAYER) || Expeditions::DamageMultiplier(target,flags)<=0)return 1;
    auto* state=Arsenal::MutableStatus(target);auto* data=Data(target);
    if(damage>0 && (flags&DMG_FIRE))state->burning=std::max(state->burning,60);
    if(damage>0 && (flags&(DMG_ARROW_ICE|DMG_MAGIC_ICE)) && Movable(target))state->frozen=std::max(state->frozen,20);
    if(auto* fx=ObjectExtension::GetInstance().Get<Effect>(source)) {
        if(fx->kind==Kind::Gust && (damage>0 || target->colChkInfo.damageEffect==1))Step(target,fx->origin,32,true);
        if(fx->kind==Kind::Thorns && target->colChkInfo.damageEffect==1)state->concussed=20;
        if(fx->kind==Kind::Fire && damage>0)state->burning=60;
        if(fx->kind==Kind::Ice && damage>0 && Movable(target))state->frozen=15;
        return fx->power;
    }
    if(data->lastContact==gPlayState->gameplayFrames)return 1;data->lastContact=gPlayState->gameplayFrames;
    const auto* item=ItemModifierSystem::WeaponForHit(source,flags);auto* player=GET_PLAYER(gPlayState);
    bool stun=target->colChkInfo.damageEffect==1;float multiplier=1;
    if(damage>0 && source->id==ACTOR_PLAYER && (flags&DMG_HAMMER)) {
        if(AdventureItems::IsUnique(item,19) && state->frozen && !prep.frostRecovery && Spawn(Kind::Ice,target->world.pos,135,16,target)){state->frozen=0;prep.frostRecovery=20;multiplier*=1.5f;}
        if(Equipped(ARPG_SLOT_BOOTS,26) && prep.Slam()){EachEnemy([&](Actor* a){if(a!=target && Movable(a) && Distance(a->world.pos,target->world.pos)<180 && Clear(target->world.pos,a->world.pos) && Expeditions::DamageMultiplier(a,DMG_HAMMER)>0)Step(a,target->world.pos,60);});Spawn(Kind::Anchor,target->world.pos,90,16,target);multiplier*=1.35f;}
    }
    if(damage>0 && AdventureItems::IsUnique(item,20) && state->tethered && !prep.stormRecovery) {
        if(Spawn(Kind::Cable,target->world.pos,38,12,nullptr,player->actor.world.pos)){prep.stormRecovery=20;if(data->wet)Conduct(target);}
    }
    if(source->id==ACTOR_EN_BOOM && AdventureItems::IsUnique(item,21) && stun && Movable(target)) {
        auto* shot=Shot(source);if(!shot->used){shot->used=true;shot->orbitTarget=data->id;shot->orbit=30;state->marked=std::max(state->marked,80);}
    }
    if(damage>0 && source->id==ACTOR_EN_BOM && AdventureItems::IsUnique(item,22) && state->burning && !Shot(source)->used) {
        Vec3f p=target->world.pos;s16 yaw=Actor_WorldYawTowardActor(source,target);p.x+=Math_SinS(yaw)*70;p.z+=Math_CosS(yaw)*70;
        if(Clear(target->world.pos,p) && Spawn(Kind::Blast,p,80,24,target,{},8))Shot(source)->used=true;
    }
    if(damage>0 && source->id==ACTOR_PLAYER && (flags&DMG_DEKU_STICK) && AdventureItems::IsUnique(item,23)) {
        if(prep.cinder && cinderReady){prep.cinderFinal=true;}
        else if(!prep.cinder && state->burning){prep.cinder=120;cinderReady=false;player->unk_860=200;}
    }
    if(damage>0 && source->id==ACTOR_EN_ARROW && (flags&(DMG_ARROW|DMG_SLINGSHOT))) {
        if(auto* lens=Equipped(ARPG_SLOT_LENS,25);lens && gPlayState->actorCtx.lensActive && gSaveContext.magic>0 && player->focusActor==target && Movable(target) && !data->weakRecovery && data->pointFrame==gPlayState->gameplayFrames && data->pointSource==source && Distance(data->point,WeakPoint(target))<Radius(lens,14)) {
            state->frozen=AdventureItems::Duration(lens,20);data->weakRecovery=40;multiplier*=1.5f;
        }
        if(AdventureItems::IsUnique(item,14) && data->wet && Movable(target) && ItemModifierSystem::AcceptsElement(target,DMG_ARROW_ICE))state->frozen=std::max(state->frozen,40);
        if((flags&(DMG_ARROW_LIGHT|DMG_MAGIC_LIGHT)) && data->wet && !prep.stormRecovery){Conduct(target);prep.stormRecovery=20;}
    }
    if(source->id==ACTOR_ARMS_HOOK && AdventureItems::IsUnique(item,30) && stun && Movable(target) && Clear(player->actor.world.pos,target->world.pos)) {
        data->heavy=Arsenal::FamilyFor(target)==Arsenal::Family::Stalfos;data->pull=20;
    }
    return multiplier;
}
void ShieldBlock(){if(Playing() && Equipped(ARPG_SLOT_SHIELD,24) && prep.Block())Audio_PlayActorSound2(&GET_PLAYER(gPlayState)->actor,NA_SE_IT_SHIELD_REFLECT_SW);}
void ArrowReleased(Actor* actor) {
    if(!Playing() || !actor || actor->id!=ACTOR_EN_ARROW || actor->params<0 || actor->params>=ARROW_SEED)return;
    auto* player=GET_PLAYER(gPlayState);
    if(Equipped(ARPG_SLOT_SHIELD,24) && prep.light) {
        if(auto* beam=Spawn(Kind::Beam,actor->world.pos,35,18)){auto* fx=ObjectExtension::GetInstance().Get<Effect>(beam);float horiz=Math_CosS(actor->world.rot.x)*60;fx->velocity={Math_SinS(actor->world.rot.y)*horiz,-Math_SinS(actor->world.rot.x)*60,Math_CosS(actor->world.rot.y)*horiz};prep.ShootLight();}
    }
    if(Equipped(ARPG_SLOT_BOOTS,27) && player->hoverBootsTimer && !(player->actor.bgCheckFlags&BGCHECKFLAG_GROUND)){prep.airborneShot=true;prep.steady=8;}
}
bool PreserveStick(){return Playing() && Equipped(ARPG_SLOT_STICK,23) && prep.cinder>0 && !prep.cinderFinal;}
void StickBroken(){if(Playing() && Equipped(ARPG_SLOT_STICK,23) && prep.cinder && prep.cinderFinal)Spawn(Kind::Fire,GET_PLAYER(gPlayState)->actor.world.pos,130);prep.cinder=0;prep.cinderFinal=false;}
bool BottleReleased(){const auto* item=Equipped(ARPG_SLOT_BOTTLE,29);if(!item)return false;return Spawn(Kind::Wisp,GET_PLAYER(gPlayState)->leftHandPos,Radius(item,500),AdventureItems::Duration(item,160))!=nullptr;}
bool BoomerangUpdate(Actor* actor) {
    auto* shot=ObjectExtension::GetInstance().Get<Projectile>(actor);if(!shot || !shot->orbit)return false;
    auto* boom=reinterpret_cast<EnBoom*>(actor);auto* player=GET_PLAYER(gPlayState);auto* target=Find(shot->orbitTarget);
    if(!Active() || !target || player->boomerangQuickRecall || !Arsenal::GetStatus(target).marked || !Clear(actor->world.pos,target->world.pos)) {shot->orbit=0;boom->returnTimer=0;boom->moveTo=&player->actor;return false;}
    if(!Playing())return true;
    float angle=(30-shot->orbit)*.48f;Vec3f p=target->world.pos;p.x+=std::sin(angle)*40;p.z+=std::cos(angle)*40;p.y+=35;
    if(!Clear(actor->world.pos,p,0)){shot->orbit=0;boom->returnTimer=0;return false;}actor->world.pos=p;boom->returnTimer=2;boom->collider.base.atFlags&=~(AT_HIT|AT_BOUNCED);
    if(shot->orbit%10==0)Spawn(Kind::Thorns,target->world.pos,48,8);
    // Bring ammunition to Link through its native pickup logic; never manufacture ammo or consume full-capacity drops.
    for(auto* a=gPlayState->actorCtx.actorLists[ACTORCAT_MISC].head;a;a=a->next)if(a->update && a->id==ACTOR_EN_ITEM00 && AmmoPickup(a->params&0xFF) && Distance(a->world.pos,actor->world.pos)<65 && Clear(a->world.pos,player->actor.world.pos)) {a->world.pos=player->actor.world.pos;a->world.pos.y+=10;}
    Actor_PlaySfx_Flagged(actor,NA_SE_IT_BOOMERANG_FLY-SFX_FLAG);
    if(--shot->orbit==0){boom->returnTimer=0;boom->moveTo=&player->actor;}return true;
}
void SteerBombchu(Actor* actor) {
    if(!Playing() || !AdventureItems::IsUnique(ItemModifierSystem::WeaponForActor(actor),22))return;
    auto* target=GET_PLAYER(gPlayState)->focusActor;if(!Enemy(target) || Distance(actor->world.pos,target->world.pos)>650 || !Clear(actor->world.pos,target->world.pos))return;
    auto* chu=reinterpret_cast<EnBomChu*>(actor);Vec3f aim{target->world.pos.x-actor->world.pos.x,target->world.pos.y-actor->world.pos.y,target->world.pos.z-actor->world.pos.z};float dot=DOTXYZ(aim,chu->axisUp);aim.x-=dot*chu->axisUp.x;aim.y-=dot*chu->axisUp.y;aim.z-=dot*chu->axisUp.z;float len=std::sqrt(DOTXYZ(aim,aim));if(len<1)return;
    Vec3f forward{chu->axisForwards.x*.9f+aim.x/len*.1f,chu->axisForwards.y*.9f+aim.y/len*.1f,chu->axisForwards.z*.9f+aim.z/len*.1f};len=std::sqrt(DOTXYZ(forward,forward));if(len<.01f)return;forward.x/=len;forward.y/=len;forward.z/=len;chu->axisForwards=forward;
    chu->axisLeft={chu->axisUp.y*forward.z-chu->axisUp.z*forward.y,chu->axisUp.z*forward.x-chu->axisUp.x*forward.z,chu->axisUp.x*forward.y-chu->axisUp.y*forward.x};
    MtxF m{};m.xx=chu->axisLeft.x;m.yx=chu->axisLeft.y;m.zx=chu->axisLeft.z;m.xy=chu->axisUp.x;m.yy=chu->axisUp.y;m.zy=chu->axisUp.z;m.xz=forward.x;m.yz=forward.y;m.zz=forward.z;Matrix_MtxFToYXZRotS(&m,&actor->world.rot,0);actor->world.rot.x=-actor->world.rot.x;
}
void Register(){
    COND_HOOK(OnPlayerUpdate,true,Update);
    COND_HOOK(OnPlayerHoldUpShield,true,[](){if(Playing() && Equipped(ARPG_SLOT_SHIELD,24))prep.guard=6;});
    COND_HOOK(OnOcarinaSongAction,true,[](){if(Active() && Equipped(ARPG_SLOT_OCARINA,28) && !prep.rainRecovery && gPlayState->msgCtx.lastPlayedSong==OCARINA_SONG_STORMS && gPlayState->msgCtx.ocarinaAction==OCARINA_ACTION_FREE_PLAY_DONE && CHECK_QUEST_ITEM(QUEST_SONG_STORMS)){rainPending=true;pendingAge=0;}});
    COND_HOOK(OnPlayDrawEnd,true,Draw);
    COND_HOOK(OnPlayDestroy,true,[](){Reset(gPlayState);});
    COND_HOOK(OnLoadFile,true,[](int32_t){Reset(gPlayState);});
}
static RegisterShipInitFunc init(Register);
}
extern "C" int ARPG_RelicCollision(PlayState* p,void* at,void* ac,void* ai,void* ci,void* pos){return ARPG::Relics::Collision(p,at,ac,ai,ci,pos);}
extern "C" void ARPG_RelicArrowReleased(Actor* a){ARPG::Relics::ArrowReleased(a);}
extern "C" void ARPG_RelicShieldBlock(){ARPG::Relics::ShieldBlock();}
extern "C" int ARPG_RelicPreserveStick(){return ARPG::Relics::PreserveStick();}
extern "C" void ARPG_RelicStickBroken(){ARPG::Relics::StickBroken();}
extern "C" int ARPG_RelicBottleReleased(){return ARPG::Relics::BottleReleased();}
extern "C" int ARPG_RelicBoomerangUpdate(Actor* a){return ARPG::Relics::BoomerangUpdate(a);}
extern "C" void ARPG_RelicSteerBombchu(Actor* a){ARPG::Relics::SteerBombchu(a);}
