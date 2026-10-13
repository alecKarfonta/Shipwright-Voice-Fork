#include "GearVisuals.h"
#include "GearGeometry.h"
#include "NativeMaterials.h"
#include "WeaponGeometry.h"
#include "RelicGeometry.h"
#include "WaystoneGeometry.h"
#include "RelayGeometry.h"
#include "ItemAppearance.h"
#include "ItemModifierSystem.h"
#include "ARPG.h"
#include "ARPGSave.h"
#include "soh/ResourceManagerHelpers.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include <algorithm>
#include <map>
#include <memory>
#include <tuple>
#include <libultraship/bridge/consolevariablebridge.h>
#include <vector>
extern "C" {
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "objects/object_link_boy/object_link_boy.h"
#include "objects/object_link_child/object_link_child.h"
extern PlayState* gPlayState;
uint8_t Player_IsCustomLinkModel(void);
}
namespace ARPG::GearVisuals {
namespace {
using Kind=ItemAppearance::Kind;
using GearGeometry::Point;
enum class Mount { Pickup, SwordHand, SwordBack, HiltBack, ShieldHand, ShieldBack, Chest, EmptyScabbard, BowHand, HammerHand };
struct Model { std::vector<Vtx> vertices; std::vector<Gfx> commands; };
std::map<int,std::unique_ptr<Model>> models;
Kind Equipped(int slot) {
    const auto& item=gARPGSave.equipment[slot];
    if (InventoryRules::Valid(item) && ItemModifierSystem::IsEquipped(item)) {
        auto shown=item; if(!shown.baseEquip) { shown.baseEquip=CUR_EQUIP_VALUE(slot); }
        return ItemAppearance::WorldKind(shown);
    }
    ARPGItemInstance base{}; base.slot=slot; base.baseEquip=CUR_EQUIP_VALUE(slot);
    return ItemAppearance::WorldKind(base);
}
bool Local(PlayState* play,void* player,bool pause) {
    return play && ARPG::ItemsEnabled() && CVarGetInteger(CVAR_ENHANCEMENT("ARPG.RemasteredGear"),1) && (pause || player==GET_PLAYER(play)) &&
           !Player_IsCustomLinkModel() && !GameInteractor_InvisibleLinkActive();
}
Point Mounted(Point p,Mount mount,bool child,int base) {
    float ageScale=child?.72f:1.0f;
    switch(mount) {
        case Mount::BowHand: return {(220+p.x*100),(-360.4f+p.z*100),(p.y*100)};
        case Mount::HammerHand: return {500+p.y*100,328+p.x*100,-78+p.z*100};
        case Mount::SwordHand: {
            return {(500+p.y*100)*ageScale,(328+p.x*100)*ageScale,(-78+p.z*100)*ageScale};
        }
        case Mount::SwordBack: case Mount::HiltBack: case Mount::EmptyScabbard:
            return {(-150+p.y*100)*ageScale,(18+p.x*100)*ageScale,p.z*100*ageScale};
        case Mount::ShieldHand: return {(-150-p.y*95)*ageScale,(100+p.x*95)*ageScale,(-420-p.z*95)*ageScale};
        case Mount::ShieldBack: return {(630+p.y*95)*ageScale,p.x*95*ageScale,(-520-p.z*95)*ageScale};
        case Mount::Chest:
            return {(740+p.y*100)*ageScale,
                    (std::min(610.0f,575-p.y*72)+p.z*100-p.x*p.x*7.5f)*ageScale,p.x*100*ageScale};
        default: return GearGeometry::Mul(p,100);
    }
}
// [TIME] | [INFO] | GearVisuals.Model | Immutable meshes keep renderer pointers stable across frames; only small composition lists use the frame arena.
Gfx* ModelFor(Kind kind,Mount mount,bool child=false,int base=2) {
    int key=int(kind)*10000+int(mount)*100+int(child)*10+base;
    if(auto it=models.find(key);it!=models.end()) { return it->second->commands.data(); }
    GearGeometry::Mesh mesh;
    if(int(kind)>=210 && int(kind)<210+Waystone::PartCount*10) {
        mesh=Waystone::Build((int(kind)-210)/10,(int(kind)-210)%10);
    } else if(int(kind)>=400 && int(kind)<430) {
        mesh=RelayGeometry::Build((int(kind)-400)/10,(int(kind)-400)%10);
    } else if(ItemAppearance::RelicIndex(kind)>=0) {
        using Pose=RelicGeometry::Pose;
        Pose pose=mount==Mount::SwordBack?Pose::Sheathed:mount==Mount::HiltBack?Pose::Hilt:
                  mount==Mount::EmptyScabbard?Pose::Scabbard:mount==Mount::Chest?Pose::Chest:
                  mount==Mount::Pickup?Pose::Pickup:Pose::Wielded;
        mesh=RelicGeometry::Build(kind,pose,base);
    } else if(kind==Kind::RecurveBow) {
        mesh=WeaponGeometry::Bow(mount!=Mount::BowHand);
    } else if(kind==Kind::WarHammer) {
        mesh=WeaponGeometry::Hammer();
    } else if(ItemAppearance::IsSword(kind)) {
        int element=kind==Kind::Ember?0:kind==Kind::Frost?1:kind==Kind::Light?2:-1;
        mesh=GearGeometry::Sword(element,
            mount==Mount::SwordBack || mount==Mount::HiltBack || mount==Mount::EmptyScabbard,
            mount==Mount::SwordBack || mount==Mount::EmptyScabbard,mount==Mount::EmptyScabbard,base);
    } else if(ItemAppearance::IsShield(kind)) {
        mesh=GearGeometry::Shield(kind==Kind::Root?0:int(kind)-int(Kind::WoodShield)+1);
    } else if(ItemAppearance::IsTunic(kind)) {
        mesh=GearGeometry::Tunic(mount==Mount::Chest,kind==Kind::Star?0:int(kind)-int(Kind::ForestTunic)+1);
    } else if(ItemAppearance::IsBoots(kind)) {
        int variant=kind==Kind::Wind?0:int(kind)-int(Kind::TravelBoots)+1;
        mesh=GearGeometry::Boots(false,variant);
    } else if(int(kind)>100 && int(kind)<100+ARPG_MATERIAL_COUNT) {
        int id=int(kind)-100;
        static constexpr GearGeometry::Color colors[]={{235,67,57},{85,160,244},{250,201,60},{60,208,138},{178,110,235},{217,239,250}};
        if(ARPG::Loot::Gem(id)) {
            float rank=1.f+(id-1)/6*.18f;
            GearGeometry::Gem(mesh,{0,0,0},3.8f*rank,5*rank,2.2f,colors[(id-1)%6]);
            GearGeometry::Ring(mesh,{0,-.7f,-.3f},4.1f*rank,5.3f*rank,.12f,{198,170,95,GearGeometry::Surface::Metal});
        } else {
            GearGeometry::Ellipsoid(mesh,{0,0,0},{4.2f,5.5f,1.6f},{80,93,105,GearGeometry::Surface::Stone},16,8);
            auto ink=GearGeometry::Paint(colors[(id-ARPG::Loot::RuneFirst)%6],GearGeometry::Surface::Gem);
            GearGeometry::Tube(mesh,{0,-3,1.6f},{0,3,1.6f},.22f,ink,8);
            int marks=id-ARPG::Loot::RuneFirst;
            for(int j=0;j<3;++j) {
                float y=2.f-j*2.f;
                GearGeometry::Tube(mesh,{0,y,1.6f},{marks&(1<<j)?2.3f:-2.3f,y+1,1.45f},.22f,ink,8);
            }
        }
    } else { return nullptr; }
    // [TIME] | [INFO] | GearVisuals.Materials | Opaque triangles batch by material while preserving all three vertices and their smooth normals.
    struct Triangle { GearGeometry::Vertex v[3]; };
    std::vector<Triangle> triangles;
    for(size_t i=0;i<mesh.size();i+=3) { triangles.push_back({mesh[i],mesh[i+1],mesh[i+2]}); }
    auto material=[](const auto& v) { return GearGeometry::MaterialKey(v.color); };
    std::stable_sort(triangles.begin(),triangles.end(),[&](const auto& a,const auto& b) { return material(a.v[0])<material(b.v[0]); });
    mesh.clear(); for(const auto& t:triangles) { mesh.insert(mesh.end(),std::begin(t.v),std::end(t.v)); }
    using Surface=NativeMaterials::Surface;
    Surface surface=ItemAppearance::IsTunic(kind)?Surface::Cloth:ItemAppearance::IsBoots(kind)?Surface::Leather:
        int(kind)>=210?Surface::Stone:kind==Kind::WoodShield || kind==Kind::Root || kind==Kind::RecurveBow?Surface::Wood:Surface::Metal;
    auto model=std::make_unique<Model>();
    model->vertices.resize(mesh.size());
    for(size_t i=0;i<mesh.size();++i) {
        auto p=Mounted(mesh[i].p,mount,child,base);
        auto& v=model->vertices[i].v;
        auto first=i-i%3;
        auto plane=GearGeometry::Cross(GearGeometry::Add(mesh[first+1].p,GearGeometry::Mul(mesh[first].p,-1)),GearGeometry::Add(mesh[first+2].p,GearGeometry::Mul(mesh[first].p,-1)));
        NativeMaterials::UV(model->vertices[i],mesh[i].p,plane,16.f,NativeMaterials::ResolveSurface(mesh[i].color,surface));
        v.ob[0]=int16_t(std::lround(p.x)); v.ob[1]=int16_t(std::lround(p.y)); v.ob[2]=int16_t(std::lround(p.z));
        auto n=mesh[i].normal;
        auto tangent=GearGeometry::Unit(GearGeometry::Cross(n,std::abs(n.y)<.9f?Point{0,1,0}:Point{1,0,0}));
        auto bitangent=GearGeometry::Cross(n,tangent);
        auto direction=[&](Point d) { return GearGeometry::Add(Mounted(GearGeometry::Add(mesh[i].p,GearGeometry::Mul(d,.01f)),mount,child,base),GearGeometry::Mul(p,-1)); };
        auto transformed=GearGeometry::Unit(GearGeometry::Cross(direction(tangent),direction(bitangent)));
        auto forward=direction(n);
        if(transformed.x*forward.x+transformed.y*forward.y+transformed.z*forward.z<0) { transformed=GearGeometry::Mul(transformed,-1); }
        v.cn[0]=uint8_t(int8_t(std::lround(transformed.x*127))); v.cn[1]=uint8_t(int8_t(std::lround(transformed.y*127)));
        v.cn[2]=uint8_t(int8_t(std::lround(transformed.z*127))); v.cn[3]=255;
    }
    model->commands.resize(mesh.size()*4+256);
    Gfx* p=model->commands.data();
    p=Gfx_SetupDL(p,25);
    gDPPipeSync(p++);
    gSPClearGeometryMode(p++,G_CULL_BOTH|G_TEXTURE_GEN|G_TEXTURE_GEN_LINEAR);
    gSPSetGeometryMode(p++,G_LIGHTING|G_ZBUFFER|G_SHADE|G_SHADING_SMOOTH);
    gDPSetCycleType(p++,G_CYC_1CYCLE);
    gDPSetRenderMode(p++,G_RM_AA_ZB_OPA_SURF,G_RM_AA_ZB_OPA_SURF2);
    gSPTexture(p++,0,0,0,G_TX_RENDERTILE,G_OFF);
    gDPSetCombineLERP(p++,SHADE,0,PRIMITIVE,0,0,0,0,1,SHADE,0,PRIMITIVE,0,0,0,0,1);
    for(size_t offset=0;offset<mesh.size();) {
        size_t end=offset+3;
        while(end<mesh.size() && material(mesh[end])==material(mesh[offset])) { end+=3; }
        const auto color=mesh[offset].color;
        p=NativeMaterials::Bind(p,NativeMaterials::ResolveSurface(color,surface),color);
        for(size_t batch=offset;batch<end;batch+=24) {
            int count=static_cast<int>(std::min<size_t>(24,end-batch));
            gSPVertex(p++,reinterpret_cast<uintptr_t>(model->vertices.data()+batch),count,0);
            for(int i=0;i<count;i+=3) { gSP1Triangle(p++,i,i+1,i+2,0); }
        }
        offset=end;
    }
    p=Gfx_SetupDL(p,25);
    gSPEndDisplayList(p++);
    model->commands.resize(p-model->commands.data());model->commands.shrink_to_fit();
    auto result=model->commands.data();
    models.emplace(key,std::move(model));
    return result;
}
// [TIME] | [INFO] | GearVisuals.Light | Analytic surface normals use the scene's actual ambient and directional lights.
Gfx* SetupSceneLights(Gfx* p,PlayState* play) {
    auto* lights=static_cast<Lights2*>(Graph_Alloc(play->state.gfxCtx,sizeof(Lights2)));
    if(!lights) { return p; }
    *lights={};
    const auto& scene=play->envCtx.lightSettings;
    for(int i=0;i<3;++i) {
        lights->a.l.col[i]=lights->a.l.colc[i]=scene.ambientColor[i];
        lights->l[0].l.col[i]=lights->l[0].l.colc[i]=scene.light1Color[i];
        lights->l[1].l.col[i]=lights->l[1].l.colc[i]=scene.light2Color[i];
        lights->l[0].l.dir[i]=scene.light1Dir[i]; lights->l[1].l.dir[i]=scene.light2Dir[i];
    }
    gSPSetLights2(p++,(*lights));
    return p;
}
Gfx* Native(const char* path) { return path?ResourceMgr_LoadGfxByName(path):nullptr; }
// [TIME] | [INFO] | GearVisuals.Compose | Native skin meshes can change the GPU matrix; restore the limb matrix before attaching ornaments.
void Compose(PlayState* play,Gfx** target,Gfx* native,Gfx* a,Gfx* b=nullptr) {
    if(!a && !b) { return; }
    auto* buffer=static_cast<Gfx*>(Graph_Alloc(play->state.gfxCtx,24*sizeof(Gfx)));
    auto* identity=static_cast<Mtx*>(Graph_Alloc(play->state.gfxCtx,sizeof(Mtx)));
    if(!buffer || !identity) { return; }
    MtxF matrix{}; matrix.xx=matrix.yy=matrix.zz=matrix.ww=1;
    Matrix_MtxFToMtx(&matrix,identity);
    Gfx* p=buffer;
    if(native) {
        gSPMatrix(p++,identity,G_MTX_PUSH|G_MTX_MUL|G_MTX_MODELVIEW);
        gSPDisplayList(p++,native);
        gSPPopMatrix(p++,G_MTX_MODELVIEW);
    }
    p=SetupSceneLights(p,play);
    if(a) { gSPDisplayList(p++,a); }
    if(b) { gSPDisplayList(p++,b); }
    gSPEndDisplayList(p++);
    *target=buffer;
}
}
void DrawRelay(PlayState* play,const Relay::View& view) {
    if(!play || !view.visible) { return; }
    GraphicsContext* __gfxCtx=play->state.gfxCtx;
    auto draw=[&](int part,int tone) {
        gSPMatrix(POLY_OPA_DISP++,MATRIX_NEWMTX(__gfxCtx),G_MTX_NOPUSH|G_MTX_LOAD|G_MTX_MODELVIEW);
        POLY_OPA_DISP=SetupSceneLights(POLY_OPA_DISP,play);
        gSPDisplayList(POLY_OPA_DISP++,ModelFor(static_cast<Kind>(400+part*10+tone),Mount::Pickup));
    };
    float time=play->gameplayFrames*.06f;
    for(int i=0;i<3;++i) {
        const auto& site=view.lanterns[i];
        Matrix_Push(); Matrix_Translate(site.position.x,site.position.y,site.position.z,MTXMODE_NEW);
        Matrix_Scale(.016f,.016f,.016f,MTXMODE_APPLY); draw(0,0);
        Matrix_Translate(0,3400+std::sin(time+i)*70,0,MTXMODE_APPLY);
        Matrix_RotateY(time*.22f,MTXMODE_APPLY); draw(1,site.lit?2:0); Matrix_Pop();
    }
    for(int i=0;i<3;++i) {
        const auto& spirit=view.spirits[i]; if(!spirit.visible) { continue; }
        auto p=spirit.position;
        if(spirit.carried) { p.x+=std::cos(time*.6f)*24; p.z+=std::sin(time*.6f)*24; }
        Matrix_Push(); Matrix_Translate(p.x,p.y+(spirit.carried?52.f:22.f)+std::sin(time+i)*3,p.z,MTXMODE_NEW);
        Matrix_RotateY(time*.3f+i,MTXMODE_APPLY); Matrix_Scale(.014f,.014f,.014f,MTXMODE_APPLY);
        draw(2,1); Matrix_Pop();
    }
}
void DrawWaystone(PlayState* play,const Waystone::View& view) {
    if(!play || !view.visible) { return; } GraphicsContext* __gfxCtx=play->state.gfxCtx;
    using namespace Waystone;
    Matrix_Push(); Matrix_Translate(view.x,view.y,view.z,MTXMODE_NEW);
    Matrix_RotateY(view.yaw,MTXMODE_APPLY); Matrix_Scale(.016f,.016f,.016f,MTXMODE_APPLY);
    auto draw=[&](int part,int tone) {
        gSPMatrix(POLY_OPA_DISP++,MATRIX_NEWMTX(__gfxCtx),G_MTX_NOPUSH|G_MTX_LOAD|G_MTX_MODELVIEW);
        POLY_OPA_DISP=SetupSceneLights(POLY_OPA_DISP,play);
        gSPDisplayList(POLY_OPA_DISP++,ModelFor(static_cast<Kind>(210+part*10+tone),Mount::Pickup));
    };
    const auto& state=view.state;
    draw(Shrine,0); draw(Coffer,0); draw(Iron+state.oath,state.oath);
    for(int i=0;i<3;++i) { draw(FirstLamp+i,LampTone(state,i)); }
    if(state.cache==Earned) { draw(Treasure,5); }
    // [TIME] | [INFO] | GearVisuals.Waystone | The crystal and coffer lid have separate authored pivots; only their matrices animate.
    Matrix_Push(); Matrix_Translate(0,3600+std::sin(play->gameplayFrames*.045f)*65,0,MTXMODE_APPLY);
    Matrix_RotateY(play->gameplayFrames*.009f,MTXMODE_APPLY); draw(Heart,state.cache==Earned?5:state.oath); Matrix_Pop();
    Matrix_Push(); Matrix_Translate(0,1400,1500,MTXMODE_APPLY);
    Matrix_RotateX(LidAngle(view.open),MTXMODE_APPLY); draw(Lid,0); Matrix_Pop();
    Matrix_Pop();
    if(view.wards) for(const auto& ward:view.sites) {
        Matrix_Push(); Matrix_Translate(ward.x,ward.y,ward.z,MTXMODE_NEW);
        Matrix_RotateY(view.yaw,MTXMODE_APPLY); Matrix_Scale(.018f,.018f,.018f,MTXMODE_APPLY);
        draw(Ward,ward.broken?5:state.oath);
        Matrix_Translate(0,ward.broken?-450.f:ward.progress*450.f,0,MTXMODE_APPLY);
        draw(Seal,ward.broken?5:state.oath); Matrix_Pop();
    }
}
bool DrawMaterial(PlayState* play,Actor* actor,int material) {
    if(!play || !actor || !ARPG::Loot::SocketMaterial(material)) { return false; }
    GraphicsContext* __gfxCtx=play->state.gfxCtx;
    Matrix_Push();
    Matrix_Translate(actor->world.pos.x,actor->world.pos.y+12+std::sin(play->gameplayFrames*.08f),actor->world.pos.z,MTXMODE_NEW);
    Matrix_RotateY(play->gameplayFrames*.025f,MTXMODE_APPLY); Matrix_Scale(.014f,.014f,.014f,MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++,MATRIX_NEWMTX(__gfxCtx),G_MTX_NOPUSH|G_MTX_LOAD|G_MTX_MODELVIEW);
    POLY_OPA_DISP=SetupSceneLights(POLY_OPA_DISP,play);
    gSPDisplayList(POLY_OPA_DISP++,ModelFor(static_cast<Kind>(100+material),Mount::Pickup));
    Matrix_Pop(); return true;
}
bool DrawLoot(PlayState* play,Actor* actor,const ARPGItemInstance& item) {
    auto kind=ItemAppearance::WorldKind(item);
    if(!play || !actor || !ARPG::ItemsEnabled() || !CVarGetInteger(CVAR_ENHANCEMENT("ARPG.RemasteredGear"),1) || kind==Kind::Native) { return false; }
    GraphicsContext* __gfxCtx=play->state.gfxCtx;
    Matrix_Push();
    // [TIME] | [INFO] | GearVisuals.Loot | Equipment floats upright and turns slowly, independently of the collectible rupee's tumbling transform.
    const float lift=item.slot==ARPG_SLOT_SHIELD?23.f:item.slot==ARPG_SLOT_SWORD?20.f:item.slot==ARPG_SLOT_TUNIC?17.f:14.f;
    Matrix_Translate(actor->world.pos.x,actor->world.pos.y+lift+std::sin(play->gameplayFrames*.045f)*1.5f,actor->world.pos.z,MTXMODE_NEW);
    Matrix_RotateY(play->gameplayFrames*.018f+actor->world.pos.x*.01f,MTXMODE_APPLY);
    Matrix_Scale(.01275f,.01275f,.01275f,MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++,MATRIX_NEWMTX(__gfxCtx),G_MTX_NOPUSH|G_MTX_LOAD|G_MTX_MODELVIEW);
    POLY_OPA_DISP=SetupSceneLights(POLY_OPA_DISP,play);
    gSPDisplayList(POLY_OPA_DISP++,ModelFor(kind,Mount::Pickup,false,item.baseEquip));
    Matrix_Pop();
    return true;
}
}
extern "C" void ARPG_EquipmentTunicColor(PlayState* play,void* player,Color_RGB8* color,s32 pause) {
    using namespace ARPG::GearVisuals;
    if(Local(play,player,pause)) {
        auto kind=Equipped(ARPG_SLOT_TUNIC);
        if(ARPG::ItemAppearance::IsTunic(kind)) {
            auto cloth=ARPG::ItemAppearance::RelicIndex(kind)>=0 ? ARPG::RelicGeometry::Cloth(kind) :
                ARPG::GearGeometry::Cloth(kind==Kind::Star?0:int(kind)-int(Kind::ForestTunic)+1);
            *color={cloth.r,cloth.g,cloth.b};
        }
    }
}
extern "C" void ARPG_OverrideEquipmentLimb(PlayState* play,s32 limb,Gfx** dl,void* ref,s32 pause) {
    using namespace ARPG::GearVisuals;
    if(!Local(play,ref,pause) || !dl || !*dl) { return; }
    auto* player=static_cast<Player*>(ref);
    if(!pause && (player->actor.scale.y<0 || player->unk_6AD==2)) { return; }
    bool child=LINK_IS_CHILD;
    // Leave both shin and foot display lists together on the native skin. Their
    // shared ankle vertices use two bone matrices; a rigid collectible boot on
    // the foot cannot follow that joint and overlaps the shin's existing cuff.
    // The native boot textures still use the upscale pack. Loot models, inventory
    // artwork, modifiers and the iron/hover equipment behavior are independent.
    Kind sword=Equipped(ARPG_SLOT_SWORD),shield=Equipped(ARPG_SLOT_SHIELD);
    int base=CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD);
    bool swordOut=pause?base!=0:(player->leftHandType==PLAYER_MODELTYPE_LH_SWORD || player->leftHandType==PLAYER_MODELTYPE_LH_BGS);
    bool shieldOut=pause?!(base==3 || (child && player->currentShield==PLAYER_SHIELD_HYLIAN)):
        player->rightHandType==PLAYER_MODELTYPE_RH_SHIELD;
    if(!pause && !child && limb==PLAYER_LIMB_L_HAND && player->leftHandType==PLAYER_MODELTYPE_LH_HAMMER) {
        Compose(play,dl,Native(gLinkAdultLeftHandClosedNearDL),ModelFor(Kind::WarHammer,Mount::HammerHand));
    } else if(!pause && !child && limb==PLAYER_LIMB_R_HAND && player->rightHandType==PLAYER_MODELTYPE_RH_BOW_SLINGSHOT && Player_HoldsBow(player)) {
        Compose(play,dl,Native(gLinkAdultRightHandClosedNearDL),ModelFor(Kind::RecurveBow,Mount::BowHand));
    } else if(limb==PLAYER_LIMB_L_HAND && sword!=Kind::Native && swordOut) {
        Compose(play,dl,Native(child?gLinkChildLeftFistNearDL:gLinkAdultLeftHandClosedNearDL),ModelFor(sword,Mount::SwordHand,child,(base==3 && !gSaveContext.bgsFlag && gSaveContext.swordHealth<=0)?4:base));
    } else if(limb==PLAYER_LIMB_R_HAND && ARPG::ItemAppearance::IsShield(shield) && shieldOut) {
        Compose(play,dl,Native(child?gLinkChildRightHandClosedNearDL:gLinkAdultRightHandClosedNearDL),ModelFor(shield,Mount::ShieldHand,child));
    } else if(limb==PLAYER_LIMB_TORSO && ARPG::ItemAppearance::IsTunic(Equipped(ARPG_SLOT_TUNIC))) {
        Compose(play,dl,*dl,ModelFor(Equipped(ARPG_SLOT_TUNIC),Mount::Chest,child));
    } else if(limb==PLAYER_LIMB_SHEATH && (sword!=Kind::Native || ARPG::ItemAppearance::IsShield(shield))) {
        bool hasSword=!swordOut,hasShield=!shieldOut && player->currentShield!=PLAYER_SHIELD_NONE;
        if(!pause) {
            hasSword=player->sheathType==PLAYER_MODELTYPE_SHEATH_16 || player->sheathType==PLAYER_MODELTYPE_SHEATH_18;
            hasShield=player->currentShield!=PLAYER_SHIELD_NONE &&
                (player->sheathType==PLAYER_MODELTYPE_SHEATH_18 || player->sheathType==PLAYER_MODELTYPE_SHEATH_19);
        }
        Gfx* native=nullptr;Gfx* blade=nullptr;Gfx* root=nullptr;
        if(ARPG::ItemAppearance::IsShield(shield) || !hasShield) {
            if(sword==Kind::Native) {
                native=Native(child?(hasSword?gLinkChildSwordAndSheathNearDL:gLinkChildSheathNearDL):
                                    (hasSword?gLinkAdultMasterSwordAndSheathNearDL:gLinkAdultSheathNearDL));
            } else { blade=ModelFor(sword,hasSword?Mount::SwordBack:Mount::EmptyScabbard,child,base); }
            if(ARPG::ItemAppearance::IsShield(shield) && hasShield) { root=ModelFor(shield,Mount::ShieldBack,child); }
        } else {
            native=Native(child?(player->currentShield==PLAYER_SHIELD_DEKU?gLinkChildDekuShieldAndSheathNearDL:gLinkChildHylianShieldAndSheathNearDL):
                                (player->currentShield==PLAYER_SHIELD_MIRROR?gLinkAdultMirrorShieldAndSheathNearDL:gLinkAdultHylianShieldAndSheathNearDL));
            if(hasSword) { blade=ModelFor(sword,Mount::HiltBack,child,base); }
        }
        if(!blade && !root && native) { *dl=native; }
        else { Compose(play,dl,native,blade,root); }
    }
}
