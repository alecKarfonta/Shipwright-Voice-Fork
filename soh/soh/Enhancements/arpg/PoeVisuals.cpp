#include "PoeVisuals.h"
#include "PoeGeometry.h"
#include "ARPG.h"
#include "NativeMaterials.h"
#include "soh/cvar_prefixes.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include <map>
#include <memory>
extern "C" {
#include "functions.h"
#include "overlays/actors/ovl_En_Poh/z_en_poh.h"
}
namespace {
struct Part { int material;std::vector<Vtx> vertices;std::vector<Gfx> commands; };
using Model=std::vector<std::unique_ptr<Part>>;
std::map<int,Model> models;
ARPG::NativeMaterials::Surface Material(int index){using namespace ARPG;auto role=PoeGeometry::Surfaces[index];return role==PoeGeometry::Surface::Cloth?NativeMaterials::Surface::Cloth:role==PoeGeometry::Surface::Skin?NativeMaterials::Surface::Bone:NativeMaterials::Surface::Metal;}
const Model& ModelFor(int limb){
 using namespace ARPG;
 if(auto found=models.find(limb);found!=models.end())return found->second;
 Model model;
 for(const auto& source:PoeBlenderData::parts){if(source.limb!=limb)continue;
  auto part=std::make_unique<Part>();part->material=source.material;part->vertices.resize(source.count);part->commands.resize(source.count+16);
  for(uint32_t i=0;i<source.count;++i){const auto& v=PoeBlenderData::vertices[PoeBlenderData::indices[source.first+i]];auto& out=part->vertices[i].v;
   out.ob[0]=v.x;out.ob[1]=v.y;out.ob[2]=v.z;out.cn[0]=uint8_t(v.nx);out.cn[1]=uint8_t(v.ny);out.cn[2]=uint8_t(v.nz);out.cn[3]=255;
   const auto& a=PoeBlenderData::vertices[PoeBlenderData::indices[source.first+i-i%3]];
   const auto& b=PoeBlenderData::vertices[PoeBlenderData::indices[source.first+i-i%3+1]];
   const auto& c=PoeBlenderData::vertices[PoeBlenderData::indices[source.first+i-i%3+2]];
   auto normal=GearGeometry::Cross({float(b.x-a.x),float(b.y-a.y),float(b.z-a.z)},{float(c.x-a.x),float(c.y-a.y),float(c.z-a.z)});
   NativeMaterials::UV(part->vertices[i],{float(v.x),float(v.y),float(v.z)},normal,1600.f,Material(source.material));
  }
  auto* p=part->commands.data();
  for(size_t batch=0;batch<source.count;batch+=30){int n=int(std::min<size_t>(30,source.count-batch));gSPVertex(p++,reinterpret_cast<uintptr_t>(part->vertices.data()+batch),n,0);for(int j=0;j<n;j+=3)gSP1Triangle(p++,j,j+1,j+2,0);}
  gSPEndDisplayList(p++);part->commands.resize(p-part->commands.data());model.push_back(std::move(part));
 }
 return models.emplace(limb,std::move(model)).first->second;
}
Gfx* DrawModel(PlayState* play,EnPoh* actor,int limb,Gfx* nativeLantern=nullptr){
 using namespace ARPG;const auto& model=ModelFor(limb);if(model.empty())return nativeLantern;
 // [TIME] | [INFO] | PoeVisuals.Fade | Immutable geometry lists are shared; short frame-owned material wrappers preserve each ghost's native fade and attack/damage tint independently.
 auto* start=static_cast<Gfx*>(Graph_Alloc(play->state.gfxCtx,sizeof(Gfx)*(model.size()*64+32)));auto* p=start;
 if(nativeLantern)gSPDisplayList(p++,nativeLantern);
 bool fading=!nativeLantern&&actor->lightColor.a<255;
 gDPPipeSync(p++);gDPSetEnvColor(p++,actor->lightColor.r,actor->lightColor.g,actor->lightColor.b,nativeLantern?255:actor->lightColor.a);
 for(const auto& part:model){auto c=nativeLantern?PoeGeometry::Palette[part->material]:PoeGeometry::Tint(part->material,fading?255:actor->lightColor.r,fading?255:actor->lightColor.g,fading?255:actor->lightColor.b);
  p=NativeMaterials::Bind(p,Material(part->material),c,fading,PoeGeometry::Surfaces[part->material]==PoeGeometry::Surface::Glow);
  gSPDisplayList(p++,part->commands.data());
 }
 p=Gfx_SetupDL(p,25);gSPEndDisplayList(p++);return start;
}
}
extern "C" s32 ARPG_UsePoeVisuals(PlayState* play,Actor* actor){
 if(!play||!actor||actor->id!=ACTOR_EN_POH||actor->params<0||actor->params>EN_POH_RUPEE||!ARPG::Enabled()||!CVarGetInteger(CVAR_ENHANCEMENT("ARPG.RemasteredEnemies"),1))return false;
 const auto& eye=GET_ACTIVE_CAM(play)->eye;float x=actor->world.pos.x-eye.x,y=actor->world.pos.y-eye.y,z=actor->world.pos.z-eye.z;return x*x+y*y+z*z<=1500.f*1500.f;
}
extern "C" void ARPG_OverridePoeLimb(PlayState* play,Actor* actor,s32 limb,Gfx** dl){
 if(!dl||!*dl||limb==18||!ARPG_UsePoeVisuals(play,actor)||!ARPG::PoeGeometry::Visible(limb))return;
 *dl=DrawModel(play,reinterpret_cast<EnPoh*>(actor),limb);
}
extern "C" Gfx* ARPG_PoeLanternDisplayList(PlayState* play,Actor* actor,Gfx* original){
 if(!original||!ARPG_UsePoeVisuals(play,actor))return original;
 // [TIME] | [INFO] | PoeVisuals.Lantern | The original glowing lamp and light remain authoritative; the fitted frame follows both the held matrix and the falling-lantern actor.
 return DrawModel(play,reinterpret_cast<EnPoh*>(actor),18,original);
}
