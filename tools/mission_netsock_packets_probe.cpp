#include "dfre/mission_netsock_packets.hpp"
#include <array>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>

namespace {
using U=std::uint32_t;
struct Fixture {
    std::array<U,15> row{};std::map<U,std::uint8_t> memory;
    struct Frame{U at,site,size;};std::vector<Frame> frames;U depth=0,reads=0,writes=0,calls=0,sleeps=0;
    std::uint64_t hash=1469598103934665603ull;
    U queue()const{return row[2]==1?0x10019b0c:0x10019c24;}
    U head()const{return 0x1200000+(row[13]?0xfff0:0);}
    U get(U at,U width=4)const{U value=0;for(U i=0;i<width;++i){const auto p=memory.find(at+i);if(p!=memory.end())value|=U(p->second)<<(i*8);}return value;}
    void put(U at,U value,U width=4){for(U i=0;i<width;++i)memory[at+i]=std::uint8_t(value>>(i*8));}
    U norm(U at)const{for(std::size_t i=0;i<frames.size();++i)if(at>=frames[i].at&&at<frames[i].at+frames[i].size)return 0x30000000u+U(i)*0x100u+(at-frames[i].at);return at;}
    void event(U value){for(U i=0;i<4;++i){hash^=std::uint8_t(value>>(i*8));hash*=1099511628211ull;}}
    U make(U site,U size,const std::vector<U>& args){const U at=0x1300000u+U(frames.size())*0x100u;frames.push_back({at,site,size});++depth;for(U i=0;i<size;++i)put(at+i,0x5a,1);U offset=(site==0x10001be0||site==0x10001c30)?8:4;if(site==0x4b97a0)offset=0x94;for(auto n:args){put(at+offset,n);offset+=4;}if(site==0x4b97a0)put(at+0x87,0,1);return at;}
    void setup(){for(U i=0;i<0x400;++i){put(0x1211000+i,(i+row[14])&255,1);put(0x1211800+i,(i*3+row[14])&255,1);put(0x1212000+i,0xcc,1);put(0x1212800+i,0xdd,1);}for(U i=0;i<0x178;++i)put(0x1214000+i,(i+row[14])&255,1);for(U i=0;i<6;++i){put(0x1214000+i,U("ALPHA"[i]),1);put(0x12140bc+i,U("BRAVO"[i]),1);}put(0x10019880,row[1]&1);put(0x10019884,row[1]&2);put(0x100198ac,row[1]&4);put(0x100199b0,row[2]);put(0x100198b8,2);put(0x10019afc,0x1214000);put(0x121409c,row[11]);put(0x1214158,row[11]);put(0x501320,row[1]&1);put(0x10006010,0x1100100);put(0x10006068,0x1100200);put(queue(),0x1200000);put(queue()+4,head());put(queue()+8,row[4]==0?head():0x1200030);put(queue()+0xc,row[4]);put(queue()+0x10,row[5]?1:0);put(head(),0x1211000);put(head()+4,row[3]);put(head()+8,17);put(head()+0xc,0x1211800);put(0x1213000,1);put(0x1213004,0xdeadbeef);put(0x1213008,0xcafebabe);}
    dfre::MissionNetsockPacketBindings bindings(){dfre::MissionNetsockPacketBindings b;
        b.read=[&](U at,U width){event(1);event(norm(at));event(width);if(++reads==row[7])throw std::runtime_error("Read failure");if((row[6]&8)&&at==queue()+4)put(queue()+8,head());if((row[6]&32)&&at==0x100198ac)put(frames[0].at+(row[0]==2?8:4),0x1212800);if((row[6]&64)&&(at==0x121409c||at==0x1214158)){for(const auto& f:frames)if(f.site==0x10001a20)put(f.at+8,0x1212800);}if((row[6]&128)&&at==head())put(head(),0x1211800);return get(at,width);};
        b.write=[&](U at,U width,U value){event(2);event(norm(at));event(width);event(norm(value));if(++writes==row[8])throw std::runtime_error("Store failure");if(row[6]&2)for(const auto& f:frames)if(f.site==0x10003c00&&at==get(f.at+8))put(head()+4,row[3]+1);put(at,value,width);};
        b.before_call=[&](U site,const std::vector<U>& args){event(3);event(site);event(U(args.size()));for(auto n:args)event(norm(n));if(++calls==row[9])throw std::runtime_error("Call failure");};
        b.frame=[&](U site,U size,const auto& args){return make(site,size,args);};b.release=[&](U,U,U){--depth;};
        b.call=[&](U site,const std::vector<U>& args)->U{if(site==0x1100100){++sleeps;if(row[6]&1)put(0x10006010,0x1100300);if(sleeps>=row[5])put(queue()+0x10,0);return 0;}if(site==0x1100200){if(row[6]&4){put(queue()+0xc,0);for(const auto& f:frames)if(f.site==0x10003c00)put(f.at+0xc,0x1200040);}return 0;}if(site==0x10003aa0)return 0;if(site==0x4c5520){const auto frame=make(0x10001aa0,0xc,args);try{const auto result=dfre::mission_netsock_player_name(bindings(),frame);--depth;return result;}catch(...){--depth;throw;}}throw std::runtime_error("Required body is not bound");};return b;
    }
};
}
int main(){std::string line;while(std::getline(std::cin,line)){if(line.empty())continue;Fixture f;std::istringstream in(line);for(auto& n:f.row)if(!(in>>n))return 1;f.setup();auto b=f.bindings();U result=0;bool failed=false;U destination=f.row[12]==1?0x1211001:0x1212000;U address=f.row[12]==3?f.head()+4:0x1213004,port=f.row[12]==4?f.head():0x1213008;U length=f.row[12]==2?f.head()+4:0x1213000;
 const U site=f.row[0]==0?0x10001c80:f.row[0]==1?0x10003c00:f.row[0]==2?0x10001aa0:0x4b97a0;const U size=f.row[0]==0?0x10:f.row[0]==1?0x18:f.row[0]==2?0xc:0x98;std::vector<U> args=f.row[0]==0?std::vector<U>{destination,length,address}:f.row[0]==1?std::vector<U>{destination,length,f.queue(),address,port}:f.row[0]==2?std::vector<U>{f.row[10],destination}:std::vector<U>{f.row[10]};const auto frame=f.make(site,size,args);
 try{if(f.row[0]==0)result=dfre::mission_netsock_get_message(b,frame);if(f.row[0]==1)result=dfre::mission_netsock_queue_pop(b,frame);if(f.row[0]==2)result=dfre::mission_netsock_player_name(b,frame);if(f.row[0]==3)result=dfre::mission_df_netsock_player_name(b,frame);}catch(const std::runtime_error&){failed=true;}--f.depth;if(f.depth)return 2;
 std::uint64_t state=1469598103934665603ull;for(auto [base,size]:std::array<std::pair<U,U>,5>{{{f.queue(),20},{f.head(),16},{0x1211000,0x1400},{0x1212800,0x900},{0xd0a500,0x100}}})for(U i=0;i<size;++i){state^=f.get(base+i,1);state*=1099511628211ull;}
 std::cout<<failed<<' '<<result<<' '<<f.reads<<' '<<f.writes<<' '<<f.calls<<' '<<f.hash<<' '<<state<<'\n';}}
