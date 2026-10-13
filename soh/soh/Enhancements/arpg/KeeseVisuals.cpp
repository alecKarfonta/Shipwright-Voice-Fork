#include "KeeseVisuals.h"
#include "KeeseGeometry.h"
#include "NativeMaterials.h"
#include "ARPG.h"
#include "soh/cvar_prefixes.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include <memory>
extern "C" {
#include "functions.h"
#include "overlays/actors/ovl_En_Firefly/z_en_firefly.h"
}
namespace {
struct Model { std::vector<Vtx> vertices; std::vector<Gfx> commands; };
std::unique_ptr<Model> models[3][28][2];
uint32_t Material(ARPG::GearGeometry::Color c) { return ARPG::GearGeometry::MaterialKey(c); }
Gfx* ModelFor(int variant,int limb,bool translucent) {
    if(limb<1 || limb>27)return nullptr;
    auto& cached=models[variant][limb][translucent];
    if(cached)return cached->commands.data();
    auto mesh=ARPG::KeeseGeometry::Keese(limb,variant);
    if(mesh.empty())return nullptr;
    struct Triangle { ARPG::GearGeometry::Vertex vertices[3]; };
    std::vector<Triangle> triangles;
    for(size_t i=0;i<mesh.size();i+=3)triangles.push_back({mesh[i],mesh[i+1],mesh[i+2]});
    std::stable_sort(triangles.begin(),triangles.end(),[](const auto& a,const auto& b){return Material(a.vertices[0].color)<Material(b.vertices[0].color);});
    mesh.clear();for(const auto& t:triangles)mesh.insert(mesh.end(),std::begin(t.vertices),std::end(t.vertices));
    auto model=std::make_unique<Model>();model->vertices.resize(mesh.size());
    for(size_t i=0;i<mesh.size();++i) {
        auto& v=model->vertices[i].v;const auto& src=mesh[i];
        ARPG::NativeMaterials::UV(model->vertices[i],src.p,src.normal,1000.f,src.color.surface);
        v.ob[0]=int16_t(std::lround(src.p.x));v.ob[1]=int16_t(std::lround(src.p.y));v.ob[2]=int16_t(std::lround(src.p.z));
        v.cn[0]=uint8_t(int8_t(std::lround(src.normal.x*127)));v.cn[1]=uint8_t(int8_t(std::lround(src.normal.y*127)));
        v.cn[2]=uint8_t(int8_t(std::lround(src.normal.z*127)));v.cn[3]=255;
    }
    model->commands.resize(mesh.size()*4+256);Gfx* p=model->commands.data();
    gDPPipeSync(p++);gSPClearGeometryMode(p++,G_CULL_BOTH|G_TEXTURE_GEN|G_TEXTURE_GEN_LINEAR);
    gSPSetGeometryMode(p++,G_LIGHTING|G_ZBUFFER|G_SHADE|G_SHADING_SMOOTH|G_FOG);
    gDPSetCycleType(p++,G_CYC_2CYCLE);
    gDPSetRenderMode(p++,G_RM_FOG_SHADE_A,translucent?G_RM_AA_ZB_XLU_SURF2:G_RM_AA_ZB_OPA_SURF2);
    gSPTexture(p++,0,0,0,G_TX_RENDERTILE,G_OFF);
    // Native lighting, damage filters, matrices and aura anchors are left in charge.
    // Fire Keese set ENV alpha to zero for their old texture; solid mesh alpha is independent.
    for(size_t offset=0;offset<mesh.size();) {
        size_t end=offset+3;auto c=mesh[offset].color;
        while(end<mesh.size() && Material(mesh[end].color)==Material(c))end+=3;
        p=ARPG::NativeMaterials::Bind(p,c.surface,c);
        if(translucent) { gDPSetRenderMode(p++,G_RM_FOG_SHADE_A,G_RM_AA_ZB_XLU_SURF2); }
        for(size_t batch=offset;batch<end;batch+=24) {
            int count=int(std::min<size_t>(24,end-batch));
            gSPVertex(p++,reinterpret_cast<uintptr_t>(model->vertices.data()+batch),count,0);
            for(int i=0;i<count;i+=3)gSP1Triangle(p++,i,i+1,i+2,0);
        }
        offset=end;
    }
    p=Gfx_SetupDL(p,25);gSPEndDisplayList(p++);
    model->commands.resize(p-model->commands.data());cached=std::move(model);return cached->commands.data();
}
}
extern "C" void ARPG_OverrideKeeseLimb(PlayState* play,Actor* actor,s32 limb,Gfx** dl,s32 translucent) {
    if(!play || !actor || actor->id!=ACTOR_EN_FIREFLY || !dl || !*dl || !ARPG::Enabled() ||
       !CVarGetInteger(CVAR_ENHANCEMENT("ARPG.RemasteredEnemies"),1))return;
    const auto& eye=GET_ACTIVE_CAM(play)->eye;
    float dx=actor->world.pos.x-eye.x,dy=actor->world.pos.y-eye.y,dz=actor->world.pos.z-eye.z;
    if(dx*dx+dy*dy+dz*dz>1500.f*1500.f)return;
    const auto* keese=reinterpret_cast<const EnFirefly*>(actor);
    int variant=ARPG::KeeseGeometry::Variant(actor->params,keese->onFire);
    if(auto* replacement=ModelFor(variant,limb,translucent!=0))*dl=replacement;
}
extern "C" s32 ARPG_IsKeeseHead(Gfx* dl) {
    if(!dl)return false;
    for(auto& variant:models)for(auto& model:variant[27])if(model && dl==model->commands.data())return true;
    return false;
}
