#include "dfre/mission_leave.hpp"
#include <array>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
using namespace dfre;using U=std::uint32_t;using Q=std::uint64_t;
int main(){try{std::string line;while(std::getline(std::cin,line)){
 U seed,capture,mode,width,height,palette,mutation,fail_call,fail_read,fail_write;std::istringstream in(line);
 if(!(in>>seed>>capture>>mode>>width>>height>>palette>>mutation>>fail_call>>fail_read>>fail_write))throw std::runtime_error("Short leave fixture");
 constexpr U P=0x1300000,B=0x1320000;std::map<U,std::uint8_t> bytes;
 const auto reserve=[&](U at,U length){for(U i=0;i<length;++i)bytes[at+i]=std::uint8_t((i*37u)^(seed>>(i%4*8)));};
 reserve(P,512);reserve(B,4096);reserve(0x7cca80,0x11400);for(U at:{0x67f41cu,0xbfc104u,0x67f400u,0x67f404u,0x67dbe4u,0x67f548u})reserve(at,4);reserve(0x67dbfc,32);
 const auto put=[&](U at,Q value,U size=4){for(U i=0;i<size;++i)bytes.at(at+i)=std::uint8_t(value>>(8*i));};
 put(0x67f41c,mode);put(0xbfc104,palette?B:0);put(0x67f400,width);put(0x67f404,height);put(0x67dbe4,seed);put(0x67f548,0xabcdef01);
 for(U i=0;i<8;++i)put(0x67dbfc+i*4,P+i*64);for(U i=0;i<256;++i)put(0x7cca98+i*0x114,(i%17==0)?0x71000000+i:0);
 std::ostringstream trace;U calls=0,reads=0,writes=0;bool failed=false;MissionLeaveBindings b;
 b.read=[&](U at,U n)->Q{if(++reads==fail_read){trace<<'F'<<at<<','<<n<<';';throw std::runtime_error("Read stopped");}Q value=0;try{for(U i=0;i<n;++i)value|=Q(bytes.at(at+i))<<(i*8);}catch(const std::out_of_range&){trace<<'F'<<at<<','<<n<<';';throw std::runtime_error("Read unowned");}trace<<'R'<<at<<','<<n<<','<<value<<';';if((mutation&16)&&at==0x67dbfc)put(0x67dc00,P+256);return value;};
 b.write=[&](U at,U n,Q value){if(++writes==fail_write){trace<<'E'<<at<<','<<n<<','<<value<<';';throw std::runtime_error("Write stopped");}for(U i=0;i<n;++i)if(!bytes.count(at+i)){trace<<'E'<<at<<','<<n<<','<<value<<';';throw std::runtime_error("Write unowned");}put(at,value,n);trace<<'W'<<at<<','<<n<<','<<value<<';';if((mutation&2)&&at>=P&&at<P+512){put(0x67f400,2);put(0x67f404,1);}if((mutation&4)&&at==0x67f548)put(0x7cca98+255*0x114,0x7fffffff);};
 b.call=[&](U site,const std::vector<U>& args){trace<<'C'<<site;for(auto n:args)trace<<','<<n;trace<<';';if(++calls==fail_call)throw std::runtime_error("Call stopped");if(site==0x404540){if(mutation&1){put(0x67f41c,32);put(0x67f400,4);put(0x67f404,2);}return capture;}if(site==0x42cd30&&(mutation&8))put(0x7cca98+17*0x114,0);return 0u;};
 try{mission_leave(b);}catch(const std::runtime_error&){failed=true;}
 Q hash=1469598103934665603ull;for(const auto& entry:bytes){hash^=entry.second;hash*=1099511628211ull;}std::cout<<failed<<' '<<hash<<' '<<trace.str()<<'\n';
}return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
