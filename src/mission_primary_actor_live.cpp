#include "dfre/mission_primary_actor_live.hpp"
#include "dfre/mission_live.hpp"
#include <array>
#include <cstring>
#include <map>
#include <utility>
namespace dfre {namespace {
using U=std::uint32_t;constexpr U base=0x7dde80,stride=0x114,limit=base+0x45000,count_at=0x8cfa1c;
U word(const AiActorImage& image,U at){U n;std::memcpy(&n,image.data()+at,4);return n;}
void put(AiActorImage& image,U at,U n){std::memcpy(image.data()+at,&n,4);}
template<class F> const F& need(const F& f,const char* text){if(!f)throw MissionTickError(text);return f;}
void width(U at,U size){if((size!=1&&size!=2&&size!=4)||std::uint64_t(at)+size>0x100000000ull)throw MissionTickError("Primary actor memory range is invalid");}
}
struct MissionPrimaryActorLive::Impl {
 struct Header {U index=0,definition=0,normalized=0;bool pending=false;};
 MissionTickState& state;MissionPrimaryActorLiveBindings b;std::array<Header,1024> headers{};std::map<std::size_t,AiActorImage> tail;std::size_t count;
 Impl(MissionTickState& s,MissionPrimaryActorLiveBindings bindings,std::vector<AiActorImage> images):state(s),b(std::move(bindings)),count(s.tables.groups[0].size()){
  if(count>1024||images.size()!=1024-count)throw MissionTickError("Primary actor tail extent differs from its physical pool");ready();
  need(b.initial_definition,"Primary initial definition source is not bound");need(b.definition_index,"Primary definition resolver is not bound");need(b.definition_address,"Primary definition address source is not bound");
  for(std::size_t i=0;i<count;++i){auto image=snapshot_mission_actor(state,{0,i});const U raw=b.initial_definition(i),index=word(image,0),normalized=word(image,4);if((raw?b.definition_index(raw):0)!=normalized||index!=normalized)throw MissionTickError("Primary initial definition differs from its canonical binding");headers[i]={index,raw,normalized,false};}
  for(std::size_t i=0;i<images.size();++i)tail.emplace(count+i,std::move(images[i]));
  state.tables.groups[0].reserve(1024);state.stored_actor_images[0].reserve(1024);state.actor_extras[0].reserve(1024);state.runtimes[0].reserve(1024);state.person_encoding[0].reserve(1024);
 }
 void ready()const{if(state.tables.groups[0].size()!=count||state.stored_actor_images[0].size()!=count||state.actor_extras[0].size()!=count||state.runtimes[0].size()!=count||state.person_encoding[0].size()!=count)throw MissionTickError("Primary actor canonical extent changed outside its owner");}
 U normalized(U raw){const U result=raw?b.definition_index(raw):0;ready();if(result>=state.definitions.entries().size())throw MissionTickError("Primary definition exceeds the canonical table");return result;}
 AiActorImage image(std::size_t slot){ready();if(slot>=count)return tail.at(slot);auto current=snapshot_mission_actor(state,{0,slot});auto& h=headers[slot];const U n=word(current,4);
  if(h.pending&&current==AiActorImage{}&&state.stored_actor_images[0][slot]==AiActorImage{}&&h.normalized){h={};}
  if(!h.pending&&(word(current,0)!=h.index||n!=h.normalized)){h.index=word(current,0);h.normalized=n;h.definition=n?b.definition_address(n):0;ready();if(normalized(h.definition)!=n)throw MissionTickError("Primary current definition address differs from its canonical binding");}
  put(current,0,h.index);put(current,4,h.definition);const auto person=state.tables.groups[0][slot].person_slot;if(person){if(*person>=state.people.size())throw MissionTickError("Primary person exceeds its canonical pool");put(current,0xc,0x794680u+U(*person)*0xb4u);}else put(current,0xc,0);return current;
 }
 void apply(std::size_t slot,AiActorImage raw,U normalized_definition){
  const auto& actor=state.tables.groups[0][slot];const U person=actor.person_slot?0x794680u+U(*actor.person_slot)*0xb4u:0;if(word(raw,0xc)!=person)throw MissionTickError("Primary person store needs its current canonical binding");
  const auto previous=snapshot_mission_actor(state,{0,slot});put(raw,0xc,word(previous,0xc));put(raw,0,normalized_definition);put(raw,4,normalized_definition);apply_mission_actor_image(state,{0,slot},raw);
 }
 void publish(U next){ready();if(next<count||next>1024)throw MissionTickError("Primary insertion count must keep or extend its current extent");if(next==count)return;
  std::vector<std::pair<AiActorImage,Header>> rows;rows.reserve(next-count);
  for(std::size_t slot=count;slot<next;++slot){auto raw=tail.at(slot);const U index=word(raw,0),definition=word(raw,4),n=normalized(definition);if(index!=n)throw MissionTickError("Primary tail definition differs from its source index");if(word(raw,0xc)||word(raw,0x18))throw MissionTickError("Primary tail publication needs explicit person or character bindings");auto image=raw;put(image,4,n);rows.push_back({image,{index,definition,n,false}});}
  const auto old=count;state.tables.groups[0].resize(next);state.stored_actor_images[0].resize(next);state.actor_extras[0].resize(next);state.runtimes[0].resize(next);state.person_encoding[0].resize(next,ActorPersonEncoding::slot);count=next;
  for(std::size_t slot=old;slot<next;++slot){headers[slot]=rows[slot-old].second;apply_mission_actor_image(state,{0,slot},rows[slot-old].first);tail.erase(slot);}
 }
 void store(std::size_t slot,U offset,U size,U value){ready();if(slot>=count){auto& current=tail.at(slot);for(U i=0;i<size;++i)current.at(offset+i)=std::uint8_t(value>>(i*8));return;}
  if(offset<8&&offset+size>4&&(offset!=4||size!=4))throw MissionTickError("Partial primary definition pointer stores need an explicit binding");
  if(offset<0x10&&offset+size>0xc&&(offset!=0xc||size!=4))throw MissionTickError("Partial primary person pointer stores need an explicit binding");
  auto raw=image(slot);for(U i=0;i<size;++i)raw.at(offset+i)=std::uint8_t(value>>(i*8));auto& h=headers[slot];
  if(offset<4){h.index=word(raw,0);h.pending=true;state.tables.groups[0][slot].active=h.index!=0;return;}
  if(offset==4){const U n=normalized(word(raw,4));if(n!=h.index)throw MissionTickError("Primary definition store differs from its current source index");apply(slot,raw,n);h.definition=word(raw,4);h.normalized=n;h.pending=false;return;}
  apply(slot,raw,h.normalized);if(h.pending)state.tables.groups[0][slot].active=h.index!=0;
 }
};
MissionPrimaryActorLive::MissionPrimaryActorLive(MissionTickState& state,MissionPrimaryActorLiveBindings b,std::vector<AiActorImage> tail):impl_(std::make_unique<Impl>(state,std::move(b),std::move(tail))){}
MissionPrimaryActorLive::~MissionPrimaryActorLive()=default;
std::optional<U> MissionPrimaryActorLive::record_base(U at,U size)const{width(at,size);const auto end=std::uint64_t(at)+size;if(at>=limit||end<=base)return {};if(at<base)throw MissionTickError("Primary actor range enters its physical pool");const U offset=at-base;if(offset%stride+size>stride)throw MissionTickError("Primary actor range crosses its physical record");return base+(offset/stride)*stride;}
bool MissionPrimaryActorLive::claims(U at,U size)const{if(record_base(at,size))return true;const auto end=std::uint64_t(at)+size;if(at>=count_at&&end<=count_at+4)return true;if(at<count_at+4&&end>count_at)throw MissionTickError("Primary count range crosses its owner");return false;}
U MissionPrimaryActorLive::read(U at,U size){if(auto record=record_base(at,size)){const auto image=impl_->image((*record-base)/stride);U n=0;for(U i=0;i<size;++i)n|=U(image.at(at-*record+i))<<(i*8);return n;}if(!claims(at,size))throw MissionTickError("Primary read has no owner");impl_->ready();const U shift=(at-count_at)*8;return (U(impl_->count)>>shift)&(size==4?0xffffffffu:(1u<<(size*8))-1);}
void MissionPrimaryActorLive::write(U at,U size,U value){if(auto record=record_base(at,size)){impl_->store((*record-base)/stride,at-*record,size,value);return;}if(!claims(at,size))throw MissionTickError("Primary store has no owner");if(at!=count_at||size!=4)throw MissionTickError("Primary insertion count needs an aligned DWORD store");impl_->publish(value);}
MissionActorInsertBindings MissionPrimaryActorLive::insertion_bindings(){MissionActorInsertBindings b;b.read=[this](U at,U size){if(claims(at,size))return read(at,size);return need(impl_->b.delegated.read,"Primary delegated read is not bound")(at,size);};b.write=[this](U at,U size,U n){if(claims(at,size)){write(at,size,n);return;}need(impl_->b.delegated.write,"Primary delegated store is not bound")(at,size,n);};b.person=[this](U actor){return need(impl_->b.delegated.person,"Primary person allocator is not bound")(actor);};return b;}
}
