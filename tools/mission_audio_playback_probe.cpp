#include "dfre/mission_audio_playback.hpp"
#include <iostream>
#include <sstream>
using namespace dfre;
namespace {
void put(std::uint8_t* p,unsigned at,std::uint32_t n,unsigned width=4){for(unsigned i=0;i<width;++i)p[at+i]=std::uint8_t(n>>(8*i));}
std::string hex(const std::uint8_t* p,std::size_t n){const char* d="0123456789abcdef";std::string r;for(std::size_t i=0;i<n;++i){r+=d[p[i]>>4];r+=d[p[i]&15];}return r;}
constexpr std::uint32_t lengths[]={0,1,2,32,100,0x80000000,0xffffffff};
constexpr std::uint32_t volumes[]={0,255,0xffffffff,0x7fffffff,0x80000000,128};
constexpr std::uint32_t rates[]={0,0x10000,0x8000,0xffffffff,0x7fffffff};
}
int main(){std::string line;while(std::getline(std::cin,line)){try{
 std::istringstream input(line);std::string op;unsigned seed;input>>op>>seed;
 std::uint32_t enabled=(seed/4)&1;MissionAudioPlaybackCells cells{enabled};
 cells.generation_4a2ae0=std::array<std::uint32_t,4>{1,0xffffffff,0,123}[seed%4];cells.master_4a2716=std::array<std::uint32_t,4>{255,0,128,0xffffffff}[seed%4];
 std::array<std::uint8_t,128> sample{};put(sample.data(),4,lengths[seed%7]);put(sample.data(),8,0x10000000);sample[12]=std::uint8_t(seed%33);
 for(unsigned i=16;i<sample.size();++i)sample[i]=std::uint8_t((0x1500000u+i)*17);
 std::vector<std::vector<std::uint32_t>> calls;std::vector<std::uint32_t> reads;
 const auto store=[&](std::uint32_t address,std::uint32_t value){put(cells.records_4a16d2.data(),address-mission_audio_voice_base,value);};
 MissionAudioPlaybackBindings b;b.sample=[&](auto handle){if(handle!=0x1500000)throw std::runtime_error("probe sample handle");return MissionAudioSampleView{sample.data(),sample.size(),handle};};
 b.pcm_byte=[&](auto address){reads.push_back(address);if(address<0x1500010||address>=0x1500080)throw std::runtime_error("probe PCM address");return sample[address-0x1500000];};
 b.completed=[&](auto site,auto token,auto source){calls.push_back({site,token,source});if(seed&1)store(0x4a1d52+3*0x30+0x10,99);};
 MissionAudioPlayback playback(cells,b);cells.records_4a16d2.fill(0x5a);playback.initialize();
 for(unsigned i=0;i<16;++i){auto* p=cells.records_4a16d2.data()+0x4a1a12-mission_audio_voice_base+i*0x34;p[0x31]=0xa5;p[0x32]=0x5a;p[0x33]=0xc3;auto* a=cells.records_4a16d2.data()+0x4a2052-mission_audio_voice_base+i*0x34;a[0x31]=0xb6;a[0x32]=0x6b;a[0x33]=0xd4;}
 const auto pan=std::uint32_t(seed*73),volume=volumes[seed%6],rate=rates[seed%5];std::uint32_t result=0;
 if(op=="init")playback.initialize();
 else if(op=="play"){
  for(unsigned i=0;i<16;++i){auto* p=cells.records_4a16d2.data()+i*0x34;const auto mode=seed%4;p[0x30]=mode==1;p[0x31]=mode==3;put(p,0x1c,i==2?0:500);if(mode==2)cells.records_4a16d2[0x4a1a12-mission_audio_voice_base+i*0x34+0x30]=1;}
  result=playback.play(0x1500000,pan,volume,rate);
 }else if(op=="fixed"){
  if(seed%3==0)cells.records_4a16d2[0x4a1a12-mission_audio_voice_base+(seed%16)*0x34+0x30]=1;
  result=playback.play_channel(seed%16,0x1500000,pan,volume,rate);
 }else if(op=="ambient")result=playback.start_ambient(0x1500000,pan,volume,rate,seed%16);
 else if(op=="update"||op=="playing"||op=="stop"){
  for(auto address:{0x4a16d2+3*0x34,0x4a1d52+11*0x30}){store(address+0x10,7);store(address+0x18,64);store(address+0x1c,100);store(address+0x14,0x10000000);}
  if(seed%3==1)store(0x4a16d2+3*0x34+0x10,0);
  store(0x4a1a12+5*0x34+0x10,7);cells.records_4a16d2[0x4a1a12-mission_audio_voice_base+5*0x34+0x30]=std::uint8_t(seed&1);
  const auto handle=std::array<std::uint32_t,3>{0,7,99}[seed%3];
  if(op=="update")result=playback.update(handle,(seed&1)?0xffffffff:pan,(seed&2)?0xffffffff:volume,rate);
  if(op=="playing")result=playback.playing(handle);if(op=="stop")playback.stop(handle);
 }else if(op=="register")playback.register_completion(0x480123);
 else if(op=="publish"||op=="frames"||op=="dirty"||op=="stream"||op=="disabled"){
  cells.master_4a2716=255;sample[12]=0;put(sample.data(),4,16+seed%32);
  (void)playback.play_channel(3,0x1500000,pan,128,0x10000);(void)playback.start_ambient(0x1500000,pan+64,100,0x10000,5);
  if(seed&1)store(0x4a1a12+3*0x34+0x2c,77);
  playback.register_completion(0x480123);
  if(op=="dirty"){playback.publish();(void)playback.update(cells.generation_4a2ae0-2,64,150,0x8000);playback.publish();}
  else if(op=="stream"){
   auto* target=cells.records_4a16d2.data()+0x4a23c2-mission_audio_voice_base;
   auto* source=cells.records_4a16d2.data()+0x4a1a12-mission_audio_voice_base+3*0x34;
   std::copy(source,source+0x30,target);put(target,0,0x1500030);put(target,4,32);put(target,12,0xffff8400);put(target,8,0x1000);target[0x30]=1;store(0x4a2392+12,0xffffffff);playback.advance_frames(8);
  }else if(op=="publish")playback.publish();else {if(op=="disabled")cells.disabled_4a2712=1;playback.advance_frames(std::array<std::uint32_t,8>{0,1,2,8,32,128,441,1024}[seed%8]);}
 }else throw std::runtime_error("probe operation");
 std::cout<<"{\"records\":\""<<hex(cells.records_4a16d2.data(),cells.records_4a16d2.size())<<"\",\"scalars\":["<<cells.generation_4a2ae0<<','<<cells.scan_4b4f5a<<','<<cells.frames_4a35b0<<','<<cells.completion_4a2ae8<<"],\"result\":"<<result<<",\"compiled\":[";
 for(unsigned i=0;i<16;++i){const auto& v=cells.compiled[i];std::cout<<(i?",[":"[")<<v.frequency<<','<<v.end<<",\""<<hex(v.right_kernel.data(),8)<<"\",\""<<hex(v.left_kernel.data(),8)<<"\","<<v.right_shift_code<<','<<v.left_shift_code<<']';}
 std::cout<<"],\"calls\":[";for(std::size_t i=0;i<calls.size();++i){std::cout<<(i?",[":"[");for(unsigned j=0;j<calls[i].size();++j)std::cout<<(j?",":"")<<calls[i][j];std::cout<<']';}
 std::cout<<"],\"reads\":[";for(std::size_t i=0;i<reads.size();++i)std::cout<<(i?",":"")<<reads[i];std::cout<<"]}\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}}
