#include "dfre/mission_frame_services.hpp"
#include <iostream>
#include <stdexcept>
#define CHECK(x) do{if(!(x))throw std::runtime_error("frame services: " #x);}while(false)
using namespace dfre;
int main(){try{
    std::uint32_t requested=7,stat=99;MissionProfilerCells profiler{0,0,{0,0}};
    Fixed3 center{100,-200,300};MissionCameraCells camera{center,{},0,11};
    std::array<std::uint32_t,3> timers{0,0x80000000,1};MissionScriptQueueCells scripts{10,0,{}};
    MissionFrameServiceCells cells{requested,profiler,timers,stat,camera,scripts};
    MissionFrameServiceBindings bindings;
    MissionFrameServices missing(cells,bindings);MissionStageCall call;
    for(const auto address:{0x490880u,0x43f0b0u,0x123456u}){call.address=address;CHECK(!missing.stage(call));}
    bool failed=false;try{missing.profile(1);}catch(const MissionTickError&){failed=true;}CHECK(failed);
    failed=false;try{missing.camera();}catch(const MissionTickError&){failed=true;}CHECK(failed);
    missing.scripts(1);scripts.enabled_8e3d68=1;
    failed=false;try{missing.scripts(1);}catch(const MissionTickError&){failed=true;}CHECK(failed);
    bindings.rdtsc=[&]{profiler.current_4f8718=1;profiler.previous_4f871c=9;return std::uint64_t(20)<<10;};
    unsigned reads=0;bindings.actor_word=[&](auto actor,auto offset){
        CHECK(actor==11 && center.x==100 && camera.delta_6ba3e0_e4_dc[0]==0);++reads;
        camera.actor_6ba3f0=22;return offset==0x24?260u:0u;
    };
    bindings.store_word=[&](auto target,auto value){CHECK(target==123 && value==99 && stat==99);stat=88;};
    MissionFrameServices services(cells,bindings);services.profile(0);
    CHECK(profiler.totals_8e3f40[1]==11 && profiler.previous_4f871c==20 && profiler.current_4f8718==0);
    services.camera();CHECK(reads==4 && center.x==110 && center.y==-187 && center.z==281 && camera.actor_6ba3f0==22);
    call.address=0x479ed0;services.stage(call);CHECK(timers[0]==0xffffffff && timers[1]==0x7fffffff && timers[2]==0);
    call.address=0x4702a0;call.argument_count=1;call.args[0]=123;services.stage(call);CHECK(stat==0);
    call.address=0x461340;call.args[0]=42;services.stage(call);call.address=0x461350;CHECK(services.stage(call)==42);
    scripts.entries_8e3d74={{100,0,0,0}};unsigned calls=0;
    bindings.script_count=[](auto){return 2u;};bindings.script_delay=[](auto,auto){return std::uint8_t(255);};
    bindings.script_call=[&](auto site,auto resource,auto index){++calls;
        if(site==0x456110){CHECK(resource==100 && index==0);scripts.entries_8e3d74[0].index=0;scripts.scale_8e3d64=0xffffffff;}
        if(site==0x447c60){CHECK(resource==104 && index==0);scripts.entries_8e3d74.push_back({200,0,2,0});}
    };
    MissionFrameServices queue(cells,bindings);queue.scripts(1);
    CHECK(calls==3 && scripts.entries_8e3d74.size()==2 && scripts.entries_8e3d74[0].index==1 &&
        scripts.entries_8e3d74[0].delay==(std::uint32_t(255)*0xffffffffu)/10u && scripts.entries_8e3d74[1].armed==1);
    bindings.script_call=[&](auto site,auto,auto){if(site==0x456110){scripts.entries_8e3d74[0].index=9;throw std::runtime_error("script stopped");}};
    scripts.entries_8e3d74={{100,0,0,0}};MissionFrameServices throwing(cells,bindings);
    failed=false;try{throwing.scripts(0);}catch(const std::runtime_error&){failed=true;}
    CHECK(failed && scripts.entries_8e3d74[0].index==9);
    std::cout<<"mission frame service checks passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
