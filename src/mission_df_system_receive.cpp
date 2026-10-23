#include "dfre/mission_df_system_receive.hpp"
#include <stdexcept>
namespace dfre {namespace {using U=std::uint32_t;using B=MissionGameTransportBindings;
U r(const B&b,U a,U n=4){if(!b.read)throw std::runtime_error("DF system current read is not bound");return b.read(a,n);}
void w(const B&b,U a,U v){if(!b.write)throw std::runtime_error("DF system current store is not bound");b.write(a,4,v);}
void call(const B&b,U target,const std::vector<U>&args={}){if(b.before_call)b.before_call(target,args);if(!b.call)throw std::runtime_error("DF system required name or callback body is not bound");b.call(target,args);}
}
void mission_df_system_receive(const B&b,U f){const auto packet=r(b,f+0x98),count=r(b,0x501358);w(b,0x501358,count+1u);const U opcode=r(b,packet,2),index=(opcode-0x1050u)&0xffffu;if(index>3)return;const auto target=r(b,0x4b93f0+index*4);switch(target){
case 0x4b9432:{const auto current=r(b,0x501348),peer=r(b,packet+4),total=r(b,0x50134c);w(b,0x501344,peer);const auto selected=r(b,0x501344);w(b,0x501348,current+1u);w(b,0x50134c,total+1u);call(b,0x4c5520,{selected,f});const auto first=r(b,f+0x80);w(b,0x50136c,first);const auto second=r(b,f+0x84),gate=r(b,0x501374);w(b,0x501370,second);if(gate)call(b,r(b,0x501374));return;}
case 0x4b94a2:{if(!r(b,0x501368))return;const auto peer=r(b,packet+4),current=r(b,0x501348);w(b,0x501344,peer);const auto gate=r(b,0x501378);w(b,0x501348,current-1u);if(gate)call(b,r(b,0x501378));return;}
case 0x4b94d9:{if(r(b,0x501380))call(b,r(b,0x501380));return;}
case 0x4b94f2:{if(r(b,0x501384)){const auto peer=r(b,packet+4);call(b,r(b,0x501384),{peer});}return;}
default:throw std::runtime_error("DF system jump target is not bound");}}
} // namespace dfre
