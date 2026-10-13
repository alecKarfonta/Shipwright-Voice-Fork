#include "SkillDetails.h"
#include "AuraGeometry.h"
#include "ARPGSaveCodec.h"
#include <cassert>
#include <iostream>
#include <tuple>
#include <set>
using namespace ARPG;
int main() {
    std::set<int> ids;
    std::set<std::tuple<int,int,int>> cells;
    for(const auto& node:SkillTree::nodes) {
        assert(node.skill>=0 && node.skill<ARPG_SKILL_COUNT_ && ids.insert(node.skill).second);
        assert(node.column>=0 && node.column<4 && node.row>=0 && node.row<5);
        assert(cells.emplace(ARPG_SKILLS[node.skill].tree,node.column,node.row).second);
        for(auto parent:node.parents) {
            if(parent.skill<0) { continue; }
            const auto* from=SkillTree::Find(parent.skill);
            assert(from && ARPG_SKILLS[parent.skill].tree==ARPG_SKILLS[node.skill].tree);
            assert(from->row<node.row && parent.rank>0 && parent.rank<=ARPG_SKILLS[parent.skill].maxRank);
        }
        for(auto direction:{std::pair{1,0},std::pair{-1,0},std::pair{0,1},std::pair{0,-1}}) {
            int next=SkillTree::Navigate(node.skill,direction.first,direction.second);
            assert(SkillTree::Find(next) && ARPG_SKILLS[next].tree==ARPG_SKILLS[node.skill].tree);
        }
    }
    assert(ids.size()==ARPG_SKILL_COUNT_);
    ARPGSaveData save{}; save.level=50; SkillRules::Initialize(save);
    int passive=49,techniques=26;
    for(int pass=0;pass<20;++pass) {
        for(int id=0;id<ARPG_SKILL_COUNT_;++id) {
            int desired=id==ARPG_SKILL_MAGIC_FLAME_BURST?2:1;
            if(save.skillRanks[id]<desired && SkillRules::CanLearn(save,id)) {
                int& points=ARPG_SKILLS[id].passive?passive:techniques;
                assert(points>0);--points;++save.skillRanks[id];
            }
        }
    }
    for(int id:ids) { assert(save.skillRanks[id]>0); }
    save.skillRanks[ARPG_SKILL_MAGIC_METEOR]=0;save.skillRanks[ARPG_SKILL_MAGIC_FLAME_BURST]=1;
    assert(!SkillRules::CanLearn(save,ARPG_SKILL_MAGIC_METEOR));
    save.skillRanks[ARPG_SKILL_MAGIC_FLAME_BURST]=2;
    assert(SkillRules::CanLearn(save,ARPG_SKILL_MAGIC_METEOR));
    save.skillRanks[ARPG_SKILL_MAGIC_FLAME_BURST]=0;save.skillRanks[ARPG_SKILL_MAGIC_METEOR]=1;
    assert(SkillRules::CanLearn(save,ARPG_SKILL_MAGIC_METEOR));
    auto restored=SaveCodec::Decode(SaveCodec::Encode(save),1);
    assert(restored.skillRanks[ARPG_SKILL_MAGIC_METEOR]==1 && restored.skillRanks[ARPG_SKILL_MAGIC_FLAME_BURST]==0);
    ARPGSaveData bonus{};
    bonus.skillRanks[ARPG_SKILL_SWORD_CLEAVE]=3;bonus.skillRanks[ARPG_SKILL_SWORD_LUNGE]=2;
    assert(SkillTree::SynergyPercent(bonus,ARPG_SKILL_SWORD_CROSSCUT)==23);
    bonus.skillRanks[ARPG_SKILL_SWORD_LUNGE]=255;
    assert(SkillTree::SynergyPercent(bonus,ARPG_SKILL_SWORD_CROSSCUT)==27);
    assert(SkillTree::SynergyPercent(bonus,ARPG_SKILL_MAGIC_CHAIN)==0);
    for(int id:ids) {
        if(!SkillRules::Castable(id)) { continue; }
        for(int rune=0;rune<3;++rune) {
            save.skillRanks[id]=1;save.skillRunes[id]=rune;
            auto before=SaveCodec::Encode(save);
            auto a=SkillDetails::Inspect(save,id),b=SkillDetails::Inspect(save,id,true);
            assert(SaveCodec::Encode(save)==before);
            assert(a.cooldown>=b.cooldown && b.cooldown>=20);
            assert(b.duration>=a.duration && b.healing>=a.healing && b.power>=a.power);
            assert(a.power==0 || a.strikes>0);
            if(id==ARPG_SKILL_SWORD_BLADESTORM) { assert(a.power>0); }
            if(id==ARPG_SKILL_MAGIC_RECALL) { assert(a.cost==(rune==2?24:12)); }
        }
    }
    assert(SkillRules::BuffDuration(save,-1)==0 && SkillRules::Healing(save,999)==0);
    std::set<int> shapes;
    int meshes=0;
    for(int id:ids) {
        if(!AuraGeometry::Supported(id)) { continue; }
        int signature=0;
        for(int rune=0;rune<3;++rune) {
            int duration=id==ARPG_SKILL_MAGIC_INFUSE?840:id==ARPG_SKILL_MAGIC_HOLY_AURA?300:
                         id==ARPG_SKILL_GUARD_WARCRY?160:id==ARPG_SKILL_GUARD_RIPOSTE?100:
                         id==ARPG_SKILL_GUARD_SECOND_WIND?240:id==ARPG_SKILL_GUARD_DODGE?26:32;
            for(int age=0;age<duration;++age) {
                auto mesh=AuraGeometry::Build(id,rune,age,duration-age,age%3);
                assert(mesh.count>0 && mesh.count<int(mesh.vertices.size()) && mesh.count%3==0);
                signature=mesh.count;
                for(int i=0;i<mesh.count;i+=3) {
                    auto a=mesh.vertices[i].p,b=mesh.vertices[i+1].p,c=mesh.vertices[i+2].p;
                    for(auto p:{a,b,c}) { assert(std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z) && std::abs(p.x)<4000 && std::abs(p.y)<4000 && std::abs(p.z)<4000); }
                    float x=(b.y-a.y)*(c.z-a.z)-(b.z-a.z)*(c.y-a.y);
                    float y=(b.z-a.z)*(c.x-a.x)-(b.x-a.x)*(c.z-a.z);
                    float z=(b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x);
                    assert(x*x+y*y+z*z>1e-9f);
                }
                ++meshes;
            }
        }
        shapes.insert(signature);
        assert(AuraGeometry::Build(id,0,20,0).count==0);
    }
    assert(shapes.size()>=6);
    std::cout<<"All 37 skill nodes reachable within earned points; prerequisites, legacy saves, synergies, rank previews and "<<meshes<<" bounded aura meshes verified.\n";
}
