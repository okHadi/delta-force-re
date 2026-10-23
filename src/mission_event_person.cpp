#include "dfre/mission_event_person.hpp"
#include <cstring>
#include <limits>
namespace dfre {namespace {
using U=std::uint32_t;using Q=std::uint64_t;
template<class F>const F& need(const F& f,const char* message){if(!f)throw MissionTickError(message);return f;}
std::int32_t si(U n){std::int32_t result;std::memcpy(&result,&n,4);return result;}
U read(const MissionEventPersonBindings& b,U at,U width=4){return need(b.read,"Person event read is not bound")(at,width);}
void write(const MissionEventPersonBindings& b,U at,U value,U width=4){need(b.write,"Person event write is not bound")(at,width,value);}
struct Binary {Q significand;int exponent;};
Binary rounded(Q numerator,Q denominator,unsigned precision,unsigned mode,bool negative){
    const Q integer=numerator/denominator;Q remainder=numerator%denominator;
    unsigned integer_bits=0;for(auto n=integer;n;n>>=1)++integer_bits;
    int bit=int(integer_bits)-1,exponent=bit;
    const auto fraction=[&](){const bool one=remainder>=denominator-remainder;remainder=one?remainder-(denominator-remainder):remainder+remainder;return one;};
    const auto next=[&](){if(bit>=0)return ((integer>>unsigned(bit--))&1)!=0;return fraction();};
    if(integer_bits==0){exponent=-1;while(!next())--exponent;}
    else (void)next();
    Q significand=1;for(unsigned n=1;n<precision;++n)significand=(significand<<1)|Q(next());
    const bool guard=next();const bool sticky=(bit>=0&&(integer&((Q(1)<<unsigned(bit+1))-1)))||remainder;
    const bool inexact=guard||sticky;
    const bool increment=mode==0?(guard&&(sticky||(significand&1))):mode==1?(negative&&inexact):mode==2?(!negative&&inexact):false;
    if(increment){if(significand==std::numeric_limits<Q>::max()){significand=Q(1)<<63;++exponent;}else{++significand;if(precision<64&&significand==(Q(1)<<precision)){significand>>=1;++exponent;}}}
    return {significand,exponent-int(precision)+1};
}
void width(U n){if(n!=1&&n!=2&&n!=4)throw MissionTickError("Person event memory width is invalid");}
}
U mission_event_person_rate(std::int32_t value,std::uint16_t control){
    if((control&0x3fu)!=0x3fu)throw MissionTickError("Person rate needs masked x87 exceptions");
    if(!value)return 0x80000000u;
    const auto pc=(control>>8)&3u,mode=(control>>10)&3u;if(pc==1)throw MissionTickError("Person rate x87 precision is reserved");
    const auto precision=pc==0?24u:pc==2?53u:64u;const bool negative=value<0;
    const Q magnitude=negative?Q(-std::int64_t(value)):Q(value);
    const auto first=rounded(magnitude,100,precision,mode,negative);
    auto second=rounded(1,first.significand,precision,mode,negative);second.exponent+=16-first.exponent;
    const auto amount=second.exponent<0?(second.exponent<=-64?Q(0):second.significand>>unsigned(-second.exponent)):second.significand<<unsigned(second.exponent);
    return negative?U(0)-U(amount):U(amount);
}
void mission_event_person_edit(const MissionEventPersonBindings& b,U action,U actor){
    const auto kind=read(b,action+8);if(kind==0||kind>15)return;
    const auto person=read(b,actor+0xc);if(!person)return;
    if(kind==5||kind==6){write(b,person+0x9c,kind==5?2:0,1);return;}
    const auto value=read(b,action+0x10);
    if(kind==1){write(b,person+0x64,read(b,0x4fb2ccu+value*4));return;}
    if(kind==2||kind==3||kind==4||kind==14||kind==15){
        const U offset=kind==14?0x25:0x24,mask=kind==2?2:kind==3?0x10:kind==4?0x20:1;
        const auto previous=read(b,person+offset,1);write(b,person+offset,value?(previous|mask):(previous&~mask),1);return;
    }
    if(kind==7){write(b,read(b,actor+0xc)+0x98,U(si(value<<16)/360));return;}
    if(kind==8){if(value){const auto operand=read(b,action+0x10);const auto control=need(b.fp_control,"Person event x87 control is not bound")();write(b,person+0x70,mission_event_person_rate(si(operand),control));}return;}
    if(kind==9){write(b,person+0x74,U(si(value<<16)/100));return;}
    if(kind==10||kind==11){write(b,person+(kind==10?0x88:0x8c),value*62);return;}
    if(kind==12){write(b,read(b,actor+0xc)+0x94,value*62);return;}
    if(kind==13)write(b,person+0x7c,U(si(value<<8)/360));
}
void mission_event_person_edits(const MissionEventPersonBindings& b,U action){
    if(read(b,action+4)!=3||!read(b,action+8))return;
    const auto side=read(b,action+0xc);if(!side)return;
    for(const auto group:{1u,3u,0u}){
        const U count=group==1?0x8cf904:group==3?0x8cfa18:0x8cfa1c;
        if(si(read(b,count))<=0)continue;
        U slot=0,actor=mission_actor_source_address(std::uint8_t(group),0);
        for(;;){
            if(U(si(read(b,actor+0x70))>>16)==side)mission_event_person_edit(b,action,actor);
            const auto bound=read(b,count);++slot;actor+=0x114;
            if(si(slot)>=si(bound))break;
        }
    }
}
MissionEventPersonBindings mission_event_person_bindings(MissionTickState& state,EventTableRecord& action,U action_source,std::function<U(U)> table,std::function<std::uint16_t()> fp){
    MissionEventPersonBindings b;b.fp_control=std::move(fp);
    b.read=[&state,&action,action_source,table=std::move(table)](U address,U n){
        width(n);
        if(address>=action_source&&Q(address-action_source)+n<=32){U result=0;for(U i=0;i<n;++i){const auto offset=address-action_source+i;result|=((action[offset/4]>>((offset%4)*8))&255u)<<(8*i);}return result;}
        if(address==0x8cf904||address==0x8cfa18||address==0x8cfa1c){if(n!=4)throw MissionTickError("Person event count width is invalid");const auto group=address==0x8cf904?1u:address==0x8cfa18?3u:0u;const auto size=state.tables.groups[group].size();if(size>(group==3?256u:1024u))throw MissionTickError("Person event actor count exceeds its physical table");return U(size);}
        constexpr std::array<U,4> caps{1024,1024,512,256};
        for(std::uint8_t group=0;group<4;++group){const auto base=mission_actor_source_address(group,0);if(address<base)continue;const auto delta=address-base,slot=delta/0x114,offset=delta%0x114;if(slot>=caps[group]||offset+n>0x114)continue;if(slot>=state.tables.groups[group].size())throw MissionTickError("Person event actor exceeds its current table");const auto& actor=state.tables.groups[group][slot];if(offset==0xc&&n==4){if(!actor.person_slot)return U(0);if(*actor.person_slot>=state.people.size()||*actor.person_slot>=0x500)throw MissionTickError("Person event person exceeds its pool");return 0x794680u+U(*actor.person_slot)*0xb4;}const auto image=snapshot_mission_actor(state,{group,slot});U result=0;for(U i=0;i<n;++i)result|=U(image[offset+i])<<(8*i);return result;}
        if(address>=0x794680&&Q(address-0x794680)+n<=Q(state.people.size())*0xb4){const auto delta=address-0x794680,slot=delta/0xb4,offset=delta%0xb4;if(slot>=0x500||offset+n>0xb4)throw MissionTickError("Person event read exceeds its physical person record");const auto image=ai_person_image(state.people[slot]);U result=0;for(U i=0;i<n;++i)result|=U(image[offset+i])<<(8*i);return result;}
        if(address>=0x794680&&address<0x8cf680)throw MissionTickError("Person event read has no current record owner");
        if(n!=4)throw MissionTickError("Person event lookup width is invalid");
        return need(table,"Person event lookup table is not bound")(address);
    };
    b.write=[&state](U address,U n,U value){
        width(n);if(address<0x794680)throw MissionTickError("Person event write has no person owner");const auto delta=address-0x794680,slot=delta/0xb4,offset=delta%0xb4;if(slot>=state.people.size()||slot>=0x500||offset+n>0xb4)throw MissionTickError("Person event write exceeds its current person record");auto& p=state.people[slot];
        if(n==1){if(offset==0x24)p.flags24=std::uint8_t(value);else if(offset==0x25)p.flags25=std::uint8_t(value);else if(offset==0x9c)p.field_9c=std::uint8_t(value);else throw MissionTickError("Person event byte field has no owner");return;}
        if(n!=4)throw MissionTickError("Person event word field has no owner");
        switch(offset){case 0x64:p.field_64=value;break;case 0x70:p.field_70=value;break;case 0x74:p.field_74=value;break;case 0x7c:p.field_7c=value;break;case 0x88:p.field_88=value;break;case 0x8c:p.field_8c=value;break;case 0x94:p.field_94=value;break;case 0x98:p.field_98=value;break;default:throw MissionTickError("Person event dword field has no owner");}
    };return b;
}
}
