#include "dfre/mission_debrief.hpp"
#include <stdexcept>
namespace dfre {namespace {
using U=std::uint32_t;
template<class T> const T& need(const T& f){if(!f)throw std::runtime_error("Debrief reached a missing provider");return f;}
U read(const MissionDebriefBindings& b,U at){return need(b.read)(at,4);}
void write(const MissionDebriefBindings& b,U at,U n){need(b.write)(at,4,n);}
U call(const MissionDebriefBindings& b,U site,const std::vector<U>& args={}){return need(b.call)(site,args);}
}
void mission_debrief_enter(const MissionDebriefBindings& b,U frame){
    call(b,0x407850);call(b,0x40d550);
    const auto menu=call(b,0x40bfd0,{0x4cae28});write(b,0x68bef4,menu);call(b,0x40bca0,{menu});
    const auto current_menu=read(b,0x68bef4);call(b,0x40e4d0,{current_menu});
    for(U offset=0;offset<0x514;offset+=4){const auto n=read(b,0x67f714+offset);write(b,0x4e7544+offset,n);}
    const auto old_table=read(b,0x503238);
    if(old_table){call(b,0x4980d0,{old_table});write(b,0x503238,0);}
    const auto profile=read(b,0x4cf2e4);const auto campaign=read(b,profile+0x28);
    call(b,0x493a80,{frame,0x4cae34,campaign+1});
    const auto table=call(b,0x499f1f,{frame});write(b,0x503238,table);
    call(b,0x413440,{0x4e6d08});
    const auto text=call(b,0x497f60,{0x2710,4,0,0x4cae44});write(b,0x68bef0,text);
    if(!text){call(b,0x4564b0,{0x4cae54});return;}
    call(b,0x49a620,{text,0,0x2710});
    call(b,0x41ef40);call(b,0x41f320);
    const auto save_profile=read(b,0x4cf2e4);call(b,0x409180,{save_profile});
    call(b,0x41ef80);
    const auto font=call(b,0x40d040,{0x4cae78});const auto current_text=read(b,0x68bef0);
    call(b,0x413460,{current_text,0x56,0x68,0x1d4,0x110,font,0,0xff00ff00,0x4e6d08,0});
    const auto final_profile=read(b,0x4cf2e4);const auto final_campaign=read(b,final_profile+0x28);write(b,0x4e6d00,final_campaign);
    const auto mission=read(b,0x68ba34);write(b,0x4e6d04,mission);
    call(b,0x422ef0,{1});call(b,0x4230a0);call(b,0x40dd80,{1,0});
}
U mission_debrief_update(const MissionDebriefBindings& b){
    call(b,0x40d9e0);
    const auto mode=read(b,0x4e9bf0);
    if(mode){const auto display=read(b,0x4e9bdc);if(display)call(b,0x4132f0,{0x4e6d08});}
    const auto update=read(b,0x4e9c1c);call(b,update);
    const auto closing=read(b,0xcc08cc);
    if(!closing)return call(b,0x461350)!=0?1u:0u;
    call(b,0x40d820);call(b,0x461340,{0});return 1;
}
void mission_debrief_render(const MissionDebriefBindings& b){
    const auto initial_menu=read(b,0x68bef4);if(!initial_menu)return;
    const auto mode=read(b,0x4e9bf0);
    if(mode){const auto display=read(b,0x4e9bdc);if(!display){const auto draw=read(b,0x4e9c18);call(b,draw);call(b,0x496c50);return;}}
    const auto menu=read(b,0x68bef4);call(b,0x40fa90,{menu});
    const auto current_mode=read(b,0x4e9bf0);
    if(current_mode){const auto display=read(b,0x4e9bdc);if(display)call(b,0x4133c0,{0x4e6d08});}
    if(read(b,0x4e9bf0))call(b,0x496c50);else call(b,0x4230e0);
}
void mission_debrief_leave(const MissionDebriefBindings& b){const auto text=read(b,0x68bef0);call(b,0x4980d0,{text});write(b,0x68bef0,0);}
void mission_debrief_accept(const MissionDebriefBindings& b,U button,U action){
    switch(action){
    case 1:call(b,0x40e2a0,{button,3});return;
    case 2:call(b,0x40e2a0,{button,1});call(b,0x461340,{0x4d0cd4});return;
    case 6:call(b,0x40e2a0,{button,2});return;
    case 7:call(b,0x40e2a0,{button,1});return;
    case 8:mission_debrief_accept(b,button,2);return;
    default:return;
    }
}
void mission_debrief_quit(const MissionDebriefBindings& b,U button,U action){
    switch(action){
    case 1:call(b,0x40b9a0);call(b,0x40e2a0,{button,3});call(b,0x461340,{0x4e6784});return;
    case 8:call(b,0x461340,{0x4e6784});return;
    case 2:case 7:call(b,0x40e2a0,{button,1});return;
    case 6:call(b,0x40e2a0,{button,2});return;
    default:return;
    }
}
}
