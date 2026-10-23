#include "dfre/mission_model_texture.hpp"
#include "dfre/mission_model_lod.hpp"
#include "dfre/mission_model_load.hpp"
#include "dfre/mission_sound_runtime.hpp"
#include "dfre/resources.hpp"
#include <array>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <stdexcept>
using namespace dfre;using U=std::uint32_t;
namespace {
void check(bool value,const char* text){if(!value)throw std::runtime_error(text);}
template<class F>void rejects(F body){bool stopped=false;try{body();}catch(const std::exception&){stopped=true;}check(stopped,"required texture service did not stop");}
struct Fixture {
 static constexpr U T=0xc54488,P=0x1300000,E=0x1302000;
 std::map<U,std::uint8_t> bytes;unsigned reads=0,releases=0,seeks=0;bool share=true,zero=false,release_error=false;
 Fixture(){for(auto row:std::array<std::pair<U,U>,6>{{{0xc54480,8},{T,72000},{P,2048},{E,4},{0xc80d10,4},{0xc65dc8,4}}})for(U i=0;i<row.second;++i)bytes[row.first+i]=0;put(0xc54480,4,1);put(0xc54484,4,1);put(T+0x10,4,16);put(T+0x17,1,2);put(T+0x1c,4,P+1500);put(T+0x20,4,P+1516);put(E,4,2);}
 U get(U at,U width=4){U value=0;for(U i=0;i<width;++i)value|=U(bytes.at(at+i))<<(i*8);return value;}
 void put(U at,U width,U value){for(U i=0;i<width;++i)bytes.at(at+i)=std::uint8_t(value>>(i*8));}
 MissionModelTextureBindings bindings(){MissionModelTextureBindings b;b.read=[&](U at,U width){return get(at,width);};b.write=[&](U at,U width,U value){put(at,width,value);};b.call=[&](U site,const std::vector<U>& a)->U{
  if(site==0x499280){++reads;if(reads==1){for(U i=0;i<36;++i)put(a.at(0)+i,1,0);put(a.at(0)+0x10,4,16);put(a.at(0)+0x16,1,1);}else for(U i=0;i<a.at(1);++i)put(a.at(0)+i,1,i&255u);return 0;}
  if(site==0x497f60)return zero?0:P;
  if(site==0x4992d0){++seeks;check(a==std::vector<U>{1040,1},"source skip differs");return 0;}
  if(site==0x499e5e)return share?0:1;
  if(site==0x4980d0){++releases;if(release_error)throw std::runtime_error("release fixture stop");put(T+0x1c,4,P+1600);return 0;}
  if(site==0x49a620){check(a==std::vector<U>{T,0,72000},"init clear span differs");for(U i=0;i<a.at(2);++i)put(a.at(0)+i,1,0);return 0;}
  throw std::runtime_error("unowned texture service");};return b;
 }
};
}
int main(int argc,char** argv){try{
 {Fixture f;mission_model_texture_load(f.bindings(),Fixture::E);check(f.reads==2&&f.releases==1&&f.get(0xc54480)==2&&f.get(Fixture::T+36+0x1c)==Fixture::P+1600&&(f.get(Fixture::T+36+0x16,1)&128),"share/current-release state differs");}
 {Fixture f;f.put(Fixture::E,4,258);mission_model_texture_load(f.bindings(),Fixture::E);check(!f.releases&&f.get(Fixture::T+36+0x17,1)==2,"full bank comparison differs");}
 {Fixture f;f.put(0xc65dc8,4,1);mission_model_texture_load(f.bindings(),Fixture::E);check(f.reads==1&&f.seeks==1&&f.get(0xc54480)==1,"skip publication differs");}
 {Fixture f;f.zero=true;rejects([&]{mission_model_texture_load(f.bindings(),Fixture::E);});check(f.reads==2&&!f.get(Fixture::T+36+0x1c)&&f.get(Fixture::T+36+0x20)==16&&f.get(0xc54480)==1,"null allocation prefix differs");}
 {Fixture f;f.release_error=true;rejects([&]{mission_model_texture_load(f.bindings(),Fixture::E);});check(f.get(Fixture::T+36+0x1c)==Fixture::P&&f.get(0xc54480)==1,"release failure prefix differs");}
 {Fixture f;f.put(0xc54480,4,2000);mission_model_texture_load(f.bindings(),0xffffffffu);check(!f.reads,"capacity gate or unread argument differs");mission_model_texture_initialize(f.bindings());check(!f.get(0xc54480)&&f.get(0xc54484)==1&&!f.get(Fixture::T+71996),"initializer table/gate differs");rejects([&]{mission_model_texture_load({},Fixture::E);});}
 if(argc==2){
  const std::filesystem::path folder(argv[1]);AssetStore assets(folder);MissionAssetCache cache(assets);cache.load_runtime("C06M01.BMS");std::ifstream exe(folder/"Df.exe",std::ios::binary);AssetBytes executable((std::istreambuf_iterator<char>(exe)),{});Fixed3 listener{};U yaw=0;MissionAudioFrameCells audio{0,listener,yaw};const TerrainGeometry* terrain=nullptr;MissionSoundRuntime owner(assets,executable,audio,terrain,{});const auto& memory=owner.asset_bindings();
  auto alloc=[&](U size){return memory.call(0x497f60,{size,4,0,0});};U graphic=alloc(0x7c),args=alloc(12),frame=alloc(4),lodargs=alloc(12),lodframe=alloc(40),name=alloc(32),texture_args=alloc(4);memory.write(0xc65dcc,4,0);memory.write(0xc54480,2,0);memory.write(0xc80d10,4,0);memory.write(0xc80d18,4,0);
  U consumed_texture_bytes=0;
  MissionModelTextureBindings texture;texture.read=memory.read;texture.write=memory.write;texture.call=[&](U site,const std::vector<U>& a)->U{if(site==0x499280)consumed_texture_bytes+=a.at(1);if(site==0x4992d0)consumed_texture_bytes+=a.at(0);if(site==0x499e5e)throw std::runtime_error("stock sharing string service required");return memory.call(site,a);};
  mission_model_texture_initialize(texture);check(memory.read(0xc54480,4)==0,"actual texture initializer count differs");
  MissionModelLodBindings lod;lod.read=memory.read;lod.write=memory.write;lod.call=memory.call;
  // Explicit fixed-constant numeric contract. This host has no CPU status consumer.
  lod.material_ticks=[&](U material,U result){check(memory.read(0x4cdc78,4)==0&&memory.read(0x4cdc7c,4)==0x404f0000&&memory.read(0x4cdc80,4)==0&&memory.read(0x4cdc84,4)==0x40590000,"current x87 constants changed");U raw=memory.read(material+0x1c,2);std::int32_t value=raw<32768?std::int32_t(raw):std::int32_t(raw)-65536;memory.write(result,4,U(value*31/50));};
  for(const std::string filename:{"gazelle.3di","bexp.3di"}){
   const auto& model=cache.model(filename);const auto actual=assets.read(filename).bytes;U tex=0,level=0,consumed=0;consumed_texture_bytes=0;const U texture_start=memory.read(0xc54480,4);std::vector<U> records;
   for(U i=0;i<=filename.size();++i)memory.write(name+i,1,i<filename.size()?U(std::uint8_t(filename[i])):0);memory.write(args,4,graphic);memory.write(args+4,4,name);memory.write(args+8,4,0);
   auto original_call=lod.call;lod.call=[&](U site,const std::vector<U>& a)->U{if(site==0x499280)consumed+=a.at(1);return memory.call(site,a);};
   MissionModelLoadBindings parent;parent.read=memory.read;parent.write=memory.write;parent.call=[&](U site,const std::vector<U>& a)->U{
    if(site==0x499280){consumed+=a.at(1);return memory.call(site,a);}
    if(site==0x485ba0){memory.write(texture_args,4,a.at(0));const U before=memory.read(0xc54480,4);mission_model_texture_load(texture,texture_args);const U after=memory.read(0xc54480,4);const auto& source=model.textures.at(tex++);if(!(source.flags&1u)){check(after==before,"stock skipped texture published count");return 0;}check(after==before+1,"stock loaded texture count differs");const U row=0xc54488+before*36,pixels=memory.read(row+0x1c,4),palette=memory.read(row+0x20,4);check(memory.read(row+0x10,4)==source.pixels.size()&&memory.read(row+0x14,2)==source.sequence,"stock descriptor differs");for(U i=0;i<source.pixels.size();++i)check(memory.read(pixels+i,1)==source.pixels[i],"stock pixels differ");for(U i=0;i<256;++i)for(U channel=0;channel<3;++channel)check(memory.read(palette+i*3+channel,1)==source.palette_rgb_pad[i*4+channel],"packed palette differs");return 0;}
    if(site==0x485e30){for(U i=0;i<3;++i)memory.write(lodargs+i*4,4,a.at(i));U record=mission_model_lod_load(lod,lodargs,lodframe);check(record!=0&&level<model.lods.size(),"actual LOD load failed");const auto& source=model.lods[level++];auto local=localize_model_lod(source);U vertices=memory.read(record+0x84,4),hardpoints=memory.read(record+0xa4,4);check(memory.read(record+0x80,4)==source.vertices.size()&&memory.read(record+0x98,4)==source.meshes.size(),"current source counts differ");for(U i=0;i<local.vertices.size();++i)for(U axis=0;axis<3;++axis)check(memory.read(vertices+i*8+axis*2,2)==std::uint16_t(local.vertices[i].position[axis]),"localized supplied vertex differs");for(U i=0;i<source.hardpoints.size();++i)for(U axis=0;axis<3;++axis)check(memory.read(hardpoints+i*12+axis*4,4)==U(source.hardpoints[i][axis]),"supplied hardpoint differs");records.push_back(record);return record;}
    if(site==0x499040||site==0x499120||site==0x499020)return memory.call(site,a);
    throw std::runtime_error("unowned model child");};
   check(mission_model_load(parent,args,frame)==1&&level==model.lods.size()&&tex==model.textures.size()&&consumed+consumed_texture_bytes==actual.size(),"supplied full model cursor differs");check(memory.read(graphic+0x50,4)==records.at(0),"parent source identity differs");std::cout<<filename<<" LODs "<<level<<" loadedtextures "<<memory.read(0xc54480,4)-texture_start<<" skippedtextures "<<tex-(memory.read(0xc54480,4)-texture_start)<<" bytes "<<consumed+consumed_texture_bytes<<'\n';lod.call=original_call;
  }
 }
 std::cout<<"model texture checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
