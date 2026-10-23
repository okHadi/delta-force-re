#include "dfre/mission_script_start.hpp"
#include <iostream>
#include <map>
#include <limits>
#include <stdexcept>
#define CHECK(x) do{if(!(x))throw std::runtime_error("script start: " #x);}while(false)
int main(){try{
 using namespace dfre;using U=std::uint32_t;
 MissionScriptQueueCells queue{};MissionScriptStartBindings bindings;MissionScriptStart start(queue,bindings);
 for(U i=0;i<16;++i)CHECK(start.start(0x1000+i*0x100)==1);CHECK(start.start(0x9000)==0&&queue.entries_8e3d74.size()==16);
 U host=0;MissionScriptRuntimeCells runtime_cells{queue,host};MissionScriptRuntimeBindings runtime_bindings;runtime_bindings.string=[](U at){return std::to_string(at);};MissionScriptRuntime runtime(runtime_cells,runtime_bindings);
 for(U iteration=0;iteration<64;++iteration){const auto slot=iteration%16;const auto removed=queue.entries_8e3d74[slot].resource;queue.entries_8e3d74[slot].armed=0x12345678;CHECK(runtime.remove(removed+4)==1&&queue.entries_8e3d74.size()==15);CHECK(start.start(0x9000+iteration*0x100)==1&&queue.entries_8e3d74.back().armed==0&&queue.entries_8e3d74.back().index==0&&queue.entries_8e3d74.back().delay==0);}
 bool failed=false;try{start.start(0);}catch(const MissionTickError&){failed=true;}CHECK(failed);queue.entries_8e3d74.clear();
 std::map<U,std::array<U,2>> registry{{1,{0x1000,2}},{2,{0x2000,3}},{3,{0x3000,0}}};bindings.registry_head=[](){return 1u;};bindings.node_word=[&](U node,U offset){CHECK(offset==0||offset==4);return registry.at(node)[offset/4];};
 U compares=0;bindings.compare=[&](U left,const MissionScriptStartName& name){CHECK(std::get<U>(name)==0x4000);++compares;if(left==0x1004){registry[1][1]=3;return 1;}CHECK(left==0x3004);registry[3][0]=0x5000;return 0;};CHECK(start.start_name(0x4000)==1&&compares==2&&queue.entries_8e3d74[0].resource==0x3000);
 std::vector<std::string> names;bindings.compare=[&](U,const MissionScriptStartName& name){names.emplace_back(std::get<std::string_view>(name));return 0;};for(const auto id:{0,1,-1,1000,std::numeric_limits<std::int32_t>::min(),std::numeric_limits<std::int32_t>::max()})start.event(id);CHECK(names==std::vector<std::string>({"dlg001","dlg-01","dlg1000","dlg-2147483648","dlg2147483647"}));
 queue.entries_8e3d74.clear();CHECK(start.start(0x1000)==1);queue.enabled_8e3d68=1;
 unsigned diagnostics=0,steps=0,cleanups=0;runtime_bindings.load_audio=[](U at){CHECK(at==0x1038);return 0u;};runtime_bindings.missing_audio=[&](U at){CHECK(at==0x1038);++diagnostics;};runtime_bindings.byte=[](U at){CHECK(at==0x1068);return std::uint8_t(0);};runtime_bindings.word=[](U at){CHECK(at==0x106c||at==0x1070||at==0x1074);return 0u;};runtime_bindings.step=[&](MissionScriptStep& value){CHECK(value.sample==0);++steps;return false;};
 U requested=0,stat=0;MissionProfilerCells profiler{};std::array<U,3> timers{};Fixed3 position{};MissionCameraCells camera{position,{},0,0};MissionFrameServiceBindings frame;
 frame.script_count=[](U resource){CHECK(resource==0x1000);return 1u;};frame.script_call=[&](U site,U resource,U index){if(site==0x455b40)++cleanups;runtime.call(site,resource,index);};MissionFrameServices services({requested,profiler,timers,stat,camera,queue},frame);services.scripts(0);CHECK(queue.entries_8e3d74.empty()&&steps==1&&diagnostics==1&&cleanups==1);
 bindings.registry_head={};start.event(0);failed=false;try{start.event(1);}catch(const MissionTickError&){failed=true;}CHECK(failed&&queue.entries_8e3d74.empty());
 bindings.registry_head=[](){return 0u;};bindings.node_word={};bindings.compare={};CHECK(start.start_name(0x4000)==0);
 queue.entries_8e3d74.push_back({0,0,0,0});failed=false;try{start.start(0x1000);}catch(const MissionTickError&){failed=true;}CHECK(failed&&queue.entries_8e3d74.size()==1);queue.entries_8e3d74.clear();
 bindings.registry_head=[](){return 1u;};bindings.node_word=[](U node,U offset){CHECK(node==1&&offset==0);return 0x1000u;};bindings.compare=[&](U,const MissionScriptStartName&){for(U i=0;i<16;++i)queue.entries_8e3d74.push_back({0x3000+i*0x100,0,0,0});return 0;};CHECK(start.start_name(0x4000)==0&&queue.entries_8e3d74.size()==16);
 std::cout<<"script start checks passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
