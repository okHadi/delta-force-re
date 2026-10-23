#include "dfre/mission_client_added.hpp"
#include "dfre/mission_client_live.hpp"
#include <iostream>
#include <unordered_map>
using namespace dfre;
using U=std::uint32_t;
namespace {
void check(bool value){if(!value)throw MissionClientAddedError("Client-added check failed");}
template<class F>void fails(F call){bool failed=false;try{call();}catch(const MissionClientAddedError&){failed=true;}check(failed);}
struct Fixture {
    MissionClientLive clients;std::unordered_map<U,std::uint8_t> bytes;std::vector<U> calls;MissionClientAddedBindings b;
    Fixture(){
        b.read=[&](U at,U width){if(clients.claims(at,width))return clients.read(at,width);U n=0;for(U i=0;i<width;++i)n|=U(bytes[at+i])<<(i*8);return n;};
        b.write=[&](U at,U width,U value){if(clients.claims(at,width)){clients.write(at,width,value);return;}for(U i=0;i<width;++i)bytes[at+i]=std::uint8_t(value>>(i*8));};
        b.call=[&](U site,const std::vector<U>& args)->U{
            calls.push_back(site);
            if(site==0x493a80){text(args.at(0),"join");return 4;}
            if(site==0x4564b0)return 0;
            if(site==0x49a620){for(U i=0;i<args.at(2);++i)b.write(args.at(0)+i,1,args.at(1));return args.at(0);}
            if(site==0x497870)return 0x1300100;
            if(site==0x4406e0){b.write(args.at(0)+0x9c,4,1);return 0;}
            if(site==0x43f610)return 0;
            if(site==0x404cf0)return 0x1300200;
            if(site==0x43f770)return 0x1300200;
            if(site==0x4423f0){check(args.at(0)==0x12&&args.at(1)==0x1301000);return 0;}
            throw MissionClientAddedError("Client-added fixture has no requested body");
        };
        b.write(0x501344,4,77);b.write(0xad3ed4,4,0x1300000);text(0x1300100,"peer");text(0x1300200,"peer joined");
    }
    void text(U at,const char* s){do{b.write(at++,1,std::uint8_t(*s));}while(*s++);}
};
}
int main(){try{
    Fixture cold;mission_client_added(cold.b,0x1301000,0x90);check(cold.calls==std::vector<U>({0x493a80,0x4564b0}));check(cold.clients.read(0x6e9a90+0x18c,1)==0);
    Fixture host;host.b.write(0x67f528,4,1);host.b.write(0x67f530,4,3);
    host.clients.write(MissionClientLive::source_address(1,0x94),4,5);host.clients.write(MissionClientLive::source_address(2,0x94),4,10);
    mission_client_added(host.b,0x1301000,0x90);const auto base=MissionClientLive::source_address(2);
    check(host.clients.read(base,4)==77&&host.clients.read(base+8,4)==0&&host.clients.read(base+0x18c,2)==1);
    check(host.clients.read(base+0x1c4,2)==1&&host.clients.read(base+0x1b8,4)==1&&host.clients.client(2).bytes[0x0c]=='p');
    check(host.clients.read(MissionClientLive::source_address(0,0x18c),1)==0&&host.clients.read(MissionClientLive::source_address(1,0x18c),1)==0);
    check(host.calls==std::vector<U>({0x493a80,0x4564b0,0x49a620,0x497870,0x4406e0,0x43f610,0x404cf0,0x43f770,0x4423f0}));
    Fixture full;full.b.write(0x67f528,4,1);full.b.write(0x67f530,4,1);mission_client_added(full.b,0x1301000,0x90);check(full.calls==std::vector<U>({0x493a80,0x4564b0,0x4564b0}));
    Fixture missing;missing.b.call={};fails([&]{mission_client_added(missing.b,0x1301000,0x90);});check(missing.b.read(0x1301000+0x6c,4)==77);
    fails([&]{mission_client_added(cold.b,0,0x90);});fails([&]{mission_client_added(cold.b,0x1301000,0x8f);});fails([&]{mission_client_added(cold.b,0xfffffff0,0x90);});
    std::cout<<"Client-added current record and required-body checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
