#include "dfre/mission_object_barrel.hpp"
#include <array>
#include <iostream>
#include <sstream>
using namespace dfre;using U=std::uint32_t;
int main(){try{std::string line;while(std::getline(std::cin,line)){
 U host,mutation,fail_call,message;std::string hex;std::istringstream in(line);if(!(in>>host>>mutation>>fail_call>>message>>hex)||hex.size()!=0x228)throw MissionTickError("Short barrel fixture");
 std::array<std::uint8_t,0x114> image{};std::array<std::uint8_t,24> frame{};frame.fill(0xa5);for(std::size_t i=0;i<image.size();++i)image[i]=std::uint8_t(std::stoul(hex.substr(i*2,2),nullptr,16));constexpr U A=0x7dde80,F=0x1300000;std::ostringstream trace;U calls=0;
 const auto bytes=[&](U at)->std::uint8_t&{if(at>=A&&at<A+image.size())return image.at(at-A);if(at>=F&&at<F+frame.size())return frame.at(at-F);throw MissionTickError("Barrel fixture address");};
 const auto put=[&](U at,U n,U width){for(U i=0;i<width;++i)bytes(at+i)=std::uint8_t(n>>(8*i));};
 MissionObjectBarrelBindings b;
 b.read=[&](U at,U width){U n=0;if(at==0x67f528&&width==4)n=host;else for(U i=0;i<width;++i)n|=U(bytes(at+i))<<(8*i);trace<<'R'<<at<<','<<width<<','<<n<<';';if(at==A+0x2c&&(mutation&4))put(A+0x28,0x12345678,4);return n;};
 b.write=[&](U at,U width,U n){put(at,n,width);trace<<'W'<<at<<','<<width<<','<<n<<';';if(at==A+0xfc&&(mutation&2))put(A+0x6c,0x80,1);};
 b.call=[&](U site,const std::vector<U>& args){trace<<'C'<<site;for(auto n:args)trace<<','<<n;trace<<';';if(++calls==fail_call)throw MissionTickError("Barrel fixture call stopped");if(site==0x446fb0&&(mutation&1)){put(A+0x24,11,4);put(A+0x28,22,4);put(A+0x2c,33,4);}return 0u;};
 bool failed=false;try{mission_object_barrel(b,A,F,message);}catch(const MissionTickError&){failed=true;}
 std::uint64_t hash=1469598103934665603ull;for(const auto& data:{std::vector<std::uint8_t>(image.begin(),image.end()),std::vector<std::uint8_t>(frame.begin(),frame.end())})for(auto n:data){hash^=n;hash*=1099511628211ull;}std::cout<<failed<<' '<<hash<<' '<<trace.str()<<'\n';
}return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
