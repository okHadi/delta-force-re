#include "dfre/mission_model_load.hpp"
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
using namespace dfre;using U=std::uint32_t;
namespace {constexpr U G=0x1300000,L=0x1301000,M=0x1302000,E=0x1303000,F=0x1304000,H=0x1305000,N=0x1306000,Q=0x1307000,T=0xc54480;constexpr std::pair<U,U> regions[]={{G,0x100},{L,2*0xb8},{M,4*0x4c},{E,12},{F,4},{H,0x7c},{N,32},{Q,4},{T,2}};}
int main(){try{U mode,mutation;std::string input;while(std::cin>>mode>>mutation>>input){std::unordered_map<U,std::uint8_t> bytes;std::size_t cursor=0;for(auto row:regions)for(U i=0;i<row.second;++i){bytes[row.first+i]=std::uint8_t(std::stoul(input.substr(cursor,2),nullptr,16));cursor+=2;}
 const auto get=[&](U at,U width=4){U n=0;for(U i=0;i<width;++i)n|=U(bytes.at(at+i))<<(i*8);return n;};const auto put=[&](U at,U n,U width=4){for(U i=0;i<width;++i)bytes.at(at+i)=std::uint8_t(n>>(i*8));};
 std::ostringstream trace;unsigned calls=0,stores=0,reads=0,textures=0,levels=0;MissionModelLoadBindings b;
 b.read=[&](U at,U width){const U n=get(at,width);trace<<'R'<<at<<','<<width<<','<<n<<';';if((mutation&32)&&at==G+0x50)put(G+0x50,L+0xb8);if((mutation&64)&&at==M+0x1f)put(M+0x1c,0x7fff,2);return n;};
 b.write=[&](U at,U width,U value){trace<<'W'<<at<<','<<width<<','<<value<<';';put(at,value,width);++stores;if((mutation&8)&&at==G+0x72)put(G+0x14,0);if((mutation&16)&&at==G+0x74)put(G+0x50,L+0xb8);if((mutation&2048)&&stores==((mutation>>7)%5+1))throw MissionModelLoadError("Fixture store stopped");};
 b.call=[&](U site,const std::vector<U>& a)->U{trace<<'C'<<site;for(U n:a)trace<<','<<n;trace<<';';++calls;if((mutation&4096)&&calls==((mutation>>7)%7+1))throw MissionModelLoadError("Fixture service stopped");
  if(site==0x499040){if(mutation&128){put(E,G+0x80);put(E+4,N+8);put(E+8,5);}return mode&1?0:1;}
  if(site==0x499120)return 0;
  if(site==0x499280){++reads;if(reads==1){const U length=mode&2?3:0x7c;for(U i=0;i<length;++i)put(a.at(0)+i,get(H+i,1),1);}else put(a.at(0),get(Q));return 0;}
  if(site==0x485ba0){++textures;put(T,(get(T,2)+1)&65535,2);if(mutation&1)put(F,0);return 0;}
  if(site==0x485e30){++levels;if(mutation&2)put(G+0x14,0);if(mutation&4)put(G+0x70,0x80018002);return mode&4?0:L+((levels-1)%2)*0xb8;}
  if(site==0x499020){if(mutation&256){put(G+0x50,L+0xb8);put(L+0xb8+0xa8,1);}return 0;}
  throw MissionModelLoadError("Fixture service is not owned");};
 bool failed=false;U result=0;try{result=mission_model_load(b,E,F);}catch(const std::exception&){failed=true;}
 std::uint64_t hash=1469598103934665603ull;for(auto row:regions)for(U i=0;i<row.second;++i){hash^=bytes.at(row.first+i);hash*=1099511628211ull;}
 std::cout<<failed<<' '<<result<<' '<<hash<<' '<<trace.str()<<'\n';
}return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
