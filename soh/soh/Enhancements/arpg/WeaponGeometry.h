#pragma once
#include "GearGeometry.h"

namespace ARPG::WeaponGeometry {
using namespace GearGeometry;
// The same authored silhouette is used by the pickup and hand mounts.
inline Mesh Bow(bool string=true) {
    Mesh m;
    for(float side:{-1.f,1.f}) {
        Curve(m,{{0,0,0},{1.2f,side*5,0},{2,side*10,0},{.5f,side*15,0},{-2,side*18,0}},.7f,oak,4);
        Curve(m,{{-.1f,side*2,.64f},{1.15f,side*6,.55f},{1.6f,side*10,.48f},{.45f,side*14,.5f},{-1.8f,side*17.6f,.2f}},.14f,gold,3);
        Curve(m,{{-2,side*18,0},{-3.1f,side*20,0},{-5,side*20.7f,0},{-5.6f,side*19.5f,0}},.46f,Paint(cream,Surface::Bone),3);
        Tube(m,{-1.95f,side*17.4f,0},{-2.2f,side*18.1f,0},.85f,bronze,12);
        Tube(m,{.3f,side*2.1f,0},{.42f,side*2.8f,0},.95f,gold,12);
    }
    Tube(m,{0,-2,0},{0,2,0},.84f,Paint(darkJade,Surface::Leather),12);
    for(int i=0;i<9;++i) {float y=-1.8f+i*.45f;Curve(m,{{-.65f,y,.3f},{0,y+.2f,.84f},{.65f,y+.4f,.3f}},.075f,Paint(jade,Surface::Leather),2);}
    Gem(m,{.8f,4,0},.9f,1.45f,.82f,bronze);Gem(m,{.8f,4,.74f},.6f,1.1f,.22f,cyan);
    if(string)Tube(m,{-2.2f,-18,0},{-2.2f,18,0},.08f,Paint(cream,Surface::Cloth),6);
    FinishMaterials(m,Surface::Metal);
    return m;
}
inline void BeveledHead(Mesh& m,Point center,Point radius,float bevel,Color ink) {
    Point corners[]={{-radius.x+bevel,-radius.y,0},{radius.x-bevel,-radius.y,0},{radius.x,-radius.y+bevel,0},{radius.x,radius.y-bevel,0},{radius.x-bevel,radius.y,0},{-radius.x+bevel,radius.y,0},{-radius.x,radius.y-bevel,0},{-radius.x,-radius.y+bevel,0}};
    for(int i=0;i<8;++i) {
        auto a=Add(center,corners[i]),b=Add(center,corners[(i+1)%8]);
        auto af=Add(a,{0,0,radius.z}),bf=Add(b,{0,0,radius.z}),ab=Add(a,{0,0,-radius.z}),bb=Add(b,{0,0,-radius.z});
        Quad(m,af,ab,bb,bf,Shade(ink,i%2?.85f:1.1f));
        Tri(m,Add(center,{0,0,radius.z}),af,bf,ink);
        Tri(m,Add(center,{0,0,-radius.z}),bb,ab,Shade(ink,.75f));
    }
}
inline Mesh Hammer() {
    Mesh m;Color iron{71,78,88},wrap{93,34,33,Surface::Leather},bright{139,73,52,Surface::Leather};
    Tube(m,{0,-14,0},{0,15,0},.68f,oak,12);
    Tube(m,{0,-12,0},{0,-3,0},.88f,wrap,12);
    for(int i=0;i<16;++i) {float y=-11.7f+i*.55f;Curve(m,{{-.7f,y,.2f},{0,y+.2f,.9f},{.7f,y+.45f,.2f}},.09f,bright,2);}
    for(float y:{-13.f,-2.5f,11.3f})Tube(m,{0,y-.35f,0},{0,y+.35f,0},1.1f,bronze,12);
    BeveledHead(m,{0,16,0},{7.3f,4.1f,3},.9f,iron);
    for(float x:{-6.8f,6.8f}) {
        BeveledHead(m,{x,16,0},{1.05f,4.6f,3.4f},.65f,bronze);
        BeveledHead(m,{x*.78f,16,0},{.28f,4.2f,3.2f},.2f,silver);
    }
    BeveledHead(m,{0,-14,0},{1.1f,1.25f,1.1f},.4f,bronze);
    for(float face:{-1.f,1.f}) {
        Gem(m,{0,16,face*3.1f},1.55f,1.55f,.35f,gold);
        Gem(m,{0,16,face*3.45f},1.1f,1.1f,.2f,{246,127,44});
        for(float side:{-1.f,1.f})Relief(m,{{side*2.4f,14.2f,face*3.03f},{side*4.5f,16,face*3.03f},{side*2.4f,17.8f,face*3.03f}},.1f,bronze);
    }
    FinishMaterials(m,Surface::Metal);
    return m;
}
}
