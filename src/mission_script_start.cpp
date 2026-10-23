#include "dfre/mission_script_start.hpp"
#include <cstdio>
namespace dfre {namespace {
using U=std::uint32_t;
constexpr U count=0x8e3d70,base=0x8e3d74;
template<class F>const F& need(const F& f,const char* text){if(!f)throw MissionTickError(text);return f;}
U read(const MissionScriptQueueBindings& b,U at){return need(b.read,"Script queue read is not bound")(at);}
void write(const MissionScriptQueueBindings& b,U at,U value){need(b.write,"Script queue write is not bound")(at,value);}
}
U mission_script_queue_start(const MissionScriptQueueBindings& b,U resource){
    for(U slot=0;slot<16;++slot){const auto at=base+slot*16;if(read(b,at))continue;
        write(b,at,resource);write(b,at+4,0);write(b,at+8,0);const auto current=read(b,count);write(b,count,current+1);return 1;
    }return 0;
}
void mission_script_queue_compact(const MissionScriptQueueBindings& b){
    for(U offset=0;offset<0xf0;offset+=16){const auto at=base+offset;if(read(b,at))continue;
        for(U field=0;field<16;field+=4){const auto value=read(b,at+16+field);write(b,at+field,value);}
        need(b.zero16,"Script queue cell clear is not bound")(at+16);
    }
}
U mission_script_queue_remove(const MissionScriptQueueBindings& b,U name){
    if(!read(b,count))return 0;
    U slot=0,at=base;
    for(;;){const auto resource=read(b,at);const auto result=need(b.compare,"Script name comparison is not bound")(resource+4,name);
        if(!result){const auto current=read(b,count);write(b,count,current-1);need(b.zero16,"Script queue cell clear is not bound")(at);mission_script_queue_compact(b);return 1;}
        const auto current=read(b,count);++slot;at+=16;if(slot>=current)return 0;
    }
}
U mission_script_registry_start(const MissionScriptQueueBindings& b,U name){
    auto node=read(b,0x8e3d58);
    while(node){const auto resource=read(b,node);if(!need(b.compare,"Script registry comparison is not bound")(resource+4,name))return mission_script_queue_start(b,resource);node=read(b,node+4);}
    return 0;
}
MissionScriptStart::MissionScriptStart(MissionScriptQueueCells& queue,const MissionScriptStartBindings& bindings):queue_(queue),bindings_(bindings){}
U MissionScriptStart::start(U resource){
    if(!resource)throw MissionTickError("Initialized script start needs a nonzero resource");
    const auto& entries=queue_.entries_8e3d74;if(entries.size()>16)throw MissionTickError("Script queue exceeds sixteen cells");
    for(const auto& entry:entries)if(!entry.resource)throw MissionTickError("Initialized script queue needs a nonzero prefix");
    if(entries.size()==16)return 0;
    queue_.entries_8e3d74.push_back({resource,0,0,0});return 1;
}
U MissionScriptStart::named(const MissionScriptStartName& name){
    auto node=need(bindings_.registry_head,"Script registry head is not bound")();
    while(node){
        const auto resource=need(bindings_.node_word,"Script registry node is not bound")(node,0);
        if(!need(bindings_.compare,"Script registry comparison is not bound")(resource+4,name))return start(resource);
        node=need(bindings_.node_word,"Script registry node is not bound")(node,4);
    }return 0;
}
U MissionScriptStart::start_name(U name){return named(MissionScriptStartName{name});}
void MissionScriptStart::event(std::int32_t id){
    if(!id)return;
    std::array<char,32> text{};const auto length=std::snprintf(text.data(),text.size(),"dlg%03d",int(id));
    if(length<0||std::size_t(length)>=text.size())throw MissionTickError("Script event name exceeds its source buffer");
    (void)named(MissionScriptStartName{std::string_view(text.data(),std::size_t(length))});
}
}
