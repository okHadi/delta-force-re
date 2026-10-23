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
template<class F>void rejects(F body){bool stopped=false;try{body();}catch(const std::exception&){stopped=true;}check(stopped,"required LOD service did not stop");}
struct Fixture{
 static constexpr U L=0xc65dd0,P=0x1300000,E=0x1301000,F=0x1302000;
 std::map<U,std::uint8_t> bytes;unsigned reads=0;bool allocate=true,ticks=false;
 Fixture(){for(auto row:std::array<std::pair<U,U>,7>{{{0xc65dcc,4},{L,0xb8},{P,512},{E,12},{F,40},{0xc80d10,12},{0xc54488,72}}})for(U i=0;i<row.second;++i)bytes[row.first+i]=0;put(L+0x14,4,76);put(L+0xa8,4,1);put(P+0x11,1,0x40);put(P+0x1c,2,0x8000);put(E+8,4,0x12345678);}
 U get(U at,U width=4){U value=0;for(U i=0;i<width;++i)value|=U(bytes.at(at+i))<<(i*8);return value;}
 void put(U at,U width,U value){for(U i=0;i<width;++i)bytes.at(at+i)=std::uint8_t(value>>(i*8));}
 MissionModelLodBindings bindings(){MissionModelLodBindings b;b.read=[&](U at,U width){return get(at,width);};b.write=[&](U at,U width,U value){put(at,width,value);};b.call=[&](U site,const std::vector<U>& a)->U{if(site==0x499280){++reads;check(a.size()==2,"read argument count");return 0;}if(site==0x497f60)return allocate?P:0;throw std::runtime_error("unowned service");};b.material_ticks=[&](U material,U result){check(material==P&&result==F+0x24,"captured material or scratch differs");ticks=true;put(result,4,U(-20316));put(E+8,4,7);};return b;}
};
}
int main(int argc,char** argv){try{
 {Fixture f;check(mission_model_lod_load(f.bindings(),Fixture::E,Fixture::F)==Fixture::L&&f.reads==2&&f.ticks,"source LOD path differs");check(f.get(Fixture::P+0x1c,2)==(U(-20316)&65535u),"x87 result differs");check(f.get(Fixture::P+0x1a,2)==7&&f.get(0xc65dcc)==1,"current bank or publication differs");}
 {Fixture f;f.allocate=false;check(!mission_model_lod_load(f.bindings(),Fixture::E,Fixture::F)&&f.reads==1&&!f.get(0xc65dcc)&&!f.get(Fixture::L+0x18),"allocation failure prefix differs");}
 {Fixture f;f.put(0xc65dcc,4,601);check(!mission_model_lod_load(f.bindings(),Fixture::E,Fixture::F)&&!f.reads,"registry cap differs");}
 {Fixture f;auto b=f.bindings();b.material_ticks={};rejects([&]{mission_model_lod_load(b,Fixture::E,Fixture::F);});check(f.reads==2&&f.get(Fixture::L+0xac)==Fixture::P&&!f.get(0xc65dcc),"missing x87 failure prefix differs");rejects([&]{mission_model_lod_load(b,Fixture::E,Fixture::F+1);});rejects([&]{mission_model_lod_load({},Fixture::E,Fixture::F);});}
 if(argc==2){
  const std::filesystem::path folder(argv[1]);AssetStore assets(folder);MissionAssetCache cache(assets);cache.load_runtime("C06M01.BMS");std::ifstream exe(folder/"Df.exe",std::ios::binary);AssetBytes executable((std::istreambuf_iterator<char>(exe)),{});Fixed3 listener{};U yaw=0;MissionAudioFrameCells audio{0,listener,yaw};const TerrainGeometry* terrain=nullptr;MissionSoundRuntime owner(assets,executable,audio,terrain,{});const auto& memory=owner.asset_bindings();
  auto alloc=[&](U size){return memory.call(0x497f60,{size,4,0,0});};U graphic=alloc(0x7c),args=alloc(12),frame=alloc(4),lodargs=alloc(12),lodframe=alloc(40),name=alloc(32),texture_header=alloc(36);memory.write(0xc65dcc,4,0);memory.write(0xc54480,2,0);memory.write(0xc80d10,4,0);memory.write(0xc80d18,4,0);
  MissionModelLodBindings lod;lod.read=memory.read;lod.write=memory.write;lod.call=memory.call;
  // Explicit fixed-constant numeric contract. This host has no CPU status consumer.
  lod.material_ticks=[&](U material,U result){check(memory.read(0x4cdc78,4)==0&&memory.read(0x4cdc7c,4)==0x404f0000&&memory.read(0x4cdc80,4)==0&&memory.read(0x4cdc84,4)==0x40590000,"current x87 constants changed");U raw=memory.read(material+0x1c,2);std::int32_t value=raw<32768?std::int32_t(raw):std::int32_t(raw)-65536;memory.write(result,4,U(value*31/50));};
  for(const std::string filename:{"gazelle.3di","bexp.3di"}){
   const auto& model=cache.model(filename);const auto actual=assets.read(filename).bytes;U tex=0,level=0,consumed=0;std::vector<U> records;
   for(U i=0;i<=filename.size();++i)memory.write(name+i,1,i<filename.size()?U(std::uint8_t(filename[i])):0);memory.write(args,4,graphic);memory.write(args+4,4,name);memory.write(args+8,4,0);
   auto original_call=lod.call;lod.call=[&](U site,const std::vector<U>& a)->U{if(site==0x499280)consumed+=a.at(1);return memory.call(site,a);};
   MissionModelLoadBindings parent;parent.read=memory.read;parent.write=memory.write;parent.call=[&](U site,const std::vector<U>& a)->U{
    if(site==0x499280){consumed+=a.at(1);return memory.call(site,a);}
    if(site==0x485ba0){check(tex<model.textures.size(),"texture source overflow");memory.call(0x499280,{texture_header,36});consumed+=36;U bytes=memory.read(texture_header+16,4),payload=alloc(bytes+1024);memory.call(0x499280,{payload,bytes+1024});consumed+=bytes+1024;U index=memory.read(0xc54480,2),descriptor=0xc54488+index*36;for(U i=0;i<36;++i)memory.write(descriptor+i,1,memory.read(texture_header+i,1));memory.write(descriptor+0x1c,4,payload);memory.write(descriptor+0x20,4,payload+bytes);memory.write(0xc54480,2,index+1);check(bytes==model.textures[tex].pixels.size(),"supplied texture size differs");++tex;return 0;}
    if(site==0x485e30){for(U i=0;i<3;++i)memory.write(lodargs+i*4,4,a.at(i));U record=mission_model_lod_load(lod,lodargs,lodframe);check(record!=0&&level<model.lods.size(),"actual LOD load failed");const auto& source=model.lods[level++];auto local=localize_model_lod(source);U vertices=memory.read(record+0x84,4),hardpoints=memory.read(record+0xa4,4);check(memory.read(record+0x80,4)==source.vertices.size()&&memory.read(record+0x98,4)==source.meshes.size(),"current source counts differ");for(U i=0;i<local.vertices.size();++i)for(U axis=0;axis<3;++axis)check(memory.read(vertices+i*8+axis*2,2)==std::uint16_t(local.vertices[i].position[axis]),"localized supplied vertex differs");for(U i=0;i<source.hardpoints.size();++i)for(U axis=0;axis<3;++axis)check(memory.read(hardpoints+i*12+axis*4,4)==U(source.hardpoints[i][axis]),"supplied hardpoint differs");records.push_back(record);return record;}
    if(site==0x499040||site==0x499120||site==0x499020)return memory.call(site,a);
    throw std::runtime_error("unowned model child");};
   check(mission_model_load(parent,args,frame)==1&&level==model.lods.size()&&tex==model.textures.size()&&consumed==actual.size(),"supplied full model cursor differs");check(memory.read(graphic+0x50,4)==records.at(0),"parent source identity differs");std::cout<<filename<<" LODs "<<level<<" bytes "<<consumed<<'\n';lod.call=original_call;
  }
 }
 std::cout<<"model LOD checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
