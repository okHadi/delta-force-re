#include "dfre/mission_character_instance.hpp"
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
using namespace dfre;using U=std::uint32_t;
int main(){try{std::string line;while(std::getline(std::cin,line)){
 U op,seed,count;MissionCharacterAllocateRegisters r;std::istringstream in(line);in>>op>>seed>>r.eax>>r.ebx>>r.ecx>>r.edx>>r.esi>>r.edi>>r.ebp>>count;std::unordered_map<U,std::uint8_t> data;
 auto put=[&](U a,U w,U n){for(U i=0;i<w;++i)data[a+i]=std::uint8_t(n>>(i*8));};auto get=[&](U a,U w){U n=0;for(U i=0;i<w;++i)n|=U(data[a+i])<<(i*8);return n;};
 for(U i=0;i<count;++i){U a,w,n;if(!(in>>a>>w>>n))throw MissionCharacterAllocateError("Short character-instance fixture");put(a,w,n);}std::ostringstream trace;
 MissionCharacterInstanceBindings b;
 b.memory.read=[&](U a,U w){if(!((a>=0x400000&&std::uint64_t(a)+w<=0xe00000)||(a>=0x1300000&&std::uint64_t(a)+w<=0x1400000)))throw MissionCharacterAllocateError("Unmapped fixture read");if((seed&16)&&a==0x1300210)throw MissionCharacterAllocateError("Fixture read fault");if((seed&1)&&a==0x1300058)put(0x1300080,4,0x1301600);if((seed&2)&&a==0x1300235)put(0x1301008,4,123);U n=get(a,w);trace<<'R'<<a<<','<<w<<','<<n<<';';return n;};
 b.memory.write=[&](U a,U w,U n){trace<<'W'<<a<<','<<w<<','<<n<<';';put(a,w,n);if((seed&4)&&a==0x130100c)put(0x1300236,1,7);if((seed&8)&&a==0x439a14)put(0x1300238,1,11);if((seed&32)&&a==0x1301004)throw MissionCharacterAllocateError("Fixture write fault");};
 b.memory.call=[&](U site,MissionCharacterAllocateRegisters& x){trace<<'E'<<site<<','<<x.edx<<';';throw MissionCharacterAllocateError("Fixture header error");};
 b.allocate=[&](U size,U align,U flags,U label){trace<<'A'<<size<<','<<align<<','<<flags<<','<<label<<';';if(seed&64)throw MissionCharacterAllocateError("Fixture allocation fault");if(seed&128)put(0x1300210,4,2);return 0x1302000u;};
 bool failed=false;try{switch(op){case 0:mission_character_instance_construct(b,r);break;case 1:mission_character_instance_auxiliary(b,r);break;case 2:mission_character_instance_frame(b,r);break;case 3:mission_character_instance_delta(b,r);break;case 4:mission_character_instance_immediate(b,r);break;default:mission_character_instance_rotate(b,r);break;}}catch(const MissionCharacterAllocateError&){failed=true;}
 trace<<'F'<<failed<<';';for(U n:{r.eax,r.ebx,r.ecx,r.edx,r.esi,r.edi,r.ebp})trace<<'G'<<n<<';';std::uint64_t h=1469598103934665603ull;for(U i=0;i<0x4000;++i){h^=get(0x1300000+i,1);h*=1099511628211ull;}for(U i=0;i<0x400;++i){h^=get(0x439980+i,1);h*=1099511628211ull;}trace<<'H'<<h<<';';std::cout<<trace.str()<<'\n';
}}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
