#pragma once
#include "GearGeometry.h"
extern "C" {
#include "functions.h"
#include "macros.h"
#include "objects/object_wood02/object_wood02.h"
#include "objects/object_link_boy/object_link_boy.h"
#include "objects/object_gi_clothes/object_gi_clothes.h"
#include "objects/gameplay_dangeon_keep/gameplay_dangeon_keep.h"
#include "objects/object_sk2/object_sk2.h"
#include "objects/object_wf/object_wf.h"
#include "objects/object_zf/object_zf.h"
#include "objects/object_firefly/object_firefly.h"
#include "objects/object_dekubaba/object_dekubaba.h"
#include "objects/object_gol/object_gol.h"
#include "objects/object_okuta/object_okuta.h"
}

namespace ARPG::NativeMaterials {
using GearGeometry::Color;
using GearGeometry::Point;
using GearGeometry::Surface;
using GearGeometry::ResolveSurface;

// Object-space coordinates are authored before mounting gear on Link. A shared
// projection per triangle avoids discontinuities between the three vertex normals.
inline void UV(Vtx& v,Point p,Point normal,float repeat,Surface surface=Surface::Stone) {
    int width=32,height=32;
    switch(surface) {
        case Surface::Metal:case Surface::Gem:case Surface::Bone:case Surface::Plant:width=height=8;break;
        case Surface::Leather:width=16;height=32;break;
        case Surface::Cloth:width=height=64;break;
        case Surface::Fur:width=8;height=16;break;
        case Surface::Wing:width=4;height=16;break;
        case Surface::Shell:case Surface::Skin:width=height=16;break;
        default:break;
    }
    float x=std::abs(normal.x),y=std::abs(normal.y),z=std::abs(normal.z);
    float u=x>y && x>z?p.z:p.x, w=y>x && y>z?p.z:-p.y;
    v.v.tc[0]=int16_t(std::clamp(std::lround(u*width*32/repeat),-32760l,32760l));
    v.v.tc[1]=int16_t(std::clamp(std::lround(w*height*32/repeat),-32760l,32760l));
}

inline Gfx* Bind(Gfx* p,Surface surface,Color color,bool fading=false,bool emissive=false) {
    gDPPipeSync(p++);
    gSPClearGeometryMode(p++,G_CULL_BOTH|G_TEXTURE_GEN|G_TEXTURE_GEN_LINEAR);
    gSPSetGeometryMode(p++,G_LIGHTING|G_FOG|G_ZBUFFER|G_SHADE|G_SHADING_SMOOTH);
    gDPSetCycleType(p++,G_CYC_2CYCLE);
    gDPSetRenderMode(p++,G_RM_FOG_SHADE_A,fading?G_RM_AA_ZB_XLU_SURF2:G_RM_AA_ZB_OPA_SURF2);
    gDPSetTextureLUT(p++,G_TT_NONE);
    gSPTexture(p++,0xFFFF,0xFFFF,0,G_TX_RENDERTILE,G_ON);
    if(surface==Surface::Flat || surface==Surface::Glow || emissive) {
        gSPTexture(p++,0,0,0,G_TX_RENDERTILE,G_OFF);
        gDPSetPrimColor(p++,0,0,color.r,color.g,color.b,255);
        if(surface==Surface::Glow || emissive) {
            if(fading) { gDPSetCombineLERP(p++,0,0,0,PRIMITIVE,0,0,0,ENVIRONMENT,0,0,0,COMBINED,0,0,0,COMBINED); }
            else { gDPSetCombineLERP(p++,0,0,0,PRIMITIVE,0,0,0,1,0,0,0,COMBINED,0,0,0,1); }
        } else if(fading) { gDPSetCombineLERP(p++,PRIMITIVE,0,SHADE,0,0,0,0,ENVIRONMENT,0,0,0,COMBINED,0,0,0,COMBINED); }
        else { gDPSetCombineLERP(p++,PRIMITIVE,0,SHADE,0,0,0,0,1,0,0,0,COMBINED,0,0,0,1); }
        return p;
    }
    switch(surface) {
        case Surface::Gem:
        case Surface::Metal:
            gDPLoadTextureBlock(p++,gLinkAdultHookshotMetalTex,G_IM_FMT_RGBA,G_IM_SIZ_16b,8,8,0,G_TX_WRAP,G_TX_WRAP,3,3,0,0);break;
        case Surface::Wood:
            gDPLoadTextureBlock(p++,object_wood02_Tex_002F90,G_IM_FMT_RGBA,G_IM_SIZ_16b,32,32,0,G_TX_WRAP,G_TX_WRAP,5,5,0,0);break;
        case Surface::Leather:
            gDPSetTextureLUT(p++,G_TT_RGBA16);gDPLoadTLUT_pal256(p++,object_link_boyTLUT_005800);
            gDPLoadTextureBlock(p++,gLinkAdultBoot2Tex,G_IM_FMT_CI,G_IM_SIZ_8b,16,32,0,G_TX_WRAP,G_TX_WRAP,4,5,0,0);break;
        case Surface::Cloth:
            gDPLoadTextureBlock_4b(p++,object_gi_clothesTex_000000,G_IM_FMT_I,64,64,0,G_TX_WRAP,G_TX_WRAP,6,6,0,0);break;
        case Surface::Stone:
            gDPLoadTextureBlock(p++,gUnusedStoneTex,G_IM_FMT_RGBA,G_IM_SIZ_16b,32,32,0,G_TX_WRAP,G_TX_WRAP,5,5,0,0);break;
        case Surface::Bone:
            gDPLoadTextureBlock(p++,gStalfosBoneDetailTex,G_IM_FMT_RGBA,G_IM_SIZ_16b,8,8,0,G_TX_WRAP,G_TX_WRAP,3,3,0,0);break;
        case Surface::Fur:
            gDPLoadTextureBlock(p++,gWolfosWhiteFurTex,G_IM_FMT_RGBA,G_IM_SIZ_16b,8,16,0,G_TX_WRAP,G_TX_WRAP,3,4,0,0);break;
        case Surface::Scales:
            gDPLoadTextureBlock(p++,object_zf_Tex_00B120,G_IM_FMT_RGBA,G_IM_SIZ_16b,32,32,0,G_TX_WRAP,G_TX_WRAP,5,5,0,0);break;
        case Surface::Wing:
            gDPLoadTextureBlock(p++,gKeeseWingTex,G_IM_FMT_RGBA,G_IM_SIZ_16b,4,16,0,G_TX_WRAP,G_TX_WRAP,2,4,0,0);break;
        case Surface::Plant:
            gDPLoadTextureBlock(p++,gDekuBabaStemTex,G_IM_FMT_RGBA,G_IM_SIZ_16b,8,8,0,G_TX_WRAP,G_TX_WRAP,3,3,0,0);break;
        case Surface::Skin:
            gDPLoadTextureBlock(p++,gOctorokRoughSkinTex,G_IM_FMT_RGBA,G_IM_SIZ_16b,16,16,0,G_TX_WRAP,G_TX_WRAP,4,4,0,0);break;
        case Surface::Shell:
            gDPLoadTextureBlock(p++,gObjectGolEggTex,G_IM_FMT_RGBA,G_IM_SIZ_16b,16,16,0,G_TX_WRAP,G_TX_WRAP,4,4,0,0);break;
        default:break;
    }
    // Blend the native painted detail with each item's authored material color,
    // then light the result. PRIM_LOD_FRAC leaves native death-fade ENV alpha intact.
    gDPSetPrimColor(p++,0,surface==Surface::Gem?20:surface==Surface::Metal?46:surface==Surface::Skin?40:86,color.r,color.g,color.b,255);
    if(fading) {
        gDPSetCombineLERP(p++,TEXEL0,PRIMITIVE,PRIM_LOD_FRAC,PRIMITIVE,0,0,0,ENVIRONMENT,COMBINED,0,SHADE,0,0,0,0,COMBINED);
    } else {
        gDPSetCombineLERP(p++,TEXEL0,PRIMITIVE,PRIM_LOD_FRAC,PRIMITIVE,0,0,0,1,COMBINED,0,SHADE,0,0,0,0,1);
    }
    return p;
}
}
