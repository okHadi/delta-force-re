#include "dfre/mission_event_resources.hpp"
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>
using U=std::uint32_t;using Q=std::uint64_t;
struct Region{U base;std::vector<unsigned char> bytes;};
int main(){U op,seed,mutation,fault;while(std::cin>>op>>seed>>mutation>>fault){
 std::vector<Region> regions;for(auto x:std::vector<std::pair<U,U>>{{0x4f2bc4,0x28},{0x6d7e4c,0x8410},{0x1300000,0x6000}})regions.push_back({x.first,std::vector<unsigned char>(x.second,static_cast<unsigned char>(seed))});
 auto byte=[&](U at)->unsigned char&{for(auto& r:regions)if(at>=r.base&&Q(at)-r.base<r.bytes.size())return r.bytes[at-r.base];throw std::runtime_error("unowned fixture byte");};auto get=[&](U at,U width){U value=0;for(U i=0;i<width;++i)value|=U(byte(at+i))<<(i*8);return value;};auto put=[&](U at,U width,U value){for(U i=0;i<width;++i)byte(at+i)=static_cast<unsigned char>(value>>(i*8));};
 for(U i=0;i<3;++i){put(0x4f2bc8+i*8,4,3);put(0x4f2bcc+i*8,4,(seed+i)%4?0x1300100+i*0x2000:0);}put(0x4f2be0,1,seed&255);
 Q hash=1469598103934665603ull;U entries=0,operations=0,frees=0,clears=0,allocs=0,reads=0,count_reads=0,event_reads=0;bool fixed=false;
 auto emit=[&](U kind,U at,U w,U v){++entries;for(U x:{kind,at,w,v})for(U i=0;i<4;++i)hash=(hash^((x>>(i*8))&255))*1099511628211ull;};auto stop=[&](){if(fault&&++operations==fault)throw std::runtime_error("operation fixture");};
 dfre::MissionEventResourceBindings b;b.read=[&](U at,unsigned w){U v=get(at,w);emit('R',at,w,v);if(at==0x4f2bc8&&++count_reads==2&&(mutation&64))put(at,4,1);if(at==0x4f2bcc&&++event_reads==3&&(mutation&256))put(at,4,0x1301000);stop();return v;};b.write=[&](U at,unsigned w,U v){put(at,w,v);emit('W',at,w,v);if(at==0x4f2bdc&&(mutation&16))put(0x4f2bcc,4,0x1300000);if(!fixed&&at>=0x1300000&&at<0x1302000){fixed=true;if(mutation&32)put(0x4f2bcc,4,0x1301000);}stop();};
 b.call=[&](U site,const std::vector<U>& a)->U{emit('C',site,U(a.size()),0);for(U v:a)emit('A',0,0,v);stop();
 if(site==0x4980d0){if(++frees==1&&(mutation&1))put(0x4f2bd4,4,0);return 0xffffffffu;}
 if(site==0x49a620){for(U i=0;i<a.at(2);++i)put(a[0]+i,1,a[1]);if(++clears==1&&(mutation&2)){put(0x6e0250,4,0xffffffffu);put(0x6e0254,4,0xffffffffu);}return a[0];}
 if(site==0x497f60){++allocs;if(allocs==1&&(mutation&4)){put(0x4f2bd0,4,7);put(0x4f2bd8,4,8);}if(allocs==3&&(mutation&8))put(0x4f2bcc,4,0x1301000);if(seed%(allocs==1?11u:allocs==2?13u:17u)==0)return 0;return 0x1300000+(allocs-1)*0x2000;}
 if(site==0x499280){++reads;if(reads<=3)put(a[0],4,reads==1?seed%4:3);else {for(U i=0;i<a[1];++i)put(a[0]+i,1,static_cast<unsigned char>(seed+i));if(reads==4)for(U row=0;row<seed%4;++row){put(a[0]+row*24+4,4,row);put(a[0]+row*24+8,4,2-row);}}return a[1];}
 if(site==0x42eea0){if(mutation&128)put(0x4f2be0,1,0xa4);return 0;}if(site==0x42f020)return 0;throw std::runtime_error("unowned fixture call");};
 bool failed=false;try{if(op)dfre::mission_event_resources_load(b);else dfre::mission_event_resources_reset(b);}catch(const std::exception&){failed=true;}
 Q digest=1469598103934665603ull;for(auto& r:regions)for(auto v:r.bytes)digest=(digest^v)*1099511628211ull;std::cout<<failed<<' '<<digest<<' '<<hash<<' '<<entries<<'\n';
}}
