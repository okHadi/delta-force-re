#include "dfre/mission_host_pump.hpp"
#include <iostream>
#include <sstream>
#include <map>
#include <array>
using namespace dfre;
namespace {
using U=std::uint32_t;
const std::array<U,29> keys{{0x5012e0,0x5012d4,0x5012d8,0x5012ec,0x49101c,0x5012e4,0x5012dc,0x5012f8,0xd08948,0x5012f4,0xd08954,0xd0894c,0xd08940,0x501950,0x1300010,0x501980,0x501974,0x50198c,0xcc0884,0xcc08bc,0x491014,0x491018,0x491020,0x491028,0x491024,0xcc0868,0x49102c,0x491010,0x49100c}};
U next(std::istream& in){std::uint64_t n;if(!(in>>n))throw MissionHostPumpError("Short pump row");return U(n);}
}
int main(){try{std::string line;while(std::getline(std::cin,line)){if(line.empty())continue;std::istringstream in(line);const U mode=next(in),mutation=next(in);std::map<U,U> words;for(U key:keys)words[key]=next(in);words[0x491030]=99;
const U read_count=next(in),read_success=next(in),queue_limit=next(in),message_count=next(in),quit_index=next(in),cap_min=next(in),cap_max=next(in);
std::vector<std::uint8_t> bytes(0x48000);for(U i=0;i<bytes.size();++i)bytes[i]=std::uint8_t(i*37+11);
std::ostringstream trace;U queues=0,reads=0,messages=0;MissionHostPumpBindings b;
b.word=[&](U a){return words[a];};b.store=[&](U a,U v){words[a]=v;};b.byte=[&](U a){return bytes.at(a-0xcc0930u);};b.store_byte=[&](U a,std::uint8_t v){bytes.at(a-0xcc0930u)=v;};
b.call=[&](U site,const std::vector<U>& args){trace<<'C'<<site;for(U v:args)trace<<','<<v;trace<<';';
if(site==0x1300100){if(mutation&1)words[0x5012e0]=0;if(mutation&2){words[0x5012dc]=91;words[0x5012e0]=7;}}
if(site==0x493cd0 && (mutation&4))words[0x5012f8]=1;
if(site==0x49efd0 && (mutation&8)){words[0x501950]=0;words[0x50198c]=91;}
if(site==0x49ef20 && (mutation&16))words[0x50198c]=17;
if(site==0x4b5243){++queues;if(mutation&32)words[0xd08954]=16;return queues<=queue_limit?1u:0u;}
if(site==0x4c658c && (mutation&64))words[0xcc0868]=1;
if(site==0x4c65a0){if(mutation&128)words[0xcc0868]=2;return 123u;}
return 0u;};
b.read_file=[&](U file,U buffer,U capacity){trace<<'R'<<file<<','<<buffer<<','<<capacity<<';';++reads;if(reads>1)return MissionHostRead{true,0};for(U i=0;i<read_count && i<capacity;++i)bytes.at(buffer-0xcc0930u+i)=std::uint8_t(i*19+3);if(mutation&256){words[0xd08954]=words[0xd08954]==16?8:16;words[0xd0894c]=87;}return MissionHostRead{bool(read_success),read_count};};
b.seek_file=[&](U file,U at){trace<<'S'<<file<<','<<at<<';';if(mutation&512)words[0x5012f4]=14;};
b.peek_remove=[&]()->std::optional<MissionHostMessage>{trace<<'P'<<';';if(messages>=message_count)return std::nullopt;const U index=messages++;return MissionHostMessage{index==quit_index?0x12u:0x100u+index,index};};
b.translate=[&](const auto& m){trace<<'T'<<m.kind<<';';};b.dispatch=[&](const auto& m){trace<<'D'<<m.kind<<';';if(mutation&1024)words[0x5012e0]=0;};b.timer_caps=[&](){trace<<'G'<<';';return std::array<U,2>{{cap_min,cap_max}};};
bool fault=false,keep=true;try{switch(mode){case 0:mission_host_clock_callback(b);break;case 1:mission_host_cd_upkeep(b);break;case 2:mission_host_midi_upkeep(b);break;case 3:mission_host_pcm_upkeep(b);break;case 4:keep=mission_host_pump(b);break;case 5:mission_host_clock_configure(b);break;default:throw MissionHostPumpError("Bad mode");}}catch(const MissionHostPumpError&){fault=true;}
U hash=0xffffffffu;for(auto v:bytes){hash^=v;for(U bit=0;bit<8;++bit)hash=(hash>>1)^((hash&1)?0xedb88320u:0);}hash^=0xffffffffu;
std::cout<<fault<<' '<<keep;for(U key:keys)std::cout<<' '<<words[key];std::cout<<' '<<words[0x491030]<<' '<<hash<<' '<<(trace.str().empty()?"-":trace.str())<<'\n';
}}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}return 0;}
