#include "CreatureRules.h"
#include "LanternMendRules.h"
#include "CreatureBlenderData.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <set>
using namespace ARPG::Creatures;
int main() {
    Situation ready{true,true,true,160,0};State s{};s.cooldown=0;
    assert(!Advance(Kind::Sentinel,s,ready) && s.phase==Phase::Windup);
    for(int i=0;i<Windup(Kind::Sentinel)-1;++i)assert(!Advance(Kind::Sentinel,s,ready));
    assert(Advance(Kind::Sentinel,s,ready) && s.attacks==1 && s.phase==Phase::Recover);
    for(int i=0;i<Recovery(Kind::Sentinel);++i)assert(!Advance(Kind::Sentinel,s,ready));
    assert(s.phase==Phase::Roam && s.cooldown>0);
    for(auto kind:{Kind::Sentinel,Kind::Wraith}) {
        s={};s.cooldown=0;Advance(kind,s,ready);auto saved=s;
        for(int i=0;i<1000;++i)assert(!Advance(kind,s,{false,true,true,160,0}));
        assert(s.timer==saved.timer && s.cooldown==saved.cooldown && s.phase==saved.phase);
        assert(!Advance(kind,s,{true,false,true,160,0}) && s.phase==Phase::Recover && s.attacks==0);
        s={};s.cooldown=0;Advance(kind,s,ready);assert(!Advance(kind,s,{true,true,false,160,0}) && s.attacks==0);
        s={};s.cooldown=0;Advance(kind,s,ready);assert(!Advance(kind,s,{true,true,true,160,1200}) && s.attacks==0);
        s={};s.cooldown=0;Advance(kind,s,{true,true,true,20,0});assert(s.phase==Phase::Roam);
    }
    s={};s.phase=Phase::Windup;
    assert(Receive(Kind::Sentinel,s,Strike::Blade,true,false)==.4f && s.phase==Phase::Windup);
    assert(Receive(Kind::Sentinel,s,Strike::Hammer,false,false)==1 && !s.exposed);
    assert(Receive(Kind::Sentinel,s,Strike::Blast,true,false)==1.5f && s.exposed==100 && s.phase==Phase::Stagger && s.interruptions==1);
    assert(Receive(Kind::Sentinel,s,Strike::Arrow,true,false)==1.5f);
    for(int i=0;i<100;++i)Advance(Kind::Sentinel,s,{true,false,true,600,0});
    assert(!s.exposed && Receive(Kind::Sentinel,s,Strike::Arrow,true,false)==.4f);
    s={};s.phase=Phase::Windup;Receive(Kind::Sentinel,s,Strike::Ice,true,false);assert(s.phase==Phase::Stagger && s.interruptions==1 && !s.exposed);
    s={};s.phase=Phase::Windup;Receive(Kind::Wraith,s,Strike::Boomerang,false,false);assert(s.phase==Phase::Windup && !s.exposed);
    Receive(Kind::Wraith,s,Strike::Hookshot,false,true);assert(s.phase==Phase::Stagger && s.timer==60 && s.exposed==100);
    assert(Receive(Kind::Wraith,s,Strike::Light,true,false)==3);
    s={};s.phase=Phase::Windup;assert(Receive(Kind::Wraith,s,Strike::Arrow,true,false)==1.5f && s.phase==Phase::Stagger && s.interruptions==1);
    s={};assert(Receive(Kind::Wraith,s,Strike::Blade,true,false)==.75f);
    s.phase=Phase::Dead;assert(Receive(Kind::Wraith,s,Strike::Light,true,false)==0 && !Advance(Kind::Wraith,s,ready));
    // Mending creates a finite priority target; interruptions consume attempts.
    assert(!NeedsMend(0,20,0) && !NeedsMend(20,20,0) && !NeedsMend(13,20,0));
    assert(NeedsMend(12,20,0) && !NeedsMend(12,20,2));
    assert(MendAmount(1,255,0)==6 && MendAmount(19,20,0)==1 && !MendAmount(0,20,0));
    s={};s.cooldown=0;MendState mend{};
    assert(!BeginMend(s,mend,0) && BeginMend(s,mend,1) && mend.spent==1);
    for(int i=0;i<100;++i)assert(AdvanceMend(s,mend,false,true)==MendResult::Waiting);
    assert(s.timer==0 && mend.completed==0);
    for(int i=0;i<MendWindup-1;++i)assert(AdvanceMend(s,mend,true,true)==MendResult::Waiting);
    assert(AdvanceMend(s,mend,true,true)==MendResult::Completed && mend.completed==1 && s.cooldown==160);
    mend.target=0;s={};s.cooldown=0;assert(BeginMend(s,mend,2));
    Receive(Kind::Wraith,s,Strike::Boomerang,false,true);
    assert(AdvanceMend(s,mend,true,true)==MendResult::Cancelled && s.phase==Phase::Stagger && s.timer==60);
    mend.target=0;s={};s.cooldown=0;assert(!BeginMend(s,mend,3) && mend.completed==1);
    mend={};assert(BeginMend(s,mend,4));
    assert(AdvanceMend(s,mend,true,false)==MendResult::Cancelled && !mend.completed && s.phase==Phase::Recover);
    // Quantized native data must retain each component, finite unit normals and nondegenerate triangles.
    using namespace ARPG::CreatureBlenderData;
    std::set<int> components;int triangles=0;
    for(auto group:parts) {
        components.insert(group.part);assert(group.material<11 && group.count%3==0 && group.first+group.count<=std::size(indices));
        for(unsigned i=0;i<group.count;i+=3) {
            auto a=vertices[indices[group.first+i]],b=vertices[indices[group.first+i+1]],c=vertices[indices[group.first+i+2]];
            double x=b.x-a.x,y=b.y-a.y,z=b.z-a.z,u=c.x-a.x,v=c.y-a.y,w=c.z-a.z;
            assert(std::hypot(y*w-z*v,z*u-x*w,x*v-y*u)>0);++triangles;
        }
    }
    assert(components.size()==16 && triangles==2492);
    for(auto v:vertices){double n=std::hypot(double(v.nx),double(v.ny),double(v.nz));assert(n>125 && n<129);}
    std::cout<<"Creature state, weapon counters, pause/cancel, death and 16 Blender components verified ("<<triangles<<" triangles).\n";
}
