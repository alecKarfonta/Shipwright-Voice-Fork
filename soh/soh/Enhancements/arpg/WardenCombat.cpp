#include "NativeEffects.h"
#include "WardenCombat.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/frame_interpolation.h"
#include <cstring>
#include <limits>
extern "C" {
#include "z64.h"
#include "functions.h"
#include "variables.h"
#include "macros.h"
}
namespace ARPG::Warden {
namespace {
struct GroundTriangle { Vec3f a,b,c; int pulse; };
struct GroundEdge { Vec3f a,b; int pulse; };
std::vector<GroundTriangle> triangles;
std::vector<GroundEdge> edges;
Actor* source=nullptr;
View view{};
int delay=100;
uint32_t lastFrame=std::numeric_limits<uint32_t>::max();
bool ClearLine(PlayState* play,Vec3f a,Vec3f b) {
    a.y+=35; b.y+=35; Vec3f hit{}; CollisionPoly* poly=nullptr; int bg=BGCHECK_SCENE;
    return !BgCheck_EntityLineTest1(&play->colCtx,&a,&b,&hit,&poly,true,true,true,true,&bg);
}
bool Ground(PlayState* play,Vec3f& p) {
    if(!BgCheck_PosInStaticBoundingBox(&play->colCtx,&p)) return false;
    Vec3f ray=p; ray.y=view.y+150; CollisionPoly* poly=nullptr; int bg=BGCHECK_SCENE;
    float floor=BgCheck_EntityRaycastFloor5(play,&play->colCtx,&poly,&bg,source,&ray);
    if(!poly || bg!=BGCHECK_SCENE || floor<=BGCHECK_Y_MIN || std::abs(floor-view.y)>120 || poly->normal.y<24000) return false;
    p.y=floor+2; return true;
}
Vec3f World(Point p) {
    float sn=std::sin(view.yaw),cs=std::cos(view.yaw);
    return {view.x+cs*p.x+sn*p.z,view.y,view.z-sn*p.x+cs*p.z};
}
// [TIME] | [INFO] | Warden.Project | Cache terrain-conforming marks once per cast; rejected ground cannot deal invisible damage.
bool Prepare(PlayState* play,float distance) {
    auto pattern=Make(view.oath,distance); triangles.clear(); edges.clear();
    Vec3f start{view.x,view.y,view.z};
    for(const auto& t:pattern.triangles) {
        auto a=World(t.a),b=World(t.b),c=World(t.c);
        if(!Ground(play,a) || !Ground(play,b) || !Ground(play,c)) continue;
        Vec3f middle{(a.x+b.x+c.x)/3,(a.y+b.y+c.y)/3,(a.z+b.z+c.z)/3};
        if(!ClearLine(play,start,middle)) continue;
        triangles.push_back({a,b,c,t.pulse});
    }
    for(const auto& e:pattern.edges) {
        auto a=World(e.a),b=World(e.b);
        if(!Ground(play,a) || !Ground(play,b)) continue;
        Vec3f middle{(a.x+b.x)/2,(a.y+b.y)/2,(a.z+b.z)/2};
        if(ClearLine(play,start,middle)) edges.push_back({a,b,e.pulse});
    }
    view.triangles=int(triangles.size()); return !triangles.empty();
}
bool Hits(Vec3f point) {
    for(const auto& t:triangles) {
        if(!Active(view.oath,view.age,t.pulse)) continue;
        Triangle flat{{t.a.x,t.a.z},{t.b.x,t.b.z},{t.c.x,t.c.z},t.pulse};
        Point p{point.x,point.z}; if(!Contains(flat,p)) continue;
        float area=Cross(flat.a,flat.b,flat.c);
        float height=(Cross(p,flat.b,flat.c)*t.a.y+Cross(flat.a,p,flat.c)*t.b.y+Cross(flat.a,flat.b,p)*t.c.y)/area;
        if(std::abs(point.y-height)<38) return true;
    }
    return false;
}
}
void Reset() { source=nullptr; view={}; triangles.clear(); edges.clear(); delay=100; lastFrame=std::numeric_limits<uint32_t>::max(); }
View GetView() { return view; }
float OpeningMultiplier(Actor* actor) { return actor && actor==source && view.phase==Phase::Opening?1.5f:1.f; }
void Tick(PlayState* play,Actor* actor,int oath,bool inControl) {
    if(!play || !actor || !actor->update || !actor->colChkInfo.health || oath<0 || oath>=int(Attacks.size())) { Reset(); return; }
    if(actor!=source || oath!=view.oath) { Reset(); source=actor; view.oath=oath; }
    if(lastFrame==play->gameplayFrames) return;
    lastFrame=play->gameplayFrames;
    if(!inControl) return;
    auto* player=GET_PLAYER(play);
    if(view.phase==Phase::Waiting) {
        if(delay>0) { --delay; return; }
        auto a=actor->world.pos,p=player->actor.world.pos;
        float distance=std::hypot(p.x-a.x,p.z-a.z);
        if(distance>680 || distance<10 || std::abs(p.y-a.y)>120 || !ClearLine(play,a,p)) return;
        view={Phase::Warning,oath,0,a.x,a.y,a.z,std::atan2(p.x-a.x,p.z-a.z),0,false};
        if(!Prepare(play,distance)) { view.phase=Phase::Waiting; delay=40; return; }
        SoundSource_PlaySfxAtFixedWorldPos(play,&a,30,NA_SE_EV_LIGHT_GATHER);
        return;
    }
    ++view.age; view.phase=Stage(oath,view.age);
    if(view.phase==Phase::Waiting) { triangles.clear(); edges.clear(); view.triangles=0; delay=80; return; }
    if(view.age==Attacks[oath].warning) {
        Vec3f p{view.x,view.y,view.z};
        SoundSource_PlaySfxAtFixedWorldPos(play,&p,30,oath==2?NA_SE_EV_EXPLOSION:oath==3?NA_SE_EV_ICE_BROKEN:NA_SE_EV_LIGHTNING);
    }
    if(view.phase!=Phase::Strike || view.hit || player->invincibilityTimer!=0 ||
       (player->stateFlags1&(PLAYER_STATE1_IN_WATER|PLAYER_STATE1_ON_HORSE)) || !Hits(player->actor.world.pos)) return;
    Vec3f origin{view.x,view.y,view.z};
    if(!ClearLine(play,origin,player->actor.world.pos)) return;
    // [TIME] | [INFO] | Warden.Hit | One hit per cast travels through outgoing affixes and the native defensive health path, respecting skill and damage invulnerability.
    float damage=8;
    GameInteractor_ExecuteOnDamageActor(player,actor,DMG_UNBLOCKABLE,AT_TYPE_ENEMY,&damage);
    view.hit=true;
    int health=gSaveContext.health;
    if(play->damagePlayer) play->damagePlayer(play,-std::clamp(int(std::lround(damage)),0,256));
    if(gSaveContext.health<health) {
        Actor_SetPlayerKnockbackLargeNoDamage(play,actor,3.f,Math_Vec3f_Yaw(&origin,&player->actor.world.pos),3.f);
        player->invincibilityTimer=20;
    }
}
void Draw(PlayState* play) {
    if(!play || (view.phase!=Phase::Warning && view.phase!=Phase::Strike)) return;
    constexpr std::array<std::array<uint8_t,3>,5> inks{{{245,189,81},{110,237,170},{255,93,34},{117,208,255},{204,151,255}}};
    auto ink=inks[view.oath]; std::array<Vtx,2048> mesh{}; int count=0;
    auto vertex=[&](Vec3f p,int alpha) {
        auto& v=mesh[count++].v;
        v.ob[0]=int16_t(std::clamp((p.x-view.x)*4,-30000.f,30000.f));
        v.ob[1]=int16_t(std::clamp((p.y-view.y)*4,-30000.f,30000.f));
        v.ob[2]=int16_t(std::clamp((p.z-view.z)*4,-30000.f,30000.f));
        v.cn[0]=ink[0]; v.cn[1]=ink[1]; v.cn[2]=ink[2]; v.cn[3]=uint8_t(alpha);
    };
    auto tri=[&](Vec3f a,Vec3f b,Vec3f c,int alpha) {
        if(count+3>int(mesh.size())) return;
        vertex(a,alpha); vertex(b,alpha); vertex(c,alpha);
    };
    for(const auto& t:triangles) {
        bool active=Active(view.oath,view.age,t.pulse);
        bool spent=view.phase==Phase::Strike && view.oath==2 && view.age-Attacks[2].warning>=(t.pulse+1)*12;
        if(!spent) tri(t.a,t.b,t.c,active?135:35+int(14*std::sin(view.age*.25f)));
    }
    for(size_t i=0;i<edges.size();++i) {
        const auto& e=edges[i]; bool active=Active(view.oath,view.age,e.pulse);
        if(view.phase==Phase::Strike && view.oath==2 && view.age-Attacks[2].warning>=(e.pulse+1)*12) continue;
        float dx=e.b.x-e.a.x,dz=e.b.z-e.a.z,length=std::max(1.f,std::hypot(dx,dz));
        float x=dz/length*2.2f,z=-dx/length*2.2f;
        Vec3f a{e.a.x-x,e.a.y+1,e.a.z-z},b{e.b.x-x,e.b.y+1,e.b.z-z},c{e.b.x+x,e.b.y+1,e.b.z+z},d{e.a.x+x,e.a.y+1,e.a.z+z};
        tri(a,b,c,230); tri(a,c,d,230);
        if(active) {
            Vec3f tip{(e.a.x+e.b.x)/2,(e.a.y+e.b.y)/2+28+18*std::sin(i*2.f+view.age*.7f),(e.a.z+e.b.z)/2};
            tri(e.a,e.b,tip,185);
        }
    }
    if(!count) return;
    GraphicsContext* __gfxCtx=play->state.gfxCtx;
    auto* vertices=static_cast<Vtx*>(Graph_Alloc(__gfxCtx,count*sizeof(Vtx))); if(!vertices) return;
    std::memcpy(vertices,mesh.data(),count*sizeof(Vtx));
    FrameInterpolation_RecordOpenChild(&view,0);
    Gfx_SetupDL_25Xlu(__gfxCtx); gDPPipeSync(POLY_XLU_DISP++);
    gSPClearGeometryMode(POLY_XLU_DISP++,G_LIGHTING|G_CULL_BOTH|G_TEXTURE_GEN|G_TEXTURE_GEN_LINEAR|G_FOG);
    gSPSetGeometryMode(POLY_XLU_DISP++,G_ZBUFFER|G_SHADE|G_SHADING_SMOOTH);
    gDPSetCycleType(POLY_XLU_DISP++,G_CYC_1CYCLE);
    gDPSetRenderMode(POLY_XLU_DISP++,G_RM_AA_ZB_XLU_SURF,G_RM_AA_ZB_XLU_SURF2);
    NativeEffects::Feather(vertices,count,.9f);
    POLY_XLU_DISP=NativeEffects::Bind(POLY_XLU_DISP,NativeEffects::Texture::Trail,play->gameplayFrames);
    Matrix_Push(); Matrix_Translate(view.x,view.y,view.z,MTXMODE_NEW); Matrix_Scale(.25f,.25f,.25f,MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++,MATRIX_NEWMTX(__gfxCtx),G_MTX_NOPUSH|G_MTX_LOAD|G_MTX_MODELVIEW);
    for(int offset=0;offset<count;offset+=24) {
        int size=std::min(24,count-offset); gSPVertex(POLY_XLU_DISP++,reinterpret_cast<uintptr_t>(vertices+offset),size,0);
        for(int i=0;i<size;i+=3) gSP1Triangle(POLY_XLU_DISP++,i,i+1,i+2,0);
    }
    Matrix_Pop(); FrameInterpolation_RecordCloseChild();
}
}
