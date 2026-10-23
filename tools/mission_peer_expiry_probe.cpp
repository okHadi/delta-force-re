#include "dfre/mission_peer_expiry.hpp"
#include <iostream>
#include <string>
#include <vector>
using namespace dfre;
namespace {
void effect(unsigned seed){
 std::array<std::uint32_t,7> words{};std::uint32_t rng=seed+1;for(auto& w:words){rng=rng*1664525u+1013904223u;w=rng;}
 std::vector<std::vector<std::uint32_t>> calls;const auto actor=0x7cca80u+seed*0x114u;
 const auto raw_count=std::array<std::uint32_t,6>{0,1,3,0xffffffff,0x80000000,7}[seed%6];const auto count=raw_count<=0x7fffffffu?std::int32_t(raw_count):std::int32_t(std::int64_t(raw_count)-0x100000000LL);
 MissionPeerExpiryEffectBindings b;b.call=[&](auto site,const auto& args){std::vector<std::uint32_t> event{site};event.insert(event.end(),args.begin(),args.end());calls.push_back(event);
  if(site==0x477850){rng=args.at(0)^seed;if(seed&1)words[0]+=11;return 0u;}
  if(site==0x477860){rng=rng*1664525u+1013904223u;if(seed&2)words[3]+=rng;return rng;}
  if(seed&1){if(site==0x45dc70)words[0]^=11;if(site==0x4657d0)words[1]+=22;if(site==0x43e7d0)words[2]-=33;if(site==0x45fcf0)words[0]+=44;}
  return 0u;
 };b.spawn=[&](auto& local,auto current,auto speed){std::vector<std::uint32_t> event{0x463200,current,speed};event.insert(event.end(),local.begin(),local.end());calls.push_back(event);if(seed&4){local[0]+=1;local[1]^=2;local[2]-=3;local[5]^=4;words[3]+=5;}};
 mission_peer_expiry_effect(b,actor,{words.data(),words.size()},count,0xa5a50000u+seed);
 const auto list=[](const auto& v){std::cout<<'[';for(std::size_t i=0;i<v.size();++i){if(i)std::cout<<',';std::cout<<v[i];}std::cout<<']';};
 std::cout<<"{\"words\":";list(words);std::cout<<",\"calls\":[";for(std::size_t i=0;i<calls.size();++i){if(i)std::cout<<',';list(calls[i]);}std::cout<<"]}\n";
}
}

int main(){std::string token;while(std::cin>>token){if(token=="e"){unsigned seed;std::cin>>seed;effect(seed);continue;}if(token=="h"){std::uint32_t h;unsigned c;std::cin>>h>>c;std::cout<<"{\"height\":"<<mission_peer_request_height(h,std::uint16_t(c))<<"}\n";continue;}const auto seed=unsigned(std::stoul(token));
 std::uint32_t gate=seed%3;std::array<std::uint32_t,7> words{};std::uint32_t rng=seed+1;for(auto& w:words){rng=rng*1664525u+1013904223u;w=rng;}words[2]=std::array<std::uint32_t,8>{0,1,0xffffffff,0x7fffffff,0x80000000,0x007fffff,0xff800000,0xffffcccd}[seed%8];
 const auto actor=std::array<std::uint32_t,6>{0x7cca80,0x7cca80+0x114*7,0,0xffffffff,0x7cca7f,0x7cca80+0x114*0x500}[seed%6];
 const auto control=std::array<std::uint16_t,12>{0x007f,0x047f,0x087f,0x0c7f,0x027f,0x067f,0x0a7f,0x0e7f,0x037f,0x077f,0x0b7f,0x0f7f}[seed%12];
 std::vector<std::uint8_t> packet;std::vector<std::uint32_t> effect;std::vector<std::uint32_t> calls;
 MissionPeerExpiryBindings b;b.enabled_67f528=[&]{return gate;};b.fp_control=[&]{return control;};b.enqueue=[&](auto id,auto* p,auto length){calls.push_back(0x442630);if(id!=0x27||length!=18)throw std::runtime_error("expiry packet");packet.assign(p,p+length);if(seed&1){words[0]^=0x12345678;words[2]=0x80000000;gate=0;}p[0]=0;};
 b.apply=[&](auto current,MissionPeerExpiryRecord position,auto life){calls.push_back(0x460950);effect={current,life};effect.insert(effect.end(),position.words,position.words+position.count);position.words[1]=99;};
 mission_peer_expiry(b,actor,{words.data(),words.size()});
 const auto list=[](const auto& values){std::cout<<'[';bool comma=false;for(auto n:values){if(comma)std::cout<<',';comma=true;std::cout<<std::uint32_t(n);}std::cout<<']';};std::cout<<"{\"words\":";list(words);std::cout<<",\"packet\":";list(packet);std::cout<<",\"effect\":";list(effect);std::cout<<",\"calls\":";list(calls);std::cout<<",\"gate\":"<<gate<<"}\n";
}}
