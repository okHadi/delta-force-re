#include "dfre/mission_character_identity.hpp"
#include <map>
#include <set>
#include <stdexcept>
#include <utility>
namespace dfre {namespace {
using U=std::uint32_t;using Stamp=MissionCharacterAllocationIdentity;
void require(bool n,const char* text){if(!n)throw std::runtime_error(text);}
bool same(Stamp a,Stamp b){return a.generation==b.generation&&a.size==b.size&&a.incarnation==b.incarnation;}
U occupied(U physical){return 0x439860+(physical-0x42fc60)/0x9c;}
void span(U payload,Stamp allocation,U at,std::size_t bytes){const auto first=std::uint64_t(payload),end=first+allocation.size;require(end<=0x100000000ull&&at>=first&&std::uint64_t(at)+bytes<=end,"Character array exceeds its source allocation");}
}
struct MissionCharacterIdentity::Impl {
 struct Resource {U row,payload;Stamp allocation;const CharacterModel* model;};
 struct Instance {U row,resource,payload;Stamp allocation;const AnimationController* controller;};
 MissionCharacterIdentityBindings b;std::map<U,Resource> resources;std::map<U,Instance> instances;std::set<U> retired_resources,retired_instances;
 explicit Impl(MissionCharacterIdentityBindings input):b(std::move(input)){require(bool(b.read)&&bool(b.resource)&&bool(b.instance)&&bool(b.allocation),"Character identity needs current owner providers");}
 U read(U at,U width=4)const{return b.read(at,width);}
 std::optional<Stamp> allocation(U payload)const{if(!payload)return std::nullopt;const auto stamp=b.allocation(payload);if(stamp){require(stamp->incarnation!=0,"Character allocation needs a distinct owner identity");require(payload>=16&&read(payload-16)==stamp->size&&read(payload-12)==stamp->generation,"Character allocation stamp differs from its current header");}return stamp;}
 Stamp live(U payload)const{const auto stamp=allocation(payload);require(bool(stamp),"Character allocation is not live");return *stamp;}
 const Resource& resource(U token)const{const auto& item=resources.at(token);const auto current=b.resource(token);require(&current.model==item.model,"Character resource owner changed");const U count=read(0x6bbd40);require(count<=64&&(item.row-0x6ba440)/100<count,"Character resource row is not current");require(read(item.row+0x60)==item.payload&&same(live(item.payload),item.allocation),"Character resource allocation changed");return item;}
 const Instance& instance(U token)const{const auto& item=instances.at(token);require(&b.instance(token)==item.controller,"Character controller owner changed");const auto& source=resource(item.resource);require(read(occupied(item.row),1)!=0,"Character instance is released");require(read(item.row)==source.payload&&read(item.row+4)==item.payload&&same(live(item.payload),item.allocation),"Character instance allocation changed");return item;}
 void match(U row,const Resource& res,AnimationController& controller,Stamp allocation)const{
  const auto s=controller.source_state();require(&s.model==res.model&&s.animation&&s.frame,"Character action setup is incomplete");span(res.payload,res.allocation,res.payload+s.animation->offset,20);span(res.payload,res.allocation,res.payload+s.frame->offset,52);require(read(row+12)==res.payload+s.animation->offset&&read(row+16)==res.payload+s.frame->offset,"Character source action setup is incomplete");require(read(res.payload+s.animation->offset+8)==s.root_count,"Character current animation root count differs");require(read(row+20)==s.bone_count&&read(row+24)==s.remaining_ticks,"Character initial counts differ");require(read(row+0x58)==0x34,"Character typed pose format differs");
  for(U i=0;i<3;++i)require(read(row+0x1c+i*4)==U(s.motion[i]),"Character initial motion differs");
  for(const auto field:{std::pair<U,U>{0x28,s.random_a},{0x2c,s.random_b},{0x34,s.accumulator},{0x38,s.condition_current},{0x3c,s.condition_previous},{0x40,s.condition_latched},{0x48,s.anim_flags},{0x4c,U(s.extra_4c)},{0x50,s.event_bits},{0x60,s.state_flags}})require(read(row+field.first)==field.second,"Character initial scalar differs");
  for(const auto field:{std::pair<U,U>{0x64,s.stance},{0x65,s.pitch},{0x66,s.switch_flag},{0x78,s.byte78},{0x79,s.byte79},{0x7a,s.aim_a},{0x7b,s.aim_b}})require(read(row+field.first,1)==field.second,"Character initial byte differs");
  const U payload=read(row+4);
  for(const auto field:{std::pair<U,AnimationSourceSpan<CharacterBonePose>>{0x80,s.pose},{0x84,s.pose_delta}}){require(field.second.size==s.bone_count,"Character typed pose extent differs");const U base=read(row+field.first);require(field.second.size<=allocation.size/24,"Character pose extent exceeds allocation");span(payload,allocation,base,field.second.size*24);for(std::size_t i=0;i<field.second.size;++i)for(U k=0;k<3;++k){require(read(base+U(i)*24+k*4)==U(field.second.data[i].angles[k]),"Character initial angle differs");require(read(base+U(i)*24+12+k*4)==U(field.second.data[i].scale[k]),"Character initial scale differs");}}
  require(s.root_count<=8,"Character source root count exceeds its region");
  for(const auto field:{std::pair<U,AnimationSourceSpan<std::array<std::int32_t,3>>>{0x8c,s.root_base},{0x90,s.root_delta}}){require(field.second.size==8,"Character typed root extent differs");const U base=read(row+field.first);span(payload,allocation,base,96);for(U i=0;i<8;++i)for(U k=0;k<3;++k)require(read(base+i*12+k*4)==U(field.second.data[i][k]),"Character initial root differs");}
 }
};
MissionCharacterIdentity::MissionCharacterIdentity(MissionCharacterIdentityBindings b):impl_(std::make_unique<Impl>(std::move(b))){}
MissionCharacterIdentity::~MissionCharacterIdentity()=default;
void MissionCharacterIdentity::publish_resource(U token,U row){
 auto& m=*impl_;require(token!=0,"Character resource token is null");require(!m.retired_resources.count(token),"Character resource token is retired");require(row>=0x6ba440&&row<0x6bbd40&&(row-0x6ba440)%100==0,"Character physical resource row is invalid");const U count=m.read(0x6bbd40);require(count<=64&&(row-0x6ba440)/100<count,"Character source resource count is invalid");const auto owner=m.b.resource(token);require(owner.model.runtime_prepared,"Character model is not prepared");const U payload=m.read(row+0x60);const auto allocation=m.live(payload);span(payload,allocation,payload,0x4c);require(m.read(payload)==0x31524843&&m.read(payload+16)==owner.model.bone_map.size(),"Character source model differs");
 for(U i=0;;++i){require(i<32,"Character source filename exceeds its field");const U x=m.read(row+i,1),y=i<owner.filename.size()?std::uint8_t(owner.filename[i]):0;const auto upper=[](U c){return c>='a'&&c<='z'?c-32:c;};require(upper(x)==upper(y),"Character source filename differs");if(!x)break;}
 if(const auto found=m.resources.find(token);found!=m.resources.end()){const auto& old=m.resource(token);require(old.row==row&&old.payload==payload&&same(old.allocation,allocation),"Character resource token is already bound");return;}
 for(const auto& item:m.resources)require(item.second.row!=row,"Character physical resource is already bound");m.resources.emplace(token,Impl::Resource{row,payload,allocation,&owner.model});
}
void MissionCharacterIdentity::publish_instance(U token,U row,U resource_token){
 auto& m=*impl_;require(token!=0,"Character instance token is null");require(!m.retired_instances.count(token),"Character instance token is retired");require(row>=0x42fc60&&row<0x439860&&(row-0x42fc60)%0x9c==0,"Character physical instance row is invalid");require(m.instances.find(token)==m.instances.end(),"Character instance token is already bound");for(const auto& item:m.instances)require(item.second.row!=row,"Character physical instance is already bound");const auto& resource=m.resource(resource_token);auto& controller=m.b.instance(token);require(m.read(occupied(row),1)==1&&m.read(row)==resource.payload,"Character source instance is not current");const U payload=m.read(row+4);const auto allocation=m.live(payload);m.match(row,resource,controller,allocation);m.instances.emplace(token,Impl::Instance{row,resource_token,payload,allocation,&controller});
}
U MissionCharacterIdentity::physical_resource(U token)const{return token?impl_->resource(token).row:0;}
U MissionCharacterIdentity::resource_token(U row)const{if(!row)return 0;for(const auto& item:impl_->resources)if(item.second.row==row){impl_->resource(item.first);return item.first;}throw std::runtime_error("Character physical resource is not published");}
U MissionCharacterIdentity::physical_instance(U token)const{return token?impl_->instance(token).row:0;}
U MissionCharacterIdentity::instance_token(U row)const{if(!row)return 0;for(const auto& item:impl_->instances)if(item.second.row==row){impl_->instance(item.first);return item.first;}throw std::runtime_error("Character physical instance is not published");}
void MissionCharacterIdentity::unpublish_instance(U token){
 auto& m=*impl_;const auto item=m.instances.at(token);const auto old_current=m.allocation(item.payload);require(!old_current||old_current->incarnation!=item.allocation.incarnation,"Character source instance allocation is still live");
 const U current_payload=m.read(item.row+4);const auto current=current_payload?m.allocation(current_payload):std::nullopt;const bool reused=current&&(current_payload!=item.payload||current->incarnation!=item.allocation.incarnation);require(m.read(occupied(item.row),1)==0||reused,"Character source instance release is incomplete");m.retired_instances.insert(token);m.instances.erase(token);
}
void MissionCharacterIdentity::unpublish_resource(U token){
 auto& m=*impl_;const auto item=m.resources.at(token);for(const auto& instance:m.instances)require(instance.second.resource!=token,"Character resource still has instance bindings");const auto old_current=m.allocation(item.payload);require(!old_current||old_current->incarnation!=item.allocation.incarnation,"Character source resource allocation is still live");
 const U current_payload=m.read(item.row+0x60);const auto current=current_payload?m.allocation(current_payload):std::nullopt;const bool reused=current&&(current_payload!=item.payload||current->incarnation!=item.allocation.incarnation);require(!current_payload||reused,"Character source registry clear is incomplete");m.retired_resources.insert(token);m.resources.erase(token);
}
std::size_t MissionCharacterIdentity::resource_count()const noexcept{return impl_->resources.size();}
std::size_t MissionCharacterIdentity::instance_count()const noexcept{return impl_->instances.size();}
}
