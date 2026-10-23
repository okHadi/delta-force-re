#include "dfre/mission_peer_expiry.hpp"
#include <iostream>
#include <stdexcept>
#define CHECK(x) do{if(!(x))throw std::runtime_error("peer expiry: " #x);}while(false)
using namespace dfre;
int main(){try{
 std::uint32_t gate=0;MissionPeerExpiryBindings b;b.enabled_67f528=[&]{return gate;};mission_peer_expiry(b,0,{});
 gate=1;bool failed=false;try{mission_peer_expiry(b,0,{});}catch(const std::runtime_error&){failed=true;}CHECK(failed);
 std::array<std::uint32_t,7> words{1,2,3,0x12340000,0,0,0};b.fp_control=[] {return std::uint16_t(0x37f);};unsigned calls=0;
 b.enqueue=[&](auto id,auto* packet,auto length){CHECK(id==0x27&&length==18&&packet[0]==7&&packet[1]==0&&packet[14]==0x34&&packet[15]==0x12&&packet[16]==2&&packet[17]==200);++calls;words[0]=77;gate=0;};
 b.apply=[&](auto actor,MissionPeerExpiryRecord position,auto life){CHECK(calls==1&&actor==0x7cca80+7*0x114&&position.words==words.data()&&position.words[0]==77&&life==200);++calls;position.words[1]=88;};
 mission_peer_expiry(b,0x7cca80+7*0x114,{words.data(),words.size()});CHECK(calls==2&&words[1]==88&&gate==0);
 gate=1;b.enqueue={};failed=false;try{mission_peer_expiry(b,0,{words.data(),words.size()});}catch(const std::runtime_error&){failed=true;}CHECK(failed&&calls==2);
 b.enqueue=[&](auto,auto*,auto){words[0]=99;};b.apply={};failed=false;try{mission_peer_expiry(b,0,{words.data(),words.size()});}catch(const std::runtime_error&){failed=true;}CHECK(failed&&words[0]==99);
 CHECK(mission_peer_request_height(0,0x37f)==13107);CHECK(mission_peer_request_height(0xffffffff,0x37f)==13106);CHECK(mission_peer_request_height(0x7fffffff,0x37f)==0x80000000);
 std::array<std::uint32_t,7> position{1,2,3,4,0,0,0};MissionPeerExpiryEffectBindings effect;unsigned stage=0,random=0,spawned=0;
 effect.call=[&](auto site,const auto& args){
  if(site==0x477850){CHECK(stage++==0&&args==std::vector<std::uint32_t>{0x5fc});position[0]=10;}
  else if(site==0x45dc70){CHECK(stage++==1&&(args==std::vector<std::uint32_t>{3,10,2,3}));position[1]=20;}
  else if(site==0x4657d0){CHECK(stage++==2&&(args==std::vector<std::uint32_t>{5,10,20,3,77}));position[2]=30;}
  else if(site==0x43e7d0){CHECK(stage++==3&&(args==std::vector<std::uint32_t>{50,10,20,30}));position[0]=40;}
  else if(site==0x45fcf0){CHECK(stage++==4&&(args==std::vector<std::uint32_t>{40,20,30,200,0,0}));position[2]=50;}
  else {CHECK(site==0x477860&&args.empty());++random;position[3]=random*100;return random;}
  return 0u;
 };effect.spawn=[&](auto& local,auto actor,auto speed){CHECK(actor==77);++spawned;
  if(spawned==1){CHECK(local[0]==40&&local[1]==20&&local[2]==50&&local[3]==200+(1u<<16)&&local[4]==(2u<<14)&&local[5]==0xa5&&speed==(3|0x100)*30);local[0]+=1;local[1]^=2;local[2]-=3;local[5]^=4;}
  else {CHECK(spawned==2&&local[0]==41&&local[1]==22&&local[2]==47&&local[3]==500+(4u<<16)&&local[4]==(5u<<14)&&local[5]==(0xa5^4)&&speed==(6|0x100)*30);}
 };
 mission_peer_expiry_effect(effect,77,{position.data(),position.size()},2,0xa5);CHECK(stage==5&&random==6&&spawned==2);
 effect.call=[](auto,const auto&){return 0u;};effect.spawn={};mission_peer_expiry_effect(effect,77,{position.data(),3},0,0xa5);mission_peer_expiry_effect(effect,77,{position.data(),position.size()},0,0xa5);mission_peer_expiry_effect(effect,77,{position.data(),position.size()},-1,0xa5);
 failed=false;try{mission_peer_expiry_effect(effect,77,{position.data(),position.size()},1,0xa5);}catch(const std::runtime_error&){failed=true;}CHECK(failed);unsigned prefix_calls=0;effect.call=[&](auto,const auto&){++prefix_calls;return 0u;};failed=false;try{mission_peer_expiry_effect(effect,77,{position.data(),3},1,0xa5);}catch(const std::runtime_error&){failed=true;}CHECK(failed&&prefix_calls==7);effect.call={};failed=false;
 try{mission_peer_expiry_effect(effect,77,{position.data(),position.size()},0,0xa5);}catch(const std::runtime_error&){failed=true;}CHECK(failed);
 std::cout<<"peer expiry checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
