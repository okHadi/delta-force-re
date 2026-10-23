#include "dfre/mission_object_helo_update.hpp"
#include "dfre/actor_behavior.hpp"
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
using namespace dfre;using U=std::uint32_t;
int main(){try{std::string line;while(std::getline(std::cin,line)){
    U message,host,seed,mutation,search,condition,patrol,distance,height,damage;std::string actor,person,other,definition,locals;std::istringstream in(line);
    if(!(in>>message>>host>>seed>>mutation>>search>>condition>>patrol>>distance>>height>>damage>>actor>>person>>other>>definition>>locals))throw MissionTickError("Short helicopter update fixture");
    constexpr U a=0x7dde80,p=0x794680,d=0x1302000,f=0x1304000;std::unordered_map<U,std::uint8_t> bytes;
    const auto load=[&](U at,const std::string& hex){for(std::size_t i=0;i<hex.size();i+=2)bytes[at+U(i/2)]=std::uint8_t(std::stoul(hex.substr(i,2),nullptr,16));};
    load(a,actor);load(p,person);load(p+0xb4,other);load(d,definition);load(f,locals);
    const auto get=[&](U at,U width){U n=0;for(U i=0;i<width;++i)n|=U(bytes[at+i])<<(8*i);return n;};
    const auto put=[&](U at,U width,U n){for(U i=0;i<width;++i)bytes[at+i]=std::uint8_t(n>>(8*i));};
    put(0x67f528,4,host);put(0xa158c0,4,damage);BehaviorState random;random.rng=seed;std::ostringstream trace;MissionObjectHeloBindings b;
    b.read=[&](U at,U width){const auto n=get(at,width);trace<<'R'<<at<<','<<width<<','<<n<<';';return n;};
    b.write=[&](U at,U width,U n){if((width==1&&n>0xff)||(width==2&&n>0xffff))throw MissionTickError("Store exceeds source width");trace<<'W'<<at<<','<<width<<','<<n<<';';put(at,width,n);
        if((mutation&1)&&at==a+0xd4)put(0xa158c0,4,6);
        if((mutation&128)&&at==p+0x40)put(a+0xfc,4,57);
        if((mutation&256)&&at==a+0x20)throw MissionTickError("Fixture store stopped");
    };
    b.call=[&](U site,const std::vector<U>& args){trace<<'C'<<site;for(auto n:args)trace<<','<<n;trace<<';';
        if((mutation&512)&&site==0x486fd0)throw MissionTickError("Fixture ground stopped");
        if(site==0x477860)return behavior_rng_next(random);
        if(site==0x49a620){for(U i=0;i<args.at(2);++i)put(args.at(0)+i,1,args.at(1));return args.at(0);}
        if(site==0x4505d0){put(args.at(1),4,0x10000);put(args.at(1)+4,4,0x20000);put(args.at(1)+8,4,0x30000);if(mutation&2){put(f+0x40,4,1);put(f+0x3c,4,0);}return 0u;}
        if(site==0x43ef80){put(a+0x1c,4,0x6e0700);if(mutation&4)put(a+0xc,4,p+0xb4);return 0u;}
        if(site==0x43f020){put(a+0x1c,4,0);return 0u;}
        if(site==0x450c40){if(mutation&4)put(a+0xc,4,p+0xb4);return search;}
        if(site==0x450ae0)return condition;
        if(site==0x4509f0)return patrol;
        if(site==0x4509b0)return distance;
        if(site==0x486fd0)return height;
        if(site==0x42a440){if(mutation&64)put(a+0xfc,4,32);return 0u;}
        if(site==0x450100){if(mutation&8){put(a+0xa8,4,0);put(a+0x98,4,0);}return 0u;}
        if(site==0x4503f0||site==0x450710||site==0x450d50){if(mutation&16)put(a+0xa8,4,0);return 0u;}
        throw MissionTickError("Unknown helicopter update fixture call");
    };
    bool failed=false;try{mission_object_helo_update(b,a,message,f,0x44);}catch(const MissionTickError&){failed=true;}
    std::uint64_t hash=1469598103934665603ull;for(const auto& region:{std::pair<U,U>{a,0x114},{p,0xb4*2},{d,0x13c},{f,0x44}})for(U i=0;i<region.second;++i){hash^=get(region.first+i,1);hash*=1099511628211ull;}
    std::cout<<failed<<' '<<random.rng<<' '<<hash<<' '<<trace.str()<<'\n';
}return 0;}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
