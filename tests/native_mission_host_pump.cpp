#include "dfre/mission_host_pump.hpp"
#include <cassert>
#include <map>
#include <string>
using namespace dfre;
int main(){using U=std::uint32_t;std::map<U,U> words;MissionHostPumpBindings b;b.word=[&](U a){return words[a];};b.store=[&](U a,U v){words[a]=v;};
words[0xcc0884]=1;words[0x491018]=3;words[0x491014]=0xfffffffeu;words[0x491020]=0xffffffffu;words[0x491028]=1;words[0x49101c]=0xffffffffu;words[0x491024]=0;
mission_host_clock_callback(b);assert(words[0x491014]==1 && words[0x491020]==0 && words[0x49101c]==0);
MissionHostClockDelivery delivery(10);assert(delivery.sample(0xfffffff0u,b)==0);assert(delivery.sample(4,b)==2);assert(words[0x491014]==7);words[0xcc0884]=0;assert(delivery.sample(29,b)==2);assert(words[0x491014]==7);words[0xcc0884]=1;assert(delivery.sample(34,b)==1);assert(words[0x491014]==10);delivery.reset();assert(delivery.sample(100,b)==0);
bool failed=false;try{MissionHostClockDelivery invalid(0);}catch(const MissionHostPumpError&){failed=true;}assert(failed);
words.clear();words[0x5012e0]=1;words[0x5012d4]=1;words[0x49101c]=1;words[0x5012e4]=55;words[0x5012dc]=4;std::string trace;
b.call=[&](U site,const std::vector<U>& args){if(site==55){trace+='C';assert(args==std::vector<U>{4});words[0x5012e0]=7;words[0x5012dc]=8;}else{assert(site==0x493cd0);trace+='R';assert((args==std::vector<U>{8,7}));}return 0;};mission_host_cd_upkeep(b);assert(trace=="CR");
words.clear();U peek=0;b.peek_remove=[&]()->std::optional<MissionHostMessage>{++peek;return MissionHostMessage{0x12,0};};b.call={};assert(!mission_host_pump(b));assert(peek==1);
b.peek_remove=[]()->std::optional<MissionHostMessage>{return MissionHostMessage{1,0};};failed=false;try{mission_host_pump(b);}catch(const MissionHostPumpError&){failed=true;}assert(failed);
words.clear();words[0x5012f8]=1;words[0x5012f4]=0;words[0xd08948]=4;words[0xd08940]=0;mission_host_pcm_upkeep(b); // disabled mixer stops before file access
words[0xd08940]=1;failed=false;try{mission_host_pcm_upkeep(b);}catch(const MissionHostPumpError&){failed=true;}assert(failed);
// Seven 8-bit bytes copy one DWORD. Tail bytes retain their ring values.
words.clear();words[0x5012f8]=1;words[0xd08954]=8;words[0x5012f4]=15;words[0xd08940]=1;
std::map<U,std::uint8_t> bytes;for(U i=0;i<8;++i){bytes[0xd00930+i]=std::uint8_t(i+1);bytes[0xcc0930+i]=0xee;}
b.byte=[&](U a){return bytes[a];};b.store_byte=[&](U a,std::uint8_t v){bytes[a]=v;};
b.read_file=[&](U,U buffer,U cap){assert(buffer==0xd00930 && cap==0x4000);return MissionHostRead{true,7};};
b.seek_file=[&](U,U at){assert(at==0x2c);};b.call=[&](U site,const auto& args){assert(site==0x4b5243);assert((args==std::vector<U>{0xcc0930,7,0}));return 0;};
mission_host_pcm_upkeep(b);assert(words[0x5012f4]==0 && words[0xd08948]==7);for(U i=0;i<4;++i)assert(bytes[0xcc0930+i]==i+1);for(U i=4;i<8;++i)assert(bytes[0xcc0930+i]==0xee);
// Read and seek callbacks change current format, file and destination slot.
words[0xd08948]=0;words[0xd0894c]=10;words[0xd08954]=8;words[0x5012f4]=0;
b.read_file=[&](U file,U,U){assert(file==10);words[0xd0894c]=20;words[0xd08954]=16;return MissionHostRead{true,4};};
b.seek_file=[&](U file,U){assert(file==20);words[0x5012f4]=14;};
b.call=[&](U site,const auto& args){assert(site==0x4b5243);assert((args==std::vector<U>{0xcfc930,4,0}));return 0;};
mission_host_pcm_upkeep(b);assert(words[0x5012f4]==15 && words[0xd08948]==4);assert(bytes[0xcfc930]==(2^0x80) && bytes[0xcfc933]==(8^0x80));
words[0xd08948]=0;words[0xd08954]=16;b.read_file=[](U,U,U){return MissionHostRead{true,2};};failed=false;try{mission_host_pcm_upkeep(b);}catch(const MissionHostPumpError&){failed=true;}assert(failed && words[0x5012f4]==15);
words.clear();words[0x501950]=0x1000;words[0x1010]=4;words[0x501980]=1;words[0x49101c]=0;words[0x501974]=0xfffff69fu;words[0x50198c]=9;trace.clear();b.call=[&](U site,const auto&){if(site==0x49efd0){trace+='S';words[0x501950]=0;}else{assert(site==0x49ef20);trace+='R';words[0x50198c]=7;}return 0;};mission_host_midi_upkeep(b);assert(trace=="SR" && words[0x50198c]==0);
words.clear();words[0xcc0868]=1;words[0x49102c]=7;b.timer_caps=[](){return std::array<U,2>{{1,10}};};b.call=[&](U site,const auto& args){if(site==0x4c65a0){assert((args==std::vector<U>{1,0,0x4906a0,7,1}));words[0xcc0868]=2;return 42u;}assert(site==0x4c658c);return 0u;};mission_host_clock_configure(b);assert(words[0x49100c]==42 && words[0x491024]==0 && words[0x491028]==1030791936 && words[0x491030]==1);
return 0;}
