#include "dfre/mission_rtxt.hpp"
#include <algorithm>
#include <iostream>
#include <sstream>
using U=std::uint32_t;
namespace {
constexpr U A=0x1200000,B=0x1201000,Q=0x1203000,K=0x1203100,F=0x1203200,G=0x499e70;
void put(std::vector<std::uint8_t>& p,U at,U n,U width=4){for(U i=0;i<width;++i)p.at(at+i)=std::uint8_t(n>>(i*8));}
U get(const std::vector<std::uint8_t>& p,U at,U width=4){U n=0;for(U i=0;i<width;++i)n|=U(p.at(at+i))<<(i*8);return n;}
void text(std::vector<std::uint8_t>& p,U at,const std::string& s){for(U i=0;i<=s.size();++i)p.at(at+i)=i<s.size()?std::uint8_t(s[i]):0;}
std::vector<std::uint8_t> file(U count){std::vector<std::uint8_t> p(1024,0xcc);put(p,0,0x54585452);put(p,4,0x100);put(p,8,0x12345678);put(p,12,count);for(U i=0;i<3;++i){put(p,16+i*16,i*4);put(p,20+i*16,0x100+i);put(p,24+i*16,0x200+i);put(p,28+i*16,0x300+i);}text(p,0x40,"one");text(p,0x44,"two");text(p,0x48,"three");put(p,0x100,2);put(p,0x104,0x80);put(p,0x108,2);put(p,0x10c,0x90);put(p,0x110,1);text(p,0x114,"Alpha");text(p,0x11a,"Beta");text(p,0x184,"KEY");text(p,0x188,"Key");text(p,0x194,"OTHER");return p;}
std::string hex(const std::vector<std::uint8_t>& p){const char* h="0123456789abcdef";std::string s;for(auto b:p){s+=h[b>>4];s+=h[b&15];}return s;}
}
int main(){try{U mode,mutation,index,group,key,loaded,count;while(std::cin>>mode>>mutation>>index>>group>>key>>loaded>>count){auto a=file(count),b=file(count);put(b,4,B+0x100);std::vector<std::uint8_t> globals(0x28,0xa5),strings(0x400,0);text(strings,0,group==0?"Alpha":group==1?"alpha":group==2?"Beta":"Missing");text(strings,0x100,key==0?"KEY":key==1?"Key":key==2?"OTHER":key==3?"key":"Missing");text(strings,0x200,"test.bin");unsigned global_reads=0;bool active=false;std::ostringstream trace;
 const auto location=[&](U at,U width)->std::pair<std::vector<std::uint8_t>*,U>{if(width!=1&&width!=4)throw dfre::MissionSoundAssetError("Unexpected RTXT width");for(auto entry:{std::make_pair(A,&a),std::make_pair(B,&b),std::make_pair(G,&globals),std::make_pair(Q,&strings)})if(at>=entry.first&&std::uint64_t(at-entry.first)+width<=entry.second->size())return {entry.second,at-entry.first};throw dfre::MissionSoundAssetError("RTXT fixture address has no owner");};
 dfre::MissionSoundAssetBindings memory;
 memory.read=[&](U at,U width){if(active){trace<<'R'<<at<<','<<width<<';';if(at==G){++global_reads;if((mutation&4)&&global_reads==2)put(globals,0,B);}if(at==G+4&&(mutation&1))put(globals,4,2);if(at==G+8&&(mutation&32))put(globals,8,0xdeadbeef);if(at==G+0x18&&(mutation&16))put(globals,0x18,1);if(at==G+0x14&&(mutation&128))put(globals,0x14,A+0x10c);}const auto p=location(at,width);return get(*p.first,p.second,width);};
 memory.write=[&](U at,U width,U value){if(active){trace<<'W'<<at<<','<<width<<','<<value<<';';if(at==G+0x14&&(mutation&64))put(a,0x100,0);}const auto p=location(at,width);put(*p.first,p.second,value,width);};
 memory.call=[&](U site,const std::vector<U>& args){if(site!=0x499410||args.size()!=1)throw dfre::MissionSoundAssetError("Unowned RTXT service");trace<<'C'<<site<<','<<args[0]<<';';if(mutation&256)put(a,12,2);return loaded;};
 if(mode>=2){put(a,12,3);dfre::mission_rtxt_fixup(memory,A);put(a,12,count);std::fill(globals.begin(),globals.end(),0xa5);}
 active=true;U result=0;bool failed=false;try{if(mode==0)result=dfre::mission_rtxt_fixup(memory,A);else if(mode==1)result=dfre::mission_rtxt_load(memory,F);else if(mode==2)result=dfre::mission_rtxt_lookup(memory,index?0:A,group==4?G+12:Q,key==5?G+16:K);else result=dfre::mission_rtxt_entry(memory,group?0:A,index);}catch(const std::exception&){failed=true;}
 std::cout<<failed<<' '<<result<<' '<<hex(a)<<' '<<hex(b)<<' '<<hex(globals)<<' '<<trace.str()<<'\n';
 }}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
