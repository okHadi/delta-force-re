#include "dfre/mission_character_init.hpp"
#include <iostream>
#include <map>
using namespace dfre;using U=std::uint32_t;
namespace {void check(bool n){if(!n)throw MissionCharacterAllocateError("Character setup check failed");}}
int main(){try{
    std::array<std::uint8_t,mission_character_source_bytes> cold{};for(auto cell:std::array<std::pair<U,U>,3>{{{0x436ea4,0x19010},{0x436ea8,0x66d564},{0x436eac,0x69683c}}})for(U i=0;i<4;++i)cold[cell.first-mission_character_instance_base+i]=std::uint8_t(cell.second>>(i*8));
    MissionCharacterAllocateState state(cold.data(),cold.size());std::map<U,std::uint8_t> workspace;for(U i=0;i<0x2000;++i)workspace[0x1300000+i]=0;
    MissionCharacterAllocateBindings b;b.read=[&](U at,U width){if(state.claims(at,width))return state.read(at,width);U n=0;for(U i=0;i<width;++i){const auto p=workspace.find(at+i);if(p==workspace.end())throw MissionCharacterAllocateError("Setup read exceeds current workspace");n|=U(p->second)<<(i*8);}return n;};
    b.write=[&](U at,U width,U n){if(state.claims(at,width)){state.write(at,width,n);return;}for(U i=0;i<width;++i){auto p=workspace.find(at+i);if(p==workspace.end())throw MissionCharacterAllocateError("Setup write exceeds current workspace");p->second=std::uint8_t(n>>(i*8));}};
    auto view=state.instance(0);for(std::size_t i=0;i<view.length;++i)view.bytes[i]=0xa5;
    MissionCharacterAllocateRegisters r{1,2,3,4,view.source_address,5,6};mission_character_clear_scalars(b,r);check(r.eax==1&&r.ebx==2&&r.esi==view.source_address&&state.read(view.source_address+0x50,4)==0xa5a5a5a5&&state.read(view.source_address+0x64,2)==0xa500&&state.read(view.source_address+0x28,4)==0);
    b.write(view.source_address,4,0x1300000);b.write(view.source_address+0x58,4,0x34);b.write(view.source_address+0x80,4,0x1301000);b.write(0x1300014,4,0x1300100);b.write(0x1300100,4,0x1300200);b.write(0x1300204,4,2);b.write(0x1300214,4,0x80);
    for(U i=0;i<8;++i)b.write(0x1300248+i,1,std::array<U,8>{1,128,255,99,5,6,7,88}[i]);
    r={0,0,3,4,view.source_address,5,6};mission_character_seed_pose(b,r);check(b.read(0x1301000,4)==1u<<17&&b.read(0x1301004,4)==128u<<17&&b.read(0x1301008,4)==255u<<17&&b.read(0x130100c,4)==65536&&b.read(0x1301018,4)==5u<<17&&r.edx==0&&r.ebp==0x1301030);
    b.write(0x1300204,4,0);for(U i=0;i<3;++i)b.write(0x13002c8+i,1,9+i);r={0,0x80000000,3,4,view.source_address,5,6};mission_character_seed_pose(b,r);check(r.edx==0xffffffff&&r.edi==0x13002cc&&b.read(0x1301000,4)==9u<<17);
    unsigned calls=0;b.call=[&](U site,MissionCharacterAllocateRegisters& current){check(site==0x43a690&&current.esi==view.source_address&&current.edi==0x1300214&&current.ebp==0x1300100&&current.eax==0&&current.ebx==0);++calls;b.write(current.esi+0x10,4,current.edi);current.eax=123;};
    r={0,0,3,4,view.source_address,5,6};check(mission_character_select_action(b,r)&&calls==1&&r.eax==123&&state.read(view.source_address+0x0c,4)==0x1300200&&state.read(view.source_address+0x66,1)==0);
    b.write(0x1300100,4,0);r={0,0,3,4,view.source_address,5,6};check(!mission_character_select_action(b,r)&&calls==1&&state.read(view.source_address+0x0c,4)==0x1300200);
    b.write(0x1300100,4,0x1300200);b.call={};bool failed=false;try{mission_character_select_action(b,r);}catch(const MissionCharacterAllocateError&){failed=true;}check(failed&&r.edi==0x1300214&&state.read(view.source_address+0x66,1)==0);
    std::cout<<"Current character scalar, pose and action setup passed; frame decoding remains required\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
