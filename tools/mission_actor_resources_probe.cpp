#include "dfre/mission_actor_resources.hpp"
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>
using U=std::uint32_t;using Q=std::uint64_t;
struct Region{U base;std::vector<unsigned char> data;};
int main(){U op,seed,mutation,fault;while(std::cin>>op>>seed>>mutation>>fault){
 std::vector<Region> regions;for(auto x:std::vector<std::pair<U,U>>{{0x42fc5c,0x9edc},{0x6ba43c,0x1da14},{0x78f27c,0x14079c},{0xbfc0fc,0x3cd0},{0x500afc,4},{0x4cc990,32},{0xc23810,8},{0x1300000,0x2000}})regions.push_back({x.first,std::vector<unsigned char>(x.second,static_cast<unsigned char>(seed))});
 auto byte=[&](U at)->unsigned char&{for(auto& r:regions)if(at>=r.base&&Q(at)-r.base<r.data.size())return r.data[at-r.base];throw std::runtime_error("unowned byte");};
 auto get=[&](U at,unsigned w){U v=0;for(U i=0;i<w;++i)v|=U(byte(at+i))<<(i*8);return v;};
 auto put=[&](U at,unsigned w,U v){for(U i=0;i<w;++i)byte(at+i)=static_cast<unsigned char>(v>>(i*8));};
 for(U i=0;i<64;++i)put(0x6ba4a0+i*100,4,(i+seed)%4?0x1500000+i*32:0);
 for(U i=0;i<10;++i)for(U off:{0u,4u,12u,16u})put(0xbfc100+i*0x614+off,4,(i+off+seed)%5?0x1600000+i*128+off:0);
 put(0xc23810,4,0x1300000);put(0xc23814,4,seed%7==0?0xffffffffu:3);for(U i=0;i<8;++i)put(0x4cc990+i,1,seed%3?U('A'+i):0);put(0x4cc998,1,0);
 const U offsets[]={0,1,0xffffffffu,155,156,255*156,256*156,0x80000000u};const U instance=0x42fc60u+offsets[seed%8];
 Q hash=1469598103934665603ull;U entries=0,operations=0,stores=0,calls=0;
 auto emit=[&](U kind,U at,U w,U v){++entries;for(U x:{kind,at,w,v})for(U i=0;i<4;++i)hash=(hash^((x>>(i*8))&255))*1099511628211ull;};
 auto stop=[&](){if(fault&&++operations==fault)throw std::runtime_error("operation fixture");};
 dfre::MissionActorResourceBindings b;b.read=[&](U at,unsigned w){U v=get(at,w);emit('R',at,w,v);stop();return v;};
 b.write=[&](U at,unsigned w,U v){put(at,w,v);emit('W',at,w,v);++stores;if(stores==1){if(mutation&4)put(0xc23810,4,0x1301000);if(mutation&8)put(0xc23814,4,1);}stop();};
 b.call=[&](U site,const std::vector<U>& a)->U{emit('C',site,U(a.size()),0);for(U v:a)emit('A',0,0,v);stop();++calls;if(calls==1&&(mutation&1)){put(0x6ba504,4,0x1700000);put(0xbfc104,4,0x1700010);}if(site==0x49a620)for(U i=0;i<a.at(2);++i)put(a.at(0)+i,1,a.at(1));else if(site==0x42ebe0&&(mutation&2)){put(0x4cc990,1,'Z');put(0x4cc991,1,0);}return 0xffffffffu;};
 bool failed=false;try{switch(op){case 0:dfre::mission_character_release(b,seed%9?instance:0);break;case 1:dfre::mission_character_release_raw(b,instance);break;case 2:dfre::mission_character_pool_clear(b);break;case 3:dfre::mission_character_resources_clear(b);break;case 4:dfre::mission_character_work_clear(b);break;case 5:dfre::mission_actor_resources_clear(b);break;case 6:dfre::mission_auxiliary_resources_clear(b);break;default:dfre::mission_item_resource_links_clear(b);break;}}catch(const std::exception&){failed=true;}
 Q digest=1469598103934665603ull;for(auto& r:regions)for(auto v:r.data)digest=(digest^v)*1099511628211ull;
 std::cout<<failed<<' '<<digest<<' '<<hash<<' '<<entries<<'\n';
}}
