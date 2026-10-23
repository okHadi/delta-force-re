#include "dfre/mission_animation_table_live.hpp"
#include "dfre/animation_driver.hpp"
#include <stdexcept>
namespace dfre {
MissionAnimationTableLive::MissionAnimationTableLive(const MissionRuntimeState& runtime,
 const MissionAssetCache& cache,std::uint32_t base,std::uint32_t stride,std::size_t capacity)
 :runtime_(runtime),cache_(cache),base_(base),stride_(stride),capacity_(capacity),end_(base){
 if(!base||base%4||stride<256u*0x1cu||stride%4||!capacity||
    capacity>(0x100000000ull-base)/stride)
  throw std::invalid_argument("Animation table reservation is invalid");
 end_=std::uint64_t(base)+std::uint64_t(capacity)*stride;
}
std::string_view MissionAnimationTableLive::name(std::uint32_t handle)const{
 const std::string* found=nullptr;
 for(const auto& entry:runtime_.animations)if(entry.second==handle){
  if(found)throw std::invalid_argument("Animation handle has multiple current names");found=&entry.first;
 }
 if(!handle||!found)throw std::invalid_argument("Animation handle has no current owner");
 return *found;
}
std::uint32_t MissionAnimationTableLive::table_address(std::uint32_t handle){
 if(!handle)return 0;
 (void)cache_.animations(name(handle));
 for(std::size_t i=0;i<handles_.size();++i)if(handles_[i]==handle)
  return base_+std::uint32_t(i*stride_);
 if(handles_.size()>=capacity_)throw std::invalid_argument("Animation table reservation is full");
 const auto address=base_+std::uint32_t(handles_.size()*stride_);handles_.push_back(handle);return address;
}
bool MissionAnimationTableLive::claims(std::uint32_t address,std::uint32_t width)const{
 if((width!=1&&width!=2&&width!=4)||std::uint64_t(address)+width>0x100000000ull)
  throw std::invalid_argument("Animation weight access width or range is invalid");
 const auto stop=std::uint64_t(address)+width;
 if(address>=end_||stop<=base_)return false;
 if(address<base_||stop>end_)throw std::invalid_argument("Animation access crosses its reservation");
 const auto relative=address-base_,table=relative/stride_,offset=relative%stride_;
 if(table>=handles_.size())throw std::invalid_argument("Animation span has no assigned handle");
 if(offset>=256u*0x1cu||offset%0x1cu<0x10u||offset%0x1cu+width>0x14u)
  throw std::invalid_argument("Animation access has no owned weight field");
 return true;
}
std::uint32_t MissionAnimationTableLive::read(std::uint32_t address,std::uint32_t width)const{
 if(!claims(address,width))throw std::invalid_argument("Animation weight has no virtual owner");
 const auto relative=address-base_,table=relative/stride_,offset=relative%stride_;
 const auto item=offset/0x1cu,shift=(offset%0x1cu-0x10u)*8u;
 const auto definition=decode_animation_definition(cache_.animations(name(handles_.at(table))));
 const auto& compiled=original_animation_compiled_actions();
 std::uint32_t weight=0;
 for(const auto& action:definition.actions){
  const AnimationCompiledAction* match=nullptr;
  for(const auto& row:compiled)if(row.name==action.name){match=&row;break;}
  if(!match)throw std::invalid_argument("ANM action has no compiled item slot: "+action.name);
  if(match->item_index==item)weight=std::uint32_t(match->weight_10);
 }
 return (weight>>shift)&(width==4?0xffffffffu:((1u<<(width*8u))-1u));
}
}
