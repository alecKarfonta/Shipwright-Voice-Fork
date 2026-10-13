#include "ArmosGeometry.h"
#include <iostream>
#include <stdexcept>
using namespace ARPG::ArmosGeometry;
void Check(bool ok,const char* label){if(!ok)throw std::runtime_error(label);}
int main(){try{
 size_t triangles=0;
 for(int limb:Limbs){auto mesh=Armos(limb);Check(mesh.size()>100&&mesh.size()%3==0,"Each original visible limb has complete geometry");triangles+=mesh.size()/3;
  for(auto v:mesh){Check(v.joint==limb,"Stone parts remain rigid on their own original joints");auto p=v.surface.p,n=v.surface.normal;Check(std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z)&&std::abs(p.x)<6500&&std::abs(p.y)<3000&&std::abs(p.z)<2600,"Finite mesh within original local envelope");Check(std::abs(n.x*n.x+n.y*n.y+n.z*n.z-1)<.001,"Unit normals");}
 }
 Check(triangles>3000&&triangles<12000,"Bounded actor geometry budget");
 for(int material=0;material<7;++material){auto a=Blend(material,0),b=Blend(material,255);Check(a.r==Palette[0][material].r&&a.g==Palette[0][material].g&&a.b==Palette[0][material].b,"Dormant palette endpoint");Check(b.r==Palette[1][material].r&&b.g==Palette[1][material].g&&b.b==Palette[1][material].b,"Awake palette endpoint");
  auto previous=a;for(int wake=1;wake<256;++wake){auto c=Blend(material,wake);Check(std::abs(int(c.r)-previous.r)<=1&&std::abs(int(c.g)-previous.g)<=1&&std::abs(int(c.b)-previous.b)<=1,"Continuous per-actor awakening palette");previous=c;}}
 for(int limb:{0,1,2,3,5,6,8,9,11,12,14})Check(Armos(limb).empty(),"Hidden native limbs remain hidden");Check(Armos(4,-1).empty()&&Armos(4,2).empty(),"Invalid palette rejected");
 std::cout<<triangles<<" triangles; four rigid native limbs; all 256 awakening steps passed.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
