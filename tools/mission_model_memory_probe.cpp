#include "dfre/mission_model_memory.hpp"
#include "dfre/mission_item_resource.hpp"
#include "dfre/mission_object_helo_pose.hpp"
#include "dfre/mission_live.hpp"
#include "dfre/model_specials.hpp"
#include "dfre/model_visibility.hpp"
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
using namespace dfre;using U=std::uint32_t;
namespace {
AssetBytes decode(const std::string& hex){AssetBytes result;for(std::size_t i=0;i<hex.size();i+=2)result.push_back(std::uint8_t(std::stoul(hex.substr(i,2),nullptr,16)));return result;}
std::int32_t si(U n){std::int32_t value;std::memcpy(&value,&n,4);return value;}
constexpr U A=0x7dde80,D=0x1300000,F=0x1b000000,O=0x1b000100,S=0x1b000200;
}
int main(){try{U mode,phase,first,mutation;std::string rawhex,actorhex,framehex,itemhex,outhex;while(std::cin>>mode>>phase>>first>>mutation>>rawhex>>actorhex>>framehex>>itemhex>>outhex){
 auto raw=decode(rawhex);Model current=decode_model(raw),second=decode_model(raw);std::array<U,5> seed{{0x12345678,0x23456789,0x3456789a,0x456789ab,0x56789abc}};
 MissionModelMemory memory([&](std::string_view name)->const Model&{return name=="fixture.3di"?current:second;},[&](std::string_view){return raw;},{{0x71000000,"fixture.3di",phase?MissionModelCallbackSeed::post_initialized(seed):MissionModelCallbackSeed::pre_producer()},{0x71000001,"second.3di",MissionModelCallbackSeed::pre_producer()}});
 std::unordered_map<U,std::uint8_t> bytes;for(auto input:std::vector<std::pair<U,AssetBytes>>{{A,decode(actorhex)},{D,decode(itemhex)},{F,decode(framehex)},{O,decode(outhex)}})for(U i=0;i<input.second.size();++i)bytes.emplace(input.first+i,input.second[i]);for(U i=0;i<4;++i)bytes.emplace(S+i,0);
 const auto raw_get=[&](U at,U width=4){U n=0;for(U i=0;i<width;++i)n|=U(bytes.at(at+i))<<(i*8);return n;};const auto raw_put=[&](U at,U width,U n){for(U i=0;i<width;++i)bytes.at(at+i)=std::uint8_t(n>>(i*8));};
 std::ostringstream trace;unsigned calls=0,stores=0;bool failed=false;
 const auto read=[&](U at,U width=4){U n=0;if(memory.claims(at,width))n=memory.read(at,width);else if(at==0xc23810)n=D;else if(at==0xc23814)n=1;else if(at==0x503138)n=1;else if(at==0x503228)n=0x4960552;else{n=raw_get(at,width);if(at==A+0x10||at==D+0x6c||at==D+0x70)n=memory.source(n);}trace<<'R'<<at<<','<<width<<','<<n<<';';return n;};
 const auto write=[&](U at,U width,U n){trace<<'W'<<at<<','<<width<<','<<n<<';';if(memory.claims(at,width))memory.write(at,width,n);else raw_put(at,width,at==D+0x6c||at==D+0x70?memory.handle(n):n);++stores;if((mutation&8)&&stores==3)throw MissionModelMemoryError("Fixture store stopped");};
 std::function<U(U,const std::vector<U>&)> call=[&](U site,const std::vector<U>& args)->U{trace<<'C'<<site;for(U n:args)trace<<','<<n;trace<<';';++calls;if((mutation&16)&&calls==2)throw MissionModelMemoryError("Fixture service stopped");
  if(site==0x486530){raw_put(S,4,first);return memory.source(0x71000000);}
  if(site==0x4818f0||site==0x481a00){const auto* tag=find_model_callback_tag(args.at(0));if(!tag)tag=&model_callback_tags.front();return site==0x4818f0?tag->draw_address:tag->matrix_address;}
  if(site==0x45d1e0){const auto p=args.at(1);const auto matrix=compose_pose({si(raw_get(p)),si(raw_get(p+4)),si(raw_get(p+8))},{raw_get(p+12),raw_get(p+16),raw_get(p+20)});for(U i=0;i<12;++i)raw_put(args.at(0)+i*4,4,U(matrix.m[i]));if(mutation&1){raw_put(A+0xc4,4,0x40000000);current.lods[0].hardpoints.at(4)[1]=INT32_MIN;}return 0;}
  if(site==0x45d400){const auto p=args.at(1);const auto matrix=model_special_pivot_matrix({si(raw_get(p)),si(raw_get(p+4)),si(raw_get(p+8))},{si(raw_get(p+12)),si(raw_get(p+16)),si(raw_get(p+20))});for(U i=0;i<12;++i)raw_put(args.at(0)+i*4,4,U(matrix[i]));return 0;}
  if(site==0x45cb40){Matrix4 left,right;for(U i=0;i<12;++i){left.m[i]=si(raw_get(args.at(0)+i*4));right.m[i]=si(raw_get(args.at(1)+i*4));}const auto matrix=matrix_multiply(left,right);for(U i=0;i<12;++i)raw_put(args.at(2)+i*4,4,U(matrix.m[i]));return 0;}
  if(site==0x45cda0){Matrix4 matrix;for(U i=0;i<12;++i)matrix.m[i]=si(raw_get(args.at(0)+i*4));const auto p=args.at(1);const auto point=transform_point(matrix,{si(raw_get(p)),si(raw_get(p+4)),si(raw_get(p+8))});raw_put(args.at(2),4,U(point.x));raw_put(args.at(2)+4,4,U(point.y));raw_put(args.at(2)+8,4,U(point.z));return 0;}
  throw MissionModelMemoryError("Fixture service is not owned");
 };
 try{if(!mode){MissionItemResourceBindings b{read,write,call};mission_item_resource_load(b,0,S);}else if(mode<=2){MissionObjectHeloBindings b{read,write,call};if(mode==1)mission_object_helo_smoke_pose(b,A,O,F,0x80);else mission_object_helo_weapon_pose(b,A,O,F,0x80);}else{
  const U graphic=memory.source(0x71000000);for(U offset=0x3c;offset<0x50;++offset)write(graphic+offset,1,mutation+offset);for(U width:{1u,2u,4u})for(U offset=0x3c;offset+width<=0x50;++offset)(void)read(graphic+offset,width);
 }}catch(const std::exception&){failed=true;}
 std::uint64_t hash=1469598103934665603ull;const auto hash_byte=[&](U value){hash^=value&255;hash*=1099511628211ull;};for(auto region:std::vector<std::pair<U,U>>{{A,0x114},{D,0x13c},{F,mode==1?0x40u:0x80u},{O,0x18},{S,4}})for(U i=0;i<region.second;++i)hash_byte(bytes.at(region.first+i));for(U i=0;i<20;++i)hash_byte(memory.read(memory.source(0x71000000)+0x3c+i,1));for(const auto& hp:current.lods[0].hardpoints)for(auto value:hp)for(U i=0;i<4;++i)hash_byte(U(value)>>(8*i));
 std::cout<<failed<<' '<<hash<<' '<<trace.str()<<'\n';
}return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
