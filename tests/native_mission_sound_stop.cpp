#include "dfre/mission_sound_stop.hpp"
#include "dfre/mission_sound_registration.hpp"
#include "dfre/mission_audio_frame.hpp"
#include "dfre/mission_audio_playback.hpp"
#include "dfre/mission_live.hpp"
#include <iostream>
#include <stdexcept>
using namespace dfre;using U=std::uint32_t;
namespace {
void check(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
template<class B>U get(const B& b,U at,U width=4){U n=0;for(U i=0;i<width;++i)n|=U(b.at(at+i))<<(8*i);return n;}
template<class B>void put(B& b,U at,U width,U n){for(U i=0;i<width;++i)b.at(at+i)=std::uint8_t(n>>(8*i));}
}
int main(){try{
 constexpr U actor=0x7dde80,ambient=0x1305000,sample_id=0x1500000,entry=0x4f2ea8+5*40;
 MissionTickState state;AiActorTables tables;tables.groups[0].resize(1);tables.groups[0][0].active=true;tables.groups[0][0].definition_index=1;init_mission_tick(state,{},std::move(tables),AiItemDefinitionTable(std::vector<AiItemDefinition>(2)),{},123);
 Fixed3 listener{};U yaw=0;MissionAudioFrameCells audio{7,listener,yaw};U identity=1;
 std::array<std::uint8_t,0x50> definition{};put(definition,0x18,4,sample_id);put(definition,0x1c,4,65536);put(definition,0x24,1,255);put(definition,0x25,1,255);
 auto* registrations=reinterpret_cast<std::uint8_t*>(audio.registrations_6e0700.data());
 MissionSoundBankBindings bank;
 bank.read=[&](U at,U width)->U{
  if(at>=actor&&std::uint64_t(at-actor)+width<=0x114)return get(snapshot_mission_actor(state,{0,0}),at-actor,width);
  if(at>=0x6e0700&&std::uint64_t(at-0x6e0700)+width<=16384){U n=0;for(U i=0;i<width;++i)n|=U(registrations[at-0x6e0700+i])<<(i*8);return n;}
  if(at>=ambient&&std::uint64_t(at-ambient)+width<=definition.size())return get(definition,at-ambient,width);
  if(at==entry&&width==4)return ambient;if(at==entry+0x20&&width==1)return 2;if(at==0x4f7ea4&&width==4)return identity;
  throw MissionSoundBankError("Native stop read is not owned");
 };
 bank.write=[&](U at,U width,U n){
  if(at>=actor&&std::uint64_t(at-actor)+width<=0x114){auto image=snapshot_mission_actor(state,{0,0});put(image,at-actor,width,n);apply_mission_actor_image(state,{0,0},image);return;}
  if(at>=0x6e0700&&std::uint64_t(at-0x6e0700)+width<=16384){for(U i=0;i<width;++i)registrations[at-0x6e0700+i]=std::uint8_t(n>>(i*8));return;}
  if(at==0x4f7ea4&&width==4){identity=n;return;}throw MissionSoundBankError("Native stop write is not owned");
 };
 bank.call=[&](U site,const std::vector<U>& args){check(site==0x49a620&&args.size()==3&&args[1]==0&&args[2]==16,"wrong clear body");for(U i=0;i<16;++i)bank.write(args[0]+i,1,0);return args[0];};
 mission_sound_attach(bank,actor,5,0,255);check(audio.registrations_6e0700[0].actor==actor&&bank.read(actor+0x1c,4)==0x6e0700,"attach used another owner");
 U enabled=1;MissionAudioPlaybackCells voice_cells{enabled};std::array<std::uint8_t,80> sample{};put(sample,4,4,32);put(sample,8,4,0x10000000);
 MissionAudioPlaybackBindings voices;voices.sample=[&](U id){check(id==sample_id,"wrong sample");return MissionAudioSampleView{sample.data(),sample.size(),id};};voices.pcm_byte=[](U at){check(at>=sample_id+16&&at<sample_id+80,"PCM range");return std::uint8_t(0x80);};voices.completed=[](U,U,U){throw std::runtime_error("Unexpected completion");};MissionAudioPlayback playback(voice_cells,voices);playback.initialize();
 MissionAudioFrameBindings frame;frame.actor_word=[&](U a,U offset){return bank.read(a+offset,4);};frame.definition_word=[&](U d,U offset){return bank.read(d+offset,(offset==0x24||offset==0x25)?1:4);};
 frame.start=[&](U sample_handle,U pan,U volume,U rate,U priority){return playback.start_ambient(sample_handle,pan,volume,rate,priority);};frame.update=[&](U h,U pan,U volume,U rate){playback.update(h,pan,volume,rate);};unsigned stops=0;frame.stop=[&](U h){++stops;playback.stop(h);};
 mission_audio_frame(audio,frame);const auto handle=audio.handles_6e06e8[0];playback.advance_frames(1);check(handle&&playback.playing(handle)==handle,"scheduler did not start shared voice");
 mission_sound_stop(bank,actor,256);check(audio.registrations_6e0700[0].actor==actor,"scale was truncated");
 mission_sound_stop(bank,actor,0);check(!audio.registrations_6e0700[0].definition&&!bank.read(actor+0x1c,4)&&playback.playing(handle)==handle,"registration stop order differs");
 audio.counter_4f7ea8=7;mission_audio_frame(audio,frame);playback.advance_frames(1);check(stops==1&&!audio.handles_6e06e8[0]&&!playback.playing(handle)&&enabled==1,"scheduler did not stop same voice");
 mission_sound_attach(bank,actor,5,0,255);auto failing=bank;const auto read=bank.read;failing.read=[&](U at,U width){if(at==actor+0x1c)throw MissionSoundBankError("Stopped actor read");return read(at,width);};bool failed=false;try{mission_sound_stop(failing,actor,0);}catch(const MissionSoundBankError&){failed=true;}check(failed&&!audio.registrations_6e0700[0].definition&&bank.read(actor+0x1c,4)==0x6e0700,"read failure lost clear prefix");
 std::cout<<"sound stop checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
