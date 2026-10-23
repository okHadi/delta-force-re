#include "dfre/mission_netsock_admission.hpp"
#include "dfre/mission_netsock_dispatch.hpp"
#include "dfre/mission_netsock_host_setup.hpp"
#include "dfre/mission_netsock_outgoing.hpp"
#include "dfre/mission_netsock_periodic.hpp"
#include <stdexcept>
namespace dfre {namespace {using U=std::uint32_t;using B=MissionNetsockAdmissionBindings;
U r(const B&b,U a,U n=4){if(!b.read)throw std::runtime_error("Admission current read is not bound");return b.read(a,n);}
void w(const B&b,U a,U v,U n=4){if(!b.write)throw std::runtime_error("Admission current store is not bound");b.write(a,n,v);}
struct Frame{const B&b;U site,n,at;Frame(const B&v,U s,U bytes,const std::vector<U>&args):b(v),site(s),n(bytes),at(0){if(!b.frame||!b.release)throw std::runtime_error("Admission current frame is not bound");at=b.frame(site,n,args);}~Frame(){b.release(site,at,n);}};
U invoke(const B&b,U s,const std::vector<U>&args){if(b.before_call)b.before_call(s,args);U n=0;switch(s){case 0x10003f00:case 0x100040c0:n=0x10;break;case 0x10003ed0:case 0x10004090:case 0x10003fe0:case 0x10004050:n=0xc;break;case 0x10003f80:case 0x10005240:n=0x14;break;case 0x10004600:n=8;break;default:if(!b.call)throw std::runtime_error("Admission required service is not bound");return b.call(s,args);}
Frame f(b,s,n,args);switch(s){case 0x10003fe0:return mission_netsock_address_lookup(b,f.at);case 0x10004050:return mission_netsock_host_address(b,f.at);case 0x10004600:mission_netsock_address_remove(b,f.at);return 0;case 0x10005240:return mission_netsock_send_direct(b,f.at);default:return mission_netsock_admission_call(b,s,f.at);}}
bool key_equal(const B&b,U packet){for(U i=0;i<4;++i){const auto server=r(b,0x10019990+i*4),client=r(b,packet+4+i*4);if(server!=client)return false;}return true;}
bool password_equal(const B&b,U packet){U client=packet+0x14,server=0x10019940;for(;;){const auto first=r(b,client,1),expected=r(b,server,1);if(first!=expected)return false;if(!first)return true;const auto second=r(b,client+1,1),next=r(b,server+1,1);if(second!=next)return false;client+=2;server+=2;if(!second)return true;}}
}
U mission_netsock_admission_call(const B&b,U s,U f){switch(s){
case 0x10003f00:{const auto input=r(b,f+4);if(!key_equal(b,input))return 0;const auto port=r(b,f+0xc),ip=r(b,f+8);invoke(b,0x10003ed0,{ip,port});const auto packet=r(b,0x10019c5c);w(b,packet,0x1011,2);w(b,packet+2,0x104,2);for(U i=0;i<0x40;++i){const auto v=r(b,0x100198b0+i*4);w(b,packet+4+i*4,v);}if(r(b,packet+8,1)&8u)for(U i=0;i<0x10;++i)w(b,packet+0x94+i*4,0);const auto current=r(b,0x10019c5c),length=r(b,packet+2,2);invoke(b,0x10005240,{current,length,ip,port});return 0;}
case 0x10003ed0:{const auto port=r(b,f+8),ip=r(b,f+4);const auto address=invoke(b,0x10003fe0,{ip,port});if(address&&r(b,address+0xc))invoke(b,0x10004600,{address});return 0;}
case 0x10004090:{const auto port=r(b,f+8),ip=r(b,f+4);const auto address=invoke(b,0x10003fe0,{ip,port});return address?address:invoke(b,0x10004050,{ip,port});}
case 0x10003f80:{const auto packet=r(b,0x10019c5c),status=r(b,f+4,2),reason=r(b,f+8,2);w(b,packet,0x1021,2);w(b,packet+2,0xc,2);w(b,packet+4,status,2);w(b,packet+6,reason,2);const auto host=r(b,0x10019b00);const U peer=host?r(b,host+0xa0):0xffffffffu;w(b,packet+8,peer);const auto port=r(b,f+0x10),ip=r(b,f+0xc),current=r(b,0x10019c5c);invoke(b,0x10005240,{current,0xc,ip,port});return 0;}
case 0x100040c0:{const auto packet=r(b,f+4);if(!key_equal(b,packet)){const auto port=r(b,f+0xc),ip=r(b,f+8);invoke(b,0x10003f80,{2,2,ip,port});return 0;}if((r(b,0x100198b4,1)&8u)&&!password_equal(b,packet)){const auto port=r(b,f+0xc),ip=r(b,f+8);invoke(b,0x10003f80,{2,1,ip,port});return 0;}const auto port=r(b,f+0xc),ip=r(b,f+8);const auto address=invoke(b,0x10004090,{ip,port});invoke(b,0x10003f80,{address?1u:2u,address?0u:3u,ip,port});return 0;}
default:throw std::runtime_error("Admission source entry is not supported");}}
} // namespace dfre
