#include "dfre/mission_sound_runtime.hpp"
#include "dfre/mission_audio_cache.hpp"
#include "dfre/mission_audio_playback.hpp"
#include "dfre/mission_sound_definitions.hpp"
#include "dfre/mission_sound_registration.hpp"
#include "dfre/mission_positional_sound.hpp"
#include "dfre/mission_sound_stop.hpp"
#include <algorithm>
#include <cstring>
#include <map>
#include <limits>
namespace dfre {
namespace {
using U=std::uint32_t;
void check(bool ok,const char* message){if(!ok)throw MissionSoundAssetError(message);}
U word(const AssetBytes& bytes,std::size_t at,unsigned width=4){U n=0;for(unsigned i=0;i<width;++i)n|=U(bytes.at(at+i))<<(8*i);return n;}
U rotate(U n,unsigned bits){return(n>>bits)|(n<<(32-bits));}
bool exact_executable(const AssetBytes& bytes){
    static constexpr U k[]={0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
    std::array<U,8> h{0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    check(bytes.size()<0x2000000,"Sound executable exceeds supported image limit");
    AssetBytes padded=bytes;padded.push_back(0x80);while(padded.size()%64!=56)padded.push_back(0);
    const auto bits=std::uint64_t(bytes.size())*8;for(unsigned i=8;i;--i)padded.push_back(std::uint8_t(bits>>((i-1)*8)));
    for(std::size_t at=0;at<padded.size();at+=64){
        std::array<U,64>w{};for(unsigned i=0;i<16;++i)for(unsigned j=0;j<4;++j)w[i]=(w[i]<<8)|padded[at+i*4+j];
        for(unsigned i=16;i<64;++i){auto a=w[i-15],b=w[i-2];w[i]=w[i-16]+(rotate(a,7)^rotate(a,18)^(a>>3))+w[i-7]+(rotate(b,17)^rotate(b,19)^(b>>10));}
        auto a=h[0],b=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],v=h[7];
        for(unsigned i=0;i<64;++i){const U t=v+(rotate(e,6)^rotate(e,11)^rotate(e,25))+((e&f)^(~e&g))+k[i]+w[i];const U u=(rotate(a,2)^rotate(a,13)^rotate(a,22))+((a&b)^(a&c)^(b&c));v=g;g=f;f=e;e=d+t;d=c;c=b;b=a;a=t+u;}
        h[0]+=a;h[1]+=b;h[2]+=c;h[3]+=d;h[4]+=e;h[5]+=f;h[6]+=g;h[7]+=v;
    }
    return h==std::array<U,8>{0xabbdf55a,0x1d189b8e,0xb6c1c464,0xda7bdd08,0xec01f6b0,0xccd72182,0xab43bb01,0xb7f651ff};
}
class Memory {
public:
    explicit Memory(const dfre::AssetBytes& raw){
        check(exact_executable(raw),"Sound tables need the supported exact executable hash");
        check(word(raw,0,2)==0x5a4d,"Original executable needs its DOS header");
        const auto pe=word(raw,0x3c);check(word(raw,pe)==0x4550&&word(raw,pe+4,2)==0x14c,"Original executable needs PE x86");
        const auto opt=pe+24;check(word(raw,opt,2)==0x10b&&word(raw,opt+28)==0x400000,"Original executable table layout differs");
        const auto size=word(raw,opt+56);check(size>=0x910000&&size<0x2000000,"Original executable image size differs");
        auto& image=blocks_[0x400000];image.resize(size);
        const auto table=opt+word(raw,pe+20,2),count=word(raw,pe+6,2);
        for(U i=0;i<count;++i){const auto at=table+i*40,offset=word(raw,at+20),length=word(raw,at+16),rva=word(raw,at+12);
            if(word(raw,at+36)&0x80u)continue;
            check(offset<=raw.size()&&length<=raw.size()-offset&&rva<=image.size()&&length<=image.size()-rva,"Original section is outside its image");
            std::copy_n(raw.begin()+offset,length,image.begin()+rva);
        }
        blocks_[0x21000000].resize(0x10000);
    }
    void bind(U at,std::uint8_t* bytes,std::size_t size){views_.push_back({at,bytes,size});}
    std::uint8_t* pointer(U at,std::size_t size){
        for(const auto& view:views_)if(at>=view.at&&std::uint64_t(at-view.at)+size<=view.size)return view.bytes+(at-view.at);
        auto it=blocks_.upper_bound(at);check(it!=blocks_.begin(),"Sound address has no storage");--it;
        check(std::uint64_t(at-it->first)+size<=it->second.size(),"Sound access exceeds its storage");return it->second.data()+(at-it->first);
    }
    U read(U at,U width=4){check(width==1||width==2||width==4,"Invalid sound read width");U n=0;for(U i=0;i<width;++i)n|=U(*pointer(at+i,1))<<(8*i);return n;}
    void write(U at,U width,U n){check(width==1||width==2||width==4,"Invalid sound write width");for(U i=0;i<width;++i)*pointer(at+i,1)=std::uint8_t(n>>(8*i));}
    U allocate(U size){check(size<0x8000000,"Sound allocation exceeds owner limit");U base=next_;std::uint64_t end=std::uint64_t(base)+size+32;
        // Keep the DF socket scratch, lookup and default work heap separate.
        if(base<0x28000000u&&end>0x23000000u){base=0x28000000u;end=std::uint64_t(base)+size+32;}
        check(end<0xf0000000,"Sound allocation handles are full");next_=U((end+15)&~std::uint64_t(15));blocks_[base].resize(std::size_t(size)+16);++allocations;return base+16;}
    void release(U pointer){const auto found=blocks_.find(pointer-16);check(found!=blocks_.end()&&pointer>=0x22000010,"Sound release does not name a live allocation");blocks_.erase(found);}
    std::size_t remaining(U at){auto it=blocks_.upper_bound(at);check(it!=blocks_.begin(),"Sample allocation is missing");--it;check(at-it->first<=it->second.size(),"Sample address exceeds allocation");return it->second.size()-(at-it->first);}
    std::string text(U at){std::string s;for(U i=0;i<4096;++i){const auto c=read(at+i,1);if(!c)return s;s+=char(c);}throw std::runtime_error("Sound name has no end byte");}
    void text(U at,const std::string& s){for(std::size_t i=0;i<=s.size();++i)write(at+U(i),1,i<s.size()?std::uint8_t(s[i]):0);}
    std::size_t allocations=0;
private:
    struct View {U at;std::uint8_t* bytes;std::size_t size;};
    std::map<U,dfre::AssetBytes> blocks_;std::vector<View> views_;U next_=0x22000000;
};
constexpr U archive=0x21001000,height_base=0x15000000;
std::int32_t si(U n){return n<=0x7fffffffu?std::int32_t(n):std::int32_t(std::int64_t(n)-0x100000000LL);}
char upper(char c){return c>='a'&&c<='z'?char(c-'a'+'A'):c;}
bool foreign_record(U at,U width){const auto end=std::uint64_t(at)+width;return(at<0x8cf680&&end>0x7cca80)||(at<0xa15898&&end>0x9f3098);}
}
struct MissionSoundRuntime::Impl {
    const AssetStore& store;MissionAudioFrameCells& frame_cells;const TerrainGeometry*& terrain;MissionSoundRuntimeBindings host;
    Memory memory;U enabled=1;MissionAudioPlaybackCells voices{enabled};MissionAudioPlaybackBindings voice_bindings;
    MissionAudioPlayback playback;MissionSoundAssetBindings assets;MissionAudioCacheBindings cache;MissionSoundBankBindings banks;
    AssetBytes file;std::size_t position=0;U depth=0;
    struct Stack {
        Impl& owner;U at;
        explicit Stack(Impl& value):owner(value),at(0x21002000+value.depth*512){check(value.depth<96,"Sound request nesting exceeds scratch storage");++owner.depth;}
        ~Stack(){--owner.depth;}
    };
    Impl(const AssetStore& s,const AssetBytes& executable,MissionAudioFrameCells& f,const TerrainGeometry*& t,MissionSoundRuntimeBindings h)
        :store(s),frame_cells(f),terrain(t),host(std::move(h)),memory(executable),playback(voices,voice_bindings){
        memory.bind(0xd08940,reinterpret_cast<std::uint8_t*>(&enabled),4);
        memory.bind(mission_audio_voice_base,voices.records_4a16d2.data(),voices.records_4a16d2.size());
        memory.bind(0x4a2ae0,reinterpret_cast<std::uint8_t*>(&voices.generation_4a2ae0),4);
        memory.bind(0x4a2716,reinterpret_cast<std::uint8_t*>(&voices.master_4a2716),4);
        memory.bind(0x6e06a8,reinterpret_cast<std::uint8_t*>(&f.listener_6e06a8),12);
        memory.bind(0x6e06b4,reinterpret_cast<std::uint8_t*>(&f.yaw_6e06b4),4);
        memory.bind(0x4f7ea8,reinterpret_cast<std::uint8_t*>(&f.counter_4f7ea8),4);
        memory.bind(0x6e0700,reinterpret_cast<std::uint8_t*>(f.registrations_6e0700.data()),sizeof(f.registrations_6e0700));
        memory.bind(0x6e0270,reinterpret_cast<std::uint8_t*>(f.candidates_6e0270.data()),sizeof(f.candidates_6e0270));
        memory.bind(0x6e0470,reinterpret_cast<std::uint8_t*>(f.selected_6e0470.data()),sizeof(f.selected_6e0470));
        memory.bind(0x6e06d0,reinterpret_cast<std::uint8_t*>(f.identities_6e06d0.data()),sizeof(f.identities_6e06d0));
        memory.bind(0x6e06e8,reinterpret_cast<std::uint8_t*>(f.handles_6e06e8.data()),sizeof(f.handles_6e06e8));
        voice_bindings.sample=[this](U at){return MissionAudioSampleView{memory.pointer(at,16),memory.remaining(at),at};};
        voice_bindings.pcm_byte=[this](U at){return std::uint8_t(read(at,1));};voice_bindings.static_byte=voice_bindings.pcm_byte;
        voice_bindings.completed=[this](U at,U token,U sample){check(bool(host.completed),"Sound completion host is not bound");host.completed(at,token,sample);};
        playback.initialize();
        assets.read=cache.read=banks.read=[this](U at,U width){return read(at,width);};
        assets.write=cache.write=banks.write=[this](U at,U width,U n){write(at,width,n);};
        assets.call=cache.call=banks.call=[this](U site,const std::vector<U>& args){return call(site,args);};
        write(0xd096e4,4,0);write(0xd096d8,4,archive);write(0xd096dc,4,0);write(0xd0973c,4,0);
    }
    U read(U at,U width=4){
        check(width==1||width==2||width==4,"Invalid sound read width");
        if(host.record_base){const auto base=host.record_base(at,width);if(base){check(*base<=at&&bool(host.record_read),"Sound current record reader is not bound");return host.record_read(*base,at-*base,width);}}
        check(!foreign_record(at,width),"Sound current actor or projectile access is not bound");
        if(at==0x503228&&width==4){check(bool(host.script_key),"Sound current script key is not bound");return host.script_key();}
        if(at==0xbd5284&&width==4){check(terrain!=nullptr,"Sound terrain is not bound");return height_base;}
        if(at>=height_base&&std::uint64_t(at-height_base)+width<=1024*1024){
            check(terrain&&terrain->height_bytes().size()==1024*1024,"Sound terrain needs its current1024x1024 height grid");
            U n=0;for(U i=0;i<width;++i)n|=U(terrain->height_bytes().at(at-height_base+i))<<(i*8);return n;
        }
        return memory.read(at,width);
    }
    void write(U at,U width,U value){check(width==1||width==2||width==4,"Invalid sound write width");if(host.record_base){const auto base=host.record_base(at,width);if(base){check(*base<=at&&bool(host.record_store),"Sound current record writer is not bound");host.record_store(*base,at-*base,width,value);return;}}check(!foreign_record(at,width),"Sound current actor or projectile access is not bound");memory.write(at,width,value);}
    U text(std::string_view value){check(value.size()<4096,"Sound name exceeds its source boundary");const auto at=memory.allocate(U(value.size()+1));memory.text(at,std::string(value));return at;}
    bool exists(U at){try{store.read(memory.text(at));return true;}catch(const AssetError&){return false;}}
    U foreign(U site,const std::vector<U>& args){check(bool(host.service),"Sound foreign service is not bound");return host.service(site,args);}
    U call(U site,const std::vector<U>& a){
        switch(site){
        case 0x49a620:for(U i=0;i<a.at(2);++i)write(a.at(0)+i,1,a.at(1)&255u);return a.at(0);
        case 0x497f60:return memory.allocate(a.at(0));
        case 0x4980d0:if(!a.at(0))return 0xffffffffu;memory.release(a.at(0));return 0;
        case 0x499040:return exists(a.at(0));
        case 0x499120:return mission_sound_file_open(assets,a.at(0));
        case 0x4992d0:return mission_audio_cache_seek(cache,a.at(0),a.at(1));
        case 0x499280:return mission_audio_cache_read(cache,a.at(0),a.at(1));
        case 0x499020:return mission_audio_cache_close(cache);
        case 0x49d050:mission_audio_cache_load(cache,a.at(0),a.at(1));return 0;
        case 0x494c30:{const auto sample=load_file(a.at(0));return sample?mission_audio_cache_convert(cache,sample):0;}
        case 0x494c50:return mission_audio_cache_convert(cache,a.at(0));
        case 0x499410:return load_file(a.at(0));
        case 0x498230:return mission_audio_cache_tag(cache,a.at(0),a.at(1));
        case 0x495160:playback.register_completion(a.at(0));return 0;
        case 0x494dd0:return playback.play(a.at(0),a.at(1),a.at(2),a.at(3));
        case 0x494e80:return playback.play_channel(a.at(0),a.at(1),a.at(2),a.at(3),a.at(4));
        case 0x494f40:return playback.start_ambient(a.at(0),a.at(1),a.at(2),a.at(3),a.at(4));
        case 0x494fa0:return playback.update(a.at(0),a.at(1),a.at(2),a.at(3));
        case 0x494ff0:return playback.playing(a.at(0));
        case 0x495070:playback.stop(a.at(0));return 0;
        case 0x4950a0:if(read(0xd08940)){check(a.at(0)<16,"Sound reserved channel exceeds16 voices");write(0x4a1703+a.at(0)*0x34,1,a.at(1));}return 0;
        case 0x456560:extension(a.at(0),a.at(1));return 0;
        case 0x49cf50:{Stack stack(*this);return mission_sound_context_load(assets,a.at(0),a.at(1),0xffffffffu,stack.at);}
        case 0x49d510:return mission_sound_find_bank(assets,a.at(0),a.at(1));
        case 0x43e3e0:return find_ambient(a.at(0));
        case 0x43edb0:return load_ambient(a.at(0));
        case 0x43ee80:return lazy_ambient(a.at(0));
        case 0x43f490:return definition_id(a.at(0));
        case 0x43ebf0:return ambient_line(a.at(0),false);
        case 0x43ec40:return ambient_line(a.at(0),true);
        case 0x456720:return parse(a.at(0),a.at(1),a.at(2));
        case 0x4565f0:return tokenize(a.at(0),a.at(1));
        case 0x49e630:{check(a.at(1)==archive,"Sound open archive handle differs");try{file=store.read(memory.text(a.at(0))).bytes;}catch(const AssetError&){write(archive+0xac,4,1);return 0xffffffffu;}position=0;write(archive+0xac,4,0);return 0;}
        case 0x49e6e0:{check(a.at(2)==archive,"Sound seek archive handle differs");check(a.at(1)<=2,"Sound seek origin is invalid");const auto origin=a.at(1)==0?0:a.at(1)==1?std::int64_t(position):std::int64_t(file.size());const auto next=origin+si(a.at(0));check(next>=0&&std::uint64_t(next)<=file.size(),"Sound seek exceeds supplied file bytes");position=std::size_t(next);return U(position);}
        case 0x49e6b0:{check(a.at(2)==archive,"Sound read archive handle differs");const auto count=std::min<std::size_t>(a.at(1),file.size()-position);std::copy_n(file.begin()+position,count,memory.pointer(a.at(0),count));position+=count;return U(count);}
        case 0x49e750:{check(a.at(1)==archive,"Sound load archive handle differs");AssetBytes bytes;try{bytes=store.read(memory.text(a.at(0))).bytes;}catch(const AssetError& e){if(std::string(e.what()).find("Missing asset: ")!=0)throw;write(archive+0xac,4,1);return 0;}const auto out=memory.allocate(U(bytes.size()));std::copy(bytes.begin(),bytes.end(),memory.pointer(out,bytes.size()));write(archive+0xac,4,0);return out;}
        default:return foreign(site,a);
        }
    }
    bool equal(U first,U second){for(U i=0;i<4096;++i){const auto a=char(read(first+i,1)),b=char(read(second+i,1));if(upper(a)!=upper(b))return false;if(!a)return true;}throw MissionSoundAssetError("Sound comparison has no end byte");}
    void extension(U name,U suffix){const auto size=U(memory.text(name).size());U index=0;while(index<size&&read(name+index,1)!='.')++index;if(index==size)write(name+index,1,'.');const auto out=name+index+1;U i=0;for(;i<memory.text(suffix).size();++i){check(i<4096,"Sound extension alias has no bounded end");const auto value=read(suffix+i,1);write(out+i,1,value);}write(out+i,1,0);}
    U number(U at){const auto s=memory.text(at);std::size_t i=0;while(i<s.size()&&(s[i]==' '||s[i]=='\t'||s[i]=='\r'||s[i]=='\n'||s[i]=='\v'||s[i]=='\f'))++i;bool negative=false;if(i<s.size()&&(s[i]=='+'||s[i]=='-'))negative=s[i++]=='-';U n=0;while(i<s.size()&&s[i]>='0'&&s[i]<='9')n=n*10+U(s[i++]-'0');return negative?0u-n:n;}
    U find_ambient(U name){U row=read(0x6e06c4),i=0;while(si(i)<si(read(0x6e06c8))){if(equal(name,row))return row;row+=40;++i;}return 0;}
    U definition_id(U name){if(read(name,1))for(U index=1;index<512;++index)if(equal(name,0x4f2edau+(index-1)*40))return index;return 0;}
    U ambient_line(U record,bool fill){
        if(!record)return 0;
        if(equal(read(record+4),0x4cc0a0)){write(0x6e06cc,4,number(read(record+8)));return 0;}
        if(si(read(record))<1)return 0;
        const auto count=read(0x6e06c8);if(!fill){write(0x6e06c8,4,count+1);return 0;}
        const auto row=read(0x6e06c4)+count*40;write(0x6e06c8,4,count+1);
        const auto copy=[this](U to,U from){for(U i=0;i<4096;++i){const auto n=read(from+i,1);write(to+i,1,n);if(!n)return;}throw MissionSoundAssetError("Ambient name has no end byte");};
        copy(row,read(record+4));copy(row+12,read(record+8));
        write(row+0x24,1,number(read(record+12)));write(row+0x25,1,number(read(record+16)));
        write(row+0x1c,4,number(read(record+20))<<16);write(row+0x26,1,number(read(record+24)));write(row+0x27,1,number(read(record+28)));
        for(U i=7;si(i)<si(read(record));++i){const auto token=read(record+(i+1)*4);if(equal(token,0x4cc0a8))write(row+0x20,1,read(row+0x20,1)|1);else if(equal(token,0x4cc0b0))write(row+0x20,1,read(row+0x20,1)|2);}
        return 0;
    }
    U tokenize(U source,U out){
        write(0x8e442c,4,0);const auto line=memory.text(source);const auto length=std::min<std::size_t>(line.size(),1000);
        if(length){for(U i=0;i<length;++i)write(0x8e4040+i,1,std::uint8_t(line[i]));write(0x8e4040+U(length),1,0);bool quoted=false,start=true;
            for(U i=0;i<=length;++i){const auto at=0x8e4040+i,c=read(at,1);if(!quoted&&(c==' '||c==','||c=='\t')){write(at,1,0);start=true;}else if(c=='"'&&!quoted){quoted=true;write(at,1,0);start=true;}else if(c=='"'){quoted=false;write(at,1,0);start=true;}else{if(start){const auto n=read(0x8e442c);write(0x8e442c,4,n+1);write(0x8e4430+n*4,4,at);}start=false;}if(si(read(0x8e442c))>=30)break;}
        }
        for(U i=0;i<31;++i)write(out+i*4,4,read(0x8e442c+i*4));return out;
    }
    U parse(U name,U callback,U key){
        if(!exists(name))return 1;
        auto data=store.read(memory.text(name)).bytes;check(!data.empty(),"Original empty script parser has no bounded line");
        const auto base=memory.allocate(U(data.size())+2);std::copy(data.begin(),data.end(),memory.pointer(base,data.size()));
        if(key){std::reverse(data.begin(),data.end());for(auto& n:data){key+=(key<<11)|(key>>21);key=((key<<4)|(key>>28))^1u;n^=std::uint8_t(key);}std::copy(data.begin(),data.end(),memory.pointer(base,data.size()));}
        Stack stack(*this);U cursor=base,line=1,result=0;const auto end=base+U(data.size());
        while(!result){if(cursor>=end){result=call(callback,{0,line});break;}U stop=cursor;while(stop<end&&!(read(stop,1)=='\r'&&read(stop+1,1)=='\n'))++stop;if(stop>=end)--stop;write(stop,1,0);tokenize(cursor,stack.at);cursor=stop+2;if(read(stack.at)&&read(read(stack.at+4),1)!='/')result=call(callback,{stack.at,line});++line;}
        memory.release(base);return result;
    }
    U load_ambient(U name){
        write(0x6e06c8,4,0);parse(name,0x43ebf0,read(0x503228));const auto allocated=memory.allocate(read(0x6e06c8)*40);write(0x6e06c4,4,allocated);
        if(!allocated){foreign(0x4564b0,{0x4cc0cc});return 0;}
        const auto size=read(0x6e06c8)*40;std::fill_n(memory.pointer(allocated,size),size,0);write(0x6e06c8,4,0);parse(name,0x43ec40,read(0x503228));return 1;
    }
    U lazy_ambient(U id){
        check(id<512,"Ambient sound id exceeds512 definitions");Stack stack(*this);const auto at=0x4f2ea8+id*40;
        const auto name=memory.text(read(at)+12);check(name.size()<32,"Ambient filename exceeds its source stack buffer");memory.text(stack.at,name);
        U index=0,offset=0;if(si(read(0x6e06c8))>0){do{
            if(equal(stack.at,read(0x6e06c4)+offset+12)&&read(read(0x6e06c4)+offset+0x18)){
                const auto first=read(at);write(first+0x20,1,read(first+0x20,1)&0xfbu);
                const auto row=read(0x6e06c4)+offset;const auto target=read(at);const auto sample=read(row+0x18);write(target+0x18,4,sample);
                return read(read(0x6e06c4)+offset+0x18);
            }
            ++index;offset+=40;
        }while(si(index)<si(read(0x6e06c8)));}
        call(0x456560,{stack.at,0x4cc0e8});const auto sample=call(0x494c30,{stack.at});write(read(at)+0x18,4,sample);
        const auto current=read(at),loaded=read(current+0x18);if(!loaded)return 0;write(current+0x20,1,read(current+0x20,1)|4);return read(read(at)+0x18);
    }
    U load_file(U name){
        std::optional<U> result;
        if(read(0xd096e4)||(!read(0xd096d8)&&!read(0xd0973c))){result=call(0x498da0,{name});if(!read(0xd096e8))return *result;}
        if(const auto context=read(0xd096d8)){U selected=name;if(read(0xd096dc))selected=call(0x498fb0,{name,context});result=call(0x49e750,{selected,context});if(!read(read(0xd096d8)+0xac))return *result;}
        if(read(0xd0973c))for(U i=0;i<16;++i){const auto slot=0xd096f0+i*4,context=read(slot);if(!context)continue;U selected=name;if(read(0xd096dc))selected=call(0x498fb0,{name,context});result=call(0x49e750,{selected,context});if(!read(read(slot)+0xac))return *result;}
        call(0x499750,{name,0x4ce314});check(result.has_value(),"Sound file load failure needs its incoming EDI value");return *result;
    }
    void resolve(U primary,U alternate){
        write(0x6e06bc,4,call(0x49cf50,{primary,0x6e0518})!=0);
        U loaded=0;if(call(0x499040,{alternate}))loaded=call(0x49cf50,{alternate,0x6e05e0})!=0;write(0x6e06c0,4,loaded);
        for(U row=0x4f2ecc;row!=0x4f7ea4;row+=40){if(!(read(row+36,1)&1))continue;write(row+36,1,read(row+36,1)&0xfbu);if(read(0x6e06c0)){const auto bank=call(0x49d510,{row+14,0x6e05e0});write(row+4,4,bank);if(bank){write(row+36,1,read(row+36,1)|4);continue;}}const auto bank=call(0x49d510,{row+14,0x6e0518});write(row+4,4,bank);}
        for(U row=0x4f2ecc;row!=0x4f7ea4;row+=40)if(read(row+36,1)&2){const auto ambient=call(0x43e3e0,{row+14});write(row+4,4,ambient);}
        for(U channel=10;channel!=16;++channel)call(0x4950a0,{channel,1});
        Stack stack(*this);const auto path=memory.text(0x8cf96c);check(path.size()<80,"Sound mission script path exceeds its source stack buffer");memory.text(stack.at,path);
        call(0x456560,{stack.at,0x4cc09c});if(!call(0x499040,{stack.at}))return;
        call(0x455c90,{stack.at});call(0x456070,{0x3e});call(0x456080,{0x43e430});call(0x456090,{0x43e470});
    }
    void frame(){
        MissionAudioFrameBindings b;
        b.actor_word=[this](U actor,U offset){if(host.actor_word)return host.actor_word(actor,offset);check(bool(host.record_base)&&bool(host.record_read),"Sound actor source is not bound");const auto base=host.record_base(actor+offset,4);check(base&&*base==actor,"Sound actor record is not claimed");return host.record_read(*base,offset,4);};
        b.definition_word=[this](U definition,U offset){return read(definition+offset);};
        b.update=[this](U handle,U pan,U volume,U rate){playback.update(handle,pan,volume,rate);};
        b.stop=[this](U handle){playback.stop(handle);};
        b.start=[this](U sample,U pan,U volume,U rate,U priority){return playback.start_ambient(sample,pan,volume,rate,priority);};
        mission_audio_frame(frame_cells,b);
    }
};
MissionSoundRuntime::MissionSoundRuntime(const AssetStore& s,const AssetBytes& e,MissionAudioFrameCells& f,const TerrainGeometry*& t,MissionSoundRuntimeBindings h):impl_(std::make_unique<Impl>(s,e,f,t,std::move(h))){}
MissionSoundRuntime::~MissionSoundRuntime()=default;
U MissionSoundRuntime::load_context(std::string_view name,U context,U tag){Impl::Stack stack(*impl_);return mission_sound_context_load(impl_->assets,impl_->text(name),context,tag,stack.at);}
void MissionSoundRuntime::initialize_definitions(std::string_view name){mission_sound_definitions_initialize(impl_->assets,impl_->text(name));}
void MissionSoundRuntime::resolve_definitions(std::string_view primary,std::string_view alternate){impl_->resolve(impl_->text(primary),impl_->text(alternate));}
void MissionSoundRuntime::global(U id){Impl::Stack stack(*impl_);mission_sound_global_request(impl_->banks,id,stack.at);}
void MissionSoundRuntime::positional(U id,Fixed3 p){Impl::Stack stack(*impl_);mission_sound_positional_request(impl_->banks,id,U(p.x),U(p.y),U(p.z),stack.at);}
void MissionSoundRuntime::attach(U actor,U id,U scale,U ceiling){mission_sound_attach(impl_->banks,actor,id,scale,ceiling);}
void MissionSoundRuntime::stop(U actor,U scale){mission_sound_stop(impl_->banks,actor,scale);}
U MissionSoundRuntime::loudness(U id){return mission_sound_loudness(impl_->banks,id);}
U MissionSoundRuntime::definition_id(std::string_view name){if(name.empty()||!name.front())return 0;return impl_->definition_id(impl_->text(name));}
void MissionSoundRuntime::frame(){impl_->frame();}
void MissionSoundRuntime::publish(){impl_->playback.publish();}
void MissionSoundRuntime::advance_frames(U frames){impl_->playback.advance_frames(frames);}
const MissionSoundAssetBindings& MissionSoundRuntime::asset_bindings()const{return impl_->assets;}
const MissionSoundBankBindings& MissionSoundRuntime::bank_bindings()const{return impl_->banks;}
}
