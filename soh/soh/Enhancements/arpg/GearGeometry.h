#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace ARPG::GearGeometry {
struct Point { float x, y, z; };
enum class Surface : uint8_t { Inherit, Metal, Wood, Leather, Cloth, Stone, Bone, Fur, Scales, Wing, Plant, Shell, Skin, Gem, Flat, Glow };
struct Color { uint8_t r, g, b; Surface surface=Surface::Inherit; };
constexpr Color Paint(Color c,Surface surface) { c.surface=surface;return c; }
constexpr Surface ResolveSurface(Color c,Surface base) { return c.surface==Surface::Inherit?base:c.surface; }
constexpr uint32_t MaterialKey(Color c) { return uint32_t(c.surface)<<24|uint32_t(c.r)<<16|uint32_t(c.g)<<8|c.b; }
struct Vertex { Point p; Color color; Point normal; };
using Mesh = std::vector<Vertex>;
inline void FinishMaterials(Mesh& mesh,Surface base) {
    for(auto& v:mesh)v.color.surface=ResolveSurface(v.color,base);
}
constexpr float pi = 3.14159265359f;
constexpr Color bronze{173,111,48,Surface::Metal}, gold{222,179,91,Surface::Metal}, cream{232,216,165}, jade{67,153,114},
                darkJade{24,76,55}, oak{83,53,29,Surface::Wood}, leather{87,46,31,Surface::Leather}, indigo{48,47,120}, cyan{117,225,235},
                silver{182,201,215,Surface::Metal}, steel{78,104,133,Surface::Metal};
