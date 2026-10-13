#include "PeahatVisuals.h"
#include "PeahatGeometry.h"
#include "ARPG.h"
#include "soh/cvar_prefixes.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include <map>
#include <memory>
extern "C" {
#include "functions.h"
#include "objects/object_peehat/object_peehat.h"

}
namespace {
struct Model { std::vector<Vtx> vertices;std::vector<Gfx> commands; };
std::map<int,std::unique_ptr<Model>> models;
Gfx* ModelFor(int limb,int variant) {
    using namespace ARPG;const int key=limb+variant*32;
    if(auto found=models.find(key);found!=models.end())return found->second->commands.data();
    size_t count=0;for(const auto& part:PeahatBlenderData::parts)if(part.limb==limb)count+=part.count;
    if(!count)return nullptr;
    auto model=std::make_unique<Model>();model->vertices.resize(count);model->commands.resize(count*4+2048);
    auto* p=model->commands.data();size_t offset=0;
    for(const auto& part:PeahatBlenderData::parts) {
        if(part.limb!=limb)continue;
        const auto ink=PeahatGeometry::Palette[variant][part.material];
        const bool texture=part.material==0 || part.material==2;
        const int width=8,height=part.material==2?32:8;
        for(uint32_t i=0;i<part.count;++i) {
            const auto& source=PeahatBlenderData::vertices[PeahatBlenderData::indices[part.first+i]];auto& v=model->vertices[offset+i].v;
            v.ob[0]=source.x;v.ob[1]=source.y;v.ob[2]=source.z;v.cn[0]=uint8_t(source.nx);v.cn[1]=uint8_t(source.ny);v.cn[2]=uint8_t(source.nz);v.cn[3]=255;
            float x=std::abs(int(source.nx)),y=std::abs(int(source.ny)),z=std::abs(int(source.nz));
            v.tc[0]=int16_t((x>y && x>z?source.z:source.x)*width*32/900);
            v.tc[1]=int16_t((y>x && y>z?source.z:-source.y)*height*32/900);
        }
        gDPPipeSync(p++);gSPClearGeometryMode(p++,G_CULL_BOTH|G_TEXTURE_GEN|G_TEXTURE_GEN_LINEAR);
        gSPSetGeometryMode(p++,G_LIGHTING|G_FOG|G_ZBUFFER|G_SHADE|G_SHADING_SMOOTH);
        gDPSetCycleType(p++,G_CYC_2CYCLE);gDPSetRenderMode(p++,G_RM_FOG_SHADE_A,G_RM_AA_ZB_OPA_SURF2);gDPSetTextureLUT(p++,G_TT_NONE);
        gDPSetPrimColor(p++,0,24,ink.r,ink.g,ink.b,255);
        // [TIME] | [INFO] | PeahatVisuals.Material | Original lighting, fog and damage filters shade the bark and thin leaf surfaces.
        if(texture){gSPTexture(p++,0xFFFF,0xFFFF,0,G_TX_RENDERTILE,G_ON);
            gDPLoadTextureBlock(p++,part.material==2?gPeehatWingTex:gPeehatLowerBodyTex,G_IM_FMT_RGBA,G_IM_SIZ_16b,width,height,0,G_TX_WRAP,G_TX_WRAP,3,part.material==2?5:3,0,0);
            gDPSetCombineLERP(p++,TEXEL0,PRIMITIVE,PRIM_LOD_FRAC,PRIMITIVE,0,0,0,1,COMBINED,0,SHADE,0,0,0,0,1);
        }else{gSPTexture(p++,0,0,0,G_TX_RENDERTILE,G_OFF);
            gDPSetCombineLERP(p++,PRIMITIVE,0,SHADE,0,0,0,0,1,0,0,0,COMBINED,0,0,0,1);}
        for(size_t batch=0;batch<part.count;batch+=24){int n=int(std::min<size_t>(24,part.count-batch));
            gSPVertex(p++,reinterpret_cast<uintptr_t>(model->vertices.data()+offset+batch),n,0);
            for(int j=0;j<n;j+=3)gSP1Triangle(p++,j,j+1,j+2,0);
        }
        offset+=part.count;
    }
    p=Gfx_SetupDL(p,25);gSPEndDisplayList(p++);model->commands.resize(p-model->commands.data());
    auto* result=model->commands.data();models.emplace(key,std::move(model));return result;
}
}
extern "C" s32 ARPG_UsePeahatVisuals(PlayState* play,Actor* actor) {
    if(!play||!actor||(actor->id!=ACTOR_EN_PEEHAT)||!ARPG::Enabled()||!CVarGetInteger(CVAR_ENHANCEMENT("ARPG.RemasteredEnemies"),1))return false;
    const auto& eye=GET_ACTIVE_CAM(play)->eye;float x=actor->world.pos.x-eye.x,y=actor->world.pos.y-eye.y,z=actor->world.pos.z-eye.z;
    return x*x+y*y+z*z<=1500.f*1500.f;
}
extern "C" void ARPG_OverridePeahatLimb(PlayState* play,Actor* actor,s32 limb,Gfx** dl) {
    if(!dl||!*dl||!ARPG_UsePeahatVisuals(play,actor))return;
    if(auto* model=ModelFor(limb,actor->params>0?1:0))*dl=model;
}

extern "C" void ARPG_OverridePeahatRoot(PlayState* play,Actor* actor,Gfx** dl) {
    if(!dl||!*dl||!actor||actor->params>0||!ARPG_UsePeahatVisuals(play,actor))return;
    // [TIME] | [INFO] | PeahatVisuals.Root | The exposed root uses the original weak-point transform and collider, with distinct authored anatomy.
    if(auto* model=ModelFor(ARPG::PeahatGeometry::WeakPoint,0))*dl=model;
}
