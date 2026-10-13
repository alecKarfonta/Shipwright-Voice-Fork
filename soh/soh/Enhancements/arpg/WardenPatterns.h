#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace ARPG::Warden {
constexpr float Pi=3.14159265359f;
enum class Phase { Waiting, Warning, Strike, Opening };
struct Definition { const char* name; const char* counter; int warning, strike, opening; };
inline constexpr std::array<Definition,5> Attacks{{
    {"Iron verdict", "Step behind the cleave", 42, 12, 48},
    {"Hunter's trace", "Dodge across the long mark", 32, 16, 42},
    {"Cinder brands", "Leave the three marked circles", 44, 36, 44},
    {"Winter's eye", "Reach the center or leave the ring", 48, 16, 48},
    {"Storm lattice", "Move between the lightning lanes", 40, 18, 44},
}};
struct Point { float x=0,z=0; };
struct Triangle { Point a,b,c; int pulse=0; };
struct Edge { Point a,b; int pulse=0; };
struct Pattern { std::vector<Triangle> triangles; std::vector<Edge> edges; };
inline float Cross(Point a,Point b,Point c) { return (b.x-a.x)*(c.z-a.z)-(b.z-a.z)*(c.x-a.x); }
inline bool Contains(const Triangle& t,Point p) {
    float a=Cross(t.a,t.b,p),b=Cross(t.b,t.c,p),c=Cross(t.c,t.a,p);
    float epsilon=std::abs(Cross(t.a,t.b,t.c))*.00001f;
    return (a>=-epsilon && b>=-epsilon && c>=-epsilon) || (a<=epsilon && b<=epsilon && c<=epsilon);
}
inline Point Polar(float angle,float radius) { return {std::sin(angle)*radius,std::cos(angle)*radius}; }
inline Phase Stage(int oath,int age) {
    if(oath<0 || oath>=int(Attacks.size()) || age<0) return Phase::Waiting;
    const auto& d=Attacks[oath];
    if(age<d.warning) return Phase::Warning;
    if(age<d.warning+d.strike) return Phase::Strike;
    if(age<d.warning+d.strike+d.opening) return Phase::Opening;
    return Phase::Waiting;
}
inline bool Active(int oath,int age,int pulse) {
    if(Stage(oath,age)!=Phase::Strike) return false;
    int elapsed=age-Attacks[oath].warning;
    return oath!=2 || (elapsed>=pulse*12 && elapsed<(pulse+1)*12);
}
// [TIME] | [INFO] | Warden.Pattern | The visible tessellation is also the hit footprint; target direction locks when the warning starts.
inline Pattern Make(int oath,float distance) {
    Pattern p;
    auto triangle=[&](Point a,Point b,Point c,int pulse=0) { p.triangles.push_back({a,b,c,pulse}); };
    auto edge=[&](Point a,Point b,int pulse=0) { p.edges.push_back({a,b,pulse}); };
    auto quad=[&](Point a,Point b,Point c,Point d) {
        int nx=std::max(1,int(std::ceil(std::hypot(b.x-a.x,b.z-a.z)/70)));
        int nz=std::max(1,int(std::ceil(std::hypot(d.x-a.x,d.z-a.z)/70)));
        auto at=[&](int x,int z) {
            float u=float(x)/nx,v=float(z)/nz;
            return Point{a.x*(1-u)*(1-v)+b.x*u*(1-v)+c.x*u*v+d.x*(1-u)*v,
                         a.z*(1-u)*(1-v)+b.z*u*(1-v)+c.z*u*v+d.z*(1-u)*v};
        };
        for(int x=0;x<nx;++x) for(int z=0;z<nz;++z) {
            auto aa=at(x,z),bb=at(x+1,z),cc=at(x+1,z+1),dd=at(x,z+1);
            triangle(aa,bb,cc); triangle(aa,cc,dd);
            if(z==0) edge(aa,bb); if(x==nx-1) edge(bb,cc);
            if(z==nz-1) edge(cc,dd); if(x==0) edge(dd,aa);
        }
    };
    auto circle=[&](Point center,float radius,int pulse) {
        for(int i=0;i<32;++i) {
            auto a=Polar(i*Pi/16,radius),b=Polar((i+1)*Pi/16,radius);
            a.x+=center.x; a.z+=center.z; b.x+=center.x; b.z+=center.z;
            triangle(center,a,b,pulse); edge(a,b,pulse);
        }
    };
    switch(oath) {
        case 0:
            for(int i=0;i<24;++i) {
                auto a=Polar(-1.05f+i*2.1f/24,270),b=Polar(-1.05f+(i+1)*2.1f/24,270);
                triangle({},a,b); edge(a,b);
                if(i==0) edge({},a); if(i==23) edge(b,{});
            } break;
        case 1: quad({-48,0},{48,0},{48,470},{-48,470});
            triangle({-48,470},{48,470},{0,535}); edge({-48,470},{0,535}); edge({0,535},{48,470}); break;
        case 2:
            distance=std::clamp(distance,120.f,460.f);
            circle({0,distance},82,0); circle({-115,distance+125},82,1); circle({115,distance+125},82,2); break;
        case 3:
            for(int i=0;i<48;++i) {
                auto a=Polar(i*Pi/24,90),b=Polar((i+1)*Pi/24,90),c=Polar((i+1)*Pi/24,320),d=Polar(i*Pi/24,320);
                triangle(a,b,c); triangle(a,c,d); edge(a,b); edge(c,d);
            } break;
        case 4:
            quad({-45,-300},{45,-300},{45,300},{-45,300});
            quad({-300,-45},{-45,-45},{-45,45},{-300,45});
            quad({45,-45},{300,-45},{300,45},{45,45}); break;
    }
    return p;
}
inline bool Hit(const Pattern& pattern,int oath,int age,Point point) {
    for(const auto& t:pattern.triangles) if(Active(oath,age,t.pulse) && Contains(t,point)) return true;
    return false;
}
}