inline Color Shade(Color c,float f) { return {uint8_t(std::clamp(c.r*f,0.f,255.f)),uint8_t(std::clamp(c.g*f,0.f,255.f)),uint8_t(std::clamp(c.b*f,0.f,255.f)),c.surface}; }
inline Point Add(Point a,Point b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
inline Point Mul(Point a,float f) { return {a.x*f,a.y*f,a.z*f}; }
inline Point Cross(Point a,Point b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
inline Point Unit(Point p) { float n=std::sqrt(p.x*p.x+p.y*p.y+p.z*p.z); return n>.00001f?Mul(p,1/n):Point{1,0,0}; }
inline void Tri(Mesh& m,Point a,Point b,Point c,Color ink) {
    auto n=Unit(Cross(Add(b,Mul(a,-1)),Add(c,Mul(a,-1))));
    m.insert(m.end(),{{a,ink,n},{b,ink,n},{c,ink,n}});
}
inline void Quad(Mesh& m,Point a,Point b,Point c,Point d,Color ink) { Tri(m,a,b,c,ink); Tri(m,a,c,d,ink); }
inline void SmoothTri(Mesh& m,Point a,Point b,Point c,Point na,Point nb,Point nc,Color ink) {
    m.insert(m.end(),{{a,ink,Unit(na)},{b,ink,Unit(nb)},{c,ink,Unit(nc)}});
}
// [TIME] | [INFO] | GearGeometry.Surface | Curved surfaces carry analytic normals; metal bevels deliberately keep sharp edges.
inline void Tube(Mesh& m,Point a,Point b,float radius,Color ink,int sides=16,bool caps=true) {
    auto axis=Unit(Add(b,Mul(a,-1))),u=Unit(Cross(axis,std::abs(axis.y)<.9f?Point{0,1,0}:Point{1,0,0})),v=Cross(axis,u);
    for(int i=0;i<sides;++i) {
        float f=i*2*pi/sides,g=(i+1)*2*pi/sides;
        Point n=Add(Mul(u,std::cos(f)),Mul(v,std::sin(f))),q=Add(Mul(u,std::cos(g)),Mul(v,std::sin(g)));
        Point ap=Add(a,Mul(n,radius)),aq=Add(a,Mul(q,radius)),bp=Add(b,Mul(n,radius)),bq=Add(b,Mul(q,radius));
        SmoothTri(m,ap,aq,bq,n,q,q,ink); SmoothTri(m,ap,bq,bp,n,q,n,ink);
        if(caps) { Tri(m,a,aq,ap,ink); Tri(m,b,bp,bq,ink); }
    }
}
inline void Ellipsoid(Mesh& m,Point center,Point radii,Color ink,int slices=24,int rings=12) {
    auto p=[&](float a,float b) { return Add(center,{radii.x*std::sin(a)*std::cos(b),radii.y*std::cos(a),radii.z*std::sin(a)*std::sin(b)}); };
    auto n=[&](Point v) { return Unit({(v.x-center.x)/(radii.x*radii.x),(v.y-center.y)/(radii.y*radii.y),(v.z-center.z)/(radii.z*radii.z)}); };
    for(int j=0;j<rings;++j) { for(int i=0;i<slices;++i) {
        auto a=p(j*pi/rings,i*2*pi/slices),b=p((j+1)*pi/rings,i*2*pi/slices),c=p((j+1)*pi/rings,(i+1)*2*pi/slices),d=p(j*pi/rings,(i+1)*2*pi/slices);
        if(j>0) { SmoothTri(m,a,b,d,n(a),n(b),n(d),ink); }
        if(j<rings-1) { SmoothTri(m,b,c,d,n(b),n(c),n(d),ink); }
    } }
}
inline void Ring(Mesh& m,Point p,float rx,float ry,float thickness,Color ink,int sides=40) {
    for(int i=0;i<sides;++i) {
        float a=i*2*pi/sides,b=(i+1)*2*pi/sides;
        Tube(m,Add(p,{std::sin(a)*rx,std::cos(a)*ry,0}),Add(p,{std::sin(b)*rx,std::cos(b)*ry,0}),thickness,ink,8,false);
    }
}
// [TIME] | [INFO] | GearGeometry.Curve | Catmull-Rom paths make continuous forged guards, roots, piping and cloth ties.
inline void Curve(Mesh& m,std::initializer_list<Point> controls,float radius,Color ink,int steps=6) {
    std::vector<Point> points(controls);
    for(size_t i=0;i+1<points.size();++i) {
        auto a=points[i?i-1:i],b=points[i],c=points[i+1],d=points[std::min(i+2,points.size()-1)];
        auto at=[&](float t) {
            return Mul(Add(Add(Mul(b,2),Mul(Add(c,Mul(a,-1)),t)),
                Add(Mul(Add(Add(Mul(a,2),Mul(b,-5)),Add(Mul(c,4),Mul(d,-1))),t*t),
                    Mul(Add(Add(Mul(a,-1),Mul(b,3)),Add(Mul(c,-3),d)),t*t*t))),.5f);
        };
        for(int s=0;s<steps;++s) { Tube(m,at(float(s)/steps),at(float(s+1)/steps),radius,ink,10,false); }
    }
}
inline void Gem(Mesh& m,Point p,float w,float h,float d,Color ink) {
    // Metal sockets use the same faceted primitive; an untagged jewel is a gem.
    ink=Paint(ink,ResolveSurface(ink,Surface::Gem));
    for(int i=0;i<8;++i) {
        float a=i*2*pi/8,b=(i+1)*2*pi/8;
        Point pa{p.x+std::sin(a)*w,p.y+std::cos(a)*h,p.z},pb{p.x+std::sin(b)*w,p.y+std::cos(b)*h,p.z};
        Point ta{p.x+std::sin(a)*w*.48f,p.y+std::cos(a)*h*.48f,p.z+d},tb{p.x+std::sin(b)*w*.48f,p.y+std::cos(b)*h*.48f,p.z+d};
        Quad(m,pb,pa,ta,tb,ink); Tri(m,tb,ta,{p.x,p.y,p.z+d},Shade(ink,1.12f));
        Tri(m,pa,pb,{p.x,p.y,p.z-d*.65f},Shade(ink,.7f));
    }
}
inline void Box(Mesh& m,Point a,Point b,Color c) {
    Quad(m,{a.x,a.y,b.z},{b.x,a.y,b.z},{b.x,b.y,b.z},{a.x,b.y,b.z},c);
    Quad(m,{b.x,a.y,a.z},{a.x,a.y,a.z},{a.x,b.y,a.z},{b.x,b.y,a.z},c);
    Quad(m,{a.x,a.y,a.z},{a.x,a.y,b.z},{a.x,b.y,b.z},{a.x,b.y,a.z},c);
    Quad(m,{b.x,a.y,b.z},{b.x,a.y,a.z},{b.x,b.y,a.z},{b.x,b.y,b.z},c);
    Quad(m,{a.x,b.y,b.z},{b.x,b.y,b.z},{b.x,b.y,a.z},{a.x,b.y,a.z},c);
    Quad(m,{a.x,a.y,a.z},{b.x,a.y,a.z},{b.x,a.y,b.z},{a.x,a.y,b.z},c);
}
inline void Star(Mesh& m,Point p,float size) {
    for(int i=0;i<4;++i) {
        float a=i*pi/2,b=a+pi/4,c=a+pi/2;
        Point tip{p.x+std::sin(a)*size,p.y+std::cos(a)*size,p.z};
        Point valley{p.x+std::sin(b)*size*.25f,p.y+std::cos(b)*size*.25f,p.z};
        Point next{p.x+std::sin(c)*size,p.y+std::cos(c)*size,p.z};
        Tri(m,p,tip,valley,i%2?cyan:cream); Tri(m,p,valley,next,cyan);
    }
    Gem(m,Add(p,{0,0,.18f}),size*.18f,size*.18f,.2f,gold);
}
// [TIME] | [INFO] | GearGeometry.Sword | Separate cutting bevel, fuller, grip braid, guard scrolls and crystal sockets share the icon silhouettes.
// [TIME] | [INFO] | GearGeometry.Relief | Crisp, beveled heraldry follows the painted designs instead of rounded tubes.
inline void Relief(Mesh& m, std::initializer_list<Point> points, float depth, Color ink) {
    std::vector<Point> outline(points); Point center{};
    for(auto p:outline) { center=Add(center,p); }
    center=Mul(center,1.f/outline.size());
    std::vector<Point> face; std::vector<int> remaining;
    for(size_t i=0;i<outline.size();++i) {
        auto p=Add(center,Mul(Add(outline[i],Mul(center,-1)),.84f)); p.z+=depth;
        face.push_back(p); remaining.push_back(int(i));
    }
    float area=0;
    for(size_t i=0;i<outline.size();++i) {
        size_t j=(i+1)%outline.size();
        area+=face[i].x*face[j].y-face[j].x*face[i].y;
        Quad(m,outline[i],outline[j],face[j],face[i],Shade(ink,.8f));
    }
    float sign=area>0?1.f:-1.f;
    auto turn=[](Point a,Point b,Point c) { return (b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x); };
    while(remaining.size()>2) {
        bool found=false;
        for(size_t i=0;i<remaining.size();++i) {
            auto a=face[remaining[(i+remaining.size()-1)%remaining.size()]],b=face[remaining[i]],c=face[remaining[(i+1)%remaining.size()]];
            if(sign*turn(a,b,c)<.00001f) { continue; }
            bool inside=false;
            for(size_t j=0;j<remaining.size();++j) {
                if(j==i || j==(i+1)%remaining.size() || j==(i+remaining.size()-1)%remaining.size()) { continue; }
                auto p=face[remaining[j]];
                if(sign*turn(a,b,p)>=0 && sign*turn(b,c,p)>=0 && sign*turn(c,a,p)>=0) { inside=true; break; }
            }
            if(inside) { continue; }
            if(sign>0) { Tri(m,a,b,c,ink); } else { Tri(m,c,b,a,ink); }
            remaining.erase(remaining.begin()+i); found=true; break;
        }
        if(!found) { break; }
    }
}
inline Mesh Sword(int element=0,bool hiltOnly=false,bool scabbard=false,bool empty=false,int base=2) {
    Mesh m;
    Color body=element==0?steel:element==1?Color{70,151,204}:element==2?Color{245,222,153}:base==3?Color{105,114,123}:silver;
    Color glow=element==0?Color{255,124,26}:element==1?cyan:element==2?cyan:jade;
    Color fitting=element==0?bronze:gold;
    Color grip=element==0 || (element<0 && base==3)?Color{110,37,40}:element==1?Color{23,88,104}:element==2?Color{99,38,76}:base==1?darkJade:indigo;
    grip=Paint(grip,Surface::Leather);glow=Paint(glow,Surface::Gem);
    // Each profile has the shoulders and point shown in its original inventory painting.
    std::vector<Point> profile;
    if(element==1) { profile={{1,1,0},{1.6f,4,0},{3.5f,6,0},{2,6.4f,0},{2,10,0},{3.5f,12,0},{2,12.4f,0},{2,16,0},{3.6f,18,0},{2.1f,18.4f,0},{2.4f,22,0},{3.5f,24,0},{.035f,29,0}}; }
    else if(element==0) { profile={{1.2f,1,0},{1.8f,3,0},{2.4f,5,0},{2.4f,21,0},{3.1f,25,0},{.035f,29,0}}; }
    else if(element==2) { profile={{1.2f,1,0},{2.3f,3,0},{1.65f,5,0},{1.65f,21,0},{2.65f,25,0},{.035f,29,0}}; }
    else if(base==1) { profile={{1.2f,1,0},{1.5f,4,0},{3.1f,11,0},{3.4f,18,0},{2.1f,23,0},{.035f,29,0}}; }
    else if(base==3) { profile={{3.1f,1,0},{3.1f,22,0},{2.5f,26,0},{.035f,29,0}}; }
    else { profile={{1.1f,1,0},{2.4f,4,0},{2.4f,7,0},{1.5f,10,0},{1.35f,24,0},{.035f,29,0}}; }
    if(!hiltOnly && !scabbard) {
        for(size_t j=0;j+1<profile.size();++j) for(float side:{-1.f,1.f}) for(int k=0;k<4;++k) {
            auto v=[&](size_t row,int col) {
                float xs[]={-1,-.72f,0,.72f,1},zs[]={0,.38f,.6f,.38f,0};
                return Point{profile[row].x*xs[col],profile[row].y,side*zs[col]*(1-profile[row].y/50)};
            };
            auto a=v(j,k),b=v(j,k+1),c=v(j+1,k+1),d=v(j+1,k);
            Color edge=element==2?cream:silver;
            if(side>0) { Quad(m,a,b,c,d,k==0||k==3?edge:body); }
            else { Quad(m,b,a,d,c,k==0||k==3?edge:body); }
        }
        for(float side:{-1.f,1.f}) {
            if(element==0) {
                Curve(m,{{-.3f,4,side*.59f},{.4f,8,side*.53f},{-.4f,12,side*.48f},{.4f,17,side*.4f},{-.1f,24,side*.32f}},.15f,glow,2);
            } else if(element<0 && base==3) {
                for(float y:{5.f,10.f,15.f,20.f}) { Gem(m,{0,y,side*.6f},.4f,.4f,.15f,silver); }
            } else if(element<0 && base==2) {
                Curve(m,{{0,4,side*.6f},{0,13,side*.49f},{0,24,side*.32f}},.12f,Shade(body,.5f),2);
                Gem(m,{0,13,side*.62f},.4f,1,.15f,jade);
            }
        }
    }
    if(scabbard) {
        Ellipsoid(m,{0,13,0},{base==3?3.4f:2.3f,13.2f,1.1f},grip);
        for(float y:{1.5f,12.f,23.f}) {
            Curve(m,{{-2.15f,y,0},{-1.65f,y,1.05f},{0,y,1.18f},{1.65f,y,1.05f},{2.15f,y,0}},.26f,fitting);
        }
        Gem(m,{0,24,.9f},1.2f,2,.4f,fitting);
    }
    if(!empty) {
        for(float sign:{-1.f,1.f}) {
            if(element<0 && base==3) {
                Relief(m,{{0,-.8f,.2f},{sign*6,-.8f,.2f},{sign*6,1,.2f},{0,1,.2f}},.5f,fitting);
                Box(m,{sign>0?5.6f:-6.6f,-1.2f,-.7f},{sign>0?6.6f:-5.6f,1.4f,.7f},fitting);
            } else if(element==2) {
                Relief(m,{{0,0,.2f},{sign*7,-1.5f,.2f},{sign*3.5f,1.3f,.2f},{sign*4.8f,4,.2f},{sign*1.2f,2,.2f},{0,4,.2f}},.55f,fitting);
            } else if(element==1 || (element<0 && base==2)) {
                Relief(m,{{0,1,.2f},{sign*3,.7f,.2f},{sign*5.8f,3.5f,.2f},{sign*6.2f,1.7f,.2f},{sign*5.2f,-.4f,.2f},{sign*3.1f,-1.5f,.2f},{0,-.5f,.2f}},.5f,fitting);
            } else {
                Relief(m,{{0,1,.2f},{sign*3,1,.2f},{sign*4.5f,2.3f,.2f},{sign*5,4,.2f},{sign*6.2f,2.5f,.2f},{sign*6,.4f,.2f},{sign*4,-1.1f,.2f},{0,-.7f,.2f}},.5f,fitting);
            }
        }
        Gem(m,{0,0,.7f},1.45f,1.8f,.5f,fitting);
        if(element!=0 && !(element<0 && base==3)) { Gem(m,{0,0,1.22f},.8f,1.1f,.3f,glow); }
        Tube(m,{0,-10.7f,0},{0,-.6f,0},.85f,grip,16);
        for(int i=0;i<9;++i) {
            float y=-1.5f-i;
            Curve(m,{{-.75f,y,-.4f},{-.65f,y-.2f,.6f},{0,y-.45f,.88f},{.65f,y-.7f,.6f},{.75f,y-.9f,-.4f}},.11f,Shade(grip,1.6f),3);
        }
        Tube(m,{0,-10.8f,0},{0,-10.2f,0},1.02f,fitting,16);
        Gem(m,{0,-12,0},1.65f,1.9f,1.1f,fitting);
        if(element!=2) { Gem(m,{0,-12,1.1f},.85f,1,.3f,glow); }
    }
    // [TIME] | [INFO] | GearGeometry.SwordScale | One profile drives pickups, hand mounts and scabbards; their proportions no longer change on equip.
    float length=base==4?.48f:base==3?1.48f:base==1?.74f:1.f;
    for(auto& v:m) { v.p.y*=length; v.normal=Unit({v.normal.x,v.normal.y/length,v.normal.z}); }
    FinishMaterials(m,Surface::Metal);
    return m;
}
// [TIME] | [INFO] | GearGeometry.Shield | Domed shells replace the flat polygon shield; all base variants have distinct relief work.
inline Mesh Shield(int variant=0) {
    Mesh m;
    Color face=variant==0||variant==1?oak:variant==2?Color{40,73,109}:silver;
    face=Paint(face,variant<=1?Surface::Wood:Surface::Metal);
    Color rim=variant==1?bronze:gold;
    std::vector<Point> edge=variant==2?std::vector<Point>{{0,15.5f,0},{4.5f,12.1f,0},{11.5f,9.5f,0},{11,-1,0},{8.2f,-8.5f,0},{0,-15.5f,0},{-8.2f,-8.5f,0},{-11,-1,0},{-11.5f,9.5f,0},{-4.5f,12.1f,0}}:
        std::vector<Point>{{0,15.5f,0},{6.2f,10.6f,0},{10.1f,5.2f,0},{11.5f,-1,0},{9.5f,-7.3f,0},{5.3f,-12.4f,0},{0,-15.5f,0},{-5.3f,-12.4f,0},{-9.5f,-7.3f,0},{-11.5f,-1,0},{-10.1f,5.2f,0},{-6.2f,10.6f,0}};
    // The mirror keeps its rounded oval; the other shields keep pointed heraldic outlines.
    if(variant==3) { edge.clear(); for(int i=0;i<48;++i) { float a=i*2*pi/48;edge.push_back({std::sin(a)*11.5f,std::cos(a)*15.5f,0}); } }
    auto surface=[](Point p,float r) { return Point{p.x*r,p.y*r,1.2f+2.4f*(1-r*r)}; };
    for(size_t i=0;i<edge.size();++i) {
        auto a=edge[i],b=edge[(i+1)%edge.size()];
        for(int j=0;j<6;++j) {
            auto p=surface(a,j/6.f),q=surface(a,(j+1)/6.f),r=surface(b,(j+1)/6.f),t=surface(b,j/6.f);
            if(j) { Tri(m,p,t,q,face); } Tri(m,q,t,r,face);
        }
        auto innerA=surface(a,.91f),innerB=surface(b,.91f);
        innerA.z+=.35f; innerB.z+=.35f;
        Quad(m,{a.x,a.y,1.2f},{b.x,b.y,1.2f},innerB,innerA,rim);
        Quad(m,{a.x,a.y,-.6f},{b.x,b.y,-.6f},{b.x,b.y,1.2f},{a.x,a.y,1.2f},Shade(rim,.65f));
        Tri(m,{0,0,-.6f},{a.x,a.y,-.6f},{b.x,b.y,-.6f},Shade(face,.6f));
        if(i%2==0) { Gem(m,{a.x*.955f,a.y*.955f,1.65f},.42f,.42f,.25f,rim); }
    }
    auto depth=[](float x,float y) { return 3.75f-2.4f*(x*x/128+y*y/234); };
    if(variant<=1) {
        for(int i=-3;i<=3;++i) {
            float x=i*2.2f;
            Curve(m,{{x,-10,depth(x,-10)},{x+.25f,-4,depth(x,-4)},{x-.2f,3,depth(x,3)},{x,10,depth(x,10)}},.06f,Color{131,86,43,Surface::Wood});
        }
    }
    if(variant==0) {
        Relief(m,{{0,12,3.2f},{3.8f,6,3.2f},{4.5f,0,3.2f},{2.8f,-6,3.2f},{0,-12,3.2f},{-2.8f,-6,3.2f},{-4.5f,0,3.2f},{-3.8f,6,3.2f}},.4f,darkJade);
        Curve(m,{{0,-11,3.75f},{0,0,4},{0,11,3.75f}},.16f,jade,2);
        for(float sign:{-1.f,1.f}) for(float y:{-6.f,-2.f,2.f,6.f}) { Tube(m,{0,y-1,3.8f},{sign*3,y+1.5f,3.8f},.1f,jade,6); }
    }
    for(float sign:{-1.f,1.f}) {
        if(variant==0) {
            Curve(m,{{sign*5,-11,2},{sign*2,-7,3.3f},{-sign*2,-2,3.8f},{sign*2,3,3.7f},{sign*3,8,3},{sign,12,2.5f}},.4f,Paint(cream,Surface::Wood));
            Curve(m,{{sign*2,3,3.7f},{sign*5,5,3.4f},{sign*7,7,2.8f}},.28f,Paint(cream,Surface::Wood));

        } else if(variant==2) {
            Relief(m,{{sign*1,-10,3.4f},{sign*2,0,3.4f},{sign*4,5,3.4f},{sign*8,9,3.4f},{sign*7,3,3.4f},{sign*4,1,3.4f},{sign*3,-6,3.4f}},.4f,cream);
            for(int i=0;i<3;++i) {
                float y=2-i*3.2f;
                Relief(m,{{sign*3,y,3.4f},{sign*8,y+2,3.4f},{sign*6.5f,y-1.1f,3.4f},{sign*3.7f,y-2,3.4f}},.35f,cream);
            }
        } else if(variant==3) {
            Curve(m,{{sign*2,-10,2.8f},{sign*6,-3,3.5f},{sign*6,5,3.3f},{sign*2,11,2.8f}},.3f,bronze);
        }
    }
    if(variant==3) {
        Ellipsoid(m,{0,0,3.5f},{5.3f,8,.9f},Color{197,226,228},32,12);
        Ring(m,{0,0,3.9f},5.4f,8.1f,.2f,gold);
        Curve(m,{{-2,-3,4.45f},{-2,2,4.5f},{-1,5,4.3f}},.12f,cream);
    } else {
        Gem(m,{0,1,3.9f},2.4f,3.3f,.7f,rim); Gem(m,{0,1,4.65f},1.65f,2.4f,.75f,jade);
        if(variant==1) { Curve(m,{{0,-8,3.3f},{0,-2,3.8f},{0,6,3.3f}},.32f,jade); }
    }
    Curve(m,{{-3,0,-.8f},{-2,0,-3},{2,0,-3},{3,0,-.8f}},.6f,leather);
    FinishMaterials(m,Surface::Metal);
    return m;
}
struct Section { float y, width, depth, z=0; };
inline void Loft(Mesh& m,std::initializer_list<Section> profile,Point offset,Color ink,int sides=32) {
    std::vector<Section> rows(profile);
    for(size_t j=0;j+1<rows.size();++j) { for(int i=0;i<sides;++i) {
        auto at=[&](size_t row,int col) { auto s=rows[row]; float a=col*2*pi/sides; return Add(offset,{std::sin(a)*s.width,s.y,std::cos(a)*s.depth+s.z}); };
        auto n=[&](size_t row,int col) {
            auto s=rows[row],lo=rows[row?row-1:row],hi=rows[std::min(row+1,rows.size()-1)];
            float a=col*2*pi/sides,dy=hi.y-lo.y;
            return Unit({std::sin(a)/s.width,-((hi.width-lo.width)/s.width*std::sin(a)*std::sin(a)+(hi.depth-lo.depth)/s.depth*std::cos(a)*std::cos(a))/dy,std::cos(a)/s.depth});
        };
        auto a=at(j,i),b=at(j,i+1),c=at(j+1,i+1),d=at(j+1,i);
        SmoothTri(m,a,b,c,n(j,i),n(j,i+1),n(j+1,i+1),ink); SmoothTri(m,a,c,d,n(j,i),n(j+1,i+1),n(j+1,i),ink);
    } }
}
inline Color Cloth(int variant) { return variant==1?Color{55,105,54}:variant==2?Color{142,43,30}:variant==3?Color{33,92,147}:Color{48,51,157}; }
inline Mesh Tunic(bool trimOnly=false,int variant=0) {
    Mesh m; Color cloth=Cloth(variant),trim=Paint(variant==2?gold:cream,Surface::Cloth);
    if(!trimOnly) {
        Loft(m,{{-11,8.4f,3.4f},{-9,8.2f,3.5f},{-4,6.7f,3.2f},{0,6,3},{5,7,3.5f},{9,7.6f,3.2f},{11,4,2.7f}}, {},cloth);
        for(float sign:{-1.f,1.f}) {
            Tube(m,{sign*6,8,0},{sign*12,4,0},3.5f,cloth,24);
            Tube(m,{sign*11.7f,4.2f,0},{sign*12.3f,3.8f,0},3.6f,trim,24,false);
            Curve(m,{{sign*7.7f,-10.6f,1.3f},{sign*5,-10.6f,2.9f},{0,-10.6f,3.5f}},.2f,trim);
            Curve(m,{{sign*4,10.8f,2.9f},{sign*3,8.8f,3.6f},{0,7,3.9f}},.3f,trim);
            for(int i=0;i<3;++i) { Curve(m,{{sign*(2.f+i*1.7f),-9,3.4f},{sign*(1.8f+i*1.55f),-6,3.4f},{sign*(1.5f+i*1.45f),-3,3.2f}},.11f,Shade(cloth,1.35f)); }
        }
        Loft(m,{{-2.5f,6.5f,3.3f},{-1,6.3f,3.3f}}, {},variant==0?gold:leather);
        Box(m,{-1.5f,-2.7f,3.5f},{1.5f,-.7f,3.8f},gold);
        Box(m,{-1,-2.25f,3.81f},{1,-1.15f,3.9f},variant==0?bronze:leather);
        if(variant==0) { Star(m,{0,3.6f,3.65f},2.8f); }
        else { Gem(m,{0,4,3.65f},.9f,1.8f,.35f,variant==2?Color{255,139,45}:jade); }
    } else {
        if(variant==0) { Star(m,{0,0,0},3); }
        else { Gem(m,{0,0,0},.8f,1.6f,.32f,variant==2?gold:jade); }
        for(float sign:{-1.f,1.f}) {
            Curve(m,{{sign*3.8f,5.5f,-1},{sign*2.7f,4.5f,-.5f},{sign,3.5f,-.15f}},.25f,trim);
            Curve(m,{{sign*3.7f,-1,-.4f},{sign*3.5f,-3,-.6f},{sign*3,-4,-.8f}},.08f,trim);
        }
    }
    FinishMaterials(m,Surface::Cloth);
    return m;
}
inline Mesh Boot(bool cuffOnly,int variant=0) {
    Mesh m; Color cuff=variant==0?Color{145,186,153}:variant==2?steel:variant==3?cream:leather;
    if(!cuffOnly) {
        Ellipsoid(m,{0,-7.75f,2.4f},{2.65f,.35f,4.5f},variant==2?steel:cream,24,6);
        Loft(m,{{-8,2.6f,4.4f,2.4f},{-7.5f,3.15f,5.6f,2.4f},{-6.3f,3.3f,5.7f,2.4f},{-5.9f,3.15f,5.5f,2.4f}}, {},variant==2?steel:cream);
        Loft(m,{{-6,3.1f,5.4f,2.4f},{-4.5f,3.2f,5.2f,2.2f},{-2.5f,2.95f,4.8f,1.5f},{-.5f,2.7f,3.4f,.6f},{1.5f,2.9f,2.8f},{5,3.3f,3.1f}}, {},leather);
        for(float sign:{-1.f,1.f}) { Curve(m,{{sign*2.8f,-5,5},{sign*2.8f,-3,2},{sign*2.6f,1,1.9f},{sign*2.9f,4,1.8f}},.08f,cream); }
        for(int i=0;i<4;++i) { Curve(m,{{-1.7f,-2.f+i*.75f,4.4f-i*.38f},{0,-1.7f+i*.75f,4.55f-i*.38f},{1.7f,-2.f+i*.75f,4.4f-i*.38f}},.1f,Paint(bronze,Surface::Cloth),3); }
        if(variant==2) { Ellipsoid(m,{0,-3.5f,5},{3.15f,1.7f,3},silver,24,10); }
    }
    float y=cuffOnly?0:3.8f;
    Loft(m,{{y,3.65f,3.5f},{y+.35f,3.9f,3.7f},{y+2.8f,3.9f,3.7f},{y+3.15f,3.65f,3.5f}}, {},cuff);
    Loft(m,{{y+2.75f,3.98f,3.78f},{y+3.12f,3.98f,3.78f}}, {},variant==2?silver:cream);
    Gem(m,{0,y+1.5f,3.8f},1.2f,1.25f,.5f,bronze);
    if(variant==0 || variant==3) {
        Relief(m,{{-.65f,y-.1f,4.35f},{-.75f,y+1.8f,4.35f},{.8f,y+3,4.35f},{.7f,y+1,4.35f},{0,y+.2f,4.35f}},.2f,gold);
        if(variant==0 && !cuffOnly) {
            for(float sign:{-1.f,1.f}) {
                Relief(m,{{sign*2.8f,1,1.8f},{sign*6,-1,1.8f},{sign*8,-4,1.8f},{sign*5,-2.8f,1.8f},{sign*3,-.2f,1.8f}},.18f,darkJade);
            }
        }
        Curve(m,{{2.6f,y+2,2.8f},{4,y+.5f,3.5f},{4.2f,y-1.8f,4},{3.5f,y-3.3f,4.5f}},.3f,jade);
    }
    FinishMaterials(m,Surface::Leather);
    return m;
}
inline Mesh Boots(bool cuffsOnly=false,int variant=0) {
    auto m=Boot(cuffsOnly,variant);
    if(cuffsOnly) { return m; }
    Mesh result; result.reserve(m.size()*2);
    for(float x:{-4.8f,4.8f}) { for(auto v:m) { v.p.x+=x;result.push_back(v); } }
    return result;
}
}
