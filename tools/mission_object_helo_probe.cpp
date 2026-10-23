#include "dfre/mission_object_helo.hpp"
#include "dfre/actor_behavior.hpp"
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
using namespace dfre;using U=std::uint32_t;
int main(){try{std::string line;while(std::getline(std::cin,line)){
    U op,desired,host,seed,mutation,height;std::string actor,person,other,definition;std::istringstream in(line);
    if(!(in>>op>>desired>>host>>seed>>mutation>>height>>actor>>person>>other>>definition))throw MissionTickError("Short helicopter fixture");
    constexpr U a=0x7dde80,p=0x794680,d=0x1302000;std::unordered_map<U,std::uint8_t> bytes;
    const auto load=[&](U at,const std::string& hex){for(std::size_t i=0;i<hex.size();i+=2)bytes[at+U(i/2)]=std::uint8_t(std::stoul(hex.substr(i,2),nullptr,16));};
    load(a,actor);load(p,person);load(p+0xb4,other);load(d,definition);
    const auto get=[&](U at,U width){U n=0;for(U i=0;i<width;++i)n|=U(bytes[at+i])<<(8*i);return n;};
    const auto put=[&](U at,U width,U n){for(U i=0;i<width;++i)bytes[at+i]=std::uint8_t(n>>(8*i));};
    put(0x67f528,4,host);BehaviorState random;random.rng=seed;std::ostringstream trace;MissionObjectHeloBindings b;
    b.read=[&](U at,U width){const auto n=get(at,width);trace<<'R'<<at<<','<<width<<','<<n<<';';return n;};
    b.write=[&](U at,U width,U n){if((width==1&&n>0xff)||(width==2&&n>0xffff))throw MissionTickError("Store value exceeds its source width");trace<<'W'<<at<<','<<width<<','<<n<<';';put(at,width,n);
        if((mutation&4)&&at==a+0x9c)put(a+0xc,4,p+0xb4);
        if((mutation&32)&&at==a+0x2c)throw MissionTickError("Fixture store stopped");
    };
    b.call=[&](U site,const std::vector<U>& args){trace<<'C'<<site;for(auto n:args)trace<<','<<n;trace<<';';
        if(site==0x47df40){if(mutation&1){put(a+0xc,4,p+0xb4);put(d+0xdc,4,0x123);}if(mutation&64)throw MissionTickError("Fixture terrain stopped");return height;}
        if(site==0x49a620){for(U i=0;i<args.at(2);++i)put(args.at(0)+i,1,args.at(1));return args.at(0);}
        if(site==0x450d50){if(mutation&2){put(a+0x6c,2,0x700);put(a+0x88,4,0x9000);}return 0u;}
        if(site==0x450830){if(mutation&128)put(p+0x90,4,77);return 0u;}
        if(site==0x477860){if(mutation&16){put(a+0xc,4,p+0xb4);put(p+0x90,4,88);}return behavior_rng_next(random);}
        throw MissionTickError("Unknown helicopter fixture call");
    };
    bool failed=false;try{switch(op){case 0:mission_object_helo_move(b,a);break;case 1:mission_object_helo_turn(b,a);break;case 2:mission_object_helo_speed(b,a,desired);break;case 3:mission_object_helo_integrate(b,a);break;case 4:mission_object_helo_bank(b,a);break;default:throw MissionTickError("Unknown helicopter fixture body");}}
    catch(const MissionTickError&){failed=true;}
    std::uint64_t hash=1469598103934665603ull;for(const auto& region:{std::pair<U,U>{a,0x114},{p,0xb4*2},{d,0x13c}})for(U i=0;i<region.second;++i){hash^=get(region.first+i,1);hash*=1099511628211ull;}
    std::cout<<failed<<' '<<random.rng<<' '<<hash<<' '<<trace.str()<<'\n';
}return 0;}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
