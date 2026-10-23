#include "dfre/mission_script_registry.hpp"
#include "dfre/mission_audio_cache.hpp"
#include "dfre/mission_sound_assets.hpp"
namespace dfre {namespace {
using U=std::uint32_t;
template<class F>const F& need(const F& fn,const char* error){if(!fn)throw MissionTickError(error);return fn;}
U read(const MissionSoundBankBindings& b,U at,U width=4){return need(b.read,"Script registry read is not bound")(at,width);}
void write(const MissionSoundBankBindings& b,U at,U value,U width=4){need(b.write,"Script registry write is not bound")(at,width,value);}
U call(const MissionSoundBankBindings& b,U site,std::initializer_list<U> args){return need(b.call,"Script registry service is not bound")(site,{args});}
}
U mission_script_registry_node(const MissionSoundBankBindings& b){
    const auto node=call(b,0x497f60,{8,4,0,0x4cca40});call(b,0x49a620,{node,0,8});
    const auto head=read(b,0x8e3d58);if(!head){write(b,0x8e3d58,node);return node;}
    const auto first_next=read(b,head+4);auto cursor=head;
    if(first_next)do{cursor=read(b,cursor+4);}while(read(b,cursor+4));
    write(b,cursor+4,node);return node;
}
void mission_script_registry_clear_queue(const MissionSoundBankBindings& b){call(b,0x49a620,{0x8e3d74,0,0x100});write(b,0x8e3d70,0);}
void mission_script_registry_audio_destroy(const MissionSoundBankBindings& b,U context){
    const auto first=read(b,context+0x1c);if(first){call(b,0x4980d0,{first});write(b,context+0x1c,0);}
    const auto second=read(b,context+0x20);if(second){call(b,0x4980d0,{second});write(b,context+0x20,0);}
    if(read(b,context+0x38))call(b,0x49db20,{context});
    call(b,0x495160,{0});
}
void mission_script_registry_destroy(const MissionSoundBankBindings& b){
    mission_script_registry_clear_queue(b);call(b,0x4980d0,{read(b,0x8e3d60)});
    auto node=read(b,0x8e3d58);while(node){const auto resource=read(b,node),next=read(b,node+4);call(b,0x4980d0,{resource});call(b,0x4980d0,{node});node=next;}
    mission_script_registry_audio_destroy(b,0x8e3e74);call(b,0x4980d0,{read(b,0x8e3f3c)});call(b,0x49a620,{0x8e3d54,0,0x1ec});
}
U mission_script_registry_load(const MissionSoundBankBindings& b,U name,U scratch){
    call(b,0x49a620,{0x8e3d54,0,0x1ec});if(!name)return 0;
    if(call(b,0x499120,{name})==0xffffffffu)return 0;
    call(b,0x499280,{scratch+0x13c,0x1c});if(read(b,scratch+0x13c)!=0x30474c44){call(b,0x499020,{});return 0;}
    write(b,0x8e3d5c,read(b,scratch+0x148));write(b,0x8e3d54,read(b,scratch+0x150));
    const auto ids=call(b,0x497f60,{read(b,scratch+0x148)*32,4,0,0x4cca48});write(b,0x8e3d60,ids);
    if(!ids){call(b,0x499020,{});return 0;}
    call(b,0x4992d0,{read(b,scratch+0x14c),0});const auto size=read(b,0x8e3d5c)*32;const auto target=read(b,0x8e3d60);call(b,0x499280,{target,size});
    call(b,0x4992d0,{read(b,scratch+0x154),0});
    if(read(b,0x8e3d54)){
        U index=0;for(;;){const auto node=mission_script_registry_node(b);if(!node){call(b,0x499020,{});mission_script_registry_destroy(b);return 0;}
            call(b,0x499280,{scratch+0x108,0x34});const auto resource=call(b,0x497f60,{read(b,scratch+0x124)*0x44+0x34,4,0,0x4cca58});write(b,node,resource);
            if(!resource){call(b,0x499020,{});mission_script_registry_destroy(b);return 0;}
            call(b,0x4992d0,{0xffffffccu,1});const auto bytes=read(b,scratch+0x124)*0x44+0x34;const auto destination=read(b,node);++index;call(b,0x499280,{destination,bytes});
            if(index>=read(b,0x8e3d54))break;
        }
    }
    call(b,0x499020,{});const auto stem=call(b,0x49ba00,{name,0x4cca68});
    if(stem){call(b,0x493a80,{scratch,0x4cca6c,stem});call(b,0x49cf50,{scratch,0x8e3e74});call(b,0x493a80,{scratch,0x4cca74,stem});const auto text=call(b,0x499f1f,{scratch});write(b,0x8e3f3c,text);}
    write(b,0x8e3d50,0);return 1;
}
U mission_script_registry_audio(const MissionSoundBankBindings& b,U name,U context){
    if(!context)return 0;const auto sample=call(b,0x49cfd0,{name,context});if(!sample)return 0;
    if(!read(b,sample+0x23,1)){call(b,0x499120,{context+0x3c});call(b,0x49d0e0,{sample});call(b,0x499020,{});}
    const auto tag=read(b,context+0xc4),allocation=read(b,sample+0x24);call(b,0x498230,{allocation,tag});return read(b,sample+0x24);
}
std::int32_t mission_script_registry_compare(const MissionSoundBankBindings& b,U left,U right){
    if(left==right)return 0;
    for(;;){for(U offset=0;offset<16;offset+=4){const auto a=read(b,left+offset),c=read(b,right+offset);
        if(a!=c){for(unsigned shift=0;shift<32;shift+=8){const auto x=(a>>shift)&255u,y=(c>>shift)&255u;if(x!=y)return x<y?-1:1;if(!x)return 0;}}
        if(((a+0xfefefeffu)&~c&0x80808080u)!=0)return 0;
    }left+=16;right+=16;}
}
U mission_script_registry_token(const MissionSoundBankBindings& b,U text,U delimiters,U scratch){
    if(!text){const auto thread=call(b,0x501d60,{});text=read(b,thread+0x10);if(!text)return 0;}
    call(b,0x49a620,{scratch,0,32});U cursor=delimiters;
    for(auto value=read(b,cursor,1);value;value=read(b,++cursor,1)){
        const auto bit=read(b,0x4ceb80+(value&7),1),prior=read(b,scratch+(value>>3),1);write(b,scratch+(value>>3),prior|bit,1);
    }
    auto value=read(b,text,1);
    while(value){const auto bits=read(b,scratch+(value>>3),1),bit=read(b,0x4ceb80+(value&7),1);if(!(bits&bit))break;++text;value=read(b,text,1);}
    if(!value)return 0;
    cursor=text;value=read(b,cursor,1);
    while(value){const auto bits=read(b,scratch+(value>>3),1),bit=read(b,0x4ceb80+(value&7),1);if(bits&bit){write(b,cursor,0,1);++cursor;const auto thread=call(b,0x501d60,{});write(b,thread+0x10,cursor);return text;}++cursor;value=read(b,cursor,1);}
    const auto thread=call(b,0x501d60,{});write(b,thread+0x10,0);return text;
}
U mission_script_registry_id(const MissionSoundBankBindings& b,U text){
    while(read(b,0x5013f4+((read(b,text,1)+1)&255u),1)&2)++text;
    const auto sign=read(b,text,1);if(sign=='+'||sign=='-')++text;U value=0;
    while(read(b,0x5013f4+((read(b,text,1)+1)&255u),1)&0x20){value=value*10+read(b,text,1)-'0';++text;}
    return sign=='-'?0u-value:value;
}
bool mission_script_registry_step(const MissionSoundBankBindings& b,U record){
    if(!read(b,0x67f51c))return false;
    write(b,record+0x18,0);write(b,record+0x20,0x10000);const auto value=read(b,0x67f51c);const auto product=value*255u;
    write(b,record+0x1c,(product>>8)|((product&0x80000000u)?0xff000000u:0));return true;
}
void mission_script_registry_text(const MissionSoundBankBindings& b,U text){
    if(!text)return;U length=0;while(read(b,text+length,1))++length;if(length)call(b,0x479f00,{text,0xffffffffu});
}
}

