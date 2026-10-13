#include "NativeMaterials.h"
#include "NativeEffects.h"
#include "BroodSystem.h"
#include "BroodGeometry.h"
#include "EnemySystem.h"
#include "ARPGTesting.h"
#include "ARPGTheme.h"
#include "soh/ActorDB.h"
#include "soh/ShipInit.hpp"
#include "soh/ObjectExtension/ObjectExtension.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/frame_interpolation.h"
#include <libultraship/bridge/consolevariablebridge.h>
extern "C" {
#include "functions.h"
#include "variables.h"
#include "macros.h"
#include "overlays/actors/ovl_Boss_Goma/z_boss_goma.h"
#include "overlays/actors/ovl_En_Goma/z_en_goma.h"
void BossGoma_FloorMain(BossGoma*,PlayState*);
void BossGoma_FloorIdle(BossGoma*,PlayState*);
void BossGoma_CeilingIdle(BossGoma*,PlayState*);
void BossGoma_CeilingMoveToCenter(BossGoma*,PlayState*);
void BossGoma_FloorStunned(BossGoma*,PlayState*);
void BossGoma_FloorDamaged(BossGoma*,PlayState*);
void BossGoma_FallStruckDown(BossGoma*,PlayState*);
void BossGoma_FloorLandStruckDown(BossGoma*,PlayState*);
void EnGoma_SetupStand(EnGoma*);
extern PlayState* gPlayState;
}
namespace ARPG::Brood {
namespace {
struct Director {uint64_t identity=0;int cooldown=120,waves=0;uint32_t lastFrame=UINT32_MAX;};
struct Child {uint64_t owner=0;};
struct Pod {Actor actor;ColliderCylinder body;State state;uint64_t owner;int invulnerable;std::array<float,17> webY;};
static ObjectExtension::Register<Director> directorRegistration;
static ObjectExtension::Register<Child> childRegistration;
uint64_t nextIdentity=1;int podId=-1;
bool Active(PlayState* play){return play && Enabled() && GameInteractor::IsPlayerInControl() && gSaveContext.health>0 && play->transitionTrigger==TRANS_TRIGGER_OFF;}
Director* GetDirector(Actor* a){auto& e=ObjectExtension::GetInstance();if(!e.Has<Director>(a))e.Set<Director>(a,Director{nextIdentity++});return e.Get<Director>(a);}
Actor* Owner(PlayState* play,uint64_t id){
    if(!play || !id)return nullptr;
    for(auto* a=play->actorCtx.actorLists[ACTORCAT_BOSS].head;a;a=a->next)
        if(a->id==ACTOR_BOSS_GOMA && a->update && a->colChkInfo.health)
            if(auto* d=ObjectExtension::GetInstance().Get<Director>(a);d && d->identity==id)return a;
    return nullptr;
}
bool Stunned(const BossGoma* g){return g->actionFunc==BossGoma_FloorStunned || g->actionFunc==BossGoma_FloorDamaged || g->actionFunc==BossGoma_FallStruckDown || g->actionFunc==BossGoma_FloorLandStruckDown;}
bool Ground(PlayState* play,Vec3f& p){
    if(!BgCheck_PosInStaticBoundingBox(&play->colCtx,&p))return false;
    Vec3f ray=p;ray.y+=80;CollisionPoly* poly=nullptr;int bg=BGCHECK_SCENE;
    float y=BgCheck_EntityRaycastFloor5(play,&play->colCtx,&poly,&bg,&GET_PLAYER(play)->actor,&ray);
    if(!poly || bg!=BGCHECK_SCENE || y<=BGCHECK_Y_MIN || std::abs(y-p.y)>65 || poly->normal.y<30000 || SurfaceType_GetSceneExitIndex(&play->colCtx,poly,bg))return false;
    int type=SurfaceType_GetFloorType(&play->colCtx,poly,bg),property=func_80041EA4(&play->colCtx,poly,bg);
    if(type==2 || type==3 || type==7 || type==9 || type==12 || property==5 || property==12 || SurfaceType_IsWallDamage(&play->colCtx,poly,bg))return false;
    float water=0;WaterBox* box=nullptr;if(WaterBox_GetSurface1(play,&play->colCtx,p.x,p.z,&water,&box) && water>y+5)return false;
    p.y=y;return true;
}
bool Line(PlayState* play,Vec3f a,Vec3f b){a.y+=30;b.y+=30;Vec3f hit{};CollisionPoly* poly=nullptr;int bg=BGCHECK_SCENE;return !BgCheck_EntityLineTest1(&play->colCtx,&a,&b,&hit,&poly,true,true,true,true,&bg);}
DamageTable damageTable={{0x10,1,1,3,0x10,1,3,0x10,1,2,3,3,0x12,3,3,3,3,3,0x12,3,0,1,1,3,2,2,3,3,0,0,3,0}};
ColliderCylinderInit bodyInit={{COLTYPE_HIT0,AT_NONE,AC_ON|AC_TYPE_PLAYER,OC1_NONE,OC2_TYPE_1,COLSHAPE_CYLINDER},
    {ELEMTYPE_UNK0,{0,0,0},{0xFFCFFFFF,0,0},TOUCH_NONE,BUMP_ON,OCELEM_NONE},{25,50,0,{0,0,0}}};
void Init(Actor* actor,PlayState* play){
    auto* p=reinterpret_cast<Pod*>(actor);p->state={};p->owner=0;p->invulnerable=0;p->webY={};
    actor->colChkInfo.health=3;actor->colChkInfo.mass=MASS_IMMOVABLE;actor->colChkInfo.damageTable=&damageTable;
    Actor_SetScale(actor,1);actor->targetMode=3;actor->naviEnemyId=0xFF;Actor_SetFocus(actor,25);
    Collider_InitCylinder(play,&p->body);Collider_SetCylinder(play,&p->body,actor,&bodyInit);
}
void Destroy(Actor* actor,PlayState* play){Collider_DestroyCylinder(play,&reinterpret_cast<Pod*>(actor)->body);}
void Update(Actor* actor,PlayState* play){
    auto* p=reinterpret_cast<Pod*>(actor);auto* owner=Owner(play,p->owner);
    if(!Enabled() || (p->owner?!owner:!Testing::Active()) || play->transitionTrigger!=TRANS_TRIGGER_OFF){Actor_Kill(actor);return;}
    if(!Active(play))return;
    if(p->state.phase==Phase::Falling && owner && Stunned(reinterpret_cast<BossGoma*>(owner)))Break(p->state);
    if(p->invulnerable>0)--p->invulnerable;
    if(p->state.phase==Phase::Incubating && (p->body.base.acFlags&AC_HIT)){
        p->body.base.acFlags&=~AC_HIT;
        if(actor->colChkInfo.damageEffect==1)Delay(p->state);
        if(actor->colChkInfo.damage>0){Actor_ApplyDamage(actor);p->invulnerable=5;}
        if(!actor->colChkInfo.health){Break(p->state);Audio_PlayActorSound2(actor,NA_SE_EN_GOMA_EGG2);}
    }
    auto before=p->state.phase;bool hatch=Advance(p->state,true);
    if(before==Phase::Falling && p->state.phase==Phase::Incubating)Audio_PlayActorSound2(actor,NA_SE_EN_GOMA_EGG1);
    if(hatch && Larvae(play)<LarvaCap && AddedLarvae(play)<PodCap && play->actorCtx.total<180){
        // Params 7 creates an independent floor larva. It never writes into
        // Gohma's three indexed children or keeps a dangling boss parent.
        auto* child=Actor_Spawn(&play->actorCtx,play,ACTOR_EN_GOMA,actor->world.pos.x,actor->world.pos.y,actor->world.pos.z,0,0,0,7);
        if(child && child->update){
            child->gravity=-1.3f;EnGoma_SetupStand(reinterpret_cast<EnGoma*>(child));
            ObjectExtension::GetInstance().Set<Child>(child,Child{p->owner});
            Audio_PlayActorSound2(actor,NA_SE_EN_GOMA_EGG2);
        }
    }
    if(Terminal(p->state))actor->flags&=~ACTOR_FLAG_ATTENTION_ENABLED;
    else if(p->state.phase==Phase::Incubating){
        actor->flags|=ACTOR_FLAG_ATTENTION_ENABLED;Collider_UpdateCylinder(actor,&p->body);
        if(!p->invulnerable)CollisionCheck_SetAC(play,&play->colChkCtx,&p->body.base);
    }
    if(Retired(p->state))Actor_Kill(actor);
}
void Draw(Actor* actor,PlayState* play){
    auto* p=reinterpret_cast<Pod*>(actor);auto s=p->state;
    float opening=Terminal(s)?std::min(1.f,s.age/12.f):0;
    float pulse=.5f+.5f*std::sin(play->gameplayFrames*(s.age>70?.6f:.25f));
    auto mesh=Geometry::Shell(opening,pulse,s.delayed>0);
    GraphicsContext* __gfxCtx=play->state.gfxCtx;
    auto* v=static_cast<Vtx*>(Graph_Alloc(__gfxCtx,mesh.size()*3*sizeof(Vtx)));if(!v)return;
    int n=0;for(auto t:mesh) {
        using namespace GearGeometry;
        auto normal=Unit(Cross({t.b.x-t.a.x,t.b.y-t.a.y,t.b.z-t.a.z},{t.c.x-t.a.x,t.c.y-t.a.y,t.c.z-t.a.z}));
        for(auto q:{t.a,t.b,t.c}) {
            auto& x=v[n++];x={};x.v.ob[0]=int16_t(q.x*16);x.v.ob[1]=int16_t(q.y*16);x.v.ob[2]=int16_t(q.z*16);
            x.v.cn[0]=int8_t(normal.x*127);x.v.cn[1]=int8_t(normal.y*127);x.v.cn[2]=int8_t(normal.z*127);x.v.cn[3]=255;
            NativeMaterials::UV(x,{q.x,q.y,q.z},normal,48,NativeMaterials::Surface::Shell);
        }
    }
    FrameInterpolation_RecordOpenChild(actor,0);Matrix_Push();
    float lift=s.phase==Phase::Falling?120.f*(1-float(s.age)/FallTicks):0;
    Matrix_Translate(actor->world.pos.x,actor->world.pos.y+lift,actor->world.pos.z,MTXMODE_NEW);Matrix_Scale(.0625f,.0625f,.0625f,MTXMODE_APPLY);
    Gfx_SetupDL_25Opa(__gfxCtx);gSPClearGeometryMode(POLY_OPA_DISP++,G_LIGHTING|G_CULL_BOTH|G_TEXTURE_GEN|G_TEXTURE_GEN_LINEAR);
    POLY_OPA_DISP=NativeMaterials::Bind(POLY_OPA_DISP,NativeMaterials::Surface::Shell,s.delayed>0?GearGeometry::Color{122,159,173}:GearGeometry::Color{167,151,113});
    gSPMatrix(POLY_OPA_DISP++,MATRIX_NEWMTX(__gfxCtx),G_MTX_NOPUSH|G_MTX_LOAD|G_MTX_MODELVIEW);
    for(int i=0;i<n;i+=24){int count=std::min(24,n-i);gSPVertex(POLY_OPA_DISP++,reinterpret_cast<uintptr_t>(v+i),count,0);for(int j=0;j<count;j+=3)gSP1Triangle(POLY_OPA_DISP++,j,j+1,j+2,0);}
    Matrix_Pop();FrameInterpolation_RecordCloseChild();
    // The landing mark is a spiderweb, not a damage ring: the pod itself is harmless.
    auto* web=static_cast<Vtx*>(Graph_Alloc(__gfxCtx,192*sizeof(Vtx)));if(!web)return;n=0;
    auto line=[&](Vec3f a,Vec3f b){float dx=b.x-a.x,dz=b.z-a.z,len=std::max(1.f,std::hypot(dx,dz)),x=dz/len,z=-dx/len;
        Vec3f q[]={{a.x-x,a.y,a.z-z},{b.x-x,b.y,b.z-z},{b.x+x,b.y,b.z+z},{a.x+x,a.y,a.z+z}};
        for(int j:{0,1,2,0,2,3}){auto& t=web[n++];t={};t.v.ob[0]=int16_t(q[j].x*8);t.v.ob[1]=int16_t(q[j].y*8);t.v.ob[2]=int16_t(q[j].z*8);t.v.cn[0]=220;t.v.cn[1]=220;t.v.cn[2]=165;t.v.cn[3]=Terminal(s)?int(180*(1-s.age/float(FadeTicks))):180;}
    };
    auto webPoint=[&](int ring,int i){float angle=i*6.2831853f/8,r=ring*18.f;return Vec3f{std::sin(angle)*r,p->webY[1+(ring-1)*8+i%8],std::cos(angle)*r};};
    for(int i=0;i<8;++i){line({0,0,0},webPoint(1,i));line(webPoint(1,i),webPoint(2,i));
        for(int ring:{1,2})line(webPoint(ring,i),webPoint(ring,(i+1)%8));}
    FrameInterpolation_RecordOpenChild(actor,1);Matrix_Push();Matrix_Translate(actor->world.pos.x,actor->world.pos.y+2,actor->world.pos.z,MTXMODE_NEW);Matrix_Scale(.125f,.125f,.125f,MTXMODE_APPLY);
    Gfx_SetupDL_25Xlu(__gfxCtx);gSPClearGeometryMode(POLY_XLU_DISP++,G_LIGHTING|G_CULL_BOTH|G_TEXTURE_GEN|G_TEXTURE_GEN_LINEAR);
    NativeEffects::Feather(web,192,.75f);
    POLY_XLU_DISP=NativeEffects::Bind(POLY_XLU_DISP,NativeEffects::Texture::Trail);
    gSPMatrix(POLY_XLU_DISP++,MATRIX_NEWMTX(__gfxCtx),G_MTX_NOPUSH|G_MTX_LOAD|G_MTX_MODELVIEW);
    for(int i=0;i<192;i+=24){gSPVertex(POLY_XLU_DISP++,reinterpret_cast<uintptr_t>(web+i),24,0);for(int j=0;j<24;j+=3)gSP1Triangle(POLY_XLU_DISP++,j,j+1,j+2,0);}
    Matrix_Pop();FrameInterpolation_RecordCloseChild();
}
int Id(){if(podId<0 && ActorDB::Instance){ActorDBInit e{"Arpg_BroodPod","Brood pod",-1,ACTORCAT_PROP,ACTOR_FLAG_HOSTILE|ACTOR_FLAG_UPDATE_CULLING_DISABLED,OBJECT_GAMEPLAY_KEEP,sizeof(Pod),Init,Destroy,Update,Draw,nullptr};podId=ActorDB::Instance->AddEntry(e).entry.id;}return podId;}
}
bool Enabled(){return Enemies::Enabled() && CVarGetInteger(CVAR_ENHANCEMENT("ARPG.BossBroods"),1);}
bool IsPod(const Actor* a){return a && podId>=0 && a->id==podId;}
bool Targetable(const Actor* a){return IsPod(a) && a->update && a->colChkInfo.health && reinterpret_cast<const Pod*>(a)->state.phase==Phase::Incubating;}
State Status(const Actor* a){return IsPod(a)?reinterpret_cast<const Pod*>(a)->state:State{};}
int LivePods(PlayState* play){int n=0;if(play)for(auto* a=play->actorCtx.actorLists[ACTORCAT_PROP].head;a;a=a->next)n+=a->update && IsPod(a);return n;}
int Larvae(PlayState* play){int n=0;if(play)for(auto* a=play->actorCtx.actorLists[ACTORCAT_ENEMY].head;a;a=a->next)n+=a->update && a->colChkInfo.health && a->id==ACTOR_EN_GOMA && a->params<10;return n;}
int AddedLarvae(PlayState* play){int n=0;if(play)for(auto* a=play->actorCtx.actorLists[ACTORCAT_ENEMY].head;a;a=a->next)n+=a->update && a->colChkInfo.health && ObjectExtension::GetInstance().Has<Child>(a);return n;}
void Clear(PlayState* play){if(play)for(int category:{ACTORCAT_PROP,ACTORCAT_ENEMY})for(auto* a=play->actorCtx.actorLists[category].head;a;a=a->next)if(IsPod(a) || ObjectExtension::GetInstance().Has<Child>(a))Actor_Kill(a);}
Actor* Spawn(PlayState* play,Actor* owner,Vec3f position){
    if(!play || !Enabled() || play->actorCtx.total>=180 || LivePods(play)+AddedLarvae(play)>=PodCap || LivePods(play)+Larvae(play)>=LarvaCap)return nullptr;
    if(owner?(play->sceneNum!=SCENE_DEKU_TREE_BOSS || owner->id!=ACTOR_BOSS_GOMA || !owner->update || !owner->colChkInfo.health):!Testing::Active())return nullptr;
    if(!Ground(play,position) || !Line(play,GET_PLAYER(play)->actor.world.pos,position))return nullptr;
    for(auto* a=play->actorCtx.actorLists[ACTORCAT_PROP].head;a;a=a->next)if(a->update && IsPod(a) && std::hypot(a->world.pos.x-position.x,a->world.pos.z-position.z)<75)return nullptr;
    // Fit both web rings to supported ground; reject ledges instead of drawing through them.
    std::array<float,17> webY{};
    for(int ring:{1,2})for(int i=0;i<8;++i){float angle=i*6.2831853f/8;auto q=position;q.x+=std::sin(angle)*ring*18;q.z+=std::cos(angle)*ring*18;
        if(!Ground(play,q) || std::abs(q.y-position.y)>16)return nullptr;webY[1+(ring-1)*8+i]=q.y-position.y;}
    int id=Id();if(id<0)return nullptr;auto* actor=Actor_Spawn(&play->actorCtx,play,id,position.x,position.y,position.z,0,0,0,0);
    if(actor && actor->update){auto* p=reinterpret_cast<Pod*>(actor);p->owner=owner?GetDirector(owner)->identity:0;p->webY=webY;}
    return actor;
}
int Waves(const Actor* a){auto* d=ObjectExtension::GetInstance().Get<Director>(a);return d?d->waves:0;}
void TickBoss(Actor* actor,PlayState* play){
    if(!play || actor->id!=ACTOR_BOSS_GOMA || play->sceneNum!=SCENE_DEKU_TREE_BOSS || !actor->update || !actor->colChkInfo.health)return;
    auto* g=reinterpret_cast<BossGoma*>(actor);auto* d=GetDirector(actor);
    if(d->lastFrame==play->gameplayFrames)return;d->lastFrame=play->gameplayFrames;
    if(!Active(play) || g->disableGameplayLogic)return;
    if(d->cooldown>0){--d->cooldown;return;}
    if(g->actionFunc!=BossGoma_FloorMain && g->actionFunc!=BossGoma_FloorIdle && g->actionFunc!=BossGoma_CeilingIdle && g->actionFunc!=BossGoma_CeilingMoveToCenter)return;
    if(LivePods(play) || Larvae(play)>=LarvaCap)return;
    auto* p=GET_PLAYER(play);int spawned=0;
    for(int side:{-1,1}){auto point=p->actor.world.pos;auto yaw=p->actor.shape.rot.y;point.x+=Math_CosS(yaw)*side*80;point.z-=Math_SinS(yaw)*side*80;if(Spawn(play,actor,point))++spawned;}
    d->cooldown=spawned?WaveCooldown:40;
    if(spawned){++d->waves;Audio_PlayActorSound2(actor,NA_SE_EN_GOMA_UNARI);}
}
void DrawTesting(){
    if(!Testing::Active())return;
    static const char* result="";
    if(Theme::Button("Summon brood pods",{260,38})){
        auto* player=GET_PLAYER(gPlayState);auto yaw=player->actor.shape.rot.y;int count=0;
        for(int side:{-1,1}){auto p=player->actor.world.pos;p.x+=Math_SinS(yaw)*180+Math_CosS(yaw)*side*70;p.z+=Math_CosS(yaw)*180-Math_SinS(yaw)*side*70;if(Spawn(gPlayState,nullptr,p))++count;}
        result=count?"Broodfall ahead. Break the landed pods before they hatch.":"Needs clear ground ahead and space below the brood limit. Clear the practice brood to try again.";
    }
    ImGui::SameLine();if(Theme::Button("Clear practice brood",{260,38})){Clear(gPlayState);result="Practice brood cleared.";}
    if(*result)Theme::Wrapped("%s",result);
}
static void Register(){
    COND_HOOK(OnActorUpdate,true,[](void* ref){if(gPlayState)TickBoss(static_cast<Actor*>(ref),gPlayState);});
    COND_HOOK(OnPlayerUpdate,true,[](){
        if(!gPlayState)return;
        for(auto* a=gPlayState->actorCtx.actorLists[ACTORCAT_ENEMY].head;a;a=a->next)if(auto* c=ObjectExtension::GetInstance().Get<Child>(a))
            if(!Enabled() || (c->owner?!Owner(gPlayState,c->owner):!Testing::Active()))Actor_Kill(a);
    });
}
static RegisterShipInitFunc init(Register);
}
