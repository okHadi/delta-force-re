#include "dfre/mission_peer_timers.hpp"
#include <iostream>
#include <string>
#include <stdexcept>
#include <vector>
using namespace dfre;
int main(){std::string token;while(std::cin>>token){if(token=="h"){std::uint32_t height;unsigned control;std::cin>>height>>control;std::cout<<"{\"height\":"<<mission_peer_expiry_height(height,std::uint16_t(control))<<"}\n";continue;}const auto seed=unsigned(std::stoul(token));
 std::array<std::uint32_t,67> cells{};std::vector<std::vector<std::uint32_t>> calls,writes;
 std::uint32_t rng=seed+1;for(auto& x:cells){rng=rng*1664525u+1013904223u;x=rng;}
 for(unsigned i=0;i<16;++i)cells[i*4+3]=std::array<std::uint32_t,6>{0,1,2,0xffffffff,0x80000000,3}[(seed+i)%6];cells[66]=std::array<std::uint32_t,4>{0,1,0xffffffff,0x80000000}[seed%4];
 const auto read=[&](auto a){return cells.at((a-0x6e5778)/4);};const auto write=[&](std::uint32_t a,std::uint32_t v){cells.at((a-0x6e5778)/4)=v;writes.push_back({a,v});};
 const auto control=std::array<std::uint16_t,12>{0x007f,0x047f,0x087f,0x0c7f,0x027f,0x067f,0x0a7f,0x0e7f,0x037f,0x077f,0x0b7f,0x0f7f}[seed%12];
 unsigned height_calls=0;MissionPeerTimerBindings b;b.read=read;b.write=write;b.fp_control=[&]{return control;};
 b.height=[&](auto x,auto y){calls.push_back({0x47df40,x,y});++height_calls;if(seed&1){write(0x6e5778,123);write(0x6e5880,17);write(0x6e5874,1);}return std::array<std::uint32_t,8>{0,0xffffffff,0x7fffffff,0x80000000,0x007fffff,0xff800000,0x00ffffff,0xffffe666}[(seed+height_calls)%8];};
 b.expire=[&](auto id,auto& position){std::vector<std::uint32_t> event{0x4470c0,id};event.insert(event.end(),position.begin(),position.end());calls.push_back(event);if(seed&2){write(0x6e5878,0);write(0x6e587c,0);write(0x6e5880,0xffffffff);}position[0]=0;};
 mission_peer_timers(b);
 const auto list=[](const auto& values){std::cout<<'[';bool comma=false;for(auto n:values){if(comma)std::cout<<',';comma=true;std::cout<<n;}std::cout<<']';};
 std::cout<<"{\"cells\":";list(cells);std::cout<<",\"calls\":[";for(std::size_t i=0;i<calls.size();++i){if(i)std::cout<<',';list(calls[i]);}std::cout<<"],\"writes\":[";for(std::size_t i=0;i<writes.size();++i){if(i)std::cout<<',';list(writes[i]);}std::cout<<"]}\n";
}}
