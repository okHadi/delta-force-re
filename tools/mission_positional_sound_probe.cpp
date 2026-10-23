#include "dfre/mission_positional_sound.hpp"
#include "dfre/mission_audio_frame.hpp"
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
using namespace dfre;
namespace {
using U=std::uint32_t;constexpr U graph=0x3100000,params=0x2007fd0,height=0x1800000;
std::int32_t si(U n){return n<=0x7fffffffu?std::int32_t(n):std::int32_t(std::int64_t(n)-0x100000000LL);}
U coordinate(unsigned seed,unsigned mul){return U(std::int32_t((seed*mul)%17)-8)*65536u+(seed*33u&0xffffu);}
}
int main(){std::string mode;unsigned seed;while(std::cin>>mode>>seed){
 std::map<U,std::uint8_t> memory;auto put=[&](U at,U value,U width=4){for(U i=0;i<width;++i)memory[at+i]=std::uint8_t(value>>(i*8));};
 auto get=[&](U at,U width){U n=0;for(U i=0;i<width;++i){auto p=memory.find(at+i);if(p!=memory.end())n|=U(p->second)<<(i*8);}return n;};
 const U id=seed%512,definition=0x4f2ea8+id*0x28;Fixed3 source{si(coordinate(seed,3)),si(coordinate(seed,5)),si(U(std::int32_t(seed%6)-2)*65536u)};
 Fixed3 listener{si(coordinate(seed+3,7)),si(coordinate(seed+1,11)),si(U(seed%4)*65536u)};U yaw=seed*0x1234567u;MissionAudioFrameCells frame{0,listener,yaw};
 if(seed%8==0){listener.x=source.x;listener.y=source.y;}if(seed%8==1){listener.x=si(U(source.x)+128);listener.y=si(U(source.y)+65535);}
 put(definition,seed%7?graph:0);put(definition+0x20,seed&4,1);put(0x5014fc,seed+1);put(0x501500,2);put(0xbd5284,height);
 for(U bank:{graph,graph+0x400}){put(bank+0x48,(bank!=graph&&(seed&32))?0u:std::array<U,6>{0,1,2,8,32,0xffffffff}[seed%6]);put(bank+0x24,1);put(bank+0x28,graph+0x100);put(bank+0x1c,bank==graph?0x10000:0x20000);}
 put(graph+0x100,1,2);put(graph+0x108,3);put(graph+0x110,graph+0x200);put(graph+0x200,graph+0x300);put(graph+0x204,0x10000);put(graph+0x323,1,1);put(graph+0x324,0x3200000);
 for(U i=0;i<32;++i)put(params+i,0xa5,1);for(U i=0;i<0xa0;++i)put(mission_sound_bank_scratch+i,0x5a,1);
 std::vector<U> samples;std::vector<std::vector<U>> calls;MissionSoundBankBindings b;
 b.read=[&](U at,U width)->U{
  if(at==0x6e06a8)return U(frame.listener_6e06a8.x);if(at==0x6e06ac)return U(frame.listener_6e06a8.y);if(at==0x6e06b0)return U(frame.listener_6e06a8.z);if(at==0x6e06b4)return frame.yaw_6e06b4;
  if(at>=height&&at<height+0x100000){if(width!=1)throw std::runtime_error("height width");samples.push_back(at);
   if(samples.size()==1&&(seed&8)){put(definition,graph+0x400);put(definition+0x20,(seed&4)^4,1);listener.x=si(U(listener.x)+65536);yaw+=0x40000000;}
   const auto index=at-height;return ((index&1023u)*13+(index>>10)*7+seed%16)&255;
  }return get(at,width);
 };b.write=[&](U at,U width,U value){put(at,value,width);};
 b.call=[&](U site,const auto& args)->U{if(site!=0x494dd0&&site!=0x494e80)throw std::runtime_error("unbound positional probe body");std::vector<U> event{site};event.insert(event.end(),args.begin(),args.end());calls.push_back(event);
  if(calls.size()==1&&(seed&16)){put(definition,0);put(params+4,7);put(params+0xc,3);yaw=0;const auto next=mission_sound_bank_scratch+0x14;put(next+4,0x3200010);put(next+8,0x10000);put(next+0xc,128);put(next+0x10,13);}return 0x90000000u+U(calls.size());
 };
 U result=0;if(mode=="o")result=mission_sound_occlusion(b,source,listener);else if(mode=="p")mission_sound_positional_request(b,id,U(source.x),U(source.y),U(source.z),params);else throw std::runtime_error("positional mode");
 const auto hex=[&](U at,U length){std::ostringstream out;out<<std::hex<<std::setfill('0');for(U i=0;i<length;++i)out<<std::setw(2)<<get(at+i,1);return out.str();};
 std::cout<<"{\"result\":"<<result<<",\"params\":\""<<hex(params,32)<<"\",\"scratch\":\""<<hex(mission_sound_bank_scratch,0xa0)<<"\",\"globals\":["<<get(0x5014fc,4)<<','<<get(definition,4)<<','<<get(definition+0x20,1)<<','<<U(listener.x)<<','<<U(listener.y)<<','<<U(listener.z)<<','<<yaw<<"],\"samples\":[";
 for(std::size_t i=0;i<samples.size();++i){if(i)std::cout<<',';std::cout<<samples[i];}std::cout<<"],\"calls\":[";for(std::size_t i=0;i<calls.size();++i){if(i)std::cout<<',';std::cout<<'[';for(std::size_t j=0;j<calls[i].size();++j){if(j)std::cout<<',';std::cout<<calls[i][j];}std::cout<<']';}std::cout<<"]}\n";
}}
