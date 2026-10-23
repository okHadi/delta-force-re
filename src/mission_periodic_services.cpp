#include "dfre/mission_periodic_services.hpp"
#include "dfre/mission_round_services.hpp"
#include <cstring>
#include <stdexcept>

namespace dfre {namespace {
using U=std::uint32_t;
std::int32_t si(U value){std::int32_t out;std::memcpy(&out,&value,4);return out;}
struct Access {
    const MissionPeriodicBindings& b;
    U read(U at,U width=4)const{if(!b.read)throw std::runtime_error("Periodic current read is not bound");const auto value=b.read(at,width);return width==1?value&255u:width==2?value&65535u:value;}
    void write(U at,U value,U width=4)const{if(!b.write)throw std::runtime_error("Periodic current store is not bound");b.write(at,width,value);}
    U call(U site,std::vector<U> args={})const{if(!b.call)throw std::runtime_error("Periodic required body is not bound");return b.call(site,args);}
};
}
void mission_periodic_team_scores(const MissionPeriodicBindings& b){
    Access a{b};MissionRoundServiceBindings round;
    round.client=[&](U slot,U offset,U width){return a.read(0x6e9a90+slot*0x9b84+offset,width);};
    round.squad=[&](U slot,U offset,U width){return a.read(0x6e8090+slot*0x68+offset,width);};
    round.squad_store=[&](U slot,U offset,U width,U value){a.write(0x6e8090+slot*0x68+offset,value,width);};
    mission_round_collect_squads(round);
}
std::int32_t mission_periodic_compare_clients(const MissionPeriodicBindings& b,U first,U second){
    Access a{b};MissionRoundServiceBindings round;
    round.cell=[&](U at,U width){return a.read(at,width);};
    round.client=[&](U address,U offset,U width){return a.read(address+offset,width);};
    return mission_round_compare_clients(round,first,second);
}
void mission_periodic_poll_clients(const MissionPeriodicBindings& b){
    Access a{b};for(U client=0x6f3614;client!=0x78ee54;client+=0x9b84)if(a.read(client+0x18c,1)&&a.read(client+0x1c4,2)==5&&a.read(client+8))a.call(0x441750,{client,0x4a,0,0});
}
void mission_periodic_clear_actor_flags(const MissionPeriodicBindings& b){
    Access a{b};for(const auto pair:{std::pair<U,U>{0x7cca80,0x8cfa18},std::pair<U,U>{0x7dde80,0x8cfa1c}}){if(si(a.read(pair.second))<=0)continue;U actor=pair.first,slot=0;for(;;){const U flags=a.read(actor+0x6d,1);a.write(actor+0x6d,flags&0xbf,1);++slot;actor+=0x114;if(si(slot)>=si(a.read(pair.second)))break;}}
}
void mission_periodic_publish_flag(const MissionPeriodicBindings& b,U frame){
    Access a{b};if(!a.read(0x67f528)||a.read(0x67f53c)!=5)return;
    U actor=0x7dde80,slot=0,match=0;const U count=a.read(0x8cf9a0);
    if(si(count)>0)for(;;){const U definition=a.read(actor+4);if(definition){const U id=a.read(definition+0x20);if(id==0xfc4||id==0xfc3){if(match==a.read(0x4f8218)){a.call(0x49a620,{frame,0,15});a.write(frame+12,slot,2);a.write(frame+14,a.read(actor+0x6c,1),1);a.write(frame,a.read(actor+0x24));a.write(frame+4,a.read(actor+0x28));a.write(frame+8,a.read(actor+0x2c));a.call(0x4423f0,{0x48,frame,15});const U current=a.read(0x4f8218);a.write(0x4f8218,current+1);return;}++match;}}
        const U current=a.read(0x8cf9a0);++slot;actor+=0x114;if(si(slot)>=si(current))break;
    }
    a.write(0x4f8218,0);
}
void mission_periodic_scoreboard(const MissionPeriodicBindings& b,U frame){
    Access a{b};a.call(0x49a620,{frame,0,0x8f});a.call(0x446da0);
    if(a.read(0x67f53c)==4){a.write(frame+2,a.read(0x6e813c,2),2);a.write(frame+4,a.read(0x6e81a4,2),2);a.write(frame+7,a.read(0x6e8140,1),1);const U value=a.read(0x6e81a8,1);a.write(frame+0x118,1);a.write(frame+8,value,1);}
    else{const U second_score=a.read(0x6e81ac,2);const U second_loss=a.read(0x6e81c0,2);const U first_score=a.read(0x6e8144,2);const U first_loss=a.read(0x6e8158,2);const U first_penalty=a.read(0x6e8154,2);const U type=a.read(0x67f53c);const U second_penalty=a.read(0x6e81bc,2);const U first=first_score-first_loss-(first_penalty<<2),second=second_score-second_loss-(second_penalty<<2);
        if(type==5){a.write(frame+2,a.read(0x6e815c,2),2);a.write(frame+4,a.read(0x6e81c4,2),2);a.write(frame+10,a.read(0x6e5764,1),1);const U value=a.read(0x6e5768,1);a.write(frame+0x118,1);a.write(frame+11,value,1);}
        else{a.write(frame+2,first,2);a.write(frame+4,second,2);a.write(frame+0x118,type==2?1:0);}}
    a.write(frame+0x12c,0);U offset=0;
    for(U slot=0;slot<17;++slot){const U client=0x6e9a90+slot*0x9b84;if(!a.read(client+0x18c,1)||a.read(client+0x1c4,2)!=5)continue;
        const U type=a.read(0x67f53c);U score;if(type==3||type==4)score=a.read(client+0x194);else{const U value=a.read(client+0x1a0);const U penalty=a.read(client+0x1b0);const U loss=a.read(client+0x1b4);score=value-loss-(penalty<<2);}
        a.write(frame+0x90+offset,score);const U count=a.read(frame+0x12c);offset+=4;a.write(frame+0xd0+offset,slot);a.write(frame+0x12c,count+1);
    }
    const U count=a.read(frame+0x12c);a.write(frame+0x120,0);a.write(frame+0x128,0);a.write(frame+0x11c,count-1);
    if(si(count-1)>0){a.write(frame+0x124,a.read(frame+0x12c)<<2);
        for(;;){U next=a.read(frame+0x120)+1;const U current=a.read(frame+0x12c);if(si(next)<si(current)){const U left=a.read(frame+0x128);const U stop=a.read(frame+0x124);U right=next*4;
                for(;;){const U second=0x6e9a90+a.read(frame+0xd4+right)*0x9b84;const U first=0x6e9a90+a.read(frame+0xd4+left)*0x9b84;
                    if(si(a.call(0x448130,{first,second}))<0){const U r=a.read(frame+0x90+right);U l=a.read(frame+0x90+left)^r;a.write(frame+0x90+left,l);const U rr=a.read(frame+0x90+right)^l;a.write(frame+0x90+right,rr);const U ll=a.read(frame+0x90+left)^rr;const U rs=a.read(frame+0xd4+right);const U ls=a.read(frame+0xd4+left)^rs;a.write(frame+0xd4+left,ls);a.write(frame+0x90+left,ll);const U rs2=a.read(frame+0xd4+right)^ls;a.write(frame+0xd4+right,rs2);a.write(frame+0xd4+left,a.read(frame+0xd4+left)^rs2);}
                    right+=4;++next;if(si(right)>=si(stop))break;
                }}
            const U before=a.read(frame+0x128);const U outer=a.read(frame+0x120);const U last=a.read(frame+0x11c);a.write(frame+0x128,before+4);a.write(frame+0x120,outer+1);if(si(outer+1)>=si(last))break;
        }}
    a.write(frame+0x8e,a.read(frame+0x12c,1),1);U slot=0,score_offset=0,word_offset=0;
    while(si(slot)<si(a.read(frame+0x8e,1))){const U client=0x6e9a90+a.read(frame+0xd4+score_offset)*0x9b84;const U actor=a.read(client+8);const U index=(actor-0x7cca80u)/0x114u;word_offset+=2;a.write(frame+13+slot,index,1);const U score=a.read(frame+0x90+score_offset,2);++slot;a.write(frame+0x4b+word_offset,score,2);const U id=a.read(frame+0xd4+score_offset,1);score_offset+=4;a.write(frame+0x2c+slot,id,1);}
    if(a.read(frame+0x118))a.write(frame+0x8d,a.read(frame+0x8d,1)|1,1);
    if(a.read(0x67f53c)==3)a.write(frame+0x8d,a.read(frame+0x8d,1)|2,1);
    a.call(0x4423f0,{0x15,frame,0x8f});
}
void mission_periodic_advertise(const MissionPeriodicBindings& b,U frame){
    Access a{b};if(a.read(0x67f524)!=4||a.read(0x6e5590))return;
    const auto copy=[&](U to,U from){for(;;){const U byte=a.read(from,1);a.write(to,byte,1);if(!byte)return;++to;++from;}};
    const auto length=[&](U from){U result=0;while(a.read(from+result,1))++result;return result;};
    const auto localized=[&](U key,U to){copy(to,a.call(0x404cf0,{0x4cc4ec,key}));};
    const U type=a.read(0x67f53c);if(type<=5){constexpr U keys[]{0x4cc4e0,0x4cc4f8,0x4cc504,0x4cc510,0x4cc51c,0x4cc528};localized(keys[type],frame+0x460);}
    const U theme=a.read(0x8cf97c);if(theme<=2){constexpr U keys[]{0x4cc534,0x4cc540,0x4cc54c};localized(keys[theme],frame+0x480);}
    U active=0;for(U client=0x6e9a90;client!=0x78ee54;client+=0x9b84)if(a.read(client+0x18c,1))++active;
    if(!a.read(0x67f52c))--active;
    copy(frame+0x440,length(0x67f498)?0x6e573c:0x6e571c);
    a.call(0x446110,{frame+0x400,0x67f478});
    const U included=a.read(0x67f52c);U maximum=a.read(0x67f530);if(!included)--maximum;if(si(maximum)>16)maximum=16;
    localized(a.read(0x67f52c)?0x4cc564:0x4cc558,frame+0x4e0);
    const U clock=a.read(0x67dbe8);if(si(clock)<0)localized(0x4cc574,frame+0x4a0);else{const U minutes=U((si(clock<<4)/1000)/60);a.call(0x493a80,{frame+0x4a0,0x4cc570,minutes});}
    localized(length(0x67f498)?0x4cc558:0x4cc564,frame+0x4c0);
    localized(a.read(0x67f548,1)&1?0x4cc564:0x4cc558,frame+0x4d0);
    a.call(0x446110,{frame+0x420,0x67f4dc});
    const U limit=a.read(0x6e55a8);
    a.call(0x493a80,{frame,0x4cc580,frame+0x440,frame+0x400,frame+0x460,frame+0x480,active,maximum,limit,frame+0x4e0,frame+0x4a0,frame+0x4c0,frame+0x4d0,frame+0x420});
    const U port=a.read(0x6e5760,2);const U address=a.read(0x6e575c);const U bytes=length(frame);a.call(0x4550c0,{frame,bytes,address,port});
}
} // namespace dfre
