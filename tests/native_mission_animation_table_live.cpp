#include "dfre/mission_animation_table_live.hpp"
#include "dfre/animation_driver.hpp"
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

namespace {
template<class F> void rejects(F action,const char* message){bool failed=false;try{action();}catch(const std::exception&){failed=true;}check(failed,message);}
void compare(const MissionAnimationTableLive& codec,std::uint32_t base,std::string_view text){
 const auto actions=build_animation_driver_actions(decode_animation_definition(text),original_animation_compiled_actions());
 for(std::uint32_t item=0;item<256;++item){std::uint32_t weight=0;for(const auto& action:actions)if(action.item_index==item)weight=std::uint32_t(action.weight_10);
  const auto at=base+item*0x1c+0x10;check(codec.read(at,4)==weight,"Current ANM weight differs from packed action");
  for(std::uint32_t byte=0;byte<4;++byte)check(codec.read(at+byte,1)==((weight>>(byte*8))&255u),"Weight byte differs");
  for(std::uint32_t byte=0;byte<3;++byte)check(codec.read(at+byte,2)==((weight>>(byte*8))&65535u),"Weight word differs");
 }
}
}
int main(int argc,char** argv){try{
 Folder folder;archive(folder.path/"Dfbase.pff",{});
 archive(folder.path/"Df.pff",{{"ITEMS.DEF",encode(
  "begin \"null\"\nid 100000\ntype marker\nend\n"
  "begin \"first\"\nid 101111\ntype object\nanim_def a\nend\n"
  "begin \"second\"\nid 101112\ntype object\nanim_def b\nend\n"
  "begin \"bad\"\nid 101113\ntype object\nanim_def bad\nend\n")},
  {"a.anm",encode("Standing_1HW 5 1.5\r\nRunning0_1HW 7 2\r\nStanding_1HW 9\r\n")},
  {"b.anm",encode("Standing_2HW 4 1\r\nWalking0_1HW 3 1\r\n")},
  {"bad.anm",encode("Unknown_Action 1 2\r\n")}});
 const AssetStore assets(folder.path);MissionAssetCache cache(assets);cache.appearance(1111);cache.appearance(1112);cache.appearance(1113);
 MissionRuntimeState runtime;const auto A=kAnimationHandleBase,B=A+1,C=A+2;runtime.animations={{"a.anm",A},{"b.anm",B},{"bad.anm",C}};
 constexpr std::uint32_t base=0x1d000000,stride=0x2000;MissionAnimationTableLive codec(runtime,cache,base,stride,3);
 check(codec.table_address(0)==0,"Null ANM handle differs");rejects([&]{codec.read(0,4);},"Null pointer read succeeded");
 const auto a=codec.table_address(A),b=codec.table_address(B);check(a==base&&b==base+stride&&codec.table_address(A)==a,"ANM spans are not stable and distinct");
 compare(codec,a,cache.animations("a.anm"));compare(codec,b,cache.animations("b.anm"));
 check(codec.read(a+8*0x1c+0x10,4)==0,"Unnamed loaded item is not zero");check(codec.read(a+2*0x1c+0x10,4)==0,"Absent ANM row is not zero");
 runtime.definitions.resize(1);runtime.definitions[0].animation=A;const auto captured=codec.table_address(runtime.definitions[0].animation);runtime.definitions[0].animation=B;
 check(codec.table_address(runtime.definitions[0].animation)==b&&codec.read(captured+0x1c+0x10,4)==0x10000&&codec.read(b+0x1c+0x10,4)==0,"Current definition switch changed captured old table identity");
 check(!codec.claims(base-4,4)&&!codec.claims(base+3*stride,4),"Codec claims another virtual owner");
 for(const auto at:{a,a+0x0f,a+0x13,a+0x14,a+256*0x1c,a+stride-1})rejects([&]{codec.read(at,4);},"Unsupported field/boundary succeeded");
 rejects([&]{codec.read(a+0x10,3);},"Invalid width succeeded");rejects([&]{codec.read(base-1,2);},"Partial reservation entry succeeded");rejects([&]{codec.read(0xffffffff,2);},"Wrapped range succeeded");
 rejects([&]{codec.read(base+2*stride+0x10,4);},"Unassigned span succeeded");const auto c=codec.table_address(C);rejects([&]{codec.read(c+0x10,4);},"Unknown ANM name succeeded");
 runtime.animations["unloaded.anm"]=C+1;rejects([&]{codec.table_address(C+1);},"Unloaded cache succeeded");rejects([&]{codec.table_address(C+2);},"Unknown handle succeeded");
 runtime.animations["alias.anm"]=A;rejects([&]{codec.read(a+0x10,4);},"Duplicate current handle succeeded");runtime.animations.erase("alias.anm");runtime.animations.erase("a.anm");rejects([&]{codec.read(a+0x10,4);},"Removed current owner succeeded");runtime.animations["a.anm"]=A;
 MissionAnimationTableLive tiny(runtime,cache,base,stride,1);tiny.table_address(A);rejects([&]{tiny.table_address(B);},"Full reservation succeeded");
 rejects([&]{MissionAnimationTableLive bad(runtime,cache,0,stride,1);},"Null reservation succeeded");rejects([&]{MissionAnimationTableLive bad(runtime,cache,base,0x1000,1);},"Short stride succeeded");rejects([&]{MissionAnimationTableLive bad(runtime,cache,base+1,stride,1);},"Unaligned base succeeded");rejects([&]{MissionAnimationTableLive bad(runtime,cache,base,stride,0);},"Empty reservation succeeded");rejects([&]{MissionAnimationTableLive bad(runtime,cache,0xffffe000,stride,2);},"Wrapped reservation succeeded");
 if(argc==2){const AssetStore stock(argv[1]);MissionAssetCache loaded(stock);const auto resources=loaded.load_runtime("C06M01.BMS");MissionRuntimeState live;std::uint32_t next=kAnimationHandleBase;
  for(const auto& step:resources.plan.steps)if(step.kind==ResourceKind::animation&&step.first_load){auto name=step.name;for(auto& ch:name)if(ch>='A'&&ch<='Z')ch=char(ch+32);if(name.size()<4||name.substr(name.size()-4)!=".anm")name+=".anm";live.animations.emplace(name,next++);}
  MissionAnimationTableLive actual(live,loaded,base,stride,1024);check(!live.animations.empty(),"Supplied mission has no ANM");for(const auto& entry:live.animations)compare(actual,actual.table_address(entry.second),loaded.animations(entry.first));std::cout<<live.animations.size()<<" supplied ANM tables checked\n";
 }else if(argc!=1)throw std::invalid_argument("Use optional supplied game folder");
 std::cout<<"animation weight live checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
