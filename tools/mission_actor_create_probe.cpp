#include "dfre/mission_actor_create.hpp"
#include <array>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
using namespace dfre;using U=std::uint32_t;
int main(){try{std::string line;while(std::getline(std::cin,line)){
    U seed,face,side,count;std::istringstream in(line);in>>seed>>face>>side>>count;std::unordered_map<U,std::uint8_t> memory;std::vector<std::array<U,3>> initial;
    auto put=[&](U at,U width,U n){for(U i=0;i<width;++i)memory[at+i]=std::uint8_t(n>>(8*i));};
    auto get=[&](U at,U width){U n=0;for(U i=0;i<width;++i){const auto p=memory.find(at+i);if(p!=memory.end())n|=U(p->second)<<(8*i);}return n;};
    for(U i=0;i<count;++i){std::array<U,3> row;if(!(in>>row[0]>>row[1]>>row[2]))throw MissionActorCreateError("Short actor-create fixture");put(row[0],row[1],row[2]);initial.push_back(row);}
    U actor=0,definition=0;std::ostringstream trace;MissionActorCreateBindings b;
    b.read=[&](U at,U width){const auto value=get(at,width);trace<<'R'<<at<<','<<width<<','<<value<<';';return value;};
    b.write=[&](U at,U width,U n){trace<<'W'<<at<<','<<width<<','<<n<<';';put(at,width,n);if(at==actor+0x14&&(seed&2))put(definition+0x6c,4,0xaabbccdd);if(at==actor+0x18&&(seed&4))put(definition+0x84,4,0x45454545);};
    b.call=[&](U site,const std::vector<U>& args)->U{
        trace<<'C'<<site;for(auto n:args)trace<<','<<n;trace<<';';
        if(site==0x4828c0){if(seed&1)put(0xc23810,4,0x1301000);return (seed>>16)%3;}
        if(site==0x49a620){actor=args.at(0);definition=get(0xc23810,4)+((seed>>16)%3)*0x13c;for(U i=0;i<0x114;++i)put(actor+i,1,0);if(seed&8)put(0x8cfa18,4,99);return actor;}
        if(site==0x42cce0){if(seed&16)throw MissionActorCreateError("Fixture CHR fault");if(seed&32)put(0xc23810,4,0x1302000);if(seed&64)put(definition+0x94,2,0xffff);return 0x55550000+seed;}
        if(site==0x44a0c0){if(seed&128)throw MissionActorCreateError("Fixture person fault");if(seed&256)put(actor+0x18,4,0x66660000);if(seed&512)put(0x67f549,1,1);return 0x794680;}
        throw MissionActorCreateError("Unexpected actor-create service");
    };
    bool failed=false;U result=0;try{result=mission_actor_create(b,face,side);}catch(const MissionActorCreateError&){failed=true;}trace<<'F'<<failed<<';'<<'O'<<result<<';';
    for(const auto& row:initial)trace<<'V'<<row[0]<<','<<row[1]<<','<<get(row[0],row[1])<<';';
    std::uint64_t hash=1469598103934665603ull;for(U i=0;i<256*0x114;++i){hash^=get(0x7cca80+i,1);hash*=1099511628211ull;}trace<<'H'<<hash<<';';std::cout<<trace.str()<<'\n';
}}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
