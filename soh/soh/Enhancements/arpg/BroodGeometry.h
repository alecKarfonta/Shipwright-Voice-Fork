#pragma once
#include <algorithm>
#include <cmath>
#include <vector>
#include <array>
#include <cstdint>
namespace ARPG::Brood::Geometry {
struct Point {float x,y,z;};
struct Triangle {Point a,b,c;uint8_t r,g,bColor;};
inline std::vector<Triangle> Shell(float opening,float pulse,bool delayed) {
    std::vector<Triangle> mesh;mesh.reserve(400);
    auto tri=[&](Point a,Point b,Point c,int r,int g,int blue){mesh.push_back({a,b,c,uint8_t(r),uint8_t(g),uint8_t(blue)});};
    auto ribbon=[&](Point a,Point b,float width,int r,int g,int blue){
        float dx=b.x-a.x,dz=b.z-a.z,len=std::max(.001f,std::hypot(dx,dz));
        Point c{b.x+dz/len*width,b.y,b.z-dx/len*width},d{a.x+dz/len*width,a.y,a.z-dx/len*width};
        tri(a,b,c,r,g,blue);tri(a,c,d,r,g,blue);
    };
    // Eight separate pointed chitin plates open outward; the inner amber
    // spindle and crossed silk bindings stay visible through the seams.
    constexpr float tau=6.2831853f;
    for(int plate=0;plate<8;++plate) {
        float yaw=tau*plate/8;
        auto point=[&](float side,float radius,float height){
            float out=radius+opening*height*.85f;
            return Point{std::sin(yaw)*out+std::cos(yaw)*side,height*(1-opening*.6f),std::cos(yaw)*out-std::sin(yaw)*side};
        };
        Point left[5],right[5];float y[]={0,8,24,38,49},rad[]={8,20,23,15,2},wide[]={2,6,7,4,.25f};
        for(int i=0;i<5;++i){left[i]=point(-wide[i],rad[i],y[i]);right[i]=point(wide[i],rad[i],y[i]);}
        for(int i=0;i<4;++i){int light=plate%2*12+i*8;tri(left[i],left[i+1],right[i+1],75+light,83+light,50+light/2);tri(left[i],right[i+1],right[i],75+light,83+light,50+light/2);}
        // A raised pale ridge gives each shell blade a sharp readable edge.
        for(int i=0;i<4;++i){auto a=point(-.7f,rad[i]+.7f,y[i]+.3f),b=point(-.7f,rad[i+1]+.7f,y[i+1]+.3f);
            auto c=point(.7f,rad[i+1]+.7f,y[i+1]+.3f),d=point(.7f,rad[i]+.7f,y[i]+.3f);tri(a,b,c,170,163,112);tri(a,c,d,170,163,112);}
    }
    for(int i=0;i<12;++i){float a=tau*i/12,b=tau*(i+1)/12;Point p{std::sin(a)*12,23,std::cos(a)*12},q{std::sin(b)*12,23,std::cos(b)*12};
        int red=delayed?90:190+int(pulse*60),green=delayed?205:110+int(pulse*70),blue=delayed?245:35;
        tri({0,5,0},q,p,red,green,blue);tri(p,q,{0,43,0},red,green,blue);
    }
    if(opening<.5f)for(int strand=0;strand<2;++strand)for(int i=0;i<48;++i){
        auto p=[&](int j){float t=j/48.f,y=4+t*40,r=11+std::sin(t*3.14159f)*14,a=t*tau*2*(strand?1:-1)+strand;
            return Point{std::sin(a)*r,y,std::cos(a)*r};};
        ribbon(p(i),p(i+1),.65f,211,211,166);
    }
    return mesh;
}
}
