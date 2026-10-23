#include "dfre/mission_item_resource.hpp"
#include "dfre/model_visibility.hpp"
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
using namespace dfre;using U=std::uint32_t;
namespace {constexpr U D=0x1300000,M=0x1302000,F=0x1303000;constexpr std::pair<U,U> regions[]={{D,0x1000},{M,0x300},{F,4},{0xc23810,8},{0x503138,4},{0x503228,4}};}
int main(){try{U index,mode,mutation;std::string input;while(std::cin>>index>>mode>>mutation>>input){
 std::unordered_map<U,std::uint8_t> bytes;std::size_t cursor=0;for(auto region:regions)for(U i=0;i<region.second;++i){bytes[region.first+i]=std::uint8_t(std::stoul(input.substr(cursor,2),nullptr,16));cursor+=2;}
 const auto get=[&](U at,U width=4){U n=0;for(U i=0;i<width;++i)n|=U(bytes.at(at+i))<<(i*8);return n;};const auto put=[&](U at,U n){for(U i=0;i<4;++i)bytes.at(at+i)=std::uint8_t(n>>(i*8));};
 std::ostringstream trace;unsigned calls=0,stores=0,loads=0;const U offset=index*0x13c,model=mode&64?D+0x20:M;
 MissionItemResourceBindings b;
 b.read=[&](U at,U width){U n=get(at,width);trace<<'R'<<at<<','<<width<<','<<n<<';';if((mutation&1)&&at==0x503138)put(0xc23810,D+0x800-offset);if((mutation&2)&&at==0x503228)put(0xc23810,D-offset);return n;};
 b.write=[&](U at,U width,U n){trace<<'W'<<at<<','<<width<<','<<n<<';';put(at,n);++stores;if((mutation&4)&&at==model+0x3c)put(model+0x2c,0x68656c6f);if((mutation&8)&&at==model+0x40)put(model+0x30,0x6f726730);if((mutation&16)&&at==model+0x44)put(model+0x34,0x74726565);if((mutation&32)&&at==model+0x48)put(model+0x28,0x65666374);if((mutation&64)&&at==D+0x9c)put(0xc23810,D+0x800-offset);if((mode&128)&&stores==((mutation>>8)%9+1))throw MissionItemResourceError("Fixture store stopped");};
 b.call=[&](U site,const std::vector<U>& args)->U{trace<<'C'<<site;for(U n:args)trace<<','<<n;trace<<';';++calls;if((mode&256)&&calls==((mutation>>8)%9+1))throw MissionItemResourceError("Fixture call stopped");
  if(site==0x4564b0)return 0;
  if(site==0x486530){++loads;put(F,loads==1?U((mode&4)!=0):U((mode&8)!=0));if(mutation&128)put(0xc23810,(loads&1?D+0x800:D)-offset);return mode&16?0:loads==1?model:M+0x100;}
  if(site==0x42cbb0){if(mutation&256)put(0xc23810,D-offset);return 0x1550000;}
  if(site==0x42d200){if(mutation&512)put(0xc23810,D+0x800-offset);return 0x1551000;}
  if(site==0x4818f0||site==0x481a00){const auto* row=find_model_callback_tag(args.at(0));if(!row)row=&model_callback_tags.front();if(mutation&1024)put(0xc23810,D+0x800-offset);return site==0x4818f0?row->draw_address:row->matrix_address;}
  throw MissionItemResourceError("Fixture body is not owned");
 };
 bool failed=false;try{mission_item_resource_load(b,index,F);}catch(const std::exception&){failed=true;}
 std::uint64_t hash=1469598103934665603ull;for(auto region:regions)for(U i=0;i<region.second;++i){hash^=bytes.at(region.first+i);hash*=1099511628211ull;}
 std::cout<<failed<<' '<<hash<<' '<<trace.str()<<'\n';
}return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
