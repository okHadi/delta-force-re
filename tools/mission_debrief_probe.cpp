#include "dfre/mission_debrief.hpp"
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
using namespace dfre;using U=std::uint32_t;using Q=std::uint64_t;
int main(){try{std::string line;while(std::getline(std::cin,line)){
 U mode,seed,flags,mutation,fail_call,fail_read,fail_write,action;std::istringstream in(line);if(!(in>>mode>>seed>>flags>>mutation>>fail_call>>fail_read>>fail_write>>action))throw std::runtime_error("Short debrief fixture");
 constexpr U P=0x1100000,F=0x1300000,T=0x1320000;std::map<U,std::uint8_t> bytes;
 const auto reserve=[&](U at,U n){for(U i=0;i<n;++i)bytes[at+i]=std::uint8_t((i*37)^(seed>>(i%4*8)));};
 reserve(0x67f714,0x514);reserve(0x4e7544,0x514);reserve(P,0x200);reserve(F,0x64);reserve(T,0x2710);
 for(U at:{0x68bef4u,0x503238u,0x4cf2e4u,0x68bef0u,0x4e6d00u,0x4e6d04u,0x68ba34u,0x4e9bf0u,0x4e9bdcu,0x4e9c1cu,0x4e9c18u,0xcc08ccu,0x8ee1d4u})reserve(at,4);
 const auto put=[&](U at,U n,U width=4){for(U i=0;i<width;++i)bytes.at(at+i)=std::uint8_t(n>>(8*i));};
 const auto get=[&](U at,U width=4){U n=0;for(U i=0;i<width;++i)n|=U(bytes.at(at+i))<<(8*i);return n;};
 put(P+0x28,1);put(P+0x128,2);put(0x4cf2e4,P);put(0x68bef4,(flags&1)?0:0x77220000);put(0x503238,(flags&2)?0:0x77330000);put(0x68bef0,(flags&4)?0:T);put(0x68ba34,3);put(0x4e9bf0,(flags>>3)&1);put(0x4e9bdc,(flags>>4)&1);put(0x4e9c1c,0x4563a0);put(0x4e9c18,0x4563b0);put(0xcc08cc,(flags>>5)&1);put(0x8ee1d4,(flags&64)?0x4d0cd4:0);
 U calls=0,reads=0,writes=0,result=0;bool failed=false;std::ostringstream trace;MissionDebriefBindings b;
 b.read=[&](U at,U width){if(++reads==fail_read){trace<<'F'<<at<<','<<width<<';';throw std::runtime_error("Read stopped");}const auto n=get(at,width);trace<<'R'<<at<<','<<width<<','<<n<<';';if((mutation&256)&&at==0x67f724){put(0x67f728,0x12345678);put(0x4e7544,0x87654321);}return n;};
 b.write=[&](U at,U width,U n){if(++writes==fail_write){trace<<'E'<<at<<','<<width<<','<<n<<';';throw std::runtime_error("Write stopped");}put(at,n,width);trace<<'W'<<at<<','<<width<<','<<n<<';';if((mutation&512)&&at==0x68bef0)put(at,T+0x20);};
 b.call=[&](U site,const std::vector<U>& args){trace<<'C'<<site;for(auto n:args)trace<<','<<n;trace<<';';if(++calls==fail_call)throw std::runtime_error("Call stopped");switch(site){
 case 0x40bfd0:return 0x77220000u;
 case 0x40bca0:if(mutation&1)put(0x68bef4,0x77220004);break;
 case 0x4980d0:if(mutation&2)put(0x503238,0x77330004);break;
 case 0x493a80:put(args.at(0),args.at(2)&255,1);break;
 case 0x499f1f:if(mutation&4)put(0x4cf2e4,P+0x100);return 0x77330008u;
 case 0x497f60:return (flags&128)?0u:T;
 case 0x49a620:for(U i=0;i<args.at(2);++i)put(args.at(0)+i,args.at(1),1);break;
 case 0x41f320:if(mutation&8){put(0x4cf2e4,P+0x100);put(0x68ba34,5);}break;
 case 0x40d040:if(mutation&16){put(0x68bef0,T+0x20);put(0x4cf2e4,P);put(P+0x28,5);}return 0x77110000u;
 case 0x413460:if(mutation&32){put(0x4cf2e4,P+0x100);put(P+0x128,4);put(0x68ba34,7);}break;
 case 0x4132f0:case 0x40fa90:if(mutation&64)put(0x4e9bf0,get(0x4e9bf0)^1);break;
 case 0x4563a0:case 0x4563b0:if(mutation&128){put(0xcc08cc,1);put(0x8ee1d4,0x4d0cd4);put(0x4e9bf0,0);}break;
 case 0x461350:return get(0x8ee1d4);
 case 0x461340:put(0x8ee1d4,args.at(0));break;
 case 0x407850:case 0x40d550:case 0x40e4d0:case 0x413440:case 0x4564b0:case 0x41ef40:case 0x409180:case 0x41ef80:case 0x422ef0:case 0x4230a0:case 0x40dd80:case 0x40d9e0:case 0x40d820:case 0x4133c0:case 0x4230e0:case 0x496c50:case 0x40e2a0:case 0x40b9a0:break;
 default:throw std::runtime_error("Unknown debrief fixture service");
 }return 0u;};
 try{switch(mode){case 0:mission_debrief_enter(b,F);break;case 1:result=mission_debrief_update(b);break;case 2:mission_debrief_render(b);break;case 3:mission_debrief_leave(b);break;case 4:mission_debrief_accept(b,0x12345678,action);break;case 5:mission_debrief_quit(b,0x12345678,action);break;default:throw std::runtime_error("Unknown debrief mode");}}catch(const std::runtime_error&){failed=true;}
 Q hash=1469598103934665603ull;for(const auto& entry:bytes){hash^=entry.second;hash*=1099511628211ull;}std::cout<<failed<<' '<<result<<' '<<hash<<' '<<trace.str()<<'\n';
}return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
