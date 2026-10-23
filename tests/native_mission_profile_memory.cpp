#include "dfre/mission_profile_memory.hpp"
#include <chrono>
#include <iostream>
namespace {
using U=std::uint32_t;
void check(bool value,const char* text){if(!value)throw std::runtime_error(text);}
template<class F>void rejects(F f){bool failed=false;try{f();}catch(const dfre::AssetError&){failed=true;}check(failed,"Unowned profile access succeeded");}
}
int main(){try{
    dfre::ProfileDefaults defaults;defaults.controls.resize(0x228);for(auto& s:defaults.macros)s="Ready";
    const auto folder=std::filesystem::temp_directory_path()/("dfre-profile-memory-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    struct Cleanup{std::filesystem::path path;~Cleanup(){std::error_code ec;std::filesystem::remove_all(path,ec);}}cleanup{folder};
    std::filesystem::create_directories(folder/"original");dfre::ProfileStore store(folder/"original",folder/"saved",defaults);store.create(2,"current");std::size_t selected=2;
    dfre::MissionProfileMemory memory([&](auto slot)->dfre::AssetBytes&{return store.at(slot).source_bytes();},[&]{return selected;});
    const U first=memory.source(2);check(first==0x21102000&&memory.read(0x4cf2e4,4)==first,"Current selected pointer differs");
    for(U width:{1u,2u,4u})for(U offset=0;offset+width<=4;++offset){const U mask=width==4?0xffffffffu:(1u<<(width*8))-1;check(memory.read(0x4cf2e4+offset,width)==((first>>(offset*8))&mask),"Profile pointer byte view differs");}
    memory.write(first+0x640,4,7);check(store.at(2).portrait()==7,"Raw profile write used another owner");store.at(2).set_portrait(4);check(memory.read(first+0x640,4)==4,"Profile read cached an old image");
    memory.write(first+0x300,4,0x12345678);memory.write(first+0x301,2,0xabcd);check(memory.read(first+0x300,4)==0x12abcd78&&store.at(2).raw().at(0x301)==0xcd,"Profile byte aliases differ");
    selected=4;check(memory.read(0x4cf2e4,4)==memory.source(4)&&memory.read(first+0x300,4)==0x12abcd78,"Captured pointer followed a new selection");
    const U last=memory.source(8);memory.write(last+dfre::PlayerProfile::byte_size-1,1,0x5a);check(store.at(8).raw().back()==0x5a,"Last physical slot is not current");
    rejects([&]{memory.read(first+dfre::PlayerProfile::byte_size-1,2);});rejects([&]{memory.read(first+0xb00,1);});rejects([&]{memory.read(0x21100000-1,2);});rejects([&]{memory.read(0x4cf2e3,2);});rejects([&]{memory.write(0x4cf2e4,4,first);});rejects([&]{memory.read(first,8);});rejects([&]{memory.read(0xffffffff,4);});rejects([&]{memory.source(9);});
    selected=9;rejects([&]{memory.read(0x4cf2e4,4);});selected=2;
    check(!memory.claims(0x21000000,4)&&!memory.claims(0x22000000,4),"Profile bank overlaps sound storage");
    store.save(2);dfre::ProfileStore loaded(folder/"original",folder/"saved",defaults);check(loaded.at(2).portrait()==4&&loaded.at(2).raw().at(0x301)==0xcd&&loaded.at(2).name()=="CURRENT","Raw effects did not survive save/reload");
    store.at(2).source_bytes().resize(4);rejects([&]{memory.read(first,4);});
    std::cout<<"Current profile pointer, bytes and save checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
