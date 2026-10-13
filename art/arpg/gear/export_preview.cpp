// [TIME] | [INFO] | GearWorkshop.Export | Export the exact runtime mesh, with vertex colors split into OBJ materials, for Blender inspection.
#include "GearGeometry.h"
#include <fstream>
#include <map>
#include <string>
using namespace ARPG::GearGeometry;
int main(int argc,char**argv) {
    std::string root=argc>1?argv[1]:".";
    std::map<std::string,Mesh> models={{"Ember",Sword(0)},{"Frost",Sword(1)},{"Light",Sword(2)},
      {"Forest",Sword(-1,false,false,false,1)},{"Knight",Sword(-1)},{"Great",Sword(-1,false,false,false,3)},
      {"Root",Shield(0)},{"Wood",Shield(1)},{"KnightShield",Shield(2)},{"Mirror",Shield(3)},
      {"Star",Tunic()},{"Wind",Boots()}};
    for(auto&[name,m]:models) {
        std::ofstream obj(root+"/"+name+".obj"),mtl(root+"/"+name+".mtl");
        obj<<"mtllib "<<name<<".mtl\no "<<name<<"\n";
        for(auto v:m) { obj<<"v "<<v.p.x<<" "<<v.p.y<<" "<<v.p.z<<"\n"; }
        for(auto v:m) { obj<<"vn "<<v.normal.x<<" "<<v.normal.y<<" "<<v.normal.z<<"\n"; }
        std::map<int,Color> colors;
        for(size_t i=0;i<m.size();i+=3) {
            auto c=m[i].color;int id=(c.r<<16)|(c.g<<8)|c.b; colors[id]=c;
            obj<<"usemtl c"<<id<<"\nf ";
            for(size_t j=i+1;j<=i+3;++j) { obj<<j<<"//"<<j<<" "; } obj<<"\n";
        }
        for(auto[id,c]:colors) { mtl<<"newmtl c"<<id<<"\nKd "<<c.r/255.f<<" "<<c.g/255.f<<" "<<c.b/255.f<<"\nNs 20\n"; }
    }
}
