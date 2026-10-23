#include "dfre/mission_game_transport.hpp"
#include <stdexcept>
#include <string>

namespace dfre {namespace {
using U=std::uint32_t;
template<class F>const F& need(const F& f,const char* message){if(!f)throw std::runtime_error(message);return f;}
U read(const MissionGameTransportBindings& b,U at){return need(b.read,"Game transport current memory read is not bound")(at,4);}
void write(const MissionGameTransportBindings& b,U at,U value){need(b.write,"Game transport current memory store is not bound")(at,4,value);}
void drain(const MissionGameTransportBindings&);
U call(const MissionGameTransportBindings& b,U target,std::vector<U> args={}){
    if(b.before_call)b.before_call(target,args);
    switch(target){
    case 0x4b9530:return mission_game_transport_receive_netsock(b);
    case 0x4b95d0:return mission_game_transport_send_server_netsock(b,args.at(0),args.at(1));
    case 0x4b96b0:return mission_game_transport_send_client_netsock(b,args.at(0),args.at(1),args.at(2));
    case 0x4b8e10:drain(b);return 0;
    default:if(!b.call)throw std::runtime_error("Game transport needs body "+std::to_string(target));return b.call(target,args);
    }
}
void drain(const MissionGameTransportBindings& b){
    if(!read(b,0x501328))return;
    do{const auto head=read(b,0x501328);call(b,0x4b8da0,{head});}while(read(b,0x501328));
}
struct Frame {
    const MissionGameTransportBindings& b;U at;
    explicit Frame(const MissionGameTransportBindings& source):b(source),at(0){
        if(!b.frame||!b.release)throw std::runtime_error("Game transport current caller frame is not bound");
        at=b.frame(0x4b9530,4);
    }
    ~Frame(){b.release(0x4b9530,at,4);}
};
U send(const MissionGameTransportBindings& b,U target,const std::vector<U>& args){
    if(!read(b,0x501320))return 0;
    write(b,0x501364,0);
    for(;;){
        const auto result=call(b,target,args);
        if(result!=0x10)return result?1:0;
        write(b,0x501364,read(b,0x501364)+1u);
        call(b,0x490880);
        if(read(b,0x501364)>0x800u)return 2;
    }
}
}
U mission_game_transport_receive(const MissionGameTransportBindings& b){const auto table=read(b,0x501398);return call(b,read(b,table+0x54));}
U mission_game_transport_send_server(const MissionGameTransportBindings& b,U data,U length){const auto table=read(b,0x501398);return call(b,read(b,table+0x58),{data,length});}
U mission_game_transport_send_client(const MissionGameTransportBindings& b,U data,U length,U peer){const auto table=read(b,0x501398);return call(b,read(b,table+0x60),{data,length,peer});}
U mission_game_transport_receive_netsock(const MissionGameTransportBindings& b){
    Frame frame(b);const auto enabled=read(b,0x501320);write(b,frame.at,0x8000);
    if(!enabled)return 0;
    const auto buffer=read(b,0xd0a540);
    const auto result=call(b,0x4c551a,{buffer,frame.at,0x501344});
    write(b,0x50135c,read(b,0xd0a540));
    write(b,0x501360,read(b,frame.at));
    if(result)return 0;
    const auto count=read(b,0x501354)+1u;const auto peer=read(b,0x501344);
    write(b,0x501354,count);
    if(peer)return read(b,0xd0a540);
    const auto current=read(b,0xd0a540);call(b,0x4b9400,{current});
    return call(b,0x4b9530);
}
U mission_game_transport_send_server_netsock(const MissionGameTransportBindings& b,U data,U length){return send(b,0x4c5514,{data,length});}
U mission_game_transport_send_client_netsock(const MissionGameTransportBindings& b,U data,U length,U peer){return send(b,0x4c550e,{data,length,peer});}
U mission_game_transport_initialize_netsock(const MissionGameTransportBindings& b){
    call(b,0x4c5562);write(b,0x501320,1);
    const auto buffer=call(b,0x4983b0,{0x8000,0x4ce53c});write(b,0xd0a540,buffer);return 0;
}
void mission_game_transport_close_netsock(const MissionGameTransportBindings& b){
    call(b,0x4b8e10);call(b,0x4c555c);
    const auto buffer=read(b,0xd0a540);call(b,0x498400,{buffer});
    write(b,0xd0a540,0);write(b,0x501320,0);
}
} // namespace dfre
