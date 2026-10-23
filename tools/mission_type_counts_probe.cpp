#include "dfre/mission_type_counts.hpp"
#include <iostream>
#include <sstream>
#include <unordered_map>
using namespace dfre;
int main(){try{std::string line;while(std::getline(std::cin,line)){if(line.empty())continue;std::istringstream in(line);std::uint32_t v[10];for(auto& n:v){std::uint64_t x;if(!(in>>x))throw MissionTickError("Short type-count fixture");n=std::uint32_t(x);}std::unordered_map<std::uint32_t,std::uint8_t> mem;auto put=[&](auto a,auto n,unsigned w=4){for(unsigned i=0;i<w;++i)mem[a+i]=std::uint8_t(n>>(i*8));};auto get=[&](auto a,unsigned w=4){std::uint32_t n=0;for(unsigned i=0;i<w;++i)n|=std::uint32_t(mem[a+i])<<(i*8);return n;};
for(unsigned i=0;i<64;++i){put(0x7936acu+i*64,v[0]+i);put(0x7936b8u+i*64,v[1]+i);}const std::uint32_t bases[]={0x867e80,0x7cca80,0x7dde80},counts[]={0x8cf904,0x8cfa18,0x8cfa1c};for(unsigned g=0;g<3;++g){put(counts[g],v[2+g]);for(unsigned i=0;i<3;++i){put(bases[g]+i*0x114,0);put(bases[g]+i*0x114+0x6c,(v[5]&(1u<<(g*3+i)))?4u:0u,1);put(bases[g]+i*0x114+0x70,((v[6]+i+g)&65535u)<<16);}}
unsigned seen[3]={};std::ostringstream trace;MissionTypeCountBindings b;
b.read=[&](auto a,auto w){trace<<'R'<<a<<','<<w<<';';for(unsigned g=0;g<3;++g)if(a==counts[g]){if(seen[g]++==1&&(v[7]&(1u<<g)))put(a,v[8]);}if(a==bases[1]+0x6c&&(v[7]&8))put(a+4,63u<<16);return get(a,w);};
b.write=[&](auto a,auto w,auto n){trace<<'W'<<a<<','<<w<<','<<n<<';';if(a==0x7936b8&&(v[7]&16))throw MissionTickError("Type-count write failure");put(a,n,w);if(a==0x7936ac&&n==0&&(v[7]&32))put(0x7936b8,v[9]);};bool failed=false;try{mission_type_counts(b);}catch(const MissionTickError&){failed=true;}std::cout<<failed;for(unsigned i=0;i<64;++i)std::cout<<' '<<get(0x7936acu+i*64)<<' '<<get(0x7936b8u+i*64);std::cout<<' '<<trace.str()<<'\n';}}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
