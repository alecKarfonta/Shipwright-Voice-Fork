#include "ExplorationRules.h"
#include "ARPGSaveCodec.h"
#include "EnemyRules.h"
#include <cassert>
#include <set>
int main() {
    using namespace ARPG;
    for(int d=0;d<4;++d)assert(SkillRules::ControllerSlot(d,false)==d);
    assert(SkillRules::ControllerSlot(3,true)==4);
    assert(SkillRules::ControllerSlot(1,true)==5);
    assert(SkillRules::ControllerSlot(0,true)==0);
    assert(SkillRules::ControllerSlot(2,true)==2);
    assert(SkillRules::ControllerSlot(-1,true)==-1);
    std::set<int> ids;
    for(int scene:Exploration::Scenes)for(bool adult:{false,true}) {
        int first=Exploration::FirstId(scene,adult);
        for(int n=0;n<3;++n) { assert(ids.insert(first+n).second);assert(Exploration::Skull(first+n)==(n==2)); }
    }
    assert(ids.size()==ARPG_SECRET_COUNT);
    assert(Exploration::FirstId(0x3e,false)==-1); // shared grotto scene is intentionally excluded
    ARPGSaveData save{};save.seed=42;
    int first=Exploration::FirstId(0x55,false);
    save.secrets[first]={125.f,14.f,-800.f,0,1,0};
    save.secrets[first+2]={500.f,14.f,-800.f,0,1,0};
    assert(Exploration::Claim(save,first));assert(!Exploration::Claim(save,first));
    assert(!Exploration::Claim(save,-1));assert(!Exploration::Claim(save,ARPG_SECRET_COUNT));
    assert(!Exploration::Claim(save,first+1));
    assert(Exploration::Claim(save,first+2));
    assert(Exploration::SkullCount(save)==1);
    assert(Exploration::RewardTier(4,save)==1);
    save.skullRewardTier=1;
    auto decoded=SaveCodec::Decode(SaveCodec::Encode(save),0);
    assert(decoded.secrets[first].claimed && decoded.secrets[first].x==125.f);
    assert(decoded.secrets[first+2].claimed && decoded.skullRewardTier==1);
    assert(!Exploration::Claim(decoded,first)); // reload cannot farm a cache
    auto legacy=SaveCodec::Decode({{"seed",42}},0);
    assert(Exploration::SkullCount(legacy)==0 && legacy.skullRewardTier==0);
    auto malformed=SaveCodec::Encode(save);malformed["secrets"][first]["x"]="bad";
    decoded=SaveCodec::Decode(malformed,0);
    assert(!decoded.secrets[first].placed && decoded.secrets[first].claimed); // keep receipt, reject unsafe position
    for(int damage=1;damage<=128;++damage)for(int armor=0;armor<=100;++armor) {
        int out=Rules::ReducedDamage(damage,1.f,armor);
        assert(out>0 && out<=damage && out*4>=damage*3);
    }
    assert(Rules::ReducedDamage(8,1.f,5)==8);
    assert(Rules::ReducedDamage(16,1.f,25)==12);
    assert(Enemies::GearChance(18,ARPG_TIER_NORMAL,1,0)==3);
    assert(Enemies::GearChance(18,ARPG_TIER_CHAMPION,1,0)==9);
    assert(Enemies::GearChance(18,ARPG_TIER_ELITE,1,0)==25);
    assert(Enemies::GearChance(18,ARPG_TIER_NORMAL,50,100)<=11);
}
