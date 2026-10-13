#include "WardenPatterns.h"
#include <cassert>
#include <iostream>
using namespace ARPG::Warden;
int main() {
    int triangles=0,probes=0;
    constexpr std::array<Point,5> danger{{{0,180},{0,300},{0,240},{0,190},{0,200}}};
    constexpr std::array<Point,5> safe{{{0,-100},{100,300},{-220,100},{0,0},{150,150}}};
    for(int oath=0;oath<5;++oath) {
        const auto& d=Attacks[oath]; auto p=Make(oath,240);
        assert(!p.triangles.empty() && p.triangles.size()<=96 && p.edges.size()<=96);
        assert(d.warning>=32 && d.opening>=42);
        for(const auto& t:p.triangles) { assert(std::abs(Cross(t.a,t.b,t.c))>1); ++triangles; }
        assert(Stage(oath,-1)==Phase::Waiting);
        assert(Stage(oath,0)==Phase::Warning);
        assert(Stage(oath,d.warning-1)==Phase::Warning);
        assert(Stage(oath,d.warning)==Phase::Strike);
        assert(Stage(oath,d.warning+d.strike)==Phase::Opening);
        assert(Stage(oath,d.warning+d.strike+d.opening)==Phase::Waiting);
        assert(Hit(p,oath,d.warning,danger[oath]));
        for(int age=0;age<d.warning+d.strike+d.opening+2;++age) {
            assert(!Hit(p,oath,age,safe[oath]));
            if(Stage(oath,age)!=Phase::Strike) assert(!Hit(p,oath,age,danger[oath]));
        }
        // [TIME] | [INFO] | WardenTest.Footprint | Independently probe every visible triangle centroid at every pulse; warnings/openings never harm.
        for(const auto& t:p.triangles) {
            Point center{(t.a.x+t.b.x+t.c.x)/3,(t.a.z+t.b.z+t.c.z)/3};
            assert(Hit(p,oath,d.warning+t.pulse*12,center));
            assert(!Hit(p,oath,d.warning-1,center));
            assert(!Hit(p,oath,d.warning+d.strike,center)); ++probes;
        }
    }
    auto fire=Make(2,240);
    for(int pulse=0;pulse<3;++pulse) {
        Point p=pulse==0?Point{0,240}:Point{pulse==1?-115.f:115.f,365};
        for(int frame=0;frame<36;++frame) assert(Hit(fire,2,Attacks[2].warning+frame,p)==(frame/12==pulse));
    }
    auto ice=Make(3,240);
    for(int degree=0;degree<360;++degree) {
        float angle=degree*Pi/180;
        assert(!Hit(ice,3,48,Polar(angle,88)));
        assert(Hit(ice,3,48,Polar(angle,100)));
        assert(Hit(ice,3,48,Polar(angle,315)));
        assert(!Hit(ice,3,48,Polar(angle,322)));
    }
    assert(Make(-1,0).triangles.empty() && Make(5,0).triangles.empty());
    assert(Stage(-1,1)==Phase::Waiting && Stage(5,1)==Phase::Waiting);
    std::cout<<"PASS: five warden patterns, "<<triangles<<" nondegenerate triangles, "<<probes<<" footprint probes, pulse timing and all safe counters\n";
}
