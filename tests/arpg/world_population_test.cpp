#include "WorldPopulationRules.h"
#include <cassert>
#include <iostream>
using namespace ARPG::Enemies;
int main() {
    PatrolPlanningBudget planning;
    for (uint32_t frame:{0u,1u,200u,0xffffffffu,0u}) {
        for (int actor=0;actor<96;++actor) { assert(planning.Take(frame)==(actor<8)); }
    }

    assert(WildlifeBudget(0,100)==6);
    assert(WildlifeBudget(60000000,100)==10);
    assert(WildlifeBudget(146978780,100)==24);
    assert(WildlifeBudget(146978780,3)==3);
    assert(WildlifeBudget(146978780,-1)==0);
    assert(!WildlifeFor(0x58,true).fish);
    assert(WildlifeFor(0x51).birds && WildlifeFor(0x51).butterflies);
    assert(WildlifeFor(0x55).birds && WildlifeFor(0x55).butterflies);
    assert(WildlifeFor(0x57).fish && WildlifeFor(0x58).fish);
    assert(!WildlifeFor(0x58).butterflies && !WildlifeFor(0x58).birds);
    for (int scene=0;scene<0x6E;++scene) {
        auto habitat=WildlifeFor(scene);
        if (scene!=0x51 && scene!=0x52 && scene!=0x53 && scene!=0x54 && scene!=0x55 && scene!=0x56 &&
            scene!=0x57 && scene!=0x58 && scene!=0x59 && scene!=0x5B && scene!=0x5F && scene!=0x63) {
            assert(!habitat.birds && !habitat.butterflies && !habitat.fish);
        }
    }
    assert(AreaPopulationBudget(0,100,64,100)==0);
    assert(AreaPopulationBudget(3000000,100,64,100)==2);
    assert(AreaPopulationBudget(12000000,100,64,100)==8);
    assert(AreaPopulationBudget(60000000,100,64,100)==40);
    assert(AreaPopulationBudget(60000000,50,64,100)==20);
    assert(AreaPopulationBudget(60000000,150,64,100)==60);
    assert(AreaPopulationBudget(300000000,100,64,100)==64);
    assert(AreaPopulationBudget(60000000,100,64,10)==10);
    assert(AreaPopulationBudget(60000000,100,64,1)==0);

    for (bool adult:{false,true}) {
        for (int scene:{0x51,0x54,0x56,0x57,0x59,0x5A,0x5B,0x5C,0x5E,0x60,0x61,0x64}) {
            assert(RegionFor(scene,adult).population==Population::Wilderness);

        }
        for (int scene:{0x1B,0x1C,0x1D,0x1E,0x1F,0x20,0x21,0x22,0x23,0x24,0x25,0x52,0x53,0x55,0x58,0x5D,0x5F,0x62,0x63}) {
            assert(RegionFor(scene,adult).population==Population::Settlement);

            assert(RegionFor(scene,adult).maxPack==0); // no indiscriminate town pack rolls
        }
        for (int scene:{0,1,2,3,4,5,6,7,8,9,10,11,13,0x3E,0x3F,0x41}) {
            assert(RegionFor(scene,adult).population==Population::Dungeon);
            assert(ActivityFor(scene,adult).watchLimit==0); // no automatic puzzle-solving allies
        }
        for (int scene:{0xC,0xE,0xF,0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1A,0x2C,0x2D,0x2E,0x2F,0x34,0x43,0x44,0x45,0x46,0x47,0x49,0x4A,0x4B,0x4F}) {
            assert(ActivityFor(scene,adult).watchLimit==0);
            assert(!ActivityFor(scene,adult).borderConflict);
            assert(RegionFor(scene,adult).population!=Population::Wilderness);
        }
        for(int scene=0;scene<0x6E;++scene) {
            auto plan=ActivityFor(scene,adult);
            bool humanPatrol=scene==0x51 || scene==0x63 || (!adult &&
                (scene==0x5F || scene==0x1B || scene==0x1C || scene==0x1E || scene==0x1F || scene==0x20 || scene==0x21));
            assert((plan.watchLimit>0)==humanPatrol);
            assert(plan.watchLimit>=0 && plan.watchLimit<=4);
            if(plan.borderConflict) {
                auto kind=RegionFor(scene,adult).population;
                assert(kind==Population::Wilderness || kind==Population::Settlement);
            }
        }
    }
    for (bool adult:{false,true}) for (int scene:{0x55,0x58,0x62,0x5D,0x5B,0x56,0x5C,0x52}) {
        assert(ActivityFor(scene,adult).watchLimit==0);
        assert(!ActivityFor(scene,adult).borderConflict);
    }
    assert(ActivityFor(0x5F,false).borderConflict && ActivityFor(0x63,true).borderConflict);
    std::cout<<"World population policies cover wilderness, settlements, combat rooms and protected scenes in both ages\n";
}
