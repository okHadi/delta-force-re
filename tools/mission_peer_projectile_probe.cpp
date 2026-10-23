#include "dfre/mission_peer_projectile.hpp"
#include "dfre/mission_peer_frame.hpp"
#include "dfre/motion_queries.hpp"
#include "dfre/mission_projectile_owner.hpp"
#include <algorithm>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>
namespace {
using U=std::uint32_t;
U word(const std::uint8_t* p,U n=4){U v=0;for(U i=0;i<n;++i)v|=U(p[i])<<(8*i);return v;}
void put(std::uint8_t* p,U v,U n=4){for(U i=0;i<n;++i)p[i]=std::uint8_t(v>>(8*i));}
std::string hex(const std::uint8_t* p,std::size_t n){static constexpr char d[]="0123456789abcdef";std::string out;for(std::size_t i=0;i<n;++i){out+=d[p[i]>>4];out+=d[p[i]&15];}return out;}
void list(const std::vector<std::vector<U>>& rows){std::cout<<'[';for(std::size_t i=0;i<rows.size();++i){if(i)std::cout<<',';std::cout<<'[';for(std::size_t j=0;j<rows[i].size();++j){if(j)std::cout<<',';std::cout<<rows[i][j];}std::cout<<']';}std::cout<<']';}
struct Transport:dfre::GameMessageHost {std::vector<std::uint8_t> packet;void send_local_frame(const std::uint8_t* p,std::size_t n)override{packet.assign(p,p+n);}};
void init(dfre::MissionTickState& state,const dfre::TerrainGeometry& terrain,const dfre::AiActorRef& ref){auto& rows=ref.group==4?state.tables.extra:state.tables.groups[ref.group];rows.resize(ref.index+2);for(U i=0;i<2;++i){auto& a=rows[ref.index+i];a.active=true;a.definition_index=1;a.field_70=std::uint16_t(i+1);a.field_78=99-i*11;}state.projectiles.resize(512);state.world.terrain=&terrain;}

std::vector<dfre::RuntimeItemDefinition> definitions(){std::vector<dfre::RuntimeItemDefinition> d(4);d[1].id=0xfa1;d[2].id=0xfc1;d[3].id=0xfca;for(U i=1;i<4;++i)d[i].model=0x7000+i;d[3].ambient_handles[0]=11;d[3].ambient_sound[0]=12;return d;}
void setup_class(dfre::MissionProjectileSpawnCells& cells,U kind,U flags,U ttl,U sound){auto& c=cells.classes[kind];c.field_30=flags;c.field_0c=kind==3?300:kind==4?500:800;c.field_10=kind+1;c.field_14=kind+2;c.field_18=ttl;c.field_20=sound;}
void run(const std::string& mode,U seed,const dfre::TerrainGeometry& low,const dfre::TerrainGeometry& high){
    const bool view=mode=="view";const U v=view?seed:seed/8,g=seed%8,initial_ring=seed%5==0?511:seed%512;
    bool fx=view?(seed%11!=10):bool(v&4);const U count=view?0:g==4?100:g==5?99:0;
    const U physical=std::array<U,6>{0,256,259,1280,2304,3328}[seed%6];const dfre::AiActorRef owner_ref=physical>=3328?dfre::AiActorRef{2,physical-3328}:physical>=2304?dfre::AiActorRef{1,physical-2304}:physical>=1280?dfre::AiActorRef{4,physical-1280}:physical>=256?dfre::AiActorRef{0,physical-256}:dfre::AiActorRef{3,physical};const dfre::AiActorRef next_owner{owner_ref.group,owner_ref.index+1};
    dfre::MissionTickState host;init(host,low,owner_ref);host.globals.multiplayer_67f520=1;host.globals.mission_67f528=1;
    auto defs=definitions();dfre::MissionProjectileSpawnCells cells;cells.ring_index=initial_ring;
    setup_class(cells,3,fx?0x40:0x80,90,fx?5:0);setup_class(cells,4,fx?0x40:0x80,44,fx?5:0);setup_class(cells,8,fx?0x40:0x80,88,fx?5:0);
    std::vector<std::vector<U>> trace;std::uint8_t* active_pose=nullptr;std::array<std::uint8_t,20> pose{};
    for(U i=0;i<5;++i)put(pose.data()+i*4,0x12345670u+seed*17+i*0x11111111u);
    dfre::MissionProjectileSpawnBindings sb;
    sb.attached_sound=[&](auto&,auto index,auto sample,auto value,auto flags){trace.push_back({0x43ef80,U(index),sample,value,flags});if((view&&(v&1))||(!view&&(v&32)))put(active_pose,word(active_pose)+777);if((view&&(v&8))||(!view&&(v&8)))throw std::runtime_error("fixture effect fault");};
    sb.positional_sound=[&](auto&,auto sound,auto p){trace.push_back({0x43e7d0,sound,U(p.x),U(p.y),U(p.z)});if((view&&(v&2))||(!view&&(v&64)))put(active_pose+4,word(active_pose+4)^555);};
    sb.loudness=[&](auto& live,auto sound){trace.push_back({0x43f4d0,sound});if((view&&(v&4))||(!view&&(v&128))){put(active_pose+8,word(active_pose+8)+333);live.actor_at(owner_ref)->field_70=0xffff;live.actor_at(next_owner)->field_70=0xffff;}if(v&16)cells.ring_index=127;return fx?9:0;};
    sb.alert=[&](auto&,auto p,auto level,auto team,auto flags){trace.push_back({0x45fcf0,U(p.x),U(p.y),U(p.z),U(level),U(team),flags});};
    dfre::MissionProjectileSpawn spawn(host,cells,defs,sb);dfre::MissionPeerRequestBindings rb;
    std::vector<std::uint8_t> clients(17*0x9b84),ledger(3604),wire;std::map<U,U> globals{{0x67f520,1},{0x67f538,g==3?0xffffffffu:100u},{0x50321c,0},{0x501344,7},{0x50135c,0x13f0000},{0x501360,0},{0xad3dd0+4*4,11}};
    const U owner0=0x7cca80+physical*0x114,owner1=owner0+0x114,row=0x6e4760+std::min(count,99u)*36;
    for(U i=0;i<17;++i)put(clients.data()+i*0x9b84,7+i);
    put(clients.data()+8,g==7?owner1:owner0);put(clients.data()+0x70,10);put(clients.data()+0x80,20);put(clients.data()+0x98,30);put(clients.data()+0xd4+4*4,g==1?0:4);put(clients.data()+0x154,g==3?0xffffffffu:100);put(clients.data()+0x15c,4);put(clients.data()+0x198,g==2?5:0);put(clients.data()+0x1c4,5,2);put(ledger.data()+3600,count);
    rb.read=[&](U a,U width){if(a>=0x6e9a90&&std::uint64_t(a-0x6e9a90)+width<=clients.size())return word(clients.data()+a-0x6e9a90,width);if(a>=0x6e4760&&std::uint64_t(a-0x6e4760)+width<=ledger.size())return word(ledger.data()+a-0x6e4760,width);return globals.at(a);};
    rb.write=[&](U a,U width,U value){
        if(a>=0x6e9a90&&std::uint64_t(a-0x6e9a90)+width<=clients.size())put(clients.data()+a-0x6e9a90,value,width);
        else if(a>=0x6e4760&&std::uint64_t(a-0x6e4760)+width<=ledger.size())put(ledger.data()+a-0x6e4760,value,width);
        else if(a==owner0+0x78||a==owner1+0x78)host.actor_at(a==owner0+0x78?owner_ref:next_owner)->field_78=value;
        else globals[a]=value;
        if(!view&&a==row+4&&(v&1)){host.world.terrain=&high;wire[15]=8;put(clients.data()+8,owner1);}
        if(!view&&a==0x6e5570&&(v&2)){put(clients.data()+0xe4,9);globals[0xad3dd0+4*4]=77;}
    };
    dfre::MissionPeerProjectile adapter(host,spawn,rb);
    rb.call=[&](U site,const std::vector<U>& args){if(site!=0x47df40||args.size()!=2)throw std::runtime_error("fixture request body");trace.push_back({site,args[0],args[1]});dfre::MissionPeerParserCall c;c.address=site;c.count=2;c.args[0]=args[0];c.args[1]=args[1];return adapter.call(c);};
    bool failed=false;dfre::ProjectileRecordImage predicted{};
    if(view){dfre::MissionPeerParserCall c;c.address=0x462f20;c.count=3;c.args[1]=fx?owner0:0;c.args[2]=3;c.payload_argument=0;c.payload=pose.data();c.length=20;active_pose=pose.data();try{adapter.call(c);}catch(const std::runtime_error&){failed=true;}}
    else{
        dfre::MissionTickState client;init(client,low,owner_ref);client.globals.multiplayer_67f520=1;client.globals.mission_67f528=0;dfre::MissionProjectileSpawnCells cc;cc.ring_index=511;setup_class(cc,4,0x80,44,0);
        dfre::GameMessageStage producer;Transport transport;producer.set_host(&transport);dfre::GameMessageLiveBindings live;live.read_global=[](U a){if(a==0x67f528)return 0u;if(a==0x67f520)return 1u;throw std::runtime_error("fixture producer global");};producer.set_live_bindings(live);
        dfre::MissionProjectileSpawnBindings cb;cb.ground=[&](auto& state,auto x,auto y){return dfre::ground_height_query(*state.world.terrain,x,y);};cb.loudness=[](auto&,auto){return 0;};cb.queue=[&](auto&,U id,const auto& data){producer.queue().enqueue(producer.table(),std::uint8_t(id),data.data(),std::uint16_t(data.size()));};
        dfre::MissionProjectileSpawn client_spawn(client,cc,defs,cb);dfre::MuzzleResult muzzle{{std::int32_t(0x12345670u+seed*17),std::int32_t(0x23456780u+seed*19),std::int32_t(0x34567890u+seed*23)},{0x12345678u+seed,0xabcdef12u-seed,0}};
        client_spawn.request(dfre::actor_handle(owner_ref.group,owner_ref.index),4,muzzle);producer.flush_queue();wire=transport.packet;predicted=dfre::snapshot_projectile_record(client.projectiles[0]);globals[0x501360]=U(wire.size());
        dfre::GameMessageStage host_messages;dfre::MissionServerMessageBuffer server;dfre::MissionClientOutboxBindings ob;ob.client=[&](std::size_t s){return dfre::MissionClientRecordView{clients.data()+s*0x9b84,0x9b84};};ob.cell=[&](U a){return globals.at(a);};dfre::MissionClientOutbox outbox(host_messages,server,ob);
        dfre::MissionPeerFrameBindings receive;receive.cell=[&](U a){return globals.at(a);};receive.store=[&](U a,U value){globals[a]=value;};receive.receive_497790=[](){return 1u;};
        dfre::MissionPeerParserBindings parser;parser.client=ob.client;parser.read=rb.read;parser.write=rb.write;parser.resolve=[&](U a,std::size_t n)->std::uint8_t*{if(a<0x13f0000||std::uint64_t(a-0x13f0000)+n>wire.size())return nullptr;return wire.data()+a-0x13f0000;};
        parser.call=[&](const dfre::MissionPeerParserCall& c){if(c.address==0x47df40)trace.push_back({c.address,c.args[0],c.args[1]});else if(c.address==0x445b80)trace.push_back({c.address});else if(c.address==0x462f20){trace.push_back({c.address,c.args[1],c.args[2]});active_pose=c.payload;}else throw std::runtime_error("fixture parser body");return adapter.call(c);};
        try{if(!dfre::mission_peer_frame_receive(host_messages.peer_frame_buffer(),receive))throw std::runtime_error("fixture transport empty");dfre::mission_peer_parser_parse(outbox,host_messages.table(),parser);}catch(const std::runtime_error&){failed=true;}
    }
    const auto entity=dfre::snapshot_projectile_record(host.projectiles[(initial_ring+1)&511]);
    std::cout<<"{\"failed\":"<<(failed?"true":"false")<<",\"ring\":"<<cells.ring_index<<",\"entity\":\""<<hex(entity.data(),entity.size())<<"\",\"predicted\":\""<<hex(predicted.data(),predicted.size())<<"\",\"wire\":\""<<hex(wire.data(),wire.size())<<"\",\"pose\":\""<<(view?hex(pose.data(),pose.size()):"")<<"\",\"ledger\":\""<<hex(ledger.data()+std::min(count,99u)*36,36)<<"\",\"count\":"<<word(ledger.data()+3600)<<",\"client\":[";
    bool comma=false;for(U offset:{8u,0x70u,0x80u,0x98u,0xe4u,0x154u,0x15cu,0x198u,0x1a4u}){if(comma)std::cout<<',';comma=true;std::cout<<word(clients.data()+offset);}std::cout<<"],\"owners\":["<<host.actor_at(owner_ref)->field_78<<','<<host.actor_at(next_owner)->field_78<<','<<unsigned(host.extra_for(owner_ref).field_6d)<<','<<unsigned(host.extra_for(next_owner).field_6d)<<"],\"calls\":";list(trace);std::cout<<"}\n";
}
}
int main(){dfre::TerrainGeometry low(dfre::AssetBytes(1024*1024,5),dfre::AssetBytes(1024*1024),std::vector<U>(65536));dfre::TerrainGeometry high(dfre::AssetBytes(1024*1024,100),dfre::AssetBytes(1024*1024),std::vector<U>(65536));std::string mode;U seed;while(std::cin>>mode>>seed)run(mode,seed,low,high);}
