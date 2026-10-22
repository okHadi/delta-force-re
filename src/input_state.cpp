#include "dfre/input_state.hpp"

#include <algorithm>
#include <cctype>

namespace dfre {
namespace {

bool ascii_equal_ignore_case(std::string_view a,std::string_view b) {
    if(a.size()!=b.size())return false;
    for(std::size_t i=0;i<a.size();++i){
        const unsigned char x=static_cast<unsigned char>(a[i]),y=static_cast<unsigned char>(b[i]);
        if(std::tolower(x)!=std::tolower(y))return false;
    }
    return true;
}

// 0x479630 look step: ebx = [0x67f500] << 14 (>>2 when 0x484740 is nonzero),
// value = ((ebx signed * (axis<<16)) >> 16) truncated to 32 bits.
std::int32_t look_value(std::int32_t axis,std::int32_t scale,bool steady) {
    std::int32_t shifted=std::int32_t(std::uint32_t(scale)<<14);
    if(steady)shifted>>=2;
    const std::int32_t widened=std::int32_t(std::uint32_t(axis)<<16);
    const std::int64_t product=std::int64_t(shifted)*std::int64_t(widened);
    const std::int64_t step=product>>16;
    return std::int32_t(std::uint32_t(step));
}

constexpr std::uint32_t mask_negate(std::uint32_t value){return std::uint32_t(0u-value);}

} // namespace

const std::vector<ProfileCommandPair>& profile_command_map() {
    static const std::vector<ProfileCommandPair> pairs={
        {0,"slide_left"},{1,"slide_right"},{2,"move_forward"},{3,"move_back"},
        {4,"Prone"},{5,"Crouch"},{6,"Stand"},{7,"move_jump"},
        {8,"attack_1"},{8,"attack_2"},{9,"scope"},{10,"NoRun"},{11,"cycle"},{12,"Binoculars"},
        {13,"weapon1"},{14,"weapon2"},{15,"weapon3"},{16,"weapon4"},{17,"weapon5"},{18,"weapon6"},
        {19,"weapon7"},{20,"weapon8"},{21,"weapon9"},{22,"weapon0"},
        {23,"zoomin"},{24,"zoomout"},{25,"talk"},{26,"stalk"},{27,"NextWaypoint"},
        {28,"AudioMacros"},{29,"resolution"},{30,"Drop"},{31,"magazine"},
        {32,"look_up"},{33,"look_down"},{34,"turn_right"},{35,"turn_left"},
        {36,"tab"},{37,"briefing"},{38,"OldMessages"},{39,"ShowFriendly"},
        {40,"MissionGoals"},{41,"BigMap"},{42,"Turbo"},{43,"NextFlag"},
        {44,"PrevWaypoint"},{45,"server"},
    };
    return pairs;
}

CommandTable::CommandTable() {
    for(const auto& command:hotkey_commands()){
        CommandBinding row;
        row.id=command.id;
        row.name=command.name;
        row.key=command.key;
        row.mouse=command.mouse;
        row.joystick=command.joystick;
        row.flags=command.flags;
        rows_.push_back(row);
    }
}

std::size_t CommandTable::find_by_name(std::string_view name) const {
    for(std::size_t i=0;i<rows_.size();++i){
        if(ascii_equal_ignore_case(rows_[i].name,name))return i;
    }
    return std::string_view::npos;
}

const CommandBinding* CommandTable::find_by_id(std::uint32_t id) const {
    for(const auto& row:rows_)if(row.id==id)return &row;
    return nullptr;
}

void CommandTable::set_binding(std::size_t index,std::uint32_t key,std::uint32_t mouse) {
    if(index>=rows_.size())return;
    CommandBinding& row=rows_[index];
    row.key=std::uint16_t(key);
    row.mouse=mouse==1?1:mouse==2?2:mouse==3?std::uint16_t(0x10):std::uint16_t(0);
}

void CommandTable::apply_profile_bindings(const ActionBindings& bindings) {
    for(const auto& pair:profile_command_map()){
        if(pair.action>=bindings.size())continue;
        const auto index=find_by_name(pair.command);
        if(index==std::string_view::npos)continue;
        set_binding(index,bindings[pair.action].key,bindings[pair.action].mouse);
    }
}

std::vector<std::uint32_t> CommandTable::active_command_indices() const {
    std::vector<std::uint32_t> out;
    for(std::size_t i=0;i<rows_.size();++i){
        if((rows_[i].flags&4)!=0)out.push_back(std::uint32_t(i));
    }
    return out;
}

bool InputQueue::push(const InputRecord& record) {
    if(count_>=capacity)return false;
    records_[count_++]=record;
    return true;
}

bool InputQueue::pop(InputRecord& record) {
    if(count_==0)return false;
    record=records_[0];
    for(std::size_t i=1;i<count_;++i)records_[i-1]=records_[i];
    --count_;
    return true;
}

std::size_t InputQueue::due_count(std::uint32_t tick) const {
    if(tick==0)return count_;
    std::size_t due=0;
    while(due<count_&&records_[due].tick<=tick)++due;
    return due;
}

InputStage::InputStage()=default;

void InputStage::activate() {
    active_=table_.active_command_indices();
    queue_.clear();
    focus_flag_=false;
}

bool InputStage::push_mouse_pulse(std::uint32_t event,std::uint8_t menu_flag,std::uint32_t actor_flags) {
    const auto first_with=[&](std::uint16_t mask)->const CommandBinding*{
        for(const auto& row:table_.rows()){
            if((row.flags&4)==0&&row.mouse==mask)return &row;
        }
        return nullptr;
    };
    switch(event){
    case 2:{
        const CommandBinding* row=first_with(1);
        if(row){queue_.push({row->id,0,0,0});return true;}
        return false;
    }
    case 8:
        if(menu_flag!=0||(actor_flags&4)!=0){queue_.push({0xD,0,0,0});return true;}
        if(const CommandBinding* row=first_with(2)){queue_.push({row->id,0,0,0});return true;}
        return false;
    case 0x20:
        if(const CommandBinding* row=first_with(0x10)){queue_.push({row->id,0,0,0});return true;}
        return false;
    case 0x100: queue_.push({0xC0,0,0,0});return true;
    case 0x200: queue_.push({0xBF,0,0,0});return true;
    default: return false;
    }
}std::size_t InputStage::push_key_event(const InputKeyEvent& event,std::uint32_t tick) {
    // 0x4793da-0x479423: the scan pushes the first matching command and then
    // restarts at 0x4793ba, so one event yields at most one record.
    for(const auto& row:table_.rows()){
        if(row.key!=event.key)continue;
        if((row.flags&4)!=0)continue;
        if((row.flags&0x40)!=0){
            if((event.modifiers&1)==0)continue;
        }else if((row.flags&0x100)!=0){
            if((event.modifiers&4)==0)continue;
        }else if((event.modifiers&5)!=0){
            if(event.key!=0x11&&event.key!=0x10)continue;
        }
        queue_.push({row.id,tick,0,0});
        return 1;
    }
    return 0;
}

GlobalKeyResult InputStage::global_key(std::uint16_t key,const GlobalKeyState& state) {
    // 0x4790a0 is a strict priority chain: once a flag is set only that branch
    // runs and an unmatched key falls straight through to "not handled".
    GlobalKeyResult result;
    if(state.type_flag!=0){
        if(key==state.type_key_a){
            result.handled=true;
            result.effect=GlobalKeyEffect::dispatch_exit;
            result.dispatched=true;
            result.dispatched_id=3;
        }else if(key==state.type_key_b){
            result.handled=true;
            result.effect=GlobalKeyEffect::clear_typing;
        }
        return result;
    }
    if(state.digit_flag!=0){
        if(key>='1'&&key<='9'){
            result.handled=true;
            result.effect=GlobalKeyEffect::digit;
            result.argument=std::uint32_t(key-'0');
        }else if(key=='0'){
            result.handled=true;
            result.effect=GlobalKeyEffect::digit;
            result.argument=0xA;
        }
        return result;
    }
    if(state.page_flag!=0){
        if(key==0x21){
            result.handled=true;
            result.effect=GlobalKeyEffect::flight_page;
            result.argument=0;
        }else if(key==0x22){
            result.handled=true;
            result.effect=GlobalKeyEffect::flight_page;
            result.argument=1;
        }
        return result;
    }
    if(state.menu_flag!=0){
        if(state.hud_menu!=0&&state.hud_menu2!=0){
            if(key==0x21){
                result.handled=true;
                result.effect=GlobalKeyEffect::menu_page;
                result.argument=0;
            }else if(key==0x22){
                result.handled=true;
                result.effect=GlobalKeyEffect::menu_page;
                result.argument=1;
            }
        }else if(key==0x20){
            result.handled=true;
            result.effect=GlobalKeyEffect::dispatch_space;
            result.pushed_record=true;
            result.pushed=InputRecord{0xD,0,0,0};
        }else if(key==0x0D){
            result.handled=true;
            result.effect=GlobalKeyEffect::enter_flag;
        }
        return result;
    }
    if(state.hud_menu!=0&&(state.actor_flags&4)!=0&&key==0x20){
        result.handled=true;
        result.effect=GlobalKeyEffect::dispatch_space;
        result.pushed_record=true;
        result.pushed=InputRecord{0xD,0,0,0};
    }
    return result;
}

std::size_t InputStage::push_mouse(const InputMouseLook& look,std::uint32_t mouse_buttons,std::uint32_t tick) {
    std::size_t pushed=0;
    const std::int32_t y=look.flip?look.y:std::int32_t(mask_negate(std::uint32_t(look.y)));
    const auto push_look=[&](std::uint32_t id,std::int32_t value){
        queue_.push({id,tick,std::uint32_t(value),0});
        ++pushed;
    };
    if(look.x!=0){
        const std::int32_t value=look_value(look.x,look.scale,look.steady);
        if(value>0)push_look(0x9E,value);
        else push_look(0x9D,std::int32_t(mask_negate(std::uint32_t(value))));
    }
    if(y!=0){
        const std::int32_t value=look_value(y,look.scale,look.steady);
        if(value>0)push_look(0x9B,value);
        else push_look(0x9C,std::int32_t(mask_negate(std::uint32_t(value))));
    }
    for(const std::uint32_t index:active_){
        const auto& row=table_.at(index);
        if((row.mouse&std::uint16_t(mouse_buttons))!=0){
            queue_.push({row.id,tick,0,0});
            ++pushed;
        }
    }
    return pushed;
}

std::size_t InputStage::push_held_keys(const std::array<std::uint8_t,256>& keys,std::uint32_t tick,
                                       std::uint16_t priority_key) {
    std::size_t pushed=0;
    for(const std::uint32_t index:active_){
        const auto& row=table_.at(index);
        if(row.key>=256)continue;
        if(keys[row.key]==0)continue;
        if((row.flags&0x80)!=0&&keys[0xC2]==0)continue;
        queue_.push({row.id,tick,0,0});
        ++pushed;
    }
    if(priority_key>=256||keys[priority_key]==0)focus_flag_=true;
    return pushed;
}

bool InputStage::command_active(std::size_t index,const std::array<std::uint8_t,256>& keys,
                                std::uint32_t mouse_buttons,std::uint32_t joystick_buttons) const {
    const auto& row=table_.at(index);
    if(row.key!=0&&row.key<256&&keys[row.key]!=0)return true;
    if((mouse_buttons&row.mouse)!=0)return true;
    if((joystick_buttons&row.joystick)!=0)return true;
    return false;
}

FrameOutput InputStage::begin_frame(const FrameInput& input) {
    FrameOutput out;
    // 0x479fa0(0xa3, 0, 0, tick) as {id, tick, value, extra}.
    const auto push=[&](const InputRecord& record){
        if(queue_.push(record))out.queued.push_back(record);
    };
    const auto collect=[&](std::size_t before){
        const std::size_t total=queue_.size();
        for(std::size_t i=before;i<total;++i)out.queued.push_back(queue_.records()[i]);
    };
    push({0xA3,input.tick,0,0});
    std::size_t event_index=0;
    const auto current=[&]{return input.current?input.current():input;};
    for(;;){
        InputKeyEvent event;
        if(input.next_event)event=input.next_event();
        else if(event_index<input.events.size())event=input.events[event_index++];
        else break;
        // The ring at 0xd094b0 stores the event as one dword; an all-zero slot
        // reads back as 0 and 0x479380 treats it as an empty ring, so the drain
        // stops there and the remaining events wait for the next frame.
        if(event.key==0&&event.state==0&&event.flag==0&&event.modifiers==0)break;
        const auto source=current();
        if(source.input_mode!=0){
            if(source.input_mode==1){
                if(input.on_chat_key)input.on_chat_key(event);
                out.chat_keys.push_back(event);
            }
            continue;
        }
        const auto filtered=global_key(event.key,source.global);
        if(filtered.handled){
            if(input.on_global_key)input.on_global_key(filtered);
            out.effects.push_back(filtered.effect);
            if(filtered.pushed_record)push(filtered.pushed);
            if(filtered.dispatched)out.immediate.push_back({filtered.dispatched_id,0,0,0});
            if(filtered.effect==GlobalKeyEffect::digit)out.digits.push_back(filtered.argument);
            if(filtered.effect==GlobalKeyEffect::flight_page||filtered.effect==GlobalKeyEffect::menu_page)
                out.pages.push_back(std::int32_t(filtered.argument));
            if(filtered.effect==GlobalKeyEffect::enter_flag)out.enter_flag_set=true;
            continue;
        }
        {
            const std::size_t before=queue_.size();
            (void)push_key_event(event,input.tick);
            collect(before);
        }
    }
    const auto mouse=current();
    if(mouse.live_input&&mouse.mouse_enabled){
        const std::size_t before=queue_.size();
        (void)push_mouse(mouse.look,mouse.mouse_buttons,input.tick);
        if(input.on_mouse_sampled)input.on_mouse_sampled();
        collect(before);
    }
    const auto held=current();
    if(held.input_mode==0){
        const std::size_t before=queue_.size();
        (void)push_held_keys(held.keys,input.tick,held.priority_key);
        collect(before);
    }
    return out;
}

std::vector<InputRecord> InputStage::take_due(std::uint32_t tick) {
    std::vector<InputRecord> out;
    while(queue_.due_count(tick)>0){
        InputRecord record;
        queue_.pop(record);
        out.push_back(record);
    }
    return out;
}

} // namespace dfre
