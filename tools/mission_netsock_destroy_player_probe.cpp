#include "dfre/mission_netsock_destroy_player.hpp"
#include <array>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <cstdio>
#include <cstdlib>
namespace {
using U=std::uint32_t;
constexpr U sites[]={0x10001b10,0x10001b40,0x10001b80,0x10004510,0x10004550,0x100045c0,0x10004660};
constexpr U sizes[]={8,8,8,0x10,0x10,0x10,0x10};
struct Test {
 std::array<U,24> row{}; std::vector<std::uint8_t> data=std::vector<std::uint8_t>(0x100000),stack=std::vector<std::uint8_t>(0x100000);
 dfre::MissionNetsockPacketMemoryBindings foreign;std::unique_ptr<dfre::MissionNetsockPacketMemory> image;
 struct F{U site,at,n,id;};std::vector<F>frames;U serial=0,reads=0,writes=0,calls=0,allocs=0,sleeps=0;std::uint64_t trace=0;
 Test(const dfre::AssetBytes&raw){foreign.read=[&](U a,U n){U v=0;for(U i=0;i<n;++i)v|=U(*ptr(a+i,1))<<(8*i);return v;};foreign.write=[&](U a,U n,U v){for(U i=0;i<n;++i)*ptr(a+i,1)=std::uint8_t(v>>(8*i));};foreign.resolve=[&](U a,std::size_t n){return ptr(a,n);};image=std::make_unique<dfre::MissionNetsockPacketMemory>(raw,foreign);}
 std::uint8_t*ptr(U a,std::size_t n){if(a>=0x1200000&&std::uint64_t(a-0x1200000)+n<=data.size())return data.data()+a-0x1200000;if(a>=0x3000000&&std::uint64_t(a-0x3000000)+n<=stack.size())return stack.data()+a-0x3000000;throw std::runtime_error("Player fixture address is not mapped");}
 U get(U a,U n=4){return image->read(a,n);}void put(U a,U v,U n=4){image->write(a,n,v);}void string(U a,const std::string&s){for(U i=0;i<s.size();++i)put(a+i,std::uint8_t(s[i]),1);put(a+U(s.size()),0,1);}
 U norm(U a){for(auto i=frames.rbegin();i!=frames.rend();++i)if(a>=i->at&&a<i->at+i->n)return 0x30000000+i->id*0x1000+a-i->at;return a;}
 void event(U v){for(U i=0;i<4;++i){trace^=std::uint8_t(v>>(8*i));trace*=1099511628211ull;}}
 bool mapped(U a,U n){if(a>=0x10000000&&std::uint64_t(a)+n<=0x1001b000)return true;if(a>=0x1200000&&std::uint64_t(a)+n<=0x1300000)return true;for(auto f:frames)if(a>=f.at&&std::uint64_t(a)+n<=std::uint64_t(f.at)+f.n)return true;return false;}
 U frame(U s){for(auto i=frames.rbegin();i!=frames.rend();++i)if(i->site==s)return i->at;return 0;}
 U make(U s,U n,const std::vector<U>&args){const U a=0x3000000+serial*0x400;frames.push_back({s,a,n,serial++});std::fill_n(ptr(a,n),n,0x5a);if(s==0x10001660)put(a+3,row[16],1);U off=s==0x10001530||s==0x10004280?8:s==0x10001660?0x18:s==0x10002b80?0x60:4;for(U v:args){put(a+off,v);off+=4;}return a;}
 U input(){return row[18]==1?0x1240000:row[18]==2?0x1201000:0x1200000;}
 std::vector<U> args(){if(row[0]<3)return{row[4]};if(row[0]==3||row[0]==5)return{row[11],0x0100007f,row[23]};return{0x1220000,0x0100007f,row[23]};}
 void setup(){serial=reads=writes=calls=allocs=sleeps=0;trace=1469598103934665603ull;frames.clear();std::fill(data.begin(),data.end(),0x5a);std::fill_n(image->resolve(0x10019880,0x426),0x426,0);for(auto[a,v]:std::vector<std::pair<U,U>>{{0x10019880,row[20]},{0x10019884,row[21]},{0x100198ac,row[22]},{0x100198b8,row[1]},{0x100198bc,2},{0x100199b0,row[3]},{0x10019afc,0x1240000},{0x10019af8,0x1260000},{0x10019c5c,0x1228000},{0x10019c68,row[17]},{0x10019b0c,0x1280000},{0x10019b10,0x1280000},{0x10019b14,0x1280000},{0x1000605c,0x1100100},{0x10006068,0x1100b00},{0x10006010,0x1100600},{0x10007698,0}})put(a,v);
 std::fill_n(ptr(0x1240000,0x10000),0x10000,0);std::fill_n(ptr(0x1260000,0x1000),0x1000,0);std::fill_n(ptr(0x1280000,0x10000),0x10000,0);
 for(U i=0;i<row[1];++i){const U a=0x1240000+i*0xbc;put(a+0x9c,row[2]==2||((row[2]==1||row[2]==3)&&i==0));put(a+0x98,row[2]==3?1:0);put(a+0xa0,0x10000+i);put(a+0xa4,row[19]==2?0x1260000:0x1260000+i*0x18);put(a+0x80,row[7]?0x1204000+i*0x100:0);put(a+0x84,7);}
 for(U i=0;i<row[1];++i){const U a=0x1260000+i*0x18;put(a,row[19]?1:0);put(a+4,0x0100007f+i);put(a+0xc,1);put(a+0x14,row[23]);}put(0x1220004,row[6]);put(0x1221004,0x10002);
 }
 dfre::MissionNetsockDestroyPlayerBindings bindings(){dfre::MissionNetsockDestroyPlayerBindings b;
 b.read=[&](U a,U n){if(std::getenv("DFRE_TRACE"))std::cerr<<"R "<<norm(a)<<" "<<n<<"\n";event(1);event(norm(a));event(n);if(++reads==row[12]||!mapped(a,n))throw std::runtime_error("Read failure");if((row[15]&4)&&a==0x12400a0)put(0x1220004,0x10000);if((row[15]&32)&&a==0x100199b0&&frame(0x10001b80))put(frame(0x10001b80)+4,0x10002);if((row[15]&64)&&a==0x10019bfc)put(0x10006010,0x1100c00);return get(a,n);};
 b.write=[&](U a,U n,U v){if(std::getenv("DFRE_TRACE"))std::cerr<<"W "<<norm(a)<<" "<<n<<" "<<norm(v)<<"\n";event(2);event(norm(a));event(n);event(norm(n==4?v:v&((1u<<(8*n))-1)));if(++writes==row[13]||!mapped(a,n))throw std::runtime_error("Write failure");if((row[15]&16)&&a==0x1228004)put(0x10019c5c,0x1229000);put(a,v,n);if((row[15]&128)&&a==0x1240000)put(0x100198b8,1);};
 b.before_call=[&](U s,const auto&args){if(std::getenv("DFRE_TRACE")){std::cerr<<"C "<<s;for(U v:args)std::cerr<<" "<<norm(v);std::cerr<<"\n";}event(3);event(s);event(U(args.size()));for(U v:args)event(norm(v));if(++calls==row[14])throw std::runtime_error("Call failure");if((row[15]&1)&&s==0x100019e0&&frame(0x10001b10))put(frame(0x10001b10)+4,0x10002);if((row[15]&2)&&s==0x100041d0&&frame(0x10004550))put(frame(0x10004550)+4,0x1221000);if((row[15]&8)&&s==0x10002b80){put(0x12400a4,0x1260018);if(frame(0x10004550)){put(frame(0x10004550)+8,0x0200007f);put(frame(0x10004550)+0xc,0x77771234);}}if(s==0x100042e0)throw std::runtime_error("Required log fixture");};
 b.call=[&](U s,const auto&)->U{switch(s){case 0x1100100:return 0x12a0000+allocs++*0x10000;case 0x1100b00:return 0;case 0x1100600:case 0x1100c00:if(++sleeps>=row[10])put(0x10019bfc,1);if(sleeps>10)throw std::runtime_error("Sleep fixture limit");return 0;case 0x10005020:return row[9];default:throw std::runtime_error("Required destroy service is not bound");}};
 b.frame=[&](U s,U n,const auto&args){return make(s,n,args);};b.release=[&](U,U,U){frames.pop_back();};b.resolve=[&](U a,std::size_t n){return image->resolve(a,n);};return b;}
};}
int main(int argc,char**argv){if(argc!=2)return 1;std::ifstream file(argv[1],std::ios::binary);dfre::AssetBytes raw((std::istreambuf_iterator<char>(file)),{});Test t(raw);std::string line;while(std::getline(std::cin,line)){if(line.empty())continue;std::istringstream in(line);for(U&v:t.row)if(!(in>>v))return 2;t.setup();auto b=t.bindings();const U op=t.row[0],f=t.make(sites[op],sizes[op],t.args());bool failed=false;U result=0;try{result=dfre::mission_netsock_destroy_player_call(b,sites[op],f);}catch(const std::runtime_error&){failed=true;}if(t.frames.size()!=1)return 3;if(failed||op>=3)result=0;std::uint64_t state=1469598103934665603ull;for(auto[a,n]:std::vector<std::pair<U,U>>{{0x10019880,0x426},{0x1200000,0x6000},{0x1220000,0x10000},{0x1240000,0x10000},{0x1260000,0x1000},{0x1280000,0x1000},{0x12a0000,0x20000}}){const auto*p=t.image->resolve(a,n);for(U i=0;i<n;++i){state^=p[i];state*=1099511628211ull;}}std::cout<<failed<<' '<<result<<' '<<t.reads<<' '<<t.writes<<' '<<t.calls<<' '<<t.trace<<' '<<state<<'\n';}}
