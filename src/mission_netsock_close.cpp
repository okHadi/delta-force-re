#include "dfre/mission_netsock_close.hpp"
#include "dfre/mission_netsock_join.hpp"
#include "dfre/mission_netsock_periodic.hpp"
#include "dfre/mission_netsock_receiver.hpp"
#include "dfre/mission_netsock_send_leaves.hpp"
#include <stdexcept>
namespace dfre { namespace {
using U=std::uint32_t;using B=MissionNetsockCloseBindings;
U r(const B&b,U a,U n=4){if(!b.read)throw std::runtime_error("Close current read is not bound");return b.read(a,n);}
void w(const B&b,U a,U v,U n=4){if(!b.write)throw std::runtime_error("Close current store is not bound");b.write(a,n,v);}
struct Frame{const B&b;U site,n,at;Frame(const B&v,U s,U bytes,const std::vector<U>&args):b(v),site(s),n(bytes),at(0){if(!b.frame||!b.release)throw std::runtime_error("Close current frame is not bound");at=b.frame(site,n,args);}~Frame(){b.release(site,at,n);}};
U invoke(const B&b,U s,const std::vector<U>&args){if(b.before_call)b.before_call(s,args);U n=0;switch(s){
case 0x10001250:case 0x100012a0:case 0x100057b0:case 0x10005780:case 0x10001990:case 0x100035f0:case 0x10003730:return mission_netsock_close_call(b,s);
case 0x100018d0:n=0x18;break;case 0x100034a0:n=8;break;case 0x10003680:n=0xc;break;
case 0x10001880:return mission_netsock_close_session(b);
case 0x100014c0:return mission_netsock_join_call(b,s);
case 0x10003630:n=0xc;break;case 0x10005830:n=4;break;case 0x100052e0:n=0x48;break;case 0x10005020:n=0x30;break;
default:if(!b.call)throw std::runtime_error("Close required OS, thread, or enum callback is not bound");return b.call(s,args);}
Frame f(b,s,n,args);switch(s){case 0x10003630:return mission_netsock_join_call(b,s,f.at);case 0x10005830:return mission_netsock_receive_poll(b,f.at);case 0x100052e0:return mission_netsock_receiver(b,f.at);case 0x10005020:return mission_netsock_encode_direct(b,f.at);default:return mission_netsock_close_call(b,s,f.at);}}
}
U mission_netsock_close_call(const B&b,U s,U f){switch(s){
case 0x10001250:{if(!r(b,0x10019880))return 1;if(!r(b,0x10019884))return 0x200;if(r(b,0x100198ac))invoke(b,0x10001880,{});if(r(b,0x100199b8))invoke(b,0x10001990,{});invoke(b,0x100057b0,{});w(b,0x10019884,0);return 0;}
case 0x100012a0:{const auto result=invoke(b,0x10001250,{}),free_target=r(b,0x10006068),first=r(b,0x10019c5c);w(b,0x10019880,0);invoke(b,free_target,{first});const auto second=r(b,0x10019c60);invoke(b,free_target,{second});const auto table=r(b,0x100199dc);invoke(b,free_target,{table});return result;}
case 0x100057b0:{const auto socket=r(b,0x100096b4);if(invoke(b,0x1000596a,{socket}))return invoke(b,0x10005916,{});if(invoke(b,0x10005964,{}))return invoke(b,0x10005916,{});const auto buffer=r(b,0x10019854),free_target=r(b,0x10006068);invoke(b,free_target,{buffer});invoke(b,0x10005780,{});return 0;}
case 0x10005780:{if(!r(b,0x10019860))return 0;w(b,0x10019860,0);w(b,0x100096a8,0);w(b,0x100096ac,0);const auto module=r(b,0x100096b0),free_library=r(b,0x10006000);return invoke(b,free_library,{module});}
case 0x10001990:{if(!r(b,0x10019880))return 1;if(!r(b,0x10019884))return 0x200;if(!r(b,0x100199b8))return 8;const auto sleep=r(b,0x10006010);w(b,0x100199bc,1);do{invoke(b,sleep,{10});}while(r(b,0x100199b8));return 0;}
case 0x100018d0:{if(!r(b,0x10019880))return 1;if(!r(b,0x10019884))return 0x200;const auto config=r(b,f+4);if(!config)return 2;const auto callback=r(b,f+0xc);if(!callback)return 2;if(r(b,0x100198ac))return 6;if(r(b,0x100199b8)){invoke(b,0x100035f0,{});return 7;}invoke(b,0x100014c0,{});const auto userdata=r(b,f+0x10),flags=r(b,f+0x14);w(b,0x100199c0,callback);w(b,0x100199c4,userdata);w(b,0x100199c8,flags);for(U i=0;i<4;++i){const auto v=r(b,config+0xe0+i*4);w(b,0x100199cc+i*4,v);}invoke(b,0x10003730,{});return 0;}
case 0x10003730:{const auto table=r(b,0x100199dc);w(b,0x100199bc,0);for(U i=0;i<0x870;++i)w(b,table+i*4,0);const auto begin=r(b,0x1000604c);invoke(b,begin,{0x10003680,0,0});w(b,0x100199b8,1);return 0;}
case 0x100034a0:{const auto packet=r(b,0x10019c5c);w(b,packet,0x1010,2);w(b,packet+2,0x14,2);for(U i=0;i<4;++i){const auto v=r(b,0x100199cc+i*4);w(b,packet+4+i*4,v);}if(r(b,0x10019c9e)){const auto ip=r(b,f+4),port=r(b,0x10019868,2);U result=0;for(U i=0;i<16;++i){const auto current=r(b,0x10019c5c),length=r(b,packet+2,2);result=invoke(b,0x10005020,{current,length,ip});const auto next=r(b,0x10019868,2)+0x1000u;w(b,0x10019868,next,2);}w(b,0x10019868,port,2);return result;}const auto ip=r(b,f+4),current=r(b,0x10019c5c);return invoke(b,0x10005020,{current,0x14,ip});}
case 0x100035f0:{auto row=r(b,0x100199dc)+6;U result=0;for(U i=0;i<32;++i,row+=0x10e){result=r(b,row+0x100);if(result){const auto userdata=r(b,0x100199c4),callback=r(b,0x100199c0);result=invoke(b,callback,{row,userdata});}}return result;}
case 0x10003680:{const auto initial_ip=r(b,0x100198a8);invoke(b,0x100034a0,{initial_ip});if(!r(b,0x100199bc)){const auto sleep=r(b,0x10006010);U iteration=0;do{auto available=invoke(b,0x10005830,{});while(available){const auto capacity=r(b,0x10019c64),packet=r(b,0x10019c5c);invoke(b,0x100052e0,{packet,capacity,f,f+4,f+8});const auto ip=r(b,f);if(ip){const auto port=r(b,f+4);invoke(b,0x10003630,{ip,port});}available=invoke(b,0x10005830,{});}if(++iteration>0x32u){const auto ip=r(b,0x100198a8);iteration=0;invoke(b,0x100034a0,{ip});}invoke(b,sleep,{20});invoke(b,0x100035f0,{});}while(!r(b,0x100199bc));}w(b,0x100199b8,0);const auto end=r(b,0x10006048);return invoke(b,end,{});}
default:throw std::runtime_error("Close source entry is not supported");}}
} // namespace dfre
