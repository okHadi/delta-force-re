#include "dfre/mission_df_socket.hpp"
#include <cstring>
namespace dfre {namespace {
using U=std::uint32_t;
std::int32_t si(U n){std::int32_t v;std::memcpy(&v,&n,4);return v;}
U read(const MissionDfSocketBindings& b,U at,U width=4){if(!b.read)throw MissionClientStartupError("DF sockets need current memory reads");return b.read(at,width);}
void write(const MissionDfSocketBindings& b,U at,U value,U width=4){if(!b.write)throw MissionClientStartupError("DF sockets need current memory writes");b.write(at,width,value);}
U call(const MissionDfSocketBindings& b,U site,std::vector<U> args){if(b.before_call)b.before_call(site,args);
    if(site==0x454a10)return mission_df_socket_open_send(b,args.at(0));
    if(site==0x454930)return mission_df_socket_open_receive(b,args.at(0),args.at(1));
    if(site==0x454890)return mission_df_socket_set_buffer(b,false,args.at(0));
    if(site==0x454900)return mission_df_socket_set_buffer(b,true,args.at(0));
    if(site==0x454850)return mission_df_socket_get_buffer(b,false);
    if(site==0x4548c0)return mission_df_socket_get_buffer(b,true);
    if(site==0x454ab0)return mission_df_socket_buffers(b);
    if(!b.call)throw MissionClientStartupError("DF sockets need body "+std::to_string(site));return b.call(site,args);
}
struct Frame {
    const MissionDfSocketBindings& b;U site,at,size;
    Frame(const MissionDfSocketBindings& source,U function,U bytes,std::vector<U> arguments={}):b(source),site(function),at(0),size(bytes){if(!b.frame||!b.release)throw MissionClientStartupError("DF sockets need current caller frame memory");at=b.frame(site,size,arguments);}
    ~Frame(){b.release(site,at,size);}
};
}
U mission_df_socket_resolve(const MissionDfSocketBindings& b,U name){
    const auto direct=call(b,0x4c55b0,{name});if(direct!=0xffffffffu)return direct;
    const auto host=call(b,0x4c55ec,{name});if(!host)return host;
    const auto list=read(b,host+0xc),first=read(b,list);return read(b,first);
}
U mission_df_socket_start(const MissionDfSocketBindings& b,U mode,U address,U port){
    const auto startup=call(b,0x4c55f8,{0x101,0x8e1390});if(startup)return call(b,0x4c55e0,{});
    if(address){const auto result=call(b,0x454930,{mode,address});if(result==1)return call(b,0x4c55e0,{});}
    write(b,0x8e3530,mode);const auto incoming=address?read(b,address,2):0;write(b,0x8e354e,incoming,2);write(b,0x8e354c,port&0xffff,2);return call(b,0x454ab0,{});
}
U mission_df_socket_open_send(const MissionDfSocketBindings& b,U input){
    Frame frame(b,0x454a10,16);const auto socket=call(b,0x4c55c8,{2,2,0});write(b,0x8e353c,socket);if(socket==0xffffffffu)return 1;
    U port=(input&0xffff)?input+1:0x4a42,initial=port;
    for(;;){write(b,frame.at+4,0);write(b,frame.at,2,2);const auto network_port=call(b,0x4c55bc,{port&0xffff});write(b,frame.at+2,network_port&0xffff,2);
        const auto current=read(b,0x8e353c);const auto bound=call(b,0x4c55b6,{current,frame.at,16});if(bound!=0xffffffffu)return 0;
        if(call(b,0x4c55e0,{})!=0x2740)return 1;++port;if((port&0xffff)>(initial&0xffff)+100)return 1;
    }
}
U mission_df_socket_open_receive(const MissionDfSocketBindings& b,U mode,U port_address){
    Frame frame(b,0x454930,20);const auto socket=call(b,0x4c55c8,{2,2,0});write(b,0x8e3548,socket);if(socket==0xffffffffu)return 1;
    write(b,frame.at+16,1);if(call(b,0x4c55c2,{socket,0x8004667e,frame.at+16})==0xffffffffu)return 1;
    write(b,frame.at+4,0);write(b,frame.at,2,2);
    if(mode==1){const auto port=read(b,port_address,2);const auto network_port=call(b,0x4c55bc,{port});write(b,frame.at+2,network_port&0xffff,2);const auto current=read(b,0x8e3548);return call(b,0x4c55b6,{current,frame.at,16})==0xffffffffu?mode:0;}
    U port=read(b,port_address,2);
    for(;;){const auto network_port=call(b,0x4c55bc,{port&0xffff});write(b,frame.at+2,network_port&0xffff,2);const auto current=read(b,0x8e3548);if(call(b,0x4c55b6,{current,frame.at,16})!=0xffffffffu){write(b,port_address,port&0xffff,2);return 0;}if(call(b,0x4c55e0,{})!=0x2740)return 1;port=(port&0xffff00ffu)|(((port+0x1000u)&0xff00u));}
}
U mission_df_socket_set_buffer(const MissionDfSocketBindings& b,bool receive,U bytes){
    const U site=receive?0x454900:0x454890,field=receive?0x8e3548:0x8e353c,option=receive?0x1002:0x1001;
    Frame frame(b,site,4,{bytes});const auto socket=read(b,field);if(call(b,0x4c55ce,{socket,0xffff,option,frame.at,4})==0xffffffffu)return call(b,0x4c55e0,{});return 0;
}
U mission_df_socket_get_buffer(const MissionDfSocketBindings& b,bool receive){
    const U site=receive?0x4548c0:0x454850,field=receive?0x8e3548:0x8e353c,option=receive?0x1002:0x1001;
    Frame frame(b,site,8);const auto socket=read(b,field);write(b,frame.at+4,4);if(call(b,0x4c55d4,{socket,0xffff,option,frame.at,frame.at+4})==0xffffffffu)return 0;return read(b,frame.at);
}
U mission_df_socket_buffers(const MissionDfSocketBindings& b){
    U receive=0;const auto port=read(b,0x8e354e,2);if(call(b,0x454a10,{port})==1)return call(b,0x4c55e0,{});
    const auto set_send=call(b,0x454890,{0x10000});if(set_send)return set_send;
    const auto send=call(b,0x454850,{});const auto current_port=read(b,0x8e354e,2);if(current_port){const auto set_receive=call(b,0x454900,{0x10000});if(set_receive)return set_receive;receive=call(b,0x4548c0,{});}
    const auto size=(si(send)>si(receive)?send:receive)+12;write(b,0x8e3544,size);const auto buffer=call(b,0x49dc80,{size});write(b,0x8e3540,buffer);if(!buffer)return 0x271e;write(b,0x8e352c,0);return 0;
}
}
