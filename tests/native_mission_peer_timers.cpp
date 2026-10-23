#include "dfre/mission_peer_timers.hpp"
#include <iostream>
#include <stdexcept>
#define CHECK(x) do{if(!(x))throw std::runtime_error("peer timers: " #x);}while(false)
using namespace dfre;
int main(){try{
 std::array<std::uint32_t,67> cells{};MissionPeerTimerBindings b;b.read=[&](auto a){return cells.at((a-0x6e5778)/4);};b.write=[&](auto a,auto v){cells.at((a-0x6e5778)/4)=v;};
 b.fp_control=[] {return std::uint16_t(0x37f);};unsigned heights=0,expired=0;
 b.height=[&](auto x,auto y){CHECK(x==0x10000&&y==0);++heights;cells[0]=99;cells[7]=1;cells[66]=20;return 0xffffffffu;};
 b.expire=[&](auto id,auto& p){CHECK(id==99&&p[0]==0x10000&&p[1]==0&&p[2]==6552&&p[3]==0&&p[4]==0&&p[5]==0&&p[6]==0xffffffff);++expired;cells[7]=0;cells[66]=0x80000000;};
 cells[0]=7;cells[3]=1;cells[64]=0x100000;cells[66]=1;mission_peer_timers(b);CHECK(heights==1&&expired==1&&cells[3]==0&&cells[66]==0x7fffffff);
 cells.fill(0);cells[66]=0x80000000;b.height={};b.expire={};b.fp_control={};mission_peer_timers(b);CHECK(cells[66]==0x7fffffff);
 cells[3]=1;bool failed=false;try{mission_peer_timers(b);}catch(const std::runtime_error&){failed=true;}CHECK(failed&&cells[3]==0&&cells[66]==0x7fffffff);
 b.height=[](auto,auto){return 0u;};cells[3]=1;failed=false;try{mission_peer_timers(b);}catch(const std::runtime_error&){failed=true;}CHECK(failed&&cells[3]==0);
 b.fp_control=[] {return std::uint16_t(0x37f);};cells[3]=1;failed=false;try{mission_peer_timers(b);}catch(const std::runtime_error&){failed=true;}CHECK(failed&&cells[3]==0);
 CHECK(mission_peer_expiry_height(0,0x37f)==6553);CHECK(mission_peer_expiry_height(0xffffffff,0x37f)==6552);CHECK(mission_peer_expiry_height(0x7fffffff,0x37f)==0x80000000);
 CHECK(mission_peer_expiry_height(0x80000000,0x37f)==std::uint32_t(std::int64_t(-2147483648LL)+6554));
 failed=false;try{(void)mission_peer_expiry_height(0,0x17f);}catch(const std::runtime_error&){failed=true;}CHECK(failed);
 // INT_MIN absolute distance stays wrapped and signed, as in the x86 body.
 cells.fill(0);cells[3]=2;cells[64]=0x80000000;cells[66]=1;mission_peer_timers(b);CHECK(cells[3]==1&&cells[1]==0x20000&&cells[2]==0&&cells[66]==0);
 cells.fill(0);cells[3]=2;cells[64]=0x80000000;cells[65]=0x40000001;cells[66]=1;failed=false;
 try{mission_peer_timers(b);}catch(const std::runtime_error&){failed=true;}CHECK(failed&&cells[3]==1&&cells[1]==0&&cells[2]==0&&cells[66]==1);
 std::cout<<"peer timer checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
