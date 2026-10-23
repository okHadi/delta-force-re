#include "dfre/mission_character_allocate.hpp"
#include <iostream>
#include <map>
using namespace dfre;using U=std::uint32_t;
namespace {
void check(bool n){if(!n)throw MissionCharacterAllocateError("Character allocation check failed");}
template<class F>void fails(F f){bool failed=false;try{f();}catch(const MissionCharacterAllocateError&){failed=true;}check(failed);}
std::array<std::uint8_t,mission_character_source_bytes> cold(){std::array<std::uint8_t,mission_character_source_bytes> image{};for(auto pair:std::array<std::pair<U,U>,3>{{{0x436ea4,0x19010},{0x436ea8,0x66d564},{0x436eac,0x69683c}}})for(U i=0;i<4;++i)image[pair.first-mission_character_instance_base+i]=std::uint8_t(pair.second>>(8*i));return image;}
}
int main(){try{
    auto image=cold();MissionCharacterAllocateState state(image.data(),image.size());
    check(state.read(0x436ea4,4)==0x19010&&state.read(0x436ea8,4)==0x66d564&&state.read(0x436eac,4)==0x69683c);
    auto last=state.instance(255);check(last.source_address==0x4397c4&&last.length==0x9c);last.bytes[4]=0xa5;check(state.read(last.source_address+4,1)==0xa5);
    state.write(0x43985f,2,0xabcd);check(state.read(0x43985f,1)==0xcd&&state.read(0x439860,1)==0xab);state.write(0x439860,1,0);
    fails([&]{state.claims(0x42fc5f,2);});fails([&]{state.claims(0x43995f,2);});fails([&]{state.claims(0xffffffff,2);});fails([&]{state.read(0x439960,1);});fails([&]{state.instance(256);});
    image[0]=1;fails([&]{MissionCharacterAllocateState invalid(image.data(),image.size());});image[0]=0;
    std::map<U,U> other{{0x1300060,0x1400000},{0x2007fe8,0},{0x2007fec,0}};MissionCharacterAllocateBindings b;bool fault=false;unsigned calls=0;
    b.read=[&](U at,U width){if(state.claims(at,width))return state.read(at,width);const auto p=other.find(at);if(p==other.end()||width!=4)throw MissionCharacterAllocateError("Character fixture read has no owner");return p->second;};
    b.write=[&](U at,U width,U n){if(state.claims(at,width)){state.write(at,width,n);return;}check(width==4&&other.count(at));other[at]=n;};
    b.call=[&](U site,MissionCharacterAllocateRegisters& r){++calls;check(r.esi>=0x42fc60&&r.esi<0x439860);const auto instance=r.esi;
        if(site==0x439e48){check(r.edi==0x1400000);state.write(instance,4,r.edi);r.ecx=100;r.edx=101;r.esi=102;r.edi=103;r.ebp=104;if(fault)throw MissionCharacterAllocateError("Character init fixture fault");}
        else if(site==0x43a0e0){check(r.eax==0&&r.ebx==0&&r.edi==103&&r.ecx==100);state.write(instance+0x10,4,7);r.esi=202;r.edi=203;}
        else if(site==0x43a150){check(r.eax==0&&r.ebx==0&&r.edi==203);state.write(instance+0x0c,4,9);r.eax=300;r.ebx=301;r.esi=302;}
        else throw MissionCharacterAllocateError("Unknown character init body");
    };
    MissionCharacterAllocateRegisters r{11,22,33,44,55,0x1400000,77};mission_character_allocate_raw(b,r);check(r.eax==0x42fc60&&r.ebx==0&&r.ecx==33&&r.edx==44&&r.esi==55&&r.edi==0x1400000&&r.ebp==77&&calls==3);check(state.read(0x439860,1)==1&&state.read(0x42fc6c,4)==9);
    r={11,22,33,44,55,66,77};check(mission_character_allocate(b,0x1300000,r,0x2007ff0)==0x42fcfc&&r.ebx==22&&r.ecx==33&&r.edi==66&&r.ebp==77&&other[0x2007fec]==1&&calls==6);
    fault=true;r={11,22,33,44,55,0x1400000,77};fails([&]{mission_character_allocate_raw(b,r);});check(state.read(0x439862,1)==1&&r.ecx==100&&r.edi==103&&r.ebp==104);
    b.read={};b.write={};b.call={};r={11,22,33,44,55,66,77};check(mission_character_allocate(b,0,r,0)==0&&r.eax==0&&r.ebx==22&&r.edi==66&&r.ebp==77);
    b.read=[&](U at,U width){return state.read(at,width);};b.write=[&](U at,U width,U n){state.write(at,width,n);};
    r={11,22,33,44,55,0x1400000,77};fails([&]{mission_character_allocate_raw(b,r);});check(state.read(0x439863,1)==1);
    for(U i=0;i<256;++i)state.write(0x439860+i,1,1);r={11,22,33,44,55,66,77};mission_character_allocate_raw(b,r);check(r.eax==0&&r.ebx==256&&r.edi==66&&r.ebp==77);
    std::cout<<"Shared character byte owner and register allocation checks passed; initialization is an explicit fixture\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
