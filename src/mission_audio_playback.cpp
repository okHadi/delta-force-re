#include "dfre/mission_audio_playback.hpp"
#include "dfre/audio_mixer.hpp"
#include <cstring>
#include <stdexcept>
namespace dfre {
namespace {
constexpr std::uint32_t live=0x4a16d2,pending=0x4a1a12,fallback=0x4a1d52,ambient=0x4a2052,cursor=0x4a2392,stream=0x4a23c2,silent=0x4a16d1;
std::int32_t si(std::uint32_t n){return n<=0x7fffffffu?std::int32_t(n):std::int32_t(std::int64_t(n)-0x100000000LL);}
std::uint32_t word(const std::uint8_t* p,unsigned at){std::uint32_t n=0;for(unsigned i=0;i<4;++i)n|=std::uint32_t(p[at+i])<<(8*i);return n;}
std::uint32_t frequency(std::uint32_t rate,std::uint32_t step){const auto product=std::int64_t(si(rate?rate:0x10000))*si(step);return std::uint32_t(si(std::uint32_t(std::uint64_t(product)>>32))>>2);}
template<class F> const F& need(const F& f,const char* text){if(!f)throw std::runtime_error(text);return f;}
void channel(std::uint32_t value){if(value>=16)throw std::runtime_error("Audio channel exceeds16 voices");}
}
MissionAudioPlayback::MissionAudioPlayback(MissionAudioPlaybackCells& c,const MissionAudioPlaybackBindings& b):cells_(c),bindings_(b){}
std::uint32_t MissionAudioPlayback::read(std::uint32_t address,unsigned width) const{
    if(address<live||std::uint64_t(address-live)+width>cells_.records_4a16d2.size())throw std::runtime_error("Audio record read exceeds its owner");
    const auto* p=cells_.records_4a16d2.data()+address-live;std::uint32_t value=0;for(unsigned i=0;i<width;++i)value|=std::uint32_t(p[i])<<(8*i);return value;
}
void MissionAudioPlayback::write(std::uint32_t address,std::uint32_t value,unsigned width){
    if(address<live||std::uint64_t(address-live)+width>cells_.records_4a16d2.size())throw std::runtime_error("Audio record write exceeds its owner");
    auto* p=cells_.records_4a16d2.data()+address-live;for(unsigned i=0;i<width;++i)p[i]=std::uint8_t(value>>(8*i));
}
void MissionAudioPlayback::initialize(){
    cells_.records_4a16d2.fill(0);
    for(unsigned i=0;i<16;++i)for(auto address:{live+i*0x34,fallback+i*0x30,ambient+i*0x34}){
        write(address+4,0x400);write(address+12,0xfff00400);write(address,silent);write(address+8,0x400);
    }
    write(cursor+4,0x400);write(cursor+12,0xfff00400);write(cursor,silent);write(cursor+8,0x1000);
    cells_.filter_4a4bcc=0;cells_.filter_4a4bd0=0;cells_.filter_4b4ca2=7;
    cells_.filter_4a4bb4=0xfff;cells_.filter_4a4bbc=0x7ff;cells_.filter_4b4ca5=0x1f;
    cells_.filter_4a4bb8=0xe00;cells_.filter_4a4bc0=0x700;cells_.filter_4a4bc4=0x4b4bd4;cells_.filter_4a4bc8=0;
}
std::uint32_t MissionAudioPlayback::start(std::uint32_t record,std::uint32_t sample,std::uint32_t pan,std::uint32_t volume,std::uint32_t rate,std::uint32_t callback){
    const auto view=need(bindings_.sample,"Audio sample records are not bound")(sample);
    if(!view.bytes||view.length<16)throw std::runtime_error("Audio sample record is incomplete");
    write(record+0x20,view.bytes[12]);write(record+0x18,pan);write(record+0x1c,volume);
    const auto gains=compute_voice_gain(si(pan),si(volume),si(cells_.master_4a2716),int(read(record+0x20)));
    write(record+0x24,std::uint32_t(gains.right));write(record+0x28,std::uint32_t(gains.left));
    const auto length=word(view.bytes,4);write(record,view.source_address+16+length);write(record+4,length);
    write(record+12,(0u-(length-1u))<<10);const auto step=word(view.bytes,8);write(record+0x14,step);write(record+8,frequency(rate,step));
    const auto generation=cells_.generation_4a2ae0;write(record+0x10,generation);++cells_.generation_4a2ae0;
    write(record+0x2c,callback);write(record+0x30,1,1);return generation;
}
std::uint32_t MissionAudioPlayback::play(std::uint32_t sample,std::uint32_t pan,std::uint32_t volume,std::uint32_t rate){
    if(!cells_.enabled_d08940)return 0;
    for(unsigned pass=0;pass<2;++pass){cells_.scan_4b4f5a=16;
        for(unsigned i=0;i<16;++i){const auto a=live+i*0x34,b=pending+i*0x34;
            if(!read(a+0x31,1)&&!read(b+0x30,1)&&(!pass?!read(a+0x30,1):si(volume)>si(read(a+0x1c))))return start(b,sample,pan,volume,rate,0);
            --cells_.scan_4b4f5a;
        }
    }return 0;
}
std::uint32_t MissionAudioPlayback::play_channel(std::uint32_t slot,std::uint32_t sample,std::uint32_t pan,std::uint32_t volume,std::uint32_t rate){
    if(!cells_.enabled_d08940)return 0;channel(slot);const auto record=pending+slot*0x34;
    if(read(record+0x30,1))return 0;return start(record,sample,pan,volume,rate,0);
}
std::uint32_t MissionAudioPlayback::start_ambient(std::uint32_t sample,std::uint32_t pan,std::uint32_t volume,std::uint32_t rate,std::uint32_t slot){
    if(!cells_.enabled_d08940)return 0;channel(slot);const auto record=ambient+slot*0x34;write(record+0x30,0,1);
    const auto result=start(record,sample,pan,volume,rate,0);if(read(live+slot*0x34)==silent)write(live+slot*0x34+12,0xffffffff);return result;
}
void MissionAudioPlayback::change(std::uint32_t record,std::uint32_t pan,std::uint32_t volume,std::uint32_t rate){
    if(pan==0xffffffff)pan=read(record+0x18);if(volume==0xffffffff)volume=read(record+0x1c);
    const auto gains=compute_voice_gain(si(pan),si(volume),si(cells_.master_4a2716),int(read(record+0x20)));
    write(record+0x24,std::uint32_t(gains.right));write(record+0x28,std::uint32_t(gains.left));
    if(rate!=0xffffffff)write(record+8,frequency(rate,read(record+0x14)));
}
std::uint32_t MissionAudioPlayback::update(std::uint32_t handle,std::uint32_t pan,std::uint32_t volume,std::uint32_t rate){
    if(!cells_.enabled_d08940)return 0;
    for(unsigned i=0;i<16;++i){const auto record=live+i*0x34;if(read(record+0x10)!=handle)continue;
        write(record+0x32,1,1);change(record,pan,volume,rate);
        for(unsigned j=0;j<16;++j){const auto f=fallback+j*0x30;if(read(f+0x10)==handle){change(f,pan,volume,rate);return handle;}}return handle;
    }
    for(unsigned i=0;i<16;++i)if(read(pending+i*0x34+0x10)==handle)return handle;return 0;
}
std::uint32_t MissionAudioPlayback::playing(std::uint32_t handle) const{
    if(!cells_.enabled_d08940)return 0;
    for(unsigned i=0;i<16;++i)if(read(live+i*0x34+0x10)==handle)return handle;
    for(unsigned i=0;i<16;++i){const auto record=pending+i*0x34;if(read(record+0x30,1)&&read(record+0x10)==handle)return handle;}return 0;
}
void MissionAudioPlayback::silence(std::uint32_t record){
    write(record+0x30,0,1);write(record+4,0x400);write(record+12,0xfff00400);write(record,silent);write(record+8,0x400);
    write(record+0x24,0);write(record+0x28,0);write(record+0x2c,0);write(record+0x30,1,1);
}
void MissionAudioPlayback::stop(std::uint32_t handle){
    if(!cells_.enabled_d08940)return;
    for(unsigned i=0;i<16;++i)if(read(live+i*0x34+0x10)==handle)silence(pending+i*0x34);
    for(unsigned i=0;i<16;++i)if(read(fallback+i*0x30+0x10)==handle){silence(pending+i*0x34);silence(ambient+i*0x34);write(fallback+i*0x30+12,0xffffffff);write(live+i*0x34+12,0xffffffff);}
}
void MissionAudioPlayback::register_completion(std::uint32_t address){if(cells_.enabled_d08940)cells_.completion_4a2ae8=address;}
void MissionAudioPlayback::compile(std::size_t slot,bool replace_end){
    auto& out=cells_.compiled.at(slot);const auto record=live+std::uint32_t(slot)*0x34;
    out.frequency=read(record+8);if(replace_end)out.end=read(record);
    const auto load=[&](std::uint32_t gain,std::array<std::uint8_t,8>& code,std::uint32_t& shift_code){
        auto kernel=gain_kernel(si(gain));std::uint32_t shift_word=0;
        if(kernel.in_window){
            constexpr std::array<std::uint32_t,7> words{0x03e0c100,0x02e0c100,0x01e0c100,0x00e0c100,0x01f8c100,0x02f8c100,0x03f8c100};
            if(kernel.shift>=words.size())throw std::runtime_error("Audio kernel shift exceeds embedded words");shift_word=words[kernel.shift];
        }else{
            const auto& byte=need(bindings_.static_byte,"Audio kernel table read is not bound");
            kernel.shift=byte(0x4a29e0+gain);const auto scaled=si(gain<<(kernel.shift&31))>>3;
            for(unsigned i=0;i<8;++i)kernel.bytes[i]=byte(0x4b5940+std::uint32_t(scaled)*8+i);
            for(unsigned i=0;i<4;++i)shift_word|=std::uint32_t(byte(0x4a28a0+std::uint32_t(kernel.shift)*4+i))<<(8*i);
        }
        shift_code=(shift_word>>8)|(shift_code&0xff000000);code=kernel.bytes;
    };
    load(read(record+0x24),out.right_kernel,out.right_shift_code);
    load(read(record+0x28),out.left_kernel,out.left_shift_code);
}
void MissionAudioPlayback::publish(){
    cells_.frames_4a35b0=16;
    for(unsigned i=0;i<16;++i){const auto record=live+i*0x34;if(read(record+0x32,1)){compile(i,false);write(record+0x32,0,1);}--cells_.frames_4a35b0;}
    cells_.frames_4a35b0=16;
    for(unsigned i=0;i<16;++i){const auto a=live+i*0x34,b=pending+i*0x34;
        if(read(b+0x30,1)){
            for(unsigned offset=0;offset<0x30;offset+=4)write(a+offset,read(b+offset));
            write(a+0x30,1,1);write(a+0x32,0,1);write(b+0x30,0,1);compile(i,true);
        }--cells_.frames_4a35b0;
    }
    for(unsigned i=0;i<16;++i){const auto a=fallback+i*0x30,b=ambient+i*0x34;
        if(read(b+0x30,1)){for(unsigned offset=0;offset<0x30;offset+=4)write(a+offset,read(b+offset));write(b+0x30,0,1);}
    }
}
std::uint8_t MissionAudioPlayback::pcm(std::uint32_t address) const{
    if(address>=0x4a12d1&&address<=silent)return 0x80;
    return need(bindings_.pcm_byte,"Audio PCM byte read is not bound")(address);
}
void MissionAudioPlayback::advance_frames(std::uint32_t frames){
    publish();if(cells_.disabled_4a2712)return;cells_.frames_4a35b0=frames;
    do{
        const auto old=read(cursor+12),sum=old+read(cursor+8);
        if(sum<old){
            const auto index=read(0x4a2702);channel(index);const auto record=stream+index*0x34;
            if(read(record+0x30,1)){
                for(int offset=0x2c;offset>=0;offset-=4)write(cursor+unsigned(offset),read(record+unsigned(offset)));
                write(record+0x30,0,1);write(0x4a2702,(read(0x4a2702)+1)&15);
            }else{write(cursor+4,0x400);write(cursor+12,0xfff00400);write(cursor,silent);write(cursor+8,0x1000);}
        }else write(cursor+12,sum);
        const auto address=read(cursor)+(std::uint32_t(si(read(cursor+12))>>10)&0xfffffffeu);(void)pcm(address);(void)pcm(address+1);
        for(unsigned i=0;i<16;++i){const auto record=live+i*0x34;const auto position=read(record+12);const auto next=position+cells_.compiled[i].frequency;
            if(next<position){
                const auto callback=read(record+0x2c);
                if(callback&&cells_.completion_4a2ae8!=0x4a2aec)need(bindings_.completed,"Audio completion callback is not bound")(cells_.completion_4a2ae8,callback,read(record)-read(record+4)-16);
                for(int offset=0x2c;offset>=0;offset-=4)write(record+unsigned(offset),read(fallback+i*0x30+unsigned(offset)));
                write(record+0x30,0,1);compile(i,true);
            }else write(record+12,next);
            (void)pcm(cells_.compiled[i].end+std::uint32_t(si(read(record+12))>>10));
        }
        --cells_.frames_4a35b0;
    }while(si(cells_.frames_4a35b0)>0);
}
} // namespace dfre
