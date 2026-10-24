#include "dfre/mission_df_host_controls.hpp"
#include <stdexcept>
namespace dfre {namespace {using U=std::uint32_t;using B=MissionGameTransportBindings;
U r(const B&b,U a,U n=4){if(!b.read)throw std::runtime_error("DF host control current read is not bound");return b.read(a,n);}
void w(const B&b,U a,U v,U n=4){if(!b.write)throw std::runtime_error("DF host control current store is not bound");b.write(a,n,v);}
U call(const B&b,U s,const std::vector<U>&args={}){if(b.before_call)b.before_call(s,args);if(!b.call)throw std::runtime_error("DF host control required service is not bound");return b.call(s,args);}
void string(const B&b,U to,U from){for(U at=0;;++at){const auto c=r(b,from+at,1);w(b,to+at,c,1);if(!c)return;}}
}
U mission_df_host_control_call(const B&b,U s,U f){switch(s){
case 0x4b91c0:{const auto password=r(b,f+0x11c);if(!r(b,0x501320))return 0;call(b,0x49a620,{f,0,0x100});const auto capacity=r(b,f+0x118);w(b,f+8,capacity);const auto first=r(b,f+0x120);w(b,f+0xf0,first);const auto second=r(b,f+0x124);w(b,f+0xf4,second);const auto third=r(b,f+0x128);w(b,f,0x100);w(b,f+0xf8,third);const auto fourth=r(b,f+0x12c);w(b,f+4,7);w(b,f+0xfc,fourth);if(password)string(b,f+0x90,password);else w(b,f+0x90,0,1);for(U i=0;i<4;++i){const auto v=r(b,0x4cebc4+i*4);w(b,f+0xe0+i*4,v);}const auto name=r(b,f+0x110);string(b,f+0x10,name);call(b,0x4c5538,{f});w(b,0x501368,1);return 0;}
case 0x4b92f0:return call(b,0x4c5532);
case 0x4b9300:{if(!r(b,0x501320))return 0;const auto flags=r(b,f+0x9c)?1u:0u;call(b,0x49a620,{f,0,0x88});const auto name=r(b,f+0x98);string(b,f,name);const auto data=r(b,f+0xa0);w(b,f+0x80,data);const auto size=r(b,f+0xa4);w(b,f+0x84,size);if(call(b,0x4c552c,{0x50133c,f,flags}))return 1;if(r(b,f+0x9c)){const auto peer=r(b,0x50133c);w(b,0x501340,peer);}return 0;}
case 0x4b93d0:{if(!r(b,0x501320))return 0;const auto peer=r(b,0x50133c);call(b,0x4c5526,{peer});return 0;}
default:throw std::runtime_error("DF host control source entry is not supported");}}
} // namespace dfre
