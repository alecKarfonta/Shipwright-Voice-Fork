#include "HandmasterGeometry.h"
#include <iostream>
#include <stdexcept>
#include <set>
using namespace ARPG::HandmasterGeometry;
void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(){try{
 for(int variant=0;variant<2;++variant){size_t count=0;std::set<int> joints,connections;
  for(int limb=1;limb<=25;++limb){auto mesh=Handmaster(limb,variant);bool visible=MatrixIndex(limb,variant)>=0||limb==25;
   Check(visible?!mesh.empty()&&mesh.size()%3==0:mesh.empty(),"Exactly fourteen native flex limbs and the separately drawn finger have geometry");
   for(auto v:mesh){Check(limb==25?v.joint==25:MatrixIndex(v.joint,variant)>=0,"The separate scaled digit cannot index the fourteen-entry flex palette");joints.insert(v.joint);if(v.joint!=limb)connections.insert(limb);auto p=v.surface.p,n=v.surface.normal;
    Check(std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z)&&std::abs(p.x)<3200&&std::abs(p.y)<2300&&std::abs(p.z)<2300,"Finite native-local anatomy stays inside the hand envelope");
    Check(std::abs(n.x*n.x+n.y*n.y+n.z*n.z-1)<.001f,"Quantized normals remain unit length");}
   count+=mesh.size()/3;
  }
  Check(joints.size()==15&&connections.size()==13,"All fourteen animated limbs, separate digit, and thirteen connected joints remain represented");
  Check(count>4000&&count<8000,"Bounded geometry budget accommodates the three-hand split");
  Check(MatrixIndex(2,variant)==0&&MatrixIndex(24,variant)==13&&MatrixIndex(25,variant)==-1,"Flex palette follows the original traversal order");
  std::cout<<"Variant "<<variant<<": "<<count<<" triangles; 14 native matrices; 13 joint connections; separate scaled digit.\n";
 }
 Check(Handmaster(2,-1).empty()&&Handmaster(2,2).empty()&&MatrixIndex(2,2)==-1,"Invalid variants cannot address geometry or matrices");
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
