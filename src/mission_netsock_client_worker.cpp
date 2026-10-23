#include "dfre/mission_netsock_client_worker.hpp"
#include "dfre/mission_netsock_receiver.hpp"
#include "dfre/mission_netsock_queue.hpp"
#include "dfre/mission_netsock_outgoing.hpp"
#include "dfre/mission_netsock_send_leaves.hpp"
#include "dfre/mission_netsock_host_setup.hpp"
#include "dfre/mission_netsock_periodic.hpp"
#include <stdexcept>
namespace dfre {namespace {using U=std::uint32_t;using B=MissionNetsockClientWorkerBindings;
U r(const B&b,U a,U n=4){if(!b.read)throw std::runtime_error("Client current read is not bound");return b.read(a,n);}
void w(const B&b,U a,U v,U n=4){if(!b.write)throw std::runtime_error("Client current store is not bound");b.write(a,n,v);}
struct Frame{const B&b;U site,n,at;Frame(const B&v,U s,U z,const std::vector<U>&a):b(v),site(s),n(z),at(0){if(!b.frame||!b.release)throw std::runtime_error("Client frame is not bound");at=b.frame(s,z,a);}~Frame(){b.release(site,at,n);}};
bool local(U s){switch(s){case 0x10001fe0:case 0x10002e00:case 0x10002e40:case 0x10002e70:case 0x10002ea0:case 0x10002ec0:case 0x10002ee0:case 0x10002f00:case 0x10002f20:case 0x10002f50:case 0x100030a0:case 0x10003130:case 0x10003170:case 0x100031a0:case 0x100031c0:case 0x10003210:case 0x10003260:case 0x10003290:case 0x10003300:case 0x10003470:return true;default:return false;}}
U invoke(const B&b,U s,const std::vector<U>&a){if(b.before_call)b.before_call(s,a);U n=0;switch(s){case 0x10003300:case 0x10003290:n=0xc;break;case 0x10001fe0:case 0x10002e70:n=0xc;break;case 0x10002e40:case 0x10005240:n=0x14;break;case 0x10003b20:n=0x1c;break;case 0x10003c00:n=0x18;break;case 0x100052e0:n=0x48;break;case 0x10005020:n=0x30;break;case 0x100021d0:n=0x104;break;case 0x100020d0:n=0x10;break;case 0x10005830:n=4;break;default:if(!a.empty())n=U(a.size()+1)*4;break;}
 if(local(s)){if(n){Frame f(b,s,n,a);return mission_netsock_client_worker_call(b,s,f.at);}return mission_netsock_client_worker_call(b,s);}
 if(s==0x100020d0){Frame f(b,s,n,a);mission_netsock_ping_timer(b,f.at);return 0;}
 if(s==0x100021d0){Frame f(b,s,n,a);mission_netsock_host_drop_seed(b,f.at);return 0;}
 if(s==0x10005830){Frame f(b,s,n,a);return mission_netsock_receive_poll(b,f.at);}
 if(s==0x100052e0){Frame f(b,s,n,a);return mission_netsock_receiver(b,f.at);}
 if(s==0x10003c00){Frame f(b,s,n,a);return mission_netsock_queue_pop(b,f.at);}
 if(s==0x10005240){Frame f(b,s,n,a);return mission_netsock_send_direct(b,f.at);}
 if(s==0x10005020){Frame f(b,s,n,a);return mission_netsock_encode_direct(b,f.at);}
 if(s==0x10003a60||s==0x10003ad0||s==0x10003b20){Frame f(b,s,n?n:8,a);MissionNetsockQueueBindings q;q.read=b.read;q.write=b.write;q.resolve=b.resolve;q.call=b.call;q.before_call=b.before_call;if(s==0x10003a60)mission_netsock_queue_initialize(q,f.at);else if(s==0x10003ad0)mission_netsock_queue_destroy(q,f.at);else mission_netsock_queue_enqueue(q,f.at);return 0;}
 if(!b.call)throw std::runtime_error("Client required OS or packet service is not bound");return b.call(s,a);
}
void copy(const B&b,U to,U from,U n){for(U i=0;i<n/4;++i){const auto v=r(b,from+i*4);w(b,to+i*4,v);}for(U i=n&~3u;i<n;++i){const auto v=r(b,from+i,1);w(b,to+i,v,1);}}
}
U mission_netsock_client_worker_call(const B&b,U s,U f){switch(s){
case 0x10001fe0:{const auto clock=r(b,0x10006088),now=invoke(b,clock,{});if(!r(b,0x10019880))return 1;if(!r(b,0x10019884))return 0x200;if(r(b,0x10019c88))return 7;const auto port=r(b,f+8,2);w(b,0x10019c90,port,2);if(!port)w(b,0x10019c90,0xf23,2);const auto ip=r(b,f+4);w(b,0x10019c92,ip);if(!ip)w(b,0x10019c92,0xc9d1b3cf);if(!r(b,0x100198ac)){invoke(b,0x10003470,{});const auto current=r(b,0x10019c92);w(b,0x100199b0,2);w(b,0x100199b4,current);}w(b,0x10019c96,now);w(b,0x10019c9a,now-5001u);w(b,0x10019c88,0x100);w(b,0x10019c8c,0);return 0;}
case 0x10003470:{invoke(b,0x10003a60,{0x10019c24});invoke(b,0x10003a60,{0x10019c38});invoke(b,0x100021d0,{});const auto begin=r(b,0x1000604c);invoke(b,begin,{0x10003300,0,0});return 0;}
case 0x10002ea0:case 0x10002ee0:{const auto value=r(b,f+4);const U base=s==0x10002ea0?0x10019bf8:0x10019c10;w(b,base,0);w(b,base+0x10- (s==0x10002ea0?0:4),value);w(b,base+4,1);return 0;}
case 0x10002ec0:case 0x10002f00:{const U gate=s==0x10002ec0?0x10019bf8:0x10019c10;if(r(b,gate)){const auto packet=r(b,f+4),value=r(b,packet+4);invoke(b,s==0x10002ec0?0x10002ea0:0x10002ee0,{value});}return 0;}
case 0x10002e00:{if(r(b,0x10019b4c)){const auto p=r(b,f+4);w(b,0x10019b4c,0);const auto status=r(b,p+4);w(b,0x10019b54,status);if(!r(b,p+4)){const auto id=r(b,p+8);w(b,0x10019bf4,id);}w(b,0x10019b50,1);}return 0;}
case 0x10002e40:{const auto port=r(b,f+0x10),ip=r(b,f+0xc),size=r(b,f+8),data=r(b,f+4);return invoke(b,0x10003b20,{data,size,0x10019c24,ip,0,port});}
case 0x10002e70:{if(!r(b,0x10019c84)){const auto ip=r(b,0x10019c4c),p=r(b,f+4),n=r(b,p+2,2);invoke(b,0x10002e40,{p+8,n-12u,ip,1});}return 0;}
case 0x100031a0:{if(r(b,0x10019bf0)>100){w(b,0x10019bf0,0);invoke(b,0x100030a0,{});}const auto n=r(b,0x10019bf0);w(b,0x10019bf0,n+1);return 0;}
case 0x100031c0:case 0x10003210:{const bool close=s==0x10003210;const U done=close?0x10002ee0:0x10002ea0,counter=close?0x10019c18:0x10019c00,total=close?0x10019c20:0x10019c0c;if(r(b,0x10019c54)){invoke(b,done,{2});return 0;}if(r(b,counter)>(close?25u:50u)){w(b,counter,0);invoke(b,close?0x10003170:0x10003130,{});}const auto n=r(b,counter);w(b,counter,n+1);const auto t=r(b,total)+1;w(b,total,t);if(t>75)invoke(b,done,{2});return 0;}
case 0x10003130:{const auto p=r(b,0x10019c5c);w(b,p,0x1032,2);w(b,p+2,8,2);const auto id=r(b,0x10019c04);w(b,p+4,id);const auto ip=r(b,0x100199b4),current=r(b,0x10019c5c);invoke(b,0x10005020,{current,8,ip});return 0;}
case 0x10003170:{const auto p=r(b,0x10019c5c);w(b,p,0x1040,2);w(b,p+2,4,2);const auto ip=r(b,0x100199b4),current=r(b,0x10019c5c);invoke(b,0x10005020,{current,4,ip});return 0;}
case 0x10003260:{const auto p=r(b,0x10019c5c),v=r(b,f+4);w(b,p,0x1053,2);w(b,p+2,8,2);w(b,p+4,v);invoke(b,0x10002e40,{p,8,0,0});return 0;}
case 0x10002f20:{const auto p=r(b,0x10019c5c);w(b,p,0x1052,2);w(b,p+2,4,2);invoke(b,0x10002e40,{p,4,0,0});w(b,0x10019c54,1);return 0;}
case 0x100030a0:{const auto p=r(b,0x10019c5c);w(b,p,0x1030,2);w(b,p+2,0xa0,2);const auto id=r(b,0x10019b48);w(b,p+4,id);copy(b,p+8,0x10019b58,0x98);const auto n=r(b,0x10019bdc);if(n){w(b,p+2,n+0xa0,2);const auto current=r(b,0x10019c5c),size=r(b,0x10019bdc),from=r(b,0x10019bd8);copy(b,current+0xa0,from,size);}const auto ip=r(b,0x100199b4),size=r(b,p+2,2),current=r(b,0x10019c5c);invoke(b,0x10005020,{current,size,ip});return 0;}
case 0x10003290:{auto pop=[&](){const auto p=r(b,0x10019c5c);return invoke(b,0x10003c00,{p,f+8,0x10019c38,f+4,f});};while(pop()){const auto port=r(b,f),ip=r(b,f+4),size=r(b,f+8),p=r(b,0x10019c5c);invoke(b,0x10005240,{p,size,ip,port});}return 0;}
case 0x10002f50:{const auto expected=r(b,0x100199b4),ip=r(b,f+4);if(ip!=expected)return 0;const auto packet=r(b,0x10019c5c),op=r(b,packet,2)-0x1012u;if(op<=0x6e){const auto index=r(b,0x10003030+op,1),target=r(b,0x10003014+index*4);switch(target){case 0x10002f8a:invoke(b,0x10002e00,{packet});break;case 0x10002fa1:invoke(b,0x10002ec0,{packet});break;case 0x10002fb8:invoke(b,0x10002f00,{packet});break;case 0x10002fcf:invoke(b,0x10002f20,{});break;case 0x10002fe2:invoke(b,0x10002e70,{packet,ip});break;case 0x10002ffa:invoke(b,0x10002180,{packet});break;case 0x10003003:break;default:throw std::runtime_error("Client opcode jump target is not bound");}}w(b,0x10019c50,0);w(b,0x10019c58,0);return 0;}
case 0x10003300:{const auto clock=r(b,0x10006088);auto last=invoke(b,clock,{});U admit=1;if(!r(b,0x10019b40)){do{auto available=invoke(b,0x10005830,{});while(available){const auto capacity=r(b,0x10019c64),p=r(b,0x10019c5c);invoke(b,0x100052e0,{p,capacity,f+4,f+2,f+8});if(r(b,0x10019ca2)){const auto random=r(b,0x10006044),value=invoke(b,random,{}),mask=r(b,0x10019ca2);admit=((value>>4)|((value&0x80000000u)?0xf0000000u:0))&mask;}const auto ip=r(b,f+4);if(ip&&admit)invoke(b,0x10002f50,{ip});available=invoke(b,0x10005830,{});}invoke(b,0x10003290,{});if(r(b,0x10019b4c))invoke(b,0x100031a0,{});if(r(b,0x10019bf8))invoke(b,0x100031c0,{});if(r(b,0x10019c10))invoke(b,0x10003210,{});const auto time=r(b,0x10006088),now=invoke(b,time,{});if(now>last+1000u){const auto n=r(b,0x10019c50)+1;last=now;w(b,0x10019c50,n);if(n>15&&!r(b,0x10019c54)){const auto count=r(b,0x10019c58)+1;w(b,0x10019c58,count);if(count>=15){w(b,0x10019c58,0);invoke(b,0x10003260,{n});}}}invoke(b,0x100020d0,{now});if(r(b,0x10019c50)>120&&!r(b,0x10019c54))invoke(b,0x10002f20,{});const auto sleep=r(b,0x10006010);invoke(b,sleep,{10});}while(!r(b,0x10019b40));}invoke(b,0x10003ad0,{0x10019c24});invoke(b,0x10003ad0,{0x10019c38});w(b,0x10019b44,0);const auto end=r(b,0x10006048);return invoke(b,end,{});}
default:throw std::runtime_error("Client source entry is not supported");}}
} // namespace dfre
