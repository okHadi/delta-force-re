#include "dfre/mission_peer_parser.hpp"
#include <algorithm>
#include <stdexcept>
namespace dfre {
namespace {
using U=std::uint32_t;
std::int32_t si(U v){return v<=0x7fffffffu?std::int32_t(v):std::int32_t(std::int64_t(v)-0x100000000LL);}
std::int32_t sh(U v){return v&0x8000u?std::int32_t(v&0xffffu)-65536:std::int32_t(v&0xffffu);}
U get(const std::uint8_t* p,U at,U width=4){U v=0;for(U i=0;i<width;++i)v|=U(p[at+i])<<(8*i);return v;}
void put(std::uint8_t* p,U at,U value,U width=4){for(U i=0;i<width;++i)p[at+i]=std::uint8_t(value>>(8*i));}
template<class F>const F& need(const F& f,const char* why){if(!f)throw std::runtime_error(why);return f;}
class Parser {
public:
    Parser(MissionClientOutbox& outbox,GameMessageTable& table,const MissionPeerParserBindings& b):outbox_(outbox),table_(table),b_(b){}
    U read(U a,U w=4)const{return need(b_.read,"Peer parser memory reader is not bound")(a,w);}
    void write(U a,U v,U w=4)const{need(b_.write,"Peer parser memory writer is not bound")(a,w,v);}
    std::uint8_t* span(U a,std::size_t n)const{auto* p=need(b_.resolve,"Peer parser source resolver is not bound")(a,n);if(!p)throw std::runtime_error("Peer parser source span is unavailable");return p;}
    U packet(U a,U w=4)const{return get(span(a,w),0,w);}
    const GameMessageDescriptor& descriptor(U id)const{if(id>=table_.count())throw std::runtime_error("Peer parser id exceeds its source descriptor table");return table_.descriptor(std::uint8_t(id));}
    U call(U address,std::initializer_list<U> args={},int pointer_arg=-1,std::uint8_t* payload=nullptr,std::size_t length=0)const{
        MissionPeerParserCall c;c.address=address;c.count=std::uint8_t(args.size());std::copy(args.begin(),args.end(),c.args.begin());c.payload_argument=std::int8_t(pointer_arg);c.payload=payload;c.length=length;
        return need(b_.call,"Peer parser handler body is not bound")(c);
    }
    U cr(U at,U width=4)const{return get(client_,at,width);}
    void cw(U at,U value,U width=4)const{put(client_,at,value,width);}
    void diagnostic(U format,U value){std::array<std::uint8_t,128> text{};call(0x493a80,{0,format,value},0,text.data(),text.size());call(0x4564b0,{0},0,text.data(),text.size());}
    void packet_call(U site,U p,std::initializer_list<U> prefix={}){
        MissionPeerParserCall c;c.address=site;c.count=std::uint8_t(prefix.size()+1);std::copy(prefix.begin(),prefix.end(),c.args.begin());c.args[prefix.size()]=p;c.payload_argument=std::int8_t(prefix.size());
        const auto& d=descriptor(id_);const auto length=(d.flags&1)?sh(packet(p,2)):sh(d.length);
        if(length<0)throw std::runtime_error("Peer parser handler payload length is negative");
        c.length=std::size_t(length);c.payload=span(p,c.length);need(b_.call,"Peer parser handler body is not bound")(c);
    }
    bool live_actor()const{if(!cr(8))return false;return !(read(cr(8)+0x6c,1)&4);}
    void actor_snapshot(U p){
        const auto slot=sh(packet(p,2));std::array<std::uint8_t,45> bytes{};
        if(slot<0||slot>=0x501)return;
        const U actor=0x7cca80+U(slot)*0x114;
        const auto record=read(actor+4);
        if(record){
            put(bytes.data(),2,read(record+0x20),2);put(bytes.data(),4,read(read(actor+4)+0x28,1),1);
            if(read(read(actor+4)+0x24,1)&4){
                U name=read(actor+0xc)+4;U i=0;
                for(;;){
                    if(i>=16)throw std::runtime_error("Peer parser actor name exceeds its source field");
                    const auto word=read(name);bytes[12+i]=std::uint8_t(word);++i;
                    if(!(word&255))break;
                    if(i>=16)throw std::runtime_error("Peer parser actor name exceeds its source field");
                    const auto byte=read(name+1,1);bytes[12+i]=std::uint8_t(byte);++i;if(!byte)break;name+=2;
                }
            }
        }
        put(bytes.data(),0,packet(p,2),2);put(bytes.data(),5,read(actor+0x70,1),1);put(bytes.data(),6,read(actor+0x6c,2),2);put(bytes.data(),8,read(actor+8));
        put(bytes.data(),28,read(actor+0x24));put(bytes.data(),32,read(actor+0x28));put(bytes.data(),36,read(actor+0x2c));
        put(bytes.data(),40,read(actor+0x30)>>16,2);put(bytes.data(),42,read(actor+0x34)>>16,2);put(bytes.data(),44,read(actor+0x98,1),1);
        const auto peer=cr(0);call(0x4423a0,{peer,0x18,0,45},2,bytes.data(),bytes.size());
    }
    void projectile(U p){
        if(!cr(8)){diagnostic(0x4cc3ac,read(0x6e5588));return;}
        U height=(packet(p+4)&255)*256+(packet(p)&255)*65536;if(height&0x800000)height|=0xff000000;
        std::array<std::uint8_t,20> pose{};const auto x=packet(p)&0xffffff00u,y=packet(p+4)&0xffffff00u;put(pose.data(),0,x);put(pose.data(),4,y);
        const auto ground=call(0x47df40,{x,y});put(pose.data(),8,ground+height);put(pose.data(),12,packet(p+10,2)<<16);put(pose.data(),16,packet(p+12,2)<<16);
        U kind=packet(p+14,1);if(kind==5||kind==6)kind=4;else if(kind==8||kind==9)kind=7;
        const U ammo=0xd4+kind*4;
        if(si(cr(ammo))>0&&si(cr(0x198))<5){
            if(kind!=cr(0x15c)){cw(0x15c,kind);cw(0x154,read(0x67f538)+1);}
            const auto tick=read(0x67f538);if(si(tick)<si(cr(0x154)))cw(0x154,tick);
            if(si(cr(0x154))>=0){
                packet_call(0x445b80,p);const auto owner=cr(8),raw_kind=packet(p+14,1);call(0x462f20,{0,owner,raw_kind},0,pose.data(),pose.size());
                const auto rate=read(0xad3dd0+kind*4);cw(0x154,cr(0x154)-rate);
            }
            cw(ammo,cr(ammo)-1);
        }
    }
    void movement(U p,U captured_tick){
        const auto owner=cr(8);cw(0x154,cr(0x154)+read(0x67f538)+1);cw(0x158,cr(0x158)+captured_tick+1);
        if(si(cr(0x170))>=si(packet(p+12))||!owner)return;
        cw(0x170,packet(p+12));if(read(owner+0x6c,1)&4)return;
        if(read(0x67dbc0)||cr(0x1c4,2)!=5)return;
        U height=(packet(p+4)&255)*256+(packet(p)&255)*65536;if(height&0x800000)height|=0xff000000;
        write(owner+0x24,packet(p)&0xffffff00u);const auto y=packet(p+4)&0xffffff00u;write(owner+0x28,y);
        const auto x=read(owner+0x24);const auto ground=call(0x47df40,{x,y});write(owner+0x2c,ground+height);
        write(owner+0x30,packet(p+8,1)<<24);write(owner+0x34,packet(p+9,1)<<24);write(owner+0x7c,packet(p+10,1));write(owner+0x94,packet(p+11,1));
        cw(0x17c,cr(0x178));cw(0x178,cr(0x174));cw(0x174,read(owner+0x24));cw(0x188,cr(0x184));cw(0x184,cr(0x180));cw(0x180,read(owner+0x28));call(0x448c00,{slot_});
    }
    U dispatch(U p,U end){
        U next=p;const auto tick=read(0x67f538);
        switch(id_){
        case 2:if(cr(0x1c4,2)!=1)cw(0x1c4,3,2);break;
        case 4:if(cr(0x1c4,2)!=1){cw(0x1c4,4,2);cw(0x1ca,0,1);cw(0x1cb,0,1);}break;
        case 9:projectile(p);break;
        case 10:movement(p,tick);break;
        case 11:packet_call(0x446a30,p,{slot_});break;
        case 0x12:{const auto length=sh(packet(p,2));next=end+1;if(length>=0&&p+U(length)<=end){packet_call(0x4488e0,p,{slot_});next=p+U(length);}return next;}
        case 0x13:
            if(!cr(8))diagnostic(0x4cc3d4,read(0x8e61bc));
            else if(!(read(cr(8)+0x6c,1)&4)){if(!read(0x67f520))call(0x441820,{client_wire_});}
            else if(!cr(0x8c))call(0x441820,{client_wire_});break;
        case 0x14:for(U i=0;i<64;++i){const U at=0x204+i*0x264;if(cr(at+9,1)&&cr(at,2)==packet(p,2))std::fill(client_+at,client_+at+0x264,0);}break;
        case 0x17:actor_snapshot(p);break;
        case 0x21:if(live_actor()){const auto owner=cr(8);const auto x=packet(p),y=packet(p+4),z=packet(p+8);call(0x4657d0,{2,x,y,z,owner});}break;
        case 0x22:if(cr(0x1c4,2)==1)packet_call(0x445f80,p,{slot_});break;
        case 0x28:if(live_actor())call(0x447150,{cr(8)});break;
        case 0x2a:if(live_actor()&&si(read(0x67f538))<si(cr(0x158))){cw(0x158,read(0x67f538)-70);packet_call(0x4473f0,p,{client_wire_});}break;
        case 0x2c:if(live_actor())call(0x447760,{client_wire_});break;
        case 0x2f:{const auto v=packet(p);write(0x6e5878,(v&0xffffu)<<16);write(0x6e587c,v&0xffff0000u);write(0x6e5880,64);break;}
        case 0x32:cw(0x1c4,2,2);break;
        case 0x40:{const auto length=sh(packet(p,2));const auto channel=packet(p+2,1);const auto n=length>=3?std::size_t(length-3):0;call(0x447dd0,{client_wire_,channel,p+3},2,span(p+3,n),n);return p+U(length);}
        case 0x43:packet_call(0x447de0,p,{client_wire_});break;
        case 0x45:packet_call(0x448040,p,{client_wire_});break;
        case 0x46:packet_call(0x4482c0,p,{client_wire_});break;
        case 0x4c:{
            const auto item=packet(p,1);const U model=U(si(packet(p+3))>>16);
            if(U(si(packet(p+5))>>16)!=cr(0x16c)||model!=read(read(0xad3ed0)+item*0xa8+0xa4)||packet(p+4,1)!=read(read(0xad3ed0)+item*0xa8+0xa0)||U(si(packet(p))>>16)!=read(read(0xad3ed0)+item*0xa8+0x9c)||packet(p+1,1)!=read(read(0xad3ed0)+item*0xa8+0x74))call(0x447fb0,{slot_});break;}
        case 0x4e:{const auto value=U(sh(packet(p,2)));const auto kind=packet(p+2,1);if(value!=read(read(0xad3ee4)+kind*0x34+8))call(0x447fb0,{slot_});break;}
        case 0x50:{const auto value=call(0x46c180);if(packet(p+4)!=17||value!=packet(p))call(0x447fb0,{slot_});break;}
        default:if(descriptor(id_).flags&1){next=end+1;const auto n=sh(packet(p,2));if(n>=0)next=p+U(n);}break;
        }
        if(!(descriptor(id_).flags&1))next+=U(sh(descriptor(id_).length));
        return next;
    }
    void run(){
        U cursor=read(0x8e61b4);const auto count=read(0x8e61b8);const U end=cursor+count;if(!cursor)return;
        auto view=need(b_.client,"Peer parser client spans are not bound")(0);check_client(view);U peer=read(0x8e61bc);slot_=0;
        if(get(view.bytes,0)!=peer){peer=read(0x8e61bc);for(slot_=1;slot_<17;++slot_){view=b_.client(slot_);check_client(view);if(get(view.bytes,0)==peer)break;}}
        if(slot_==17){diagnostic(0x4cc36c,read(0x8e61bc));return;}
        client_=view.bytes;client_wire_=0x6e9a90+slot_*0x9b84;
        const auto received=read(0x8e61b8);const auto bytes=cr(0x70),packets=cr(0x80);cw(0x98,0);cw(0x70,bytes+received);cw(0x80,packets+1);
        while(cursor<end){
            id_=packet(cursor,1);descriptor(id_);U payload=cursor+1;
            if(descriptor(id_).flags&6){
                const auto sequence=packet(cursor+1,2);std::array<std::uint8_t,2> ack{};put(ack.data(),0,sequence,2);outbox_.enqueue(slot_,0x14,ack.data(),2);payload=cursor+3;
                bool duplicate=false;for(U i=0;i<64;++i)if(cr(0x9b04+i*2,2)==sequence){duplicate=true;break;}
                if(duplicate){const auto next=payload+U((descriptor(id_).flags&1)?sh(packet(payload,2)):sh(descriptor(id_).length));if(next<=cursor)throw std::runtime_error("Peer parser source cursor does not advance");cursor=next;continue;}
                for(U i=63;i>0;--i)cw(0x9b04+i*2,cr(0x9b04+(i-1)*2,2),2);cw(0x9b04,sequence,2);
            }
            const auto next=dispatch(payload,end);
            if(next<=cursor)throw std::runtime_error("Peer parser source cursor does not advance");
            cursor=next;
        }
    }
    static void check_client(const MissionClientRecordView& v){if(!v.bytes||v.length<mission_client_record_size)throw std::runtime_error("Peer parser client record is incomplete");}
private:
    MissionClientOutbox& outbox_;GameMessageTable& table_;const MissionPeerParserBindings& b_;std::uint8_t* client_=nullptr;U slot_=0,client_wire_=0,id_=0;
};
}
void mission_peer_parser_parse(MissionClientOutbox& outbox,GameMessageTable& table,const MissionPeerParserBindings& bindings){Parser(outbox,table,bindings).run();}
}
