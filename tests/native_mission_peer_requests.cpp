#include "dfre/mission_peer_requests.hpp"
#include "dfre/mission_projectile_spawn.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <algorithm>
#include <array>
#include <map>
#include <stdexcept>
#include <vector>
namespace {
using U=std::uint32_t;
U get(const std::uint8_t* p,U w=4){U v=0;for(U i=0;i<w;++i)v|=U(p[i])<<(8*i);return v;}
void put(std::uint8_t* p,U v,U w=4){for(U i=0;i<w;++i)p[i]=std::uint8_t(v>>(8*i));}
}
int main(){
    constexpr U ACTOR=0x7dde80,OWNER=0x7cca80,CLIENT=0x6e9a90,DEF=0x1600000;
    std::vector<std::uint8_t> clients(17*0x9b84);std::map<U,std::uint8_t> memory;
    const auto read=[&](U a,U w=4){if(a>=CLIENT&&std::uint64_t(a-CLIENT)+w<=clients.size())return get(clients.data()+a-CLIENT,w);U v=0;for(U i=0;i<w;++i)v|=U(memory.at(a+i))<<(8*i);return v;};
    const auto store=[&](U a,U v,U w=4){if(a>=CLIENT&&std::uint64_t(a-CLIENT)+w<=clients.size()){put(clients.data()+a-CLIENT,v,w);return;}for(U i=0;i<w;++i)memory[a+i]=std::uint8_t(v>>(8*i));};
    auto reset=[&]{std::fill(clients.begin(),clients.end(),0);memory.clear();for(U i=0;i<3604;++i)memory[0x6e4760+i]=0;for(U i=0;i<0x110;++i)memory[0x6e5778+i]=0;store(0x8cfa1c,1);store(0x50321c,3);
        for(U i=0;i<3;++i){const U a=ACTOR+i*0x114;store(a,1);store(a+4,DEF+i*0x13c);store(a+0x6c,0,1);store(a+0x74,0xabcd1234);store(a+0xf8,OWNER);store(a+0x110,0x410500);store(DEF+i*0x13c+0x20,i==2?0xfb4:0xfc8);}
        store(CLIENT+8,OWNER);store(OWNER+0x24,11);store(OWNER+0x28,22);store(OWNER+0x114+0x24,33);store(OWNER+0x114+0x28,44);
    };
    reset();dfre::GameMessageStage messages;dfre::MissionServerMessageBuffer buffer;dfre::MissionClientOutboxBindings ob;
    ob.client=[&](std::size_t s){return dfre::MissionClientRecordView{clients.data()+s*0x9b84,0x9b84};};ob.cell=[&](U a){return read(a);};dfre::MissionClientOutbox outbox(messages,buffer,ob);
    std::function<void(U,U,U)> after_store;dfre::MissionPeerRequestBindings b;b.read=read;b.write=[&](U a,U w,U v){store(a,v,w);if(after_store)after_store(a,w,v);};
    unsigned calls=0;after_store=[&](U a,U,U){if(a==ACTOR+0x74)store(ACTOR+0x110,0x410600);};
    b.call=[&](U site,const std::vector<U>& args){++calls;assert(args.size()==2&&args[1]==1);assert(read(args[0]+0x74)==0xabcd0000);assert(site==(args[0]==ACTOR?0x410600u:0x410500u));store(0x8cfa1c,3);store(ACTOR+2*0x114+0x6c,2,1);return 0u;};
    dfre::mission_peer_trigger_charges(b,OWNER);assert(calls==2&&read(ACTOR+2*0x114+0x74)==0xabcd1234);
    reset();after_store={};b.call={};bool failed=false;try{dfre::mission_peer_trigger_charges(b,OWNER);}catch(const std::runtime_error&){failed=true;}assert(failed&&read(ACTOR+0x74)==0xabcd0000);
    store(0x8cfa1c,0xffffffff);dfre::mission_peer_trigger_charges(b,OWNER);
    reset();store(0x6e5884,2);after_store=[&](U a,U,U){if(a==0x6e5778)store(CLIENT+8,OWNER+0x114);if(a==0x6e5780)store(0x6e5884,50);};
    dfre::mission_peer_request_strike(b,outbox,0);assert(read(0x6e5784)==0x500&&read(0x6e5778)==OWNER&&read(0x6e577c)==11&&read(0x6e5780)==44&&read(0x6e5884)==49&&clients[0x204+8]==0x2d);
    reset();after_store={};store(0x6e5884,1);for(U i=0;i<16;++i)store(0x6e5784+i*16,1);dfre::mission_peer_request_strike(b,outbox,0);assert(read(0x6e5884)==1&&clients[0x204+8]==0x2e);
    reset();store(0x6e5884,1);after_store=[&](U a,U,U){if(a==0x6e5784)throw std::runtime_error("fixture store fault");};failed=false;
    try{dfre::mission_peer_request_strike(b,outbox,0);}catch(const std::runtime_error&){failed=true;}assert(failed&&read(0x6e5784)==0x500&&read(0x6e5884)==1&&!clients[0x204+9]);
    reset();after_store={};std::array<std::uint8_t,15> packet{};store(0x6e5570,100);dfre::mission_peer_record_shot(b,nullptr,0);
    store(0x6e5570,0);failed=false;try{dfre::mission_peer_record_shot(b,packet.data(),14);}catch(const std::runtime_error&){failed=true;}assert(failed&&read(0x6e5570)==0);
    put(packet.data(),0x11223380);put(packet.data()+4,0x445566ff);put(packet.data()+8,0,2);put(packet.data()+10,0x1234,2);put(packet.data()+12,0x5678,2);packet[14]=4;
    b.call={};failed=false;try{dfre::mission_peer_record_shot(b,packet.data(),packet.size());}catch(const std::runtime_error&){failed=true;}
    assert(failed&&read(0x6e4764)==0x11223300&&read(0x6e4768)==0x44556600&&read(0x6e5570)==0);
    // Real host-message ledger and real projectile spawn share the existing ring.
    reset();dfre::MissionTickState state;state.tables.groups[3].resize(1);state.tables.groups[3][0].active=true;state.tables.groups[3][0].field_70=1;state.tables.groups[3][0].field_78=99;state.projectiles.resize(512);state.globals.mission_67f528=1;
    dfre::MissionProjectileSpawnCells cells;cells.ring_index=511;cells.classes[3].field_30=0x80;cells.classes[3].field_18=10;cells.classes[3].field_20=0;
    std::vector<dfre::RuntimeItemDefinition> definitions(3);definitions[0].id=0xfa1;definitions[1].id=0xfc1;definitions[2].id=0xfca;
    b.write=[&](U a,U w,U v){if(a==dfre::mission_actor_source_address(3,0)+0x78){assert(w==4);state.tables.groups[3][0].field_78=v;}else store(a,v,w);};
    b.call=[](U site,const std::vector<U>& args){assert(site==0x47df40&&args.size()==2);return 0x1000u;};
    dfre::MissionProjectileSpawnBindings sb;sb.ground=[](auto&,auto,auto){return 0x1000;};sb.loudness=[](auto&,auto id){assert(id==0);return 0;};
    sb.host_message=[&](auto& live,auto& bytes){assert(&live==&state);const auto before=cells.ring_index;dfre::mission_peer_record_shot(b,bytes.data(),bytes.size());assert(cells.ring_index==before);};
    dfre::MissionProjectileSpawn spawn(state,cells,definitions,sb);dfre::MuzzleResult muzzle{{0x12345,0x23456,0x34567},{0x12345678,0xabcdef12,0}};const auto* pool=state.projectiles.data();
    spawn.request(dfre::actor_handle(3,0),3,muzzle);assert(cells.ring_index==0&&state.projectiles.data()==pool&&state.projectiles[0].owner_slot==0&&state.projectiles[0].ttl==10&&read(0x6e5570)==1&&read(CLIENT+0x1a4)==1&&state.tables.groups[3][0].field_78==0);
    spawn.request(dfre::actor_handle(3,0),3,muzzle);assert(cells.ring_index==1&&state.projectiles.data()==pool&&read(0x6e5570)==2&&read(CLIENT+0x1a4)==2);
    store(0x6e5570,100);state.tables.groups[3][0].field_78=123;spawn.request(dfre::actor_handle(3,0),3,muzzle);
    assert(cells.ring_index==2&&state.projectiles.data()==pool&&state.projectiles[2].ttl==10&&read(0x6e5570)==100&&read(CLIENT+0x1a4)==2&&state.tables.groups[3][0].field_78==123);
}
