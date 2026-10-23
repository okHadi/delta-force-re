#include "dfre/mission_client_admission_helpers.hpp"
#include "dfre/mission_client_live.hpp"
#include <iostream>
#include <unordered_map>
using namespace dfre;using U=std::uint32_t;
int main(){try{
    auto check=[](bool n){if(!n)throw MissionClientAddedError("Admission-helper check failed");};
    MissionClientLive clients;std::unordered_map<U,std::uint8_t> memory;MissionClientAddedBindings b;
    b.read=[&](U at,U width){if(clients.claims(at,width))return clients.read(at,width);U n=0;for(U i=0;i<width;++i)n|=U(memory[at+i])<<(8*i);return n;};
    b.write=[&](U at,U width,U value){if(clients.claims(at,width)){clients.write(at,width,value);return;}for(U i=0;i<width;++i)memory[at+i]=std::uint8_t(value>>(8*i));};
    auto text=[&](U at,const char* p){do{b.write(at++,1,std::uint8_t(*p));}while(*p++);};
    clients.write(MissionClientLive::source_address(0,0x18c),1,1);clients.write(MissionClientLive::source_address(0,0x1d0),1,2);clients.write(MissionClientLive::source_address(1,0x1d0),1,2);
    mission_client_clear_references(b,MissionClientLive::source_address(2));check(clients.client(0).bytes[0x1d0]==0&&clients.client(1).bytes[0x1d0]==2);
    text(0x1300000,"Hello $A / $A");text(0x1300100,"Hadi");check(mission_client_name_message(b,0x1300000,0x1300100)==0x78ee54);
    const char* expected="Hello Hadi / Hadi";for(U i=0;i<=17;++i)check(b.read(0x78ee54+i,1)==std::uint8_t(expected[i]));
    check(mission_client_name_message(b,0x1300000,0)==0x78ee54&&b.read(0x78ee54,1)==0);
    b.write(0x501398,4,0x1300200);b.write(0x130026c,4,0x410000);b.call=[&](U target,const std::vector<U>& a){check(target==0x410000&&a==std::vector<U>{77});return 0x1300100u;};check(mission_client_peer_name(b,77)==0x1300100);
    b.call={};bool failed=false;try{mission_client_peer_name(b,77);}catch(const MissionClientAddedError&){failed=true;}check(failed);
    std::cout<<"Current client admission helpers passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
