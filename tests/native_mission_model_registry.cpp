#include "dfre/mission_model_registry.hpp"
#include "dfre/mission_model_memory.hpp"
#include "dfre/mission_item_resource.hpp"
#include "dfre/model_visibility.hpp"
#include "dfre/resources.hpp"
#include "dfre/item_runtime.hpp"
#include <array>
#include <iostream>
#include <memory>
#include <stdexcept>
using namespace dfre;using U=std::uint32_t;
namespace {
void check(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
struct Fixture {
 static constexpr U R=0xc80d20,C=0xc80d14,N=0x1300000,E=0x1301000,F=0x1302000,P=0x1303000;
 std::array<std::uint8_t,500*0x7c> records{};std::array<std::uint8_t,64> name{};std::array<std::uint8_t,12> args{};std::array<std::uint8_t,32> frame{};std::array<std::uint8_t,8> flags{};U count=0,loads=0,opens=0,closes=0,last_bank=0;bool fail_load=false,fail_close=false,mutate_enter=false,mutate_close=false;
 Fixture(){write(E,4,N);write(E+4,4,2);write(E+8,4,P);text(N,"bexp");}
 std::pair<std::uint8_t*,U> block(U at){if(at>=R&&at<R+records.size())return {records.data(),at-R};if(at>=N&&at<N+name.size())return {name.data(),at-N};if(at>=E&&at<E+args.size())return {args.data(),at-E};if(at>=F&&at<F+frame.size())return {frame.data(),at-F};if(at>=P&&at<P+flags.size())return {flags.data(),at-P};if(at>=C&&at<C+4)return {reinterpret_cast<std::uint8_t*>(&count),at-C};throw MissionModelRegistryError("Native registry memory is not owned");}
 U read(U at,U width=4){U n=0;for(U i=0;i<width;++i){const auto value=block(at+i);n|=U(value.first[value.second])<<(i*8);}return n;}
 void write(U at,U width,U n){for(U i=0;i<width;++i){const auto value=block(at+i);value.first[value.second]=std::uint8_t(n>>(i*8));}}
 std::string string(U at){std::string result;for(U i=0;i<1024;++i){const U n=read(at+i,1);if(!n)return result;result+=char(n);}throw MissionModelRegistryError("Native string exceeds current owner");}
 void text(U at,const std::string& value){for(U i=0;i<=value.size();++i)write(at+i,1,i<value.size()?U(std::uint8_t(value[i])):0);}
 MissionModelRegistryBindings bindings(){MissionModelRegistryBindings b;b.read=[&](U at,U width){return read(at,width);};b.write=[&](U at,U width,U n){write(at,width,n);};b.call=[&](U site,const std::vector<U>& a)->U{
  if(site==0x499e5e){auto x=string(a.at(0)),y=string(a.at(1));for(auto& c:x)if(c>='A'&&c<='Z')c=char(c+32);for(auto& c:y)if(c>='A'&&c<='Z')c=char(c+32);return x==y?0:1;}
  if(site==0x456560){auto value=string(a.at(0));const auto dot=value.find('.');if(dot==std::string::npos)value+=".3di";else{value.resize(dot+1);value+="3di";}text(a.at(0),value);return 0;}
  if(site==0x48fd00){++opens;check(string(a.at(0))=="bexp.3di","registry extension differs");if(mutate_enter)write(E+4,4,5);return 0;}
  if(site==0x486370){++loads;last_bank=a.at(2);check(a.at(0)==R+count*0x7c&&string(a.at(1))=="bexp.3di","cold record or source name differs");text(a.at(0)+4,fail_load?"partial":"bexp");write(a.at(0)+0x20,4,0xabcdef01);return fail_load?0:1;}
  if(site==0x48fd70){++closes;if(fail_close)throw MissionModelRegistryError("Close stopped");if(mutate_close)write(E+8,4,P+4);return 0;}
  throw MissionModelRegistryError("Native registry service is not owned");};return b;}
};
template<class F>void rejects(F body){bool failed=false;try{body();}catch(const std::exception&){failed=true;}check(failed,"Invalid registry call succeeded");}
}
int main(int argc,char** argv){try{
 {Fixture f;f.fail_load=true;check(mission_model_registry_load(f.bindings(),Fixture::E,Fixture::F)==0&&f.count==0&&f.read(Fixture::P)==0&&f.string(Fixture::R+4)=="partial"&&f.closes==1,"cold failure state differs");f.fail_load=false;f.mutate_enter=true;f.mutate_close=true;check(mission_model_registry_load(f.bindings(),Fixture::E,Fixture::F)==Fixture::R&&f.count==1&&f.read(Fixture::P)==0&&f.read(Fixture::P+4)==1&&f.last_bank==5&&f.loads==2,"retry or current argument reads differ");f.text(Fixture::N,"BEXP");check(mission_model_registry_load(f.bindings(),Fixture::E,Fixture::F)==Fixture::R&&f.count==1&&!f.read(Fixture::P+4)&&f.loads==2&&f.opens==2,"cached lookup or firstload flag differs");}
 {Fixture f;f.fail_close=true;rejects([&]{mission_model_registry_load(f.bindings(),Fixture::E,Fixture::F);});check(!f.count&&!f.read(Fixture::P)&&f.string(Fixture::R+4)=="bexp"&&f.loads==1,"close failure published count or flag");}
 {Fixture f;f.count=500;check(!mission_model_registry_load(f.bindings(),Fixture::E,Fixture::F)&&!f.loads&&!f.opens&&!f.read(Fixture::P),"full registry attempted a load");}
 {Fixture f;f.text(Fixture::N,std::string(40,'x'));rejects([&]{mission_model_registry_load(f.bindings(),Fixture::E,Fixture::F);});for(U i=0;i<32;++i)check(f.frame[i]=='x',"overflow did not preserve scratch prefix");check(!f.loads&&!f.opens&&!f.count,"copy overflow reached a later service");rejects([&]{mission_model_registry_load(f.bindings(),Fixture::E+1,Fixture::F);});rejects([&]{mission_model_registry_load({},Fixture::E,Fixture::F);});}
 if(argc==2){
  AssetStore assets(argv[1]);MissionAssetCache cache(assets);const auto resources=cache.load_runtime("C06M01.BMS");(void)resources;
  MissionModelMemory models([&](std::string_view name)->const Model&{return cache.model(name);},[&](std::string_view name){return assets.read(name).bytes;},{{0x71000000,"bexp.3di",MissionModelCallbackSeed::pre_producer()}});
  Fixture f;RuntimeItemDefinition item{};item.type=8;item.graphic[0]='b';item.graphic[1]='e';item.graphic[2]='x';item.graphic[3]='p';item.effect_count=0x12345678;constexpr U D=0x18000000,S=0x1304000;U flag=0;
  MissionItemResourceBindings b;b.read=[&](U at,U width){if(models.claims(at,width))return models.read(at,width);if(at==0xc23810)return D;if(at==0xc23814)return 1u;if(at==0x503138)return 1u;if(at==S)return flag;if(at>=D&&std::uint64_t(at-D)+width<=sizeof(item)){if(at==D+0x6c)return models.source(item.model);if(at==D+0x70)return models.source(item.husk_model);const auto* bytes=reinterpret_cast<const std::uint8_t*>(&item);U n=0;for(U i=0;i<width;++i)n|=U(bytes[at-D+i])<<(i*8);return n;}throw MissionModelRegistryError("Joined item read is not owned");};
  b.write=[&](U at,U width,U value){if(models.claims(at,width)){models.write(at,width,value);return;}check(width==4,"Joined item width differs");if(at==D+0x6c)item.model=models.handle(value);else if(at==D+0x9c)item.effect_count=value;else throw MissionModelRegistryError("Joined item write is not owned");};
  b.call=[&](U site,const std::vector<U>& a)->U{
   if(site==0x486530){check(a==std::vector<U>{D+0x2c,0,S},"Joined resource request differs");f.write(Fixture::E,4,Fixture::N);f.write(Fixture::E+4,4,a.at(1));f.write(Fixture::E+8,4,Fixture::P);const U record=mission_model_registry_load(f.bindings(),Fixture::E,Fixture::F);flag=f.read(Fixture::P);if(!record)return 0;check(record==Fixture::R&&f.count==1,"Registry-to-model record conversion differs");return models.source(0x71000000);}
   if(site==0x4818f0||site==0x481a00){const auto* tag=find_model_callback_tag(a.at(0));if(!tag)tag=&model_callback_tags.front();return site==0x4818f0?tag->draw_address:tag->matrix_address;}
   throw MissionModelRegistryError("Joined item service is not owned");};
  f.fail_load=true;mission_item_resource_load(b,0,S);check(!item.model&&item.effect_count==0x12345678&&!f.count&&!flag,"Failed registry join published item metadata");f.fail_load=false;mission_item_resource_load(b,0,S);check(item.model==0x71000000&&item.effect_count==96&&f.count==1&&flag==1&&f.loads==2,"Actual registry/item/model join differs");const U count=f.loads;mission_item_resource_load(b,0,S);check(f.loads==count&&f.count==1,"Cached item repeated registry load");
 }
 std::cout<<"model registry checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
