#pragma once
#include "GearGeometry.h"

namespace ARPG::Quests::Plants {
using GearGeometry::Point;
using GearGeometry::Add;
using GearGeometry::Mul;
using GearGeometry::Cross;
using GearGeometry::Unit;
using GearGeometry::pi;

struct Vertex { Point p, normal; float u, v; };
using Mesh = std::vector<Vertex>;
struct Sapling { Mesh bark, leaves; };

inline void Triangle(Mesh& mesh, Vertex a, Vertex b, Vertex c) {
    mesh.insert(mesh.end(), {a,b,c});
}

// Continuous UVs along tapered branches keep the original wood02 bark grain vertical.
inline void Branch(Mesh& mesh, Point a, Point b, float r0, float r1, float v0=0) {
    auto delta=Add(b,Mul(a,-1)), axis=Unit(delta);
    auto u=Unit(Cross(axis,std::abs(axis.y)<.9f?Point{0,1,0}:Point{1,0,0})), v=Cross(axis,u);
    float length=std::sqrt(delta.x*delta.x+delta.y*delta.y+delta.z*delta.z);
    for(int i=0;i<6;++i) {
        float t=i*pi/3, next=(i+1)*pi/3;
        auto n=Add(Mul(u,std::cos(t)),Mul(v,std::sin(t)));
        auto q=Add(Mul(u,std::cos(next)),Mul(v,std::sin(next)));
        auto nn=Unit(Add(n,Mul(axis,(r0-r1)/length)));
        auto nq=Unit(Add(q,Mul(axis,(r0-r1)/length)));
        Vertex ap{Add(a,Mul(n,r0)),nn,i/6.f,v0}, aq{Add(a,Mul(q,r0)),nq,(i+1)/6.f,v0};
        Vertex bp{Add(b,Mul(n,r1)),nn,i/6.f,v0-length/18}, bq{Add(b,Mul(q,r1)),nq,(i+1)/6.f,v0-length/18};
        Triangle(mesh,ap,aq,bq);Triangle(mesh,ap,bq,bp);
    }
}

// A gently folded leaf, with its stem at the bottom of the native Hana leaf texture.
// The cutout supplies the serrated outline; the fold gives the silhouette depth from above.
inline void Leaf(Mesh& mesh, Point root, float yaw, float length, float lift) {
    Point forward{std::sin(yaw),0,std::cos(yaw)}, side{std::cos(yaw),0,-std::sin(yaw)};
    Vertex grid[3][3];
    for(int row=0;row<3;++row)for(int col=0;col<3;++col) {
        float t=row*.5f, s=(col-1)*.5f;
        Point p=Add(root,Add(Mul(forward,t*length),Mul(side,s*length*.70f)));
        p.y+=lift*t+std::sin(t*pi)*length*.14f-std::abs(s)*length*.12f;
        // Like the native two-sided foliage, normals face predominantly up.
        auto normal=Unit(Add(Point{0,1,0},Add(Mul(forward,-lift/length),Mul(side,s*.3f))));
        grid[row][col]={p,normal,col*.5f,1-t};
    }
    for(int row=0;row<2;++row)for(int col=0;col<2;++col) {
        Triangle(mesh,grid[row][col],grid[row+1][col],grid[row+1][col+1]);
        Triangle(mesh,grid[row][col],grid[row+1][col+1],grid[row][col+1]);
    }
}

inline Sapling Build(bool restored,int variant) {
    Sapling result;
    float height=restored?67.f+variant*5.f:22.f+variant*2.f;
    auto trunk=[&](float t) -> Point {return {std::sin(t*2.3f+variant*.7f)*t*4,height*t,std::sin(t*3+variant)*t*2};};
    for(int i=0;i<5;++i) {
        float t=i/5.f, next=(i+1)/5.f;
        float radius=restored?1.8f:.7f;
        Branch(result.bark,trunk(t),trunk(next),radius*(1-t)+.15f,radius*(1-next)+.15f,-height*t/18);
    }
    int branches=restored?9:3;
    for(int i=0;i<branches;++i) {
        float t=.35f+.58f*i/branches, yaw=i*2.39996f+variant*.83f;
        float reach=restored?12.f+(i%3)*2.f:3.5f;
        Point start=trunk(t), end=Add(start,{std::sin(yaw)*reach,restored?7.f:3.f,std::cos(yaw)*reach});
        Branch(result.bark,start,end,restored?.72f:.35f,.16f);
        float leafSize=restored?13.f+(i%3)*2.f:8.f;
        Leaf(result.leaves,end,yaw,leafSize,leafSize*.24f);
        if(restored) {
            Point fork=Add(Mul(start,.22f),Mul(end,.78f));
            Leaf(result.leaves,fork,yaw+1.05f,leafSize*.85f,leafSize*.42f);
            Leaf(result.leaves,fork,yaw-1.05f,leafSize*.80f,leafSize*.38f);
        }
    }
    Leaf(result.leaves,trunk(.94f),variant+.3f,restored?12.f:7.f,restored?11.f:5.f);
    return result;
}

// Placement is already saved by the quest system, so visual variation survives reloads.
inline uint32_t SiteSeed(float x,float z) {
    uint32_t seed=static_cast<uint32_t>(static_cast<int32_t>(std::floor(x)))*0x9e3779b9u;
    seed^=static_cast<uint32_t>(static_cast<int32_t>(std::floor(z)))*0x85ebca6bu;
    seed^=seed>>16;seed*=0x7feb352du;return seed^(seed>>15);
}
}
