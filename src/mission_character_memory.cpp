#include "dfre/mission_character_memory.hpp"
#include <limits>
#include <stdexcept>
#include <utility>
namespace dfre {namespace {
using U=std::uint32_t;using B=MissionCharacterMemoryBindings;
void need(bool n,const char* text){if(!n)throw std::runtime_error(text);}
struct Field{U offset,size;};
constexpr Field fields[]={{12,4},{16,4},{20,4},{24,4},{28,4},{32,4},{36,4},{40,4},{44,4},{52,4},{56,4},{60,4},{64,4},{72,4},{76,4},{80,4},{96,4},{100,1},{101,1},{102,1},{120,1},{121,1},{122,1},{123,1}};
bool overlap(U a,U w,U b,std::uint64_t size){return std::uint64_t(a)<std::uint64_t(b)+size&&std::uint64_t(b)<std::uint64_t(a)+w;}
struct Location{MissionCharacterMemoryEntry entry{};U field{},byte{},kind{};};
void width(U a,U w){need(w==1||w==2||w==4,"Character memory width must be1,2or4");need(std::uint64_t(a)+w<=0x100000000ull,"Character memory address wraps");}
Location locate(MissionCharacterIdentity& ids,const B& b,U address,U w){
 width(address,w);Location found{};
 b.entries([&](MissionCharacterMemoryEntry e){
  need(e.token&&e.incarnation,"Character memory publication metadata is invalid");
  const auto allocation=b.allocation(e.payload);if(!allocation||allocation->incarnation!=e.incarnation)return;
  need(ids.physical_instance(e.token)==e.physical,"Character memory publication row differs");
  need(b.read(e.physical+4,4)==e.payload,"Character memory publication payload differs");
  auto accept=[&](U field,U byte,U kind){need(!found.kind,"Character memory fields overlap");found={e,field,byte,kind};};
  for(const auto f:fields)if(overlap(address,w,e.physical+f.offset,f.size)){
   need(address>=e.physical+f.offset&&std::uint64_t(address)+w<=std::uint64_t(e.physical)+f.offset+f.size,"Character memory access crosses a field");accept(f.offset,address-e.physical-f.offset,1);
  }
  //Read all pointer metadata before obtaining any borrowed typed array view.
  const U pointers[4]={b.read(e.physical+0x80,4),b.read(e.physical+0x84,4),b.read(e.physical+0x8c,4),b.read(e.physical+0x90,4)};
  auto& controller=b.controller(e.token);const auto state=controller.source_state();
  const std::size_t rows[4]={state.pose.size,state.pose_delta.size,state.root_base.size,state.root_delta.size};
  need(state.pose.size==state.bone_count&&state.pose_delta.size==state.bone_count&&state.root_base.size==8&&state.root_delta.size==8,"Character memory typed array extent differs");
  const std::uint64_t expected[4]={std::uint64_t(e.payload)+state.bone_count*4,std::uint64_t(e.payload)+state.bone_count*28,std::uint64_t(e.payload)+state.bone_count*112,std::uint64_t(e.payload)+state.bone_count*112+96};
  for(U i=0;i<4;++i)need(expected[i]<=0xffffffffu&&pointers[i]==expected[i],"Character memory source array layout differs");
  for(U i=0;i<4;++i){const U stride=i<2?24:12;const auto bytes=std::uint64_t(rows[i])*stride;
   need(pointers[i]>=e.payload&&std::uint64_t(pointers[i])+bytes<=std::uint64_t(e.payload)+allocation->size,"Character memory array exceeds allocation");
   need(!overlap(e.physical,0x9c,pointers[i],bytes),"Character memory array overlaps instance");
   for(U j=0;j<i;++j)need(!overlap(pointers[i],U(bytes),pointers[j],std::uint64_t(rows[j])*(j<2?24:12)),"Character memory arrays overlap");
   if(overlap(address,w,pointers[i],bytes)){const U off=address-pointers[i];need(address>=pointers[i]&&std::uint64_t(address)+w<=std::uint64_t(pointers[i])+bytes&&off%4+w<=4,"Character memory access crosses an array component");accept(off/4,off%4,i+2);}
  }
 });return found;
}
U pointer(U payload,U offset){need(std::uint64_t(payload)+offset<=0xffffffffu,"Character current pointer wraps");return payload+offset;}
U value(AnimationSourceState& s,U f,U payload){switch(f){case 12:return s.animation?pointer(payload,s.animation->offset):0;case 16:return s.frame?pointer(payload,s.frame->offset):0;case 20:need(s.bone_count<=std::numeric_limits<U>::max(),"Character bone count exceeds source width");return U(s.bone_count);case 24:return s.remaining_ticks;case 28:case 32:case 36:return U(s.motion[(f-28)/4]);case 40:return s.random_a;case 44:return s.random_b;case 52:return s.accumulator;case 56:return s.condition_current;case 60:return s.condition_previous;case 64:return s.condition_latched;case 72:return s.anim_flags;case 76:return U(s.extra_4c);case 80:return s.event_bits;case 96:return s.state_flags;case 100:return s.stance;case 101:return s.pitch;case 102:return s.switch_flag;case 120:return s.byte78;case 121:return s.byte79;case 122:return s.aim_a;case 123:return s.aim_b;default:throw std::runtime_error("Character memory field is not owned");}}
std::int32_t signed_word(U v){return v<=0x7fffffffu?std::int32_t(v):std::int32_t(std::int64_t(v)-0x100000000ll);}
void store(AnimationSourceState& s,U f,U v){switch(f){case 24:s.remaining_ticks=v;break;case 28:case 32:case 36:s.motion[(f-28)/4]=signed_word(v);break;case 40:s.random_a=v;break;case 44:s.random_b=v;break;case 52:s.accumulator=v;break;case 56:s.condition_current=v;break;case 60:s.condition_previous=v;break;case 64:s.condition_latched=v;break;case 72:s.anim_flags=v;break;case 76:s.extra_4c=signed_word(v);break;case 80:s.event_bits=v;break;case 96:s.state_flags=v;break;case 100:s.stance=std::uint8_t(v);break;case 101:s.pitch=std::uint8_t(v);break;case 102:s.switch_flag=std::uint8_t(v);break;case 120:s.byte78=std::uint8_t(v);break;case 121:s.byte79=std::uint8_t(v);break;case 122:s.aim_a=std::uint8_t(v);break;case 123:s.aim_b=std::uint8_t(v);break;default:throw std::runtime_error("Character derived field is read-only");}}
std::int32_t& component(AnimationSourceState& s,const Location& p){if(p.kind==2||p.kind==3){auto span=p.kind==2?s.pose:s.pose_delta;need(p.field/6<span.size,"Character pose changed during access");auto& row=span.data[p.field/6];return p.field%6<3?row.angles[p.field%6]:row.scale[p.field%6-3];}auto span=p.kind==4?s.root_base:s.root_delta;need(p.field/3<span.size,"Character root array changed during access");return span.data[p.field/3][p.field%3];}
U mask(U w){return w==4?0xffffffffu:(1u<<(w*8))-1;}
}
MissionCharacterMemory::MissionCharacterMemory(MissionCharacterIdentity& ids,B b):identity_(ids),bindings_(std::move(b)){need(bool(bindings_.entries)&&bool(bindings_.controller)&&bool(bindings_.read)&&bool(bindings_.write)&&bool(bindings_.allocation),"Character memory needs current owner providers");}
bool MissionCharacterMemory::claims(U a,U w)const{return locate(identity_,bindings_,a,w).kind!=0;}
U MissionCharacterMemory::read(U a,U w)const{const auto p=locate(identity_,bindings_,a,w);if(!p.kind)return bindings_.read(a,w);const U payload=bindings_.read(p.entry.physical,4);auto s=bindings_.controller(p.entry.token).source_state();const U v=p.kind==1?value(s,p.field,payload):U(component(s,p));return(v>>(p.byte*8))&mask(w);}
void MissionCharacterMemory::write(U a,U w,U v){const auto p=locate(identity_,bindings_,a,w);if(!p.kind){bindings_.write(a,w,v);return;}auto s=bindings_.controller(p.entry.token).source_state();const U old=p.kind==1?value(s,p.field,0):U(component(s,p));const U bits=mask(w)<<(p.byte*8),next=(old&~bits)|((v<<(p.byte*8))&bits);if(p.kind==1)store(s,p.field,next);else component(s,p)=signed_word(next);}

void MissionCharacterMemory::handoff(U token){
    need(token!=0,"Character handoff token is null");
    std::optional<MissionCharacterMemoryEntry> entry;
    bindings_.entries([&](MissionCharacterMemoryEntry current){
        if(current.token==token){need(!entry,"Character handoff token is duplicated");entry=current;}
    });
    need(bool(entry),"Character handoff token is not published");
    //Reuse current Identity, allocation, field and exact array-layout checks.
    const auto current=locate(identity_,bindings_,entry->physical+0x18,4);
    need(current.kind==1&&current.entry.token==token&&current.entry.payload==entry->payload&&current.entry.incarnation==entry->incarnation,"Character handoff publication is not current");
    const U model=bindings_.read(entry->physical,4);
    const U arrays[4]={bindings_.read(entry->physical+0x80,4),bindings_.read(entry->physical+0x84,4),bindings_.read(entry->physical+0x8c,4),bindings_.read(entry->physical+0x90,4)};
    //No owner-changing call is permitted after this fresh borrowed view.
    auto state=bindings_.controller(token).source_state();
    for(const auto field:fields)bindings_.write(entry->physical+field.offset,field.size,value(state,field.offset,model));
    for(U table=0;table<4;++table){
        const auto rows=table==0?state.pose.size:table==1?state.pose_delta.size:table==2?state.root_base.size:state.root_delta.size;
        const U components=table<2?6:3;
        for(std::size_t row=0;row<rows;++row)for(U k=0;k<components;++k){
            const Location item{*entry,U(row)*components+k,0,table+2};
            bindings_.write(arrays[table]+U(row)*components*4+k*4,4,U(component(state,item)));
        }
    }
}
}
