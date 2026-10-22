#include "dfre/mission_audio_playback.hpp"
#include "dfre/audio_mixer.hpp"
#include <iostream>
#include <stdexcept>
#define CHECK(x) do{if(!(x))throw std::runtime_error("audio playback: " #x);}while(false)
using namespace dfre;
namespace {
void put(std::uint8_t* p,unsigned at,std::uint32_t n){for(unsigned i=0;i<4;++i)p[at+i]=std::uint8_t(n>>(8*i));}
std::uint32_t get(const std::uint8_t* p,unsigned at){std::uint32_t n=0;for(unsigned i=0;i<4;++i)n|=std::uint32_t(p[at+i])<<(8*i);return n;}
}
int main(){try{
 std::uint32_t enabled=1;MissionAudioPlaybackCells cells{enabled};std::array<std::uint8_t,80> sample{};put(sample.data(),4,32);put(sample.data(),8,0x10000000);
 unsigned resolved=0,reads=0,completed=0;MissionAudioPlaybackBindings b;
 b.sample=[&](auto handle){CHECK(handle==0x1500000);++resolved;return MissionAudioSampleView{sample.data(),sample.size(),handle};};
 b.pcm_byte=[&](auto address){CHECK(address>=0x1500010&&address<0x1500050);++reads;return std::uint8_t(0x80);};
 b.completed=[&](auto address,auto token,auto source){CHECK(address==0x480123&&token==77&&source==0x1500000);++completed;put(cells.records_4a16d2.data(),0x4a1d52-mission_audio_voice_base+3*0x30+0x10,99);};
 MissionAudioPlayback playback(cells,b);playback.initialize();
 CHECK(cells.filter_4a4bb4==0xfff&&cells.filter_4a4bc4==0x4b4bd4);
 auto* pending=cells.records_4a16d2.data()+0x4a1a12-mission_audio_voice_base+3*0x34;pending[0x31]=0xa5;pending[0x32]=0x5a;
 const auto handle=playback.play_channel(3,0x1500000,192,128,0x10000);CHECK(handle==1&&playback.playing(handle)==handle&&resolved==1);
 CHECK(pending[0x31]==0xa5&&pending[0x32]==0x5a);playback.advance_frames(32);CHECK(reads>0&&playback.playing(handle)==0&&enabled==1);
 const auto ambient_handle=playback.start_ambient(0x1500000,0,128,0x10000,5);CHECK(ambient_handle==2);playback.advance_frames(128);CHECK(playback.playing(ambient_handle)==ambient_handle);
 playback.stop(ambient_handle);playback.advance_frames(1);CHECK(playback.playing(ambient_handle)==0);
 const auto callback_handle=playback.play_channel(3,0x1500000,0,128,0x10000);CHECK(callback_handle==3);put(pending,0x2c,77);playback.register_completion(0x480123);playback.advance_frames(32);
 CHECK(completed==1&&playback.playing(99)==99); // live IDs stay visible with active byte0
 auto* current=cells.records_4a16d2.data()+3*0x34;const auto original_pan=get(current,0x18),original_volume=get(current,0x1c);
 CHECK(playback.update(99,64,200,0xffffffff)==99);CHECK(get(current,0x18)==original_pan&&get(current,0x1c)==original_volume);
 const auto generation=cells.generation_4a2ae0;playback.initialize();CHECK(cells.generation_4a2ae0==generation&&cells.completion_4a2ae8==0x480123);
 enabled=0;const auto previous=resolved;CHECK(playback.play(123,0,0,0)==0&&resolved==previous);playback.register_completion(0);CHECK(cells.completion_4a2ae8==0x480123);
 enabled=1;MissionAudioPlaybackBindings missing;MissionAudioPlayback no_sample(cells,missing);
 bool failed=false;auto* incoming=cells.records_4a16d2.data()+0x4a2052-mission_audio_voice_base;incoming[0x30]=1;
 try{no_sample.start_ambient(123,0,0,0,0);}catch(const std::runtime_error&){failed=true;}CHECK(failed&&incoming[0x30]==0);
 for(unsigned i=0;i<16;++i)cells.records_4a16d2[0x4a1a12-mission_audio_voice_base+i*0x34+0x30]=1;
 CHECK(no_sample.play(123,0,0,0)==0); // full queue does not read a sample
 playback.initialize();const auto missing_handle=playback.play_channel(3,0x1500000,0,128,0x10000);CHECK(missing_handle!=0);put(pending,0x2c,77);
 MissionAudioPlaybackBindings no_completion=b;no_completion.completed={};MissionAudioPlayback incomplete(cells,no_completion);failed=false;
 try{incomplete.advance_frames(32);}catch(const std::runtime_error&){failed=true;}CHECK(failed);
 playback.initialize();(void)playback.play_channel(3,0x1500000,0,128,0x10000);MissionAudioPlaybackBindings no_pcm=b;no_pcm.pcm_byte={};MissionAudioPlayback unreadable(cells,no_pcm);failed=false;
 try{unreadable.advance_frames(1);}catch(const std::runtime_error&){failed=true;}CHECK(failed);
 playback.initialize();const auto old_reads=reads;cells.disabled_4a2712=1;(void)playback.play_channel(3,0x1500000,0,128,0x10000);playback.advance_frames(128);CHECK(reads==old_reads&&pending[0x30]==0);cells.disabled_4a2712=0;
 SampleRecord record;record.length=0;CHECK(start_voice(record,0,0,0,0,0).position==0x400);record.length=0xffffffff;CHECK(start_voice(record,0,0,0,0,0).position==0x800);
 CHECK(gain_kernel(-127).in_window);
 std::cout<<"audio playback checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
