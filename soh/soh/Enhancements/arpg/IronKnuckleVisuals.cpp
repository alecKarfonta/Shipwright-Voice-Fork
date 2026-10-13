#include "IronKnuckleVisuals.h"
#include "IronKnuckleGeometry.h"
#include "ARPG.h"
#include "soh/cvar_prefixes.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include <map>
#include <memory>
#include <cstring>
#include "soh/ResourceManagerHelpers.h"
extern "C" {
#include "functions.h"
#include "objects/object_ik/object_ik.h"
#include "overlays/actors/ovl_En_Ik/z_en_ik.h"
}
namespace {
struct Model { std::vector<Vtx> vertices;std::vector<Gfx> commands; };
std::map<int,std::unique_ptr<Model>> models;
Gfx* ModelFor(int limb,int variant) {
    using namespace ARPG;const int key=limb+variant*32;
    if(auto found=models.find(key);found!=models.end())return found->second->commands.data();
    size_t count=0;for(const auto& part:IronKnuckleBlenderData::parts)if(part.limb==limb)count+=part.count;
    if(!count)return nullptr;
    auto model=std::make_unique<Model>();model->vertices.resize(count);model->commands.resize(count*4+2048);
    auto* p=model->commands.data();size_t offset=0;int loaded=limb;
    auto load=[&](int joint){if(joint==loaded)return;
        // [TIME] | [INFO] | IronKnuckleVisuals.Connection | Vertices at either end of a joint bridge use the original flex matrix palette.
        auto matrix=uintptr_t(0x0D000001)+sizeof(Mtx)*IronKnuckleGeometry::MatrixIndex(joint);
        gSPMatrix(p++,matrix,G_MTX_MODELVIEW|G_MTX_LOAD|G_MTX_NOPUSH);loaded=joint;};
    for(const auto& part:IronKnuckleBlenderData::parts) {
        if(part.limb!=limb)continue;
        const auto ink=IronKnuckleGeometry::Palette[variant][part.material];
        const bool metal=part.material<=2,mail=part.material==3;
        const int width=metal?32:16,height=metal?64:16;
        for(uint32_t i=0;i<part.count;++i) {
            const auto& source=IronKnuckleBlenderData::vertices[IronKnuckleBlenderData::indices[part.first+i]];auto& v=model->vertices[offset+i].v;
            v.ob[0]=source.x;v.ob[1]=source.y;v.ob[2]=source.z;v.cn[0]=uint8_t(source.nx);v.cn[1]=uint8_t(source.ny);v.cn[2]=uint8_t(source.nz);v.cn[3]=255;
            float x=std::abs(int(source.nx)),y=std::abs(int(source.ny)),z=std::abs(int(source.nz));
            v.tc[0]=int16_t((x>y && x>z?source.z:source.x)*width*32/900);
            v.tc[1]=int16_t((y>x && y>z?source.z:-source.y)*height*32/900);
        }
        gDPPipeSync(p++);gSPClearGeometryMode(p++,G_CULL_BOTH|G_TEXTURE_GEN|G_TEXTURE_GEN_LINEAR);
        gSPSetGeometryMode(p++,G_LIGHTING|G_FOG|G_ZBUFFER|G_SHADE|G_SHADING_SMOOTH);
        gDPSetCycleType(p++,G_CYC_2CYCLE);gDPSetRenderMode(p++,G_RM_FOG_SHADE_A,G_RM_AA_ZB_OPA_SURF2);gDPSetTextureLUT(p++,G_TT_NONE);
        gDPSetPrimColor(p++,0,metal?70:110,ink.r,ink.g,ink.b,255);
        // [TIME] | [INFO] | IronKnuckleVisuals.Material | Original metal reflection and chain-mail detail keep armor tied to native scene lighting and damage flashes.
        if(metal||mail){
            if(metal){gSPSetGeometryMode(p++,G_TEXTURE_GEN);gSPTexture(p++,0x07C0,0x0FC0,0,G_TX_RENDERTILE,G_ON);
                gDPLoadTextureBlock_4b(p++,gIronKnuckleMetalTex,G_IM_FMT_I,32,64,0,G_TX_WRAP,G_TX_WRAP,5,6,0,0);
            }else{gSPTexture(p++,0xFFFF,0xFFFF,0,G_TX_RENDERTILE,G_ON);
                gDPLoadTextureBlock(p++,gIronKnuckleChainMailTex,G_IM_FMT_RGBA,G_IM_SIZ_16b,16,16,0,G_TX_WRAP,G_TX_WRAP,4,4,0,0);}
            gDPSetCombineLERP(p++,TEXEL0,PRIMITIVE,PRIM_LOD_FRAC,PRIMITIVE,0,0,0,1,COMBINED,0,SHADE,0,0,0,0,1);
        }else{gSPTexture(p++,0,0,0,G_TX_RENDERTILE,G_OFF);
            gDPSetCombineLERP(p++,PRIMITIVE,0,SHADE,0,0,0,0,1,0,0,0,COMBINED,0,0,0,1);}
        for(size_t batch=0;batch<part.count;batch+=24){int n=int(std::min<size_t>(24,part.count-batch));
            for(int first=0;first<n;){int joint=IronKnuckleBlenderData::vertices[IronKnuckleBlenderData::indices[part.first+batch+first]].joint,last=first+1;
                while(last<n && IronKnuckleBlenderData::vertices[IronKnuckleBlenderData::indices[part.first+batch+last]].joint==joint)++last;
                load(joint);gSPVertex(p++,reinterpret_cast<uintptr_t>(model->vertices.data()+offset+batch+first),last-first,first);first=last;}
            for(int j=0;j<n;j+=3)gSP1Triangle(p++,j,j+1,j+2,0);
        }
        offset+=part.count;
    }
    load(limb);p=Gfx_SetupDL(p,25);gSPEndDisplayList(p++);model->commands.resize(p-model->commands.data());
    auto* result=model->commands.data();models.emplace(key,std::move(model));return result;
}
}
extern "C" s32 ARPG_UseIronKnuckleVisuals(PlayState* play,Actor* actor) {
    if(!play || !actor || actor->id!=ACTOR_EN_IK || actor->params<1 || actor->params>3 || !ARPG::Enabled() || !CVarGetInteger(CVAR_ENHANCEMENT("ARPG.RemasteredEnemies"),1))return false;
    const auto& eye=GET_ACTIVE_CAM(play)->eye;float x=actor->world.pos.x-eye.x,y=actor->world.pos.y-eye.y,z=actor->world.pos.z-eye.z;
    return x*x+y*y+z*z<=1500.f*1500.f;
}
extern "C" void ARPG_OverrideIronKnuckleLimb(PlayState* play,Actor* actor,s32 limb,Gfx** dl) {
    if(!dl || !*dl || !ARPG_UseIronKnuckleVisuals(play,actor))return;
    if(auto* model=ModelFor(limb,actor->params-1))*dl=model;
}
extern "C" s32 ARPG_IsIronKnuckleRearArmor(Gfx* dl) {
    if(!dl)return false;
    // [TIME] | [INFO] | IronKnuckleVisuals.Debris | Detached armor retains its front/back launch direction for both resource paths and cached replacement lists.
    for(int variant=0;variant<3;++variant){auto found=models.find(27+variant*32);if(found!=models.end() && found->second->commands.data()==dl)return true;}
    return ResourceMgr_OTRSigCheck(reinterpret_cast<char*>(dl)) && !std::strcmp(reinterpret_cast<char*>(dl),object_ik_DL_015380);
}
