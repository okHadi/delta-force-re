#include "dfre/mission_object_tree_spawn.hpp"
#include <array>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
using namespace dfre;using U=std::uint32_t;
int main(){try{std::string line;while(std::getline(std::cin,line)){
 U mode,mutation,fail,signal,distance,seed,count,local,flags,message;std::array<std::string,3> hex;
 std::istringstream in(line);if(!(in>>mode>>mutation>>fail>>signal>>distance>>seed>>count>>local>>flags>>message>>hex[0]>>hex[1]>>hex[2]))throw MissionTickError("Short tree fixture");
 constexpr U A=0x7dde80,G=0x7cca80,D=0x1100000,W=0x1110000,F=0x1300000;
 std::vector<std::uint8_t> a(0x114),g(256*0x114),d(4*0x13c),w(0x100),frame(0x114,0xa5);std::map<U,U> globals{{0xc23810,D},{0xc23814,count},{0xc23820,local},{0x67dbc9,flags}};
 const auto bytes=[&](U at)->std::uint8_t&{if(at>=A&&at<A+a.size())return a.at(at-A);if(at>=G&&at<G+g.size())return g.at(at-G);if(at>=D&&at<D+d.size())return d.at(at-D);if(at>=W&&at<W+w.size())return w.at(at-W);if(at>=F&&at<F+frame.size())return frame.at(at-F);throw MissionTickError("Tree fixture address");};
 const auto put=[&](U at,U n,U width=4){for(U i=0;i<width;++i)bytes(at+i)=std::uint8_t(n>>(8*i));};
 for(U k=0;k<3;++k){if(hex[k].size()!=0x228)throw MissionTickError("Short tree image");for(U i=0;i<0x114;++i)bytes((k==0?A:(k==1?G:G+255*0x114))+i)=std::uint8_t(std::stoul(hex[k].substr(i*2,2),nullptr,16));}
 for(U i=0;i<4;++i){put(D+i*0x13c+0x20,(i==1||i==2)?0xfd3:0);put(D+i*0x13c+0x6c,0x11110000+i);put(D+i*0x13c+0x70,0x22220000+i);put(D+i*0x13c+0x78,W);put(D+i*0x13c+0x84,0x33330000+i);put(D+i*0x13c+0x90,0x44440000+i);}
 for(U i=0;i<4;++i)put(W+i*0x1c+0x10,0x10000+i*0x10000);
 std::ostringstream trace;U calls=0,clears=0;bool failed=false;MissionObjectTreeBindings b;
 b.read=[&](U at,U width){U n=0;auto found=globals.find(at);if(found!=globals.end())n=found->second&((width==4)?0xffffffffu:((1u<<(width*8))-1));else for(U i=0;i<width;++i)n|=U(bytes(at+i))<<(i*8);trace<<'R'<<at<<','<<width<<','<<n<<';';if((mutation&16)&&at==G+0x28)put(A+0x28,0x12345678);if((mutation&32)&&at>=W&&at<W+w.size())put(G+0x6d,0x40,1);if((mutation&64)&&at==D+0x20)globals[0xc23814]=2;return n;};
 b.write=[&](U at,U width,U n){put(at,n,width);trace<<'W'<<at<<','<<width<<','<<n<<';';if((mutation&128)&&at==A+0xe0)put(A+0xfc,77);};
 b.call=[&](U site,const std::vector<U>& args){trace<<'C'<<site;for(auto n:args)trace<<','<<n;trace<<';';if(++calls==fail)throw MissionTickError("Tree call stopped");if(site==0x44fba0){if(mutation&1)globals[0xc23810]=D+0x13c;return signal;}if(site==0x477670)return distance;if(site==0x477860){seed=seed*1664525u+1013904223u;return seed;}if(site==0x43e7d0){if(mutation&2){put(A+0x24,11);put(A+0x28,22);put(A+0x2c,33);}return 0u;}if(site==0x49a620){for(U i=0;i<args.at(2);++i)bytes(args.at(0)+i)=std::uint8_t(args.at(1));if((mutation&4)&&++clears==2)globals[0xc23810]=D+0x13c;return args.at(0);}if(site==0x45d970){if(mutation&8){put(A+0x24,44);put(D+2*0x13c+0x84,0xabcdef01);}return 0u;}throw MissionTickError("Unknown tree fixture body");};
 U result=0;try{if(mode)result=mission_object_tree_signal(b,A);else mission_object_tree_spawn(b,A,F,message);}catch(const MissionTickError&){failed=true;}
 std::uint64_t hash=1469598103934665603ull;const auto digest=[&](U n,U width){for(U i=0;i<width;++i){hash^=std::uint8_t(n>>(i*8));hash*=1099511628211ull;}};for(const auto* data:{&a,&g,&d,&w,&frame})for(auto n:*data)digest(n,1);for(const auto& entry:globals)digest(entry.second,4);digest(seed,4);std::cout<<failed<<' '<<result<<' '<<hash<<' '<<trace.str()<<'\n';
}return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
