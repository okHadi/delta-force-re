#include "dfre/profiles.hpp"
#include "dfre/menu.hpp"
#include "dfre/input_config.hpp"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <optional>
#include <sstream>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace dfre {
namespace {
std::uint32_t word(const AssetBytes& b,std::size_t at) {
    if(at>b.size() || b.size()-at<4)throw AssetError("Profile word outside data");
    return b[at]|(std::uint32_t(b[at+1])<<8)|(std::uint32_t(b[at+2])<<16)|(std::uint32_t(b[at+3])<<24);
}
void put(AssetBytes& b,std::size_t at,std::uint32_t value){for(unsigned i=0;i<4;++i)b.at(at+i)=std::uint8_t(value>>(8*i));}
std::string lower(std::string value){for(auto& c:value)if(c>='A'&&c<='Z')c=char(c+32);return value;}
std::string slot_name(std::size_t slot){std::ostringstream o;o << "player" << std::setw(2) << std::setfill('0') << slot+1 << ".ply";return o.str();}
void field(AssetBytes& data,std::size_t at,std::size_t size,std::string_view text) {
    if(text.size()>=size)throw AssetError("Profile text exceeds field size");
    if(text.find('\0')!=std::string_view::npos)throw AssetError("Profile text contains NUL");
    std::fill_n(data.begin()+at,size,0);std::copy(text.begin(),text.end(),data.begin()+at);
}
AssetBytes read(const std::filesystem::path& path) {
    std::ifstream source(path,std::ios::binary);if(!source)throw AssetError("Cannot read profile source: "+path.string());
    source.seekg(0,std::ios::end);const auto size=source.tellg();
    if(size<0 || size>16*1024*1024)throw AssetError("Profile source exceeds read limit");
    AssetBytes out(static_cast<std::size_t>(size));source.seekg(0);
    source.read(reinterpret_cast<char*>(out.data()),out.size());if(!source)throw AssetError("Truncated profile source");return out;
}
std::optional<std::filesystem::path> existing(const std::filesystem::path& folder,std::string_view name) {
    std::error_code error;if(!std::filesystem::exists(folder,error))return {};
    std::optional<std::filesystem::path> out;
    for(const auto& entry:std::filesystem::directory_iterator(folder))if(lower(entry.path().filename().string())==lower(std::string(name))) {
        if(out)throw AssetError("Duplicate profile filename");out=entry.path();
    }
    return out;
}
void safe_save_folder(const std::filesystem::path& game,const std::filesystem::path& save) {
    if(save.empty())throw AssetError("A separate profile save folder is required");
    const auto source=std::filesystem::weakly_canonical(game),target=std::filesystem::weakly_canonical(save);
    auto a=source.begin(),b=target.begin();while(a!=source.end()&&b!=target.end()&&
#ifdef _WIN32
        CompareStringOrdinal(a->c_str(),-1,b->c_str(),-1,TRUE)==CSTR_EQUAL
#else
        *a==*b
#endif
    ){++a;++b;}
    if(a==source.end())throw AssetError("Profile saves must be outside the original game folder");
}
AssetBytes control_defaults(const AssetBytes& source) {
    ActionBindings bindings{};
    load_action_bindings_text(bindings,std::string_view(
        reinterpret_cast<const char*>(source.data()),source.size()));
    AssetBytes out(action_count*12,0);
    for(std::size_t i=0;i<bindings.size();++i){
        put(out,i*12,bindings[i].key);
        put(out,i*12+4,bindings[i].mouse);
        put(out,i*12+8,bindings[i].joystick);
    }
    return out;
}
}