#include <cstring>
#include <exception>
#include <map>
#include <string>
#include <utility>
namespace dfre {
struct MissionScriptRegistry::Impl {
    MissionScriptRuntimeCells& cells;const MissionSoundBankBindings& backend;U scratch,size,depth=0;MissionScriptRegistryBindings host;
    MissionSoundBankBindings bindings;MissionScriptRuntimeBindings runtime_bindings;MissionScriptRuntime runtime;
    MissionScriptStartBindings start_bindings;MissionScriptStart starter;
    struct Cache {U address;MissionScriptAudioCache value;std::uint8_t loaded;U allocation;};std::vector<std::unique_ptr<std::map<U,Cache>>> cache_scopes;
    struct Stack {Impl& owner;U at;explicit Stack(Impl& o):owner(o),at(o.scratch+o.depth*0x200){if(std::uint64_t(o.depth+1)*0x200>o.size)throw MissionTickError("Script scratch nesting exceeds its owned span");++o.depth;}~Stack(){--owner.depth;}};
    Impl(MissionScriptRuntimeCells& c,const MissionSoundBankBindings& b,U at,U bytes,MissionScriptRegistryBindings h)
        :cells(c),backend(b),scratch(at),size(bytes),host(std::move(h)),runtime(c,runtime_bindings),starter(c.queue,start_bindings){
        if((at&3)||bytes<0x200||std::uint64_t(at)+bytes>0x100000000ull)throw MissionTickError("Script scratch needs an aligned complete shared span");
        bindings.read=[this](U address,U width){return get(address,width);};bindings.write=[this](U address,U width,U value){put(address,width,value);};bindings.call=[this](U site,const std::vector<U>& args){return invoke(site,args);};
        start_bindings.registry_head=[this](){return get(0x8e3d58,4);};start_bindings.node_word=[this](U node,U offset){return get(node+offset,4);};
        start_bindings.compare=[this](U left,const MissionScriptStartName& right){if(const auto* address=std::get_if<U>(&right))return mission_script_registry_compare(bindings,left,*address);Stack stack(*this);text(stack.at,std::get<std::string_view>(right));return mission_script_registry_compare(bindings,left,stack.at);};
        runtime_bindings.byte=[this](U address){std::uint8_t result=0;boundary([&]{result=std::uint8_t(get(address,1));});return result;};
        runtime_bindings.word=[this](U address){U result=0;boundary([&]{result=get(address,4);});return result;};
        runtime_bindings.string=[this](U address){std::string result;boundary([&]{result=string(address);});return result;};
        runtime_bindings.load_audio=[this](U name){U result=0;boundary([&]{result=mission_script_registry_audio(bindings,name,0x8e3e74);});return result;};
        runtime_bindings.missing_audio=[this](U name){boundary([&]{need(host.missing_audio,"Script audio diagnostic is not bound")(name);});};
        runtime_bindings.parse_id=[this](U name){U result=0;boundary([&]{result=mission_script_registry_id(bindings,name);});return result;};
        runtime_bindings.text=[this](U dictionary,U id){std::optional<U> result;boundary([&]{const auto address=invoke(0x49a076,{dictionary,id});if(address)result=get(address,4);});return result;};
        runtime_bindings.step=[this](MissionScriptStep& step){bool result=false;boundary([&]{const auto callback=get(0x8e3d68,4);if(callback==0x43e430){Stack stack(*this);result=mission_script_registry_step(bindings,stack.at);if(result)step.playback=std::array<U,3>{get(stack.at+0x18,4),get(stack.at+0x1c,4),get(stack.at+0x20,4)};}else{if(!callback)throw MissionTickError("Script step callback is null");result=need(host.step,"Script step callback body is not bound")(callback,step);}});return result;};
        runtime_bindings.play=[this](U sample,const std::array<U,3>& args){U result=0;boundary([&]{result=invoke(0x494dd0,{sample,args[0],args[1],args[2]});});return result;};
        runtime_bindings.show_text=[this](U name){boundary([&]{const auto callback=get(0x8e3d6c,4);if(!callback)return;if(callback==0x43e470)mission_script_registry_text(bindings,name);else need(host.show_text,"Script text callback body is not bound")(callback,name);});};
        runtime_bindings.playing=[this](U handle){bool result=false;boundary([&]{result=invoke(0x494ff0,{handle})!=0;});return result;};
        runtime_bindings.cache=[this](U name)->MissionScriptAudioCache*{U address=0;boundary([&]{MissionSoundAssetBindings b{bindings.read,bindings.write,bindings.call};address=mission_sound_find_sample(b,name,0x8e3e74);});if(!address)return nullptr;if(cache_scopes.empty())throw MissionTickError("Script cache needs its runtime call scope");auto& caches=*cache_scopes.back();auto found=caches.find(address);const auto loaded=std::uint8_t(get(address+0x23,1));const auto allocation=get(address+0x24,4);if(found==caches.end())found=caches.emplace(address,Cache{address,{loaded,allocation},loaded,allocation}).first;else found->second={address,{loaded,allocation},loaded,allocation};return &found->second.value;};
        runtime_bindings.free_audio=[this](U pointer){boundary([&]{invoke(0x4980d0,{pointer});});};
        runtime_bindings.message=[this](U id,const std::array<std::uint8_t,10>& bytes){boundary([&]{need(host.message,"Script message producer is not bound")(id,bytes);});};
    }
    static void width(U n){if(n!=1&&n!=2&&n!=4)throw MissionTickError("Script memory width is invalid");}
    U* scalar(U address){
        if(address>=0x8e3d50&&address<0x8e3d54)return &cells.voice_count_8e3d50;
        if(address>=0x8e3d64&&address<0x8e3d68)return &cells.queue.scale_8e3d64;
        if(address>=0x8e3d68&&address<0x8e3d6c)return &cells.queue.enabled_8e3d68;
        if(address>=0x8e3f3c&&address<0x8e3f40)return &cells.dictionary_8e3f3c;
        if(address>=0x8e3550&&address<0x8e3950)return &cells.voices[(address-0x8e3550)/4].handle;
        if(address>=0x8e3950&&address<0x8e3d50)return &cells.voices[(address-0x8e3950)/4].name;
        return nullptr;
    }
    bool owns(U address,U n)const {
        width(n);const auto end=std::uint64_t(address)+n;
        bool overlap=false,complete=false;
        for(const auto& range:std::array<std::array<U,2>,4>{{{{0x8e3550,0x8e3d54}},{{0x8e3d64,0x8e3d6c}},{{0x8e3d70,0x8e3e74}},{{0x8e3f3c,0x8e3f40}}}}){
            if(address<range[1]&&end>range[0]){overlap=true;complete=address>=range[0]&&end<=range[1];break;}
        }
        if(overlap&&(!complete||(address&3)+n>4))throw MissionTickError("Script access crosses its current cell");
        return complete;
    }
    U get(U address,U n){
        width(n);(void)owns(address,n);
        if(address>=0x67f51c&&address+n<=0x67f520){const auto value=need(host.voice_volume,"Current script voice volume is not bound")();return (value>>((address&3)*8))&(n==4?0xffffffffu:(1u<<(n*8))-1);}
        if(const auto* word=scalar(address)){if((address&3)+n>4)throw MissionTickError("Script access crosses its current word");return (*word>>((address&3)*8))&(n==4?0xffffffffu:(1u<<(n*8))-1);}
        if(address>=0x8e3d70&&address+n<=0x8e3d74){const U value=U(cells.queue.entries_8e3d74.size());return (value>>((address&3)*8))&(n==4?0xffffffffu:(1u<<(n*8))-1);}
        if(address>=0x8e3d74&&address+n<=0x8e3e74){const auto offset=address-0x8e3d74,slot=offset/16,field=(offset%16)/4;U value=0;if(slot<cells.queue.entries_8e3d74.size()){const auto& entry=cells.queue.entries_8e3d74[slot];std::memcpy(&value,reinterpret_cast<const std::uint8_t*>(&entry)+field*4,4);}return (value>>((address&3)*8))&(n==4?0xffffffffu:(1u<<(n*8))-1);}
        return need(backend.read,"Script shared memory reader is not bound")(address,n);
    }
    void put(U address,U n,U value){
        width(n);(void)owns(address,n);const auto shift=(address&3)*8,mask=n==4?0xffffffffu:((1u<<(n*8))-1)<<shift;
        if(auto* word=scalar(address)){if((address&3)+n>4)throw MissionTickError("Script access crosses its current word");*word=(*word&~mask)|((value<<shift)&mask);return;}
        if(address>=0x8e3d70&&address+n<=0x8e3d74){const auto current=U(cells.queue.entries_8e3d74.size()),next=(current&~mask)|((value<<shift)&mask);if(next==0){cells.queue.entries_8e3d74.clear();return;}if(next!=current)throw MissionTickError("Script queue count needs its canonical queue operation");return;}
        if(address>=0x8e3d74&&address+n<=0x8e3e74){const auto offset=address-0x8e3d74,slot=offset/16,field=(offset%16)/4;if(slot>=cells.queue.entries_8e3d74.size()){if(value&((n==4)?0xffffffffu:((1u<<(n*8))-1)))throw MissionTickError("Unused initialized script cells must remain zero");return;}auto& entry=cells.queue.entries_8e3d74[slot];U old=0;std::memcpy(&old,reinterpret_cast<const std::uint8_t*>(&entry)+field*4,4);old=(old&~mask)|((value<<shift)&mask);std::memcpy(reinterpret_cast<std::uint8_t*>(&entry)+field*4,&old,4);return;}
        need(backend.write,"Script shared memory writer is not bound")(address,n,value);
    }
    std::string string(U address){std::string result;for(U i=0;i<65536;++i){const auto c=get(address+i,1);if(!c)return result;result+=char(c);}throw MissionTickError("Script string exceeds its owned boundary");}
    void text(U address,std::string_view value){for(std::size_t i=0;i<value.size();++i)put(address+U(i),1,std::uint8_t(value[i]));put(address+U(value.size()),1,0);}
    void flush(){for(auto& scope:cache_scopes)for(auto& pair:*scope){auto& item=pair.second;if(item.value.loaded!=item.loaded){put(item.address+0x23,1,item.value.loaded);item.loaded=item.value.loaded;}if(item.value.allocation!=item.allocation){put(item.address+0x24,4,item.value.allocation);item.allocation=item.value.allocation;}}}
    void refresh(){for(auto& scope:cache_scopes)for(auto& pair:*scope){auto& item=pair.second;item.value.loaded=std::uint8_t(get(item.address+0x23,1));item.value.allocation=get(item.address+0x24,4);item.loaded=item.value.loaded;item.allocation=item.value.allocation;}}
    void boundary(const std::function<void()>& action){flush();try{action();}catch(...){const auto error=std::current_exception();refresh();std::rethrow_exception(error);}refresh();}
    template<class F>void run(F action){cache_scopes.push_back(std::make_unique<std::map<U,Cache>>());try{action();}catch(...){const auto error=std::current_exception();flush();cache_scopes.pop_back();std::rethrow_exception(error);}flush();cache_scopes.pop_back();}
    U invoke(U site,const std::vector<U>& args){
        if(const auto result=service(site,args))return *result;
        return need(backend.call,"Script foreign service is not bound")(site,args);
    }
    std::optional<U> service(U site,const std::vector<U>& a){
        switch(site){
        case 0x455c30:return mission_script_registry_node(bindings);
        case 0x455c90:{Stack stack(*this);return mission_script_registry_load(bindings,a.at(0),stack.at);}
        case 0x455ea0:mission_script_registry_destroy(bindings);return 0;
        case 0x4560a0:mission_script_registry_clear_queue(bindings);return 0;
        case 0x456070:put(0x8e3d64,4,a.at(0));return 0;
        case 0x456080:put(0x8e3d68,4,a.at(0));return 0;
        case 0x456090:put(0x8e3d6c,4,a.at(0));return 0;
        case 0x455f10:return starter.start(a.at(0));
        case 0x455f60:return starter.start_name(a.at(0));
        case 0x43f500:{std::int32_t id;const auto value=a.at(0);std::memcpy(&id,&value,4);starter.event(id);return 0;}
        case 0x456110:run([&]{runtime.step(a.at(0),a.at(1));});return 0;
        case 0x447c60:run([&]{runtime.announce(a.at(0),std::uint16_t(a.at(1)));});return 0;
        case 0x456000:{U result=0;run([&]{result=runtime.remove(a.at(0));});return result;}
        case 0x455b40:run([&]{runtime.cleanup();});return 0;
        case 0x49d400:return mission_script_registry_audio(bindings,a.at(0),a.at(1));
        case 0x49cfd0:{MissionSoundAssetBindings b{bindings.read,bindings.write,bindings.call};return mission_sound_find_sample(b,a.at(0),a.at(1));}
        case 0x49d0e0:{MissionAudioCacheBindings b{bindings.read,bindings.write,bindings.call};mission_audio_cache_load(b,a.at(0),0);return 0;}
        case 0x49cf70:mission_script_registry_audio_destroy(bindings,a.at(0));return 0;
        case 0x49ad30:return U(mission_script_registry_compare(bindings,a.at(0),a.at(1)));
        case 0x49ba00:{Stack stack(*this);return mission_script_registry_token(bindings,a.at(0),a.at(1),stack.at);}
        case 0x49dff0:return mission_script_registry_id(bindings,a.at(0));
        case 0x43e430:return mission_script_registry_step(bindings,a.at(0));
        case 0x43e470:mission_script_registry_text(bindings,a.at(0));return 0;
        case 0x501d60:return need(host.crt_thread,"Current script CRT thread is not bound")();
        case 0x49a620:{if(a.at(1))throw MissionTickError("Script clear needs a zero fill");U offset=0;for(;a.at(2)-offset>=4;offset+=4)put(a.at(0)+offset,4,0);for(;offset<a.at(2);++offset)put(a.at(0)+offset,1,0);return a.at(0);}
        case 0x493a80:{if(a.at(1)!=0x4cca6c&&a.at(1)!=0x4cca74)return std::nullopt;const auto name=string(a.at(2));const auto suffix=a.at(1)==0x4cca6c?".pwf":".bin";if(name.size()+4>=0x108)throw MissionTickError("Script archive filename exceeds its source buffer");text(a.at(0),name+suffix);return U(name.size()+4);}
        default:return std::nullopt;
        }
    }
};
MissionScriptRegistry::MissionScriptRegistry(MissionScriptRuntimeCells& c,const MissionSoundBankBindings& b,U at,U size,MissionScriptRegistryBindings h):impl_(std::make_unique<Impl>(c,b,at,size,std::move(h))){}
MissionScriptRegistry::~MissionScriptRegistry()=default;
std::optional<U> MissionScriptRegistry::service(U site,const std::vector<U>& args){return impl_->service(site,args);}
MissionScriptStart& MissionScriptRegistry::start(){return impl_->starter;}
const MissionSoundBankBindings& MissionScriptRegistry::memory()const{return impl_->bindings;}
bool MissionScriptRegistry::owns(U address,U n)const{return impl_->owns(address,n);}
MissionFrameServiceBindings MissionScriptRegistry::frame_bindings(){MissionFrameServiceBindings b;b.script_count=[this](U resource){return impl_->get(resource+0x1c,4);};b.script_delay=[this](U resource,U index){return std::uint8_t(impl_->get(resource+index*0x44+0x69,1));};b.script_call=[this](U site,U resource,U index){if(!service(site,{resource,index}))throw MissionTickError("Frame script call is not owned by the registry");};return b;}
}
