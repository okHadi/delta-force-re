#include "dfre/mission_frame_services.hpp"
#include <cstring>
#include <utility>

namespace dfre {
namespace {
std::uint32_t sar(std::uint32_t value,unsigned bits){
    return (value>>bits)|((value&0x80000000u)?(~std::uint32_t(0)<<(32-bits)):0);
}
void require(bool value,const char* message){if(!value)throw MissionTickError(message);}
std::int32_t integer(std::uint32_t value){std::int32_t result;std::memcpy(&result,&value,4);return result;}
}
MissionFrameServices::MissionFrameServices(MissionFrameServiceCells cells,MissionFrameServiceBindings bindings)
    :cells_(cells),bindings_(std::move(bindings)){}
void MissionFrameServices::profile(std::uint32_t next){
    require(bool(bindings_.rdtsc),"Frame profiler needs an RDTSC provider");
    const auto time=std::uint32_t(bindings_.rdtsc()>>10);
    auto& cells=cells_.profiler;
    require(cells.current_4f8718<cells.totals_8e3f40.size(),"Frame profiler index exceeds its totals");
    cells.totals_8e3f40[cells.current_4f8718]+=time-cells.previous_4f871c;
    cells.previous_4f871c=time;cells.current_4f8718=next;
}
void MissionFrameServices::camera(){
    require(bool(bindings_.actor_word),"Frame camera needs current actor words");
    auto& cells=cells_.camera;
    const auto actor=cells.actor_6ba3f0;
    std::array<std::uint32_t,4> words;
    for(std::uint32_t axis=0;axis<4;++axis)words[axis]=bindings_.actor_word(actor,0x24+axis*4);
    auto& position=cells.position_6ba3d0;
    const std::array<std::uint32_t,3> before{std::uint32_t(position.x),std::uint32_t(position.y),std::uint32_t(position.z)};
    for(std::uint32_t axis=0;axis<3;++axis)
        cells.delta_6ba3e0_e4_dc[axis]=words[axis]-before[axis];
    const auto yaw=sar(words[3]-cells.yaw_6ba3e8+0x10,5);
    position.x=integer(before[0]+sar(cells.delta_6ba3e0_e4_dc[0]+8,4));
    position.y=integer(before[1]+sar(cells.delta_6ba3e0_e4_dc[1]+8,4));
    position.z=integer(before[2]+sar(cells.delta_6ba3e0_e4_dc[2]+8,4));
    cells.yaw_6ba3e8+=yaw;
}
void MissionFrameServices::scripts(std::uint32_t pulse){
    auto& cells=cells_.scripts;
    if(!cells.enabled_8e3d68)return;
    const auto call=[&](std::uint32_t site,std::uint32_t resource=0,std::uint32_t index=0){
        require(bool(bindings_.script_call),"Frame script scheduler needs reached script bodies");
        bindings_.script_call(site,resource,index);
    };
    const auto entry=[&](std::uint32_t index)->MissionScriptQueueEntry&{
        require(index<cells.entries_8e3d74.size(),"Frame script callback removed its current queue slot");
        return cells.entries_8e3d74[index];
    };
    for(std::uint32_t i=0;i<cells.entries_8e3d74.size();++i){
        if(entry(i).delay){
            if(entry(i).armed)--entry(i).delay;
            else if(pulse)entry(i).armed=1;
            continue;
        }
        const auto resource=entry(i).resource,index=entry(i).index;
        require(bool(bindings_.script_count),"Frame script scheduler needs current step counts");
        if(index<bindings_.script_count(resource)){
            call(0x456110,resource,index);
            call(0x447c60,resource+4,entry(i).index);
        }
        if(entry(i).index<bindings_.script_count(resource)-1u){
            ++entry(i).index;
            require(bool(bindings_.script_delay),"Frame script scheduler needs its step delay");
            const auto delay=bindings_.script_delay(resource,index);
            entry(i).delay=(std::uint32_t(delay)*cells.scale_8e3d64)/10u;
            entry(i).armed=0;
        }else{
            --i;call(0x456000,resource+4);
        }
    }
    call(0x455b40);
}
std::optional<std::uint32_t> MissionFrameServices::stage(const MissionStageCall& call){
    const auto argument=[&]{require(call.argument_count>0,"Frame service needs its argument");return call.args[0];};
    switch(call.address){
    case 0x461350:return cells_.requested_8ee1d4;
    case 0x461340:cells_.requested_8ee1d4=argument();return 0;
    case 0x456e30:profile(argument());return 0;
    case 0x479ed0:for(auto& timer:cells_.timers_bd4814)--timer;return 0;
    case 0x4702a0:{const auto target=argument();
        require(bool(bindings_.store_word),"Frame statistic needs its output word");
        bindings_.store_word(target,cells_.stat_bcfc24);cells_.stat_bcfc24=0;return 0;}
    case 0x42ab70:camera();return 0;
    case 0x4562a0:scripts(argument());return 0;
    default:return std::nullopt;
    }
}
} // namespace dfre