ProfileDefaults load_profile_defaults(const std::filesystem::path& game_folder) {
    const AssetStore assets(game_folder);const RtxtTable text(assets.read("dfmenu.bin").bytes);
    ProfileDefaults out;out.controls=control_defaults(assets.read("dfdk.key").bytes);out.empty_name=text.get("NSEMPTY");out.first_name=text.get("NSDELETEME");
    if(out.empty_name.empty()||out.first_name.empty())throw AssetError("Missing original profile names");
    for(unsigned i=0;i<10;++i){out.macros[i]=text.get("Macros","DEFAULTMACRO"+std::to_string(i+1));if(out.macros[i].empty())throw AssetError("Missing original profile macro");}
    return out;
}
PlayerProfile::PlayerProfile():raw_(byte_size,0){}
PlayerProfile PlayerProfile::decode(AssetBytes bytes,const ProfileDefaults& defaults) {
    if(bytes.size()!=byte_size || bytes[0]!='P'||bytes[1]!='L'||bytes[2]!='Y')throw AssetError("Expected a 2704-byte PLY profile");
    if(bytes[3]<0x1b){if(defaults.controls.size()!=0x228)throw AssetError("Missing legacy PLY defaults");
        std::copy(defaults.controls.begin()+0x210,defaults.controls.end(),bytes.begin()+0x8b0);}
    bytes[3]=0x1b;PlayerProfile out;out.raw_=std::move(bytes);return out;
}
PlayerProfile PlayerProfile::empty(const ProfileDefaults& defaults,std::string_view filename) {
    if(defaults.controls.size()!=0x228)throw AssetError("Expected 552-byte original control defaults");
    PlayerProfile out;out.set_filename(filename);
    for(unsigned i=0;i<10;++i)field(out.raw_,0x4b0+i*40,40,defaults.macros[i]);
    std::copy(defaults.controls.begin(),defaults.controls.end(),out.raw_.begin()+0x6a0);
    put(out.raw_,0x680,6);put(out.raw_,0x684,9);put(out.raw_,0x688,1);put(out.raw_,0x68c,8);
    return out;
}
std::string PlayerProfile::name() const {const auto end=std::find(raw_.begin()+4,raw_.begin()+20,0);return {raw_.begin()+4,end};}
bool PlayerProfile::active() const noexcept{return bool(raw_[0x24]&1);}
std::uint32_t PlayerProfile::portrait() const{return word(raw_,0x640);}
ProfileStats PlayerProfile::stats() const {
    ProfileStats out;out.campaign_kills=word(raw_,0x664);out.multiplayer_kills=word(raw_,0x648);
    out.multiplayer_deaths=word(raw_,0x650);out.multiplayer_team_games_won=word(raw_,0x65c);
    for(unsigned campaign=0;campaign<5;++campaign)for(unsigned mission=0;mission<16;++mission)
        out.completed_missions+=word(raw_,0x74+campaign*0x44+mission*4)!=0;
    return out;
}
void PlayerProfile::set_name(std::string_view value) {
    if(value.empty()||value.size()>15)throw AssetError("Soldier name must have 1 to 15 characters");
    std::string text(value);for(auto& c:text){if(static_cast<unsigned char>(c)<32||static_cast<unsigned char>(c)>126)throw AssetError("Soldier name requires original ASCII characters");if(c>='a'&&c<='z')c=char(c-32);}
    field(raw_,4,16,text);
}
void PlayerProfile::set_active(bool value) noexcept{raw_[0x24]=std::uint8_t((raw_[0x24]&0xfe)|(value?1:0));}
void PlayerProfile::set_portrait(std::uint32_t index){if(index>=portrait_count)throw AssetError("Portrait index must be below 11");put(raw_,0x640,index);}
void PlayerProfile::set_filename(std::string_view filename){field(raw_,0x14,16,filename);}
AssetBytes PlayerProfile::encoded() const{auto out=raw_;out[0]='P';out[1]='L';out[2]='Y';out[3]=0x1b;return out;}

ProfileStore::ProfileStore(std::filesystem::path game,std::filesystem::path save):ProfileStore(game,save,load_profile_defaults(game)){}
ProfileStore::ProfileStore(std::filesystem::path game,std::filesystem::path save,ProfileDefaults defaults):
    game_folder_(std::move(game)),save_folder_(std::move(save)),defaults_(std::move(defaults)) {
    safe_save_folder(game_folder_,save_folder_);std::size_t loaded=0;
    for(std::size_t slot=0;slot<profiles_.size();++slot) {
        const auto filename=slot_name(slot);auto source=existing(save_folder_,filename);if(!source)source=existing(game_folder_,filename);
        profiles_[slot]=PlayerProfile::empty(defaults_,filename);
        if(source){try{
            profiles_[slot]=PlayerProfile::decode(read(*source),defaults_);++loaded;
            if(!profiles_[slot].active()){profiles_[slot]=PlayerProfile::empty(defaults_,filename);field(profiles_[slot].raw_,4,16,defaults_.empty_name);}
        }catch(const AssetError&){profiles_[slot]=PlayerProfile::empty(defaults_,filename);}}
        profiles_[slot].set_filename(filename);
    }
    if(!loaded){field(profiles_[0].raw_,4,16,defaults_.first_name);profiles_[0].set_active(true);}
}
PlayerProfile& ProfileStore::at(std::size_t slot){return profiles_.at(slot);}
const PlayerProfile& ProfileStore::at(std::size_t slot) const{return profiles_.at(slot);}
std::size_t ProfileStore::first_active() const noexcept{for(std::size_t i=0;i<profiles_.size();++i)if(profiles_[i].active())return i;return 0;}
void ProfileStore::create(std::size_t slot,std::string_view name){auto profile=PlayerProfile::empty(defaults_,slot_name(slot));profile.set_name(name);profile.set_active(true);profiles_.at(slot)=std::move(profile);}
void ProfileStore::erase(std::size_t slot){profiles_.at(slot)=PlayerProfile::empty(defaults_,slot_name(slot));}
void ProfileStore::save(std::size_t slot) const {
    safe_save_folder(game_folder_,save_folder_);std::filesystem::create_directories(save_folder_);
    const auto target=save_folder_/slot_name(slot);
    if(std::filesystem::is_symlink(target))throw AssetError("Profile save target must be a regular file");
    const auto temporary=save_folder_/(slot_name(slot)+".tmp-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto data=profiles_.at(slot).encoded();
    {std::ofstream output(temporary,std::ios::binary|std::ios::trunc);output.write(reinterpret_cast<const char*>(data.data()),data.size());if(!output)throw AssetError("Cannot write profile save");}
#ifdef _WIN32
    if(!MoveFileExW(temporary.c_str(),target.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw AssetError("Cannot replace profile save");
#else
    std::filesystem::rename(temporary,target);
#endif
}
void ProfileStore::save_all() const{for(std::size_t slot=0;slot<profiles_.size();++slot)save(slot);}
std::string portrait_filename(std::uint32_t index){
    if(index>=PlayerProfile::portrait_count)throw AssetError("Unknown original portrait");
    std::ostringstream out;if(index<8)out<<"face"<<std::setw(2)<<std::setfill('0')<<index+1;else out<<"female"<<std::setw(2)<<std::setfill('0')<<index-7;out<<".pcx";return out.str();
}

} // namespace dfre
