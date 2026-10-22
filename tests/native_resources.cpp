#include "dfre/resources.hpp"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace dfre;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void put(AssetBytes& bytes,std::size_t offset,std::uint32_t word){for(unsigned i=0;i<4;++i)bytes.at(offset+i)=std::uint8_t(word>>(i*8));}
AssetBytes encode(std::string text){
    std::uint32_t key=0x04960552;
    for(auto& byte:text){key+=(key<<11)|(key>>21);key=((key<<4)|(key>>28))^1;byte=char(std::uint8_t(byte)^(key&255));}
    return AssetBytes(text.rbegin(),text.rend());
}
AssetBytes model(){
    const std::size_t base=312;AssetBytes bytes(base+24+8+72+112+76);
    bytes[0]='3';bytes[1]='D';bytes[2]='I';bytes[3]=5;put(bytes,20,1);put(bytes,40,0x676e7263);
    put(bytes,148,std::uint32_t(bytes.size()-base));
    put(bytes,256,3);put(bytes,264,1);put(bytes,272,1);put(bytes,280,1);put(bytes,296,1);
    bytes[base+32+30]=1;bytes[base+32+32]=2;
    put(bytes,base+32+72+4,3);put(bytes,base+32+72+12,1);put(bytes,base+32+72+20,1);
    return bytes;
}
void archive(const std::filesystem::path& path,const std::vector<std::pair<std::string,AssetBytes>>& members){
    AssetBytes bytes(20),directory;put(bytes,0,20);std::copy_n("PFF3",4,bytes.begin()+4);
    put(bytes,8,std::uint32_t(members.size()));put(bytes,12,32);
    for(const auto& member:members){const auto at=directory.size();directory.resize(at+32);
        put(directory,at+4,std::uint32_t(bytes.size()));put(directory,at+8,std::uint32_t(member.second.size()));
        std::copy(member.first.begin(),member.first.end(),directory.begin()+at+16);
        bytes.insert(bytes.end(),member.second.begin(),member.second.end());
    }
    put(bytes,16,std::uint32_t(bytes.size()));bytes.insert(bytes.end(),directory.begin(),directory.end());
    std::ofstream file(path,std::ios::binary);file.write(reinterpret_cast<const char*>(bytes.data()),std::streamsize(bytes.size()));
    if(!file)throw std::runtime_error("Cannot write resource test archive");
}
struct Folder {
    std::filesystem::path path=std::filesystem::temp_directory_path()/("dfre-resources-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Folder(){std::filesystem::create_directory(path);}
    ~Folder(){std::error_code ignored;std::filesystem::remove_all(path,ignored);}
};
}
int main(){try{
    Folder folder;
    archive(folder.path/"Dfbase.pff",{});
    archive(folder.path/"Df.pff",{{"ITEMS.DEF",encode(
        "begin \"null\"\nid 100000\ntype marker\nend\n"
        "begin \"first\"\nid 104001\ntype object\ngraphic shared\nend\n"
        "begin \"second\"\nid 104033\ntype effect\ngraphic SHARED\nend\n"
        "begin \"third\"\nid 104042\ntype object\ngraphic absent\nend\n"
        "begin \"other\"\nid 104999\ntype object\ngraphic other\nend\n")},
        {"shared.3di",model()},{"other.3di",model()}});
    const AssetStore assets(folder.path);MissionAssetCache cache(assets);MissionRuntimeResources resources;
    cache.load_projectile_runtime(resources);
    check(resources.plan.calls.size()==3 && resources.models.size()==1 && cache.model_count()==1,"Projectile cache counts");
    check(resources.models[0].name=="shared.3di" && resources.models[0].bank==5,"Projectile model registry bank");
    check(resources.missing_models==std::vector<std::string>{"absent.3di"},"Missing projectile model stays null");
    check(&cache.model("SHARED.3DI")==&cache.model("shared.3di"),"Shared model cache identity");
    const auto steps=resources.plan.steps.size();cache.load_projectile_runtime(resources);
    check(resources.models.size()==1 && resources.plan.steps.size()==steps && resources.plan.calls.size()==6,"Repeated projectile pass");
    bool failed=false;try{cache.model("absent.3di");}catch(const AssetError&){failed=true;}check(failed,"Missing model cache lookup");
    MissionRuntimeResources wrong;failed=false;try{cache.load_projectile_runtime(wrong);}catch(const AssetError&){failed=true;}
    check(failed && wrong.plan.calls.empty(),"Mismatched runtime cache rejected before plan writes");
    MissionAssetCache different(assets);different.appearance(4999);
    check(different.model_count()==resources.models.size(),"Same-sized registry fixture");
    const auto calls=resources.plan.calls.size();failed=false;
    try{different.load_projectile_runtime(resources);}catch(const AssetError&){failed=true;}
    check(failed && resources.plan.calls.size()==calls,"Same-sized different model registry rejected");
    MissionAssetCache cache8(assets);MissionRuntimeResources resources8;resources8.options.color32=false;
    cache8.load_projectile_runtime(resources8);check(resources8.models[0].bank==0,"Shared runtime color mode");
    std::cout<<"resource cache checks passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
