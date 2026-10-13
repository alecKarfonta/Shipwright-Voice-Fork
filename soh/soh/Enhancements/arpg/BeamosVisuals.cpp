#include "BeamosVisuals.h"
#include "BeamosGeometry.h"
#include "ARPG.h"
#include "NativeMaterials.h"
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
 size_t count=0;for(const auto& part:BeamosBlenderData::parts)if(part.limb==limb)count+=part.count;
 if(!count)return nullptr;
 auto model=std::make_unique<Model>();model->vertices.resize(count);model->commands.resize(count+512);
 auto* p=model->commands.data();size_t offset=0;
 for(const auto& part:BeamosBlenderData::parts) {
  if(part.limb!=limb)continue;const auto ink=BeamosGeometry::Palette[part.material];
  for(uint32_t i=0;i<part.count;++i){const auto& source=BeamosBlenderData::vertices[BeamosBlenderData::indices[part.first+i]];auto& v=model->vertices[offset+i].v;
   v.ob[0]=source.x;v.ob[1]=source.y;v.ob[2]=source.z;v.cn[0]=uint8_t(source.nx);v.cn[1]=uint8_t(source.ny);v.cn[2]=uint8_t(source.nz);v.cn[3]=255;
   const auto role=BeamosGeometry::Surfaces[part.material];const auto surface=role==BeamosGeometry::Surface::Stone?NativeMaterials::Surface::Stone:NativeMaterials::Surface::Metal;
   // [TIME] | [INFO] | Beamos.UV | Project each triangle as a plane, using native model units consistently across its corners.
   const auto& a=BeamosBlenderData::vertices[BeamosBlenderData::indices[part.first+i-i%3]];
   const auto& b=BeamosBlenderData::vertices[BeamosBlenderData::indices[part.first+i-i%3+1]];
   const auto& c=BeamosBlenderData::vertices[BeamosBlenderData::indices[part.first+i-i%3+2]];
   const auto normal=GearGeometry::Cross({float(b.x-a.x),float(b.y-a.y),float(b.z-a.z)},{float(c.x-a.x),float(c.y-a.y),float(c.z-a.z)});
   NativeMaterials::UV(model->vertices[offset+i],{float(source.x),float(source.y),float(source.z)},normal,1200.f,surface);}
  const auto role=BeamosGeometry::Surfaces[part.material];
  p=NativeMaterials::Bind(p,role==BeamosGeometry::Surface::Stone?NativeMaterials::Surface::Stone:NativeMaterials::Surface::Metal,ink,false,role==BeamosGeometry::Surface::Glow);
  for(size_t batch=0;batch<part.count;batch+=30){int n=int(std::min<size_t>(30,part.count-batch));gSPVertex(p++,reinterpret_cast<uintptr_t>(model->vertices.data()+offset+batch),n,0);for(int j=0;j<n;j+=3)gSP1Triangle(p++,j,j+1,j+2,0);}
  offset+=part.count;
 }
 p=Gfx_SetupDL(p,25);gSPEndDisplayList(p++);model->commands.resize(p-model->commands.data());
 // [TIME] | [INFO] | BeamosVisuals.Cache | Four cached rigid meshes follow the original scanner pivots without allocating geometry each frame.
 auto* result=model->commands.data();models.emplace(limb,std::move(model));return result;
}
}
extern "C" void ARPG_OverrideBeamosLimb(PlayState* play,Actor* actor,s32 limb,Gfx** dl) {
 if(!dl||!*dl||!play||!actor||actor->id!=ACTOR_EN_VM||!ARPG::Enabled()||!CVarGetInteger(CVAR_ENHANCEMENT("ARPG.RemasteredEnemies"),1))return;
 const auto& eye=GET_ACTIVE_CAM(play)->eye;float x=actor->world.pos.x-eye.x,y=actor->world.pos.y-eye.y,z=actor->world.pos.z-eye.z;
 if(x*x+y*y+z*z>1500.f*1500.f)return;
 if(auto* model=ModelFor(limb))*dl=model;
}
