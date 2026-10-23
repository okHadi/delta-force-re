#include "dfre/mission_model_cleanup.hpp"
#include "dfre/mission_model_registry.hpp"
#include "dfre/mission_model_load.hpp"
#include "dfre/mission_model_texture.hpp"
#include "dfre/mission_model_lod.hpp"
#include "dfre/mission_sound_runtime.hpp"
#include "dfre/resources.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
#include <set>
#include <stdexcept>
using namespace dfre;using U=std::uint32_t;
namespace {
void check(bool value,const char* text){if(!value)throw std::runtime_error(text);}
template<class F>void rejects(F body){bool stopped=false;try{body();}catch(const std::exception&){stopped=true;}check(stopped,"required cleanup error did not stop");}
struct Fixture {
 static constexpr U B=0xc54480,Z=0xc8ff50,T=B+8,L=0xc65dd0;
 std::vector<std::uint8_t> bytes=std::vector<std::uint8_t>(Z-B);std::set<U> live;std::vector<U> released;U clears=0;bool release_error=false,stop_palette=false,mutate_null=false,grow_lods=false;
 U get(U at,U width=4){check(at>=B&&std::uint64_t(at)+width<=Z,"fixture read extent");U value=0;for(U i=0;i<width;++i)value|=U(bytes.at(at-B+i))<<(i*8);return value;}
 void put(U at,U width,U value){check(at>=B&&std::uint64_t(at)+width<=Z,"fixture write extent");for(U i=0;i<width;++i)bytes.at(at-B+i)=std::uint8_t(value>>(i*8));}
 MissionModelCleanupBindings bindings(){MissionModelCleanupBindings b;b.read=[&](U at,U width){U value=get(at,width);if(mutate_null&&at==T+0x1c)put(at,4,0x1300090);return value;};b.write=[&](U at,U width,U value){put(at,width,value);if(stop_palette&&at==T+0x20)throw std::runtime_error("palette fixture stop");};b.call=[&](U site,const std::vector<U>& a)->U{if(site==0x4980d0){if(release_error)throw std::runtime_error("release fixture stop");check(live.erase(a.at(0))==1,"duplicate or unowned release");released.push_back(a.at(0));return 0;}if(site==0x49a620){++clears;for(U i=0;i<a.at(2);++i)put(a.at(0)+i,1,a.at(1)&255u);if(grow_lods&&a.at(0)==L){put(0xc65dcc,4,2);put(L+0xb8+0x18,4,0x1300200);}return 0;}throw std::runtime_error("unowned cleanup service");};return b;}
};
}
int main(int argc,char** argv){try{
 {Fixture f;f.put(Fixture::B,4,2);f.put(Fixture::T+0x1c,4,0x1300000);f.put(Fixture::T+36+0x16,1,128);f.put(Fixture::T+36+0x1c,4,0x1300000);f.put(0xc65dcc,4,1);f.put(Fixture::L+0x18,4,0x1300100);f.live={0x1300000,0x1300100};f.put(0xc80d10,4,0x12345678);f.put(0xc80d18,4,0x87654321);f.put(0xc54484,4,7);f.put(0xc65dc8,4,8);f.put(Fixture::L+5*0xb8,4,0x1234);mission_model_cleanup(f.bindings());check(f.live.empty()&&f.released==std::vector<U>{0x1300000,0x1300100},"release order or sharing differs");check(!f.get(Fixture::B)&&!f.get(0xc65dcc)&&!f.get(0xc80d14)&&!f.get(0xc80d18)&&f.get(0xc80d10)==0x12345678&&f.get(0xc54484)==7&&f.get(0xc65dc8)==8&&f.get(Fixture::L+5*0xb8)==0x1234,"exact clear extents differ");}
 {Fixture f;f.put(Fixture::B,4,1);f.mutate_null=true;f.stop_palette=true;rejects([&]{mission_model_cleanup(f.bindings());});check(f.get(Fixture::T+0x1c)==0x1300090&&f.get(Fixture::B)==1&&f.clears==0,"null branch added a pointer clear");}
 {Fixture f;f.put(Fixture::B,4,1);f.put(Fixture::T+0x1c,4,0x1300000);f.release_error=true;rejects([&]{mission_model_cleanup(f.bindings());});check(f.get(Fixture::T+0x1c)==0x1300000&&f.get(Fixture::B)==1&&f.clears==0,"release error prefix differs");}
 {Fixture f;f.put(0xc65dcc,4,601);f.put(0xc80d10,4,0x12345678);f.put(0xc80d28,4,0x1300600);f.live={0x1300600};mission_model_cleanup(f.bindings());check(f.live.empty()&&f.released==std::vector<U>{0x1300600}&&!f.get(0xc80d10)&&f.clears==603,"slot600 physical alias differs");}
 {Fixture f;f.put(0xc65dcc,4,1);f.grow_lods=true;f.live={0x1300200};mission_model_cleanup(f.bindings());check(f.live.empty()&&f.released==std::vector<U>{0x1300200}&&!f.get(0xc65dcc),"current LOD count read differs");rejects([&]{mission_model_cleanup({});});}
 if(argc==2){
  const std::filesystem::path folder(argv[1]);AssetStore assets(folder);MissionAssetCache cache(assets);cache.load_runtime("C06M01.BMS");std::ifstream exe(folder/"Df.exe",std::ios::binary);AssetBytes executable((std::istreambuf_iterator<char>(exe)),{});Fixed3 listener{};U yaw=0;MissionAudioFrameCells audio{0,listener,yaw};const TerrainGeometry* terrain=nullptr;MissionSoundRuntime owner(assets,executable,audio,terrain,{});const auto& memory=owner.asset_bindings();auto alloc=[&](U size){return memory.call(0x497f60,{size,4,0,0});};
  const U regargs=alloc(12),regframe=alloc(32),flag=alloc(4),name=alloc(32),coldargs=alloc(12),coldframe=alloc(4),texargs=alloc(4),lodargs=alloc(12),lodframe=alloc(40);memory.write(0xc80d10,4,0);memory.write(0xc65dcc,4,0);memory.write(0xc80d14,4,0);memory.write(0xc65dc8,4,0);
  MissionModelCleanupBindings cleanup;cleanup.read=memory.read;cleanup.write=memory.write;cleanup.call=memory.call;
  MissionModelTextureBindings texture;texture.read=memory.read;texture.write=memory.write;texture.call=memory.call;mission_model_texture_initialize(texture);
  MissionModelLodBindings lod;lod.read=memory.read;lod.write=memory.write;lod.call=memory.call;lod.material_ticks=[&](U material,U result){check(memory.read(0x4cdc78,4)==0&&memory.read(0x4cdc7c,4)==0x404f0000&&memory.read(0x4cdc80,4)==0&&memory.read(0x4cdc84,4)==0x40590000,"x87 constants changed");U raw=memory.read(material+0x1c,2);std::int32_t value=raw<32768?std::int32_t(raw):std::int32_t(raw)-65536;memory.write(result,4,U(value*31/50));};
  for(const std::string stem:{"gazelle","bexp"}){
   const std::string filename=stem+".3di";const auto& model=cache.model(filename);const auto actual=assets.read(filename).bytes;
   for(unsigned pass=0;pass<2;++pass){
    U consumed=0,enters=0,leaves=0;std::vector<U> allocations,records;for(U i=0;i<=stem.size();++i)memory.write(name+i,1,i<stem.size()?U(std::uint8_t(stem[i])):0);memory.write(regargs,4,name);memory.write(regargs+4,4,0);memory.write(regargs+8,4,flag);
    auto file_call=[&](U site,const std::vector<U>& a)->U{if(site==0x499280)consumed+=a.at(1);if(site==0x4992d0)consumed+=a.at(0);if(site==0x497f60){U result=memory.call(site,a);allocations.push_back(result);return result;}return memory.call(site,a);};texture.call=file_call;lod.call=file_call;
    MissionModelLoadBindings cold;cold.read=memory.read;cold.write=memory.write;cold.call=[&](U site,const std::vector<U>& a)->U{if(site==0x485ba0){memory.write(texargs,4,a.at(0));mission_model_texture_load(texture,texargs);return 0;}if(site==0x485e30){for(U i=0;i<3;++i)memory.write(lodargs+i*4,4,a.at(i));U result=mission_model_lod_load(lod,lodargs,lodframe);records.push_back(result);return result;}return file_call(site,a);};
    MissionModelRegistryBindings registry;registry.read=memory.read;registry.write=memory.write;registry.call=[&](U site,const std::vector<U>& a)->U{if(site==0x48fd00){++enters;return 0;}if(site==0x48fd70){++leaves;return 0;}if(site==0x486370){for(U i=0;i<3;++i)memory.write(coldargs+i*4,4,a.at(i));return mission_model_load(cold,coldargs,coldframe);}if(site==0x499e5e){for(U i=0;i<128;++i){U x=memory.read(a.at(0)+i,1),y=memory.read(a.at(1)+i,1);if(x>='A'&&x<='Z')x+=32;if(y>='A'&&y<='Z')y+=32;if(x!=y)return 1;if(!x)return 0;}throw std::runtime_error("native ASCII comparison extent");}return memory.call(site,a);};
    U graphic=mission_model_registry_load(registry,regargs,regframe);check(graphic==0xc80d20&&memory.read(flag,4)==1&&memory.read(0xc80d14,4)==1&&enters==1&&leaves==1&&consumed==actual.size(),"cold registry chain/flag/file cursor differs");check(records.size()==model.lods.size(),"loaded LOD count differs");for(U level=0;level<records.size();++level){const auto local=localize_model_lod(model.lods[level]);U vertices=memory.read(records[level]+0x84,4),hp=memory.read(records[level]+0xa4,4);for(U i=0;i<local.vertices.size();++i)for(U axis=0;axis<3;++axis)check(memory.read(vertices+i*8+axis*2,2)==std::uint16_t(local.vertices[i].position[axis]),"reloaded vertices differ from samecache");for(U i=0;i<local.hardpoints.size();++i)for(U axis=0;axis<3;++axis)check(memory.read(hp+i*12+axis*4,4)==U(local.hardpoints[i][axis]),"reloaded HP differs from samecache");}
    U texture_index=0;for(const auto& source:model.textures){if(!(source.flags&1u))continue;U descriptor=0xc54488+texture_index++*36,pixels=memory.read(descriptor+0x1c,4),palette=memory.read(descriptor+0x20,4);check(memory.read(descriptor+0x10,4)==source.pixels.size(),"reloaded texture count differs");for(U i=0;i<source.pixels.size();++i)check(memory.read(pixels+i,1)==source.pixels[i],"reloaded pixels differ from samecache");for(U i=0;i<256;++i)for(U channel=0;channel<3;++channel)check(memory.read(palette+i*3+channel,1)==source.palette_rgb_pad[i*4+channel],"reloaded palette differs from samecache");}check(texture_index==memory.read(0xc54480,4),"reloaded texture span differs");
    check(mission_model_registry_load(registry,regargs,regframe)==graphic&&!memory.read(flag,4)&&enters==1&&consumed==actual.size(),"cached flag or file read differs");memory.write(0xc80d10,4,3);mission_model_cleanup(cleanup);check(!memory.read(0xc54480,4)&&!memory.read(0xc65dcc,4)&&!memory.read(0xc80d14,4)&&!memory.read(graphic+0x50,4)&&memory.read(0xc80d10,4)==3,"actual cleanup counts/registry/level differs");for(U pointer:allocations)rejects([&]{memory.read(pointer,1);});memory.write(0xc80d10,4,0);std::cout<<filename<<" pass "<<pass+1<<" released "<<allocations.size()<<" bytes "<<consumed<<'\n';
   }
  }
 }
 std::cout<<"model cleanup checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
