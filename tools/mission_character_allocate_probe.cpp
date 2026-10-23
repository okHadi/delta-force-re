#include "dfre/mission_character_allocate.hpp"
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
using namespace dfre;using U=std::uint32_t;
int main(){try{std::string line;while(std::getline(std::cin,line)){
    U op,seed,resource,count,cold_values[3];std::istringstream in(line);in>>op>>seed>>resource;for(auto& n:cold_values)in>>n;in>>count;
    std::array<std::uint8_t,mission_character_source_bytes> image{};for(U cell=0;cell<3;++cell)for(U i=0;i<4;++i)image[0x436ea4-mission_character_instance_base+cell*4+i]=std::uint8_t(cold_values[cell]>>(8*i));
    MissionCharacterAllocateState state(image.data(),image.size());std::unordered_map<U,std::uint8_t> other;
    auto put=[&](U at,U width,U n){if(state.claims(at,width)){state.write(at,width,n);return;}for(U i=0;i<width;++i)other[at+i]=std::uint8_t(n>>(8*i));};
    auto get=[&](U at,U width){if(state.claims(at,width))return state.read(at,width);U n=0;for(U i=0;i<width;++i){const auto p=other.find(at+i);if(p!=other.end())n|=U(p->second)<<(8*i);}return n;};
    for(U i=0;i<count;++i){U at,width,n;if(!(in>>at>>width>>n))throw MissionCharacterAllocateError("Short character fixture");put(at,width,n);}
    std::ostringstream trace;MissionCharacterAllocateBindings b;unsigned stage=0,frame_reads=0;
    b.read=[&](U at,U width){if(at==0x2007fe8&&frame_reads++==0&&(seed&64))for(U i=0;i<256;++i)state.write(0x439860+i,1,1);const auto n=get(at,width);trace<<'R'<<at<<','<<width<<','<<n<<';';return n;};
    b.write=[&](U at,U width,U n){trace<<'W'<<at<<','<<width<<','<<n<<';';put(at,width,n);if(at==0x2007fe8&&(seed&16))put(at,4,0x1300900);if(at==0x2007fec&&(seed&32))put(0x2007fe8,4,0xdeadbeef);};
    b.call=[&](U site,MissionCharacterAllocateRegisters& r){trace<<'C'<<site;for(auto n:{r.eax,r.ebx,r.ecx,r.edx,r.esi,r.edi,r.ebp})trace<<','<<n;trace<<';';const auto instance=r.esi;state.write(instance+stage*4,4,r.edi);if(seed&1)state.write(0x43995f,1,1);++stage;r={0xa000+stage,0xb000+stage,0xc000+stage,0xd000+stage,0xe000+stage,0xf000+stage,0x10000+stage};if(seed&(1u<<stage))throw MissionCharacterAllocateError("Fixture initialization fault");};
    MissionCharacterAllocateRegisters r{11,22,33,44,55,0x1300800,77};bool failed=false;try{if(op)mission_character_allocate(b,resource,r,0x2007ff0);else mission_character_allocate_raw(b,r);}catch(const MissionCharacterAllocateError&){failed=true;}
    trace<<'F'<<failed<<';';for(auto n:{r.eax,r.ebx,r.ecx,r.edx,r.esi,r.edi,r.ebp})trace<<'G'<<n<<';';
    if(op)trace<<'L'<<get(0x2007fe8,4)<<','<<get(0x2007fec,4)<<';';std::uint64_t hash=1469598103934665603ull;for(U i=0;i<mission_character_source_bytes;++i){hash^=state.read(mission_character_instance_base+i,1);hash*=1099511628211ull;}trace<<'H'<<hash<<';';std::cout<<trace.str()<<'\n';
}}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
