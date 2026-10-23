#include "dfre/mission_netsock_dispatch.hpp"
#include "dfre/mission_netsock_queue.hpp"
#include <stdexcept>
namespace dfre {namespace {using U=std::uint32_t;
U read(const MissionNetsockDispatchBindings& b,U at,U n=4){if(!b.read)throw std::runtime_error("Netsock dispatch current read is not bound");return b.read(at,n);}
void write(const MissionNetsockDispatchBindings& b,U at,U value){if(!b.write)throw std::runtime_error("Netsock dispatch current store is not bound");b.write(at,4,value);}
struct Frame {const MissionNetsockDispatchBindings& b;U site,size,at;Frame(const MissionNetsockDispatchBindings& source,U target,U bytes,const std::vector<U>& args):b(source),site(target),size(bytes),at(0){if(!b.frame||!b.release)throw std::runtime_error("Netsock dispatch caller frame is not bound");at=b.frame(site,size,args);}~Frame(){b.release(site,at,size);}};
U call(const MissionNetsockDispatchBindings& b,U site,const std::vector<U>& args){
 if(b.before_call)b.before_call(site,args);
 if(site==0x10003fe0||site==0x100041d0){Frame f(b,site,0xc,args);return site==0x10003fe0?mission_netsock_address_lookup(b,f.at):mission_netsock_player_lookup(b,f.at);}
 if(site==0x10004420){Frame f(b,site,0x10,args);mission_netsock_data_gateway(b,f.at);return 0;}
 if(site==0x10004220){Frame f(b,site,0x14,args);mission_netsock_receive_enqueue(b,f.at);return 0;}
 if(site==0x10003b20){Frame f(b,site,0x1c,args);MissionNetsockQueueBindings q;q.read=b.read;q.write=b.write;q.resolve=b.resolve;q.call=b.call;q.before_call=b.before_call;mission_netsock_queue_enqueue(q,f.at);return 0;}
 if(!b.call)throw std::runtime_error("Netsock packet handler is not bound");return b.call(site,args);
}
}
std::uint32_t mission_netsock_address_lookup(const MissionNetsockDispatchBindings& b,std::uint32_t frame){const auto count=read(b,0x100198b8),base=read(b,0x10019af8);if(!count)return 0;const auto port=read(b,frame+8),ip=read(b,frame+4);U at=base;for(U index=0;index<count;++index,at+=0x18){if(read(b,at)&&read(b,at+4)==ip&&read(b,at+0x14)==port)return at;}return 0;}
std::uint32_t mission_netsock_player_lookup(const MissionNetsockDispatchBindings& b,std::uint32_t frame){const auto base=read(b,0x10019afc),count=read(b,0x100198b8);if(!count)return 0;const auto port=read(b,frame+8),ip=read(b,frame+4);U at=base;for(U index=0;index<count;++index,at+=0xbc){if(read(b,at+0x9c)&&read(b,at+0x98)){const auto link=read(b,at+0xa4);if(read(b,link+4)==ip&&read(b,link+0x14)==port)return at;}}return 0;}
void mission_netsock_receive_enqueue(const MissionNetsockDispatchBindings& b,std::uint32_t frame){const auto priority=read(b,frame+0x10),sender=read(b,frame+0xc),length=read(b,frame+8),source=read(b,frame+4);call(b,0x10003b20,{source,length,0x10019b0c,sender,0,priority});}
void mission_netsock_data_gateway(const MissionNetsockDispatchBindings& b,std::uint32_t frame){const auto port=read(b,frame+0xc),ip=read(b,frame+8);const auto connection=call(b,0x10003fe0,{ip,port});if(!connection)return;write(b,connection+8,0);const auto player=call(b,0x100041d0,{ip,port});if(!player)return;const U count=read(b,player+0xb0)+1u;write(b,player+0xb0,count);if(read(b,0x10019c80))return;const auto sender=read(b,player+0xa0),packet=read(b,frame+4);const U length=read(b,packet+2,2)-0xcu;call(b,0x10004220,{packet+8,length,sender,1});}
void mission_netsock_dispatch(const MissionNetsockDispatchBindings& b,std::uint32_t frame){const auto packet=read(b,0x10019c5c);const U index=read(b,packet,2)-0x1010u;if(index>0x70)return;const auto choice=read(b,0x100047a0+index,1);const auto branch=read(b,0x1000477c+choice*4);if(branch==0x1000477b)return;if(branch==0x10004774){call(b,0x10002180,{packet});return;}U target=0;switch(branch){case 0x100046e8:target=0x10003f00;break;case 0x100046fc:target=0x100040c0;break;case 0x10004710:target=0x10004340;break;case 0x10004724:target=0x10004550;break;case 0x10004738:target=0x10004660;break;case 0x1000474c:target=0x10004420;break;case 0x10004760:target=0x100029d0;break;default:throw std::runtime_error("Netsock dispatch jump target is not bound");}const auto port=read(b,frame+8),ip=read(b,frame+4);call(b,target,{packet,ip,port});}
} // namespace dfre
