#include "dfre/mission_model_registry.hpp"
#include <cctype>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
using namespace dfre;using U=std::uint32_t;
namespace {constexpr U N=0x1300000,F=0x1301000,E=0x1302000,P=0x1303000,C=0xc80d14,R=0xc80d20;constexpr std::pair<U,U> regions[]={{N,64},{F,32},{E,12},{P,8},{C,4},{R,4*0x7c}};}
int main(){try{U mode,mutation;std::string input;while(std::cin>>mode>>mutation>>input){
 std::unordered_map<U,std::uint8_t> bytes;std::size_t cursor=0;for(auto region:regions)for(U i=0;i<region.second;++i){bytes[region.first+i]=std::uint8_t(std::stoul(input.substr(cursor,2),nullptr,16));cursor+=2;}
 const auto get=[&](U at,U width=4){U n=0;for(U i=0;i<width;++i){const auto found=bytes.find(at+i);if(found==bytes.end()){if(at+i>=R&&at+i<R+500*0x7c)continue;throw MissionModelRegistryError("Fixture memory is not owned");}n|=U(found->second)<<(i*8);}return n;};const auto put=[&](U at,U width,U n){for(U i=0;i<width;++i)bytes.at(at+i)=std::uint8_t(n>>(i*8));};
 const auto string=[&](U at){std::string value;for(U i=0;i<512;++i){const U ch=get(at+i,1);if(!ch)return value;value+=char(ch);}throw MissionModelRegistryError("Fixture string is too long");};const auto text=[&](U at,const std::string& value){for(U i=0;i<=value.size();++i)put(at+i,1,i<value.size()?U(std::uint8_t(value[i])):0);};
 std::ostringstream trace;unsigned calls=0,stores=0,compares=0;MissionModelRegistryBindings b;
 b.read=[&](U at,U width){U n=get(at,width);trace<<'R'<<at<<','<<width<<','<<n<<';';if((mutation&1)&&at==E)put(E,4,N+16);return n;};
 b.write=[&](U at,U width,U n){trace<<'W'<<at<<','<<width<<','<<n<<';';put(at,width,n);++stores;if((mutation&2)&&at==P)put(C,4,2);if((mutation&4)&&at==P+4)put(C,4,3);if((mutation&8)&&at==F+1)put(N+2,1,'X');if((mutation&1024)&&stores==((mutation>>7)%6+1))throw MissionModelRegistryError("Fixture store stopped");};
 b.call=[&](U site,const std::vector<U>& args)->U{trace<<'C'<<site;for(U n:args)trace<<','<<n;trace<<';';++calls;if((mutation&2048)&&calls==((mutation>>7)%6+1))throw MissionModelRegistryError("Fixture service stopped");
  if(site==0x499e5e){++compares;auto a=string(args.at(0)),z=string(args.at(1));for(auto& ch:a)ch=char(std::tolower(static_cast<unsigned char>(ch)));for(auto& ch:z)ch=char(std::tolower(static_cast<unsigned char>(ch)));if((mutation&16)&&compares==1)put(C,4,1);return a==z?0:1;}
  if(site==0x456560){auto name=string(args.at(0));const auto dot=name.find('.');if(dot==std::string::npos)name+=".3di";else name.resize(dot+1),name+="3di";text(args.at(0),name);return 0;}
  if(site==0x48fd00){if(mutation&32){put(E+4,4,5);put(C,4,1);}return 0;}
  if(site==0x486370){if(mutation&64)put(E+8,4,P+4);put(args.at(0)+0x20,4,0xabcdef01);return mode&1?0:1;}
  if(site==0x48fd70){if(mutation&128)put(E+8,4,P+4);if(mutation&256)put(C,4,2);return 0;}
  throw MissionModelRegistryError("Fixture service is not owned");
 };
 bool failed=false;U result=0;try{result=mission_model_registry_load(b,E,F);}catch(const std::exception&){failed=true;}
 std::uint64_t hash=1469598103934665603ull;for(auto region:regions)for(U i=0;i<region.second;++i){hash^=bytes.at(region.first+i);hash*=1099511628211ull;}
 std::cout<<failed<<' '<<result<<' '<<hash<<' '<<trace.str()<<'\n';
}return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
