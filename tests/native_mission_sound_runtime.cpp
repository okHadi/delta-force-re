#include "dfre/mission_sound_runtime.hpp"
#include "dfre/item_callbacks.hpp"
#include "dfre/mission.hpp"
#include <chrono>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <type_traits>
static_assert(!std::is_constructible<dfre::MissionSoundRuntime,const dfre::AssetStore&,const dfre::AssetBytes&,dfre::MissionAudioFrameCells&,const dfre::TerrainGeometry*&&,dfre::MissionSoundRuntimeBindings>::value,"Current terrain must be a stable pointer lvalue");
static_assert(!std::is_constructible<dfre::MissionSoundRuntime,dfre::AssetStore&&,const dfre::AssetBytes&,dfre::MissionAudioFrameCells&,const dfre::TerrainGeometry*&,dfre::MissionSoundRuntimeBindings>::value,"Asset storage must stay alive");
namespace {
using U=std::uint32_t;
void check(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
template<class F>void fails(F fn){bool failed=false;try{fn();}catch(const std::exception&){failed=true;}check(failed,"Missing runtime service was accepted");}
struct Fixture {
 std::filesystem::path path=std::filesystem::temp_directory_path()/("dfre-sound-runtime-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
 Fixture(){std::filesystem::create_directory(path);for(auto name:{"Dfbase.pff","Df.pff"}){const unsigned char header[]={20,0,0,0,'P','F','F','3',0,0,0,0,32,0,0,0,20,0,0,0};std::ofstream out(path/name,std::ios::binary);out.write(reinterpret_cast<const char*>(header),20);}}
 ~Fixture(){std::error_code e;std::filesystem::remove_all(path,e);}
};
}
int main(int argc,char** argv){try{
 Fixture fixture;dfre::AssetStore empty(fixture.path);dfre::Fixed3 listener{};U yaw=0;dfre::MissionAudioFrameCells frame{0,listener,yaw};const dfre::TerrainGeometry* terrain=nullptr;
 fails([&]{dfre::MissionSoundRuntime owner(empty,{},frame,terrain,{});});
 if(argc==1)return 0;
 check(argc==2,"Use: native_mission_sound_runtime [GAME_FOLDER]");const std::filesystem::path game(argv[1]);dfre::AssetStore assets(game);std::ifstream input(game/"Df.exe",std::ios::binary);dfre::AssetBytes executable((std::istreambuf_iterator<char>(input)),{});
 auto wrong=executable;wrong.back()^=1;fails([&]{dfre::MissionSoundRuntime owner(assets,wrong,frame,terrain,{});});
 dfre::MissionSoundRuntime owner(assets,executable,frame,terrain,{});const auto& b=owner.asset_bindings();
 fails([&]{b.read(0x7cca80,4);});fails([&]{b.write(0x9f3098+0x1c,4,7);});fails([&]{b.call(0x455c90,{0});});fails([&]{owner.initialize_definitions("LOOP.DEF");});
 check(frame.counter_4f7ea8==b.read(0x4f7ea8,4),"Frame counter has duplicate storage");b.write(0x4f7ea8,4,7);check(frame.counter_4f7ea8==7,"Frame counter is not shared");
 b.write(0x6e0700,4,123);check(frame.registrations_6e0700[0].definition==123,"Registration storage is not shared");frame.registrations_6e0700[0]={};
 listener={1,2,3};yaw=9;check(b.read(0x6e06a8,4)==1&&b.read(0x6e06ac,4)==2&&b.read(0x6e06b0,4)==3&&b.read(0x6e06b4,4)==9,"Listener storage is not shared");
 check(b.read(0x6e06b3,2)==0x900,"Mixed-width listener/yaw alias is not shared");b.write(0x6e06b3,2,0xa1b2);check(U(listener.z)==0xb2000003&&yaw==0xa1,"Mixed-width store did not reach both owners");listener={1,2,3};yaw=9;
 fails([&]{b.read(0xbd5284,4);});dfre::TerrainGeometry low(dfre::AssetBytes(1024*1024,5),dfre::AssetBytes(1024*1024),std::vector<U>(65536)),high(dfre::AssetBytes(1024*1024,100),dfre::AssetBytes(1024*1024),std::vector<U>(65536));
 terrain=&low;const auto at=b.read(0xbd5284,4);check(b.read(at+99,1)==5,"Current terrain was not read");terrain=&high;check(b.read(at+99,1)==100,"Changed terrain was not read");
 std::array<std::uint8_t,0x114> record{};dfre::MissionSoundRuntimeBindings bindings;
 bindings.record_base=[](U address,U width)->std::optional<U>{if(address>=0x7cca80&&std::uint64_t(address-0x7cca80)+width<=0x114)return 0x7cca80;return std::nullopt;};
 bindings.record_read=[&](U base,U offset,U width){check(base==0x7cca80,"Wrong current record base");U value=0;for(U i=0;i<width;++i)value|=U(record.at(offset+i))<<(8*i);return value;};
 bindings.record_store=[&](U base,U offset,U width,U value){check(base==0x7cca80,"Wrong current record store base");for(U i=0;i<width;++i)record.at(offset+i)=std::uint8_t(value>>(8*i));};
 U key_reads=0;bindings.script_key=[&](){++key_reads;return 0x04960552u;};
 dfre::MissionSoundRuntime current(assets,executable,frame,terrain,bindings);const auto& c=current.asset_bindings();c.write(0x7cca9c,4,0x12345678);check(record[0x1c]==0x78&&c.read(0x7cca9d,1)==0x56,"Current record aliases are not shared");
 fails([&]{c.read(0x7cca80+0x113,4);});
 const auto mission=dfre::load_mission(assets,"C08M47.BMS");check(dfre::mission_dialogue_archive(mission).empty(),"Ambient fixture needs an empty dialogue header");for(U i=0;i<mission.header.size();++i)c.write(0x8cf908+i,1,mission.header[i]);current.initialize_definitions("LOOP.DEF");check(key_reads==2,"Ambient passes do not read the current key separately");current.resolve_definitions("SNIPER.PWF","NEW.PWF");U ambient=0;
 for(U id=1;id<512;++id)if(c.read(0x4f2ea4+id*40+36,1)&2){ambient=id;break;}
 check(ambient!=0,"Stock ambient definition is missing");record[0]=1;current.attach(0x7cca80,ambient,255,255);
 auto items=dfre::build_runtime_items(dfre::load_item_catalog(assets));U lookups=0,bound=0;dfre::bind_runtime_item_callbacks(items,[&](std::string_view name){++lookups;return current.definition_id(name);});
 for(const auto& item:items)for(auto id:item.ambient_handles)bound+=id!=0;
 check(lookups==items.size()*4&&bound!=0,"Actual item ambient names did not bind to live sound definitions");check(current.definition_id("")==0&&current.definition_id("not_a_sound")==0,"Unknown sound name was accepted");
 check(frame.registrations_6e0700[0].actor==0x7cca80&&c.read(0x7cca9c,4)==0x6e0700,"Attached sound did not use the shared registration");
 check(frame.counter_4f7ea8==7&&c.read(0x4f7ea4,4)!=0,"Identity and frame counters overlap");
 current.frame();check(frame.counter_4f7ea8==0&&frame.handles_6e06e8[0]!=0,"Shared scheduler did not start an ambient voice");current.publish();current.advance_frames(100);
 return 0;
}catch(const std::exception&){return 1;}}
