#include "dfre/mission_periodic_services.hpp"
#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
namespace {
using U=std::uint32_t;
constexpr U stack=0x2008000,definition=0x1210000,text=0x13f0000;
struct Fixture {
    std::array<U,6> row{};U frame=0,frame_size=0,sequence=0,reads=0,writes=0,calls=0;
    std::vector<std::uint8_t> data=std::vector<std::uint8_t>(0xa00000),heap=std::vector<std::uint8_t>(0x300000),stack_bytes=std::vector<std::uint8_t>(0x10000);
    std::string trace;
    std::uint8_t& byte(U at){if(at>=0x400000&&at<0xe00000)return data[at-0x400000];if(at>=0x1100000&&at<0x1400000)return heap[at-0x1100000];if(at>=0x2000000&&at<0x2010000)return stack_bytes[at-0x2000000];throw std::runtime_error("Fixture address has no owner");}
    U get(U at,U width=4){U value=0;for(U i=0;i<width;++i)value|=U(byte(at+i))<<(8*i);return value;}
    void put(U at,U value,U width=4){for(U i=0;i<width;++i)byte(at+i)=std::uint8_t(value>>(8*i));}
    U next(){sequence=sequence*1664525+1013904223;return sequence;}
    void string(U at,const std::string& value){for(char c:value)byte(at++)=std::uint8_t(c);byte(at)=0;}
    void initialize(){std::fill(data.begin(),data.end(),0);std::fill(heap.begin(),heap.end(),0);std::fill(stack_bytes.begin(),stack_bytes.end(),0);sequence=row[1]?row[1]:1;reads=writes=calls=0;trace.clear();frame=row[0]==3?stack-0x500:row[0]==4?stack-0x20:stack-0x140;frame_size=row[0]==0?0x130:row[0]==3?0x4f0:row[0]==4?0x10:0;
        for(U at=0x6e8090;at<0x6e9a90;at+=4)put(at,next());for(U i=0;i<17;++i){const U c=0x6e9a90+i*0x9b84;for(U at=0x194;at<=0x1bc;at+=4)put(c+at,next());put(c+8,(row[1]&64)?next():0x7cca80+i*3*0x114);put(c+0x9c,i%3);put(c+0x18c,i<row[1]%6,1);put(c+0x1c4,5,2);}
        for(U i=0;i<5;++i){const U d=definition+i*0x200;put(d+0x20,i%3==0?0xfc3:i%3==1?0xfc4:777);for(const U base:{0x7cca80u,0x7dde80u}){const U actor=base+i*0x114;put(actor+4,d);put(actor+0x24,next());put(actor+0x28,next());put(actor+0x2c,next());put(actor+0x6c,next(),1);put(actor+0x6d,next(),1);}}
        for(U i=0;i<0x4f0;i+=4)put(frame+i,next());put(0x67f53c,row[1]%8);put(0x67f524,row[1]&1?4:0);put(0x6e5590,row[1]&2?1:0);put(0x67f528,row[1]&32?0:1);put(0x67f52c,row[1]&8?0:1);put(0x8cf97c,(row[1]>>4)%4);put(0x67dbe8,next());put(0x4f8218,row[1]%4);put(0x6e5760,next());put(0x6e575c,next());put(0x6e55a8,next());put(0x67f530,next());put(0x67f548,next());put(0x8cfa18,row[1]%6-1);put(0x8cfa1c,row[1]%6-1);put(0x8cf9a0,row[1]%6-1);if(row[1]&0x100)for(const U off:{0x194u,0x1a0u,0x1b0u,0x1b4u,0x1bcu})put(0x6f3614+off,get(0x6e9a90+off));if(row[1]&0x200)put(0x6f3614+0x1ac,get(0x6e9a90+0x1ac));if(row[1]&0x400)put(0x6f3614+0x1a8,get(0x6e9a90+0x1a8));if(row[1]&0x800)put(0x6f3614+8,0);if(row[1]&0x1000)put(0x6e9a90+0x18c,0x80,1);if(row[1]&0x2000)put(0x6e9a90+0x1c4,0x8005,2);string(0x67f498,row[1]&16?"pw":"");string(0x6e573c,"named");string(0x6e571c,"default");string(0x67f478,"addr");string(0x67f4dc,"route");string(text,"text");
    }
    dfre::MissionPeriodicBindings bindings(){return {
        [&](U at,U width){trace+='R'+std::to_string(at)+','+std::to_string(width)+';';if(++reads==row[3])throw std::runtime_error("Read failure");if((row[2]&8)&&reads==7)put(0x7dde80+0x24,get(0x7dde80+0x24)^0x12345678);return get(at,width);},
        [&](U at,U width,U value){if(width!=4)value&=width==1?255u:65535u;trace+='W'+std::to_string(at)+','+std::to_string(width)+','+std::to_string(value)+';';if(++writes==row[4])throw std::runtime_error("Store failure");if((row[2]&16)&&writes==4)put(0x8cfa18,1);put(at,value,width);},
        [&](U site,const std::vector<U>& args){trace+='C'+std::to_string(site);for(U value:args)trace+=','+std::to_string(value);trace+=';';if(++calls==row[5])throw std::runtime_error("Call failure");U result=0;
            if(site==0x49a620){for(U i=0;i<args.at(2);++i)byte(args.at(0)+i)=std::uint8_t(args.at(1));result=args.at(0);if(row[2]&1)put(0x67f53c,(get(0x67f53c)+1)%6);}
            else if(site==0x446da0){dfre::mission_periodic_team_scores(bindings());if(row[2]&2)put(0x67f53c,(row[1]>>5)%6);}
            else if(site==0x448130){result=U(dfre::mission_periodic_compare_clients(bindings(),args.at(0),args.at(1)));if(row[2]&4)put(0x67f53c,(get(0x67f53c)+1)%6);}
            else if(site==0x4423f0||site==0x441750){if(row[2]&32){put(0x4f8218,0xffffffff);put(0x6e9a90+0x9b84+0x18c,1,1);}}
            else if(site==0x404cf0)result=text;
            else if(site==0x446110)string(args.at(0),"net");
            else if(site==0x493a80)string(args.at(0),"formatted");
            else if(site!=0x4550c0)throw std::runtime_error("Unknown required fixture body");return result;}
    };}
    std::string snapshot(){static constexpr char hex[]="0123456789abcdef";std::string out;const auto append=[&](U at,U length){for(U i=0;i<length;++i){const U v=byte(at+i);out+=hex[v>>4];out+=hex[v&15];}};append(frame,frame_size);append(0x6e8090,0x1a00);for(U i=0;i<17;++i){append(0x6e9a90+i*0x9b84+0x18c,1);append(0x6e9a90+i*0x9b84+0x194,0x30);}append(0x7cca80,5*0x114);append(0x7dde80,5*0x114);for(const U at:{0x67f53cu,0x8cfa18u,0x8cfa1cu,0x4f8218u})append(at,4);return out;}
};
}
int main(){Fixture f;while(std::cin>>f.row[0]){for(std::size_t i=1;i<f.row.size();++i)std::cin>>f.row[i];f.initialize();bool failed=false;U result=0;try{const auto b=f.bindings();switch(f.row[0]){case 0:dfre::mission_periodic_scoreboard(b,f.frame);break;case 1:dfre::mission_periodic_poll_clients(b);break;case 2:dfre::mission_periodic_clear_actor_flags(b);break;case 3:dfre::mission_periodic_advertise(b,f.frame);break;case 4:dfre::mission_periodic_publish_flag(b,f.frame);break;case 5:dfre::mission_periodic_team_scores(b);break;case 6:result=U(dfre::mission_periodic_compare_clients(b,0x6e9a90,0x6f3614));break;default:throw std::runtime_error("Unknown probe mode");}}catch(const std::runtime_error&){failed=true;}std::cout<<failed<<' '<<f.reads<<' '<<f.writes<<' '<<f.calls<<' '<<result<<' '<<f.snapshot()<<' '<<f.trace<<'\n';}}
