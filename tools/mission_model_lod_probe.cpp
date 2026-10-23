#include "dfre/mission_model_lod.hpp"
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
using namespace dfre; using U=std::uint32_t;
namespace {constexpr U C=0xc65dcc,L=0xc65dd0,P=0x1300000,H=0x1302000,Q=0x1303000,E=0x1304000,F=0x1305000,T=0xc54488,V=0xc80d10;constexpr std::pair<U,U> regions[]={{C,4},{L,0x170},{P,4096},{H,0xb8},{Q,4096},{E,12},{F,40},{T,72},{V,12}};}
int main(){try{U mode,mutation;std::string input;while(std::cin>>mode>>mutation>>input){std::unordered_map<U,std::uint8_t> bytes;std::size_t cursor=0;for(auto row:regions)for(U i=0;i<row.second;++i){bytes[row.first+i]=std::uint8_t(std::stoul(input.substr(cursor,2),nullptr,16));cursor+=2;}
 auto get=[&](U at,U width=4){U value=0;for(U i=0;i<width;++i)value|=U(bytes.at(at+i))<<(i*8);return value;};
 auto put=[&](U at,U value,U width=4){for(U i=0;i<width;++i)bytes.at(at+i)=std::uint8_t(value>>(i*8));};
 std::ostringstream trace;U stores=0,calls=0,reads=0;MissionModelLodBindings b;
 b.read=[&](U at,U width){U value=get(at,width);trace<<'R'<<at<<','<<width<<','<<value<<';';if((mutation&1)&&at==L+0x98)put(L+0x98,0);if((mutation&2)&&at==E+8)put(E+8,value+1);return value;};
 b.write=[&](U at,U width,U value){trace<<'W'<<at<<','<<width<<','<<value<<';';put(at,value,width);++stores;if((mutation&4)&&at==L+0x84)put(L+0x18,P+16);if((mutation&8)&&at==F+0x18)put(E+8,get(E+8)+1);if((mutation&16)&&stores==((mutation>>8)%20+1))throw std::runtime_error("store fixture stop");};
 b.call=[&](U site,const std::vector<U>& a)->U{trace<<'C'<<site;for(U value:a)trace<<','<<value;trace<<';';++calls;if((mutation&32)&&calls==((mutation>>8)%3+1))throw std::runtime_error("call fixture stop");if(site==0x497f60)return mode&1?0:P;if(site==0x499280){++reads;U from=reads==1?H:Q;for(U i=0;i<a.at(1);++i)put(a.at(0)+i,get(from+i,1),1);if((mutation&64)&&reads==2)put(L+0x18,P+8);return 0;}throw std::runtime_error("unowned call");};
 b.material_ticks=[&](U material,U result){trace<<'F'<<material<<','<<result<<';';U raw=get(material+0x1c,2);std::int32_t signedraw=raw<32768?std::int32_t(raw):std::int32_t(raw)-65536;put(result,U(signedraw*31/50));};
 U result=0;bool failed=false;try{result=mission_model_lod_load(b,E,F);}catch(const std::exception&){failed=true;}
 std::uint64_t hash=1469598103934665603ull;for(auto row:regions)for(U i=0;i<row.second;++i){hash^=bytes.at(row.first+i);hash*=1099511628211ull;}
 std::cout<<failed<<' '<<result<<' '<<hash<<' '<<trace.str()<<'\n';
}return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
