#include "dfre/mission_actor_insert.hpp"
#include "dfre/mission_terrain_mark.hpp"
#include <cassert>
#include <cstring>
#include <map>
#include <memory>
#include <stdexcept>
namespace {using U=std::uint32_t;U word(const dfre::AiActorImage& a,U off){U n;std::memcpy(&n,a.data()+off,4);return n;}}
int main(){using namespace dfre;auto pool=std::make_unique<MissionTransientActorPool>();std::map<U,std::uint8_t> source;const U raw=0x1200000;
 const auto put=[&](U at,U n,U value){for(U i=0;i<n;++i)source[at+i]=std::uint8_t(value>>(i*8));};
 MissionActorInsertBindings foreign;foreign.read=[&](U at,U n){U value=0;for(U i=0;i<n;++i)value|=U(source.at(at+i))<<(i*8);return value;};foreign.write=[&](U at,U n,U value){put(at,n,value);};
 for(U i=0;i<0x114;++i)source[raw+i]=std::uint8_t(i);put(raw,4,7);auto bindings=mission_transient_actor_bindings(*pool,foreign);
 assert(mission_actor_insert(bindings,raw,1)==0x822e80);assert(pool->count_8cfa14==1&&word(pool->records[0],0)==7&&pool->records[0][0x113]==0x13);
 source[raw+0x113]=0xe7;assert(mission_actor_insert(bindings,raw,0xffffffff)==0x822f94);assert(pool->records[1][0x113]==0xe7&&pool->count_8cfa14==2);
 bindings.write(0x822e80+0x113,2,0xabcd);assert(pool->records[0][0x113]==0xcd&&pool->records[1][0]==0xab);assert(bindings.read(0x822e80+0x113,2)==0xabcd);
 bindings.write(0x8cfa15,2,0x1234);assert(pool->count_8cfa14==0x123402);assert(bindings.read(0x8cfa14,4)==0x123402);
 bool failed=false;try{bindings.read(0x867e7f,2);}catch(const std::runtime_error&){failed=true;}assert(failed);failed=false;try{bindings.write(0xffffffff,2,0);}catch(const std::runtime_error&){failed=true;}assert(failed);
 mission_transient_actor_clear(*pool);assert(pool->count_8cfa14==0x123402);for(const auto& a:pool->records)for(auto byte:a)assert(byte==0);
 pool->count_8cfa14=0xffffffff;assert(mission_actor_insert(bindings,raw,1)==0x822e80&&pool->count_8cfa14==1);
 for(auto& a:pool->records)a[0]=1;assert(mission_actor_insert(bindings,raw,1)==0&&pool->count_8cfa14==1);
 // The type0 branch writes the caller's current physical group0 storage.
 auto group0=std::make_unique<std::array<AiActorImage,1024>>();U group0_count=0;foreign.read=[&](U at,U n){if(at>=0x7dde80&&std::uint64_t(at)+n<=0x822e80){U value=0;for(U i=0;i<n;++i){auto off=at-0x7dde80+i;value|=U((*group0)[off/0x114][off%0x114])<<(i*8);}return value;}if(at==0x8cfa1c){assert(n==4);return group0_count;}U value=0;for(U i=0;i<n;++i)value|=U(source.at(at+i))<<(i*8);return value;};
 foreign.write=[&](U at,U n,U value){if(at>=0x7dde80&&std::uint64_t(at)+n<=0x822e80){for(U i=0;i<n;++i){auto off=at-0x7dde80+i;(*group0)[off/0x114][off%0x114]=std::uint8_t(value>>(i*8));}return;}if(at==0x8cfa1c){group0_count=value;return;}put(at,n,value);};
 put(raw+4,4,0x1210000);put(0x1210028,4,6);put(0x1210024,1,4);unsigned persons=0;foreign.person=[&](U actor){assert(actor==0x7dde80);++persons;group0_count=99;(*group0)[0][0xc]=255;return 0u;};bindings=mission_transient_actor_bindings(*pool,foreign);
 assert(mission_actor_insert(bindings,raw,0)==0&&persons==1&&group0_count==99&&word((*group0)[0],0xc)==0&&word((*group0)[0],0)==7);
 (*group0)[0].fill(0);foreign.person={};bindings=mission_transient_actor_bindings(*pool,foreign);failed=false;try{mission_actor_insert(bindings,raw,0);}catch(const std::runtime_error&){failed=true;}assert(failed&&word((*group0)[0],0)==7);
 // Real45DAB0 feeds its full mutable image into the real separate insertion pool.
 mission_transient_actor_clear(*pool);pool->count_8cfa14=0;bindings=mission_transient_actor_bindings(*pool,foreign);MissionTickState state;MissionTerrainMarkBindings terrain;
 terrain.read=[](U at,U n){if(at==0xbd52ac)return 0x1300000u;if(at==0x1300000&&n==1)return 7u;if(at==0xbd533b&&n==1)return 1u;if(at==0x8e44c4)return 3u;if(at==0xc23810)return 0x1210000u;if(at==0x1210000+3*0x13c+0x6c)return 0x71000001u;if(at==0x1210000+3*0x13c+0x84)return 0x453810u;if(at==0x1210000+3*0x13c+0x90)return 0x4563a0u;throw std::runtime_error("unowned terrain fixture read");};
 unsigned sounds=0,zones=0;terrain.call=[&](U site,const auto& args,std::uint8_t* data,std::size_t length){if(site==0x43e7d0){++sounds;return;}if(site==0x45fcf0){++zones;return;}assert(site==0x45d970&&length==0x114&&args[1]==1);for(U i=0;i<length;++i)source[raw+i]=data[i];assert(mission_actor_insert(bindings,raw,args[1])==0x822e80);};mission_terrain_mark(state,{},terrain);assert(sounds==1&&zones==1&&pool->count_8cfa14==1&&word(pool->records[0],0)==3&&word(pool->records[0],0x110)==0x453810&&pool->records[0][0x113]==0);
}
