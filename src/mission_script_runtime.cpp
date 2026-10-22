#include "dfre/mission_script_runtime.hpp"
#include <utility>

namespace dfre {
namespace {
template<class F> F need(const F& function,const char* error){if(!function)throw MissionTickError(error);return function;}
std::int32_t signed_word(std::uint32_t value){return value<=0x7fffffffu?std::int32_t(value):std::int32_t(std::int64_t(value)-0x100000000LL);}
}
MissionScriptRuntime::MissionScriptRuntime(MissionScriptRuntimeCells& cells,const MissionScriptRuntimeBindings& bindings)
    :cells_(cells),bindings_(bindings){}
void MissionScriptRuntime::step(std::uint32_t resource,std::uint32_t index){
    const auto base=resource+0x34u+index*0x44u;
    MissionScriptStep value;
    value.sample=need(bindings_.load_audio,"Script audio loader is not bound")(base+4);
    value.mode=need(bindings_.byte,"Script byte source is not bound")(base+0x34);
    for(unsigned axis=0;axis<3;++axis)value.position[axis]=need(bindings_.word,"Script word source is not bound")(base+0x38+axis*4);
    if(!value.sample)need(bindings_.missing_audio,"Script audio diagnostic is not bound")(base+4);
    if(cells_.dictionary_8e3f3c){
        const auto start=base+0x1c;std::uint32_t length=0;
        while(need(bindings_.byte,"Script byte source is not bound")(start+length)){
            if(++length==0)throw MissionTickError("Script text name has no terminator");
        }
        auto cursor=start+length-1;
        while(need(bindings_.byte,"Script byte source is not bound")(cursor-1)!='_'&&cursor>=start)--cursor;
        if(cursor>start){
            const auto id=need(bindings_.parse_id,"Script text id parser is not bound")(cursor);
            const auto text=need(bindings_.text,"Script text dictionary is not bound")(cells_.dictionary_8e3f3c,id);
            if(text)value.text=*text;
        }
    }
    if(!need(bindings_.step,"Script step callback is not bound")(value))return;
    if(value.sample){
        if(!value.playback)throw MissionTickError("Script step callback needs playback arguments");
        const auto handle=need(bindings_.play,"Script playback is not bound")(value.sample,*value.playback);
        const auto slot=cells_.voice_count_8e3d50;
        if(slot>=cells_.voices.size())throw MissionTickError("Script voice table is full");
        cells_.voices[slot]={handle,base+4};cells_.voice_count_8e3d50=slot+1;
    }
    if(bindings_.show_text&&value.text){const auto show=bindings_.show_text;show(value.text);}
}
void MissionScriptRuntime::announce(std::uint32_t name,std::uint16_t index){
    if(!cells_.host_67f528)return;
    std::array<std::uint8_t,10> payload{};
    for(unsigned i=0;i<8;++i){payload[i]=need(bindings_.byte,"Script byte source is not bound")(name+i);if(!payload[i])break;}
    payload[8]=std::uint8_t(index);payload[9]=std::uint8_t(index>>8);
    need(bindings_.message,"Script message sender is not bound")(0x3c,payload);
}
std::uint32_t MissionScriptRuntime::remove(std::uint32_t name){
    auto& entries=cells_.queue.entries_8e3d74;
    if(entries.size()>16)throw MissionTickError("Script queue exceeds sixteen slots");
    for(const auto& entry:entries)if(!entry.resource)throw MissionTickError("Script queue needs active resource entries");
    for(std::size_t i=0;i<entries.size();++i){
        if(need(bindings_.string,"Script name source is not bound")(entries[i].resource+4)==
           need(bindings_.string,"Script name source is not bound")(name)){
            entries.erase(entries.begin()+std::ptrdiff_t(i));return 1;
        }
    }
    return 0;
}
void MissionScriptRuntime::cleanup(){
    const auto voice=[&](std::uint32_t index)->MissionScriptVoice&{
        if(index>=cells_.voices.size())throw MissionTickError("Script voice index exceeds its table");return cells_.voices[index];
    };
    for(std::uint32_t i=0;signed_word(i)<signed_word(cells_.voice_count_8e3d50);){
        if(need(bindings_.playing,"Script playback state is not bound")(voice(i).handle)){++i;continue;}
        bool shared=false;
        for(std::uint32_t j=0;signed_word(j)<signed_word(cells_.voice_count_8e3d50);++j)
            if(j!=i&&need(bindings_.string,"Script name source is not bound")(voice(i).name)==
               need(bindings_.string,"Script name source is not bound")(voice(j).name)){shared=true;break;}
        if(!shared){
            auto* cache=need(bindings_.cache,"Script audio cache is not bound")(voice(i).name);
            if(cache&&cache->loaded){
                cache->loaded=0;
                if(cache->allocation)need(bindings_.free_audio,"Script audio release is not bound")(cache->allocation);
                cache->allocation=0;
            }
        }
        --cells_.voice_count_8e3d50;
        const auto last=cells_.voice_count_8e3d50;
        const auto name=voice(last).name;voice(i).handle=voice(last).handle;voice(i).name=name;
    }
}
void MissionScriptRuntime::call(std::uint32_t site,std::uint32_t resource,std::uint32_t index){
    switch(site){
    case 0x456110:step(resource,index);return;
    case 0x447c60:announce(resource,std::uint16_t(index));return;
    case 0x456000:remove(resource);return;
    case 0x455b40:cleanup();return;
    default:throw MissionTickError("Script runtime call is not bound");
    }
}
} // namespace dfre
