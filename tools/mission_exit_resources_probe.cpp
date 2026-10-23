#include "dfre/mission_exit_resources.hpp"
#include <cstdint>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
using U=std::uint32_t;
int main(){U op,seed,mutation,fault;while(std::cin>>op>>seed>>mutation>>fault){
 constexpr U base=0xbd4800,end=0xbf6c04;std::vector<unsigned char> bytes(end-base);U status=seed;std::ostringstream trace;unsigned calls=0,stores=0;
 for(U i=0;i<bytes.size();++i)bytes[i]=static_cast<unsigned char>(i*13u+seed);
 auto put=[&](U at,U width,U value){for(U i=0;i<width;++i)bytes.at(at-base+i)=static_cast<unsigned char>(value>>(i*8));};
 auto get=[&](U at,U width){if(at==0x67dbbc&&width==4)return status;U v=0;for(U i=0;i<width;++i)v|=U(bytes.at(at-base+i))<<(i*8);return v;};
 for(U at=0xbd5284;at<0xbd5480;at+=4)put(at,4,((at/4+seed)%7)?0x1300000+(at-0xbd5284)*4:0);
 put(0xbd4828,4,seed%5?0x1400000:0);put(0xbd482c,4,seed%7?0x1400010:0);
 dfre::MissionExitResourceBindings b;
 b.read=[&](U at,unsigned w){U v=get(at,w);trace<<'R'<<at<<','<<w<<','<<v<<';';return v;};
 b.write=[&](U at,unsigned w,U v){trace<<'W'<<at<<','<<w<<','<<v<<';';if(at==0x67dbbc)status=v;else put(at,w,v);if(fault&0x10000u)if(++stores==(fault&255u))throw std::runtime_error("store fixture");};
 b.call=[&](U site,const std::vector<U>& a)->U{trace<<'C'<<site;for(U v:a)trace<<','<<v;trace<<';';++calls;if(!(fault&0x10000u)&&fault&&calls==fault)throw std::runtime_error("call fixture");if(mutation&&calls==1){put(0xbd52ac,4,0x1400080);put(0xbd4828,4,77);put(0xbd482c,4,88);}if(site==0x49a620)for(U i=0;i<a.at(2);++i)put(a.at(0)+i,1,a.at(1));return 0xffffffffu;};
 bool failed=false;try{if(op==0)dfre::mission_input_record_close(b);else if(op==1)dfre::mission_input_replay_close(b);else dfre::mission_terrain_resources_release(b);}catch(const std::exception&){failed=true;}
 std::uint64_t digest=1469598103934665603ull;for(auto v:bytes)digest=(digest^v)*1099511628211ull;for(U i=0;i<4;++i)digest=(digest^((status>>(i*8))&255))*1099511628211ull;
 std::cout<<failed<<' '<<digest<<' '<<trace.str()<<'\n';
}}
