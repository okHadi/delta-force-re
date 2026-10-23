#include "dfre/mission_person_allocate.hpp"
#include <array>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
using namespace dfre;using U=std::uint32_t;
int main(){try{std::string line;while(std::getline(std::cin,line)){
    U seed,owner,count;std::istringstream in(line);in>>seed>>owner>>count;std::unordered_map<U,std::uint8_t> bytes;std::vector<std::array<U,3>> initial;
    auto put=[&](U at,U width,U n){for(U i=0;i<width;++i)bytes[at+i]=std::uint8_t(n>>(i*8));};
    auto get=[&](U at,U width){U n=0;for(U i=0;i<width;++i){const auto p=bytes.find(at+i);if(p!=bytes.end())n|=U(p->second)<<(i*8);}return n;};
    for(U i=0;i<count;++i){std::array<U,3> row;if(!(in>>row[0]>>row[1]>>row[2]))throw MissionActorCreateError("Short person fixture");put(row[0],row[1],row[2]);initial.push_back(row);}
    std::ostringstream trace;U selected=0;MissionActorCreateBindings b;
    b.read=[&](U at,U width){const auto n=get(at,width);trace<<'R'<<at<<','<<width<<','<<n<<';';return n;};
    b.write=[&](U at,U width,U n){trace<<'W'<<at<<','<<width<<','<<n<<';';put(at,width,n);if(seed&2)throw MissionActorCreateError("Fixture owner-store fault");};
    b.call=[&](U site,const std::vector<U>& args)->U{
        trace<<'C'<<site;for(auto n:args)trace<<','<<n;trace<<';';if(site!=0x49a620)throw MissionActorCreateError("Unexpected person body");selected=args.at(0);for(U i=0;i<args.at(2);++i)put(selected+i,1,0);
        if(seed&4)put(selected,4,0xabcdef01);if(seed&8)put(0x794680,4,0);if(seed&1)throw MissionActorCreateError("Fixture clear fault");return selected;
    };
    bool failed=false;U result=0;try{result=mission_person_allocate(b,owner);}catch(const MissionActorCreateError&){failed=true;}trace<<'F'<<failed<<';'<<'O'<<result<<';';
    for(const auto& row:initial)trace<<'V'<<row[0]<<','<<row[1]<<','<<get(row[0],row[1])<<';';
    std::uint64_t hash=1469598103934665603ull;for(U i=0;i<1280*0xb4;++i){hash^=get(0x794680+i,1);hash*=1099511628211ull;}trace<<'H'<<hash<<';';std::cout<<trace.str()<<'\n';
}}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
