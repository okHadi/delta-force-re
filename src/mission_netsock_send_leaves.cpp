#include "dfre/mission_netsock_send_leaves.hpp"
#include <stdexcept>
namespace dfre {namespace {using U=std::uint32_t;
U read(const MissionNetsockSendLeafBindings& b,U at,U n=4){if(!b.read)throw std::runtime_error("Netsock send current read is not bound");return b.read(at,n);}
void write(const MissionNetsockSendLeafBindings& b,U at,U value,U n=4){if(!b.write)throw std::runtime_error("Netsock send current store is not bound");b.write(at,n,value);}
std::int64_t si(U value){return value<=0x7fffffffu?std::int64_t(value):std::int64_t(value)-0x100000000ll;}
struct Frame {const MissionNetsockSendLeafBindings& b;U site,size,at;Frame(const MissionNetsockSendLeafBindings& source,U target,U bytes,const std::vector<U>& args):b(source),site(target),size(bytes),at(0){if(!b.frame||!b.release)throw std::runtime_error("Netsock send caller frame is not bound");at=b.frame(site,size,args);}~Frame(){b.release(site,at,size);}};
U call(const MissionNetsockSendLeafBindings& b,U site,const std::vector<U>& args){if(b.before_call)b.before_call(site,args);if(site==0x10005020||site==0x10005120){Frame f(b,site,site==0x10005020?0x30:0x40,args);return site==0x10005020?mission_netsock_encode_direct(b,f.at):mission_netsock_encode_extended(b,f.at);}if(!b.call)throw std::runtime_error("Netsock send required codec or OS import is not bound");return b.call(site,args);}
U encode(const MissionNetsockSendLeafBindings& b,U frame,bool extended){const U arg=extended?0x30:0x24,header=extended?10u:4u,sockaddr=frame+(extended?0x1c:0x10);const auto initial_length=read(b,frame+arg+4);if(!initial_length)return 0;const U limit=read(b,0x1001985c)-4u;if(si(initial_length)>si(limit))return 0x2738;
 const auto initial_buffer=read(b,0x10019854),source=read(b,frame+arg);U source_at=source,destination=initial_buffer+header,count=initial_length,crc=0xffffffffu;
 do{const auto byte=read(b,source_at,1)&255u;const U index=((crc>>24)^byte)&255u;write(b,destination,byte,1);const auto row=read(b,0x10007234+index*4);++source_at;++destination;crc=(crc<<8)^row;--count;}while(count);
 const auto current_length=read(b,frame+arg+4);U packed=crc,high=initial_buffer&0xffff0000u;
 if(si(current_length)>=32){const auto buffer=read(b,0x10019854);packed=0;U seed=crc;for(U bit=0;bit<32;++bit){const auto byte=read(b,buffer+header+bit,1)&255u;packed|=(byte&1u)<<bit;write(b,buffer+header+bit,((byte^seed)&1u)^byte,1);seed>>=1;}high=0;}
 U ip=0,port=0;
 if(extended){const auto compressed=read(b,frame+0x3c),buffer=read(b,0x10019854);write(b,frame+0x19,10,1);write(b,buffer,0);ip=read(b,frame+0x38);write(b,frame+0x18,compressed?1u:0u,1);const auto fields=read(b,frame+0x18,2);write(b,buffer+4,packed);write(b,buffer+8,fields,2);port=(buffer&0xffff0000u)|read(b,0x10019868,2);
 }else{const auto buffer=read(b,0x10019854);ip=read(b,frame+0x2c);write(b,buffer,packed);port=high|read(b,0x10019868,2);}
 write(b,sockaddr,2,2);write(b,sockaddr+4,ip);const auto network_port=call(b,0x1000592e,{port});const auto buffer=read(b,0x10019854);write(b,sockaddr+2,network_port&0xffffu,2);const U length=read(b,frame+arg+4)+header;const auto socket=read(b,0x100096b4);const auto result=call(b,0x10005952,{socket,buffer,length,0,sockaddr,16});if(result==0xffffffffu)return call(b,0x10005916,{});return 0;
}
}
std::uint32_t mission_netsock_encode_direct(const MissionNetsockSendLeafBindings& b,std::uint32_t frame){return encode(b,frame,false);}
std::uint32_t mission_netsock_encode_extended(const MissionNetsockSendLeafBindings& b,std::uint32_t frame){return encode(b,frame,true);}
std::uint32_t mission_netsock_encode_select(const MissionNetsockSendLeafBindings& b,std::uint32_t frame){if(!read(b,0x10019860))return 0x6a;const auto length=read(b,frame+8),data=read(b,frame+4),codec=read(b,0x100096a8);const auto bits=call(b,codec,{data,length,0x10009854,0x10000});U bytes=(bits>>3)|((bits&0x80000000u)?0xe0000000u:0u);if(bits&7u)++bytes;const auto ip=read(b,frame+0xc);if(bytes+10u<length+4u)return call(b,0x10005120,{0x10009854,bytes,ip,1});return call(b,0x10005020,{data,length,ip});}
} // namespace dfre
