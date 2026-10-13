#include "NativeEffects.h"
#include "ArsenalSystem.h"
#include "BroodSystem.h"
#include "AdventureItemRules.h"
#include "AffixSystem.h"
#include "CreatureSystem.h"
#include "ARPG.h"
#include "ARPGSave.h"
#include "ExpeditionSystem.h"
#include "EnemySystem.h"
#include "ARPGTheme.h"
#include "ItemModifierSystem.h"
#include "soh/ShipInit.hpp"
#include "soh/ObjectExtension/ObjectExtension.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/EnemyDifficulty/EnemyDifficulty.h"
#include "soh/frame_interpolation.h"
#include <cmath>
#include <cstring>
#include <limits>
extern "C" {
#include "functions.h"
#include "variables.h"
#include "macros.h"
#include "overlays/actors/ovl_En_M_Thunder/z_en_m_thunder.h"
extern PlayState* gPlayState;
}
namespace ARPG::Arsenal {
namespace {
struct Target { Status status; std::array<uint32_t,4> faults{}; uint32_t lastFrame=UINT32_MAX; Weapon lastWeapon=Weapon::Other; };
struct Fault { bool blast=false; float damageScale=.65f; int age=0; uint32_t token=0; Vec3f origin{}; s16 yaw=0; std::array<Vec3f,3> points{}; std::array<bool,3> valid{}; std::array<ColliderCylinder,3> colliders{}; };
static ObjectExtension::Register<Target> targetRegistration;
static ObjectExtension::Register<Fault> faultRegistration;
struct Detonated {};
static ObjectExtension::Register<Detonated> detonationRegistration;
uint32_t nextToken=1;
int hammerRecovery=0;
Target* Data(Actor* actor) {
    auto& ext=ObjectExtension::GetInstance();
    if(!ext.Has<Target>(actor)) ext.Set<Target>(actor,Target{});
    return ext.Get<Target>(actor);
}
bool Active() { return ItemsEnabled() && gPlayState && gSaveContext.health>0 && gPlayState->transitionTrigger==TRANS_TRIGGER_OFF; }
bool ClearLine(PlayState* play,Vec3f a,Vec3f b) {
    a.y+=25;b.y+=25;Vec3f hit{};CollisionPoly* poly=nullptr;int bg=BGCHECK_SCENE;
    return !BgCheck_EntityLineTest1(&play->colCtx,&a,&b,&hit,&poly,true,true,true,true,&bg);
}
bool Ground(PlayState* play,Vec3f& point) {
    Vec3f ray=point;ray.y+=80;CollisionPoly* poly=nullptr;int bg=BGCHECK_SCENE;
    float floor=BgCheck_EntityRaycastFloor5(play,&play->colCtx,&poly,&bg,&GET_PLAYER(play)->actor,&ray);
    if(!poly || bg!=BGCHECK_SCENE || poly->normal.y<24000 || floor<=BGCHECK_Y_MIN || std::abs(floor-point.y)>65) return false;
    point.y=floor+2;return true;
}
struct Mesh {
    std::array<Vtx,360> vertices{};int count=0;
    void Tri(Vec3f a,Vec3f b,Vec3f c,std::array<uint8_t,4> color) {
        if(count+3>int(vertices.size())) return;
        for(auto p:{a,b,c}) { auto& v=vertices[count++].v;v.ob[0]=int16_t(p.x*4);v.ob[1]=int16_t(p.y*4);v.ob[2]=int16_t(p.z*4);std::memcpy(v.cn,color.data(),4); }
    }
    void Ring(float radius,float height,std::array<uint8_t,4> color,float turn=0,float sweep=6.28318f) {
        for(int i=0;i<16;++i) {
            float a=turn+sweep*i/16,b=turn+sweep*(i+1)/16;
            Vec3f p{std::sin(a)*radius,height,std::cos(a)*radius},q{std::sin(b)*radius,height,std::cos(b)*radius},
                  r{std::sin(b)*(radius-2),height,std::cos(b)*(radius-2)},s{std::sin(a)*(radius-2),height,std::cos(a)*(radius-2)};
            Tri(p,q,r,color);Tri(p,r,s,color);
        }
    }
};
void DrawMesh(PlayState* play,Mesh& mesh,Vec3f origin,const void* identity,int part) {
    if(!mesh.count) return;
    GraphicsContext* __gfxCtx=play->state.gfxCtx;
    auto* vertices=static_cast<Vtx*>(Graph_Alloc(__gfxCtx,mesh.count*sizeof(Vtx)));if(!vertices)return;
    std::memcpy(vertices,mesh.vertices.data(),mesh.count*sizeof(Vtx));
    FrameInterpolation_RecordOpenChild(identity,part);Gfx_SetupDL_25Xlu(__gfxCtx);gDPPipeSync(POLY_XLU_DISP++);
    gSPClearGeometryMode(POLY_XLU_DISP++,G_LIGHTING|G_CULL_BOTH|G_TEXTURE_GEN|G_TEXTURE_GEN_LINEAR|G_FOG);
    gSPSetGeometryMode(POLY_XLU_DISP++,G_ZBUFFER|G_SHADE|G_SHADING_SMOOTH);
    gDPSetCycleType(POLY_XLU_DISP++,G_CYC_1CYCLE);gDPSetRenderMode(POLY_XLU_DISP++,G_RM_AA_ZB_XLU_SURF,G_RM_AA_ZB_XLU_SURF2);
    NativeEffects::Feather(vertices,mesh.count,.72f);
    POLY_XLU_DISP=NativeEffects::Bind(POLY_XLU_DISP,NativeEffects::Texture::Trail,play->gameplayFrames);
    Matrix_Push();Matrix_Translate(origin.x,origin.y,origin.z,MTXMODE_NEW);Matrix_Scale(.25f,.25f,.25f,MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++,MATRIX_NEWMTX(__gfxCtx),G_MTX_NOPUSH|G_MTX_LOAD|G_MTX_MODELVIEW);
    for(int offset=0;offset<mesh.count;offset+=24) { int n=std::min(24,mesh.count-offset);gSPVertex(POLY_XLU_DISP++,reinterpret_cast<uintptr_t>(vertices+offset),n,0);for(int i=0;i<n;i+=3)gSP1Triangle(POLY_XLU_DISP++,i,i+1,i+2,0); }
    Matrix_Pop();FrameInterpolation_RecordCloseChild();
}
void DrawFault(Actor* actor,PlayState* play) {
    auto* fault=ObjectExtension::GetInstance().Get<Fault>(actor);if(!fault || !Active())return;
    for(int i=0;i<3;++i) {
        int age=fault->age-(fault->blast?6+i*6:FaultLine[i].start);if(!fault->valid[i] || age<0 || age>12)continue;
        Mesh mesh;float height=std::max(1.f,26.f-age*2.f),radius=20.f+age*2.f;
        mesh.Ring(radius,1,{238,169,65,uint8_t(220-age*15)});
        for(int j=0;j<6;++j) {
            float a=j*1.0472f+.3f;Vec3f p{std::sin(a)*radius,0,std::cos(a)*radius},q{std::sin(a+.45f)*radius,0,std::cos(a+.45f)*radius},tip{std::sin(a+.2f)*radius*.8f,height,std::cos(a+.2f)*radius*.8f};
            mesh.Tri(p,q,tip,{210,143,59,uint8_t(220-age*15)});mesh.Tri(tip,q,{tip.x+5,2,tip.z-4},{98,80,54,uint8_t(220-age*15)});
        }
        DrawMesh(play,mesh,fault->points[i],actor,i);
    }
}
void DestroyFault(Actor* actor,PlayState* play) {
    if(auto* fault=ObjectExtension::GetInstance().Get<Fault>(actor)) for(auto& c:fault->colliders)Collider_DestroyCylinder(play,&c);
    auto* effect=reinterpret_cast<EnMThunder*>(actor);Collider_DestroyCylinder(play,&effect->collider);LightContext_RemoveLight(play,&play->lightCtx,effect->lightNode);
}
void UpdateFault(Actor* actor,PlayState* play) {
    auto* fault=ObjectExtension::GetInstance().Get<Fault>(actor);
    if(!fault || !Active() || fault->age>=(fault->blast?34:22)) { Actor_Kill(actor);return; }
    if(GameInteractor::IsGameplayPaused() || !GameInteractor::IsPlayerInControl())return;
    for(int i=0;i<3;++i) {
        int start=fault->blast?6+i*6:FaultLine[i].start;
        if(!fault->valid[i] || fault->age<start || fault->age>=start+3)continue;
        if(fault->blast && fault->age==start) {
            Vec3f velocity{},accel{0,.2f,0};auto pos=fault->points[i];pos.y+=15;
            EffectSsBomb2_SpawnLayered(play,&pos,&velocity,&accel,65,12);
            Audio_PlayActorSound2(actor,NA_SE_IT_BOMB_EXPLOSION);
        }
        auto& c=fault->colliders[i];c.base.atFlags&=~(AT_HIT|AT_BOUNCED);c.info.toucherFlags&=~(TOUCH_HIT|TOUCH_DREW_HITMARK);
        CollisionCheck_SetAT(play,&play->colChkCtx,&c.base);
    }
    ++fault->age;
}
bool StartFault(Actor* primary) {
    auto* play=gPlayState;if(!play || hammerRecovery || play->actorCtx.total>=180 || LiveFaults(play)>=4)return false;
    auto* player=GET_PLAYER(play);if(!(player->actor.bgCheckFlags&BGCHECKFLAG_GROUND) || player->stateFlags2&PLAYER_STATE2_SPIN_ATTACKING)return false;
    Fault data;
    auto* weapon=ItemModifierSystem::WeaponForHit(&player->actor,DMG_HAMMER_SWING);
    bool mountain=weapon && Loot::Unique(*weapon) && weapon->uniqueId==10;
    data.damageScale=mountain?1.f:.65f;
    data.origin=player->actor.world.pos;data.yaw=player->actor.shape.rot.y;bool any=false;
    for(int i=0;i<3;++i) {
        auto& p=data.points[i];p=data.origin;p.x+=Math_SinS(data.yaw)*FaultLine[i].forward;p.z+=Math_CosS(data.yaw)*FaultLine[i].forward;
        data.valid[i]=Ground(play,p) && ClearLine(play,data.origin,p);any|=data.valid[i];
    }
    if(!any)return false;
    auto* actor=Actor_Spawn(&play->actorCtx,play,ACTOR_EN_M_THUNDER,data.origin.x,data.origin.y,data.origin.z,0,0,0,1);
    if(!actor || !actor->update)return false;
    data.token=nextToken++;if(!nextToken)++nextToken;
    ObjectExtension::GetInstance().Set<Fault>(actor,std::move(data));auto* fault=ObjectExtension::GetInstance().Get<Fault>(actor);
    ColliderCylinderInit init={{COLTYPE_NONE,AT_ON|AT_TYPE_PLAYER,AC_NONE,OC1_NONE,OC2_TYPE_1,COLSHAPE_CYLINDER},
        {ELEMTYPE_UNK0,{DMG_HAMMER_SWING,0,2},{0,0,0},TOUCH_ON|TOUCH_SFX_NONE,BUMP_NONE,OCELEM_NONE},{40,60,-10,{0,0,0}}};
    for(int i=0;i<3;++i) { auto& c=fault->colliders[i];Collider_InitCylinder(play,&c);Collider_SetCylinder(play,&c,actor,&init);c.dim.pos={s16(fault->points[i].x),s16(fault->points[i].y),s16(fault->points[i].z)}; }
    Data(primary)->faults[data.token%4]=data.token;
    actor->update=UpdateFault;actor->draw=DrawFault;actor->destroy=DestroyFault;actor->flags|=ACTOR_FLAG_UPDATE_CULLING_DISABLED|ACTOR_FLAG_DRAW_CULLING_DISABLED;
    hammerRecovery=mountain?10:20;return true;
}
void EachTarget(const auto& visit) {
    if(!gPlayState)return;
    for(auto* a=gPlayState->actorCtx.actorLists[ACTORCAT_ENEMY].head;a;a=a->next) if(auto* data=ObjectExtension::GetInstance().Get<Target>(a))visit(a,*data);
}
}
void Detonate(Actor* bomb) {
    auto* item=ItemModifierSystem::WeaponForActor(bomb);
    if(!Active() || !AdventureItems::IsUnique(item,12) || bomb->id!=ACTOR_EN_BOM ||
       gPlayState->actorCtx.total>=180 || LiveFaults(gPlayState)>=4 || ObjectExtension::GetInstance().Has<Detonated>(bomb))return;
    ObjectExtension::GetInstance().Set<Detonated>(bomb,Detonated{});
    Fault data;data.blast=true;data.origin=bomb->world.pos;
    data.damageScale=.5f*(1+std::clamp(Loot::Power(*item,ARPG_MOD_ARCHERY),0,200)/100.f);
    bool any=false;
    for(int i=0;i<3;++i) {
        float angle=i*2.094395f;
        auto& p=data.points[i];p=data.origin;p.x+=std::sin(angle)*70;p.z+=std::cos(angle)*70;
        data.valid[i]=Ground(gPlayState,p) && ClearLine(gPlayState,data.origin,p);any|=data.valid[i];
    }
    if(!any)return;
    auto* actor=Actor_Spawn(&gPlayState->actorCtx,gPlayState,ACTOR_EN_M_THUNDER,data.origin.x,data.origin.y,data.origin.z,0,0,0,1);
    if(!actor || !actor->update)return;
    data.token=nextToken++;if(!nextToken)++nextToken;
    ObjectExtension::GetInstance().Set<Fault>(actor,std::move(data));
    auto* fault=ObjectExtension::GetInstance().Get<Fault>(actor);
    ColliderCylinderInit init={{COLTYPE_NONE,AT_ON|AT_TYPE_PLAYER,AC_NONE,OC1_NONE,OC2_TYPE_1,COLSHAPE_CYLINDER},
        {ELEMTYPE_UNK0,{DMG_EXPLOSIVE,0,2},{0,0,0},TOUCH_ON|TOUCH_SFX_NONE,BUMP_NONE,OCELEM_NONE},{60,90,-10,{0,0,0}}};
    for(int i=0;i<3;++i) { auto& c=fault->colliders[i];Collider_InitCylinder(gPlayState,&c);Collider_SetCylinder(gPlayState,&c,actor,&init);c.dim.pos={s16(fault->points[i].x),s16(fault->points[i].y),s16(fault->points[i].z)}; }
    actor->update=UpdateFault;actor->draw=DrawFault;actor->destroy=DestroyFault;
    actor->flags|=ACTOR_FLAG_UPDATE_CULLING_DISABLED|ACTOR_FLAG_DRAW_CULLING_DISABLED;
}
Weapon Classify(uint32_t flags) {
    if(flags&DMG_HAMMER)return Weapon::Hammer;if(flags&DMG_EXPLOSIVE)return Weapon::Bomb;
    if(flags&DMG_BOOMERANG)return Weapon::Boomerang;if(flags&DMG_HOOKSHOT)return Weapon::Hookshot;
    if(flags&DMG_ARROW)return Weapon::Bow;if(flags&DMG_SLINGSHOT)return Weapon::Slingshot;
    if(flags&DMG_DEKU_NUT)return Weapon::Nut;if(flags&DMG_SWORD)return Weapon::Sword;return Weapon::Other;
}
Element ElementFor(uint32_t flags) {
    if(flags&DMG_FIRE)return Element::Fire;if(flags&(DMG_ARROW_ICE|DMG_MAGIC_ICE))return Element::Ice;
    if(flags&(DMG_ARROW_LIGHT|DMG_MAGIC_LIGHT))return Element::Light;return Element::None;
}
Family FamilyFor(Actor* actor) {
    if(!actor)return Family::Other;
    return actor->id==ACTOR_EN_TEST?Family::Stalfos:actor->id==ACTOR_EN_WF?Family::Wolfos:actor->id==ACTOR_EN_ZF?Family::Lizalfos:Family::Other;
}
bool IsFault(Actor* actor) { return actor && ObjectExtension::GetInstance().Has<Fault>(actor); }
int LiveFaults(PlayState* play) {
    int count=0;if(play)for(auto* a=play->actorCtx.actorLists[ACTORCAT_ITEMACTION].head;a;a=a->next)count+=a->update && IsFault(a);return count;
}
float OnContact(Actor* target,Actor* source,uint32_t flags,uint8_t alignment,float damage) {
    auto family=FamilyFor(target);
    if(!Active() || !target || !source || (family==Family::Other && !Creatures::IsCreature(target)) || !EnemyDifficulty::IsEnemyActor(target) || Enemies::IsWatchman(target) || !(alignment&AT_TYPE_PLAYER) ||
       GameInteractor::IsGameplayPaused() || Expeditions::DamageMultiplier(target,flags)<=0)return 1;
    auto weapon=Classify(flags);auto* data=Data(target);
    float affinity=Affinity(family,weapon,ElementFor(flags));
    if(IsFault(source))return affinity*ObjectExtension::GetInstance().Get<Fault>(source)->damageScale;
    // Effects cast by skills do not masquerade as the player's actual hammer or utility projectiles.
    bool native=source->id==ACTOR_PLAYER || source->id==ACTOR_EN_ARROW || source->id==ACTOR_EN_BOOM ||
                source->id==ACTOR_ARMS_HOOK || source->id==ACTOR_EN_BOM || source->id==ACTOR_EN_BOM_CHU || source->id==ACTOR_EN_M_FIRE1;
    if(!native)return affinity;
    if(data->lastFrame==gPlayState->gameplayFrames && data->lastWeapon==weapon)return affinity;
    data->lastFrame=gPlayState->gameplayFrames;data->lastWeapon=weapon;
    const auto* tool=ItemModifierSystem::WeaponForHit(source,flags);
    bool marked=data->status.marked>0;
    float scale=Resolve(data->status,weapon,damage>0,target->colChkInfo.damageEffect==1);
    if(target->colChkInfo.damageEffect==1) {
        if(weapon==Weapon::Boomerang) { data->status.marked=AdventureItems::Duration(tool,80);data->status.markBonus=AdventureItems::Followup(tool); }
        if(weapon==Weapon::Hookshot) { data->status.tethered=AdventureItems::Duration(tool,80);data->status.tetherBonus=AdventureItems::Followup(tool); }
        if(weapon==Weapon::Nut) {
            data->status.concussed=AdventureItems::Duration(tool,30);
            if(tool) {data->status.marked=AdventureItems::Duration(tool,80);data->status.markBonus=AdventureItems::Followup(tool);}
        }
    }
    if(damage>0 && weapon==Weapon::Bomb)data->status.concussed=AdventureItems::Duration(tool,40);
    if(damage>0 && weapon==Weapon::Slingshot && AffixSystem::ResistanceForFlags(target,flags)>0) {
        if(AdventureItems::IsUnique(tool,14) && ItemModifierSystem::AcceptsElement(target,DMG_ARROW_ICE) && target->category!=ACTORCAT_BOSS)data->status.frozen=std::max(data->status.frozen,20);
        if(AdventureItems::IsUnique(tool,13) && ItemModifierSystem::AcceptsElement(target,DMG_ARROW_FIRE))data->status.burning=30;
    }
    if(!marked && damage>0 && weapon==Weapon::Bow) {
        auto* item=ItemModifierSystem::WeaponForHit(source,flags);
        if(item && Loot::Unique(*item) && item->uniqueId==9) {data->status.marked=80;data->status.markBonus=.75f;}
    }
    if(damage>0 && weapon==Weapon::Hammer && source->id==ACTOR_PLAYER)StartFault(target);
    return affinity*scale;
}
Status* MutableStatus(Actor* actor) { return Active() && actor ? &Data(actor)->status : nullptr; }
Status GetStatus(Actor* actor) {
    auto* data=actor?ObjectExtension::GetInstance().Get<Target>(actor):nullptr;return Active() && data?data->status:Status{};
}
float MovementMultiplier(Actor* actor) { return actor && actor->category!=ACTORCAT_BOSS?Movement(GetStatus(actor)):1; }
bool Collision(PlayState* play,void* atRef,void* acRef) {
    auto* at=static_cast<Collider*>(atRef),*ac=static_cast<Collider*>(acRef);
    auto* fault=ObjectExtension::GetInstance().Get<Fault>(at->actor);if(!fault)return true;
    if(!Active() || !ac->actor || !ClearLine(play,fault->origin,ac->actor->world.pos))return false;
    if(!Brood::Targetable(ac->actor) && ((FamilyFor(ac->actor)==Family::Other && !Creatures::IsCreature(ac->actor)) || !EnemyDifficulty::IsEnemyActor(ac->actor) || Enemies::IsWatchman(ac->actor)))return false;
    auto* data=Data(ac->actor);auto& token=data->faults[fault->token%4];if(token==fault->token)return false;token=fault->token;return true;
}
void Reset(PlayState* play) {
    hammerRecovery=0;EachTarget([](Actor*,Target& t){t={};});
    if(play)for(auto* a=play->actorCtx.actorLists[ACTORCAT_ITEMACTION].head;a;a=a->next)if(IsFault(a))Actor_Kill(a);
}
int BossWeaponDamage(Actor* actor,uint32_t flags,int nativeDamage) {
    if(!ItemsEnabled() || !actor)return nativeDamage;
    auto weapon=Classify(flags);
    if(actor->id==ACTOR_BOSS_GOMA) {
        if(weapon==Weapon::Hammer)return std::min(255,nativeDamage*2);
        if(weapon==Weapon::Bow || weapon==Weapon::Slingshot)return 1;
    }
    if(actor->id==ACTOR_BOSS_DODONGO && weapon==Weapon::Hammer)return std::min(255,nativeDamage*2);
    return nativeDamage;
}
void DrawGuide() {
    Theme::Label("ARSENAL",24,IM_COL32(235,221,156,255));
    Theme::Muted("Equip adventure items on C buttons. These effects follow their native attacks.");
    if(Theme::Section("Weapon loot & builds")) {
        Theme::Wrapped("Bows, hammers, bomb and seed satchels, hookshots, boomerangs and nut satchels drop once you own the original item. Equip a roll in Equipment, then assign its original item on Items. A satchel changes every use of that ammo; it does not grant free ammunition. Launched items retain their rolls if you change gear.");
        Theme::Wrapped("Windpiercer alternates bow hits and marked finishers. Mountainwake strengthens fault lines and halves their recovery. Common three-socket bases can become Galeweave bows or Deepforge hammers; find the ordered recipes in the Forge.");
        Theme::Wrapped("Choose any adventure item as an expedition cache target to hunt these builds. Your existing sword and armor remain independently equipped.");
    }
    if(Theme::Section("Adventure item discoveries")) {
        for(int id=11;id<=Loot::UniqueCount;++id) {
            ARPGItemInstance item{};item.slot=Loot::Uniques[id].slot;item.baseEquip=1;Loot::IdentifyUnique(item,id);
            Theme::Label(Loot::Uniques[id].name,20,IM_COL32(235,221,156,255));
            Theme::Wrapped("%s",Loot::WeaponPowerDescription(item));ImGui::Spacing();
        }
        Theme::Muted("Setup effects work on susceptible arsenal enemies. Shields, elemental immunity and expedition protection still apply. Echo explosions affect enemies, once per chorus.");
    }
    const char* names[]={"Hammer / Fault line","Boomerang / Hunter's mark","Hookshot / Tether break","Bombs / Concussion","Deku nuts / Disrupt"};
    const char* details[]={
        "A damaging hammer hit sends three staggered ground eruptions ahead. They strike nearby Stalfos, Wolfos and Lizalfos once each; the original target is excluded. Requires ground and a clear path. Recharges in 1 second.",
        "A native boomerang stun marks Stalfos, Wolfos or Lizalfos for 4 seconds. The next arrow or slingshot seed consumes the mark for +75% damage.",
        "A native hookshot stun tethers those enemies for 4 seconds, slowing movement by 25%. A hammer or bomb consumes the tether for +75% damage.",
        "A damaging bomb or Bombchu hit slows those enemies by 40% for 2 seconds. Blast a formation, then reposition or close with the hammer.",
        "An accepted nut stun leaves susceptible enemies moving 40% slower for 1.5 seconds. Equipping a nut satchel also marks them for an arrow or seed finisher. Native stun and guard rules still apply."};
    for(int i=0;i<5;++i)if(Theme::Section(names[i]))Theme::Wrapped("%s",details[i]);
    if(Theme::Section("Enemy strengths & weaknesses"))for(auto family:{Family::Stalfos,Family::Wolfos,Family::Lizalfos}) { Theme::Wrapped("%s",FamilyGuide(family));ImGui::Spacing(); }
    Creatures::DrawGuide();
    if(Theme::Section("Boss openings")) {
        Theme::Wrapped("%s",Brood::Guide());ImGui::Spacing();
        Theme::Wrapped("Gohma: first expose and stun the eye as usual. While stunned, arrows and seeds can chip it for 1 damage, and hammer attacks deal double their native damage. The closed eye remains protected.");
        Theme::Wrapped("King Dodongo: feed a bomb to create his native opening. Hammer attacks deal double their native damage during that opening. His armored walking and rolling phases remain protected.");
        Theme::Wrapped("Expedition oaths: Iron resists swords and is weak to hammers, arrows and bombs. Hunt is weak to arrows and seeds. Cinder is weak to ice; Winter is weak to fire. Storm resists light but is weak to hammers. Break ritual or relay protection first.");
    }
}
void Register() {
    COND_HOOK(OnPlayerUpdate,true,[](){
        if(!Active()) {Reset(gPlayState);return;}
        if(GameInteractor::IsGameplayPaused() || !GameInteractor::IsPlayerInControl())return;
        if(hammerRecovery>0)--hammerRecovery;
        EachTarget([](Actor*,Target& data){Tick(data.status,true);});
    });
    COND_HOOK(OnPlayDestroy,true,[](){Reset(gPlayState);});
    COND_HOOK(OnLoadFile,true,[](int32_t){Reset(gPlayState);});
    COND_HOOK(OnPlayDrawEnd,true,[](){
        if(!Active())return;int count=0;
        EachTarget([&](Actor* actor,Target& data){
            auto s=data.status;if(!actor->update || !actor->colChkInfo.health || (!s.marked && !s.tethered && !s.concussed && !s.frozen && !s.burning) || ++count>16)return;
            Mesh mesh;float turn=gPlayState->gameplayFrames*.035f;
            if(s.frozen) {mesh.Ring(29,3,{143,221,255,230});mesh.Ring(20,24,{173,231,255,210},turn);}
            else if(s.burning) {mesh.Ring(26,10,{255,123,44,220},turn);mesh.Ring(19,32,{255,208,70,190},-turn);}
            else if(s.marked) {mesh.Ring(26,35,{115,235,196,220},turn,2.7f);mesh.Ring(26,35,{115,235,196,220},turn+3.14f,2.7f);}
            else if(s.tethered) {mesh.Ring(23,15,{178,165,245,220});mesh.Ring(23,38,{178,165,245,220});}
            else mesh.Ring(29,20,{233,182,99,180},-turn,4.7f);
            DrawMesh(gPlayState,mesh,actor->world.pos,actor,44);
        });
    });
}
static RegisterShipInitFunc init(Register);
}
extern "C" int ARPG_BossWeaponDamage(Actor* actor,uint32_t flags,int nativeDamage) { return ARPG::Arsenal::BossWeaponDamage(actor,flags,nativeDamage); }

extern "C" int ARPG_BossWeaponDamageWithWeapon(Actor* actor,Actor* source,uint32_t flags,int nativeDamage) {
    int damage=ARPG::Arsenal::BossWeaponDamage(actor,flags,nativeDamage);
    if(damage<=0)return damage;
    return std::clamp<int>(std::lround(damage*ItemModifierSystem::BossWeaponMultiplier(actor,source,flags)),1,255);
}

extern "C" void ARPG_AdventureDetonate(Actor* actor) { ARPG::Arsenal::Detonate(actor); }
