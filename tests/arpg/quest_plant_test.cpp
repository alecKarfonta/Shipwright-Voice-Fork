#include "QuestPlantGeometry.h"
#include <cassert>
#include <iostream>
#include <limits>

using namespace ARPG::Quests::Plants;
int main() {
    for(bool restored:{false,true})for(int variant=0;variant<3;++variant) {
        auto plant=Build(restored,variant);
        float highest=0;
        for(auto* mesh:{&plant.bark,&plant.leaves}) {
            assert(!mesh->empty() && mesh->size()%3==0);
            for(const auto& v:*mesh) {
                assert(std::isfinite(v.p.x) && std::isfinite(v.p.y) && std::isfinite(v.p.z));
                assert(std::abs(v.p.x)<45 && std::abs(v.p.z)<45 && v.p.y>-.5f && v.p.y<100);
                assert(std::abs(v.p.x*16)<32767 && std::abs(v.p.y*16)<32767);
                assert(std::abs(v.u*1024)<32767 && std::abs(v.v*1024)<32767);
                float length=v.normal.x*v.normal.x+v.normal.y*v.normal.y+v.normal.z*v.normal.z;
                assert(std::abs(length-1)<.001f);
                highest=std::max(highest,v.p.y);
                if(mesh==&plant.leaves)assert(v.u>=0 && v.u<=1 && v.v>=0 && v.v<=1);
            }
            for(size_t i=0;i<mesh->size();i+=3) {
                // Thin twigs must remain nondegenerate after conversion to signed N64 coordinates.
                auto quantize=[](Point p)->Point{return {std::round(p.x*16),std::round(p.y*16),std::round(p.z*16)};};
                auto a=quantize((*mesh)[i].p),b=quantize((*mesh)[i+1].p),c=quantize((*mesh)[i+2].p);
                auto n=Cross(Add(b,Mul(a,-1)),Add(c,Mul(a,-1)));
                assert(n.x*n.x+n.y*n.y+n.z*n.z>0);
            }
        }
        assert(restored?highest>70:highest<35);
        assert(plant.bark.size()+plant.leaves.size()<1300);
    }
    assert(SiteSeed(-91.3f,112.9f)==SiteSeed(-91.3f,112.9f));
    assert(SiteSeed(-91.3f,112.9f)!=SiteSeed(91.3f,112.9f));
    std::cout<<"Six plant meshes: valid bounds, normals, UVs and quantized triangles; stable site variation.\n";
}
