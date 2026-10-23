#include "dfre/mission_netsock_host_worker.hpp"
#include "dfre/mission_netsock_queue.hpp"
#include <stdexcept>
namespace dfre {namespace {using U=std::uint32_t;
U read(const MissionNetsockHostWorkerBindings& b,U at){if(!b.read)throw std::runtime_error("Netsock host worker current read is not bound");return b.read(at,4);}
void write(const MissionNetsockHostWorkerBindings& b,U at,U value){if(!b.write)throw std::runtime_error("Netsock host worker current store is not bound");b.write(at,4,value);}
struct Frame {const MissionNetsockHostWorkerBindings& b;U site,size,at;Frame(const MissionNetsockHostWorkerBindings& source,U target,U bytes,const std::vector<U>& args):b(source),site(target),size(bytes),at(0){if(!b.frame||!b.release)throw std::runtime_error("Netsock host worker caller frame is not bound");at=b.frame(site,size,args);}~Frame(){b.release(site,at,size);}};
U call(const MissionNetsockHostWorkerBindings& b,U target,const std::vector<U>& args){
 if(b.before_call)b.before_call(target,args);
 if(target==0x10005830){Frame frame(b,target,4,args);return mission_netsock_receive_poll(b,frame.at);}
 if(target==0x100052e0){Frame frame(b,target,0x48,args);return mission_netsock_receiver(b,frame.at);}
 if(target==0x10003ad0){Frame frame(b,target,8,args);MissionNetsockQueueBindings queue;queue.read=b.read;queue.write=b.write;queue.resolve=b.resolve;queue.call=b.call;queue.before_call=b.before_call;mission_netsock_queue_destroy(queue,frame.at);return 0;}
 if(!b.call)throw std::runtime_error("Netsock host worker required import or handler is not bound");return b.call(target,args);
}
}
std::uint32_t mission_netsock_host_worker(const MissionNetsockHostWorkerBindings& b,std::uint32_t frame){
 U admit=1;write(b,frame,0);const auto initial_clock=read(b,0x10006088);const auto initial=call(b,initial_clock,{});write(b,0x10019b34,initial);
 const auto initial_stop=read(b,0x10019b08);
 if(!initial_stop){const auto random=read(b,0x10006044),sleep=read(b,0x10006010);do{
  auto available=call(b,0x10005830,{});
  while(available){const auto capacity=read(b,0x10019c64),buffer=read(b,0x10019c5c);call(b,0x100052e0,{buffer,capacity,frame+4,frame,frame+8});const auto drop=read(b,0x10019ca2);
   if(drop){const auto value=call(b,random,{});const auto mask=read(b,0x10019ca2);admit=((value>>4)|((value&0x80000000u)?0xf0000000u:0))&mask;}
   const auto ip=read(b,frame+4);if(ip&&admit){const auto port=read(b,frame);call(b,0x100046c0,{ip,port});}available=call(b,0x10005830,{});
  }
  call(b,0x100048c0,{});call(b,0x100028c0,{});const auto clock=read(b,0x10006088);const auto now=call(b,clock,{});const auto last=read(b,0x10019b34);
  if(now>last+1000u){call(b,0x10004830,{});write(b,0x10019b34,now);}call(b,0x100020d0,{now});call(b,sleep,{5});
 }while(!read(b,0x10019b08));}
 call(b,0x10003ad0,{0x10019b0c});call(b,0x10003ad0,{0x10019b20});call(b,0x10002d90,{});const auto end=read(b,0x10006048);return call(b,end,{});
}
} // namespace dfre
