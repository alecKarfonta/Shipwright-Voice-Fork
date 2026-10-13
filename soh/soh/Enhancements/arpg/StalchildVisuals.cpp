#include "StalchildVisuals.h"
#include "StalchildGeometry.h"
#include "ARPG.h"
#include "soh/cvar_prefixes.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include <map>
#include <memory>
extern "C" {
#include "functions.h"
}
namespace {
struct Model { std::vector<Vtx> vertices;std::vector<Gfx> commands; };
std::map<int,std::unique_ptr<Model>> models;
Gfx* ModelFor(int limb) {
 using namespace ARPG;
 if(auto found=models.find(limb);found!=models.end())return found->second->commands.data();
 size_t count=0;for(const auto& part:StalchildBlenderData::parts)if(part.limb==limb)count+=part.count;
 if(!count)return nullptr;
 auto model=std::make_unique<Model>();model->vertices.resize(count);model->commands.resize(count+512);
 auto* p=model->commands.data();size_t offset=0;
 for(const auto& part:StalchildBlenderData::parts) {
  if(part.limb!=limb)continue;const auto ink=StalchildGeometry::Palette[part.material];
  for(uint32_t i=0;i<part.count;++i){const auto& source=StalchildBlenderData::vertices[StalchildBlenderData::indices[part.first+i]];auto& v=model->vertices[offset+i].v;
   v.ob[0]=source.x;v.ob[1]=source.y;v.ob[2]=source.z;v.cn[0]=uint8_t(source.nx);v.cn[1]=uint8_t(source.ny);v.cn[2]=uint8_t(source.nz);v.cn[3]=255;}
  gDPPipeSync(p++);gSPClearGeometryMode(p++,G_CULL_BOTH|G_TEXTURE_GEN|G_TEXTURE_GEN_LINEAR);
  gSPSetGeometryMode(p++,G_LIGHTING|G_FOG|G_ZBUFFER|G_SHADE|G_SHADING_SMOOTH);
  gDPSetCycleType(p++,G_CYC_2CYCLE);gDPSetRenderMode(p++,G_RM_FOG_SHADE_A,G_RM_AA_ZB_OPA_SURF2);gDPSetTextureLUT(p++,G_TT_NONE);
  gDPSetPrimColor(p++,0,0,ink.r,ink.g,ink.b,255);gSPTexture(p++,0,0,0,G_TX_RENDERTILE,G_OFF);
  // [TIME] | [INFO] | StalchildVisuals.Material | Only the inset embers use the native environment-color pulse; bones keep native lighting, fog and damage filters.
  if(part.material==4){gDPSetCombineLERP(p++,PRIMITIVE,0,ENVIRONMENT,0,0,0,0,1,0,0,0,COMBINED,0,0,0,1);}
  else{gDPSetCombineLERP(p++,PRIMITIVE,0,SHADE,0,0,0,0,1,0,0,0,COMBINED,0,0,0,1);}
  for(size_t batch=0;batch<part.count;batch+=30){int n=int(std::min<size_t>(30,part.count-batch));gSPVertex(p++,reinterpret_cast<uintptr_t>(model->vertices.data()+offset+batch),n,0);for(int j=0;j<n;j+=3)gSP1Triangle(p++,j,j+1,j+2,0);}
  offset+=part.count;
 }
 p=Gfx_SetupDL(p,25);gSPEndDisplayList(p++);model->commands.resize(p-model->commands.data());
 // [TIME] | [INFO] | StalchildVisuals.Debris | Stable rigid display lists survive the parent and need no flex palette when the native BodyBreak actor draws them later.
 auto* result=model->commands.data();models.emplace(limb,std::move(model));return result;
}
}
extern "C" void ARPG_OverrideStalchildLimb(PlayState* play,Actor* actor,s32 limb,Gfx** dl) {
 if(!dl||!*dl||!play||!actor||actor->id!=ACTOR_EN_SKB||!ARPG::Enabled()||!CVarGetInteger(CVAR_ENHANCEMENT("ARPG.RemasteredEnemies"),1))return;
 const auto& eye=GET_ACTIVE_CAM(play)->eye;float x=actor->world.pos.x-eye.x,y=actor->world.pos.y-eye.y,z=actor->world.pos.z-eye.z;
 if(x*x+y*y+z*z>1500.f*1500.f)return;
 if(auto* model=ModelFor(limb))*dl=model;
}
