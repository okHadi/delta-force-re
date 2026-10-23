#include "dfre/mission_object_helo_shot.hpp"
#include "dfre/actor_queries.hpp"
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
using namespace dfre;using U=std::uint32_t;
namespace {std::int32_t si(U n){std::int32_t s;std::memcpy(&s,&n,4);return s;}}
int main(){try{std::string line;while(std::getline(std::cin,line)){
    U math,mutation,seed,yaw,pitch,distance;std::string actor,person,other,definition,frame;std::istringstream in(line);
    if(!(in>>math>>mutation>>seed>>yaw>>pitch>>distance>>actor>>person>>other>>definition>>frame))throw MissionTickError("Short helicopter shot fixture");
    constexpr U a=0x7dde80,p=0x794680,d=0x1302000,f=0x1304000;std::unordered_map<U,std::uint8_t> bytes;
    const auto load=[&](U at,const std::string& hex){for(std::size_t i=0;i<hex.size();i+=2)bytes[at+U(i/2)]=std::uint8_t(std::stoul(hex.substr(i,2),nullptr,16));};
    load(a,actor);load(p,person);load(p+0xb4,other);load(d,definition);load(f,frame);
    const auto get=[&](U at){U n=0;for(U i=0;i<4;++i)n|=U(bytes[at+i])<<(8*i);return n;};
    const auto put=[&](U at,U n){for(U i=0;i<4;++i)bytes[at+i]=std::uint8_t(n>>(8*i));};
    std::ostringstream trace;BehaviorState random;random.rng=seed;unsigned angles=0,draws=0;MissionObjectHeloBindings b;
    b.read=[&](U at,U width){if(width!=4)throw MissionTickError("Shot read width");const auto n=get(at);trace<<'R'<<at<<",4,"<<n<<';';if((mutation&16)&&at==a+4){put(f+0xc,0x12345678);put(a+0xb8,123);}return n;};
    b.write=[&](U at,U width,U n){if(width!=4)throw MissionTickError("Shot write width");trace<<'W'<<at<<",4,"<<n<<';';put(at,n);
        if((mutation&4)&&at==f+0xc)put(f+0x18,0x12345678);
        if((mutation&8)&&at==f+0x10){put(a+0x34,0x30000000);put(a+0x30,0x20000000);}
        if((mutation&512)&&at==f+0x10)throw MissionTickError("Fixture store stopped");
    };
    b.call=[&](U site,const std::vector<U>& args){trace<<'C'<<site;for(auto n:args)trace<<','<<n;trace<<';';
        if(site==0x450630){if(mutation&128)throw MissionTickError("Fixture pose stopped");if(mutation&1)put(a+0xc,p+0xb4);if(mutation&2)put(a+0x94,7);return 0u;}
        if(site==0x477620)return math?query_isqrt_477620(args.at(0)):distance;
        if(site==0x477770){++angles;return math?query_angle_477770(si(args.at(0)),si(args.at(1))):angles==1?yaw:pitch;}
        if(site==0x477860){++draws;if(mutation&256)throw MissionTickError("Fixture random stopped");if(mutation&32){if(draws==1){put(f+0xc,get(f+0xc)+0x10000);put(a+0x94,7);}else put(f+0x10,get(f+0x10)-0x10000);}return behavior_rng_next(random);}
        if(site==0x45ea00){trace<<'D'<<get(a+0x94)<<','<<get(a+0xc);for(U i=0;i<6;++i)trace<<','<<get(f+i*4);trace<<';';if(mutation&64)put(a+0x34,0xabcdef01);return 0u;}
        throw MissionTickError("Unknown helicopter shot fixture body");
    };
    bool failed=false;try{mission_object_helo_shot(b,a,f,0x1c);}catch(const MissionTickError&){failed=true;}
    std::uint64_t hash=1469598103934665603ull;for(const auto& region:{std::pair<U,U>{a,0x114},{p,0xb4*2},{d,0x13c},{f,0x1c}})for(U i=0;i<region.second;++i){hash^=bytes[region.first+i];hash*=1099511628211ull;}
    std::cout<<failed<<' '<<random.rng<<' '<<hash<<' '<<trace.str()<<'\n';
}return 0;}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
