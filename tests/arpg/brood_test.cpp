#include "BroodRules.h"
#include "BroodGeometry.h"
#include <cassert>
#include <iostream>
using namespace ARPG::Brood;
int main(){
    State s;
    for(int i=0;i<100;++i)assert(!Advance(s,false));assert(s.age==0 && s.phase==Phase::Falling);
    assert(!Delay(s));for(int i=0;i<FallTicks;++i)assert(!Advance(s,true));
    assert(s.phase==Phase::Incubating && s.age==0);
    assert(Delay(s) && !Delay(s));for(int i=0;i<DelayTicks;++i)assert(!Advance(s,true));assert(s.age==0);
    for(int i=0;i<HatchTicks-1;++i)assert(!Advance(s,true));assert(Advance(s,true) && s.phase==Phase::Hatched);
    for(int i=0;i<FadeTicks;++i)assert(!Advance(s,true));assert(Retired(s));
    for(auto phase:{Phase::Falling,Phase::Incubating}){s={phase,12,20,true};Break(s);assert(s.phase==Phase::Broken && s.age==0 && !s.delayed);for(int i=0;i<100;++i)assert(!Advance(s,true));}
    for(float opening:{0.f,.25f,.5f,.75f,1.f})for(bool delayed:{false,true}){
        auto mesh=Geometry::Shell(opening,.8f,delayed);assert(mesh.size()>100 && mesh.size()<512);
        for(auto t:mesh){double x=t.b.x-t.a.x,y=t.b.y-t.a.y,z=t.b.z-t.a.z,u=t.c.x-t.a.x,v=t.c.y-t.a.y,w=t.c.z-t.a.z;
            assert(std::hypot(y*w-z*v,z*u-x*w,x*v-y*u)>.0001);
            for(auto p:{t.a,t.b,t.c})assert(std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z) && std::abs(p.x)<100 && std::abs(p.z)<100 && p.y>=0 && p.y<60);
        }
    }
    std::cout<<"Brood lifecycle, finite utility delay, pause, cancellation and articulated cocoon geometry passed.\n";
}
