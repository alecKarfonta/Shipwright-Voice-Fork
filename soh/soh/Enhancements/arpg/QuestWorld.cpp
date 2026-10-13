#include "QuestWorld.h"
#include "GearGeometry.h"
#include "QuestPlantGeometry.h"
#include "NativeMaterials.h"
#include "NativeEffects.h"
#include "assets/objects/gameplay_field_keep/gameplay_field_keep.h"
#include "assets/objects/object_wood02/object_wood02.h"
#include "assets/objects/object_syokudai/object_syokudai.h"
#include "assets/objects/object_haka/object_haka.h"
#include <array>
#include <memory>
extern "C" {
#include "functions.h"
#include "macros.h"
}
namespace ARPG::Quests {
namespace {
using namespace GearGeometry;
struct Model {std::vector<Vtx> vertices;std::vector<Gfx> commands;};
std::array<std::unique_ptr<Model>,18> models;
std::array<std::unique_ptr<Model>,12> plantModels;

std::unique_ptr<Model> PlantPart(const Plants::Mesh& mesh, bool leaves) {
    auto model=std::make_unique<Model>();
    model->vertices.resize(mesh.size());model->commands.resize(mesh.size()*2+80);
    for(size_t i=0;i<mesh.size();++i) {
        const auto& from=mesh[i];auto& to=model->vertices[i].v;to={};
        // Sub-unit twigs keep their thickness instead of rounding away in N64 vertices.
        to.ob[0]=std::lround(from.p.x*16);to.ob[1]=std::lround(from.p.y*16);to.ob[2]=std::lround(from.p.z*16);
        to.tc[0]=std::lround(from.u*1024);to.tc[1]=std::lround(from.v*1024);
        to.cn[0]=static_cast<int8_t>(std::lround(from.normal.x*127));
        to.cn[1]=static_cast<int8_t>(std::lround(from.normal.y*127));
        to.cn[2]=static_cast<int8_t>(std::lround(from.normal.z*127));to.cn[3]=255;
    }
    auto* p=model->commands.data();p=Gfx_SetupDL(p,25);gDPPipeSync(p++);
    gDPSetTextureLUT(p++,G_TT_NONE);gSPTexture(p++,0xFFFF,0xFFFF,0,G_TX_RENDERTILE,G_ON);
    if(leaves) {
        gDPLoadTextureBlock(p++,gHanaLeafTex,G_IM_FMT_RGBA,G_IM_SIZ_16b,32,32,0,
                           G_TX_CLAMP,G_TX_CLAMP,5,5,G_TX_NOLOD,G_TX_NOLOD);
        gDPSetRenderMode(p++,G_RM_FOG_SHADE_A,G_RM_AA_ZB_TEX_EDGE2);
    } else {
        gDPLoadTextureBlock(p++,object_wood02_Tex_002F90,G_IM_FMT_RGBA,G_IM_SIZ_16b,32,32,0,
                           G_TX_WRAP,G_TX_WRAP,5,5,G_TX_NOLOD,G_TX_NOLOD);
        gDPSetRenderMode(p++,G_RM_FOG_SHADE_A,G_RM_AA_ZB_OPA_SURF2);
    }
    gDPSetCombineLERP(p++,TEXEL0,0,SHADE,0,0,0,0,TEXEL0,COMBINED,0,PRIMITIVE,0,0,0,0,COMBINED);
    gDPSetPrimColor(p++,0,0,255,255,255,255);
    gSPClearGeometryMode(p++,G_CULL_BOTH|G_TEXTURE_GEN|G_TEXTURE_GEN_LINEAR);
    gSPSetGeometryMode(p++,G_LIGHTING|G_FOG|G_ZBUFFER|G_SHADE|G_SHADING_SMOOTH);
    for(size_t i=0;i<mesh.size();i+=24) {
        int count=std::min<size_t>(24,mesh.size()-i);
        gSPVertex(p++,reinterpret_cast<uintptr_t>(model->vertices.data()+i),count,0);
        for(int n=0;n<count;n+=3)gSP1Triangle(p++,n,n+1,n+2,0);
    }
    gSPEndDisplayList(p++);model->commands.resize(p-model->commands.data());return model;
}

void DrawPlant(PlayState* play,Prop kind,bool restored,float x,float y,float z) {
    GraphicsContext* __gfxCtx=play->state.gfxCtx;
    Vec3f position{x,y,z};auto* lights=LightContext_NewLights(&play->lightCtx,__gfxCtx);
    Lights_BindAll(lights,play->lightCtx.listHead,&position);Lights_Draw(lights,__gfxCtx);
    uint32_t seed=Plants::SiteSeed(x,z);int variant=seed%3;
    Matrix_RotateY((seed%360)*pi/180,MTXMODE_APPLY);
    auto native=[&](const char* displayList,float px,float pz,float scale,float yaw) {
        Matrix_Push();Matrix_Translate(px,0,pz,MTXMODE_APPLY);Matrix_RotateY(yaw,MTXMODE_APPLY);
        Matrix_Scale(scale,scale,scale,MTXMODE_APPLY);
        Gfx_DrawDListOpa(play,reinterpret_cast<Gfx*>(const_cast<char*>(displayList)));Matrix_Pop();
    };
    if(kind==Prop::Herbs) {
        // The original wildflowers have painted veins, irregular leaves and small warm blooms.
        // Offset rosettes keep a gatherable patch distinct from individual scenery flowers.
        native(gHanaDL,-11,-4,.018f,0);
        native(gHanaDL,9,-7,.022f,2.2f);
        native(gHanaDL,1,11,.016f,4.1f+variant);
    } else {
        native(gFieldBushDL,0,0,restored?.10f:.07f,.6f);
        int key=(int(restored)*3+variant)*2;
        if(!plantModels[key]) {
            auto plant=Plants::Build(restored,variant);
            plantModels[key]=PlantPart(plant.bark,false);plantModels[key+1]=PlantPart(plant.leaves,true);
        }
        Matrix_Push();Matrix_Scale(1/16.f,1/16.f,1/16.f,MTXMODE_APPLY);
        gSPMatrix(POLY_OPA_DISP++,MATRIX_NEWMTX(__gfxCtx),G_MTX_NOPUSH|G_MTX_LOAD|G_MTX_MODELVIEW);
        gSPDisplayList(POLY_OPA_DISP++,plantModels[key]->commands.data());
        gSPDisplayList(POLY_OPA_DISP++,plantModels[key+1]->commands.data());Matrix_Pop();
    }
    Gfx_SetupDL_25Opa(__gfxCtx);
}

Gfx* ModelFor(Prop prop,bool restored) {
    int key=int(prop)*2+restored;if(models[key])return models[key]->commands.data();
    Mesh m;
    switch(prop) {
        case Prop::Herbs:
        case Prop::Sapling:
            break; // Textured plants use DrawPlant, below.
        case Prop::Timber:
            for(int n=0;n<4;++n)Tube(m,{-22,5.f+(n/2)*8,(n%2)*12.f-6},{22,5.f+(n/2)*8,(n%2)*12.f-6},5,oak,7);
            Box(m,{-9,0,-14},{-5,20,14},bronze);Box(m,{6,0,-14},{10,20,14},bronze);break;
        case Prop::Ore:
        case Prop::Parcel:
        case Prop::Memorial:
            break; // Native painted models are placed by DrawNativeProp.
        case Prop::Supplies:
            if(restored) {
                Tube(m,{30,0,0},{30,70,0},1.8f,oak,8);
                // A folded cloth pennant retains the supply-cache landmark.
                Quad(m,{30,66,0},{43,64,3},{43,45,3},{30,44,0},Paint(darkJade,Surface::Cloth));
                Quad(m,{43,64,3},{57,61,0},{51,45,0},{43,45,3},Paint(darkJade,Surface::Cloth));
            } else {
                Box(m,{-22,0,-18},{22,8,18},oak);
                Box(m,{-18,9,-20},{20,12,22},leather);Tube(m,{-28,3,-18},{30,15,18},3,oak,6);
            }
            break;
        case Prop::Beacon:
            if(!restored) {
                Tube(m,{0,0,0},{0,23,0},5,oak,8);
                Tube(m,{-25,4,-5},{26,20,5},4,oak,8);
            }
            break;
        case Prop::Spring:
            Box(m,{-28,0,-28},{28,9,28},Paint(steel,Surface::Stone));Box(m,{-22,9,-22},{22,10,22},Paint(restored?cyan:leather,Surface::Flat));
            Box(m,{-9,10,14},{9,55,27},silver);Tube(m,{0,45,14},{0,45,0},4,bronze,8);
            if(restored)Tube(m,{0,45,0},{0,11,0},2,Paint(cyan,Surface::Flat),6);
            break;
    }
    if(m.empty())return nullptr;
    struct Triangle { GearGeometry::Vertex v[3]; };
    std::vector<Triangle> triangles;
    auto material=[](const auto& v){return GearGeometry::MaterialKey(v.color);};
    for(size_t i=0;i<m.size();i+=3)triangles.push_back({m[i],m[i+1],m[i+2]});
    std::stable_sort(triangles.begin(),triangles.end(),[&](const auto& a,const auto& b){return material(a.v[0])<material(b.v[0]);});
    m.clear();for(const auto& t:triangles)m.insert(m.end(),std::begin(t.v),std::end(t.v));
    auto model=std::make_unique<Model>();model->vertices.resize(m.size());model->commands.resize(m.size()*4+256);
    using Surface=NativeMaterials::Surface;
    Surface base=prop==Prop::Timber || prop==Prop::Parcel || prop==Prop::Supplies?Surface::Wood:
                 prop==Prop::Ore || prop==Prop::Beacon?Surface::Metal:Surface::Stone;
    for(size_t n=0;n<m.size();++n) {
        auto& v=model->vertices[n].v;v={};v.ob[0]=std::lround(m[n].p.x);v.ob[1]=std::lround(m[n].p.y);v.ob[2]=std::lround(m[n].p.z);
        v.cn[0]=int8_t(std::lround(m[n].normal.x*127));v.cn[1]=int8_t(std::lround(m[n].normal.y*127));v.cn[2]=int8_t(std::lround(m[n].normal.z*127));v.cn[3]=255;
        NativeMaterials::UV(model->vertices[n],m[n].p,m[n].normal,48,NativeMaterials::ResolveSurface(m[n].color,base));
    }
    auto* p=model->commands.data();p=Gfx_SetupDL(p,25);
    for(size_t first=0;first<m.size();) {
        size_t end=first+3;while(end<m.size() && material(m[end])==material(m[first]))end+=3;
        p=NativeMaterials::Bind(p,NativeMaterials::ResolveSurface(m[first].color,base),m[first].color);
        for(size_t n=first;n<end;n+=24) {
            int count=std::min<size_t>(24,end-n);gSPVertex(p++,reinterpret_cast<uintptr_t>(model->vertices.data()+n),count,0);
            for(int i=0;i<count;i+=3)gSP1Triangle(p++,i,i+1,i+2,0);
        }
        first=end;
    }
    p=Gfx_SetupDL(p,25);gSPEndDisplayList(p++);model->commands.resize(p-model->commands.data());model->commands.shrink_to_fit();
    auto* result=model->commands.data();models[key]=std::move(model);return result;
}
// Reuse original painted scene props where their silhouette already communicates
// the quest action. Draw-only references do not add actors or gameplay collision.
void DrawNativeProp(PlayState* play,Prop kind,bool restored) {
    auto native=[&](const char* dl,float x,float y,float z,float scale,float yaw=0.f,float tilt=0.f) {
        Matrix_Push();Matrix_Translate(x,y,z,MTXMODE_APPLY);Matrix_RotateY(yaw,MTXMODE_APPLY);
        Matrix_RotateZ(tilt,MTXMODE_APPLY);Matrix_Scale(scale,scale,scale,MTXMODE_APPLY);
        Gfx_DrawDListOpa(play,reinterpret_cast<Gfx*>(const_cast<char*>(dl)));Matrix_Pop();
    };
    if(kind==Prop::Ore) {
        native(gFieldKakeraDL,-12,8,-4,.16f,.3f);
        native(gFieldKakeraDL,10,6,8,.12f,2.f);
        native(gFieldKakeraDL,4,9,-13,.13f,4.4f);
    } else if(kind==Prop::Parcel) {
        native(gSmallWoodenBoxDL,0,0,0,.15f,.15f);
    } else if(kind==Prop::Supplies && restored) {
        native(gSmallWoodenBoxDL,-12,0,0,.14f,.08f);
        native(gSmallWoodenBoxDL,13,0,5,.12f,-.1f);
        native(gSmallWoodenBoxDL,-7,28,-1,.085f,.3f);
    } else if(kind==Prop::Beacon && restored) {
        native(gWoodenTorchDL,0,0,0,1.3f);
    } else if(kind==Prop::Memorial) {
        native(gGravestoneStoneDL,0,restored?0:10,0,.07f,0,restored?0:.36f);
        if(restored)native(gHanaDL,-19,0,18,.011f,.7f);
    }
}

}
void DrawProp(PlayState* play,Prop kind,float x,float y,float z,bool complete,bool selected,float progress) {
    if(!play)return;GraphicsContext* __gfxCtx=play->state.gfxCtx;
    Matrix_Push();Matrix_Translate(x,y,z,MTXMODE_NEW);
    bool plant=kind==Prop::Herbs || kind==Prop::Sapling;
    if(plant)DrawPlant(play,kind,complete,x,y,z);
    else {
        Vec3f position{x,y,z};auto* lights=LightContext_NewLights(&play->lightCtx,__gfxCtx);
        Lights_BindAll(lights,play->lightCtx.listHead,&position);Lights_Draw(lights,__gfxCtx);
        DrawNativeProp(play,kind,complete);
        gSPMatrix(POLY_OPA_DISP++,MATRIX_NEWMTX(__gfxCtx),G_MTX_NOPUSH|G_MTX_LOAD|G_MTX_MODELVIEW);
        if(auto* model=ModelFor(kind,complete))gSPDisplayList(POLY_OPA_DISP++,model);
    }
    Matrix_Pop();
    if(kind==Prop::Beacon && complete) {
        NativeEffects::Mesh flame(play);
        flame.Sprite(NativeEffects::Texture::Fire,{0,82,0},9,20,NativeEffects::fire);
        flame.Draw(play,{x,y,z},0,&models[int(kind)*2+complete],int(Plants::SiteSeed(x,z)&0x7fffffff));
    }
    if(!complete && (selected || progress>0)) {
        // An in-world segmented ring is also the controller-friendly interaction prompt.
        auto* vertices=static_cast<Vtx*>(Graph_Alloc(__gfxCtx,24*6*sizeof(Vtx)));if(!vertices)return;
        int filled=int(progress*24);
        for(int n=0;n<24;++n) {
            float a=n*2*pi/24,b=(n+.75f)*2*pi/24;
            Point corners[]={{std::sin(a)*51,1,std::cos(a)*51},{std::sin(b)*51,1,std::cos(b)*51},
                             {std::sin(b)*56,1,std::cos(b)*56},{std::sin(a)*56,1,std::cos(a)*56}};
            int order[]={0,1,2,0,2,3};Color color=n<filled?gold:selected?cream:jade;
            for(int k=0;k<6;++k) {auto& v=vertices[n*6+k].v;v={};auto p=corners[order[k]];
                v.ob[0]=std::lround(p.x);v.ob[1]=1;v.ob[2]=std::lround(p.z);v.cn[0]=color.r;v.cn[1]=color.g;v.cn[2]=color.b;v.cn[3]=255;}
        }
        Matrix_Push();Matrix_Translate(x,y+1,z,MTXMODE_NEW);Gfx_SetupDL_25Xlu(__gfxCtx);
        gSPMatrix(POLY_XLU_DISP++,MATRIX_NEWMTX(__gfxCtx),G_MTX_NOPUSH|G_MTX_LOAD|G_MTX_MODELVIEW);
        NativeEffects::Feather(vertices,24*6,.7f);
        POLY_XLU_DISP=NativeEffects::Bind(POLY_XLU_DISP,NativeEffects::Texture::Trail);
        for(int n=0;n<24;n+=4) {gSPVertex(POLY_XLU_DISP++,reinterpret_cast<uintptr_t>(vertices+n*6),24,0);
            for(int k=0;k<24;k+=3)gSP1Triangle(POLY_XLU_DISP++,k,k+1,k+2,0);}
        Gfx_SetupDL_25Xlu(__gfxCtx);Matrix_Pop();
    }
}
}
