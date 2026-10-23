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
void check(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
template<class F>void rejects(F body){bool failed=false;try{body();}catch(const std::exception&){failed=true;}check(failed,"Invalid model load succeeded");}
struct Fixture {
 static constexpr U G=0x1300000,E=0x1301000,F=0x1302000,L=0x1303000,M=0x1304000,N=0x1305000;
 std::map<U,std::uint8_t> bytes;std::array<std::uint8_t,0x7c> header{};U texture_count=1,opens=0,closes=0,textures=0,levels=0;bool exists=true,null_lod=false,stop_close=false;
 Fixture(){for(auto row:std::array<std::pair<U,U>,7>{{{G,0x100},{E,12},{F,4},{L,0xb8},{M,0x4c},{N,16},{0xc54480,2}}})for(U i=0;i<row.second;++i)bytes.emplace(row.first+i,0);write(E,4,G);write(E+4,4,N);write(E+8,4,3);header[0]='3';header[1]='D';header[2]='I';header[3]=5;header[0x14]=1;header[0x68]=0x77;header[0x74]=0x55;write(L+0xa8,4,1);write(L+0xac,4,M);write(L+0x24,4,0x12345678);write(M+0x1c,2,7);write(M+0x1e,1,3);write(M+0x1f,1,1);}
 U read(U at,U width=4){U value=0;for(U i=0;i<width;++i)value|=U(bytes.at(at+i))<<(i*8);return value;}
 void write(U at,U width,U value){for(U i=0;i<width;++i)bytes.at(at+i)=std::uint8_t(value>>(i*8));}
 MissionModelLoadBindings bindings(){MissionModelLoadBindings b;b.read=[&](U at,U width){return read(at,width);};b.write=[&](U at,U width,U value){write(at,width,value);};b.call=[&](U site,const std::vector<U>& a)->U{
  if(site==0x499040)return U(exists);
  if(site==0x499120){++opens;return 0;}
  if(site==0x499280){if(a.at(1)==0x7c){for(U i=0;i<0x7c;++i)write(a.at(0)+i,1,header[i]);}else{check(a.at(0)==F&&a.at(1)==4,"texture frame read differs");write(F,4,texture_count);}return 0;}
  if(site==0x485ba0){check(a.at(0)==3,"texture bank was not captured");++textures;write(0xc54480,2,read(0xc54480,2)+1);write(E+8,4,5);return 0;}
  if(site==0x485e30){check(a==std::vector<U>{0,1,3},"LOD texture endpoints or captured bank differ");++levels;return null_lod?0:L;}
  if(site==0x499020){++closes;if(stop_close)throw MissionModelLoadError("Close stopped");return 0;}
  throw MissionModelLoadError("Native model service is not owned");};return b;}
};
}
int main(int argc,char** argv){try{
 {Fixture f;check(mission_model_load(f.bindings(),Fixture::E,Fixture::F)==1&&f.opens==1&&f.closes==1&&f.textures==1&&f.levels==1,"cold parent call order differs");check(f.read(Fixture::G+0x50)==Fixture::L&&f.read(Fixture::G+0x54)==0&&f.read(Fixture::G+0x70,2)==0&&f.read(Fixture::G+0x72,2)==1&&f.read(Fixture::G+0x74)==21&&f.read(Fixture::G+0x68)==0x12345678,"cold metadata differs");}
 {Fixture f;f.null_lod=true;check(mission_model_load(f.bindings(),Fixture::E,Fixture::F)==1&&!f.read(Fixture::G+0x50)&&!f.read(Fixture::G+0x74)&&f.read(Fixture::G+0x68)==0x77,"null LOD source-success behavior differs");}
 {Fixture f;f.header[3]=6;check(mission_model_load(f.bindings(),Fixture::E,Fixture::F)==1,"source version6 gate differs");Fixture old;old.header[3]=4;check(!mission_model_load(old.bindings(),Fixture::E,Fixture::F)&&old.closes==1&&!old.textures&&!old.levels,"invalid header close differs");}
 {Fixture f;f.exists=false;check(!mission_model_load(f.bindings(),Fixture::E,Fixture::F)&&!f.opens&&!f.closes&&!f.read(Fixture::G),"missing file changed load record");}
 {Fixture f;f.stop_close=true;rejects([&]{mission_model_load(f.bindings(),Fixture::E,Fixture::F);});check(f.read(Fixture::G+0x74)==0x55&&f.read(Fixture::G+0x50)==Fixture::L,"close failure reached metadata pass");rejects([&]{mission_model_load({},Fixture::E,Fixture::F);});rejects([&]{mission_model_load(f.bindings(),Fixture::E,Fixture::F+1);});}
 if(argc==2){
  const std::filesystem::path folder(argv[1]);AssetStore assets(folder);MissionAssetCache cache(assets);const auto resources=cache.load_runtime("C06M01.BMS");(void)resources;std::ifstream exe(folder/"Df.exe",std::ios::binary);AssetBytes executable((std::istreambuf_iterator<char>(exe)),{});Fixed3 listener{};U yaw=0;MissionAudioFrameCells audio{0,listener,yaw};const TerrainGeometry* terrain=nullptr;MissionSoundRuntime file_owner(assets,executable,audio,terrain,{});const auto& memory=file_owner.asset_bindings();memory.write(0xc54480,2,0);
  const auto allocate=[&](U size){return memory.call(0x497f60,{size,4,0,0});};const U graphic=allocate(0x7c),arguments=allocate(12),frame=allocate(4),name=allocate(32),texture_header=allocate(36);
  for(const std::string filename:{"gazelle.3di","bexp.3di"}){
   const auto& model=cache.model(filename);const auto actual=assets.read(filename).bytes;U texture_index=0,level_index=0,consumed=0;std::vector<U> source_lods;const U first_texture=memory.read(0xc54480,2);
   for(U i=0;i<=filename.size();++i)memory.write(name+i,1,i<filename.size()?U(std::uint8_t(filename[i])):0);memory.write(arguments,4,graphic);memory.write(arguments+4,4,name);memory.write(arguments+8,4,0);
   MissionModelLoadBindings b;b.read=memory.read;b.write=memory.write;b.call=[&](U site,const std::vector<U>& a)->U{
    if(site==0x499280){consumed+=a.at(1);return memory.call(site,a);}
    if(site==0x485ba0){check(texture_index<model.textures.size(),"texture fixture exceeds current cache");memory.call(0x499280,{texture_header,36});consumed+=36;const auto bytes=memory.read(texture_header+16,4);const auto& texture=model.textures[texture_index++];check(bytes==texture.pixels.size()&&memory.read(texture_header+24,2)==texture.width&&memory.read(texture_header+26,2)==texture.height,"actual texture header differs from cache");const U payload=allocate(bytes+1024);memory.call(0x499280,{payload,bytes+1024});consumed+=bytes+1024;for(U i=0;i<bytes;++i)check(memory.read(payload+i,1)==texture.pixels[i],"actual texture pixels differ from cache");memory.write(0xc54480,2,memory.read(0xc54480,2)+1);return 0;}
    if(site==0x485e30){check(level_index<model.lods.size()&&a==std::vector<U>{first_texture,U(first_texture+texture_index),0},"actual LOD call span differs");const auto& lod=model.lods[level_index++];const U record=allocate(0xb8);memory.call(0x499280,{record,0xb8});consumed+=0xb8;for(U i=0;i<lod.raw_header.size();++i)check(memory.read(record+i,1)==lod.raw_header[i],"actual LOD header differs from cache");const U size=memory.read(record+0x14,4),payload=allocate(size);memory.call(0x499280,{payload,size});consumed+=size;memory.write(record+0x18,4,payload);
     U at=payload;memory.write(record+0x84,4,at);at+=memory.read(record+0x80,4)*8;memory.write(record+0x8c,4,at);at+=memory.read(record+0x88,4)*8;memory.write(record+0x94,4,at);at+=memory.read(record+0x90,4)*72;memory.write(record+0x9c,4,at);at+=memory.read(record+0x98,4)*112;memory.write(record+0xa4,4,at);check(memory.read(record+0xa0,4)==lod.hardpoints.size(),"actual hardpoint count differs");for(U hp=0;hp<lod.hardpoints.size();++hp)for(U axis=0;axis<3;++axis)check(memory.read(at+hp*12+axis*4,4)==U(lod.hardpoints[hp][axis]),"actual hardpoint data differs from cache");at+=memory.read(record+0xa0,4)*12;memory.write(record+0xb4,4,at);at+=memory.read(record+0xb0,4)*72;memory.write(record+0xac,4,at);check(at+memory.read(record+0xa8,4)*76==payload+size,"actual material payload extent differs");source_lods.push_back(record);return record;}
    if(site==0x499040||site==0x499120||site==0x499020)return memory.call(site,a);
    throw MissionModelLoadError("Stock model child is not owned");};
   check(mission_model_load(b,arguments,frame)==1&&consumed==actual.size()&&texture_index==model.textures.size()&&level_index==model.lods.size(),"actual file cursor or parent result differs");check(memory.read(graphic+0x50,4)==source_lods.at(0)&&memory.read(graphic+0x68,4)==memory.read(source_lods[0]+0x24,4)&&memory.read(graphic+0x70,2)==first_texture&&memory.read(graphic+0x72,2)==first_texture+texture_index,"actual current header metadata differs");
   std::cout<<filename<<" textures "<<texture_index<<" LODs "<<level_index<<" bytes "<<consumed<<'\n';
  }
 }
 std::cout<<"model load checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
