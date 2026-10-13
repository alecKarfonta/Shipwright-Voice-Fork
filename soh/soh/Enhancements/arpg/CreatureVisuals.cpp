#include "CreatureVisuals.h"
#include "NativeMaterials.h"
#include "NativeEffects.h"
#include "CreatureSystem.h"
#include "CreatureBlenderData.h"
#include "soh/frame_interpolation.h"
#include <array>
#include <cmath>
#include <memory>
#include <vector>
extern "C" {
#include "functions.h"
#include "macros.h"
}
namespace ARPG::Creatures {
namespace {
struct Mesh { std::vector<Vtx> vertices;std::vector<Gfx> commands; };
std::array<std::unique_ptr<Mesh>,16> models;
constexpr uint8_t colors[][3]={{53,64,73},{108,126,136},{165,181,181},{141,91,51},{215,178,102},{96,233,214},{217,211,175},{24,29,43},{39,81,101},{74,139,153},{168,233,231}};
using Surface=NativeMaterials::Surface;
constexpr Surface surfaces[]={Surface::Stone,Surface::Stone,Surface::Stone,Surface::Metal,Surface::Metal,Surface::Gem,Surface::Bone,Surface::Cloth,Surface::Cloth,Surface::Cloth,Surface::Gem};
Gfx* Model(int part) {
    if(models[part])return models[part]->commands.data();
    auto mesh=std::make_unique<Mesh>();
    for(auto group:CreatureBlenderData::parts)if(group.part==part)for(uint32_t i=0;i<group.count;++i) {
        auto p=CreatureBlenderData::vertices[CreatureBlenderData::indices[group.first+i]];Vtx v{};
        NativeMaterials::UV(v,{float(p.x),float(p.y),float(p.z)},{float(p.nx),float(p.ny),float(p.nz)},1000.f,surfaces[group.material]);
        v.v.ob[0]=p.x;v.v.ob[1]=p.y;v.v.ob[2]=p.z;
        v.v.cn[0]=p.nx;v.v.cn[1]=p.ny;v.v.cn[2]=p.nz;v.v.cn[3]=255;mesh->vertices.push_back(v);
    }
    mesh->commands.resize(mesh->vertices.size()*4+256);Gfx* p=mesh->commands.data();
    gDPPipeSync(p++);gSPTexture(p++,0,0,0,G_TX_RENDERTILE,G_OFF);
    gSPClearGeometryMode(p++,G_CULL_BOTH|G_TEXTURE_GEN|G_TEXTURE_GEN_LINEAR);
    gSPSetGeometryMode(p++,G_LIGHTING|G_ZBUFFER|G_SHADE|G_SHADING_SMOOTH);
    gDPSetCombineLERP(p++,SHADE,0,PRIMITIVE,0,0,0,0,1,SHADE,0,PRIMITIVE,0,0,0,0,1);
    size_t offset=0;
    for(auto group:CreatureBlenderData::parts)if(group.part==part) {
        auto* c=colors[group.material];
        p=NativeMaterials::Bind(p,surfaces[group.material],{c[0],c[1],c[2]});
        for(uint32_t j=0;j<group.count;j+=24) {
            int count=std::min<uint32_t>(24,group.count-j);gSPVertex(p++,reinterpret_cast<uintptr_t>(mesh->vertices.data()+offset+j),count,0);
            for(int k=0;k<count;k+=3)gSP1Triangle(p++,k,k+1,k+2,0);
        }
        offset+=group.count;
    }
    gSPEndDisplayList(p++);mesh->commands.resize(p-mesh->commands.data());models[part]=std::move(mesh);return models[part]->commands.data();
}
void DrawMend(PlayState* play,Actor* caster,Actor* target,float progress) {
    using namespace NativeEffects;
    NativeEffects::Mesh ribbons(play);
    Point delta{target->world.pos.x-caster->world.pos.x,target->world.pos.y-caster->world.pos.y-4,target->world.pos.z-caster->world.pos.z};
    float length=std::max(1.f,std::hypot(delta.x,delta.z));
    float sx=delta.z/length,sz=-delta.x/length,time=play->gameplayFrames*.18f;
    // Camera-facing strands remain legible from the normal low gameplay camera.
    // Their braided arch still distinguishes a healing channel from attack bolts.
    for(int strand=0;strand<2;++strand)for(int i=0;i<20;++i) {
        auto point=[&](float t)->Point {
            float envelope=std::sin(t*pi),wave=std::sin(t*4*pi-time+strand*pi)*envelope*9;
            return {delta.x*t+sx*wave,delta.y*t+envelope*24,delta.z*t+sz*wave};
        };
        float pulse=.5f+.5f*std::cos(i/20.f*6*pi-time*2);
        NativeEffects::Color color{uint8_t(140+pulse*65),215,uint8_t(145+pulse*45),uint8_t(140+progress*85)};
        ribbons.Line(point(i/20.f),point((i+1)/20.f),1.5f+progress*1.5f,color);
    }
    ribbons.Draw(play,{caster->world.pos.x,caster->world.pos.y+40,caster->world.pos.z},0,caster,82);
}
}
void DrawCreaturePart(PlayState* play,int part,float glow) {
    GraphicsContext* __gfxCtx=play->state.gfxCtx;
    auto* lights=static_cast<Lights2*>(Graph_Alloc(__gfxCtx,sizeof(Lights2)));if(!lights)return;*lights={};
    auto& source=play->envCtx.lightSettings;
    for(int i=0;i<3;++i) {
        lights->a.l.col[i]=lights->a.l.colc[i]=static_cast<uint8_t>(std::clamp(source.ambientColor[i]+glow*18,0.f,255.f));
        lights->l[0].l.col[i]=lights->l[0].l.colc[i]=source.light1Color[i];lights->l[0].l.dir[i]=source.light1Dir[i];
        lights->l[1].l.col[i]=lights->l[1].l.colc[i]=source.light2Color[i];lights->l[1].l.dir[i]=source.light2Dir[i];
    }
    Gfx_SetupDL_25Opa(__gfxCtx);gSPSetLights2(POLY_OPA_DISP++,(*lights));
    gSPMatrix(POLY_OPA_DISP++,MATRIX_NEWMTX(__gfxCtx),G_MTX_NOPUSH|G_MTX_LOAD|G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++,Model(part));
}
void DrawWarning(PlayState* play,const Actor* actor,float radius,float height,float strength,bool rally) {
    // Warning rings behind the camera can cross its near plane as huge flashes.
    // They are only useful for nearby on-screen threats.
    Vec3f projected,position=actor->world.pos;float w;
    SkinMatrix_Vec3fMtxFMultXYZW(&play->viewProjectionMtxF,&position,&projected,&w);
    if(w<=1 || actor->xzDistToPlayer>900)return;
    GraphicsContext* __gfxCtx=play->state.gfxCtx;
    auto* vertices=static_cast<Vtx*>(Graph_Alloc(__gfxCtx,96*sizeof(Vtx)));if(!vertices)return;
    for(int i=0;i<16;++i) {
        float a=i*6.283185f/16,b=(i+1)*6.283185f/16;
        Vec3f q[]={{std::sin(a)*radius,0,std::cos(a)*radius},{std::sin(b)*radius,0,std::cos(b)*radius},
                  {std::sin(b)*(radius-2),0,std::cos(b)*(radius-2)},{std::sin(a)*(radius-2),0,std::cos(a)*(radius-2)}};
        int order[]={0,1,2,0,2,3};for(int j=0;j<6;++j) {
            auto& v=vertices[i*6+j];v={};auto x=q[order[j]];v.v.ob[0]=static_cast<int16_t>(std::clamp(x.x*8,-32768.f,32767.f));v.v.ob[2]=static_cast<int16_t>(std::clamp(x.z*8,-32768.f,32767.f));
            v.v.cn[0]=rally?150:240;v.v.cn[1]=rally?210:170;v.v.cn[2]=rally?240:90;v.v.cn[3]=static_cast<uint8_t>(std::clamp(strength*230,0.f,230.f));
        }
    }
    FrameInterpolation_RecordOpenChild(actor,81);Matrix_Push();Matrix_Translate(actor->world.pos.x,actor->world.pos.y+height,actor->world.pos.z,MTXMODE_NEW);Matrix_Scale(.125f,.125f,.125f,MTXMODE_APPLY);
    Gfx_SetupDL_25Xlu(__gfxCtx);gDPPipeSync(POLY_XLU_DISP++);gSPClearGeometryMode(POLY_XLU_DISP++,G_LIGHTING|G_CULL_BOTH|G_TEXTURE_GEN|G_TEXTURE_GEN_LINEAR);
    NativeEffects::Feather(vertices,96);
    POLY_XLU_DISP=NativeEffects::Bind(POLY_XLU_DISP,NativeEffects::Texture::Trail,play->gameplayFrames);
    gSPMatrix(POLY_XLU_DISP++,MATRIX_NEWMTX(__gfxCtx),G_MTX_NOPUSH|G_MTX_LOAD|G_MTX_MODELVIEW);
    for(int offset=0;offset<96;offset+=24){gSPVertex(POLY_XLU_DISP++,reinterpret_cast<uintptr_t>(vertices+offset),24,0);for(int j=0;j<24;j+=3)gSP1Triangle(POLY_XLU_DISP++,j,j+1,j+2,0);}
    Matrix_Pop();FrameInterpolation_RecordCloseChild();
}
void DrawCreature(Actor* actor,PlayState* play) {
    auto state=Status(actor);bool sentinel=actor->id==ActorId(Kind::Sentinel);float time=play->gameplayFrames*.14f;
    auto* mend=MendTarget(actor,play);
    float charge=state.phase==Phase::Windup?float(state.timer)/(mend?MendWindup:Windup(sentinel?Kind::Sentinel:Kind::Wraith)):0;
    float death=state.phase==Phase::Dead?std::max(.01f,state.timer/24.f):1;
    for(int part=sentinel?0:8;part<(sentinel?8:15);++part) {
        FrameInterpolation_RecordOpenChild(actor,100+part);Matrix_Push();
        Matrix_Translate(actor->world.pos.x,actor->world.pos.y+(sentinel?0:8+std::sin(time)*3),actor->world.pos.z,MTXMODE_NEW);
        Matrix_RotateY(actor->shape.rot.y*(3.14159265f/32768),MTXMODE_APPLY);float scale=(sentinel?.012f:.009f)*death;Matrix_Scale(scale,scale,scale,MTXMODE_APPLY);
        if(state.phase==Phase::Stagger)Matrix_RotateZ(std::sin(time*5)*.045f,MTXMODE_APPLY);
        if(sentinel && (part==1||part==2)) {
            float side=part==1?-1.f:1.f;Matrix_Translate(side*2400,2600,0,MTXMODE_APPLY);
            Matrix_RotateZ(side*(state.exposed?.6f:std::sin(time)*.015f),MTXMODE_APPLY);Matrix_Translate(-side*2400,-2600,0,MTXMODE_APPLY);
        } else if(sentinel && part>=4) {
            int i=part-4;float x=(i%2?1.f:-1.f)*1500,z=(i<2?-1.f:1.f)*1500;
            Matrix_Translate(x,2300,z,MTXMODE_APPLY);Matrix_RotateX(std::sin(time*2+(i==0||i==3?0:3.14159f))*std::min(.18f,actor->speedXZ*.09f),MTXMODE_APPLY);Matrix_Translate(-x,-2300,-z,MTXMODE_APPLY);
        } else if(!sentinel && (part==10||part==11)) {
            float side=part==10?-1.f:1.f;Matrix_Translate(side*1100,5400,0,MTXMODE_APPLY);Matrix_RotateZ(side*(std::sin(time)*.045f+charge*.65f),MTXMODE_APPLY);Matrix_Translate(-side*1100,-5400,0,MTXMODE_APPLY);
        } else if(!sentinel && (part==12||part==13)) {
            Matrix_Translate(0,4900,-400,MTXMODE_APPLY);Matrix_RotateX(std::sin(time+(part-12))* .09f,MTXMODE_APPLY);Matrix_Translate(0,-4900,400,MTXMODE_APPLY);
        } else if(part==14) {
            Matrix_Translate(0,3300,300,MTXMODE_APPLY);Matrix_RotateY(time,MTXMODE_APPLY);float pulse=1+charge*.6f;Matrix_Scale(pulse,pulse,pulse,MTXMODE_APPLY);Matrix_Translate(0,-3300,-300,MTXMODE_APPLY);
        }
        DrawCreaturePart(play,part,state.exposed?.75f:charge);Matrix_Pop();FrameInterpolation_RecordCloseChild();
    }
    if(mend && mend->update && mend->colChkInfo.health)DrawMend(play,actor,mend,charge);
    else if(charge>0)DrawWarning(play,actor,40+charge*15,2,.45f+charge*.5f);
}
}
