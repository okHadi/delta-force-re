#include "dfre/mission_model_cleanup.hpp"
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
using namespace dfre;using U=std::uint32_t;
namespace {constexpr U B=0xc54480,Z=0xc8ff50;constexpr std::pair<U,U> regions[]={{B,116},{0xc65dc8,8},{0xc65dd0,368},{0xc80d10,372}};}
int main(){try{U mutation;std::string input;while(std::cin>>mutation>>input){std::vector<std::uint8_t> bytes(Z-B);std::size_t cursor=0;for(auto row:regions)for(U i=0;i<row.second;++i){bytes.at(row.first-B+i)=std::uint8_t(std::stoul(input.substr(cursor,2),nullptr,16));cursor+=2;}
 auto get=[&](U at,U width=4){if(at<B||std::uint64_t(at)+width>Z)throw std::runtime_error("fixture read extent");U value=0;for(U i=0;i<width;++i)value|=U(bytes.at(at-B+i))<<(i*8);return value;};auto put=[&](U at,U value,U width=4){if(at<B||std::uint64_t(at)+width>Z)throw std::runtime_error("fixture write extent");for(U i=0;i<width;++i)bytes.at(at-B+i)=std::uint8_t(value>>(i*8));};
 std::ostringstream trace;U stores=0,calls=0;MissionModelCleanupBindings b;
 b.read=[&](U at,U width){U value=get(at,width);trace<<'R'<<at<<','<<width<<','<<value<<';';if((mutation&1)&&at==B+0x24)put(B+0x24,0x1300090);if((mutation&256)&&at==0xc65dcc)put(0xc65dcc,0);return value;};
 b.write=[&](U at,U width,U value){trace<<'W'<<at<<','<<width<<','<<value<<';';put(at,value,width);++stores;if((mutation&2)&&at==B+0x24)put(B,2);if((mutation&16)&&stores==((mutation>>10)%8+1))throw std::runtime_error("fixture store stop");};
 b.call=[&](U site,const std::vector<U>& a)->U{trace<<'C'<<site;for(U value:a)trace<<','<<value;trace<<';';++calls;if((mutation&32)&&calls==((mutation>>10)%8+1))throw std::runtime_error("fixture call stop");if(site==0x4980d0){if(mutation&4)put(0xc65dcc,2);if(mutation&64)put(B+0x24,0x1300080);return 0;}if(site==0x49a620){for(U i=0;i<a.at(2);++i)put(a.at(0)+i,a.at(1)&255u,1);if(a.at(0)==0xc65dd0){if(mutation&8)put(0xc65dcc,1);if(mutation&128){put(0xc65dd0+0xb8+0x18,0x1300200);put(0xc65dcc,2);}}return 0;}throw std::runtime_error("unowned cleanup service");};
 bool failed=false;try{mission_model_cleanup(b);}catch(const std::exception&){failed=true;}
 std::uint64_t hash=1469598103934665603ull;for(auto value:bytes){hash^=value;hash*=1099511628211ull;}std::cout<<failed<<' '<<hash<<' '<<trace.str()<<'\n';
}return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
