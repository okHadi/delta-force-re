#include "dfre/mission_script_runtime.hpp"
#include <iostream>
#include <map>
#include <stdexcept>
using namespace dfre;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void run(){
    MissionScriptQueueCells queue{};std::uint32_t host=1;MissionScriptRuntimeCells cells{queue,host};
    MissionScriptRuntimeBindings bindings;std::map<std::uint32_t,std::uint8_t> bytes;
    const auto text=[&](std::uint32_t at,const std::string& value){for(auto byte:value)bytes[at++]=std::uint8_t(byte);bytes[at]=0;};
    text(0x1004,"radio");text(0x2004,"radio");text(0x3004,"other");
    bindings.byte=[&](auto at){return bytes[at];};
    bindings.string=[&](auto at){std::string value;while(bytes[at])value+=char(bytes[at++]);return value;};
    MissionScriptRuntime runtime(cells,bindings);
    queue.entries_8e3d74={{0x1000,1,2,3},{0x3000,4,5,6},{0x2000,7,8,9}};
    check(runtime.remove(0x2004)==1&&queue.entries_8e3d74.size()==2&&queue.entries_8e3d74[0].resource==0x3000,"Script removal order differs");
    check(runtime.remove(0xdead)==0,"Unknown script was removed");
    unsigned messages=0;bindings.message=[&](auto id,const auto& payload){
        check(id==0x3c&&payload[0]=='r'&&payload[4]=='o'&&payload[5]==0&&payload[8]==0x34&&payload[9]==0x12,"Script packet differs");++messages;
    };
    runtime.announce(0x1004,0x1234);host=0;runtime.announce(0x1004,0x1234);check(messages==1,"Script host gate differs");
    cells.voice_count_8e3d50=3;cells.voices[0]={0,0x1004};cells.voices[1]={8,0x3004};cells.voices[2]={9,0x2004};
    MissionScriptAudioCache cache{1,42};unsigned frees=0;
    bindings.playing=[](auto handle){return handle!=0;};
    bindings.cache=[&](auto name){check(name==0x3004,"Shared script audio was released");return &cache;};
    bindings.free_audio=[&](auto allocation){check(cache.loaded==0&&allocation==42,"Script free order differs");cache.allocation=99;++frees;};
    runtime.cleanup();check(cells.voice_count_8e3d50==2&&frees==0&&cells.voices[0].handle==9,"Shared voice removal differs");
    cells.voices[1].handle=0;runtime.cleanup();check(frees==1&&cache.loaded==0&&cache.allocation==0,"Script audio release differs");
    cells.voice_count_8e3d50=0;
    const auto base=0x4034u;text(base+4,"voice.wav");text(base+0x1c,"radio_123");
    bytes[base+0x34]=5;bindings.word=[](auto at){return at;};
    bindings.load_audio=[&](auto at){check(at==base+4,"Script audio name differs");return 17u;};
    cells.dictionary_8e3f3c=8;bindings.parse_id=[&](auto at){check(at==base+0x1c+6,"Script text suffix differs");return 123u;};
    bindings.text=[](auto dictionary,auto id)->std::optional<std::uint32_t>{check(dictionary==8&&id==123,"Script dictionary differs");return 77;};
    bindings.step=[&](auto& value){check(value.sample==17&&value.text==77&&value.mode==5&&value.position[0]==base+0x38,"Script callback input differs");value.playback=std::array<std::uint32_t,3>{1,2,3};value.text=88;return true;};
    bindings.play=[&](auto sample,const auto& args){check(sample==17&&args[2]==3,"Script playback arguments differ");cells.voice_count_8e3d50=2;return 91u;};
    bindings.show_text=[&](auto value){check(value==88&&cells.voice_count_8e3d50==3&&cells.voices[2].handle==91,"Script publication order differs");};
    runtime.step(0x4000,0);
    bindings.play=[](auto,const auto&)->std::uint32_t{throw std::runtime_error("Playback stopped");};
    bool failed=false;try{runtime.step(0x4000,0);}catch(const std::runtime_error&){failed=true;}
    check(failed&&cells.voice_count_8e3d50==3,"Failed playback published a voice");
    bool played=false;bindings.step=[](auto&){return true;};
    bindings.play=[&](auto,const auto&){played=true;return 1u;};failed=false;
    try{runtime.step(0x4000,0);}catch(const MissionTickError&){failed=true;}
    check(failed&&!played&&cells.voice_count_8e3d50==3,"Missing playback arguments reached the backend");
    queue.entries_8e3d74.push_back({0,0,0,0});failed=false;
    try{runtime.remove(0xdead);}catch(const MissionTickError&){failed=true;}
    check(failed&&queue.entries_8e3d74.size()==3,"Invalid script queue was compacted");
}
void joined_queue(){
    MissionScriptQueueCells queue{10,1,{{0x1000,0,0,0},{0x2000,0,0,0}}};
    std::uint32_t host=1;MissionScriptRuntimeCells cells{queue,host};
    MissionScriptRuntimeBindings bindings;
    bindings.byte=[](auto at){return std::uint8_t((at==0x1004||at==0x2004)?'a':0);};
    bindings.word=[](auto){return 0u;};
    bindings.string=[](auto){return std::string("a");};
    bindings.load_audio=[](auto){return 7u;};
    unsigned steps=0,messages=0,cleanups=0;
    bindings.step=[&](auto&){++steps;return false;};
    bindings.message=[&](auto id,const auto& payload){
        check(id==0x3c&&payload[0]=='a'&&queue.entries_8e3d74.size()==2-messages,
            "Scheduler announcement did not use the shared queue");++messages;
    };
    cells.voice_count_8e3d50=1;cells.voices[0]={9,0x1004};
    bindings.playing=[&](auto handle){check(handle==9&&queue.entries_8e3d74.empty(),
        "Scheduler cleaned voices before queue removal");++cleanups;return true;};
    MissionScriptRuntime runtime(cells,bindings);
    std::uint32_t requested=0,stat=0;MissionProfilerCells profiler{};
    Fixed3 position{};MissionCameraCells camera{position,{},0,0};
    std::array<std::uint32_t,3> timers{};
    MissionFrameServiceBindings frame;
    frame.script_count=[](auto){return 1u;};
    frame.script_call=[&](auto site,auto resource,auto index){runtime.call(site,resource,index);};
    MissionFrameServices services({requested,profiler,timers,stat,camera,queue},frame);
    services.scripts(1);
    check(steps==2&&messages==2&&cleanups==1&&queue.entries_8e3d74.empty(),
        "Scheduler skipped the compacted shared queue slot");
}
}
int main(){try{run();joined_queue();std::cout<<"mission script runtime checks passed\n";}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
