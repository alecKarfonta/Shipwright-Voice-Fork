#include "DodongoVisuals.h"
#include "DodongoGeometry.h"
#include "ARPG.h"
#include "soh/cvar_prefixes.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include <map>
#include <memory>
extern "C" {
#include "functions.h"
#include "objects/object_dodongo/object_dodongo.h"
}
namespace {
struct Model { std::vector<Vtx> vertices;std::vector<Gfx> commands; };
std::map<int,std::unique_ptr<Model>> models;
Gfx* ModelFor(int limb) {
    using namespace ARPG;
    if(auto found=models.find(limb);found!=models.end())return found->second->commands.data();
    size_t count=0;for(const auto& part:DodongoBlenderData::parts)if(part.limb==limb)count+=part.count;
    if(!count)return nullptr;
    auto model=std::make_unique<Model>();model->vertices.resize(count);model->commands.resize(count*2+2048);
    auto* p=model->commands.data();size_t offset=0;
    for(const auto& part:DodongoBlenderData::parts) {
        if(part.limb!=limb)continue;
        const auto ink=DodongoGeometry::Palette[part.material];
        // [TIME] | [INFO] | DodongoVisuals.Material | Explicit Blender material IDs select native skin detail; eyes, mouth and bone retain authored paint.
        const bool skin=part.material<=2;
        for(uint32_t i=0;i<part.count;++i) {
            const auto& source=DodongoBlenderData::vertices[DodongoBlenderData::indices[part.first+i]];
            auto& v=model->vertices[offset+i].v;
            v.ob[0]=source.x;v.ob[1]=source.y;v.ob[2]=source.z;
            v.cn[0]=uint8_t(source.nx);v.cn[1]=uint8_t(source.ny);v.cn[2]=uint8_t(source.nz);v.cn[3]=255;
            float x=std::abs(int(source.nx)),y=std::abs(int(source.ny)),z=std::abs(int(source.nz));
            v.tc[0]=int16_t((x>y && x>z?source.z:source.x)*8*32/600);
            v.tc[1]=int16_t((y>x && y>z?source.z:-source.y)*16*32/600);
        }
        gDPPipeSync(p++);gSPClearGeometryMode(p++,G_CULL_BOTH|G_TEXTURE_GEN|G_TEXTURE_GEN_LINEAR);
        gSPSetGeometryMode(p++,G_LIGHTING|G_FOG|G_ZBUFFER|G_SHADE|G_SHADING_SMOOTH);
        gDPSetCycleType(p++,G_CYC_2CYCLE);gDPSetRenderMode(p++,G_RM_FOG_SHADE_A,G_RM_AA_ZB_OPA_SURF2);gDPSetTextureLUT(p++,G_TT_NONE);
        gDPSetPrimColor(p++,0,48,ink.r,ink.g,ink.b,255);
        if(skin) {
            gSPTexture(p++,0xFFFF,0xFFFF,0,G_TX_RENDERTILE,G_ON);
            gDPLoadTextureBlock(p++,part.material==1?gDodongoRoughSkinTex:gDodongoSkinTex,G_IM_FMT_RGBA,G_IM_SIZ_16b,8,16,0,G_TX_WRAP,G_TX_WRAP,3,4,0,0);
            gDPSetCombineLERP(p++,TEXEL0,PRIMITIVE,PRIM_LOD_FRAC,PRIMITIVE,0,0,0,1,COMBINED,0,SHADE,0,0,0,0,1);
        } else {
            gSPTexture(p++,0,0,0,G_TX_RENDERTILE,G_OFF);
            gDPSetCombineLERP(p++,PRIMITIVE,0,SHADE,0,0,0,0,1,0,0,0,COMBINED,0,0,0,1);
        }
        for(size_t i=0;i<part.count;i+=24) {
            int n=int(std::min<size_t>(24,part.count-i));
            gSPVertex(p++,reinterpret_cast<uintptr_t>(model->vertices.data()+offset+i),n,0);
            for(int j=0;j<n;j+=3)gSP1Triangle(p++,j,j+1,j+2,0);
        }
        offset+=part.count;
    }
    p=Gfx_SetupDL(p,25);gSPEndDisplayList(p++);model->commands.resize(p-model->commands.data());
    auto* result=model->commands.data();models.emplace(limb,std::move(model));return result;
}
}
extern "C" void ARPG_OverrideDodongoLimb(PlayState* play,Actor* actor,s32 limb,Gfx** dl) {
    if(!play || !actor || actor->id!=ACTOR_EN_DODONGO || !dl || !*dl || !ARPG::Enabled() ||
       !CVarGetInteger(CVAR_ENHANCEMENT("ARPG.RemasteredEnemies"),1))return;
    const auto& eye=GET_ACTIVE_CAM(play)->eye;float x=actor->world.pos.x-eye.x,y=actor->world.pos.y-eye.y,z=actor->world.pos.z-eye.z;
    if(x*x+y*y+z*z>1500.f*1500.f)return;
    if(auto* model=ModelFor(limb))*dl=model;
}
