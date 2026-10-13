#include "ArmosVisuals.h"
#include "ArmosGeometry.h"
#include "ARPG.h"
#include "soh/cvar_prefixes.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include <map>
#include <memory>
extern "C" {
#include "functions.h"
#include "objects/object_am/object_am.h"
#include "overlays/actors/ovl_En_Am/z_en_am.h"
}
namespace {
struct Part { int material; std::vector<Vtx> vertices;std::vector<Gfx> commands; };
using Model=std::vector<std::unique_ptr<Part>>;
std::map<int,Model> models;
const Model& ModelFor(int limb) {
 if(auto found=models.find(limb);found!=models.end())return found->second;
 Model model;
 for(const auto& source:ARPG::ArmosBlenderData::parts) {
  if(source.limb!=limb)continue;
  auto part=std::make_unique<Part>();part->material=source.material;part->vertices.resize(source.count);part->commands.resize(source.count+64);
  for(uint32_t i=0;i<source.count;++i){const auto& v=ARPG::ArmosBlenderData::vertices[ARPG::ArmosBlenderData::indices[source.first+i]];auto& out=part->vertices[i].v;
   out.ob[0]=v.x;out.ob[1]=v.y;out.ob[2]=v.z;out.cn[0]=uint8_t(v.nx);out.cn[1]=uint8_t(v.ny);out.cn[2]=uint8_t(v.nz);out.cn[3]=255;
   float x=std::abs(int(v.nx)),y=std::abs(int(v.ny)),z=std::abs(int(v.nz));out.tc[0]=int16_t((x>y&&x>z?v.z:v.x)*32*32/1000);out.tc[1]=int16_t((y>x&&y>z?v.z:-v.y)*32*32/1000);
  }
  auto* p=part->commands.data();gDPPipeSync(p++);gSPClearGeometryMode(p++,G_CULL_BOTH|G_TEXTURE_GEN|G_TEXTURE_GEN_LINEAR);gSPSetGeometryMode(p++,G_LIGHTING|G_FOG|G_ZBUFFER|G_SHADE|G_SHADING_SMOOTH);
  gDPSetCycleType(p++,G_CYC_2CYCLE);gDPSetRenderMode(p++,G_RM_FOG_SHADE_A,G_RM_AA_ZB_OPA_SURF2);gDPSetTextureLUT(p++,G_TT_NONE);
  // [TIME] | [INFO] | ArmosVisuals.Material | Native ENV alpha is an awakening blend, never model opacity. Stone uses the original intensity texture beneath native lights and damage filters.
  if(source.material==0){gSPTexture(p++,0xFFFF,0xFFFF,0,G_TX_RENDERTILE,G_ON);gDPLoadTextureBlock_4b(p++,object_amTex_002638,G_IM_FMT_I,32,32,0,G_TX_WRAP,G_TX_WRAP,5,5,0,0);
   gDPSetCombineLERP(p++,TEXEL0,PRIMITIVE,PRIM_LOD_FRAC,PRIMITIVE,0,0,0,1,COMBINED,0,SHADE,0,0,0,0,1);
  }else{gSPTexture(p++,0,0,0,G_TX_RENDERTILE,G_OFF);gDPSetCombineLERP(p++,PRIMITIVE,0,SHADE,0,0,0,0,1,0,0,0,COMBINED,0,0,0,1);}
  for(size_t batch=0;batch<source.count;batch+=30){int n=int(std::min<size_t>(30,source.count-batch));gSPVertex(p++,reinterpret_cast<uintptr_t>(part->vertices.data()+batch),n,0);for(int j=0;j<n;j+=3)gSP1Triangle(p++,j,j+1,j+2,0);}
  gSPEndDisplayList(p++);part->commands.resize(p-part->commands.data());model.push_back(std::move(part));
 }
 return models.emplace(limb,std::move(model)).first->second;
}
}
extern "C" s32 ARPG_UseArmosVisuals(PlayState* play,Actor* actor) {
 if(!play||!actor||actor->id!=ACTOR_EN_AM||!ARPG::Enabled()||!CVarGetInteger(CVAR_ENHANCEMENT("ARPG.RemasteredEnemies"),1))return false;
 const auto& eye=GET_ACTIVE_CAM(play)->eye;float x=actor->world.pos.x-eye.x,y=actor->world.pos.y-eye.y,z=actor->world.pos.z-eye.z;return x*x+y*y+z*z<=1500.f*1500.f;
}
extern "C" void ARPG_OverrideArmosLimb(PlayState* play,Actor* actor,s32 limb,Gfx** dl) {
 if(!dl||!*dl||!ARPG_UseArmosVisuals(play,actor)||ARPG::ArmosGeometry::MatrixIndex(limb)<0)return;
 const auto& model=ModelFor(limb);if(model.empty())return;
 // [TIME] | [INFO] | ArmosVisuals.Awakening | Cache rigid geometry once; allocate only short material wrappers per frame so each actor keeps its own continuous native awakening color.
 auto* start=static_cast<Gfx*>(Graph_Alloc(play->state.gfxCtx,sizeof(Gfx)*(model.size()*3+24)));auto* p=start;
 const auto blend=reinterpret_cast<EnAm*>(actor)->textureBlend;
 for(const auto& part:model){auto c=ARPG::ArmosGeometry::Blend(part->material,blend);gDPPipeSync(p++);gDPSetPrimColor(p++,0,24,c.r,c.g,c.b,255);gSPDisplayList(p++,part->commands.data());}
 p=Gfx_SetupDL(p,25);gSPEndDisplayList(p++);*dl=start;
}
