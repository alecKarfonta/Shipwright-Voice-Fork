#include "SkillAuras.h"
#include "AuraGeometry.h"
#include "NativeEffects.h"
#include "SkillSystem.h"
#include "ARPG.h"
#include "soh/ObjectExtension/ObjectExtension.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/frame_interpolation.h"
extern "C" {
#include "functions.h"
#include "variables.h"
#include "macros.h"
#include "overlays/actors/ovl_En_M_Thunder/z_en_m_thunder.h"
}

namespace ARPG::SkillAuras {
namespace {
struct Aura { int skill,rune,age=0,remaining=0; bool followsBuff=false,fixed=false; Vec3f position{}; s16 yaw=0; };
static ObjectExtension::Register<Aura> registration;
void Destroy(Actor* actor,PlayState* play) {
    auto* effect=reinterpret_cast<EnMThunder*>(actor);
    Collider_DestroyCylinder(play,&effect->collider);
    LightContext_RemoveLight(play,&play->lightCtx,effect->lightNode);
}
void Update(Actor* actor,PlayState* play) {
    auto* data=ObjectExtension::GetInstance().Get<Aura>(actor);
    if(!data || !SkillsEnabled() || play->transitionTrigger!=TRANS_TRIGGER_OFF || gSaveContext.health<=0) { Actor_Kill(actor);return; }
    if(GameInteractor::IsGameplayPaused() || !GameInteractor::IsPlayerInControl()) { return; }
    data->remaining=data->followsBuff?SkillSystem::BuffRemaining(data->skill):data->remaining-1;
    if(data->remaining<=0) { Actor_Kill(actor);return; }
    auto* player=GET_PLAYER(play);
    if(!data->fixed) {
        data->position=data->skill==ARPG_SKILL_MAGIC_INFUSE?player->bodyPartsPos[PLAYER_BODYPART_L_HAND]:player->actor.world.pos;
        data->yaw=player->actor.shape.rot.y;
    }
    actor->world.pos=data->position;
    ++data->age;
}
// [TIME] | [INFO] | SkillAuras.Draw | Sigils share gear motifs, are depth tested in the world, and never register combat colliders.
void Draw(Actor* actor,PlayState* play) {
    auto* data=ObjectExtension::GetInstance().Get<Aura>(actor);
    if(!data || !SkillsEnabled()) { return; }
    int element=SkillSystem::InfusionFlags()==DMG_MAGIC_ICE?1:SkillSystem::InfusionFlags()==DMG_MAGIC_LIGHT?2:0;
    using namespace NativeEffects;
    Mesh fx(play,data->yaw);
    float age=data->age,turn=age*.055f;
    float fade=std::min({1.f,(age+1)/5.f,data->remaining/10.f});
    auto color=Fade(element==1?ice:element==2?gold:fire,fade);
    switch(data->skill) {
        case ARPG_SKILL_MAGIC_INFUSE:
            fx.Sprite(Texture::Glow,{0,0,0},8,8,Fade(color,.6f));
            for(int i=0;i<3;++i)fx.Sprite(Texture::Glow,{std::sin(turn+i*2.1f)*8,4+std::cos(turn+i)*5,std::cos(turn+i*2.1f)*8},2,3,color);
            break;
        case ARPG_SKILL_GUARD_RIPOSTE:
            fx.Arc(38,5,32,-.85f,1.7f,Fade(ice,fade));
            fx.Sprite(Texture::Glow,{0,36,32},7,12,Fade(gold,fade*.5f));break;
        case ARPG_SKILL_MAGIC_HOLY_AURA:
        case ARPG_SKILL_GUARD_WARCRY:
            fx.Arc(data->skill==ARPG_SKILL_MAGIC_HOLY_AURA?52:60,4,3,turn,2*pi,Fade(gold,fade*.5f));
            for(int i=0;i<4;++i)fx.Sprite(Texture::Glow,{std::sin(turn+i*pi/2)*32,15+std::fmod(age+i*17,48.f),std::cos(turn+i*pi/2)*32},3,4,Fade(gold,fade*.6f));
            break;
        case ARPG_SKILL_GUARD_DODGE:
            for(int i=0;i<5;++i)fx.Sprite(Texture::Dust,{std::sin(i*2.4f)*18,5+age*.6f,age*1.5f-i*6},7+age*.5f,6+age*.4f,Fade(dust,fade));break;
        case ARPG_SKILL_MAGIC_RECALL:
        case ARPG_SKILL_GUARD_SECOND_WIND:
            for(int i=0;i<6;++i)fx.Sprite(Texture::Glow,{std::sin(turn+i)*23,8+std::fmod(age*1.4f+i*13,65.f),std::cos(turn+i)*23},3,5,Fade(green,fade));
            if(data->skill==ARPG_SKILL_MAGIC_RECALL)fx.Arc(32,5,6+age*.8f,turn,2*pi,Fade(green,fade*.5f));
            break;
    }
    fx.Draw(play,data->position,data->yaw,actor,0,LINK_IS_CHILD?.72f:1.f);

}
}
int LiveCount(PlayState* play) {
    if(!play) { return 0; }
    int count=0;
    for(auto* actor=play->actorCtx.actorLists[ACTORCAT_ITEMACTION].head;actor;actor=actor->next) {
        if(actor->update && ObjectExtension::GetInstance().Has<Aura>(actor)) { ++count; }
    }
    return count;
}
bool Start(PlayState* play,int skill,int rune,int duration,bool followsBuff,const Vec3f* position) {
    if(!play || !SkillsEnabled() || !AuraGeometry::Supported(skill) || duration<=0 || play->actorCtx.total>=180 || LiveCount(play)>=8) { return false; }
    auto* player=GET_PLAYER(play);
    if(player->stateFlags2&PLAYER_STATE2_SPIN_ATTACKING) { return false; }
    Vec3f origin=position?*position:player->actor.world.pos;
    auto* actor=Actor_Spawn(&play->actorCtx,play,ACTOR_EN_M_THUNDER,origin.x,origin.y,origin.z,0,0,0,1);
    if(!actor || !actor->update) { return false; }
    ObjectExtension::GetInstance().Set<Aura>(actor,Aura{skill,rune,0,duration,followsBuff,position!=nullptr,origin,player->actor.shape.rot.y});
    actor->update=Update;actor->draw=Draw;actor->destroy=Destroy;
    actor->flags|=ACTOR_FLAG_UPDATE_CULLING_DISABLED|ACTOR_FLAG_DRAW_CULLING_DISABLED;
    actor->world.pos=origin;
    return true;
}
void CancelAll(PlayState* play) {
    if(!play) { return; }
    for(auto* actor=play->actorCtx.actorLists[ACTORCAT_ITEMACTION].head;actor;actor=actor->next) {
        if(ObjectExtension::GetInstance().Has<Aura>(actor)) { Actor_Kill(actor); }
    }
}
}
