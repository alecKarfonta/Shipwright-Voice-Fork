#include "EnemyVisuals.h"
#include "EnemyGeometry.h"
#include "NativeMaterials.h"
#include "WolfosGeometry.h"
#include "WolfosTexture.h"
#include "WolfosMasterAssets.h"
#include "LizalfosGeometry.h"
#include "TektiteGeometry.h"
#include "DekuBabaGeometry.h"
#include "OctorokGeometry.h"
#include "SkulltulaGeometry.h"
#include "BubbleGeometry.h"
#include "ARPG.h"
#include "AffixSystem.h"
#include "soh/cvar_prefixes.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include <map>
#include <memory>
#include <tuple>
extern "C" {
#include "functions.h"
#include "overlays/actors/ovl_En_Test/z_en_test.h"
#include "overlays/actors/ovl_En_Wf/z_en_wf.h"
#include "overlays/actors/ovl_En_Zf/z_en_zf.h"
#include "overlays/actors/ovl_En_Tite/z_en_tite.h"
#include "overlays/actors/ovl_En_Dekubaba/z_en_dekubaba.h"
}
namespace ARPG::EnemyVisuals {
namespace {
static_assert(STALFOS_LIMB_HEAD==11 && STALFOS_LIMB_SWORD==34 && STALFOS_LIMB_WAIST==60);
static_assert(ENZF_LIMB_HEAD==9 && ENZF_LIMB_SWORD==15 && ENZF_LIMB_TRUNK==48);
struct Model { std::vector<Vtx> vertices; std::vector<Gfx> commands; };
using ModelKey=std::tuple<int,int,int,bool,int,bool>;
std::map<ModelKey,std::unique_ptr<Model>> models;
uint32_t Material(GearGeometry::Color c) { return GearGeometry::MaterialKey(c); }
// [TIME] | [INFO] | EnemyVisuals.Model | Permanent limb lists remain valid when native BodyBreak stores them for detached bones and armor.
Gfx* ModelFor(int family,int limb,int rank,bool variant=false,int state=0,bool fading=false) {
    const ModelKey key{family,limb,rank,variant,state,fading};
    if(auto it=models.find(key);it!=models.end()) { return it->second->commands.data(); }
    WolfosGeometry::SkinMesh mesh;
    if(family==ACTOR_EN_WF) { mesh=WolfosGeometry::Wolfos(limb,rank,variant,state); }
    else if(family==ACTOR_EN_ZF) { WolfosGeometry::Attach(mesh,LizalfosGeometry::Lizalfos(limb,rank,variant,state!=0),-1); }
    else if(family==ACTOR_EN_TITE) { WolfosGeometry::Attach(mesh,TektiteGeometry::Tektite(limb,variant),-1); }
    else if(family==ACTOR_EN_DEKUBABA) { WolfosGeometry::Attach(mesh,DekuBabaGeometry::DekuBaba(limb,variant),-1); }
    else if(family==ACTOR_EN_OKUTA) { WolfosGeometry::Attach(mesh,OctorokGeometry::Octorok(limb),-1); }
    else if(family==ACTOR_EN_ST) { WolfosGeometry::Attach(mesh,SkulltulaGeometry::Skulltula(limb),-1); }
    else if(family==ACTOR_EN_BB) { WolfosGeometry::Attach(mesh,BubbleGeometry::Bubble(limb,state),-1); }
    else { WolfosGeometry::Attach(mesh,EnemyGeometry::Stalfos(limb,rank),-1); }
    if(mesh.empty()) { return nullptr; }
    struct Triangle { WolfosGeometry::SkinVertex v[3]; };
    std::vector<Triangle> triangles;
    for(size_t i=0;i<mesh.size();i+=3) { triangles.push_back({mesh[i],mesh[i+1],mesh[i+2]}); }
    std::stable_sort(triangles.begin(),triangles.end(),[](const auto& a,const auto& b) { return Material(a.v[0].surface.color)<Material(b.v[0].surface.color); });
    mesh.clear();for(const auto& t:triangles) { mesh.insert(mesh.end(),std::begin(t.v),std::end(t.v)); }
    using Surface=NativeMaterials::Surface;
    auto surface=family==ACTOR_EN_WF?Surface::Fur:family==ACTOR_EN_ZF?Surface::Scales:
                 family==ACTOR_EN_TITE?Surface::Scales:family==ACTOR_EN_DEKUBABA?Surface::Plant:Surface::Bone;
    auto model=std::make_unique<Model>();model->vertices.resize(mesh.size());
    for(size_t i=0;i<mesh.size();++i) {
        auto& v=model->vertices[i].v;const auto& src=mesh[i].surface;
        NativeMaterials::UV(model->vertices[i],src.p,src.normal,1000.f,NativeMaterials::ResolveSurface(src.color,surface));
        if(family==ACTOR_EN_WF && mesh[i].material<3) { v.tc[0]=mesh[i].s;v.tc[1]=mesh[i].t; }
        v.ob[0]=int16_t(std::lround(src.p.x));v.ob[1]=int16_t(std::lround(src.p.y));v.ob[2]=int16_t(std::lround(src.p.z));
        v.cn[0]=uint8_t(int8_t(std::lround(src.normal.x*127)));v.cn[1]=uint8_t(int8_t(std::lround(src.normal.y*127)));
        v.cn[2]=uint8_t(int8_t(std::lround(src.normal.z*127)));v.cn[3]=255;
    }
    model->commands.resize(mesh.size()*4+256);Gfx* p=model->commands.data();
    gDPPipeSync(p++);
    gSPClearGeometryMode(p++,G_CULL_BOTH|G_TEXTURE_GEN|G_TEXTURE_GEN_LINEAR);
    gSPSetGeometryMode(p++,G_LIGHTING|G_ZBUFFER|G_SHADE|G_SHADING_SMOOTH|G_FOG);
    // [TIME] | [INFO] | EnemyVisuals.Lighting | Keep native actor lights and fog colors so stun, damage flashes and scene fog still affect the replacement.
    gDPSetCycleType(p++,G_CYC_2CYCLE);
    gDPSetRenderMode(p++,G_RM_FOG_SHADE_A,fading?G_RM_AA_ZB_XLU_SURF2:G_RM_AA_ZB_OPA_SURF2);
    gSPTexture(p++,0,0,0,G_TX_RENDERTILE,G_OFF);
    int loadedJoint=family==ACTOR_EN_WF?limb:-1;
    auto loadJoint=[&](int joint) {
        if(joint>=0 && joint!=loadedJoint) {
            // [TIME] | [INFO] | EnemyVisuals.Skin | Bit zero marks a segmented native flex-palette pointer; each vertex is transformed when loaded.
            auto matrix=uintptr_t(0x0D000001)+sizeof(Mtx)*WolfosGeometry::MatrixIndex(joint);
            gSPMatrix(p++,matrix,G_MTX_MODELVIEW|G_MTX_LOAD|G_MTX_NOPUSH);loadedJoint=joint;
        }
    };
    for(size_t offset=0;offset<mesh.size();) {
        size_t end=offset+3;auto c=mesh[offset].surface.color;
        while(end<mesh.size() && Material(mesh[end].surface.color)==Material(c)) { end+=3; }
        if(family==ACTOR_EN_WF && mesh[offset].material<3) { p=WolfosTexture::Bind(p,c); }
        else { p=NativeMaterials::Bind(p,NativeMaterials::ResolveSurface(c,surface),c,fading); }
        if(family==ACTOR_EN_ST && limb==4) {
            // Native teeth warning intentionally uses a flat surface so its ENV pulse stays legible.
            gSPTexture(p++,0,0,0,G_TX_RENDERTILE,G_OFF);
            gDPSetCombineLERP(p++,ENVIRONMENT,PRIMITIVE,ENV_ALPHA,PRIMITIVE,0,0,0,1,COMBINED,0,SHADE,0,0,0,0,1);
        }
        for(size_t batch=offset;batch<end;batch+=24) {
            int count=static_cast<int>(std::min<size_t>(24,end-batch));
            for(int first=0;first<count;) {
                int joint=mesh[batch+first].joint,last=first+1;
                while(last<count && mesh[batch+last].joint==joint) { ++last; }
                loadJoint(joint);
                gSPVertex(p++,reinterpret_cast<uintptr_t>(model->vertices.data()+batch+first),last-first,first);
                first=last;
            }
            for(int i=0;i<count;i+=3) { gSP1Triangle(p++,i,i+1,i+2,0); }
        }
        offset=end;
    }
    if(family==ACTOR_EN_WF) { loadJoint(limb); }
    p=Gfx_SetupDL(p,25);gSPEndDisplayList(p++);
    model->commands.resize(p-model->commands.data());auto result=model->commands.data();
    models.emplace(key,std::move(model));return result;
}
}
}
extern "C" void ARPG_OverrideEnemyLimb(PlayState* play,Actor* actor,s32 limb,Gfx** dl) {
    if(!play || !actor || (actor->id!=ACTOR_EN_TEST && actor->id!=ACTOR_EN_WF && actor->id!=ACTOR_EN_ZF && actor->id!=ACTOR_EN_TITE && actor->id!=ACTOR_EN_DEKUBABA && actor->id!=ACTOR_EN_OKUTA && actor->id!=ACTOR_EN_ST && actor->id!=ACTOR_EN_BB) || !dl || !*dl || !ARPG::Enabled() ||
       !CVarGetInteger(CVAR_ENHANCEMENT("ARPG.RemasteredEnemies"),1)) { return; }
    // [TIME] | [INFO] | EnemyVisuals.WolfosMaster | Keep the native display-list path when an edited Blender master is installed for this species.
    if(actor->id==ACTOR_EN_WF && ARPG::WolfosMasterAssets::Available(actor->params!=WOLFOS_NORMAL))return;
    // Keep distant inhabitants visible using the lightweight native limbs.
    // Dense remastered meshes are only useful nearby; interpreting them for an
    // entire region every interpolated frame overwhelms the graphics thread.
    // Measure from the camera so free cameras and cutscenes choose useful detail.
    const auto& eye=GET_ACTIVE_CAM(play)->eye;
    float dx=actor->world.pos.x-eye.x,dy=actor->world.pos.y-eye.y,dz=actor->world.pos.z-eye.z;
    if (dx*dx+dy*dy+dz*dz>1500.f*1500.f) { return; }
    // [TIME] | [INFO] | EnemyVisuals.Octorok | Projectile actors share the ID but never use the creature's limb mesh.
    if(actor->id==ACTOR_EN_OKUTA && actor->params!=0)return;
    // [TIME] | [INFO] | EnemyVisuals.Bubble | Flame-trail actors share the ID but must never acquire skull geometry.
    if(actor->id==ACTOR_EN_BB && (actor->params>=0 || actor->params < -5))return;
    const auto* affix=AffixSystem::GetAffixData(actor);
    int rank=affix?std::min<int>(affix->tier,2):0;
    bool variant=false,fading=false;int state=0;
    if(actor->id==ACTOR_EN_WF) {
        const auto* wolfos=reinterpret_cast<const EnWf*>(actor);variant=actor->params!=WOLFOS_NORMAL;
        if(limb==WOLFOS_LIMB_EYES) { state=wolfos->eyeIndex==3?1:std::clamp<int>(wolfos->eyeIndex,0,2); }
    } else if(actor->id==ACTOR_EN_ZF) {
        const auto* lizalfos=reinterpret_cast<const EnZf*>(actor);variant=actor->params==ENZF_TYPE_DINOLFOS;fading=lizalfos->alpha<255;
        if(limb==ENZF_LIMB_SWORD || limb==ENZF_LIMB_SCABBARD) { state=lizalfos->swordSheathed!=0; }
    } else if(actor->id==ACTOR_EN_TITE) {
        variant=actor->params==TEKTITE_BLUE;
        rank=0; // Red/blue anatomy is shared across affix ranks; keep the cache bounded.
    } else if(actor->id==ACTOR_EN_OKUTA || actor->id==ACTOR_EN_ST) {
        rank=0;
    } else if(actor->id==ACTOR_EN_BB) {
        rank=0;state=std::min(3,-actor->params-1);
    } else if(actor->id==ACTOR_EN_DEKUBABA) {
        variant=actor->params==DEKUBABA_BIG;
        rank=0; // Native size, jaw frames and stem transforms remain authoritative.
    }
    if(auto* replacement=ARPG::EnemyVisuals::ModelFor(actor->id,limb,rank,variant,state,fading)) { *dl=replacement; }
}
