#include "dfre/mission_character_init.hpp"
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
using namespace dfre;using U=std::uint32_t;
int main(){try{std::string line;while(std::getline(std::cin,line)){
    U op,seed,instance,frame,bones,count,cold_values[3];std::istringstream in(line);in>>op>>seed>>instance>>frame>>bones;for(auto& n:cold_values)in>>n;in>>count;
    std::array<std::uint8_t,mission_character_source_bytes> cold{};for(U k=0;k<3;++k)for(U i=0;i<4;++i)cold[0x436ea4-mission_character_instance_base+k*4+i]=std::uint8_t(cold_values[k]>>(i*8));MissionCharacterAllocateState state(cold.data(),cold.size());std::unordered_map<U,std::uint8_t> other;
    auto put=[&](U at,U width,U n){if(state.claims(at,width)){state.write(at,width,n);return;}for(U i=0;i<width;++i)other[at+i]=std::uint8_t(n>>(i*8));};
    auto get=[&](U at,U width){if(state.claims(at,width))return state.read(at,width);U n=0;for(U i=0;i<width;++i){const auto p=other.find(at+i);if(p!=other.end())n|=U(p->second)<<(i*8);}return n;};
    for(U i=0;i<count;++i){U at,width,n;if(!(in>>at>>width>>n))throw MissionCharacterAllocateError("Short character-init fixture");put(at,width,n);}
    std::ostringstream trace;MissionCharacterAllocateBindings b;
    b.read=[&](U at,U width){if(seed&32)if(at==0x1300214||at==0x1300248)throw MissionCharacterAllocateError("Fixture read fault");if(at==instance+0x58&&(seed&1))put(instance+0x80,4,0x1301800);if(at==0x130024a&&(seed&8))put(0x1300248,1,99);const auto n=get(at,width);trace<<'R'<<at<<','<<width<<','<<n<<';';return n;};
    b.write=[&](U at,U width,U n){trace<<'W'<<at<<','<<width<<','<<n<<';';put(at,width,n);if(at==0x1301000&&(seed&2))put(0x130024c,1,77);if(at==instance+0x0c&&(seed&4))put(0x1300214,4,0x90);if(seed&16)if(at==0x1301004||at==0x1301804||at==instance+0x28)throw MissionCharacterAllocateError("Fixture store fault");};
    b.call=[&](U site,MissionCharacterAllocateRegisters& r){trace<<'C'<<site;for(auto n:{r.eax,r.ebx,r.ecx,r.edx,r.esi,r.edi,r.ebp})trace<<','<<n;trace<<';';put(r.esi+0x10,4,r.edi);r={101,102,103,104,105,106,107};if(seed&64)throw MissionCharacterAllocateError("Fixture decoder fault");};
    MissionCharacterAllocateRegisters r{0,frame,33,44,instance,55,66};bool failed=false,selected=false;
    try{if(op==0)mission_character_clear_scalars(b,r);else if(op==1)mission_character_seed_pose(b,r);else selected=mission_character_select_action(b,r);}catch(const MissionCharacterAllocateError&){failed=true;}
    trace<<'F'<<failed<<';'<<'O'<<selected<<';';for(auto n:{r.eax,r.ebx,r.ecx,r.edx,r.esi,r.edi,r.ebp})trace<<'G'<<n<<';';
    std::uint64_t hash=1469598103934665603ull;for(U i=0;i<mission_character_source_bytes;++i){hash^=state.read(mission_character_instance_base+i,1);hash*=1099511628211ull;}for(U i=0;i<0x2000;++i){hash^=get(0x1300000+i,1);hash*=1099511628211ull;}trace<<'H'<<hash<<';';std::cout<<trace.str()<<'\n';
}}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
