#include "TektiteBehavior.h"
#include "ARPG.h"
extern "C" {
#include "functions.h"
#include "overlays/actors/ovl_En_Tite/z_en_tite.h"
void EnTite_SetupIdle(EnTite*);
}
extern "C" s32 ARPG_TektiteWaterRecovery(Actor* actor,PlayState* play) {
    if(!play || !actor || actor->id!=ACTOR_EN_TITE || actor->params!=TEKTITE_BLUE || !ARPG::Enabled())return false;
    // Native flotation leaves gravity at zero. A recovery jump must be ballistic
    // until it meets water again, and water contact must finish the recovery.
    actor->gravity=-1.0f;
    if(!(actor->bgCheckFlags&BGCHECKFLAG_WATER) || actor->velocity.y>0.0f)return false;
    actor->world.pos.y+=actor->yDistToWater;
    actor->velocity.y=0.0f;actor->speedXZ=0.0f;actor->gravity=0.0f;
    actor->shape.rot.z=0;actor->shape.yOffset=0.0f;
    EffectSsGRipple_Spawn(play,&actor->world.pos,0,450,0);
    Audio_PlayActorSound2(actor,NA_SE_EN_TEKU_LAND_WATER);
    EnTite_SetupIdle(reinterpret_cast<EnTite*>(actor));
    return true;
}
