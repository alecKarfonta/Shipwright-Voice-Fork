#include "PeahatGeometry.h"
#include <iostream>
#include <stdexcept>
using namespace ARPG::PeahatGeometry;
void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(){try{
 for(int variant=0;variant<2;++variant){size_t triangles=0;for(int limb:Limbs){auto mesh=Peahat(limb,variant);Check(mesh.size()>150&&mesh.size()%3==0,"Eight original visible limbs have complete geometry");triangles+=mesh.size()/3;
  for(auto v:mesh){Check(v.joint==limb,"Body and leaves retain their original rigid native joints");auto p=v.surface.p,n=v.surface.normal;
   Check(std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z)&&std::abs(p.x)<5100&&std::abs(p.y)<3300&&std::abs(p.z)<3300,"Finite geometry remains within the local native envelope");Check(std::abs(n.x*n.x+n.y*n.y+n.z*n.z-1)<.001,"Normals remain unit length");if(limb!=3&&limb!=23)Check(std::abs(p.y)<200,"Spinning blades stay thin around their original collision plane");}
 }
 auto root=Peahat(WeakPoint,variant);if(variant==1)Check(root.empty(),"Larvae never gain the adult weak-point draw");else{Check(root.size()>300,"Adult exposed root is an independent authored mesh");triangles+=root.size()/3;
  for(auto v:root)for(float angle:{2.85f,3.2f,3.55f}){auto p=v.surface.p;float x=-500+std::cos(angle)*p.x*.3f+std::sin(angle)*p.z*.2f,y=p.y*.2f,z=-std::sin(angle)*p.x*.3f+std::cos(angle)*p.z*.2f;
   Check(v.joint==WeakPoint,"Root uses the existing separate native post-limb transform");Check(std::sqrt((x+1000)*(x+1000)+y*y+z*z)*.036f<=20.2f,"Visible weak root fits its original damage sphere throughout the hit wobble");}}
 Check(triangles>7000&&triangles<14000,"Bounded geometry budget");std::cout<<"Variant "<<variant<<": "<<triangles<<" triangles.\n";
 }
 for(int limb:{0,1,2,4,5,6,8,9,11,12,14,15,17,18,20,21,25})Check(Peahat(limb).empty(),"Original hidden joints stay hidden");Check(Peahat(3,-1).empty()&&Peahat(3,2).empty(),"Unknown palette rejected");
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
