#include "dfre/mission_positional_sound.hpp"
#include "dfre/mission_audio_frame.hpp"
#include "dfre/mission_audio_cache.hpp"
#include "dfre/mission_audio_playback.hpp"
#include <iostream>
#include <map>
#include <stdexcept>
#define CHECK(x) do{if(!(x))throw std::runtime_error("positional sound: " #x);}while(false)
using namespace dfre;
namespace {using U=std::uint32_t;constexpr U B=0x3100000,P=0x2007fd0,D=0x3300010,A=B+0x300;}
int main(){try{
 MissionSoundBankBindings none;CHECK(mission_sound_occlusion(none,{1,2,3},{1,2,999})==0x10000);CHECK(mission_sound_occlusion(none,{0,0,0},{65535,65535,0})==0x10000);bool failed=false;try{mission_sound_occlusion(none,{0,0,0},{65536,0,0});}catch(const MissionSoundBankError&){failed=true;}CHECK(failed);
 // Each terrain sample reloads the current source pointer and reads one byte.
 std::vector<U> reads;U pointer=0x3400000;MissionSoundBankBindings terrain;terrain.read=[&](U at,U width)->U{reads.push_back(at);if(at==0xbd5284){CHECK(width==4);return pointer;}CHECK(width==1);if(at==0x3400000){pointer=0x3500000;return 0;}CHECK(at==0x3500001);return 255;};
 CHECK(mission_sound_occlusion(terrain,{0,0,0},{65536,0,0})==(512+63)*128);CHECK((reads==std::vector<U>{0xbd5284,0x3400000,0xbd5284,0x3500001}));
 std::map<U,std::uint8_t> bytes;std::array<std::uint8_t,160> sample{};
 auto put=[&](U at,U value,U width=4){for(U i=0;i<width;++i){const auto address=at+i;if(address>=D-16&&address<D-16+sample.size())sample[address-(D-16)]=std::uint8_t(value>>(i*8));else bytes[address]=std::uint8_t(value>>(i*8));}};
 auto get=[&](U at,U width){U result=0;for(U i=0;i<width;++i){const auto address=at+i;std::uint8_t value=0;if(address>=D-16&&address<D-16+sample.size())value=sample[address-(D-16)];else{const auto found=bytes.find(address);if(found!=bytes.end())value=found->second;}result|=U(value)<<(i*8);}return result;};
 Fixed3 listener{};U yaw=0;MissionAudioFrameCells frame{0,listener,yaw};U enabled=1;MissionAudioPlaybackCells voices{enabled};MissionAudioPlaybackBindings playback_bindings;unsigned pcm_reads=0;
 playback_bindings.sample=[&](U handle){CHECK(handle==D);return MissionAudioSampleView{sample.data()+16,sample.size()-16,D};};playback_bindings.pcm_byte=[&](U at){CHECK(at>=D+16&&at<D+24);++pcm_reads;return std::uint8_t(get(at,1));};MissionAudioPlayback playback(voices,playback_bindings);playback.initialize();
 MissionAudioCacheBindings cache;cache.read=get;cache.write=[&](U at,U width,U value){put(at,value,width);};std::vector<U> cache_calls;
 cache.call=[&](U site,const std::vector<U>& args)->U{cache_calls.push_back(site);if(site==0x497f60){CHECK(args.at(0)==64&&args.at(1)==4);return D;}if(site==0x498d60){CHECK(args.at(0)==17&&args.at(1)==0);return 0;}if(site==0x498d30){CHECK(args.at(0)==D&&args.at(1)==64);return 64;}if(site==0x4c6854){CHECK(args.at(0)==7);return 0;}throw std::runtime_error("unbound cache I/O fixture");};
 put(0x4cdff0,0x12345678);put(D+0x18,22050);put(D+0x22,8,2);put(D+0x28,8);for(U i=0;i<8;++i)put(D+0x2c+i,0x80+i,1);put(A+0x18,17);put(A+0x1c,64);put(0xd096d0,7);
 put(0x4f2ea8+50*0x28,B);put(B+0x48,32);put(B+0x24,1);put(B+0x28,B+0x100);put(B+0x1c,0x10000);put(B+0x100,1,2);put(B+0x108,3);put(B+0x110,B+0x200);put(B+0x200,A);put(B+0x204,0x10000);put(0x501500,2);put(0x5014fc,1);put(0x6e0518+0xc4,9);
 MissionSoundBankBindings bank;bank.read=[&](U at,U width){if(at==0x6e06a8)return U(frame.listener_6e06a8.x);if(at==0x6e06ac)return U(frame.listener_6e06a8.y);if(at==0x6e06b0)return U(frame.listener_6e06a8.z);if(at==0x6e06b4)return frame.yaw_6e06b4;return get(at,width);};bank.write=cache.write;std::vector<U> calls;
 bank.call=[&](U site,const std::vector<U>& args)->U{calls.push_back(site);switch(site){case 0x499120:CHECK(args.at(0)==0x6e0518+0x3c);return 0;case 0x49d050:mission_audio_cache_load(cache,args.at(0),args.at(1));return 0;case 0x499020:return mission_audio_cache_close(cache);case 0x498230:return mission_audio_cache_tag(cache,args.at(0),args.at(1));case 0x494dd0:return playback.play(args.at(0),args.at(1),args.at(2),args.at(3));default:throw std::runtime_error("unbound positional bank body");}};
 mission_sound_positional_request(bank,50,0,0,0,P);CHECK((calls==std::vector<U>{0x499120,0x49d050,0x499020,0x498230,0x494dd0}));CHECK((cache_calls==std::vector<U>{0x497f60,0x498d60,0x498d30,0x4c6854}));CHECK(get(A+0x23,1)==1&&get(A+0x24,4)==D&&get(P+4,4)==255&&get(P+0x14,4)==0&&get(mission_sound_bank_scratch,4)==1&&playback.playing(1)==1&&enabled==1);
 playback.advance_frames(32);CHECK(pcm_reads>0&&playback.playing(1)==0&&enabled==1);
 const auto count=calls.size();put(B+0x48,0);mission_sound_positional_request(bank,50,0x100000,0,0,P);CHECK(calls.size()==count);bank.call={};put(B+0x48,32);failed=false;try{mission_sound_positional_request(bank,50,0,0,0,P);}catch(const MissionSoundBankError&){failed=true;}CHECK(failed);
 failed=false;try{mission_sound_positional_request(bank,512,0,0,0,P);}catch(const MissionSoundBankError&){failed=true;}CHECK(failed);
 std::cout<<"positional sound checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
