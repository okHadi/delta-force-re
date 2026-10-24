#include "dfre/events.hpp"

#include <algorithm>

namespace dfre {

// Each reached foreign operation requires its current host binding.

std::uint32_t EventHost::object_index_dword(std::uint32_t, std::uint32_t) { throw EventError("Event host object field is not bound"); }
std::uint32_t EventHost::probe_460330(std::size_t) { throw EventError("Event host 460330 is not bound"); }
std::uint32_t EventHost::probe_460560(std::uint32_t, std::uint32_t) { throw EventError("Event host 460560 is not bound"); }
bool EventHost::world_67f520_is_zero() { throw EventError("Event host 67F520 is not bound"); }
bool EventHost::world_67f549_bit1() { throw EventError("Event host 67F549 is not bound"); }
void EventHost::action_45f670(std::uint32_t, std::uint32_t) { throw EventError("Event host 45F670 is not bound"); }
void EventHost::action_45f4c0(std::uint32_t) { throw EventError("Event host 45F4C0 is not bound"); }
void EventHost::action_4601a0(std::size_t) { throw EventError("Event host 4601A0 is not bound"); }
void EventHost::action_45f5b0(std::uint32_t) { throw EventError("Event host 45F5B0 is not bound"); }
void EventHost::action_404dd0(std::uint32_t) { throw EventError("Event host 404DD0 is not bound"); }
void EventHost::action_43f500(std::uint32_t) { throw EventError("Event host 43F500 is not bound"); }
void EventHost::action_444ca0(std::uint32_t) { throw EventError("Event host 444CA0 is not bound"); }
void EventHost::action_460640(std::uint32_t, std::uint32_t) { throw EventError("Event host 460640 is not bound"); }
void EventHost::action_460280(std::size_t) { throw EventError("Event host 460280 is not bound"); }

std::uint32_t EventHost::external_event_fired(std::size_t){throw EventError("Event external fired byte is not bound");}

std::uint32_t EventHost::probe_460330_record(std::uint32_t){throw EventError("Raw event rectangle provider is not bound");}
void EventHost::action_4601a0_record(std::uint32_t){throw EventError("Raw event person edit is not bound");}
void EventHost::action_460280_record(std::uint32_t){throw EventError("Raw event actor order is not bound");}
std::uint32_t EventHost::fired_byte_record(std::uint32_t,std::uint32_t){throw EventError("Raw event fired byte is not bound");}
namespace {
using U=std::uint32_t;
std::int64_t signed_word(U n){return n<=0x7fffffffu?std::int64_t(n):std::int64_t(n)-0x100000000ll;}
std::int32_t signed_half(U n){n&=0xffffu;return n<=0x7fffu?std::int32_t(n):std::int32_t(n)-0x10000;}
U bounded_size(std::size_t n){if(n>0x7fffffffu)throw EventError("Owned event table count is too large");return U(n);}
}
struct EventStage::EngineScope {EventStage& stage;explicit EngineScope(EventStage& s):stage(s){++stage.engine_depth_;}~EngineScope(){--stage.engine_depth_;}};
struct EventStage::Record {
 EventStage* stage;EventTable kind;U address;std::size_t index;
 U* words()const{switch(kind){case EventTable::events:return stage->events_.at(index).words.data();case EventTable::triggers:return stage->triggers_.at(index).data();case EventTable::actions:return stage->actions_.at(index).data();}throw EventError("Invalid event record kind");}
 U read(U offset,unsigned width)const{const U bytes=kind==EventTable::events?24:32;if((width!=1&&width!=2&&width!=4)||offset>bytes||width>bytes-offset)throw EventError("Event field exceeds its record");if(stage->source_){const auto at=std::uint64_t(address)+offset;if(at+width>0x100000000ull)throw EventError("Event field address wraps");return stage->source_->read(U(at),width);}U n=0;const auto* w=words();for(U i=0;i<width;++i)n|=((w[(offset+i)/4]>>(((offset+i)%4)*8))&255u)<<(i*8);return n;}
 void write(U offset,unsigned width,U value)const{const U bytes=kind==EventTable::events?24:32;if((width!=1&&width!=2&&width!=4)||offset>bytes||width>bytes-offset)throw EventError("Event field exceeds its record");if(stage->source_){const auto at=std::uint64_t(address)+offset;if(at+width>0x100000000ull)throw EventError("Event field address wraps");stage->source_->write(U(at),width,value);return;}auto* w=words();for(U i=0;i<width;++i){const U shift=((offset+i)%4)*8;auto& word=w[(offset+i)/4];word=(word&~(255u<<shift))|(((value>>(i*8))&255u)<<shift);}}
};
EventStage::EventStage(EventHost* host):host_(host){}
void EventStage::owned_only()const{if(source_)throw EventError("Owned event reference API is unavailable in source mode");}
void EventStage::source_only()const{if(!source_)throw EventError("Event source memory is not attached");}
void EventStage::attach_source(MissionEventMemory& source){if(source_||engine_depth_||!events_.empty()||!triggers_.empty()||!actions_.empty())throw EventError("Attach event source before loading or dispatch");source_=&source;}
EventRecordView EventStage::event_view(std::size_t i)const{source_only();return source_->event_at(i);}
EventRecordView EventStage::trigger_view(std::size_t i)const{source_only();return source_->trigger_at(i);}
EventRecordView EventStage::action_view(std::size_t i)const{source_only();return source_->action_at(i);}
U EventStage::count_bits(EventTable t)const{if(source_)return source_->count_bits(t);switch(t){case EventTable::events:return bounded_size(events_.size());case EventTable::triggers:return bounded_size(triggers_.size());case EventTable::actions:return bounded_size(actions_.size());}throw EventError("Invalid event table");}
std::size_t EventStage::event_count()const{const U n=count_bits(EventTable::events);return signed_word(n)>0?n:0;}
std::size_t EventStage::trigger_count()const{const U n=count_bits(EventTable::triggers);return signed_word(n)>0?n:0;}
std::size_t EventStage::action_count()const{const U n=count_bits(EventTable::actions);return signed_word(n)>0?n:0;}
MissionEvent& EventStage::event(std::size_t i){owned_only();return events_.at(i);}const MissionEvent& EventStage::event(std::size_t i)const{owned_only();return events_.at(i);}
EventTableRecord& EventStage::trigger(std::size_t i){owned_only();return triggers_.at(i);}const EventTableRecord& EventStage::trigger(std::size_t i)const{owned_only();return triggers_.at(i);}
EventTableRecord& EventStage::action(std::size_t i){owned_only();return actions_.at(i);}const EventTableRecord& EventStage::action(std::size_t i)const{owned_only();return actions_.at(i);}
EventStage::Record EventStage::owned_record(EventTable kind,std::size_t index){std::size_t count=0;switch(kind){case EventTable::events:count=events_.size();break;case EventTable::triggers:count=triggers_.size();break;case EventTable::actions:count=actions_.size();break;}if(index>=count)throw EventError("Event record index exceeds its table");return{this,kind,0,index};}
EventStage::Record EventStage::source_record(EventTable kind,U address){source_only();return{this,kind,address,0};}
EventStage::Record EventStage::run_record(const Record& event,EventTable kind,U offset){const U base=event.read(kind==EventTable::triggers?4:8,4);if(source_)return source_record(kind,base+offset);return owned_record(kind,std::size_t(base)+offset/32);}
void EventStage::reset(){if(source_){source_->reset();return;}if(engine_depth_)throw EventError("Owned event storage must stay stable during dispatch");events_.clear();triggers_.clear();actions_.clear();flags_6e0050_.fill(0);flags_6d7e50_.fill(0);flags_6d8050_.fill(0);flags_6e0254_=0;flags_6e0250_=0;}
void EventStage::load(const MissionData& mission){owned_only();reset();events_=mission.events;triggers_=mission.triggers;actions_=mission.actions;bind_object_triggers(mission.object_groups[2]);bind_object_actions(mission.object_groups[2]);loader_flag_^=1;}
void EventStage::load_tables(std::vector<MissionEvent> events,std::vector<EventTableRecord> triggers,std::vector<EventTableRecord> actions){owned_only();reset();events_=std::move(events);triggers_=std::move(triggers);actions_=std::move(actions);}
void EventStage::bind_object_triggers(const std::vector<MissionObject>& markers){bind_records(markers,true);}
void EventStage::bind_object_actions(const std::vector<MissionObject>& markers){bind_records(markers,false);}
void EventStage::bind_records(const std::vector<MissionObject>& markers, bool trigger_table) {
    owned_only();
    // 0x42eea0 for triggers, 0x42f020 for actions. Both walk every event and
    // every slot of its run, and both match markers by type 0x177d plus the
    // converter's value for raw byte 0x4a (0x449710: param_1[0x1f] for 0x177d).
    for (auto& record : events_) {
        const std::uint32_t base = record.words[trigger_table ? 1 : 2];
        const std::uint32_t count = trigger_table ? record.trigger_count() : record.action_count();
        auto& table = trigger_table ? triggers_ : actions_;
        for (std::uint32_t slot = 0; slot < count; ++slot) {
            const auto index=std::uint64_t(base)+slot;
            if(index>=table.size())throw EventError("Event binding run exceeds its table");
            auto& entry = table[std::size_t(index)];
            const bool bind = trigger_table ? (entry[1] == 1 && entry[2] == 10)
                                            : (entry[1] == 0xcu || entry[1] == 0xdu);
            if (!bind) continue;
            const std::uint32_t id = trigger_table ? entry[4] : entry[3];
            // Rectangle slots: triggers 4,5,6,7; actions 3,5,6,7 (0x42f0f2).
            const std::size_t slots[4] = {trigger_table ? 4u : 3u, 5u, 6u, 7u};
            bool found = false;
            for (const auto& marker : markers) {
                if (marker.type_id != 0x177d) continue;
                if (static_cast<std::uint32_t>(marker.raw[0x4a]) != id) continue;
                const std::uint32_t x = static_cast<std::uint32_t>(marker.position_raw[0]);
                const std::uint32_t y = static_cast<std::uint32_t>(marker.position_raw[1]);
                if (!found) {
                    // First hit: point rectangle, min x, min y, max x, max y.
                    entry[slots[0]] = x;
                    entry[slots[1]] = y;
                    entry[slots[2]] = x;
                    entry[slots[3]] = y;
                    found = true;
                } else {
                    if (static_cast<std::int32_t>(x) < static_cast<std::int32_t>(entry[slots[0]])) entry[slots[0]] = x;
                    if (static_cast<std::int32_t>(y) < static_cast<std::int32_t>(entry[slots[1]])) entry[slots[1]] = y;
                    if (static_cast<std::int32_t>(x) > static_cast<std::int32_t>(entry[slots[2]])) entry[slots[2]] = x;
                    if (static_cast<std::int32_t>(y) > static_cast<std::int32_t>(entry[slots[3]])) entry[slots[3]] = y;
                }
            }
            if (!found) {
                // No marker: the trigger becomes kind 0 (never true); the
                // action keeps its index but its kind is cleared (never runs).
                entry[1] = 0;
                if (trigger_table) entry[2] = 0;
            }
        }
    }
}

U EventStage::global_read(U at)const{
 if(source_)return source_->read(at,4);
 if(at==0x4f2be4)return cursor_;if(at==0x4f2be0)return loader_flag_;if(at==0x6e4734)return frame_counter_;if(at==0x67dbec)return flags_67dbec_;if(at==0x6e0250)return flags_6e0250_;if(at==0x6e0254)return flags_6e0254_;
 if(at>=0x6e0050&&at<0x6e0250)return flags_6e0050_.at((at-0x6e0050)/4);if(at>=0x6d7e50&&at<0x6d8050)return flags_6d7e50_.at((at-0x6d7e50)/4);if(at>=0x6d8050&&at<0x6e0050)return flags_6d8050_.at((at-0x6d8050)/4);throw EventError("Unknown owned event global");
}
void EventStage::global_write(U at,U value){
 if(source_){source_->write(at,4,value);return;}
 if(at==0x4f2be4){cursor_=value;return;}if(at==0x4f2be0){loader_flag_=value;return;}if(at==0x6e4734){frame_counter_=value;return;}if(at==0x67dbec){flags_67dbec_=value;return;}if(at==0x6e0250){flags_6e0250_=value;return;}if(at==0x6e0254){flags_6e0254_=value;return;}
 if(at>=0x6e0050&&at<0x6e0250){flags_6e0050_.at((at-0x6e0050)/4)=value;return;}if(at>=0x6d7e50&&at<0x6d8050){flags_6d7e50_.at((at-0x6d7e50)/4)=value;return;}if(at>=0x6d8050&&at<0x6e0050){flags_6d8050_.at((at-0x6d8050)/4)=value;return;}throw EventError("Unknown owned event global");
}
U EventStage::cursor()const{return global_read(0x4f2be4);}U EventStage::frame_counter()const{return global_read(0x6e4734);}U EventStage::loader_flag()const{return global_read(0x4f2be0);}
void EventStage::set_cursor(U n){global_write(0x4f2be4,n&7u);}void EventStage::set_frame_counter(U n){global_write(0x6e4734,n);}void EventStage::set_loader_flag(U n){global_write(0x4f2be0,n);}
void EventStage::tick(){EngineScope scope(*this);const U offset_cursor=cursor();const U base=source_?source_->table_base(EventTable::events):0;const U count=count_bits(EventTable::events);U index=cursor(),address=base+offset_cursor*24;std::size_t owned_index=offset_cursor;
 if(signed_word(index)<signed_word(count))do{auto record=source_?source_record(EventTable::events,address):owned_record(EventTable::events,owned_index);if(!(record.read(0,1)&6u))process_record(record);const U current=count_bits(EventTable::events);index+=8;address+=192;owned_index+=8;if(signed_word(index)>=signed_word(current))break;}while(true);
 const U current=cursor();global_write(0x4f2be4,(current+1)&7u);
}
void EventStage::run_bits(std::uint8_t bits){EngineScope scope(*this);U address=source_?source_->table_base(EventTable::events):0;const U count=count_bits(EventTable::events);U index=0;if(signed_word(count)<=0)return;do{auto record=source_?source_record(EventTable::events,address):owned_record(EventTable::events,index);if(record.read(0,1)&bits)process_record(record);const U current=count_bits(EventTable::events);++index;address+=24;if(signed_word(index)>=signed_word(current))return;}while(true);}
void EventStage::run_bit1_events(){run_bits(2);}void EventStage::run_bit2_events(){run_bits(4);}
void EventStage::frame_driver_step(){const U next=frame_counter()+1;set_frame_counter(next);if(signed_word(next)>15){tick();set_frame_counter(0);}}
U EventStage::event_fired(std::size_t index)const{if(source_)return source_->event_at(index).read(20,1);return events_.at(index).words[5]&255u;}
U event_fired_reference(const EventStage& stage,std::size_t requested,EventReferencePolicy policy,const EventReferenceDiagnostic& diagnostic){if(stage.uses_source())throw EventError("Owned event reference policy is unavailable in source mode");const auto loaded=stage.event_count();if(requested<loaded)return stage.event_fired(requested);if(policy!=EventReferencePolicy::missing_not_fired)throw EventError("Event fired reference exceeds its loaded table");if(!diagnostic)throw EventError("Missing event repair needs its diagnostic");diagnostic(requested,loaded);return 0;}
EventHost& EventStage::require_host()const{if(!host_)throw EventError("Event host is not bound");return *host_;}
U EventStage::host_object_dword(U address,U index)const{if(source_)return source_->read(address+index*64,4);return require_host().object_index_dword(address,index);}
void EventStage::process_event(std::size_t index){EngineScope scope(*this);if(source_)process_record(source_record(EventTable::events,event_view(index).source_address()));else process_record(owned_record(EventTable::events,index));}
U EventStage::evaluate_event_triggers(std::size_t index){EngineScope scope(*this);return evaluate_record_triggers(source_?source_record(EventTable::events,event_view(index).source_address()):owned_record(EventTable::events,index));}
U EventStage::evaluate_trigger(std::size_t index){EngineScope scope(*this);return evaluate_record(source_?source_record(EventTable::triggers,trigger_view(index).source_address()):owned_record(EventTable::triggers,index));}
void EventStage::dispatch_action(std::size_t index){EngineScope scope(*this);dispatch_record(source_?source_record(EventTable::actions,action_view(index).source_address()):owned_record(EventTable::actions,index));}
void EventStage::process_event_record(U at){EngineScope scope(*this);process_record(source_record(EventTable::events,at));}U EventStage::evaluate_event_triggers_record(U at){EngineScope scope(*this);return evaluate_record_triggers(source_record(EventTable::events,at));}U EventStage::evaluate_trigger_record(U at){EngineScope scope(*this);return evaluate_record(source_record(EventTable::triggers,at));}void EventStage::dispatch_action_record(U at){EngineScope scope(*this);dispatch_record(source_record(EventTable::actions,at));}
void EventStage::dispatch_event_actions(Record record){U index=0,offset=0;for(;;){const U count=record.read(22,1);if(index>=count)return;auto action=run_record(record,EventTable::actions,offset);++index;dispatch_record(action);offset+=32;}}
void EventStage::process_record(Record record){if(source_&&!record.address)return;
 const U delay_compare=record.read(16,2);if(!delay_compare){const U repeat_compare=record.read(12,2);if(!repeat_compare){if(record.read(20,1))return;if(!evaluate_record_triggers(record))return;const U reload=record.read(18,2);record.write(20,1,1);if(reload)record.write(16,2,reload);else dispatch_event_actions(record);if(record.read(0,1)&1u){const U repeat_reload=record.read(14,2);if(repeat_reload)record.write(12,2,repeat_reload);else record.write(20,1,0);}return;}}
 const U delay=record.read(16,2);if(delay){const U next=(delay-128u)&0xffffu;record.write(16,2,next);if(signed_half(next)<=0){record.write(16,2,0);if(!evaluate_record_triggers(record)){record.write(20,1,0);record.write(12,2,0);return;}dispatch_event_actions(record);}}
 const U repeat=record.read(12,2);if(repeat){const U next=(repeat-128u)&0xffffu;record.write(12,2,next);if(signed_half(next)<=0){record.write(12,2,0);record.write(20,1,0);}}
}
U EventStage::evaluate_record_triggers(Record record){if(!record.read(21,1))return 1;U result=evaluate_record(run_record(record,EventTable::triggers,0));if(run_record(record,EventTable::triggers,0).read(0,1)&1u)result^=1;U index=1,offset=32;for(;;){if(index>=record.read(21,1))return result;U value=evaluate_record(run_record(record,EventTable::triggers,offset));if(run_record(record,EventTable::triggers,offset).read(0,1)&1u)value^=1;const U operation=run_record(record,EventTable::triggers,offset-32).read(0,1);if(operation&2)result|=value;else if(operation&4)result^=value;else result&=value;++index;offset+=32;}}
U EventStage::evaluate_record(Record entry){const U kind=entry.read(4,4);switch(kind){case 0:return 0;case 1:{const U subtype=entry.read(8,4);switch(subtype){
 case 1:{const U b=entry.read(16,4),a=entry.read(12,4);return flag_42e520(a,b)?1u:0u;}
 case 2:{const U b=entry.read(16,4),a=entry.read(12,4);return flag_42e550(a,b)?1u:0u;}
 case 3:{const U index=entry.read(12,4);return host_object_dword(event_object_field_7936a4,index)==2?1u:0u;}
 case 4:{const U index=entry.read(12,4);return signed_word(host_object_dword(event_object_field_7936ac,index))<=0?1u:0u;}
 case 5:{const U index=entry.read(12,4);return signed_word(host_object_dword(event_object_field_7936ac,index))>0?1u:0u;}
 case 6:{const U index=entry.read(12,4);const U initial=host_object_dword(event_object_field_7936a8,index);const U current=host_object_dword(event_object_field_7936ac,index);const U threshold=entry.read(16,4);return signed_word(initial-current)>=signed_word(threshold)?1u:0u;}
 case 7:{const U c=entry.read(20,4),b=entry.read(16,4),a=entry.read(12,4);return flag_42e580(a,b,c)?1u:0u;}
 case 9:{const U index=entry.read(12,4);const U initial=host_object_dword(event_object_field_7936a8,index);const U current=host_object_dword(event_object_field_7936ac,index);return initial==current?1u:0u;}
 case 10:return source_?require_host().probe_460330_record(entry.address):require_host().probe_460330(entry.index);
 case 11:{const U b=entry.read(16,4),a=entry.read(12,4);return require_host().probe_460560(a,b);}
 case 12:{const U index=entry.read(12,4),threshold=entry.read(16,4);return signed_word(threshold)<=signed_word(host_object_dword(event_object_field_7936ac,index))?1u:0u;}
 default:return 0;}}
 case 2:{const U index=entry.read(12,4);if(source_){const U base=source_->table_base(EventTable::events);return require_host().fired_byte_record(base,index)&255u;}if(index>=events_.size())return require_host().external_event_fired(index)&255u;return events_[index].words[5]&255u;}
 case 3:{const U bit=entry.read(12,4);return flag_42e5b0(bit)?1u:0u;}
 case 4:return loader_flag();
 case 5:if(source_){if(source_->read(0x67f520,4))return 0;return(source_->read(0x67f549,1)&2u)?0u:1u;}return require_host().world_67f520_is_zero()&&!require_host().world_67f549_bit1()?1u:0u;
 default:return 0;}}
void EventStage::dispatch_record(Record entry){const U kind=entry.read(4,4);switch(kind){case 0:return;
 case 1:{const U b=entry.read(16,4),a=entry.read(12,4);require_host().action_45f670(a,b);return;}
 case 2:{const U a=entry.read(12,4);require_host().action_45f4c0(a);return;}
 case 3:if(source_)require_host().action_4601a0_record(entry.address);else require_host().action_4601a0(entry.index);return;
 case 4:{const U a=entry.read(12,4);require_host().action_45f5b0(a);return;}
 case 5:{const U a=entry.read(12,4);set_flag_42ee50(a);return;}
 case 6:{const U a=entry.read(12,4);require_host().action_404dd0(a);return;}
 case 7:{const U a=entry.read(12,4);require_host().action_43f500(a);return;}
 case 8:require_host().action_444ca0(1);return;case 9:require_host().action_444ca0(2);return;case 10:require_host().action_444ca0(0);return;
 case 11:{const U b=entry.read(16,4),a=entry.read(12,4);require_host().action_460640(a,b);return;}
 case 12:case 13:if(source_)require_host().action_460280_record(entry.address);else require_host().action_460280(entry.index);return;
 case 14:{const U bit=entry.read(12,1);set_flags_6e0254_bit(bit);return;}case 15:{const U bit=entry.read(12,1);set_flags_6e0250_bit(bit);return;}default:return;}}
namespace {std::int64_t index_2d(U a,U b){const U shifted=(b>>5)|((b&0x80000000u)?0xf8000000u:0u);return signed_word(shifted+a*2u);}std::int64_t index_3d(U a,U b){return signed_word((a<<7)+b);}}
U EventStage::flag_word_6e0050(std::size_t i)const{if(i>=flag_2d_words)throw EventError("Event flag index exceeds 6E0050");return global_read(0x6e0050+U(i)*4);}U EventStage::flag_word_6d7e50(std::size_t i)const{if(i>=flag_2d_words)throw EventError("Event flag index exceeds 6D7E50");return global_read(0x6d7e50+U(i)*4);}U EventStage::flag_word_6d8050(std::size_t i)const{if(i>=flag_3d_words)throw EventError("Event flag index exceeds 6D8050");return global_read(0x6d8050+U(i)*4);}
bool EventStage::flag_42e520(U a,U b)const{const auto i=index_2d(a,b);if(i<0||i>=std::int64_t(flag_2d_words))throw EventError("Event flag index exceeds 6E0050");return(flag_word_6e0050(std::size_t(i))&(1u<<(b&31)))!=0;}
bool EventStage::flag_42e550(U a,U b)const{const auto i=index_2d(a,b);if(i<0||i>=std::int64_t(flag_2d_words))throw EventError("Event flag index exceeds 6D7E50");return(flag_word_6d7e50(std::size_t(i))&(1u<<(b&31)))!=0;}
bool EventStage::flag_42e580(U a,U b,U c)const{const auto i=index_3d(a,b);if(i<0||i>=std::int64_t(flag_3d_words))throw EventError("Event flag index exceeds 6D8050");return(flag_word_6d8050(std::size_t(i))&(1u<<(c&31)))!=0;}
void EventStage::set_flag_42edd0(U a,U b){const auto i=index_2d(a,b);if(i<0||i>=std::int64_t(flag_2d_words))throw EventError("Event flag index exceeds 6E0050");const U at=0x6e0050+U(i)*4;const U value=global_read(at);global_write(at,value|(1u<<(b&31)));}
void EventStage::set_flag_42ee00(U a,U b){const auto i=index_2d(a,b);if(i<0||i>=std::int64_t(flag_2d_words))throw EventError("Event flag index exceeds 6D7E50");const U at=0x6d7e50+U(i)*4;const U value=global_read(at);global_write(at,value|(1u<<(b&31)));}
void EventStage::set_flag_42ee30(U a,U b,U c){const auto i=index_3d(a,b);if(i<0||i>=std::int64_t(flag_3d_words))throw EventError("Event flag index exceeds 6D8050");const U at=0x6d8050+U(i)*4;const U value=global_read(at);global_write(at,value|(1u<<(c&31)));}
bool EventStage::flag_42e5b0(U bit)const{return(flag_value_67dbec()&(1u<<(bit&31)))!=0;}void EventStage::set_flag_42ee50(U bit){const U value=flag_value_67dbec();set_flag_value_67dbec(value|(1u<<(bit&31)));}
U EventStage::flags_6e0254()const{return global_read(0x6e0254);}U EventStage::flags_6e0250()const{return global_read(0x6e0250);}U& EventStage::flags_67dbec(){owned_only();return flags_67dbec_;}U EventStage::flags_67dbec()const{return flag_value_67dbec();}U EventStage::flag_value_67dbec()const{return global_read(0x67dbec);}void EventStage::set_flag_value_67dbec(U n){global_write(0x67dbec,n);}
void EventStage::set_flags_6e0254_bit(U bit){const U value=flags_6e0254();global_write(0x6e0254,value|(1u<<(bit&31)));}void EventStage::set_flags_6e0250_bit(U bit){const U value=flags_6e0250();global_write(0x6e0250,value|(1u<<(bit&31)));}
} // namespace dfre
