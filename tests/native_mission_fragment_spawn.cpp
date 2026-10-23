#include "dfre/mission_fragment_spawn.hpp"
#include <iostream>
#include <stdexcept>
#define CHECK(x) do{if(!(x))throw std::runtime_error("fragment spawn: " #x);}while(false)
using namespace dfre;
namespace {void put(ProjectileRecordImage& a,unsigned at,std::uint32_t n){for(unsigned i=0;i<4;++i)a[at+i]=std::uint8_t(n>>(i*8));}}
int main(){try{
 // Every source byte has one owner; null and slot zero are distinct.
 for(unsigned slot=0;slot<3;++slot){ProjectileRecordImage image;for(unsigned i=0;i<image.size();++i)image[i]=std::uint8_t(i*17+slot);put(image,0xf8,slot?0x7cca80+(slot-1)*0x114:0);ProjectileEntity entity;apply_projectile_record(entity,image);CHECK(snapshot_projectile_record(entity)==image&&entity.owner_slot==std::int32_t(slot)-1);}
 MissionTickState state;state.projectiles.resize(512);state.tables.groups[3].resize(2);MissionProjectileSpawnCells cells;cells.ring_index=511;
 for(auto& p:state.projectiles)p.reserved_source_bytes.fill(0xa5);const auto neighbor=snapshot_projectile_record(state.projectiles[1]);
 std::array<std::uint32_t,6> position{1,2,3,0x12345678,0x98765432,0xdeadbeef};unsigned calls=0;MissionFragmentSpawnBindings b;
 b.sincos_4776c0=[&](auto angle,auto& sine,auto& cosine){++calls;auto image=snapshot_projectile_record(state.projectiles[0]);
  if(calls==1){CHECK(cells.ring_index==0&&angle==0x98760000);CHECK(image==ProjectileRecordImage{});position[3]=0x44445555;position[4]=0x66667777;cells.ring_index=17;image[0x113]=0xfe;put(image,0x10,0x12345678);apply_projectile_record(state.projectiles[0],image);}
  else {CHECK(calls==2&&angle==0x12340000&&cells.ring_index==17&&image[0x113]==0xfe);position[0]=99;cells.ring_index=0xffffffff;}
  sincos_q22(angle,sine,cosine);
 };
 MissionFragmentSpawn spawn(state,cells,b);CHECK(spawn.spawn(position,0x7cca80,0x400000)==0xffffffff);const auto& p=state.projectiles[0];
 CHECK(calls==2&&p.position.x==99&&p.aim_yaw==0x44445555&&p.aim_pitch==0x66667777&&p.owner_slot==0&&p.class_flags==0x20&&p.ttl==400&&p.item_field==0);
 CHECK(snapshot_projectile_record(p)[0x113]==0xfe&&snapshot_projectile_record(state.projectiles[1])==neighbor);
 MissionFragmentSpawn missing(state,cells,{});bool failed=false;try{missing.spawn(position,0,0);}catch(const MissionTickError&){failed=true;}CHECK(failed&&cells.ring_index==0&&snapshot_projectile_record(state.projectiles[0])==ProjectileRecordImage{});
 MissionFragmentSpawnBindings direct;direct.sincos_4776c0=sincos_q22;MissionFragmentSpawn ordinary(state,cells,direct);position.fill(0);const auto index=ordinary.spawn(position,0,0x400000);auto& active=state.projectiles[index];CHECK(active.owner_slot==-1&&active.ttl==400&&active.item_main==0&&active.velocity.x==65535);
 unsigned queries=0;ProjectileQuery q;q.terrain=[&](const auto&,const auto&,auto,auto&){++queries;return 1;};q.actors_a=q.terrain;q.actors_b=q.terrain;q.special_a=q.terrain;std::vector<ProjectileEvent> events;
 tick_projectile(active,{0x7fffffff,0x7fffffff,0x7fffffff},q,events);CHECK(queries==4&&active.ttl==399&&active.position.x==65535&&active.velocity.z==-178&&events.size()==1&&events[0].kind==ProjectileEventKind::move);
 const auto previous=cells.ring_index;failed=false;try{ordinary.spawn(position,0x7cca81,0);}catch(const MissionTickError&){failed=true;}CHECK(failed&&cells.ring_index==((previous+1)&511)&&state.projectiles[cells.ring_index].ttl==400);
 state.projectiles.resize(1);failed=false;try{MissionFragmentSpawn invalid(state,cells,direct);}catch(const MissionTickError&){failed=true;}CHECK(failed);
 std::cout<<"fragment spawn checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
