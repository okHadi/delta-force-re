#include "dfre/mission_script_registry.hpp"
#include "dfre/mission_sound_runtime.hpp"
#include "dfre/mission_rtxt.hpp"
#include "dfre/mission.hpp"
#include "dfre/input_config.hpp"
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <stdexcept>
#define CHECK(x) do{if(!(x))throw std::runtime_error("script registry: " #x);}while(false)
namespace {
using namespace dfre;using U=std::uint32_t;
struct Memory {
 std::map<U,std::uint8_t> bytes;U thread=0x19000000,messages=0;
 U read(U at,U n){U value=0;for(U i=0;i<n;++i)value|=U(bytes.at(at+i))<<(i*8);return value;}
 void write(U at,U n,U value){for(U i=0;i<n;++i)bytes.at(at+i)=std::uint8_t(value>>(i*8));}
 void block(U at,U n){for(U i=0;i<n;++i)bytes[at+i]=0;}
 void text(U at,std::string_view text){for(std::size_t i=0;i<text.size();++i)bytes[at+U(i)]=std::uint8_t(text[i]);bytes[at+U(text.size())]=0;}
};
void small(){
 Memory m;m.block(0x1000,512);m.block(0x2000,256);m.block(m.thread,64);m.block(0x4ceb80,8);m.block(0x67f51c,4);m.block(0x5013f4,256);
 for(U i=0;i<8;++i)m.write(0x4ceb80+i,1,1u<<i);
 for(U c=0;c<255;++c)m.write(0x5013f4+((c+1)&255),1,((c==' '||c=='\t')?2u:0u)|((c>='0'&&c<='9')?0x20u:0u));
 MissionSoundBankBindings b;b.read=[&](U at,U n){return m.read(at,n);};b.write=[&](U at,U n,U value){m.write(at,n,value);};b.call=[&](U site,const std::vector<U>& args){if(site==0x501d60)return m.thread;if(site==0x49a620){CHECK(args[1]==0);for(U i=0;i<args[2];++i)m.write(args[0]+i,1,0);return args[0];}if(site==0x479f00){CHECK(args==std::vector<U>({0x1000,0xffffffffu}));++m.messages;return 0u;}throw std::runtime_error("missing unit service");};
 m.text(0x1000,"..dfdlg05.dbf");m.text(0x1100,".");CHECK(mission_script_registry_token(b,0x1000,0x1100,0x2000)==0x1002&&m.read(m.thread+16,4)==0x100a);CHECK(mission_script_registry_token(b,0,0x1100,0x2000)==0x100a&&m.read(m.thread+16,4)==0);
 m.text(0x1001,"same");m.text(0x1102,"same");CHECK(mission_script_registry_compare(b,0x1001,0x1102)==0);m.text(0x1102,"samp");CHECK(mission_script_registry_compare(b,0x1001,0x1102)==-1);
 m.text(0x1000," -2147483649tail");CHECK(mission_script_registry_id(b,0x1000)==0x7fffffffu);
 m.write(0x67f51c,4,256);CHECK(mission_script_registry_step(b,0x2000)&&m.read(0x2018,4)==0&&m.read(0x201c,4)==255&&m.read(0x2020,4)==0x10000);m.write(0x67f51c,4,0);m.write(0x2018,4,77);CHECK(!mission_script_registry_step(b,0x2000)&&m.read(0x2018,4)==77);
 mission_script_registry_text(b,0);m.text(0x1000,"");mission_script_registry_text(b,0x1000);CHECK(m.messages==0);m.text(0x1000,"dialogue");mission_script_registry_text(b,0x1000);CHECK(m.messages==1);
}
void stock(const std::filesystem::path& folder){
 AssetStore assets(folder);std::ifstream f(folder/"Df.exe",std::ios::binary);AssetBytes executable((std::istreambuf_iterator<char>(f)),{});Fixed3 listener{};U yaw=0;MissionAudioFrameCells frame{0,listener,yaw};const TerrainGeometry* terrain=nullptr;
 MissionScriptQueueCells queue{};U host=0;MissionScriptRuntimeCells cells{queue,host};std::unique_ptr<MissionScriptRegistry> registry;MissionSoundRuntime* sound_ptr=nullptr;
 unsigned texts=0,diagnostics=0;MissionSoundRuntimeBindings sound_host;sound_host.script_key=[](){return 0x04960552u;};sound_host.service=[&](U site,const std::vector<U>& args)->U{
  if(registry){if(const auto result=registry->service(site,args))return *result;}
  if(site==0x499f1f)return mission_rtxt_load(sound_ptr->asset_bindings(),args.at(0));
  if(site==0x49a076)return mission_rtxt_entry(sound_ptr->asset_bindings(),args.at(0),args.at(1));
  if(site==0x479f00){CHECK(args.size()==2&&args[1]==0xffffffffu);const auto& memory=registry->memory();CHECK(memory.read(args[0],1)!=0);++texts;return 0;}
  throw std::runtime_error("unbound stock script service "+std::to_string(site));
 };
 MissionSoundRuntime sound(assets,executable,frame,terrain,sound_host);sound_ptr=&sound;const auto& b=sound.asset_bindings();
 const auto scratch=b.call(0x497f60,{0x4000,4,0,0}),thread=b.call(0x497f60,{64,4,0,0});for(U i=0;i<64;i+=4)b.write(thread+i,4,0);b.write(thread+0xc,4,1);
 auto settings=default_game_config(6);std::ifstream cfg(folder/"df.cfg",std::ios::binary);std::string config((std::istreambuf_iterator<char>(cfg)),{});load_game_config_text(settings,config);U volume=U(settings.dialog_volume);
 MissionScriptRegistryBindings script_host;script_host.crt_thread=[thread](){return thread;};script_host.voice_volume=[&](){return volume;};script_host.missing_audio=[&](U){++diagnostics;throw std::runtime_error("stock dialogue sample missing");};script_host.message=[](U,const std::array<std::uint8_t,10>&){throw std::runtime_error("single-player host-zero announced a script");};
 registry=std::make_unique<MissionScriptRegistry>(cells,sound.bank_bindings(),scratch,0x4000,script_host);
 for(const auto at:{0x8e3d73u,0x8e3d77u,0x8e3e73u,0x8e3f3fu}){
  bool rejected_read=false,rejected_write=false,rejected_claim=false;
  try{registry->memory().read(at,2);}catch(const MissionTickError&){rejected_read=true;}
  try{registry->memory().write(at,2,1);}catch(const MissionTickError&){rejected_write=true;}
  try{registry->owns(at,2);}catch(const MissionTickError&){rejected_claim=true;}
  CHECK(rejected_read&&rejected_write&&rejected_claim);
 }
 CHECK(registry->owns(0x8e3d64,4)&&!registry->owns(0x67f51c,4)&&!registry->owns(0x8e3d58,4));

 const auto mission=load_mission(assets,"C05M07.BMS");CHECK(mission_dialogue_archive(mission)=="dfdlg05");for(U i=0;i<mission.header.size();++i)b.write(0x8cf908+i,1,mission.header[i]);
 sound.initialize_definitions("LOOP.DEF");sound.resolve_definitions("SNIPER.PWF","NEW.PWF");const auto& memory=registry->memory();CHECK(memory.read(0x8e3d54,4)==120&&cells.dictionary_8e3f3c!=0&&queue.enabled_8e3d68==0x43e430&&queue.scale_8e3d64==62);CHECK(memory.read(0x8e3d6c,4)==0x43e470);
 U node=memory.read(0x8e3d58,4),nodes=0,steps=0;while(node){const auto resource=memory.read(node,4);CHECK(resource!=0);++nodes;steps+=memory.read(resource+0x1c,4);node=memory.read(node+4,4);}CHECK(nodes==120&&steps==142&&queue.entries_8e3d74.empty());
 registry->start().event(706);CHECK(queue.entries_8e3d74.size()==1);const auto resource=queue.entries_8e3d74[0].resource;CHECK(memory.read(resource+0x1c,4)==1);
 U requested=0,stat=0;MissionProfilerCells profiler{};std::array<U,3> timers{};Fixed3 camera_position{};MissionCameraCells camera{camera_position,{},0,0};MissionFrameServices services({requested,profiler,timers,stat,camera,queue},registry->frame_bindings());services.scripts(0);
 const auto configured_volume=volume;CHECK(queue.entries_8e3d74.empty()&&diagnostics==0);CHECK(volume==0?cells.voice_count_8e3d50==0:cells.voice_count_8e3d50==1);CHECK(texts==0);
 if(!volume){volume=200;registry->start().event(706);services.scripts(0);CHECK(queue.entries_8e3d74.empty()&&cells.voice_count_8e3d50==1&&diagnostics==0);}
 CHECK(b.call(0x494ff0,{cells.voices[0].handle})!=0);sound.publish();sound.advance_frames(22050*30);registry->service(0x455b40,{});CHECK(cells.voice_count_8e3d50==0);
 CHECK(b.call(0x4980d0,{0})==0xffffffffu);const auto temporary=b.call(0x497f60,{16,4,0,0});CHECK(b.call(0x4980d0,{temporary})==0);bool rejected=false;try{b.call(0x4980d0,{temporary});}catch(const MissionSoundAssetError&){rejected=true;}CHECK(rejected);
 registry->service(0x455ea0,{});CHECK(memory.read(0x8e3d58,4)==0&&cells.dictionary_8e3f3c==0&&queue.entries_8e3d74.empty());
 std::cout<<"stock registry120dialogues142steps, volume="<<volume<<", configured_volume="<<configured_volume<<", texts="<<texts<<", device_output=0\n";
}
}
int main(int argc,char** argv){try{small();if(argc==2)stock(argv[1]);else CHECK(argc==1);std::cout<<"script registry checks passed\n";return 0;}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
