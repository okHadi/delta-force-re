#include "dfre/mission_client_admission_helpers.hpp"
#include <array>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
using namespace dfre;using U=std::uint32_t;
int main(){try{std::string line;while(std::getline(std::cin,line)){
    std::istringstream in(line);U op,seed,arg1,arg2,count;in>>op>>seed>>arg1>>arg2>>count;
    std::unordered_map<U,std::uint8_t> memory;std::vector<std::array<U,3>> initial;
    auto put=[&](U at,U width,U n){for(U i=0;i<width;++i)memory[at+i]=std::uint8_t(n>>(8*i));};
    auto get=[&](U at,U width){U n=0;for(U i=0;i<width;++i){const auto found=memory.find(at+i);if(found!=memory.end())n|=U(found->second)<<(8*i);}return n;};
    for(U i=0;i<count;++i){std::array<U,3> row;if(!(in>>row[0]>>row[1]>>row[2]))throw MissionClientAddedError("Short admission-helper fixture");put(row[0],row[1],row[2]);initial.push_back(row);}
    unsigned name_reads=0,reference_reads=0;std::ostringstream trace;MissionClientAddedBindings b;
    b.read=[&](U at,U width){
        if(at==0x1300100&&name_reads++==0&&(seed&1))put(at+1,1,'B');
        if(at==0x6e9c60&&reference_reads++==1&&(seed&4))put(at,1,0);
        const auto value=get(at,width);trace<<'R'<<at<<','<<width<<','<<value<<';';
        if(at==0x501398&&(seed&8))put(value+0x6c,4,0x410100);
        return value;
    };
    b.write=[&](U at,U width,U value){trace<<'W'<<at<<','<<width<<','<<value<<';';put(at,width,value);if(at==0x78ee55&&(seed&2))put(0x1300200,1,'Z');if(at==0x78ee56&&(seed&16))throw MissionClientAddedError("Fixture store fault");};
    b.call=[&](U site,const std::vector<U>& args){trace<<'C'<<site;for(auto v:args)trace<<','<<v;trace<<';';if(seed&32)throw MissionClientAddedError("Fixture call fault");return site+args.at(0);};
    bool failed=false;U result=0;try{if(op==0)mission_client_clear_references(b,arg1);else if(op==1)result=mission_client_name_message(b,arg1,arg2);else result=mission_client_peer_name(b,arg1);}catch(const MissionClientAddedError&){failed=true;}
    trace<<'F'<<failed<<';';if(op)trace<<'O'<<result<<';';for(const auto& row:initial)trace<<'V'<<row[0]<<','<<row[1]<<','<<get(row[0],row[1])<<';';
    std::uint64_t hash=1469598103934665603ull;for(U i=0;i<0x800;++i){hash^=get(0x78ee54+i,1);hash*=1099511628211ull;}trace<<'H'<<hash<<';';std::cout<<trace.str()<<'\n';
}}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
