#pragma once
#include "GearGeometry.h"
#include "soh/frame_interpolation.h"
extern "C" {
#include "functions.h"
#include "macros.h"
#include "objects/gameplay_keep/gameplay_keep.h"
}

namespace ARPG::NativeEffects {
using GearGeometry::Point;
using GearGeometry::Add;
using GearGeometry::Mul;
using GearGeometry::pi;
struct Color { uint8_t r,g,b,a; };
enum class Texture { Trail, Dust, Fire, Glow, Lightning, Count };
inline Color Fade(Color c,float fade) { c.a=uint8_t(c.a*std::clamp(fade,0.f,1.f));return c; }
constexpr Color steel{205,226,240,170}, gold{255,223,146,170}, fire{255,146,55,190},
                ice{161,214,241,160}, green{167,213,145,130}, dust{176,154,118,130};

inline Gfx* Bind(Gfx* p,Texture texture,int frame=0) {
    p=Gfx_SetupDL(p,25);gDPPipeSync(p++);
    gSPClearGeometryMode(p++,G_LIGHTING|G_CULL_BOTH|G_TEXTURE_GEN|G_TEXTURE_GEN_LINEAR|G_FOG);
    gSPSetGeometryMode(p++,G_ZBUFFER|G_SHADE|G_SHADING_SMOOTH);
    gDPSetCycleType(p++,G_CYC_1CYCLE);
    gDPSetRenderMode(p++,G_RM_AA_ZB_XLU_SURF,G_RM_AA_ZB_XLU_SURF2);
    gDPSetTextureLUT(p++,G_TT_NONE);gSPTexture(p++,0xFFFF,0xFFFF,0,G_TX_RENDERTILE,G_ON);
    switch(texture) {
        case Texture::Trail:
            gDPLoadTextureBlock(p++,gUnknownEffBlureTex,G_IM_FMT_I,G_IM_SIZ_8b,64,32,0,G_TX_CLAMP,G_TX_CLAMP,6,5,0,0);break;
        case Texture::Dust: {
            const char* frames[]={gDust1Tex,gDust2Tex,gDust3Tex,gDust4Tex,gDust5Tex,gDust6Tex,gDust7Tex,gDust8Tex};
            gDPLoadTextureBlock(p++,frames[(frame/2)%8],G_IM_FMT_I,G_IM_SIZ_8b,32,32,0,G_TX_CLAMP,G_TX_CLAMP,5,5,0,0);break;
        }
        case Texture::Fire: {
            const char* frames[]={gEffEnemyDeathFlame1Tex,gEffEnemyDeathFlame2Tex,gEffEnemyDeathFlame3Tex,gEffEnemyDeathFlame4Tex,gEffEnemyDeathFlame5Tex,gEffEnemyDeathFlame6Tex,gEffEnemyDeathFlame7Tex,gEffEnemyDeathFlame8Tex};
            gDPLoadTextureBlock(p++,frames[(frame/2)%8],G_IM_FMT_I,G_IM_SIZ_8b,32,64,0,G_TX_CLAMP,G_TX_CLAMP,5,6,0,0);break;
        }
        case Texture::Lightning: {
            const char* frames[]={gEffLightning1Tex,gEffLightning2Tex,gEffLightning3Tex,gEffLightning4Tex,gEffLightning5Tex,gEffLightning6Tex,gEffLightning7Tex,gEffLightning8Tex};
            gDPLoadTextureBlock(p++,frames[frame%8],G_IM_FMT_I,G_IM_SIZ_8b,16,96,0,G_TX_CLAMP,G_TX_CLAMP,4,0,0,0);break;
        }
        default:
            gDPLoadTextureBlock(p++,gFlashTex,G_IM_FMT_I,G_IM_SIZ_8b,64,64,0,G_TX_CLAMP,G_TX_CLAMP,6,6,0,0);break;
    }
    gDPSetCombineLERP(p++,TEXEL0,0,SHADE,0,TEXEL0,0,SHADE,0,TEXEL0,0,SHADE,0,TEXEL0,0,SHADE,0);
    return p;
}

// Existing warning meshes retain their exact outlines. The original sword-trail
// falloff feathers their edges instead of presenting solid colored polygons.
inline void Feather(Vtx* vertices,int count,float opacity=.72f) {
    constexpr int uv[6][2]={{0,0},{2048,0},{2048,1024},{0,0},{2048,1024},{0,1024}};
    for(int i=0;i<count;++i) {
        vertices[i].v.tc[0]=uv[i%6][0];vertices[i].v.tc[1]=uv[i%6][1];
        vertices[i].v.cn[3]=uint8_t(vertices[i].v.cn[3]*opacity);
    }
}

class Mesh {
    std::array<std::vector<Vtx>,int(Texture::Count)> meshes;
    int count=0;
    Point right,up;
public:
    explicit Mesh(PlayState* play,int16_t yaw=0) {
        float sn=Math_SinS(yaw),cs=Math_CosS(yaw);auto& m=play->billboardMtxF;
        auto local=[&](Point p)->Point{return {cs*p.x-sn*p.z,p.y,sn*p.x+cs*p.z};};
        right=local({m.xx,m.yx,m.zx});up=local({m.xy,m.yy,m.zy});
    }
    void Quad(Texture texture,Point a,Point b,Point c,Point d,Color color,float alphaA=1,float alphaB=1,float u0=0,float u1=1) {
        if(count+6>1536)return;
        Point corners[]={a,b,c,d};int order[]={0,1,2,0,2,3};
        int width=texture==Texture::Lightning?16:texture==Texture::Dust || texture==Texture::Fire?32:64;
        int height=texture==Texture::Lightning?96:texture==Texture::Glow || texture==Texture::Fire?64:32;
        int uv[4][2]={{int(u0*width*32),0},{int(u1*width*32),0},{int(u1*width*32),height*32},{int(u0*width*32),height*32}};
        for(int i:order) {
            Vtx v{};auto p=corners[i];
            v.v.ob[0]=int16_t(std::clamp(p.x*4,-32760.f,32760.f));v.v.ob[1]=int16_t(std::clamp(p.y*4,-32760.f,32760.f));v.v.ob[2]=int16_t(std::clamp(p.z*4,-32760.f,32760.f));
            v.v.tc[0]=uv[i][0];v.v.tc[1]=uv[i][1];
            v.v.cn[0]=color.r;v.v.cn[1]=color.g;v.v.cn[2]=color.b;
            v.v.cn[3]=uint8_t(color.a*(i==0||i==3?alphaA:alphaB));
            meshes[int(texture)].push_back(v);
        }
        count+=6;
    }
    void Sprite(Texture texture,Point p,float width,float height,Color color) {
        auto x=Mul(right,width),y=Mul(up,height);
        Quad(texture,Add(p,Add(Mul(x,-1),y)),Add(p,Add(x,y)),Add(p,Add(x,Mul(y,-1))),Add(p,Add(Mul(x,-1),Mul(y,-1))),color);
    }
    void Line(Point a,Point b,float width,Color color,Texture texture=Texture::Trail) {
        auto axis=GearGeometry::Unit(Add(b,Mul(a,-1)));
        auto view=GearGeometry::Cross(right,up);
        auto side=Mul(GearGeometry::Unit(GearGeometry::Cross(axis,view)),width);
        if(texture==Texture::Lightning)Quad(texture,Add(a,side),Add(a,Mul(side,-1)),Add(b,Mul(side,-1)),Add(b,side),color);
        else Quad(texture,Add(a,side),Add(b,side),Add(b,Mul(side,-1)),Add(a,Mul(side,-1)),color);
    }
    void Arc(float radius,float width,float y,float start,float sweep,Color color,float z=0,float tilt=0,float x=0) {
        for(int i=0;i<24;++i) {
            float a=start+sweep*i/24,b=start+sweep*(i+1)/24;
            auto at=[&](float angle,float r)->Point{return {x+std::sin(angle)*r,y+std::sin(angle)*tilt,std::cos(angle)*r+z};};
            float alpha=std::sin((i+.5f)*pi/24);
            Quad(Texture::Trail,at(a,radius-width),at(b,radius-width),at(b,radius),at(a,radius),Fade(color,alpha),1,1,i/24.f,(i+1)/24.f);
        }
    }
    void Draw(PlayState* play,Vec3f origin,int16_t yaw,const void* identity,int part=0,float scale=1.f) {
        if(!count)return;GraphicsContext* __gfxCtx=play->state.gfxCtx;
        FrameInterpolation_RecordOpenChild(identity,part);Matrix_Push();
        Matrix_Translate(origin.x,origin.y,origin.z,MTXMODE_NEW);Matrix_RotateY(yaw*pi/32768,MTXMODE_APPLY);Matrix_Scale(.25f*scale,.25f*scale,.25f*scale,MTXMODE_APPLY);
        gSPMatrix(POLY_XLU_DISP++,MATRIX_NEWMTX(__gfxCtx),G_MTX_NOPUSH|G_MTX_LOAD|G_MTX_MODELVIEW);
        for(int kind=0;kind<int(Texture::Count);++kind) {
            const auto& mesh=meshes[kind];if(mesh.empty())continue;
            auto* vertices=static_cast<Vtx*>(Graph_Alloc(__gfxCtx,mesh.size()*sizeof(Vtx)));if(!vertices)continue;
            std::copy(mesh.begin(),mesh.end(),vertices);
            POLY_XLU_DISP=Bind(POLY_XLU_DISP,Texture(kind),play->gameplayFrames);
            for(size_t i=0;i<mesh.size();i+=24) {
                int n=std::min<size_t>(24,mesh.size()-i);gSPVertex(POLY_XLU_DISP++,reinterpret_cast<uintptr_t>(vertices+i),n,0);
                for(int j=0;j<n;j+=3)gSP1Triangle(POLY_XLU_DISP++,j,j+1,j+2,0);
            }
        }
        Gfx_SetupDL_25Xlu(__gfxCtx);Matrix_Pop();FrameInterpolation_RecordCloseChild();
    }
};
}
