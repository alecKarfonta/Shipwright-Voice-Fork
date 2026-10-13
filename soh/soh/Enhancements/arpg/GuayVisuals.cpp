#include "GuayVisuals.h"
#include "GuayGeometry.h"
#include "ARPG.h"
#include "NativeMaterials.h"
#include "soh/cvar_prefixes.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include <map>
#include <memory>
extern "C" {
#include "functions.h"
#include "objects/object_crow/object_crow.h"
}
namespace {
ARPG::NativeMaterials::Surface Material(int index){using namespace ARPG;return GuayGeometry::Surfaces[index]==GuayGeometry::Surface::Feather?NativeMaterials::Surface::Fur:NativeMaterials::Surface::Bone;}
struct Model { std::vector<Vtx> vertices;std::vector<Gfx> commands; };
std::map<int,std::unique_ptr<Model>> models;
Gfx* ModelFor(int limb) {
 using namespace ARPG;
 if(auto found=models.find(limb);found!=models.end())return found->second->commands.data();
 size_t count=0;for(const auto& part:GuayBlenderData::parts)if(part.limb==limb)count+=part.count;
 if(!count)return nullptr;
 auto model=std::make_unique<Model>();model->vertices.resize(count);model->commands.resize(count*4+1024);
 auto* p=model->commands.data();size_t offset=0;int loaded=limb;
 auto load=[&](int joint){if(loaded==joint)return;auto matrix=uintptr_t(0x0D000001)+sizeof(Mtx)*GuayGeometry::MatrixIndex(joint);gSPMatrix(p++,matrix,G_MTX_MODELVIEW|G_MTX_LOAD|G_MTX_NOPUSH);loaded=joint;};
 for(const auto& part:GuayBlenderData::parts) {
  if(part.limb!=limb)continue;const auto ink=GuayGeometry::Palette[part.material];
  for(uint32_t i=0;i<part.count;++i){const auto& source=GuayBlenderData::vertices[GuayBlenderData::indices[part.first+i]];auto& v=model->vertices[offset+i].v;
   v.ob[0]=source.x;v.ob[1]=source.y;v.ob[2]=source.z;v.cn[0]=uint8_t(source.nx);v.cn[1]=uint8_t(source.ny);v.cn[2]=uint8_t(source.nz);v.cn[3]=255;
   const auto surface=Material(part.material);
   // [TIME] | [INFO] | Guay.UV | Project each triangle as a plane, using native model units consistently across its corners.
   const auto& a=GuayBlenderData::vertices[GuayBlenderData::indices[part.first+i-i%3]];
   const auto& b=GuayBlenderData::vertices[GuayBlenderData::indices[part.first+i-i%3+1]];
   const auto& c=GuayBlenderData::vertices[GuayBlenderData::indices[part.first+i-i%3+2]];
   const auto normal=GearGeometry::Cross({float(b.x-a.x),float(b.y-a.y),float(b.z-a.z)},{float(c.x-a.x),float(c.y-a.y),float(c.z-a.z)});
   NativeMaterials::UV(model->vertices[offset+i],{float(source.x),float(source.y),float(source.z)},normal,1200.f,surface);}
  const auto role=GuayGeometry::Surfaces[part.material];
  p=NativeMaterials::Bind(p,Material(part.material),ink,false,role==GuayGeometry::Surface::Eye);
  // [TIME] | [INFO] | Guay.Material | Native purple feather paint retains the original bird's identity; the beak remains scene-lit and only the small eyes are unlit.
  if(role==GuayGeometry::Surface::Feather){gDPLoadTextureBlock(p++,gGuayBodyTex,G_IM_FMT_RGBA,G_IM_SIZ_16b,16,16,0,G_TX_WRAP,G_TX_WRAP,4,4,0,0);}
  for(size_t batch=0;batch<part.count;batch+=24){int n=int(std::min<size_t>(24,part.count-batch));
   for(int first=0;first<n;){int joint=GuayBlenderData::vertices[GuayBlenderData::indices[part.first+batch+first]].joint,last=first+1;
    while(last<n&&GuayBlenderData::vertices[GuayBlenderData::indices[part.first+batch+last]].joint==joint)++last;
    load(joint);gSPVertex(p++,reinterpret_cast<uintptr_t>(model->vertices.data()+offset+batch+first),last-first,first);first=last;}
   for(int j=0;j<n;j+=3)gSP1Triangle(p++,j,j+1,j+2,0);
  }
  offset+=part.count;
 }
 load(limb);p=Gfx_SetupDL(p,25);gSPEndDisplayList(p++);model->commands.resize(p-model->commands.data());
 // [TIME] | [INFO] | GuayVisuals.Cache | Seven cached meshes use the original flex matrix palette, including connected wing and tail vertices.
 auto* result=model->commands.data();models.emplace(limb,std::move(model));return result;
}
}
extern "C" void ARPG_OverrideGuayLimb(PlayState* play,Actor* actor,s32 limb,Gfx** dl) {
 if(!dl||!*dl||!play||!actor||actor->id!=ACTOR_EN_CROW||!ARPG::Enabled()||!CVarGetInteger(CVAR_ENHANCEMENT("ARPG.RemasteredEnemies"),1))return;
 const auto& eye=GET_ACTIVE_CAM(play)->eye;float x=actor->world.pos.x-eye.x,y=actor->world.pos.y-eye.y,z=actor->world.pos.z-eye.z;
 if(x*x+y*y+z*z>1500.f*1500.f)return;
 if(auto* model=ModelFor(limb))*dl=model;
}
