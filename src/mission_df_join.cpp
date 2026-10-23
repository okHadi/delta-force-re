#include "dfre/mission_df_join.hpp"
#include <stdexcept>
namespace dfre {namespace {using U=std::uint32_t;using B=MissionGameTransportBindings;
U r(const B&b,U a,U n=4){if(!b.read)throw std::runtime_error("DF Join current read is not bound");return b.read(a,n);}
void w(const B&b,U a,U v,U n=4){if(!b.write)throw std::runtime_error("DF Join current store is not bound");b.write(a,n,v);}
U call(const B&b,U target,const std::vector<U>&args={}){if(b.before_call)b.before_call(target,args);if(!b.call)throw std::runtime_error("DF Join required DLL or memory service is not bound");return b.call(target,args);}
}
U mission_df_join_stop_enum(const B&b){return call(b,0x4c5544);}
U mission_df_join(const B&b,U f){const auto selected=r(b,f+0x110),password=r(b,f+0x114);if(!r(b,0x501320)||!selected)return 0xffffffffu;if(b.before_call)b.before_call(0x4b9080,{});mission_df_join_stop_enum(b);call(b,0x49a620,{f,0,0x100});w(b,f,0x100);for(U i=0;i<4;++i){const auto key=r(b,selected+0x10+i*4);w(b,f+0xd0+i*4,key);}for(U i=0;i<4;++i){const auto app=r(b,0x4cebc4+i*4);w(b,f+0xe0+i*4,app);}if(password){U from=password,to=f+0x90;for(;;){const auto first=r(b,from,1);w(b,to,first,1);if(!first)break;const auto second=r(b,from+1,1);from+=2;w(b,to+1,second,1);to+=2;if(!second)break;}}else w(b,f+0x90,0,1);w(b,0x501368,0);const auto result=call(b,0x4c553e,{f});if(!result)return 0;if(result==9)return 1;if(result==0xf)return 0xfffffffeu;if(result==5)return 0xfffffffdu;return 0xffffffffu;}
} // namespace dfre
