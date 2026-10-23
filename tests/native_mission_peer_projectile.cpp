#include "dfre/mission_peer_projectile.hpp"
#include "dfre/mission_behavior_zones.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <vector>
namespace {
using U=std::uint32_t;
void put(std::uint8_t* p,U value){for(U i=0;i<4;++i)p[i]=std::uint8_t(value>>(8*i));}
}
int main(){
    dfre::TerrainGeometry low(dfre::AssetBytes(1024*1024,5),dfre::AssetBytes(1024*1024),std::vector<U>(65536));
    dfre::TerrainGeometry high(dfre::AssetBytes(1024*1024,100),dfre::AssetBytes(1024*1024),std::vector<U>(65536));
    dfre::MissionTickState state;state.tables.groups[3].resize(2);state.tables.groups[3][0].active=true;state.tables.groups[3][0].field_70=1;state.projectiles.resize(512);state.world.terrain=&low;
    std::vector<dfre::RuntimeItemDefinition> defs(4);defs[1].id=0xfa1;defs[2].id=0xfc1;defs[3].id=0xfca;defs[3].model=0x7003;defs[3].ambient_handles[0]=11;defs[3].ambient_sound[0]=12;
    dfre::MissionProjectileSpawnCells cells;cells.ring_index=511;auto& cls=cells.classes[3];cls.field_30=0x40;cls.field_18=90;cls.field_20=7;
    std::array<std::uint8_t,21> bytes{};auto* pose=bytes.data()+1;for(U i=0;i<5;++i)put(pose+i*4,i+1);
    state.globals.mission_67f528=1;dfre::MissionBehaviorZones zones;
    bool effects=true;std::vector<U> order;dfre::MissionProjectileSpawnBindings b;
    b.attached_sound=[&](auto&,auto index,auto sample,auto value,auto flags){assert(effects&&index==0&&sample==11&&value==12&&flags==0);assert(state.projectiles[0].position.x==1);order.push_back(1);put(pose,9);};
    b.positional_sound=[&](auto&,auto sound,auto p){assert(effects&&sound==7&&p.x==9&&p.y==2&&p.z==3);order.push_back(2);put(pose+4,10);};
    b.loudness=[&](auto&,auto sound){if(!effects){assert(sound==0);return 0;}assert(sound==7);order.push_back(3);put(pose+8,11);return 9;};
    b.alert=[&](auto&,auto p,auto level,auto team,auto mode){assert(effects&&p.x==9&&p.y==10&&p.z==11&&level==4&&team==1&&mode==2);order.push_back(4);assert(dfre::mission_behavior_zone_add(zones.bindings(state.globals.mission_67f528),U(p.x),U(p.y),U(p.z),level,team,mode));};
    dfre::MissionProjectileSpawn spawn(state,cells,defs,b);dfre::MissionPeerRequestBindings requests;dfre::MissionPeerProjectile adapter(state,spawn,requests);
    dfre::MissionPeerParserCall call;call.address=0x462f20;call.count=3;call.args[1]=dfre::mission_actor_source_address(3,0);call.args[2]=3;call.payload_argument=0;call.payload=pose;call.length=20;
    assert(adapter.call(call)==0&&cells.ring_index==0&&state.projectiles[0].owner_slot==0);
    assert((order==std::vector<U>{1,2,3,4})&&state.projectiles[0].position.x==1&&state.projectiles[0].position.y==2&&state.projectiles[0].position.z==3);
    assert(zones.count==1);state.people.resize(1);auto& actor=state.tables.groups[3][0];actor.person_slot=0;actor.position[0]=8;actor.position[1]=10;actor.position[2]=11;actor.field_72=6;actor.angle_yaw=99;
    dfre::ActorQueryTable query_table;query_table.actors=&state.tables.groups[3];query_table.people=&state.people;
    dfre::ActorQueryData query_data;query_data.zones=zones.active_records();
    dfre::query_zone_45fd90(query_table,0,query_data,state.behavior);
    assert(state.people[0].field_9c==2&&actor.angle_yaw==0&&state.behavior.actions.size()==1);
    assert(state.behavior.actions[0].site==dfre::behavior_site::kTypeSlotMark&&state.behavior.actions[0].args[0]==6);
    const auto allocated=dfre::snapshot_projectile_record(state.projectiles[0]);
    call.length=19;bool failed=false;try{adapter.call(call);}catch(const dfre::MissionTickError&){failed=true;}assert(failed&&cells.ring_index==0&&allocated==dfre::snapshot_projectile_record(state.projectiles[0]));call.length=20;
    call.args[1]++;failed=false;try{adapter.call(call);}catch(const dfre::MissionTickError&){failed=true;}assert(failed&&cells.ring_index==0);
    call.args[1]=dfre::mission_actor_source_address(3,2);failed=false;try{adapter.call(call);}catch(const dfre::MissionTickError&){failed=true;}assert(failed&&cells.ring_index==0);
    effects=false;cls.field_30=0x80;cls.field_20=0;call.args[1]=0;assert(adapter.call(call)==1&&state.projectiles[1].owner_slot==-1&&state.projectiles[1].color==0);
    call={};call.address=0x47df40;call.count=2;call.args[0]=0xffffffff;call.args[1]=0x80000000;
    assert(adapter.call(call)==5*16384);state.world.terrain=&high;assert(adapter.call(call)==100*16384);state.world.terrain=nullptr;failed=false;try{adapter.call(call);}catch(const dfre::MissionTickError&){failed=true;}assert(failed);
    requests.read=[](U a,U width){assert(a==0x6e5570&&width==4);return 100u;};std::array<std::uint8_t,15> packet{};
    call={};call.address=0x445b80;call.count=1;call.payload_argument=0;call.payload=packet.data();call.length=15;assert(adapter.call(call)==0&&cells.ring_index==1);
    call.address=0x123456;failed=false;try{adapter.call(call);}catch(const dfre::MissionTickError&){failed=true;}assert(failed&&!dfre::MissionPeerProjectile::owns(call.address));
}
