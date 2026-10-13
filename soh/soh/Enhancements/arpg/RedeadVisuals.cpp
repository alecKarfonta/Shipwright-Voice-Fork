#include "RedeadVisuals.h"
#include "RedeadGeometry.h"
#include "ARPG.h"
#include "soh/cvar_prefixes.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include <map>
#include <memory>
extern "C" {
#include "functions.h"
#include "objects/object_rd/object_rd.h"
#include "overlays/actors/ovl_En_Rd/z_en_rd.h"
}
namespace {
struct Model { std::vector<Vtx> vertices;std::vector<Gfx> commands; };
std::map<int,std::unique_ptr<Model>> models;
Gfx* ModelFor(int limb,int variant,bool fading) {
    using namespace ARPG;const int key=limb+variant*32+int(fading)*64;
    if(auto found=models.find(key);found!=models.end())return found->second->commands.data();
    size_t count=0;for(const auto& part:RedeadBlenderData::parts)if(part.limb==limb && part.variant==variant)count+=part.count;
    if(!count)return nullptr;
    auto model=std::make_unique<Model>();model->vertices.resize(count);model->commands.resize(count*4+2048);
    auto* p=model->commands.data();size_t offset=0;int loaded=limb;
    auto load=[&](int joint){if(joint==loaded)return;
        // [TIME] | [INFO] | RedeadVisuals.Connection | Vertices at either end of a joint bridge use the original flex matrix palette.
        auto matrix=uintptr_t(0x0D000001)+sizeof(Mtx)*RedeadGeometry::MatrixIndex(joint);
        gSPMatrix(p++,matrix,G_MTX_MODELVIEW|G_MTX_LOAD|G_MTX_NOPUSH);loaded=joint;};
    for(const auto& part:RedeadBlenderData::parts) {
        if(part.limb!=limb || part.variant!=variant)continue;
        const auto ink=RedeadGeometry::Palette[variant][part.material];
        const bool cloth=variant && (part.material==0 || part.material==4 || part.material==8);
        const bool texture=cloth || (!variant && part.material<=1);
        const int width=cloth?64:16,height=32;
        for(uint32_t i=0;i<part.count;++i) {
            const auto& source=RedeadBlenderData::vertices[RedeadBlenderData::indices[part.first+i]];auto& v=model->vertices[offset+i].v;
            v.ob[0]=source.x;v.ob[1]=source.y;v.ob[2]=source.z;v.cn[0]=uint8_t(source.nx);v.cn[1]=uint8_t(source.ny);v.cn[2]=uint8_t(source.nz);v.cn[3]=255;
            float x=std::abs(int(source.nx)),y=std::abs(int(source.ny)),z=std::abs(int(source.nz));
            v.tc[0]=int16_t((x>y && x>z?source.z:source.x)*width*32/900);
            v.tc[1]=int16_t((y>x && y>z?source.z:-source.y)*height*32/900);
        }
        gDPPipeSync(p++);gSPClearGeometryMode(p++,G_CULL_BOTH|G_TEXTURE_GEN|G_TEXTURE_GEN_LINEAR);
        gSPSetGeometryMode(p++,G_LIGHTING|G_FOG|G_ZBUFFER|G_SHADE|G_SHADING_SMOOTH);
        gDPSetCycleType(p++,G_CYC_2CYCLE);gDPSetRenderMode(p++,G_RM_FOG_SHADE_A,fading?G_RM_AA_ZB_XLU_SURF2:G_RM_AA_ZB_OPA_SURF2);gDPSetTextureLUT(p++,G_TT_NONE);
        gDPSetPrimColor(p++,0,48,ink.r,ink.g,ink.b,255);
        // [TIME] | [INFO] | RedeadVisuals.Fade | Native ENV alpha drives corpse fading; native lights and fog carry damage and stun colors.
        if(texture){gSPTexture(p++,0xFFFF,0xFFFF,0,G_TX_RENDERTILE,G_ON);
            gDPLoadTextureBlock(p++,cloth?gGibdoWrappingTex:gRedeadMuscleTex,G_IM_FMT_RGBA,G_IM_SIZ_16b,width,height,0,G_TX_WRAP,G_TX_WRAP,cloth?6:4,5,0,0);
            gDPSetCombineLERP(p++,TEXEL0,PRIMITIVE,PRIM_LOD_FRAC,PRIMITIVE,0,0,0,ENVIRONMENT,COMBINED,0,SHADE,0,0,0,0,COMBINED);
        }else{gSPTexture(p++,0,0,0,G_TX_RENDERTILE,G_OFF);
            gDPSetCombineLERP(p++,PRIMITIVE,0,SHADE,0,0,0,0,ENVIRONMENT,0,0,0,COMBINED,0,0,0,COMBINED);}
        for(size_t batch=0;batch<part.count;batch+=24){int n=int(std::min<size_t>(24,part.count-batch));
            for(int first=0;first<n;){int joint=RedeadBlenderData::vertices[RedeadBlenderData::indices[part.first+batch+first]].joint,last=first+1;
                while(last<n && RedeadBlenderData::vertices[RedeadBlenderData::indices[part.first+batch+last]].joint==joint)++last;
                load(joint);gSPVertex(p++,reinterpret_cast<uintptr_t>(model->vertices.data()+offset+batch+first),last-first,first);first=last;}
            for(int j=0;j<n;j+=3)gSP1Triangle(p++,j,j+1,j+2,0);
        }
        offset+=part.count;
    }
    load(limb);p=Gfx_SetupDL(p,25);gSPEndDisplayList(p++);model->commands.resize(p-model->commands.data());
    auto* result=model->commands.data();models.emplace(key,std::move(model));return result;
}
}
extern "C" void ARPG_OverrideRedeadLimb(PlayState* play,Actor* actor,s32 limb,Gfx** dl) {
    if(!play || !actor || actor->id!=ACTOR_EN_RD || !dl || !*dl || !ARPG::Enabled() || !CVarGetInteger(CVAR_ENHANCEMENT("ARPG.RemasteredEnemies"),1))return;
    const auto& eye=GET_ACTIVE_CAM(play)->eye;float x=actor->world.pos.x-eye.x,y=actor->world.pos.y-eye.y,z=actor->world.pos.z-eye.z;
    if(x*x+y*y+z*z>1500.f*1500.f)return;
    if(auto* model=ModelFor(limb,actor->params< -1,reinterpret_cast<EnRd*>(actor)->alpha<255))*dl=model;
}
