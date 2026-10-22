#include "dfre/mission_audio_cache.hpp"
#include "dfre/audio.hpp"

namespace dfre {
namespace {
using U=std::uint32_t;
template<class F> const F& need(const F& f,const char* error){if(!f)throw MissionAudioCacheError(error);return f;}
U read(const MissionAudioCacheBindings& b,U at,U width=4){return need(b.read,"Missing audio cache reader")(at,width);}
void write(const MissionAudioCacheBindings& b,U at,U value,U width=4){need(b.write,"Missing audio cache writer")(at,width,value);}
U call(const MissionAudioCacheBindings& b,U site,std::vector<U> args){return need(b.call,"Missing audio cache body")(site,args);}
U route(const MissionAudioCacheBindings& b,U first,U second,U loose,U archive){
    if(!read(b,0xd09730)){
        if(!read(b,0xd096e0))return call(b,loose,{first,second});
        return call(b,archive,{first,second,read(b,0xd096d8)});
    }
    const U entry=read(b,0xd09738);return call(b,archive,{first,second,read(b,entry)});
}
}
U mission_audio_cache_seek(const MissionAudioCacheBindings& b,U offset,U origin){return route(b,offset,origin,0x498d60,0x49e6e0);}
U mission_audio_cache_read(const MissionAudioCacheBindings& b,U destination,U size){return route(b,destination,size,0x498d30,0x49e6b0);}
U mission_audio_cache_close_loose(const MissionAudioCacheBindings& b){return call(b,0x4c6854,{read(b,0xd096d0)});}
U mission_audio_cache_close(const MissionAudioCacheBindings& b){if(read(b,0xd09730)||read(b,0xd096e0))return 0;return mission_audio_cache_close_loose(b);}
U mission_audio_cache_allocate_alternate(const MissionAudioCacheBindings& b,U size){
    if(!read(b,0xd09398))return 0;
    write(b,0x5013a0,0xd09398);const U pointer=call(b,0x497f60,{size,4,0,0});write(b,0x5013a0,0xd09384);return pointer;
}
U mission_audio_cache_tag(const MissionAudioCacheBindings& b,U pointer,U tag){
    if(pointer && tag<0x1000000u){write(b,pointer-0xcu,tag);return pointer-0x10u;}return pointer;
}
U mission_audio_cache_convert(const MissionAudioCacheBindings& b,U record){
    const U tag=read(b,0x4cdff0),prior=read(b,record);if(prior==tag)return record;
    const U length=read(b,record+0x28),bits=read(b,record+0x22,2);
    write(b,record,tag);const U rate=read(b,record+0x18),step=audio_mixer_step(rate);
    write(b,record+0xc,0x100,2);write(b,record+8,step);
    if(bits!=16){
        write(b,record+4,length);
        if(!length)throw MissionAudioCacheError("Original8-bit PCM loop cannot terminate within its buffer");
        for(U i=0;i<length;++i)write(b,record+0x10u+i,read(b,record+0x2cu+i,1),1);
        return record;
    }
    const U count=(length>>1)|(length&0x80000000u);write(b,record+4,count);
    if(!count || (count&0x80000000u))throw MissionAudioCacheError("Original16-bit PCM loop cannot terminate within its buffer");
    U peak=0;
    for(U i=0;i<count;++i){U sample=read(b,record+0x2cu+i*2u,2);if(sample&0x8000u)sample=(0u-sample)&0xffffu;if(sample>=peak)peak=sample;}
    if(!peak)throw MissionAudioCacheError("Original PCM normalization divides by zero");
    const U norm=0x7eff0000u/peak;
    for(U i=0;i<count;++i){const U low=read(b,record+0x2cu+i*2u,1);const U signed_low=low&0x80u?low|0xffffff00u:low;
        const U value=((signed_low*norm+0x800000u)>>24)^0x80u;write(b,record+0x10u+i,value&0xffu,1);}
    return record;
}
void mission_audio_cache_load(const MissionAudioCacheBindings& b,U asset,U alternate){
    if(read(b,asset+0x23u,1))return;
    const U size=read(b,asset+0x1cu);
    const U pointer=alternate?mission_audio_cache_allocate_alternate(b,size):call(b,0x497f60,{size,4,0,0x4ce42c});
    if(!pointer){write(b,asset+0x24u,0);return;}
    mission_audio_cache_seek(b,read(b,asset+0x18u),0);
    mission_audio_cache_read(b,pointer,read(b,asset+0x1cu));
    mission_audio_cache_convert(b,pointer);write(b,asset+0x23u,1,1);write(b,asset+0x24u,pointer);
}
} // namespace dfre
