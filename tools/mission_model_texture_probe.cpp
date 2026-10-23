#include "dfre/mission_model_texture.hpp"
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
using namespace dfre;using U=std::uint32_t;
namespace{constexpr U C=0xc54480,T=0xc54488,G=0xc65dc8,V=0xc80d10,E=0x1300000,H=0x1301000,Q=0x1302000,P=0x1304000;constexpr std::pair<U,U> regions[]={{C,8},{T,144},{G,4},{V,4},{E,4},{H,36},{Q,2048},{P,4096}};}
int main(){try{U mode,mutation;std::string input;while(std::cin>>mode>>mutation>>input){std::unordered_map<U,std::uint8_t> bytes;std::size_t cursor=0;for(auto row:regions)for(U i=0;i<row.second;++i){bytes[row.first+i]=std::uint8_t(std::stoul(input.substr(cursor,2),nullptr,16));cursor+=2;}
 auto get=[&](U at,U width=4){U value=0;for(U i=0;i<width;++i)value|=U(bytes.at(at+i))<<(i*8);return value;};auto put=[&](U at,U value,U width=4){for(U i=0;i<width;++i)bytes.at(at+i)=std::uint8_t(value>>(i*8));};
 std::ostringstream trace;U stores=0,calls=0,reads=0;MissionModelTextureBindings b;
 b.read=[&](U at,U width){U value=get(at,width);trace<<'R'<<at<<','<<width<<','<<value<<';';if((mutation&1)&&at==E)put(E,value+1);if((mutation&2)&&at==G)put(G,1);return value;};
 b.write=[&](U at,U width,U value){trace<<'W'<<at<<','<<width<<','<<value<<';';put(at,value,width);++stores;if((mutation&4)&&at==T+36+0x1c)put(T+36+0x10,8);if((mutation&8)&&at==P+16)put(T+36+0x20,P+24);if((mutation&16)&&stores==((mutation>>8)%800+1))throw std::runtime_error("fixture store stop");};
 b.call=[&](U site,const std::vector<U>& a)->U{trace<<'C'<<site;for(U value:a)trace<<','<<value;trace<<';';++calls;if((mutation&32)&&calls==((mutation>>8)%5+1))throw std::runtime_error("fixture call stop");if(site==0x497f60)return mode&1?0:P;if(site==0x499280){++reads;U from=reads==1?H:Q;for(U i=0;i<a.at(1);++i)put(a.at(0)+i,get(from+i,1),1);if((mutation&64)&&reads==2)put(T+36+0x20,P+32);return 0;}if(site==0x4992d0)return 0;if(site==0x499e5e){if(mutation&128){put(E,2);put(C,2);put(T+0x1c,P+2000);}return mode&4?1:0;}if(site==0x4980d0){if(mutation&256)put(T+0x20,P+2010);return 0;}if(site==0x49a620){for(U i=0;i<144;++i)put(T+i,0,1);return 0;}throw std::runtime_error("unowned service");};
 bool failed=false;try{if(mode&2)mission_model_texture_initialize(b);else mission_model_texture_load(b,E);}catch(const std::exception&){failed=true;}
 std::uint64_t hash=1469598103934665603ull;for(auto row:regions)for(U i=0;i<row.second;++i){hash^=bytes.at(row.first+i);hash*=1099511628211ull;}std::cout<<failed<<' '<<hash<<' '<<trace.str()<<'\n';
}return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
