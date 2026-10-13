#include "ExplorationRules.h"
#include "ARPG.h"
#include "ARPGSave.h"
#include "ARPGBridge.h"
#include "EnemySystem.h"
#include "ExpeditionSystem.h"
#include "ItemModifierSystem.h"
#include "SkillSystem.h"
#include "soh/ShipInit.hpp"
#include "soh/Notification/Notification.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/cvar_prefixes.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include <set>
#include <cmath>
extern "C" {
#include "functions.h"
#include "variables.h"
#include "macros.h"
#include "overlays/actors/ovl_En_Box/z_en_box.h"
}
namespace ARPG::Exploration {
namespace {
constexpr int Marker=0x6000;
std::set<int> attemptedRooms;
int roomSeen=-1;
uint32_t readyAt=0;
bool Enabled() { return ARPG::Enabled() && !IS_RANDO && !Expeditions::Busy() && CVarGetInteger(CVAR_ENHANCEMENT("ARPG.ExplorationSecrets"),1); }
int Id(Actor* actor) {
    if(!actor || (actor->id!=ACTOR_EN_BOX && actor->id!=ACTOR_EN_SW && actor->id!=ACTOR_EN_SI))return -1;
    int id=actor->home.rot.z-Marker;
    return id>=0 && id<ARPG_SECRET_COUNT ? id : -1;
}
void Bounties() {
    int tier=RewardTier(gSaveContext.inventory.gsTokens,gARPGSave);
    if(tier<=gARPGSave.skullRewardTier)return;
    int essence=0,points=0;
    while(gARPGSave.skullRewardTier<tier) {
        int next=++gARPGSave.skullRewardTier;
        essence+=5;
        ItemModifierSystem::GiveMaterial(1+(next-1)%6);
        if(next%4==0 && next<=24) { gARPGSave.activePoints=std::min<int>(255,gARPGSave.activePoints+1);++points; }
    }
    ItemModifierSystem::GiveMaterial(Loot::Essence,essence);
    Notification::Emit({.message=points ? "Skulltula bounty!\nGem, essence and technique point." : "Skulltula bounty!\nGem and forge essence.",.remainingTime=5.f});
}
void Reward(int id) {
    if(!Claim(gARPGSave,id))return;
    if(Skull(id)) {
        ItemModifierSystem::GiveMaterial(Loot::Essence,2);
        Notification::Emit({.message="Wild Gold Skulltula found!\n+2 forge essence",.remainingTime=4.f});
        Bounties();
    } else {
        ItemModifierSystem::GiveMaterial(Loot::Essence,3);
        // Finite exploration caches, not repeatable enemy farming. Contents are
        // stable for this save and independent of unrelated combat RNG draws.
        uint32_t rng=gARPGSave.seed ^ (uint32_t(id+1)*2654435761u);
        if(Rules::RandomRange(rng,0,3)==0)ItemModifierSystem::GiveMaterial(1+Rules::RandomRange(rng,0,5));
        Notification::Emit({.message="Hidden cache found!\n+3 forge essence",.remainingTime=4.f});
    }
}
bool Live(int id) {
    for(auto& list:gPlayState->actorCtx.actorLists)for(auto* a=list.head;a;a=a->next)if(a->update && Id(a)==id)return true;
    return false;
}
void Populate() {
    if(!gPlayState || !GameInteractor::IsSaveLoaded(true) || !Enabled() || !GameInteractor::IsPlayerInControl())return;
    auto* play=gPlayState;int room=play->roomCtx.curRoom.num;
    if(room<0 || room>=64 || play->roomCtx.status || (play->numRooms>1 && play->roomCtx.prevRoom.num>=0) || play->transitionTrigger!=TRANS_TRIGGER_OFF)return;
    if(roomSeen!=room) { roomSeen=room;readyAt=play->gameplayFrames+30;attemptedRooms.erase(room); }
    if(play->gameplayFrames<readyAt || attemptedRooms.count(room))return;
    int first=FirstId(play->sceneNum,LINK_IS_ADULT);if(first<0)return;
    auto sites=Enemies::ExplorationSites();
    for(int n=0;n<3;++n) {
        int id=first+n;auto& saved=gARPGSave.secrets[id];
        if(saved.claimed || Live(id) || (saved.placed && saved.room!=room))continue;
        if(!saved.placed) {
            for(const auto& p:sites) {
                bool apart=true;
                for(int j=first;j<first+3;++j)if(gARPGSave.secrets[j].placed &&
                   std::hypot(p.x-gARPGSave.secrets[j].x,p.z-gARPGSave.secrets[j].z)<280)apart=false;
                if(!apart)continue;
                saved={p.x,p.y,p.z,int8_t(room),1,0};break;
            }
        }
        if(!saved.placed)continue;
        Vec3f p{saved.x,saved.y,saved.z};if(!Enemies::QuestSiteSupported(p))continue;
        if(play->actorCtx.total>=175)continue;
        // Gold spiders use a zero native token-bit mask, so they can never
        // overwrite a vanilla Skulltula location. Our save owns their receipt.
        Actor* actor=Actor_Spawn(&play->actorCtx,play,Skull(id)?ACTOR_EN_SW:ACTOR_EN_BOX,
            p.x,p.y,p.z,0,0,Skull(id)?0:Marker+id,
            Skull(id)?0x2100:ENBOX_PARAMS(ENBOX_TYPE_SMALL,GI_RUPEE_BLUE,0));
        if(actor && actor->update) {
            actor->home.rot.z=Marker+id;
            actor->room=room;
            ARPG::Log("info", std::string("Exploration ")+(Skull(id)?"Skulltula":"cache")+" #"+std::to_string(id)+
                " placed in scene "+std::to_string(play->sceneNum)+" room "+std::to_string(room)+" at "+
                std::to_string(p.x)+", "+std::to_string(p.y)+", "+std::to_string(p.z));
        }
    }
    attemptedRooms.insert(room);
    Bounties();
}
void Register() {
    COND_HOOK(OnGameFrameUpdate,true,Populate);
    COND_HOOK(OnPlayDestroy,true,[](){attemptedRooms.clear();roomSeen=-1;readyAt=0;});
    COND_HOOK(OnFlagSet,true,[](int16_t type,int16_t){if(type==FLAG_GS_TOKEN && Enabled())Bounties();});
    COND_VB_SHOULD(VB_CHEST_CONSIDER_CHEST_OPEN,true,{
        auto* actor=va_arg(args,Actor*);int id=Id(actor);
        if(id>=0) *should=gARPGSave.secrets[id].claimed;
    });
    COND_VB_SHOULD(VB_CHEST_SET_TREASURE_FLAG,true,{
        auto* actor=va_arg(args,Actor*);int id=Id(actor);
        if(id>=0) { *should=false;Reward(id); }
    });
    COND_VB_SHOULD(VB_GIVE_ITEM_SKULL_TOKEN,true,{
        auto* actor=va_arg(args,Actor*);int id=Id(actor);
        if(id>=0) { *should=false;Reward(id);Audio_PlayFanfare(NA_BGM_SMALL_ITEM_GET);Actor_Kill(actor); }
    });
}
static RegisterShipInitFunc init(Register);
}
int SecretId(Actor* actor) { return Id(actor); }
void CopySecret(Actor* child,Actor* parent) {
    int id=Id(parent);if(child && id>=0)child->home.rot.z=Marker+id;
}
}
extern "C" void ARPG_CopyExplorationSecret(Actor* child,Actor* parent) { ARPG::Exploration::CopySecret(child,parent); }

extern "C" int ARPG_IsExplorationSecret(Actor* actor) { return ARPG::Exploration::SecretId(actor)>=0; }
