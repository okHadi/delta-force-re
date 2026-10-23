#include "dfre/mission_object_mine_update.hpp"
#include <iostream>
#include <sstream>
#include <map>
using namespace dfre;using U=std::uint32_t;
int main(){try{std::string line;while(std::getline(std::cin,line)){
 U sine,cosine,mutation,fail_at;std::string hex;std::istringstream in(line);if(!(in>>sine>>cosine>>mutation>>fail_at>>hex)||hex.size()!=0x238)throw MissionTickError("Short mine update fixture");
 constexpr U a=0x7dde80,f=0x1300000;std::map<U,std::uint8_t> bytes;for(U i=0;i<0x114;++i)bytes[a+i]=std::uint8_t(std::stoul(hex.substr(i*2,2),nullptr,16));for(U i=0;i<8;++i)bytes[f+i]=std::uint8_t(std::stoul(hex.substr((i+0x114)*2,2),nullptr,16));
 const auto put=[&](U at,U width,U n){for(U i=0;i<width;++i)bytes.at(at+i)=std::uint8_t(n>>(8*i));};
 U accesses=0;std::ostringstream trace;MissionObjectMineUpdateBindings b;
 b.read=[&](U at,U width){U n=0;for(U i=0;i<width;++i)n|=U(bytes.at(at+i))<<(8*i);trace<<'R'<<at<<','<<width<<','<<n<<';';if(++accesses==fail_at)throw MissionTickError("Fixture stopped");return n;};
 b.write=[&](U at,U width,U n){trace<<'W'<<at<<','<<width<<','<<n<<';';put(at,width,n);if((mutation&2)&&at==a+0x3c){put(a+0x7c,4,0x80000000);put(f,4,0x7fffffff);}if(++accesses==fail_at)throw MissionTickError("Fixture stopped");};
 b.call=[&](U site,const std::vector<U>& args){trace<<'C'<<site;for(auto n:args)trace<<','<<n;trace<<';';if(site==0x477820){put(f,4,sine);put(f+4,4,cosine);if(mutation&1)put(a+0x7c,4,0x7fffffff);if(mutation&8)throw MissionTickError("Fixture stopped");}
 else if(site==0x45da30){if(mutation&4)for(U i=0;i<0x114;++i)bytes.at(a+i)=0;if(mutation&16)throw MissionTickError("Fixture stopped");}else throw MissionTickError("Unknown fixture body");return 0u;};
 bool failed=false;try{mission_object_mine_update(b,a,f);}catch(const MissionTickError&){failed=true;}
 std::uint64_t hash=1469598103934665603ull;for(const auto& r:{std::pair<U,U>{a,0x114},{f,8}})for(U i=0;i<r.second;++i){hash^=bytes.at(r.first+i);hash*=1099511628211ull;}std::cout<<failed<<' '<<hash<<' '<<trace.str()<<'\n';
}return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
