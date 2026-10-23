#include "dfre/mission_object_helo_condition.hpp"
#include "dfre/actor_queries.hpp"
#include "dfre/world_hits.hpp"
#include <cstring>
#include <iostream>
#include <sstream>
#include <unordered_map>
using namespace dfre;using U=std::uint32_t;
namespace {std::int32_t si(U n){std::int32_t s;std::memcpy(&s,&n,4);return s;}constexpr U A=0x7dde80,P=0x794680,M=0x8ace80,L=0x78f280,S=0x793600,T=0x867e80,F=0x1304000;constexpr std::pair<U,U> regions[]={{A,0x114},{P,0x168},{M,0x33c},{L,0x220},{S,0x300},{T,0x114},{F,16}};}
int main(){try{std::string line;while(std::getline(std::cin,line)){
 U mode,mutation,angle,distance,radius,yaw;std::string hex;std::istringstream in(line);if(!(in>>mode>>mutation>>angle>>distance>>radius>>yaw>>hex))throw MissionTickError("Short condition fixture");
 std::unordered_map<U,std::uint8_t> bytes;std::size_t pos=0;for(auto region:regions)for(U i=0;i<region.second;++i){bytes[region.first+i]=std::uint8_t(std::stoul(hex.substr(pos,2),nullptr,16));pos+=2;}
 const auto get=[&](U at,U width=4){U n=0;for(U i=0;i<width;++i)n|=U(bytes.at(at+i))<<(8*i);return n;};
 const auto put=[&](U at,U n){for(U i=0;i<4;++i)bytes.at(at+i)=std::uint8_t(n>>(8*i));};
 bool armed_read=false;std::ostringstream trace;MissionObjectHeloBindings b;
 b.read=[&](U at,U width){U n=get(at,width);trace<<'R'<<at<<','<<width<<','<<n<<';';if(!mode&&!armed_read&&(mutation&128)&&at==P+0xa4&&!n){armed_read=true;put(P+0xa4,1);}return n;};
 b.write=[&](U at,U width,U n){if(width!=4)throw MissionTickError("Fixture write width");trace<<'W'<<at<<",4,"<<n<<';';put(at,n);if((mutation&256)&&at==A+0x8c)put(A+0x70,3u<<16);if((mutation&16)&&at==A+0x8c)throw MissionTickError("Fixture store stopped");};
 b.call=[&](U site,const std::vector<U>& args)->U{trace<<'C'<<site;for(U n:args)trace<<','<<n;trace<<';';
  if(site==0x45c6a0){U result=mission_object_helo_marker(b,args.at(0),args.at(1),args.at(2),args.at(3),args.at(4));if(mutation&32)put(P+0xa4,0);return result;}
  if(site==0x42ee30){if(mutation&4){put(P+0xac,2);put(P+0xb0,1);put(A+0xc,P+0xb4);}if(mutation&64)throw MissionTickError("Fixture event stopped");return 0;}
  if(site==0x477770||site==0x477670){
   if(mutation&1)put(A+0xc,P+0xb4);
   if(mutation&2){put(M+0x7c,0x12345);put(M+0x30,0x12345678);put(A+0x70,2u<<16);}
   if(mutation&8)throw MissionTickError("Fixture math stopped");
   return site==0x477770?query_angle_477770(si(args.at(0)),si(args.at(1))):U(collision_approximate_length(si(args.at(0)),si(args.at(1)),si(args.at(2))));
  }throw MissionTickError("Fixture body is not owned");
 };
 bool failed=false;U result=0;try{result=mode?mission_object_helo_marker(b,A,angle,distance,radius,yaw):mission_object_helo_condition(b,A,F,16);}catch(const MissionTickError&){failed=true;}
 std::uint64_t hash=1469598103934665603ull;for(auto region:regions)for(U i=0;i<region.second;++i){hash^=bytes.at(region.first+i);hash*=1099511628211ull;}
 std::cout<<failed<<' '<<result<<' '<<hash<<' '<<trace.str()<<'\n';
}return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
