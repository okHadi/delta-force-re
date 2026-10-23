#include "dfre/mission_object_helo_pose.hpp"
#include "dfre/model_specials.hpp"
#include "dfre/projectiles.hpp"
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
using namespace dfre;using U=std::uint32_t;
namespace {std::int32_t si(U n){std::int32_t s;std::memcpy(&s,&n,4);return s;}}
int main(){try{std::string line;while(std::getline(std::cin,line)){
    U op,math,mutation,out,count;std::istringstream in(line);if(!(in>>op>>math>>mutation>>out>>count))throw MissionTickError("Short helicopter pose fixture");
    constexpr U a=0x7dde80,g=0x1301000,l=0x1302000,f=0x1304000,hp=0x1306000,hp2=0x1307000;std::unordered_map<U,std::uint8_t> bytes;
    const auto get=[&](U at){U n=0;for(U i=0;i<4;++i)n|=U(bytes[at+i])<<(8*i);return n;};
    const auto put=[&](U at,U n){for(U i=0;i<4;++i)bytes[at+i]=std::uint8_t(n>>(8*i));};
    for(U i=0;i<count;++i){U at,n;if(!(in>>at>>n))throw MissionTickError("Short helicopter pose words");put(at,n);}
    std::ostringstream trace;MissionObjectHeloBindings b;
    b.read=[&](U at,U width){if(width!=4)throw MissionTickError("Unexpected helicopter pose read width");const auto n=get(at);trace<<'R'<<at<<",4,"<<n<<';';if((mutation&16)&&at==l+0xa4)put(hp+0x34,0x80000000);return n;};
    b.write=[&](U at,U width,U n){if(width!=4)throw MissionTickError("Unexpected helicopter pose write width");trace<<'W'<<at<<",4,"<<n<<';';put(at,n);
        if((mutation&2)&&at==out)put(l+0xa4,hp2);
        if((mutation&4)&&at==out+0xc)put(a+0xc8,0x11223344);
        if((mutation&64)&&at==out+4)throw MissionTickError("Fixture store stopped");
    };
    b.call=[&](U site,const std::vector<U>& args){trace<<'C'<<site;for(auto n:args)trace<<','<<n;trace<<';';
        if(site==0x45d1e0){if(mutation&1){put(a+0x10,g+0x100);put(g+0x50,l+0x100);put(l+0xa4,hp2);}if(mutation&32)throw MissionTickError("Fixture compose stopped");}
        if(site==0x45d400&&(mutation&8))put(f+0x40,0xffc00000);
        if(site==0x45cda0&&(mutation&128))throw MissionTickError("Fixture transform stopped");
        if(!math){const U destination=site==0x45cb40||site==0x45cda0?args.at(2):args.at(0);const U source=site==0x45cb40||site==0x45cda0?args.at(1):args.at(1);const U size=site==0x45cda0?3:12;std::vector<U> values;for(U i=0;i<size;++i)values.push_back(get(source+(i%6)*4)^site^(i*0x10203));for(U i=0;i<size;++i)put(destination+i*4,values[i]);return 0u;}
        if(site==0x45d1e0){const U pose=args.at(1);const auto result=compose_pose({si(get(pose)),si(get(pose+4)),si(get(pose+8))},{get(pose+0xc),get(pose+0x10),get(pose+0x14)});for(U i=0;i<12;++i)put(args.at(0)+i*4,U(result.m[i]));}
        else if(site==0x45d400){const U pose=args.at(1);const auto result=model_special_pivot_matrix({si(get(pose)),si(get(pose+4)),si(get(pose+8))},{si(get(pose+0xc)),si(get(pose+0x10)),si(get(pose+0x14))});for(U i=0;i<12;++i)put(args.at(0)+i*4,U(result[i]));}
        else if(site==0x45cb40){Matrix4 left,right;for(U i=0;i<12;++i){left.m[i]=si(get(args.at(0)+i*4));right.m[i]=si(get(args.at(1)+i*4));}const auto result=matrix_multiply(left,right);for(U i=0;i<12;++i)put(args.at(2)+i*4,U(result.m[i]));}
        else if(site==0x45cda0){Matrix4 matrix;for(U i=0;i<12;++i)matrix.m[i]=si(get(args.at(0)+i*4));const U point=args.at(1);const auto result=transform_point(matrix,{si(get(point)),si(get(point+4)),si(get(point+8))});put(args.at(2),U(result.x));put(args.at(2)+4,U(result.y));put(args.at(2)+8,U(result.z));}
        else throw MissionTickError("Unknown helicopter pose matrix service");return 0u;
    };
    bool failed=false;try{if(op)mission_object_helo_weapon_pose(b,a,out,f,0x80);else mission_object_helo_smoke_pose(b,a,out,f,0x80);}catch(const MissionTickError&){failed=true;}
    std::uint64_t hash=1469598103934665603ull;
    for(const auto& region:{std::pair<U,U>{a,0x114},{g,0x200},{l,0x200},{0x1303000,0x18},{f,0x80},{hp,0x200},{hp2,0x200}})for(U i=0;i<region.second;++i){hash^=bytes[region.first+i];hash*=1099511628211ull;}
    std::cout<<failed<<' '<<hash<<' '<<trace.str()<<'\n';
}return 0;}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
