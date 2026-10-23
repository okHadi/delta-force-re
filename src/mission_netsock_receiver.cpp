#include "dfre/mission_netsock_receiver.hpp"
#include <stdexcept>
namespace dfre {namespace {
using U=std::uint32_t;
U read(const MissionNetsockReceiverBindings& b,U at,U width=4){if(!b.read)throw std::runtime_error("Netsock receiver current read is not bound");return b.read(at,width);}
void write(const MissionNetsockReceiverBindings& b,U at,U value,U width=4){if(!b.write)throw std::runtime_error("Netsock receiver current store is not bound");b.write(at,width,value);}
std::int64_t si(U n){return n<=0x7fffffffu?std::int64_t(n):std::int64_t(n)-0x100000000ll;}
struct Frame {const MissionNetsockReceiverBindings& b;U at;Frame(const MissionNetsockReceiverBindings& source,U ip):b(source),at(0){if(!b.frame||!b.release)throw std::runtime_error("Netsock receiver current caller frame is not bound");at=b.frame(0x10005280,8,{ip});}~Frame(){b.release(0x10005280,at,8);}};
U call(const MissionNetsockReceiverBindings& b,U target,const std::vector<U>& args){if(b.before_call)b.before_call(target,args);if(target==0x10005280){Frame f(b,args.at(0));return mission_netsock_ip_filter(b,f.at);}if(!b.call)throw std::runtime_error("Netsock receiver required OS or codec body is not bound");return b.call(target,args);}
void clear_outputs(const MissionNetsockReceiverBindings& b,U frame){const auto ip=read(b,frame+0x3c),port=read(b,frame+0x40),length=read(b,frame+0x44);write(b,ip,0);write(b,port,0,2);write(b,length,0);}
U unscramble(const MissionNetsockReceiverBindings& b,U at,U seed){U extracted=0;for(U bit=0;bit<32;++bit){const auto byte=read(b,at+bit,1)&255u;extracted|=(byte&1u)<<bit;write(b,at+bit,((byte^seed)&1u)^byte,1);seed>>=1;}return extracted;}
U crc_copy(const MissionNetsockReceiverBindings& b,U source,U destination,U count){U crc=0xffffffffu;do{const auto byte=read(b,source,1)&255u;write(b,destination,byte,1);const U index=((crc>>24)^byte)&255u;++source;++destination;crc=(crc<<8)^read(b,0x10007234+index*4);--count;}while(si(count)>0);return crc;}
}
U mission_netsock_ip_filter(const MissionNetsockReceiverBindings& b,U frame){
    if(!read(b,0x100096b8))return 0;const auto ip=read(b,frame+4);const auto host=call(b,0x10005958,{ip});const auto count=read(b,0x100096b8);U low=0,high=count-1u;
    while(si(high)>=si(low)){const U sum=high+low;const U mid=(sum>>1)|(sum&0x80000000u);const auto base=read(b,0x100076a8+mid*8),span=read(b,0x100076ac+mid*8);if(host>base+span)low=mid+1;else if(host<base)high=mid-1;else return 1;}return 0;
}
U mission_netsock_receive_poll(const MissionNetsockReceiverBindings& b,U frame){const auto socket=read(b,0x100096b4);const auto result=call(b,0x10005934,{socket,0x4004667f,frame});const auto bytes=read(b,frame);return(result+1u?0xffffffffu:0)&bytes;}
U mission_netsock_receiver(const MissionNetsockReceiverBindings& b,U frame){
    const auto capacity=read(b,0x1001985c),buffer=read(b,0x10019854),socket=read(b,0x100096b4);write(b,frame+0x1c,0x10);
    const auto received_result=call(b,0x1000595e,{socket,buffer,capacity,0,frame+0x20,frame+0x1c});write(b,frame+0x14,received_result);
    if(received_result==0xffffffffu){clear_outputs(b,frame);const auto error=call(b,0x10005916,{});return error==0x2733?0:error;}
    const auto initial_ip=read(b,frame+0x24);if(call(b,0x10005280,{initial_ip})){clear_outputs(b,frame);return 0x66;}
    U current=read(b,0x10019854),seed=read(b,current),header=seed?4u:read(b,current+9,1)&255u;
    const auto destination_capacity=read(b,frame+0x38);U payload=received_result-header;const bool fits=si(destination_capacity)>=si(payload);write(b,frame+0x10,header);write(b,frame+0x18,payload);if(!fits){clear_outputs(b,frame);return 0x2738;}
    if(si(payload)>=32){if(header!=4)seed=read(b,current+4);const auto extracted=unscramble(b,current+header,seed);header=read(b,frame+0x10);const auto output=read(b,0x10019854);write(b,output+(header==4?0u:4u),extracted);current=read(b,0x10019854);payload=read(b,frame+0x18);}
    auto destination=read(b,frame+0x34);U crc=crc_copy(b,current+header,destination,payload);
    header=read(b,frame+0x10);current=read(b,0x10019854);U expected=read(b,current+(header==4?0u:4u)),received=read(b,frame+0x14);
    if(crc!=expected||si(received)<=si(header)){
        if(si(header)>4){
            seed=read(b,current+4);const auto extracted=unscramble(b,current+header,seed);const auto output=read(b,0x10019854);write(b,frame+0x10,4);write(b,output+4,extracted);
            current=read(b,0x10019854);U recovered=0;for(U bit=0;bit<32;++bit){const auto byte=read(b,current+4+bit,1)&255u;write(b,current+4+bit,byte&254u,1);recovered|=(byte&1u)<<bit;}
            current=read(b,0x10019854);const auto length=read(b,frame+0x14);destination=read(b,frame+0x34);write(b,current,recovered);const auto input=read(b,0x10019854);crc=crc_copy(b,input+4,destination,length-4u);
            current=read(b,0x10019854);header=read(b,frame+0x10);received=read(b,frame+0x14);expected=read(b,current);
        }
        if(crc!=expected||si(received)<=si(header)){clear_outputs(b,frame);return 0x271e;}
    }
    if(si(header)>4&&(read(b,current+8,1)&1u)){
        if(!read(b,0x10019860)){clear_outputs(b,frame);return 0x6a;}
        destination=read(b,frame+0x34);const auto codec=read(b,0x100096ac);const auto expanded=call(b,codec,{destination,0x10009854,0x10000});
        const auto ip=read(b,frame+0x24);U offset=0;for(U i=0;i<(expanded>>2);++i,offset+=4){const auto value=read(b,0x10009854+offset);write(b,destination+offset,value);}
        const auto raw_port=read(b,frame+0x22);for(U i=0;i<(expanded&3);++i,++offset){const auto value=read(b,0x10009854+offset,1);write(b,destination+offset,value,1);}
        const auto ip_output=read(b,frame+0x3c);write(b,ip_output,ip);const auto port=call(b,0x1000592e,{raw_port});const auto port_output=read(b,frame+0x40),length_output=read(b,frame+0x44);write(b,port_output,port&0xffffu,2);write(b,length_output,expanded);return 0;
    }
    const auto ip_output=read(b,frame+0x3c),raw_port=read(b,frame+0x22),ip=read(b,frame+0x24);write(b,ip_output,ip);const auto port=call(b,0x1000592e,{raw_port});const auto port_output=read(b,frame+0x40),length_output=read(b,frame+0x44);write(b,port_output,port&0xffffu,2);write(b,length_output,received-header);return 0;
}
} // namespace dfre
