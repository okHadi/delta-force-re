#include "dfre/mission_netsock_queue.hpp"
#include <array>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
namespace {using U=std::uint32_t;
struct Fixture {
 std::array<U,16> row{};std::map<U,std::uint8_t> memory;U reads=0,writes=0,calls=0,sleeps=0;std::uint64_t hash=1469598103934665603ull;
 static constexpr U frame=0x1300000,queue=0x10019b20,other=0x10019c38;
 U tail()const{return 0x1200000+(row[4]?0xfff0:0);}
 U get(U at,U width=4)const{U value=0;for(U i=0;i<width;++i){const auto p=memory.find(at+i);if(p!=memory.end())value|=U(p->second)<<(i*8);}return value;}
 void put(U at,U value,U width=4){for(U i=0;i<width;++i)memory[at+i]=std::uint8_t(value>>(i*8));}
 U norm(U at)const{return at>=frame&&at<frame+0x1c?0x30000000u+at-frame:at;}
 void event(U value){for(U i=0;i<4;++i){hash^=std::uint8_t(value>>(8*i));hash*=1099511628211ull;}}
 void setup(){for(U i=0;i<0x10000;++i){put(0x1200000+i,row[0]==0?0xa5:0,1);put(0x1220000+i,0,1);}for(U i=0;i<0x400;++i){put(0x1211000+i,(i+row[13])&255,1);put(0x1211800+i,(i*3+row[13])&255,1);put(0x1212000+i,0xcc,1);}put(0x10006010,0x1100100);put(0x1000605c,0x1100200);put(0x10006068,0x1100300);for(U q:{queue,other}){put(q,row[14]?0:0x1200000);put(q+4,0x1200000);put(q+8,tail());put(q+0xc,row[2]);put(q+0x10,row[6]?1:0);}if(row[0]!=0){put(tail(),row[5]?0x1210000:0);put(tail()+4,0x12345678);put(tail()+8,5);put(tail()+0xc,7);if(row[15])put(0x120fff0,0x1210080);}if(row[0]==1){put(frame+4,row[12]==2?tail()+4:0x1211000);put(frame+8,row[1]);put(frame+0xc,queue);put(frame+0x10,0x01020304);put(frame+0x14,0x11223344);put(frame+0x18,row[3]);}else put(frame+4,queue);}
 dfre::MissionNetsockQueueBindings bindings(){dfre::MissionNetsockQueueBindings b;
 b.read=[&](U at,U width){event(1);event(norm(at));event(width);if(++reads==row[8])throw std::runtime_error("Read failure");if(row[7]&64&&at==frame+4&&row[0]==1)put(frame+8,0xffffffffu);if(row[7]&16&&(at==0x1211000||at==0x1211800))put(queue+0xc,row[2]^0xffffffffu);return get(at,width);};
 b.write=[&](U at,U width,U value){event(2);event(norm(at));event(width);event(norm(value));if(++writes==row[9]||at<0x1000)throw std::runtime_error("Store failure");if(row[7]&4&&at==tail()+4)put(tail(),0x1210080);if(row[7]&32&&at==queue+8)put(queue+0xc,0x13579bdf);if(row[7]&512&&at==queue)put(frame+4,other);put(at,value,width);};
 b.before_call=[&](U site,const std::vector<U>& args){event(3);event(site);event(U(args.size()));for(auto n:args)event(norm(n));if(++calls==row[10])throw std::runtime_error("Call failure");};
 b.call=[&](U site,const auto&)->U{if(site==0x1100100){++sleeps;if(row[7]&256)put(0x10006010,0x1100400);if(sleeps>=row[6])put(queue+0x10,0);return 0;}if(site==0x1100200){if(row[7]&1)put(frame+4,row[0]==0?other:0x1211800);if(row[7]&8){put(queue,0x1220000);put(queue+0xc,row[2]^0x80000000u);}if(row[11])return 0;return row[0]==0?0x1200000u:row[12]==1?0x1211001u:0x1212000u;}if(site==0x1100300){if(row[7]&2)put(0x10006068,0x1100400);if(row[7]&128)put(queue,0x1220000);return 0;}if(site==0x10003aa0)return 0;throw std::runtime_error("Required body is not bound");};return b;}
};}
int main(){std::string line;while(std::getline(std::cin,line)){if(line.empty())continue;Fixture f;std::istringstream in(line);for(auto& n:f.row)if(!(in>>n))return 1;f.setup();auto b=f.bindings();bool failed=false;try{if(f.row[0]==0)dfre::mission_netsock_queue_initialize(b,f.frame);if(f.row[0]==1)dfre::mission_netsock_queue_enqueue(b,f.frame);if(f.row[0]==2)dfre::mission_netsock_queue_destroy(b,f.frame);}catch(const std::runtime_error&){failed=true;}std::uint64_t state=1469598103934665603ull;for(auto [at,size]:std::array<std::pair<U,U>,7>{{{f.queue,20},{f.other,20},{0x1200000,64},{0x120ffc0,64},{0x1220000,64},{0x122ffc0,64},{0x1210000,0x3000}}})for(U i=0;i<size;++i){state^=f.get(at+i,1);state*=1099511628211ull;}std::cout<<failed<<' '<<f.reads<<' '<<f.writes<<' '<<f.calls<<' '<<f.hash<<' '<<state<<'\n';}}
