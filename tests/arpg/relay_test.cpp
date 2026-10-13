#include "RelayRules.h"
#include "RelayGeometry.h"
#include "ARPGSaveCodec.h"
#include "WaystoneState.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <set>
using namespace ARPG;
using namespace ARPG::ExpeditionRules;
int main() {
    Relay::Input input{}; input.active=true; input.lanterns={Relay::Point{240,0,0},{-120,0,208},{-120,0,-208}};
    for(uint32_t seed=1;seed<=10000;++seed) {
        ARPGSaveData save{}; auto& r=save.expedition; assert(Start(r,seed,seed%4,1+seed%3,20));
        std::set<int> objectives;int relayRoute=-1;
        for(int i=0;i<3;++i) { objectives.insert(RouteObjective(r,i)); if(RouteObjective(r,i)==SpiritRelay) relayRoute=i; }
        assert(objectives.size()==3 && relayRoute>=0 && ChooseRoute(r,relayRoute));
        assert(WaveSize(r)==4 && BeginWave(r,4) && Protected(r,0) && !Defeat(r,0));
        Relay::State state{}; input.player={}; input.damaged=false;input.canInteract=true;
        assert(!Relay::Release(r,state,1,{}) && !Relay::Release(r,state,0,{}));
        for(int guard:{3,1,2}) {
            int site=guard%3;
            input.player={float(guard*70),0,0};
            assert(Defeat(r,guard) && !Defeat(r,guard));
            assert(Relay::Release(r,state,guard,input.player) && !Relay::Release(r,state,guard,input.player));
            assert(Relay::Tick(r,state,input)==Relay::Event::PickedUp && state.carried==guard-1);
            assert(!(r.sealMask&(1<<site)));
            input.player=input.lanterns[site];
            assert(Relay::Tick(r,state,input)==Relay::Event::Delivered && state.carried==-1 && (r.sealMask&(1<<site)));
            assert(Relay::Tick(r,state,input)==Relay::Event::None);
            auto view=Waystone::FromExpedition(r);assert(Waystone::LampTone(view,site)==5);
            assert(r.phase==Combat && r.clears==0);
        }
        assert(!Protected(r,0) && r.sealMask==7 && Defeat(r,0) && !Defeat(r,0));
        assert(r.phase==Reward && r.clears==1);
        auto restored=SaveCodec::Decode(SaveCodec::Encode(save),1);
        assert(restored.expedition.phase==Reward && restored.expedition.objective==SpiritRelay);
        assert(Waystone::FromExpedition(restored.expedition).seals==7);
        assert(Claim(restored,seed%3));auto once=restored;assert(!Claim(restored,0) && !memcmp(&restored,&once,sizeof(once)));
    }
    ARPGSaveData save{};auto& r=save.expedition;Start(r,42,0,1,20);r.objective=SpiritRelay;r.phase=Queued;BeginWave(r,4);
    Relay::State state{};Defeat(r,1);Relay::Release(r,state,1,{40,0,0});
    input.player={40,0,0};input.canInteract=false;
    assert(Relay::Tick(r,state,input)==Relay::Event::None && state.carried==-1);
    input.canInteract=true;assert(Relay::Tick(r,state,input)==Relay::Event::PickedUp);
    auto before=state;input.active=false;input.damaged=true;
    assert(Relay::Tick(r,state,input)==Relay::Event::None && !memcmp(&before,&state,sizeof(state)));
    input.active=true;input.safeDrop={60,0,0};input.player=input.lanterns[0];
    assert(Relay::Tick(r,state,input)==Relay::Event::Dropped && state.carried==-1 && r.sealMask==0);
    assert(state.spirits[0].position.x==60 && state.pickupLock==Relay::PickupDelay);
    input.player=input.safeDrop;input.damaged=false;
    for(int t=0;t<Relay::PickupDelay-1;++t) assert(Relay::Tick(r,state,input)==Relay::Event::None);
    assert(Relay::Tick(r,state,input)==Relay::Event::PickedUp);
    Defeat(r,2);Relay::Release(r,state,2,input.player);
    assert(Relay::Tick(r,state,input)==Relay::Event::None && state.carried==0 && state.spirits[1].state==Relay::SpiritState::Loose);
    input.player=input.lanterns[2];assert(Relay::Tick(r,state,input)==Relay::Event::Delivered);
    auto recovered=SaveCodec::Decode(SaveCodec::Encode(save),1);
    assert(recovered.expedition.phase==Route && !recovered.expedition.sealMask && !recovered.expedition.defeatedMask && recovered.expedition.objective==SpiritRelay);
    input.player={500,0,0};input.refuge={-50,0,0};state.spirits[1].unattended=Relay::ReturnTicks-1;
    assert(Relay::Tick(r,state,input)==Relay::Event::Returned && state.spirits[1].position.x==-50);
    auto invalid=state;r.phase=Reward;assert(Relay::Tick(r,state,input)==Relay::Event::None && !memcmp(&state,&invalid,sizeof(state)));
    auto legacy=SaveCodec::Encode(save);legacy["expedition"].erase("objective");assert(SaveCodec::Decode(legacy,1).expedition.objective==Gauntlet);
    legacy["expedition"]["objective"]=255;assert(SaveCodec::Decode(legacy,1).expedition.objective==Gauntlet);
    assert(!Relay::Near({0,40,0},{},Relay::PickupReach));
    size_t triangles=0;
    for(int part=0;part<3;++part)for(int tone=0;tone<3;++tone) {
        auto mesh=RelayGeometry::Build(part,tone);assert(mesh.size()>0 && mesh.size()%3==0);
        for(const auto& v:mesh) {
            assert(std::isfinite(v.p.x+v.p.y+v.p.z));
            float n=v.normal.x*v.normal.x+v.normal.y*v.normal.y+v.normal.z*v.normal.z;assert(std::abs(n-1)<.001f);
            assert(std::abs(v.p.x)<12 && std::abs(v.p.z)<12 && v.p.y>-6 && v.p.y<54);
        }
        for(size_t i=0;i<mesh.size();i+=3) {
            auto a=mesh[i].p,b=mesh[i+1].p,c=mesh[i+2].p;
            auto cross=GearGeometry::Cross({b.x-a.x,b.y-a.y,b.z-a.z},{c.x-a.x,c.y-a.y,c.z-a.z});
            assert(cross.x*cross.x+cross.y*cross.y+cross.z*cross.z>1e-9f);++triangles;
        }
    }
    assert(RelayGeometry::Build(3).empty() && RelayGeometry::Build(0,3).empty());
    std::cout<<"10,000 relay seeds, three distinct objectives, guard release, delivery, pause, damage/drop/recovery, one-spirit capacity, delayed return, save recovery and atomic rewards; "<<triangles<<" rendered triangles checked\n";
}
