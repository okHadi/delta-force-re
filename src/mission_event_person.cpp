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
namespace {
constexpr std::array<U,4> actor_caps{1024,1024,512,256};
void range(U at,U n){width(n);if(Q(at)+n>0x100000000ull)throw MissionTickError("Event current memory address wraps");}
bool overlaps(U at,U n,U base,Q bytes){return Q(at)<Q(base)+bytes&&Q(at)+n>base;}
std::optional<std::pair<std::uint8_t,std::size_t>> actor_record(MissionTickState& state,U at,U n){
 for(std::uint8_t group=0;group<4;++group){const U base=mission_actor_source_address(group,0);if(!overlaps(at,n,base,Q(actor_caps[group])*0x114))continue;
  if(state.tables.groups[group].size()>actor_caps[group])throw MissionTickError("Event current actor count exceeds its physical pool");
  if(at<base)throw MissionTickError("Event current access enters an actor pool");
  const U delta=at-base,slot=delta/0x114,offset=delta%0x114;if(offset+n>0x114||slot>=state.tables.groups[group].size())throw MissionTickError("Event current actor access has no complete current row");return std::make_pair(group,std::size_t(slot));
 }
 if(overlaps(at,n,0x7cca80,0x8cf680ull-0x7cca80))throw MissionTickError("Event current access has no actor pool owner");return {};
}
struct PersonField {U* word=nullptr;std::uint8_t* byte=nullptr;U base=0;};
PersonField person_field(MissionTickState& state,std::size_t slot,U offset){
 auto& p=state.people.at(slot);if(offset==0x24)return {nullptr,&p.flags24,offset};if(offset==0x25)return {nullptr,&p.flags25,offset};if(offset==0x9c)return {nullptr,&p.field_9c,offset};
 const U base=offset&~3u;U* word=nullptr;
 switch(base){case 0x54:case 0x58:case 0x5c:word=&p.position[(base-0x54)/4];break;case 0x60:word=&p.angle_yaw;break;case 0x64:word=&p.field_64;break;case 0x68:word=&p.field_68;break;case 0x70:word=&p.field_70;break;case 0x74:word=&p.field_74;break;case 0x78:word=&p.field_78;break;case 0x7c:word=&p.field_7c;break;case 0x80:word=&p.field_80;break;case 0x84:word=&p.field_84;break;case 0x88:word=&p.field_88;break;case 0x8c:word=&p.field_8c;break;case 0x90:word=&p.field_90;break;case 0x94:word=&p.field_94;break;case 0x98:word=&p.field_98;break;case 0xac:word=&p.field_ac;break;case 0xb0:word=&p.field_b0;break;default:break;}
 if(!word)throw MissionTickError("Event current person field has no owned source value");return {word,nullptr,base};
}
std::size_t person_record(MissionTickState& state,U at,U n){
 if(at<0x794680||Q(at)+n>0x7cca80ull)throw MissionTickError("Event current access crosses its person pool");const U delta=at-0x794680,slot=delta/0xb4,offset=delta%0xb4;
 if(state.people.size()>0x500||slot>=state.people.size()||offset+n>0xb4)throw MissionTickError("Event current access has no complete person row");return slot;
}
std::optional<U> count_read(MissionTickState& state,U at,U n){
 constexpr std::array<U,4> counts{0x8cfa1c,0x8cf904,0x8cf900,0x8cfa18};
 for(std::size_t group=0;group<4;++group)if(overlaps(at,n,counts[group],4)){if(at<counts[group]||Q(at)+n>Q(counts[group])+4)throw MissionTickError("Event current count access crosses its field");const auto size=state.tables.groups[group].size();if(size>actor_caps[group])throw MissionTickError("Event current count exceeds its physical pool");return (U(size)>>((at-counts[group])*8))&(n==4?0xffffffffu:(1u<<(n*8))-1);}
 return {};
}
}
MissionEventPersonBindings mission_event_person_current_bindings(MissionTickState& state,std::function<U(U,U)> foreign_read,std::function<void(U,U,U)> foreign_write,std::function<U(std::size_t)> definition_address,std::function<std::uint16_t()> fp){
 MissionEventPersonBindings b;b.fp_control=std::move(fp);
 b.read=[&state,foreign=std::move(foreign_read),definition=std::move(definition_address)](U at,U n){range(at,n);if(const auto value=count_read(state,at,n))return *value;
  if(const auto record=actor_record(state,at,n)){const auto group=record->first;const auto slot=record->second;const U base=mission_actor_source_address(group,slot),offset=at-base;auto image=snapshot_mission_actor(state,{group,slot});const auto actor=state.tables.groups[group][slot];
   const auto put=[&](U field,U value){for(U i=0;i<4;++i)image[field+i]=std::uint8_t(value>>(8*i));};
   if(offset<8&&offset+n>4){const U physical=actor.active?need(definition,"Event current definition pointer codec is not bound")(actor.definition_index):0;put(4,physical);}
   if(offset<0x10&&offset+n>0xc){if(actor.person_slot&&(*actor.person_slot>=state.people.size()||*actor.person_slot>=0x500))throw MissionTickError("Event current actor person has no current owner");put(0xc,actor.person_slot?0x794680u+U(*actor.person_slot)*0xb4:0);}
   U value=0;for(U i=0;i<n;++i)value|=U(image[offset+i])<<(8*i);return value;
  }
  if(overlaps(at,n,0x794680,0x7cca80ull-0x794680)){const auto slot=person_record(state,at,n);const U offset=(at-0x794680)%0xb4;U value=0;for(U i=0;i<n;++i){const auto field=person_field(state,slot,offset+i);const U byte=field.byte?*field.byte:(*field.word>>((offset+i-field.base)*8))&255u;value|=byte<<(i*8);}return value;}
  return need(foreign,"Event current foreign read is not bound")(at,n);
 };
 b.write=[&state,foreign=std::move(foreign_write)](U at,U n,U value){range(at,n);if(count_read(state,at,n))throw MissionTickError("Event current count stores require their allocator owner");if(actor_record(state,at,n))throw MissionTickError("Event current actor stores require their field owner");
  if(overlaps(at,n,0x794680,0x7cca80ull-0x794680)){const auto slot=person_record(state,at,n);const U offset=(at-0x794680)%0xb4;std::array<PersonField,4> fields{};for(U i=0;i<n;++i)fields[i]=person_field(state,slot,offset+i);for(U i=0;i<n;++i){const U byte=(value>>(i*8))&255u;const auto field=fields[i];if(field.byte)*field.byte=std::uint8_t(byte);else{const U shift=(offset+i-field.base)*8;*field.word=(*field.word&~(255u<<shift))|(byte<<shift);}}return;}
  need(foreign,"Event current foreign write is not bound")(at,n,value&(n==4?0xffffffffu:(1u<<(n*8))-1));
 };return b;
}
MissionEventPersonBindings mission_event_person_bindings(MissionTickState& state,EventTableRecord& action,U action_source,std::function<U(U)> table,std::function<std::uint16_t()> fp){
 auto b=mission_event_person_current_bindings(state,[table=std::move(table)](U at,U n){if(n!=4)throw MissionTickError("Person event lookup width is invalid");return need(table,"Person event lookup table is not bound")(at);},[](U,U,U){throw MissionTickError("Person event write has no person owner");},[](std::size_t index){return U(index);},std::move(fp));
 const auto current=b.read;b.read=[&state,&action,action_source,current](U address,U n){range(address,n);if(address>=action_source&&Q(address-action_source)+n<=32){U result=0;for(U i=0;i<n;++i){const auto offset=address-action_source+i;result|=((action[offset/4]>>((offset%4)*8))&255u)<<(8*i);}return result;}
  // Owned-action callers retain the serialized typed person view.
  if(address>=0x794680&&Q(address)+n<=0x7cca80ull){const auto slot=person_record(state,address,n);const U offset=(address-0x794680)%0xb4;const auto image=ai_person_image(state.people[slot]);U result=0;for(U i=0;i<n;++i)result|=U(image[offset+i])<<(8*i);return result;}
  return current(address,n);
 };return b;
}
}
