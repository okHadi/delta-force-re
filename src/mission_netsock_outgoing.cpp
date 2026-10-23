#include "dfre/mission_netsock_outgoing.hpp"
#include <stdexcept>
namespace dfre {namespace {using U=std::uint32_t;
U read(const MissionNetsockOutgoingBindings& b,U at,U n=4){if(!b.read)throw std::runtime_error("Netsock outgoing current read is not bound");return b.read(at,n);}
void write(const MissionNetsockOutgoingBindings& b,U at,U value,U n){if(!b.write)throw std::runtime_error("Netsock outgoing current store is not bound");b.write(at,n,value);}
struct Frame{const MissionNetsockOutgoingBindings& b;U site,size,at;Frame(const MissionNetsockOutgoingBindings& source,U target,U bytes,const std::vector<U>& args):b(source),site(target),size(bytes),at(0){if(!b.frame||!b.release)throw std::runtime_error("Netsock outgoing caller frame is not bound");at=b.frame(site,size,args);}~Frame(){b.release(site,at,size);}};
U call(const MissionNetsockOutgoingBindings& b,U site,const std::vector<U>& args){if(b.before_call)b.before_call(site,args);if(site==0x10003c00){Frame f(b,site,0x18,args);return mission_netsock_queue_pop(b,f.at);}if(site==0x10003dd0||site==0x10005240||site==0x100058d0){Frame f(b,site,0x14,args);if(site==0x10003dd0)return mission_netsock_send_select(b,f.at);return site==0x10005240?mission_netsock_send_direct(b,f.at):mission_netsock_send_compressed(b,f.at);}if(!b.call)throw std::runtime_error("Netsock outgoing leaf send or import is not bound");return b.call(site,args);}
U port_send(const MissionNetsockOutgoingBindings& b,U frame,U leaf){const auto port=read(b,frame+0x10,2),ip=read(b,frame+0xc),length=read(b,frame+8),saved=read(b,0x10019868,2);write(b,0x10019868,port,2);const auto data=read(b,frame+4);const auto result=call(b,leaf,{data,length,ip});write(b,0x10019868,saved,2);return result;}
}
void mission_netsock_outgoing(const MissionNetsockOutgoingBindings& b,std::uint32_t frame){auto buffer=read(b,0x10019c5c);auto found=call(b,0x10003c00,{buffer,frame+8,0x10019b20,frame+4,frame});while(found){const auto port=read(b,frame),ip=read(b,frame+4),length=read(b,frame+8);buffer=read(b,0x10019c5c);call(b,0x10003dd0,{buffer,length,ip,port});buffer=read(b,0x10019c5c);found=call(b,0x10003c00,{buffer,frame+8,0x10019b20,frame+4,frame});}}
std::uint32_t mission_netsock_send_select(const MissionNetsockOutgoingBindings& b,std::uint32_t frame){const auto compressed=read(b,0x10019c78);const auto port=read(b,frame+0x10),ip=read(b,frame+0xc),length=read(b,frame+8),data=read(b,frame+4);return call(b,compressed?0x100058d0:0x10005240,{data,length,ip,port});}
std::uint32_t mission_netsock_send_direct(const MissionNetsockOutgoingBindings& b,std::uint32_t frame){return port_send(b,frame,0x10005020);}
std::uint32_t mission_netsock_send_compressed(const MissionNetsockOutgoingBindings& b,std::uint32_t frame){return port_send(b,frame,0x10005860);}
} // namespace dfre
