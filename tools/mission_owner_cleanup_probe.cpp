#include "dfre/mission_owner_cleanup.hpp"
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
using namespace dfre;using U=std::uint32_t;
namespace{constexpr U C=0x1300000,B=0x1301000,L=0x1302000,M=0x1303000,A=0x1304000,F=0x1305000,E=0x1306000,T=0x1307000,V=0x6e06c4,H=0x6e06e8,J=0x6e0700,R=0x503304,D=0xd08940,Z=0x4a2712;constexpr std::pair<U,U> regions[]={{C,128},{B,160},{L,144},{M,168},{A,156},{F,16},{E,4},{T,160},{V,12},{H,24},{J,64},{R,64},{D,4},{Z,4}};}
int main(){try{U op,mutation;std::string input;while(std::cin>>op>>mutation>>input){std::unordered_map<U,std::uint8_t> bytes;std::size_t cursor=0;for(auto row:regions)for(U i=0;i<row.second;++i){bytes[row.first+i]=std::uint8_t(std::stoul(input.substr(cursor,2),nullptr,16));cursor+=2;}for(U i=64;i<0x4000;++i)bytes[J+i]=0;
 auto get=[&](U at,U width=4){U value=0;for(U i=0;i<width;++i)value|=U(bytes.at(at+i))<<(i*8);return value;};auto put=[&](U at,U value,U width=4){for(U i=0;i<width;++i)bytes.at(at+i)=std::uint8_t(value>>(i*8));};std::ostringstream trace;U calls=0,stores=0;MissionOwnerCleanupBindings b;
 b.read=[&](U at,U width){U value=get(at,width);trace<<'R'<<at<<','<<width<<','<<value<<';';if((mutation&1)&&at==H)put(H+4,77);if((mutation&2)&&at==C+0x28)put(E,C+64);if((mutation&8)&&at==A+0x23)put(A+0x24,0x1400090);return value;};
 b.write=[&](U at,U width,U value){trace<<'W'<<at<<','<<width<<','<<value<<';';put(at,value,width);++stores;if((mutation&4)&&at==T+0x18)put(V,T+40);if((mutation&16)&&stores==((mutation>>10)%10+1))throw std::runtime_error("store fixture stop");};
 b.call=[&](U site,const std::vector<U>& a)->U{trace<<'C'<<site;for(U value:a)trace<<','<<value;trace<<';';++calls;if((mutation&32)&&calls==((mutation>>10)%12+1))throw std::runtime_error("call fixture stop");if(site==0x4980d0){if(mutation&64){put(B+0x24,1);put(L,0,2);}if(mutation&128){put(L+0x10,M+56);put(C+0x38,B+80);}if(mutation&512)put(R+4,get(R+4)+1);return 0;}if(site==0x49a620){for(U i=0;i<a.at(2);++i)put(a.at(0)+i,a.at(1)&255u,1);return 0;}if(site==0x4a2aed){if(mutation&256)put(H,99);return 0;}if(site==0x4560a0||site==0x455b40||site==0x455ea0||site==0x4950a0||site==0x49cf70||site==0x495070)return 0;throw std::runtime_error("unowned service");};
 bool failed=false;try{if(op==0)mission_sound_cleanup(b);else if(op==1)mission_sound_bank_cleanup(b,E,F);else if(op==2)mission_resource_cleanup_global(b);else mission_resource_cleanup(b,R+4);}catch(const std::exception&){failed=true;}
 std::uint64_t hash=1469598103934665603ull;for(auto row:regions)for(U i=0;i<row.second;++i){hash^=bytes.at(row.first+i);hash*=1099511628211ull;}std::cout<<failed<<' '<<hash<<' '<<trace.str()<<'\n';
}return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
