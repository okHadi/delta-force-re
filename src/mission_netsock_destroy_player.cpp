#include "dfre/mission_netsock_destroy_player.hpp"
#include "dfre/mission_netsock_send_exports.hpp"
#include "dfre/mission_netsock_dispatch.hpp"
#include "dfre/mission_netsock_outgoing.hpp"
#include "dfre/mission_netsock_periodic.hpp"
#include <stdexcept>
namespace dfre {namespace {using U=std::uint32_t;using B=MissionNetsockDestroyPlayerBindings;
U r(const B&b,U a,U n=4){if(!b.read)throw std::runtime_error("DestroyPlayer current read is not bound");return b.read(a,n);}
void w(const B&b,U a,U v,U n=4){if(!b.write)throw std::runtime_error("DestroyPlayer current store is not bound");b.write(a,n,v);}
struct Frame{const B&b;U site,n,at;Frame(const B&v,U s,U bytes,const std::vector<U>&args):b(v),site(s),n(bytes),at(0){if(!b.frame||!b.release)throw std::runtime_error("DestroyPlayer current frame is not bound");at=b.frame(site,n,args);}~Frame(){b.release(site,at,n);}};
U invoke(const B&b,U s,const std::vector<U>&args){if(b.before_call)b.before_call(s,args);U n=0;switch(s){case 0x10001b10:case 0x10001b40:case 0x10001b80:case 0x100019e0:case 0x10004490:case 0x10004600:n=8;break;case 0x10004510:case 0x10004550:case 0x100045c0:case 0x10004660:n=0x10;break;case 0x10003fe0:case 0x100041d0:n=0xc;break;case 0x10005240:n=0x14;break;case 0x10002b80:n=0x68;break;default:if(!b.call)throw std::runtime_error("DestroyPlayer required service is not bound");return b.call(s,args);}
Frame f(b,s,n,args);switch(s){case 0x100019e0:return mission_netsock_send_player_lookup(b,f.at);case 0x10003fe0:return mission_netsock_address_lookup(b,f.at);case 0x100041d0:return mission_netsock_player_lookup(b,f.at);case 0x10005240:return mission_netsock_send_direct(b,f.at);case 0x10004490:mission_netsock_player_remove(b,f.at);return 0;case 0x10004600:mission_netsock_address_remove(b,f.at);return 0;case 0x10002b80:mission_netsock_player_usage(b,f.at);return 0;default:return mission_netsock_destroy_player_call(b,s,f.at);}}
}
U mission_netsock_destroy_player_call(const B&b,U s,U f){switch(s){
case 0x10001b10:{const auto peer=r(b,f+4);const auto player=invoke(b,0x100019e0,{peer});if(!player)return 0xb;invoke(b,0x10004490,{player});return 0;}
case 0x10001b40:{const auto peer=r(b,f+4),sleep=r(b,0x10006010);w(b,0x10019c04,peer);w(b,0x10019c00,0x10000);w(b,0x10019bfc,0);w(b,0x10019bf8,1);do{invoke(b,sleep,{10});}while(!r(b,0x10019bfc));return 0;}
case 0x10001b80:{if(!r(b,0x10019880))return 1;if(!r(b,0x10019884))return 0x200;if(!r(b,0x100198ac))return 4;const auto role=r(b,0x100199b0),peer=r(b,f+4);return invoke(b,role==1?0x10001b10:0x10001b40,{peer});}
case 0x10004510:case 0x100045c0:{const auto packet=r(b,0x10019c5c),status=r(b,f+4),port=r(b,f+0xc);w(b,packet,s==0x10004510?0x1033:0x1041,2);w(b,packet+2,8,2);w(b,packet+4,status);const auto ip=r(b,f+8),current=r(b,0x10019c5c);invoke(b,0x10005240,{current,8,ip,port});return 0;}
case 0x10004550:{const auto ip=r(b,f+8),port=r(b,f+0xc);if(!invoke(b,0x10003fe0,{ip,port}))return 0;const auto player=invoke(b,0x100041d0,{ip,port});if(player){const auto packet=r(b,f+4),id=r(b,player+0xa0),requested=r(b,packet+4);if(id==requested){invoke(b,0x10002b80,{player,3});invoke(b,0x10004490,{player});invoke(b,0x10004510,{0,ip,port});return 0;}}invoke(b,0x10004510,{1,ip,port});return 0;}
case 0x10004660:{const auto port=r(b,f+0xc),ip=r(b,f+8);const auto address=invoke(b,0x10003fe0,{ip,port});if(!address){invoke(b,0x100045c0,{1,ip,port});return 0;}const auto player=invoke(b,0x100041d0,{ip,port});if(player)invoke(b,0x10002b80,{player,3});invoke(b,0x10004600,{address});invoke(b,0x100045c0,{0,ip,port});return 0;}
default:throw std::runtime_error("DestroyPlayer source entry is not supported");}}
} // namespace dfre
