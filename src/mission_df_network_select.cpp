#include "dfre/mission_df_network_select.hpp"
#include <stdexcept>
namespace dfre {namespace {using U=std::uint32_t;using B=MissionGameTransportBindings;
U r(const B&b,U a,U n=4){if(!b.read)throw std::runtime_error("DF network current read is not bound");return b.read(a,n);}
void w(const B&b,U a,U v){if(!b.write)throw std::runtime_error("DF network current store is not bound");b.write(a,4,v);}
U call(const B&b,U s,const std::vector<U>&args={}){if(b.before_call)b.before_call(s,args);if(s==0x4974a0)return mission_df_network_select_call(b,s);if(!b.call)throw std::runtime_error("DF network required service is not bound");return b.call(s,args);}
}
U mission_df_network_select_call(const B&b,U s,U f){switch(s){
case 0x497370:{const auto selector=r(b,f+4),selected=selector==1?0x501ab8u:0x501a28u;if(selected!=r(b,0x50139c)){if(r(b,0x501320))call(b,0x4974a0);w(b,0x501398,selected);w(b,0x50139c,selected);}return 0;}
case 0x4974a0:{if(r(b,0x501320)){const auto table=r(b,0x501398),target=r(b,table+0x1c);call(b,target);}return 0;}
case 0x4b8f20:{const auto descriptor=r(b,f+4),address=r(b,descriptor);call(b,0x4c5556,{address});w(b,0x501324,2);return 0;}
case 0x4b8f40:{if(!r(b,0x501320))return 1;if(!r(b,0x501324))return 0;call(b,0x4c5550);w(b,0x501324,0);return 0;}
default:throw std::runtime_error("DF network source entry is not supported");}}
} // namespace dfre
