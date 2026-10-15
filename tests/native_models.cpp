#include "dfre/models.hpp"
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <vector>
namespace {
using Bytes=std::vector<std::uint8_t>;
void put(Bytes& b,std::size_t p,std::uint32_t v){for(int i=0;i<4;++i)b.at(p+i)=std::uint8_t(v>>(8*i));}
Bytes triangle(unsigned version) {
    const std::size_t hs=version==5?184:176,ms=version==2?88:(version==5?112:104),base=128+hs;
    Bytes b(base+24+8+72+ms+76);b[0]='3';b[1]='D';b[2]='I';b[3]=version;put(b,20,1);
    put(b,128+20,std::uint32_t(b.size()-base));put(b,128+128,3);put(b,128+136,1);put(b,128+144,1);put(b,128+152,1);put(b,128+168,1);
    b[base]=255;b[base+1]=255;b[base+8+2]=10;b[base+16+4]=20;
    const auto face=base+32;b[face+30]=1;b[face+32]=2;
    const auto mesh=face+72;put(b,mesh+4,3);put(b,mesh+12,1);put(b,mesh+20,1);
    return b;
}
void check(bool result,const char* message){if(!result){std::cerr<<message<<'\n';std::exit(1);}}
void rejects(const Bytes& b){try{dfre::decode_model(b);check(false,"accepted invalid model");}catch(const std::invalid_argument&) {}}
}
int main() {
    for(unsigned version: {2,3,4,5}) {
        const auto b=triangle(version);const auto m=dfre::decode_model(b);
        check(m.version==version && m.lods.size()==1,"version or LOD changed");
        check(m.lods[0].vertices[0].position[0]==-1,"signed vertex changed");
        check(m.lods[0].faces[0].vertices[2]==2,"triangle index changed");
        for(std::size_t n=0;n<b.size();++n)rejects(Bytes(b.begin(),b.begin()+n));
        auto bad=b;bad.push_back(0);rejects(bad);
        bad=b;put(bad,128+128,0xffffffff);rejects(bad);
        bad=b;put(bad,128+20,0xffffffff);rejects(bad);
        bad=b;const std::size_t face=128+(version==5?184:176)+32;bad[face+28]=3;rejects(bad);
        bad=b;bad[face+34]=1;rejects(bad);
        bad=b;put(bad,face+68,1);rejects(bad);
    }
    auto b=triangle(5);b[3]=8;rejects(b);b=triangle(5);b[0]=0;rejects(b);
    b=triangle(5);put(b,124,0xffffffff);rejects(b);b=triangle(5);put(b,20,0xffffffff);rejects(b);
    // Texture size and dimensions are separate fields; verify safe payload preservation.
    b=triangle(5);Bytes texture(1061);texture[0]='T';put(texture,16,1);texture[20]=7;texture[24]=1;texture[26]=1;texture[36]=42;texture[37]=123;
    b.insert(b.begin()+128,texture.begin(),texture.end());put(b,124,1);
    put(b,b.size()-76+16,1);b[b.size()-76+52]=7;
    auto m=dfre::decode_model(b);check(m.textures[0].pixels[0]==42 && m.textures[0].palette_rgb_pad[0]==123,"texture data changed");
    check(m.lods[0].materials[0].texture_candidates==std::vector<std::size_t>{0},"texture sequence binding changed");
    b[128+16]=255;b[128+17]=255;b[128+18]=255;b[128+19]=255;rejects(b);
    std::cout<<"model bounds and geometry tests passed\n";
}
