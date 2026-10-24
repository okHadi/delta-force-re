#include "dfre/mission_event_memory.hpp"
#include <limits>
namespace dfre {
namespace detail {struct MissionEventMemoryState {
 const MissionEventResourceBindings& bindings;bool alive=true;
 explicit MissionEventMemoryState(const MissionEventResourceBindings& b):bindings(b){}
};}
namespace {
using U=std::uint32_t;using Q=std::uint64_t;using State=detail::MissionEventMemoryState;
void require(bool value,const char* text){if(!value)throw MissionEventMemoryError(text);}
void width_ok(unsigned width){require(width==1||width==2||width==4,"Event memory width must be 1, 2 or 4");}
void address_ok(U at,unsigned width){width_ok(width);require(Q(at)+width<=0x100000000ull,"Event memory access wraps its address");}
U mask(unsigned width){return width==4?0xffffffffu:((1u<<(width*8))-1);}
U get(const std::shared_ptr<State>& s,U at,unsigned width){require(s&&s->alive,"Event memory owner has expired");address_ok(at,width);require(bool(s->bindings.read),"Event memory read provider is not bound");return s->bindings.read(at,width)&mask(width);}
void put(const std::shared_ptr<State>& s,U at,unsigned width,U value){require(s&&s->alive,"Event memory owner has expired");address_ok(at,width);require(bool(s->bindings.write),"Event memory write provider is not bound");s->bindings.write(at,width,value&mask(width));}
struct Fields {U count,pointer,stride;};
Fields fields(EventTable kind){switch(kind){case EventTable::events:return{0x4f2bc8,0x4f2bcc,24};case EventTable::triggers:return{0x4f2bd0,0x4f2bd4,32};case EventTable::actions:return{0x4f2bd8,0x4f2bdc,32};}throw MissionEventMemoryError("Event table kind is invalid");}
struct Table {U count,base,stride;};
Table table(const std::shared_ptr<State>& s,EventTable kind){const auto f=fields(kind);const U count=get(s,f.count,4),base=get(s,f.pointer,4);require(count<=0x7fffffffu,"Event table count is negative");require(!count||base,"Active event table has a null base");require(Q(base)+Q(count)*f.stride<=0x100000000ull,"Event table extent wraps its address");return{count,base,f.stride};}
void run_ok(const Table& t,std::size_t index,std::size_t run){require(index<=t.count&&run<=std::size_t(t.count)-index,"Event reference exceeds its current table");}
std::size_t index_of(const Table& t,U at,std::size_t run){require(at>=t.base,"Event reference precedes its current table");const U delta=at-t.base;require(delta%t.stride==0,"Event reference is not at a record boundary");const auto index=std::size_t(delta/t.stride);run_ok(t,index,run);return index;}
U record_address(const Table& t,std::size_t index,std::size_t run){run_ok(t,index,run);const Q at=Q(t.base)+Q(index)*t.stride;require(at<=std::numeric_limits<U>::max(),"Event reference has no 32-bit address");return U(at);}
U field_address(U base,U bytes,U offset,unsigned width){width_ok(width);require(offset<=bytes&&width<=bytes-offset,"Event record access exceeds its captured extent");const Q at=Q(base)+offset;require(at+width<=0x100000000ull,"Event record access wraps its address");return U(at);}
}
EventRecordView::EventRecordView(const std::shared_ptr<State>& s,U at,U bytes):state_(s),address_(at),bytes_(bytes){}
U EventRecordView::read(U offset,unsigned width)const{return get(state_.lock(),field_address(address_,bytes_,offset,width),width);}
void EventRecordView::write(U offset,unsigned width,U value)const{put(state_.lock(),field_address(address_,bytes_,offset,width),width,value);}
MissionEventMemory::MissionEventMemory(const MissionEventResourceBindings& b):state_(std::make_shared<State>(b)){}
MissionEventMemory::~MissionEventMemory(){state_->alive=false;}
U MissionEventMemory::count_bits(EventTable kind)const{return get(state_,fields(kind).count,4);}
U MissionEventMemory::table_base(EventTable kind)const{return get(state_,fields(kind).pointer,4);}
EventRecordView MissionEventMemory::at(EventTable kind,std::size_t index)const{const auto t=table(state_,kind);return EventRecordView(state_,record_address(t,index,1),t.stride);}
EventRecordView MissionEventMemory::event_at(std::size_t index)const{return at(EventTable::events,index);}
EventRecordView MissionEventMemory::trigger_at(std::size_t index)const{return at(EventTable::triggers,index);}
EventRecordView MissionEventMemory::action_at(std::size_t index)const{return at(EventTable::actions,index);}
EventRecordView MissionEventMemory::referenced_record(EventTable kind,U address)const{const auto t=table(state_,kind);index_of(t,address,1);return EventRecordView(state_,address,t.stride);}
std::size_t MissionEventMemory::current_index(EventTable kind,U address,std::size_t run)const{return index_of(table(state_,kind),address,run);}
U MissionEventMemory::encoded_reference(EventTable kind,std::size_t index,std::size_t run)const{return record_address(table(state_,kind),index,run);}
U MissionEventMemory::read(U address,unsigned width)const{return get(state_,address,width);}
void MissionEventMemory::write(U address,unsigned width,U value)const{put(state_,address,width,value);}
void MissionEventMemory::load_current_file(){mission_event_resources_load(state_->bindings);}
void MissionEventMemory::reset(){mission_event_resources_reset(state_->bindings);}
}
