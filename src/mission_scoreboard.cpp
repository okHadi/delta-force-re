#include "dfre/mission_scoreboard.hpp"
#include <stdexcept>

namespace dfre {namespace {
using U=std::uint32_t;
std::int32_t si(U n){return n<=0x7fffffffu?std::int32_t(n):std::int32_t(std::int64_t(n)-0x100000000LL);}
}
void mission_scoreboard_receive(const MissionScoreboardBindings& b,U frame){
    const auto read=[&](U at,U width=4){if(!b.read)throw std::runtime_error("Scoreboard current read is not bound");return b.read(at,width);};
    const auto write=[&](U at,U value,U width=4){if(!b.write)throw std::runtime_error("Scoreboard current write is not bound");b.write(at,width,value);};
    write(frame+0x5c,read(frame+0x44));
    const U packet=read(frame+0x5c);
    write(0xbcfbe8,read(packet+0x8e,1));
    write(frame+0x68,0);
    const auto mode=read(packet+0x8d,1),initial_count=read(0xbcfbe8);
    write(0xbcfbe4,mode);
    if(si(initial_count)>0){
        U destination=0xbcf664,offset=0;
        write(frame+0x60,packet);write(frame+0x64,packet);
        for(;;){
            const auto row=read(frame+0x60),actor_index=read(row+0x0d,1);
            const auto person=read(0x7cca8c+actor_index*0x114);
            U name=person+4;
            if(!person){if(!b.call)throw std::runtime_error("Scoreboard name lookup is not bound");name=b.call(0x404cf0,{0x4ccccc,0x4cccc0});}
            U to=destination;
            for(;;){const auto first=read(name,1);write(to,first,1);if(!first)break;const auto second=read(name+1,1);name+=2;write(to+1,second,1);to+=2;if(!second)break;}
            const auto values=read(frame+0x64),current_row=read(frame+0x60);
            const auto score=U(si(read(values+0x4b))>>16),next_values=read(frame+0x64);
            destination+=0x2c;write(0xbcf684+offset,score);
            offset+=0x2c;write(0xbcf65c+offset,read(current_row+0x0d,1));
            const auto side=read(current_row+0x2d,1);
            write(frame+0x60,current_row+1);write(0xbcf660+offset,side);
            const auto count=read(frame+0x68);
            write(frame+0x64,next_values+2);
            const U next=count+1,limit=read(0xbcfbe8);
            write(frame+0x68,next);
            if(si(next)>=si(limit))break;
        }
    }
    const auto pairs=read(frame+0x5c);
    for(U i=0;i<3;++i){
        const auto n=read(pairs+i*2,2);
        write(0xbcfbf0+i*4,n&0x8000?n|0xffff0000u:n);
        write(0xbcfbfc+i*4,read(pairs+i+9,1));
        write(0xbcfc08+i*4,read(pairs+i+6,1));
    }
}
}
