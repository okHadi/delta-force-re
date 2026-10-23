#include "dfre/mission_item_resource.hpp"
#include "dfre/mission_effect_emit.hpp"
#include "dfre/model_visibility.hpp"
#include "dfre/resources.hpp"
#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <unordered_map>
using namespace dfre;using U=std::uint32_t;
namespace {
void check(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
constexpr U D=0x1300000,M=0x1302000,L=0x1304000,F=0x1305000;
struct Fixture {
 std::unordered_map<U,std::uint8_t> bytes;
 std::array<const Model*,4> models{};
 U flag=0,loads=0,lookups=0,diagnostics=0;bool missing=false,first=true;
 MissionItemResourceBindings b;
 Fixture(){
  for(auto row:std::array<std::pair<U,U>,6>{{{D,0x1000},{M,0x400},{F,4},{0xc23810,8},{0x503138,4},{0x503228,4}}})for(U i=0;i<row.second;++i)bytes.emplace(row.first+i,0);
  put(0xc23810,D);put(0xc23814,4);put(0x503138,1);
  b.read=[&](U at,U width){
   if(at>=M&&at<M+0x400){const U slot=(at-M)/0x100,offset=(at-M)%0x100;const Model* model=models[slot];if(model){check(width==4,"model access width differs");if(offset>=0x28&&offset<=0x34)return model->render_types[(offset-0x28)/4];if(offset==0x50)return model->lods.empty()?0u:L+slot*0x100;}}
   if(at>=L&&at<L+0x400){const U slot=(at-L)/0x100,offset=(at-L)%0x100;check(models[slot]&&!models[slot]->lods.empty(),"LOD owner is absent");const auto& header=models[slot]->lods.front().raw_header;U value=0;for(U j=0;j<width;++j)value|=U(header.at(offset+j))<<(j*8);return value;}
   return get(at,width);
  };
  b.write=[&](U at,U width,U value){check(width==4,"resource store width differs");put(at,value);};
  b.call=[&](U site,const std::vector<U>& args)->U{
   if(site==0x4564b0){++diagnostics;return 0;}
   if(site==0x486530){++loads;check(args.at(2)==F,"loader flag owner differs");const U slot=(args.at(0)-D)/0x13c;check(slot<4,"loader item slot differs");check(args.at(1)==0,"effect model bank differs");put(F,U(first&&!missing));return missing?0:M+slot*0x100;}
   if(site==0x4818f0||site==0x481a00){const auto* tag=find_model_callback_tag(args.at(0));if(!tag)tag=&model_callback_tags.front();return site==0x4818f0?tag->draw_address:tag->matrix_address;}
   throw MissionItemResourceError("Native resource service is not owned");
  };
 }
 U get(U at,U width=4)const{U value=0;for(U i=0;i<width;++i)value|=U(bytes.at(at+i))<<(i*8);return value;}
 void put(U at,U value){for(U i=0;i<4;++i)bytes.at(at+i)=std::uint8_t(value>>(i*8));}
 void item(U index,const char* name){const U at=D+index*0x13c;put(at+0x28,8);for(U i=0;name[i];++i)bytes.at(at+0x2c+i)=std::uint8_t(name[i]);put(at+0x9c,0x12345678);}
};
}
int main(int argc,char** argv){try{
 Fixture f;f.item(0,"sheet");
 // A current cached graphic returns before reading flag, bank, type or model.
 f.put(D+0x6c,M);MissionItemResourceBindings cached;cached.read=[&](U at,U width){check(width==4,"cached read width differs");if(at==0xc23814)return 4u;if(at==0xc23810)return D;if(at==D+0x6c)return M;throw std::runtime_error("cached load read a later field");};mission_item_resource_load(cached,0,F);
 f.put(D+0x6c,0);f.missing=true;mission_item_resource_load(f.b,0,F);check(!f.get(D+0x6c)&&f.get(D+0x9c)==0x12345678,"missing graphic changed metadata");
 f.missing=false;f.first=false;mission_item_resource_load(f.b,0,F);check(f.get(D+0x6c)==M&&f.get(D+0x9c)==0x12345678,"null LOD changed metadata");
 f.put(D+0x6c,0);f.put(0xc23814,0);mission_item_resource_load(f.b,0,F);check(f.diagnostics==1&&f.get(D+0x6c)==M,"diagnostic did not continue");
 bool failed=false;try{mission_item_resource_load({},0,F);}catch(const MissionItemResourceError&){failed=true;}check(failed,"missing reader succeeded");
 failed=false;try{mission_item_resource_load(f.b,0,F+1);}catch(const MissionItemResourceError&){failed=true;}check(failed,"unaligned flag accepted");
 {
  Fixture order;order.item(0,"sheet");const auto store=order.b.write;
  order.b.write=[&](U at,U width,U value){store(at,width,value);if(at==M+0x3c)order.put(M+0x2c,0x68656c6f);};
  mission_item_resource_load(order.b,0,F);
  check(order.get(M+0x40)==model_callback_tags.front().draw_address,"graphic did not capture tag before callback store");
  order.put(D+0x6c,0);order.bytes.at(D+0x2c)=0;order.bytes.at(D+0x3c)='h';order.put(M+0x2c,0);
  mission_item_resource_load(order.b,0,F);
  check(order.get(M+0x40)==find_model_callback_tag(0x68656c6f)->draw_address,"husk did not read current tag after callback store");
 }
 {
  Fixture prefix;prefix.item(0,"sheet");const auto store=prefix.b.write;
  prefix.b.write=[&](U at,U width,U value){store(at,width,value);if(at==M+0x3c)throw MissionItemResourceError("Fixture store stopped");};
  bool stopped=false;try{mission_item_resource_load(prefix.b,0,F);}catch(const MissionItemResourceError&){stopped=true;}
  check(stopped&&prefix.get(D+0x6c)==M&&prefix.get(M+0x3c)==model_callback_tags.front().draw_address&&!prefix.get(M+0x40),"model failure prefix differs");
 }
 if(argc==2){
  AssetStore assets(argv[1]);MissionAssetCache cache(assets);const auto resources=cache.load_runtime("C06M01.BMS");(void)resources;Fixture stock;const std::array<const char*,4> names{{"bexp","splash2","splash1","smoke_a"}};const std::array<U,4> counts{{96,80,40,480}};
  for(U i=0;i<4;++i){stock.models[i]=&cache.model(std::string(names[i])+".3di");stock.item(i,names[i]);}
  const auto model_count=cache.model_count();
  for(U i=0;i<4;++i){mission_item_resource_load(stock.b,i,F);check(stock.get(D+i*0x13c+0x9c)==counts[i],"stock resource effect count differs");const auto* tag=find_model_callback_tag(stock.models[i]->render_types[0]);if(!tag)tag=&model_callback_tags.front();check(stock.get(M+i*0x100+0x3c)==tag->draw_address&&stock.get(M+i*0x100+0x4c)==tag->matrix_address,"model callback fields differ");std::cout<<names[i]<<' '<<counts[i]<<'\n';}
  check(cache.model_count()==model_count&&stock.loads==4,"resource proof changed cache ownership");
  // Real45DD40 calls real45D6F0; the lookup provider selects these current loaded rows.
  MissionEffectEmitBindings producer;producer.write=[&](U at,U width,U value){check(width==4,"producer store width differs");stock.bytes[at]=std::uint8_t(value);for(U j=1;j<4;++j)stock.bytes[at+j]=std::uint8_t(value>>(8*j));};
  producer.call=[&](U site,const std::vector<U>& args)->U{if(site==0x4828c0)return stock.lookups++%4;if(site==0x45d6f0){mission_item_resource_load(stock.b,args.at(0),F);return 0;}throw MissionItemResourceError("Producer service is not owned");};mission_effect_definitions_initialize(producer);check(stock.lookups==31&&stock.loads==4,"cached producer repeated model loads");
 }
 std::cout<<"item resource checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
