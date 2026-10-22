#include "dfre/assets.hpp"
#include "dfre/mission_audio_cache.hpp"
#include "dfre/mission_audio_playback.hpp"
#include "dfre/mission_sound_assets.hpp"
#include "dfre/mission_sound_bank.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <stdexcept>

namespace {
using U=std::uint32_t;
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
U word(const dfre::AssetBytes& bytes,std::size_t at,unsigned width=4){U n=0;for(unsigned i=0;i<width;++i)n|=U(bytes.at(at+i))<<(8*i);return n;}
class Memory {
public:
    explicit Memory(const std::filesystem::path& game){
        std::ifstream f(game/"Df.exe",std::ios::binary);check(bool(f),"Cannot open original executable tables");
        dfre::AssetBytes raw((std::istreambuf_iterator<char>(f)),{});
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
    U read(U at,U width=4){check(width==1||width==2||width==4,"Invalid sound read width");const auto* p=pointer(at,width);U n=0;for(U i=0;i<width;++i)n|=U(p[i])<<(8*i);return n;}
    void write(U at,U width,U n){check(width==1||width==2||width==4,"Invalid sound write width");auto* p=pointer(at,width);for(U i=0;i<width;++i)p[i]=std::uint8_t(n>>(8*i));}
    U allocate(U size){check(size<0x8000000,"Sound allocation exceeds probe limit");const U base=next_;const std::uint64_t end=std::uint64_t(base)+size+32;
        check(end<0xf0000000,"Sound allocation handles are full");next_=U((end+15)&~std::uint64_t(15));blocks_[base].resize(std::size_t(size)+16);++allocations;return base+16;}
    std::size_t remaining(U at){auto it=blocks_.upper_bound(at);check(it!=blocks_.begin(),"Sample allocation is missing");--it;check(at-it->first<=it->second.size(),"Sample address exceeds allocation");return it->second.size()-(at-it->first);}
    std::string text(U at){std::string s;for(U i=0;i<4096;++i){const auto c=read(at+i,1);if(!c)return s;s+=char(c);}throw std::runtime_error("Sound name has no end byte");}
    void text(U at,const std::string& s){for(std::size_t i=0;i<=s.size();++i)write(at+U(i),1,i<s.size()?std::uint8_t(s[i]):0);}
    std::size_t allocations=0;
private:
    struct View {U at;std::uint8_t* bytes;std::size_t size;};
    std::map<U,dfre::AssetBytes> blocks_;std::vector<View> views_;U next_=0x22000000;
};
}
int main(int argc,char** argv){try{
    using namespace dfre;
    if(argc!=2)throw std::invalid_argument("Use: mission_sound_stock_probe GAME_FOLDER");
    const std::filesystem::path game(argv[1]);AssetStore store(game);Memory memory(game);
    // File/archive and allocator providers use actual supplied file bytes.
    // Actor state, window startup and device output are outside this probe.
    const U archive=0x21001000,scratch=0x21000000,name=0x21000100,params=0x21000200;
    U enabled=1;MissionAudioPlaybackCells voices{enabled};MissionAudioPlaybackBindings voice_bindings;
    memory.bind(0xd08940,reinterpret_cast<std::uint8_t*>(&enabled),4);
    memory.bind(mission_audio_voice_base,voices.records_4a16d2.data(),voices.records_4a16d2.size());
    memory.bind(0x4a2ae0,reinterpret_cast<std::uint8_t*>(&voices.generation_4a2ae0),4);
    memory.bind(0x4a2716,reinterpret_cast<std::uint8_t*>(&voices.master_4a2716),4);
    voice_bindings.sample=[&](U at){return MissionAudioSampleView{memory.pointer(at,16),memory.remaining(at),at};};
    voice_bindings.pcm_byte=[&](U at){return std::uint8_t(memory.read(at,1));};
    voice_bindings.static_byte=voice_bindings.pcm_byte;
    voice_bindings.completed=[](U,U,U){throw std::runtime_error("Stock probe needs a completion message host");};
    MissionAudioPlayback playback(voices,voice_bindings);playback.initialize();
    MissionSoundAssetBindings assets;MissionAudioCacheBindings cache;MissionSoundBankBindings banks;
    const auto read=[&](U at,U width){return memory.read(at,width);};
    const auto write=[&](U at,U width,U n){memory.write(at,width,n);};
    assets.read=cache.read=banks.read=read;assets.write=cache.write=banks.write=write;
    AssetBytes file;std::size_t position=0;unsigned sample_loads=0,starts=0;
    std::vector<U> handles;
    const auto call=[&](U site,const std::vector<U>& args)->U{
        switch(site){
        case 0x497f60:return memory.allocate(args.at(0));
        case 0x499120:return mission_sound_file_open(assets,args.at(0));
        case 0x4992d0:return mission_audio_cache_seek(cache,args.at(0),args.at(1));
        case 0x499280:return mission_audio_cache_read(cache,args.at(0),args.at(1));
        case 0x499020:return mission_audio_cache_close(cache);
        case 0x49d050:mission_audio_cache_load(cache,args.at(0),args.at(1));++sample_loads;return 0;
        case 0x498230:return mission_audio_cache_tag(cache,args.at(0),args.at(1));
        case 0x495160:playback.register_completion(args.at(0));return 0;
        case 0x494dd0:{const auto handle=playback.play(args.at(0),args.at(1),args.at(2),args.at(3));handles.push_back(handle);++starts;return handle;}
        case 0x494e80:{const auto handle=playback.play_channel(args.at(0),args.at(1),args.at(2),args.at(3),args.at(4));handles.push_back(handle);++starts;return handle;}
        case 0x49e630:{check(args.at(1)==archive,"Unexpected sound archive");const auto filename=memory.text(args.at(0));
            file=store.read(filename).bytes;position=0;memory.write(archive+0xac,4,0);return 0;}
        case 0x49e6e0:{check(args.at(2)==archive,"Unexpected seek archive");const auto origin=args.at(1),offset=args.at(0);
            const auto signed_offset=offset<=0x7fffffffu?std::int64_t(offset):std::int64_t(offset)-0x100000000LL;
            const auto next=(origin==0?0:origin==1?std::int64_t(position):std::int64_t(file.size()))+signed_offset;
            check(origin<=2&&next>=0&&std::uint64_t(next)<=file.size(),"Sound file seek exceeds supplied bytes");position=std::size_t(next);return U(position);}
        case 0x49e6b0:{check(args.at(2)==archive,"Unexpected read archive");const auto count=std::min<std::size_t>(args.at(1),file.size()-position);
            std::copy_n(file.begin()+position,count,memory.pointer(args.at(0),count));position+=count;return U(count);}
        default:throw std::runtime_error("Unbound stock sound service: "+std::to_string(site));
        }
    };
    assets.call=cache.call=banks.call=call;
    memory.write(0xd096e4,4,0);memory.write(0xd096d8,4,archive);memory.write(0xd096dc,4,0);memory.write(0xd0973c,4,0);
    memory.text(name,"sniper.pwf");
    check(mission_sound_context_load(assets,name,0x6e0518,0xffffffffu,scratch)==1,"Stock sound bank load failed");
    memory.text(name,"FANFARE");const auto bank=mission_sound_find_bank(assets,name,0x6e0518);check(bank!=0,"Stock result sound bank is missing");
    // Seed sound499 from its actual static definition; full mission startup
    // owns definition sorting, ambient definitions and optional bank fallback.
    U definition=0;for(U i=0;i<512;++i)if(memory.read(0x4f2ea4+i*0x28,2)==499){definition=0x4f2ea4+i*0x28;break;}
    check(definition!=0&&memory.text(definition+14)=="FANFARE","Stock result sound definition differs");
    AssetBytes record(40);std::copy_n(memory.pointer(definition,40),40,record.begin());
    const U indexed=0x4f2ea4+499*0x28;std::copy(record.begin(),record.end(),memory.pointer(indexed,40));memory.write(indexed+4,4,bank);
    mission_sound_global_request(banks,499,params);
    check(starts>0&&!handles.empty()&&handles.front()!=0xffffffffu,"Stock result sound did not create a voice");
    playback.publish();unsigned active=0;for(auto handle:handles)active+=playback.playing(handle)!=0;
    check(active>0,"Published stock sound has no live voice");
    playback.advance_frames(22050*30);unsigned remaining=0;for(auto handle:handles)remaining+=playback.playing(handle)!=0;
    check(remaining==0,"Stock sound voice did not finish");
    std::cout<<"samples "<<memory.read(0x6e0520)<<" banks "<<memory.read(0x6e0540)<<" allocations "<<memory.allocations
             <<" sample_loads "<<sample_loads<<" starts "<<starts<<" remaining "<<remaining<<" device_output 0\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
