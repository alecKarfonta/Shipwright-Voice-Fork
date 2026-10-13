#include "GuayBehavior.h"
#include "XPSystem.h"
#include "ItemModifierSystem.h"
#include "soh/ObjectExtension/ActorMaximumHealth.h"
#include <algorithm>
extern "C" void ARPG_GuayRespawned(Actor* actor){
 if(!actor||actor->id!=ACTOR_EN_CROW)return;
 // [TIME] | [INFO] | Guay.Respawn | Only a completed native respawn begins a new life. Preserve its existing scaled maximum and affix identity, and allow one earned reward for that new life.
 actor->colChkInfo.health=std::max<uint8_t>(1,GetActorMaximumHealth(actor));
 XPSystem::ResetDefeatCredit(actor);ItemModifierSystem::ResetDefeatCredit(actor);
}
