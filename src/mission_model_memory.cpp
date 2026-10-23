#include "dfre/mission_model_memory.hpp"
#include <algorithm>
#include <limits>
#include <map>
#include <set>
namespace dfre {
namespace {
using U=std::uint32_t;
constexpr U graphics=0x20000000,lods=0x20100000,points=0x20200000,limit=0x20800000;
void check(bool value,const char* message){if(!value)throw MissionModelMemoryError(message);}
void width(U at,U size){check((size==1||size==2||size==4)&&std::uint64_t(at)+size<=0x100000000ull,"Model memory access width or range is invalid");}
U word(const AssetBytes& bytes,std::size_t at){check(at<=bytes.size()&&bytes.size()-at>=4,"Model source header is short");U result=0;for(U i=0;i<4;++i)result|=U(bytes[at+i])<<(i*8);return result;}
bool inside(U at,U size,U start,U bytes){return at>=start&&std::uint64_t(at-start)+size<=bytes;}
}
MissionModelCallbackSeed MissionModelCallbackSeed::pre_producer(){return {Phase::pre_producer,{}};}
MissionModelCallbackSeed MissionModelCallbackSeed::post_initialized(std::array<U,5> words){return {Phase::post_initialized,words};}
struct MissionModelMemory::Impl {
 struct Lod {U address,hardpoints,point_extent,reserved;std::size_t count,header_size;};
 struct Entry {MissionModelMemoryBinding binding;U address;std::array<U,5> callbacks;std::string identity;std::uint8_t version;std::vector<Lod> levels;};
 struct View {std::size_t entry,level;U base,offset;enum Kind{graphic,lod,hardpoint} kind;};
 ModelProvider models;FileProvider files;std::vector<Entry> entries;std::map<U,std::size_t> handles;U next_lod=0,next_points=points;
 Impl(ModelProvider m,FileProvider f,std::vector<MissionModelMemoryBinding> bindings):models(std::move(m)),files(std::move(f)){
  check(bool(models)&&bool(files),"Model memory needs current cache and source file providers");check(bindings.size()<=500,"Model memory exceeds the source graphic capacity");
  entries.reserve(bindings.size());std::set<const Model*> owners;
  for(auto& binding:bindings){
   check(binding.handle&&!binding.name.empty()&&binding.callbacks.has_value(),"Model memory binding needs a handle, name and explicit callback seed");check(!handles.count(binding.handle),"Model memory handle is already registered");
   const auto& current=models(binding.name);check(owners.insert(&current).second,"Cached model already has a memory owner");const auto raw=files(binding.name);const auto decoded=decode_model(raw);
   check(current.name==decoded.name&&current.version==decoded.version&&current.render_types==decoded.render_types&&current.lod_thresholds==decoded.lod_thresholds&&current.bounding_radius==decoded.bounding_radius&&current.lods.size()==decoded.lods.size(),"Model cache differs from its source file identity or header");
   check(current.lods.size()<=4&&next_lod+current.lods.size()<=601,"Model memory exceeds the source LOD capacity");
   Entry entry{std::move(binding),graphics+U(entries.size())*0x80,{},current.name,current.version,{}};
   switch(entry.binding.callbacks->phase){case MissionModelCallbackSeed::Phase::pre_producer:for(U i=0;i<5;++i)entry.callbacks[i]=word(raw,0x3c+i*4);break;case MissionModelCallbackSeed::Phase::post_initialized:entry.callbacks=entry.binding.callbacks->words;break;default:throw MissionModelMemoryError("Model callback seed phase is invalid");}
   for(std::size_t i=0;i<current.lods.size();++i){const auto& level=current.lods[i];check(level.raw_header==decoded.lods[i].raw_header&&level.hardpoints==decoded.lods[i].hardpoints,"Model cache LOD or hardpoints differ from the source file");check(level.hardpoints.size()<=std::numeric_limits<U>::max()/12,"Model hardpoint byte extent exceeds source width");const U extent=U(level.hardpoints.size())*12;const auto reserved=std::max<std::uint64_t>(16,(std::uint64_t(extent)+15)&~15ull);check(std::uint64_t(next_points)+reserved<=limit,"Model hardpoint memory range is full");entry.levels.push_back({lods+next_lod++*0x100,next_points,extent,U(reserved),level.hardpoints.size(),level.raw_header.size()});next_points+=U(reserved);}
   handles.emplace(entry.binding.handle,entries.size());entries.push_back(std::move(entry));
  }
 }
 const Model& current(std::size_t index)const{
  const auto& entry=entries.at(index);const auto& model=models(entry.binding.name);check(model.name==entry.identity&&model.version==entry.version&&model.lods.size()==entry.levels.size(),"Current model identity or LOD topology changed");
  for(std::size_t i=0;i<entry.levels.size();++i){const auto& actual=model.lods[i];const auto& stored=entry.levels[i];check(actual.hardpoints.size()==stored.count&&actual.raw_header.size()==stored.header_size,"Current model hardpoint or header topology changed");check(word(actual.raw_header,0xa0)==actual.hardpoints.size(),"Current model hardpoint count differs from its source header");}
  return model;
 }
 std::optional<View> view(U at,U size)const{
  width(at,size);const auto end=std::uint64_t(at)+size;
  if(at>=limit||end<=graphics)return {};
  check(at>=graphics&&end<=limit,"Model access crosses the reserved memory range");
  if(at<lods){const U slot=(at-graphics)/0x80,offset=(at-graphics)%0x80;check(slot<entries.size()&&offset+size<=0x7c,"Model graphic access is outside a current record");check(inside(offset,size,0x14,4)||inside(offset,size,0x28,16)||inside(offset,size,0x3c,36),"Model graphic field has no proved current owner");current(slot);return View{slot,0,entries[slot].address,offset,View::graphic};}
  if(at<points){const U slot=(at-lods)/0x100,offset=(at-lods)%0x100;check(offset+size<=0xb8,"Model LOD access crosses a source record");check(inside(offset,size,0x40,4)||inside(offset,size,0x58,4)||inside(offset,size,0xa0,8),"Model LOD field has no proved current owner");for(std::size_t e=0;e<entries.size();++e)for(std::size_t l=0;l<entries[e].levels.size();++l)if(entries[e].levels[l].address==lods+slot*0x100){current(e);return View{e,l,lods+slot*0x100,offset,View::lod};}throw MissionModelMemoryError("Model LOD address has no current owner");}
  for(std::size_t e=0;e<entries.size();++e)for(std::size_t l=0;l<entries[e].levels.size();++l){const auto& level=entries[e].levels[l];if(at<level.hardpoints||at>=level.hardpoints+level.reserved)continue;check(inside(at,size,level.hardpoints,level.point_extent),"Model hardpoint access is outside the current span");current(e);return View{e,l,level.hardpoints,at-level.hardpoints,View::hardpoint};}
  throw MissionModelMemoryError("Model memory address is in an unowned gap");
 }
 U byte(const View& view,U offset)const{
  const auto& entry=entries.at(view.entry);const auto& model=current(view.entry);
  if(view.kind==View::hardpoint){const auto& value=model.lods.at(view.level).hardpoints.at(offset/12);return (U(value.at((offset%12)/4))>>((offset%4)*8))&255;}
  U value=0,base=offset&~3u;
  if(view.kind==View::graphic){if(base==0x14)value=U(model.lods.size());else if(base>=0x28&&base<=0x34)value=model.render_types.at((base-0x28)/4);else if(base>=0x3c&&base<=0x4c)value=entry.callbacks.at((base-0x3c)/4);else if(base>=0x50&&base<=0x5c){const U level=(base-0x50)/4;value=level<entry.levels.size()?entry.levels[level].address:0;}else throw MissionModelMemoryError("Model graphic byte is not owned");}
  else{const auto& level=entry.levels.at(view.level);if(base==0x40||base==0x58)value=word(model.lods.at(view.level).raw_header,base);else if(base==0xa0)value=U(model.lods.at(view.level).hardpoints.size());else if(base==0xa4)value=level.hardpoints;else throw MissionModelMemoryError("Model LOD byte is not owned");}
  return (value>>((offset%4)*8))&255;
 }
};
MissionModelMemory::MissionModelMemory(ModelProvider m,FileProvider f,std::vector<MissionModelMemoryBinding> bindings):impl_(std::make_unique<Impl>(std::move(m),std::move(f),std::move(bindings))){}
MissionModelMemory::~MissionModelMemory()=default;
U MissionModelMemory::source(U handle)const{if(!handle)return 0;const auto found=impl_->handles.find(handle);check(found!=impl_->handles.end(),"Typed model handle has no current source mapping");impl_->current(found->second);return impl_->entries[found->second].address;}
U MissionModelMemory::handle(U address)const{if(!address)return 0;check(address>=graphics&&address<lods&&(address-graphics)%0x80==0,"Model source pointer is not an exact graphic base");const U slot=(address-graphics)/0x80;check(slot<impl_->entries.size(),"Model source pointer has no current mapping");impl_->current(slot);return impl_->entries[slot].binding.handle;}
const Model& MissionModelMemory::model(U address)const{const auto mapped=handle(address);check(mapped,"Null model has no cache object");return impl_->current(impl_->handles.at(mapped));}
bool MissionModelMemory::claims(U address,U size)const{return impl_->view(address,size).has_value();}
std::optional<U> MissionModelMemory::record_base(U address,U size)const{const auto view=impl_->view(address,size);return view?std::optional<U>{view->base}:std::nullopt;}
U MissionModelMemory::read(U address,U size)const{const auto view=impl_->view(address,size);check(view.has_value(),"Model memory read is outside its owner");U value=0;for(U i=0;i<size;++i)value|=impl_->byte(*view,view->offset+i)<<(i*8);return value;}
void MissionModelMemory::write(U address,U size,U value){const auto view=impl_->view(address,size);check(view.has_value()&&view->kind==Impl::View::graphic&&inside(view->offset,size,0x3c,20),"Model memory write needs current callback bytes");auto& words=impl_->entries.at(view->entry).callbacks;for(U i=0;i<size;++i){const U offset=view->offset-0x3c+i,shift=(offset%4)*8;auto& word=words.at(offset/4);word=(word&~(255u<<shift))|(((value>>(i*8))&255u)<<shift);}}
}
