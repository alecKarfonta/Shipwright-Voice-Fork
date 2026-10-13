#include "IronKnuckleGeometry.h"
#include <iostream>
#include <stdexcept>
#include <set>
using namespace ARPG::IronKnuckleGeometry;
void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(){try{
 for(int variant=0;variant<3;++variant){size_t armored=0,exposed=0;std::set<int> joints;
  for(int limb:Limbs){auto mesh=Knight(limb,variant);Check(mesh.size()>30&&mesh.size()%3==0,"Native visible joints retain geometry");
   for(auto v:mesh){Check(MatrixIndex(v.joint)>=0,"Every vertex has a valid native flex matrix");joints.insert(v.joint);auto p=v.surface.p,n=v.surface.normal;
    Check(std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z),"Finite local coordinates");Check(std::abs(p.x)<6000&&std::abs(p.y)<4000&&std::abs(p.z)<6000,"Body and axe stay bounded");Check(std::abs(n.x*n.x+n.y*n.y+n.z*n.z-1)<.001f,"Unit normals");
    if(limb==26||limb==27)Check(v.joint==limb,"Detached chest plates are rigid and never depend on another actor's flex palette");}
   if(limb!=28&&limb!=29)armored+=mesh.size()/3;
   if(limb!=26&&limb!=27)exposed+=mesh.size()/3;
  }
  Check(joints.size()==24&&armored<16000&&exposed<16000,"Complete palette within per-state triangle budget");
  std::cout<<"Palette "<<variant<<": "<<armored<<" armored / "<<exposed<<" exposed triangles.\n";
 }
 Check(Knight(1,0).empty()&&Knight(16,0).empty()&&Knight(26,3).empty(),"Hidden joints and invalid palette stay empty");
 Check(MatrixIndex(2)==0&&MatrixIndex(29)==23,"Native flex matrix order");
 Check(Palette[0][0].r!=Palette[1][0].r&&Palette[1][0].r!=Palette[2][0].r,"Three distinct native armor colors");
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
