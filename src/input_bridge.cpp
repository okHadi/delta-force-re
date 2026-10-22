#include "dfre/input_bridge.hpp"
#include <cstring>
#include <stdexcept>

namespace dfre {
namespace {

// Mouse button values: platform 1 left / 2 right / 3 middle; the legacy zero is
// the original left-button event.
unsigned normalized_button(unsigned button){return button==0?1:button;}
std::int32_t add_delta(std::int32_t a,std::int32_t b){
    const std::uint32_t bits=std::uint32_t(a)+std::uint32_t(b);
    std::int32_t value;std::memcpy(&value,&bits,sizeof(value));return value;
}

} // namespace

std::uint32_t InputBridge::mouse_mask(unsigned button) {
    switch(normalized_button(button)){
    case 1: return 0x1;   // 0xd09348 bit 0
    case 2: return 0x2;   // bit 1
    case 3: return 0x10;  // bit 4, the original middle button
    default: return 0;
    }
}

std::uint32_t InputBridge::mouse_pulse(unsigned button) {
    switch(normalized_button(button)){
    case 1: return 2;     // 0x497a40 left down
    case 2: return 8;     // right down
    case 3: return 0x20;  // middle down
    default: return 0;
    }
}

InputBridge::InputBridge(const ActionBindings& profile_bindings) {
    apply_profile(profile_bindings);
}

InputBridge InputBridge::with_shipped_profile() {
    return InputBridge(default_action_bindings());
}

void InputBridge::apply_profile(const ActionBindings& profile_bindings) {
    stage_.table().apply_profile_bindings(profile_bindings); // 0x41ce20
    stage_.activate();                                       // 0x47a300 + 0x477d00
}

void InputBridge::queue_key_press(int key,bool extended) {
    if(key<=0||key>255)return;
    keys_[std::size_t(key)]=1;
    if((sources_?sources_():flags_).keyboard_ring_blocked)return;
    // 0x498ae0 sets the VK state first, then 0x498a70 snapshots the modifier
    // bytes 0xd093c0/0xd093c1 (shift and control are key states 0x10/0x11).
    InputKeyEvent event;
    event.key=std::uint8_t(key);
    event.state=0;
    event.flag=extended?0x80:0;
    event.modifiers=std::uint8_t((keys_[0x10]?1:0)|(keys_[0x11]?4:0));
    ring_[ring_write_]=event;
    ring_last_=ring_write_;
    ring_write_=(ring_write_+1)&127;
}

void InputBridge::feed(const InputEvent& event) {
    switch(event.kind){
    case InputKind::quit:
        quit_=true;
        break;
    case InputKind::key_down:
        queue_key_press(event.key,event.extended);
        break;
    case InputKind::key_up:
        if(event.key>0&&event.key<=255)keys_[std::size_t(event.key)]=0; // 0x498a60
        break;
    case InputKind::pointer_delta:
        delta_x_=add_delta(delta_x_,event.x);
        delta_y_=add_delta(delta_y_,event.y);
        break;
    case InputKind::pointer_down: {
        if(sources_)flags_=sources_();
        const std::uint32_t button=normalized_button(event.button);
        mouse_mask_|=mouse_mask(button);
        const std::uint32_t pulse=mouse_pulse(button);
        if(pulse){
            // 0x497a40 -> callback 0x47a150 with the current menu/actor flags.
            stage_.push_mouse_pulse(pulse,flags_.global.menu_flag,flags_.global.actor_flags);
        }
        break;
    }
    case InputKind::pointer_up:
        mouse_mask_&=~mouse_mask(normalized_button(event.button));
        break;
    case InputKind::pointer_move:
        pointer_x_=event.x;
        pointer_y_=event.y;
        break;
    case InputKind::focus_lost:
        focus_lost();
        break;
    case InputKind::text_input:
        for(const unsigned char character:event.text)if(character<128)feed_character(character);
        break;
    }
}
void InputBridge::feed_character(std::uint8_t character){
    if(ring_last_)ring_[*ring_last_].state=character;
}

void InputBridge::feed(const std::vector<InputEvent>& events) {
    for(const auto& event:events)feed(event);
}

void InputBridge::focus_lost() {
    // 490367 calls498B80 and clearsCC0884.
    keys_.fill(0);
    flags_.window_active=false;
}
bool InputBridge::query_active(std::size_t row_index) const{
    const auto flags=sources_?sources_():flags_;
    if(row_index<stage_.table().rows().size())
        return stage_.command_active(row_index,keys_,mouse_mask_,flags.joystick_buttons);
    if(!query_row_)throw std::out_of_range("Input query needs its loaded table row");
    const auto row=query_row_(row_index);
    if(!row)throw std::out_of_range("Input query row is not loaded");
    if(row->key>=keys_.size())throw std::out_of_range("Input query key exceeds VK state");
    return (row->key && keys_[row->key]) || (row->mouse&mouse_mask_) || (row->joystick&flags.joystick_buttons);
}
void InputBridge::set_live_sources(std::function<InputSourceFlags()> source,
    std::function<void(const GlobalKeyResult&)> global_effect,
    std::function<void(const InputKeyEvent&)> chat){
    sources_=std::move(source);global_effect_=std::move(global_effect);chat_=std::move(chat);
}

BridgeFrame InputBridge::frame(std::uint32_t tick,const InputSourceFlags& flags) {
    flags_=flags;
    return frame(tick);
}

BridgeFrame InputBridge::frame(std::uint32_t tick) {
    const auto source=[this,tick]{
    if(sources_)flags_=sources_();
    FrameInput input;
    input.tick=tick;
    input.keys=keys_;
    input.mouse_buttons=mouse_mask_;
    input.joystick_buttons=flags_.joystick_buttons;
    input.look.x=delta_x_;
    input.look.y=delta_y_;
    input.look.scale=flags_.mouse_scale;
    input.look.flip=flags_.mouse_flip;
    input.look.steady=flags_.steady;
    input.input_mode=flags_.input_mode;
    input.live_input=flags_.live_input;
    input.mouse_enabled=!flags_.windowed&&flags_.window_active;
    input.priority_key=flags_.priority_key;
    input.global=flags_.global;
    return input;};
    auto input=source();input.current=source;
    input.next_event=[this]{
        if(ring_read_==ring_write_)return InputKeyEvent{};
        const auto event=ring_[ring_read_];ring_read_=(ring_read_+1)&127;return event;
    };
    input.on_global_key=global_effect_;input.on_chat_key=chat_;
    input.on_mouse_sampled=[this]{delta_x_=0;delta_y_=0;};
    const auto output=stage_.begin_frame(input);
    BridgeFrame result;
    result.commands=stage_.take_due(tick);
    result.immediate=output.immediate;
    result.effects=output.effects;
    result.digits=output.digits;
    result.pages=output.pages;
    result.chat_keys=output.chat_keys;
    result.enter_flag_set=output.enter_flag_set;
    result.focus_flag=stage_.focus_flag();
    return result;
}

} // namespace dfre
