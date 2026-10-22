#include "dfre/mission_logic.hpp"

#include <algorithm>
#include <cstring>
#include <limits>

namespace dfre {
namespace {

void put_word(AssetBytes& bytes, std::size_t at, std::uint16_t value) {
    bytes.at(at) = std::uint8_t(value & 0xFF);
    bytes.at(at + 1) = std::uint8_t(value >> 8);
}

void put_dword(AssetBytes& bytes, std::size_t at, std::uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) bytes.at(at + i) = std::uint8_t(value >> (8 * i));
}

} // namespace

namespace {
template<class F> const F& logic_need(const F& f,const char* message){if(!f)throw std::runtime_error(message);return f;}
std::int32_t logic_signed(std::uint32_t n){return n<=0x7fffffffu?std::int32_t(n):std::int32_t(std::int64_t(n)-0x100000000LL);}
std::uint32_t logic_sar(std::uint32_t n,unsigned shift){return (n>>shift)|((n&0x80000000u)?(~std::uint32_t(0)<<(32-shift)):0);}
class LogicFrame {
public:
    explicit LogicFrame(const MissionLogicFrameBindings& b):b_(b){}
    std::uint32_t read(std::uint32_t at){return logic_need(b_.cell,"Mission logic needs its current cells")(at);}
    void put(std::uint32_t at,std::uint32_t n){logic_need(b_.store,"Mission logic needs its cell writer")(at,n);}
    std::uint32_t call(std::uint32_t site,std::initializer_list<std::uint32_t> args={}){return logic_need(b_.call,"Mission logic needs its reached stage")(site,{args});}
    std::uint32_t client(std::size_t slot,std::uint32_t at,std::uint32_t width=4){return logic_need(b_.client,"Mission logic needs current client fields")(slot,at,width);}
    void client_put(std::size_t slot,std::uint32_t at,std::uint32_t n){logic_need(b_.client_store,"Mission logic needs its client writer")(slot,at,4,n);}
    std::uint32_t actor(std::uint32_t group,std::size_t slot,std::uint32_t at,std::uint32_t width=4){return logic_need(b_.actor,"Mission logic needs current actor fields")(group,slot,at,width);}
    void actor_clear(std::uint32_t group,std::size_t slot,std::uint32_t at,std::uint8_t mask){const auto value=std::uint8_t(actor(group,slot,at,1))&mask;logic_need(b_.actor_byte_store,"Mission logic needs its actor byte writer")(group,slot,at,std::uint8_t(value));}
    bool timer(std::uint32_t at,std::int32_t limit){const auto n=read(at)+1;put(at,n);return at==0x6e4734?logic_signed(n)>limit:n>std::uint32_t(limit);}
    void periodic(std::uint32_t at,std::int32_t limit,std::initializer_list<std::uint32_t> sites){if(timer(at,limit)){for(auto site:sites)call(site);put(at,0);}}
    void send(std::uint32_t id,const std::vector<std::uint8_t>& bytes){logic_need(b_.send,"Mission logic needs its reliable message producer")(id,bytes);}
    static void pack(std::vector<std::uint8_t>& p,std::size_t at,std::uint32_t n,unsigned width){for(unsigned i=0;i<width;++i)p.at(at+i)=std::uint8_t(n>>(8*i));}
    void common(std::vector<std::uint8_t>& p,std::uint32_t group,std::size_t slot,std::size_t at=2){for(unsigned i=0;i<3;++i)pack(p,at+i*4,actor(group,slot,0x24+i*4),4);}
    void replication(){
        if(read(0x67f520)){
            for(std::uint32_t i=0;logic_signed(i)<logic_signed(read(0x8cfa18));++i){
                if(!(actor(3,i,0x6c,1)&1)&&(actor(3,i,0x6d,1)&1)){
                    std::vector<std::uint8_t> p(18);pack(p,0,actor(3,i,0x30)>>16,2);pack(p,2,i,2);common(p,3,i,4);p[16]=std::uint8_t(actor(3,i,0x6c,1));p[17]=std::uint8_t(actor(3,i,0x7c,1));send(0x0e,p);actor_clear(3,i,0x6d,0xfe);
                }
            }
            for(std::uint32_t i=0;logic_signed(i)<logic_signed(read(0x8cfa1c));++i){
                if(!(actor(0,i,0x6d,1)&1))continue;
                const auto callback=actor(0,i,0x110);std::uint32_t id=0x29;std::size_t size=16;
                if(callback==0x429bc0){id=0x34;size=24;}else if(callback==0x450e40){id=0x35;size=28;}
                else if(callback==0x451fd0||callback==0x452e10||callback==0x451d40){id=0x36;size=28;}
                else if(callback==0x4528c0){id=0x37;size=24;}else if(callback==0x4519a0){id=0x38;size=20;}
                else if(callback==0x453f30){id=0x3b;size=18;}
                std::vector<std::uint8_t> p(size);
                if(id==0x34||id==0x37){const auto pad=logic_need(b_.packet_padding,"Mission logic needs explicit packet padding")(id);for(unsigned j=0;j<3;++j)p[21+j]=pad[j];}
                if(id==0x3b){common(p,0,i,0);pack(p,12,i,2);}else{pack(p,0,i,2);common(p,0,i);}
                p[14]=std::uint8_t(actor(0,i,0x30)>>24);p[15]=std::uint8_t(actor(0,i,0x6c,1));
                if(id==0x34||id==0x35||id==0x36){pack(p,16,actor(0,i,0x7c)>>8,2);pack(p,18,actor(0,i,0x88)>>8,2);}
                if(id==0x34||id==0x35)p[20]=std::uint8_t(actor(0,i,0x8c)>>24);
                if(id==0x35){p[21]=std::uint8_t(logic_sar(actor(0,i,0x98),22));p[22]=std::uint8_t(actor(0,i,0xa8,1));p[23]=std::uint8_t(actor(0,i,0xcc)>>24);p[24]=std::uint8_t(actor(0,i,0xd0)>>24);p[25]=std::uint8_t(logic_sar(actor(0,i,0xac),4));p[26]=std::uint8_t(logic_sar(actor(0,i,0xb0),4));p[27]=std::uint8_t(actor(0,i,0xd4,1));}
                if(id==0x36){pack(p,20,actor(0,i,0xec),4);pack(p,24,logic_sar(actor(0,i,0xf0),4),2);p[26]=std::uint8_t(actor(0,i,0x8c)>>24);p[27]=std::uint8_t(actor(0,i,0xe8,1));}
                if(id==0x37){pack(p,16,actor(0,i,0xe0,2),2);pack(p,18,actor(0,i,0xe4,2),2);p[20]=std::uint8_t(actor(0,i,0xf0,1));}
                if(id==0x38){p[16]=std::uint8_t(actor(0,i,0x8c)>>24);p[17]=std::uint8_t(actor(0,i,0xf0)>>24);pack(p,18,actor(0,i,0xec)>>8,2);}
                if(id==0x3b){p[16]=std::uint8_t(actor(0,i,0x8c)>>24);p[17]=std::uint8_t(actor(0,i,0xf0,1));}
                send(id,p);actor_clear(0,i,0x6d,0xfe);
            }
        }
        for(std::uint32_t i=0;logic_signed(i)<logic_signed(read(0x8cfa1c));++i)if(actor(0,i,0x6e,1)&0x20){
            std::vector<std::uint8_t> p(16);pack(p,0,i,2);common(p,0,i);p[14]=std::uint8_t(actor(0,i,0x30)>>24);p[15]=std::uint8_t(actor(0,i,0x6c,1));send(0x4f,p);actor_clear(0,i,0x6e,0xdf);
        }
    }
    void run(){
        if(read(0x67f520)&&!read(0x67f528))return;
        while(call(0x460e40))call(0x442ae0);
        call(0x4477f0);
        periodic(0x6e4734,15,{0x42eaf0});periodic(0x6e4738,62,{0x44a200});periodic(0x6e473c,310,{0x4438c0,0x448c30});periodic(0x6e4744,496,{0x445c80});periodic(0x6e4748,434,{0x45fd80});periodic(0x6e474c,62,{0x444630,0x448590});periodic(0x6e4750,1860,{0x446200,0x4482e0});periodic(0x6e4754,62,{0x447930});
        if(timer(0x6e4740,62)){
            for(std::size_t i=0;i<17;++i){
                if(client(i,0x18c,1)){
                    client_put(i,0x74,client(i,0x6c));client_put(i,0x6c,0);client_put(i,0x98,client(i,0x98)+1);
                    client_put(i,0x78,client(i,0x70));client_put(i,0x70,0);client_put(i,0x84,client(i,0x7c));client_put(i,0x7c,0);client_put(i,0x88,client(i,0x80));client_put(i,0x80,0);
                }
                const auto n=client(i,0x8c);client_put(i,0x8c,logic_signed(n)<1?0:n-1);
            }put(0x6e4740,0);
        }
        if(timer(0x6e475c,62)){
            for(std::size_t i=0;i<17;++i){
                const auto n=client(i,0x90);if(logic_signed(n)>0)client_put(i,0x90,n-1);else if(logic_signed(n)<0)client_put(i,0x90,0);
                const auto flags=logic_need(b_.client_actor_flags,"Mission logic needs current client actor flags")(i);
                if(!flags||!(*flags&4))client_put(i,0x198,0);else{const auto value=client(i,0x198)+1;client_put(i,0x198,value);if(logic_signed(value)>360)call(0x447fb0,{std::uint32_t(i)});}
            }put(0x6e475c,0);
        }
        if(read(0x67f520)&&read(0x67dbc0)){const auto n=read(0x6e4758)-1;put(0x6e4758,n);if(!n)put(0x67dbbc,!read(0x67f534)?3u:!read(0x6e558c)?4u:1u);}
        if(read(0x67f520)){const auto n=read(0x6e4730)+1;put(0x6e4730,n);if(n<read(0x67f538))return;put(0x6e4730,0);}
        const auto time=read(0x67dbe8);if(time!=0xffffffffu){std::vector<std::uint8_t> p(2);pack(p,0,std::uint32_t(logic_signed(time<<4)/1000),2);send(0x24,p);}
        call(0x444e30);
        for(std::size_t i=0;i<17;++i){if(!client(i,0x18c,1)){const auto n=client(i,0x94);if(logic_signed(n)<40000)client_put(i,0x94,n+1);}else if(client(i,0x1c4,2))call(0x441920,{std::uint32_t(i)});}
        replication();call(0x444f60);call(0x4421d0);
    }
private:const MissionLogicFrameBindings& b_;
};
}
void mission_logic_frame(const MissionLogicFrameBindings& bindings){LogicFrame(bindings).run();}

namespace {
class LogicResult {
public:
    explicit LogicResult(const MissionLogicResultBindings& b):b_(b){}
    std::uint32_t read(std::uint32_t at){return logic_need(b_.state.cell,"Mission result needs its current cells")(at);}
    void put(std::uint32_t at,std::uint32_t n){logic_need(b_.state.store,"Mission result needs its cell writer")(at,n);}
    std::uint32_t call(std::uint32_t site,std::initializer_list<std::uint32_t> args={}){return logic_need(b_.state.call,"Mission result needs its reached body")(site,{args});}
    std::uint32_t client(std::size_t slot,std::uint32_t at,std::uint32_t width=4){return logic_need(b_.state.client,"Mission result needs current client fields")(slot,at,width);}
    void client_put(std::size_t slot,std::uint32_t at,std::uint32_t width,std::uint32_t n){logic_need(b_.state.client_store,"Mission result needs its current client writer")(slot,at,width,n);}
    static void pack(MissionLogicResultPacket& p,std::size_t at,std::uint32_t n,unsigned width){for(unsigned i=0;i<width;++i)p.at(at+i)=std::uint8_t(n>>(i*8));}
    static std::uint32_t signed_word(std::uint32_t value){value&=0xffffu;return value&0x8000u?value|0xffff0000u:value;}
    void build(MissionLogicResultPacket& p){
        call(0x446da0);const auto include_filtered=read(0x67f52c);std::vector<std::uint32_t> slots;
        for(std::uint32_t i=0;i<17;++i)if(client(i,0x18c,1)&&(!client(i,0x18d,1)||include_filtered))slots.push_back(i);
        for(std::size_t i=0;i+1<slots.size();++i)for(std::size_t j=i+1;j<slots.size();++j)
            if(logic_signed(call(0x448130,{slots[i],slots[j]}))<0)std::swap(slots[i],slots[j]);
        for(std::size_t row=0;row<slots.size();++row){const auto slot=slots[row];
            const auto address=logic_need(b_.actor_source_address,"Mission result needs its actor wire address")(client(slot,8));
            p[3+row]=std::uint8_t((address-0x7cca80u)/0x114u);
            const auto mode=read(0x67f53c);std::uint32_t score=0;
            if(mode==5)score=client(slot,0x1bc);
            else if(mode==3||mode==4)score=client(slot,0x194);
            else score=client(slot,0x1a0)-client(slot,0x1b4)-(client(slot,0x1b0)<<2);
            pack(p,0x25+row*2,score,2);
            auto a=client(slot,0x1a8);if(logic_signed(a)>254)a=255;p[0x69+row]=std::uint8_t(a);
            auto c=client(slot,0x1ac);if(logic_signed(c)>254)c=255;p[0x8b+row]=std::uint8_t(c);
        }
        p[1]=std::uint8_t(slots.size());
        for(unsigned i=0;i<3;++i){const auto offset=i*0x68u;
            pack(p,0xad+i*2,read(0x6e80dc+offset)-read(0x6e80f0+offset)-(read(0x6e80ec+offset)<<2),2);
            pack(p,0xb3+i*2,read(0x6e80e4+offset),2);pack(p,0xb9+i*2,read(0x6e80e8+offset),2);pack(p,0xbf+i*4,read(0x6e80e0+offset),4);p[0xcb+i]=std::uint8_t(read(0x6e80f4+offset));
        }
    }
    void request(std::uint32_t mode,MissionLogicResultPacket& p){
        call(0x446100,{0});if(read(0x67dbc0))return;call(0x448d20,{mode});
        if(read(0x67f520)){p[2]=(!read(0x67f534)||read(0x6e558c))?0:1;put(0x6e4758,0xf00);}
        p[0]=std::uint8_t(mode);
        for(std::size_t i=0;i<17;++i)if(client(i,0x18c,1)){
            const auto state=client(i,0x1c4,2);if(state!=5&&(state==3||state==4))client_put(i,0x1c4,2,2);
            build(p);logic_need(b_.publish,"Mission result needs the current client outbox producer")(i,0x1e,p);
        }
        const auto game_type=read(0x67f53c);
        if(game_type==2||game_type==4||game_type==5){
            put(0x6e5774,read(0x6e5774)+1);
            if(mode==1)put(0x6e576c,read(0x6e576c)+1);else if(mode==2)put(0x6e5770,read(0x6e5770)+1);
        }
        put(0x67dbc0,1);
    }
    void receive(std::uint8_t* p,std::size_t length){
        if(!p||length!=0xce)throw std::runtime_error("Mission result needs its 206-byte packet");
        auto get=[&](std::size_t at,unsigned width){if(at+width>length)throw std::runtime_error("Mission result row exceeds its packet");std::uint32_t n=0;for(unsigned i=0;i<width;++i)n|=std::uint32_t(p[at+i])<<(i*8);return n;};
        if(!read(0x67f528))put(0x67dbc0,1);
        const auto& local=logic_need(b_.local_actor,"Mission result needs current local actor presence");
        const auto actor=local();if(!actor)return;
        put(0x67f718,get(0,1));
        const auto& word=logic_need(b_.actor_word,"Mission result needs current actor fields");
        put(0x67f714,logic_sar(word(*actor,0x6e,4),16));put(0x67f71c,get(1,1));
        for(std::uint32_t i=0;logic_signed(i)<logic_signed(read(0x67f71c));++i){
            const auto row_actor=logic_need(b_.actor_identity,"Mission result needs its current row actor")(3,get(3+i,1));
            const auto name=logic_need(b_.actor_name,"Mission result needs its current actor name")(row_actor);
            logic_need(b_.store_name,"Mission result needs its name buffer writer")(0x67f724+i*0x24,name);
            put(0x67f734+i*0x24,logic_sar(word(row_actor,0x6e,4),16));put(0x67f738+i*0x24,signed_word(get(0x25+i*2,2)));
            put(0x67f740+i*0x24,get(0x69+i,1));const auto byte_8b=get(0x8b+i,1);
            const auto current=local();put(0x67f73c+i*0x24,byte_8b);
            if(current&&*current==row_actor)put(0x67f720,i);
        }
        for(unsigned i=0;i<3;++i){
            put(0x67fbec+i*4,signed_word(get(0xad+i*2,2)));put(0x67fbf8+i*4,signed_word(get(0xb3+i*2,2)));put(0x67fc04+i*4,signed_word(get(0xb9+i*2,2)));put(0x67fc10+i*4,get(0xcb+i,1));
            const auto denominator=signed_word(get(0xb9+i*2,2));
            const auto value=logic_signed(get(0xbf+i*4,4));
            if(!denominator)put(0x67fc1c+i*4,0xffffffffu);
            else{const auto ratio=(std::int64_t(value)*65536)/logic_signed(denominator);if(ratio<std::numeric_limits<std::int32_t>::min()||ratio>std::numeric_limits<std::int32_t>::max())throw std::runtime_error("Mission result ratio divides outside signed range");put(0x67fc1c+i*4,std::uint32_t(ratio));}
        }
        if(read(0x67f718)==read(0x67f714))call(0x43e720,{499});
    }
private:const MissionLogicResultBindings& b_;
};
}
void mission_logic_build_result(const MissionLogicResultBindings& b,MissionLogicResultPacket& p){LogicResult(b).build(p);}
void mission_logic_request_result(const MissionLogicResultBindings& b,std::uint32_t mode,MissionLogicResultPacket& p){LogicResult(b).request(mode,p);}
void mission_logic_receive_result(const MissionLogicResultBindings& b,std::uint8_t* p,std::size_t length){LogicResult(b).receive(p,length);}

// Events stage host: mission-logic actions are consumed here, everything else
// forwards to the caller host (or the fail-closed base default).
class MissionLogic::EventAdapter : public EventHost {
public:
    explicit EventAdapter(MissionLogic& logic) : logic_(logic) {}

    // 0x42E810 case 8/9/10 -> 0x444CA0. Consumed; the caller never sees it.
    void action_444ca0(std::uint32_t mode) override { logic_.end_mission(mode); }
    // 0x42E810 case 6 -> 0x404DD0. Recorded and forwarded for display.
    void action_404dd0(std::uint32_t id) override { logic_.triggered_text(id); }

    std::uint32_t object_index_dword(std::uint32_t address, std::uint32_t index) override {
        return host() ? host()->object_index_dword(address, index)
                      : EventHost::object_index_dword(address, index);
    }
    std::uint32_t probe_460330(std::size_t trigger_index) override {
        return host() ? host()->probe_460330(trigger_index) : EventHost::probe_460330(trigger_index);
    }
    std::uint32_t probe_460560(std::uint32_t word3, std::uint32_t word4) override {
        return host() ? host()->probe_460560(word3, word4) : EventHost::probe_460560(word3, word4);
    }
    bool world_67f520_is_zero() override {
        return host() ? host()->world_67f520_is_zero() : EventHost::world_67f520_is_zero();
    }
    bool world_67f549_bit1() override {
        return host() ? host()->world_67f549_bit1() : EventHost::world_67f549_bit1();
    }
    void action_45f670(std::uint32_t word3, std::uint32_t word4) override {
        if (host()) host()->action_45f670(word3, word4); else EventHost::action_45f670(word3, word4);
    }
    void action_45f4c0(std::uint32_t word3) override {
        if (host()) host()->action_45f4c0(word3); else EventHost::action_45f4c0(word3);
    }
    void action_4601a0(std::size_t action_index) override {
        if (host()) host()->action_4601a0(action_index); else EventHost::action_4601a0(action_index);
    }
    void action_45f5b0(std::uint32_t word3) override {
        if (host()) host()->action_45f5b0(word3); else EventHost::action_45f5b0(word3);
    }
    void action_43f500(std::uint32_t word3) override {
        if (host()) host()->action_43f500(word3); else EventHost::action_43f500(word3);
    }
    void action_460640(std::uint32_t word3, std::uint32_t word4) override {
        if (host()) host()->action_460640(word3, word4); else EventHost::action_460640(word3, word4);
    }
    void action_460280(std::size_t action_index) override {
        if (host()) host()->action_460280(action_index); else EventHost::action_460280(action_index);
    }

private:
    MissionLogicHost* host() const noexcept { return logic_.host(); }
    MissionLogic& logic_;
};

// Message host: the single-player peer loop is local (0x460FB0 echoes a frame
// addressed to the local client into the receive buffer 0x8EA1C8; 0x461050
// hands it to the dispatcher on the next update). Actor and world queries
// forward to the caller host.
class MissionLogic::MessageAdapter : public GameMessageHost {
public:
    explicit MessageAdapter(MissionLogic& logic) : logic_(logic) {}

    void send_local_frame(const std::uint8_t* frame, std::size_t length) override {
        logic_.loopback_send(frame, length);
    }
    std::size_t receive_local_frame(const std::uint8_t** frame) override {
        return logic_.loopback_receive(frame);
    }

    std::int32_t lookup_4828c0(std::int32_t value) override {
        return host() ? host()->lookup_4828c0(value) : GameMessageHost::lookup_4828c0(value);
    }
    std::uint32_t actor_record_base() override {
        return host() ? host()->actor_record_base() : GameMessageHost::actor_record_base();
    }
    std::int32_t allocate_44a0c0(GameRecordArray array, std::int32_t index) override {
        return host() ? host()->allocate_44a0c0(array, index)
                      : GameMessageHost::allocate_44a0c0(array, index);
    }
    std::int32_t actor_record_dword(std::int32_t index, std::uint32_t offset) override {
        return host() ? host()->actor_record_dword(index, offset)
                      : GameMessageHost::actor_record_dword(index, offset);
    }
    std::uint16_t actor_record_word(std::int32_t index, std::uint32_t offset) override {
        return host() ? host()->actor_record_word(index, offset)
                      : GameMessageHost::actor_record_word(index, offset);
    }
    std::int32_t lookup_42cce0(std::int32_t value) override {
        return host() ? host()->lookup_42cce0(value) : GameMessageHost::lookup_42cce0(value);
    }
    std::int32_t terrain_height_47df40(std::uint32_t x, std::uint32_t y) override {
        return host() ? host()->terrain_height_47df40(x, y)
                      : GameMessageHost::terrain_height_47df40(x, y);
    }
    void sincos_477820(std::int32_t angle, std::int32_t& sin_out, std::int32_t& cos_out) override {
        if (host()) host()->sincos_477820(angle, sin_out, cos_out);
        else GameMessageHost::sincos_477820(angle, sin_out, cos_out);
    }
    std::int32_t weapon_list_46c180() override {
        return host() ? host()->weapon_list_46c180() : GameMessageHost::weapon_list_46c180();
    }
    std::uint32_t session_counter_c2382c() override {
        return host() ? host()->session_counter_c2382c() : GameMessageHost::session_counter_c2382c();
    }
    std::uint32_t current_weapon_item_c239b8_c239c0() override {
        return host() ? host()->current_weapon_item_c239b8_c239c0()
                      : GameMessageHost::current_weapon_item_c239b8_c239c0();
    }
    std::uint32_t weapon_aim_c23b70() override {
        return host() ? host()->weapon_aim_c23b70() : GameMessageHost::weapon_aim_c23b70();
    }
    std::uint32_t weapon_aim_c23b74() override {
        return host() ? host()->weapon_aim_c23b74() : GameMessageHost::weapon_aim_c23b74();
    }
    std::uint32_t weapon_table_field(std::uint32_t index, std::uint32_t offset,
                                     std::uint32_t width) override {
        return host() ? host()->weapon_table_field(index, offset, width)
                      : GameMessageHost::weapon_table_field(index, offset, width);
    }
    std::uint32_t class_table_field(std::uint32_t index, std::uint32_t offset,
                                    std::uint32_t width) override {
        return host() ? host()->class_table_field(index, offset, width)
                      : GameMessageHost::class_table_field(index, offset, width);
    }
    std::uint32_t slot_map_word(std::uint32_t slot) override {
        return host() ? host()->slot_map_word(slot) : GameMessageHost::slot_map_word(slot);
    }
    std::int32_t record_field_110(GameRecordArray array, std::int32_t index) override {
        return host() ? host()->record_field_110(array, index)
                      : GameMessageHost::record_field_110(array, index);
    }
    void weapons_tick_482b00() override {
        if (host()) host()->weapons_tick_482b00(); else GameMessageHost::weapons_tick_482b00();
    }

private:
    MissionLogicHost* host() const noexcept { return logic_.host(); }
    MissionLogic& logic_;
};

// Host defaults that keep absence explicit.
std::size_t MissionLogicHost::clients(MissionLogicClient*, std::size_t) { throw std::runtime_error("Mission inspection needs client records"); }
void MissionLogicHost::set_client_state(std::size_t, std::uint16_t) { throw std::runtime_error("Mission inspection needs its client state writer"); }
std::size_t MissionLogicHost::squad_rows(MissionLogicSquadRow*, std::size_t) { throw std::runtime_error("Mission inspection needs squad rows"); }
bool MissionLogicHost::local_player_present() { throw std::runtime_error("Mission inspection needs local actor presence"); }
std::uint32_t MissionLogicHost::local_player_actor_6e() { throw std::runtime_error("Mission inspection needs local actor metadata"); }
std::int32_t MissionLogicHost::local_actor_index() { throw std::runtime_error("Mission inspection needs its local actor slot"); }
std::string MissionLogicHost::client_name(std::uint32_t) { throw std::runtime_error("Mission inspection needs actor names"); }
std::uint32_t MissionLogicHost::client_actor_6e(std::uint32_t) { throw std::runtime_error("Mission inspection needs client actor metadata"); }
void MissionLogicHost::play_sound_43e720(std::uint32_t) { throw std::runtime_error("Mission inspection needs its sound provider"); }

MissionLogic::MissionLogic()
    : event_adapter_(std::make_unique<EventAdapter>(*this)),
      message_adapter_(std::make_unique<MessageAdapter>(*this)) {
    events_.set_host(event_adapter_.get());
    messages_.set_host(message_adapter_.get());
    messages_.set_inspection_mode(true);
}

MissionLogic::~MissionLogic() = default;

void MissionLogic::set_host(MissionLogicHost* host) noexcept { host_ = host; }

void MissionLogic::set_result_context(std::uint32_t campaign, std::uint32_t mission) noexcept {
    campaign_ = campaign;
    mission_index_ = mission;
}

void MissionLogic::begin_mission(const MissionData& mission, bool single_player) {
    single_player_ = single_player;
    flag_67f52c_ = single_player;
    ended_ = false;
    end_mode_ = MissionResultMode::neutral;
    block_ = {};
    outputs_ = {};
    loopback_.clear();
    messages_.reset();
    // 0x470261 sets the sequence counter to 1 at game init; the dedup ring at
    // 0xBCFC3C starts zeroed, so sequence 0 must never be used.
    messages_.queue().set_sequence_counter(1);
    messages_.set_single_player(single_player);
    messages_.set_networked(!single_player);
    events_.load(mission);
}

void MissionLogic::begin_tables(std::vector<MissionEvent> events, std::vector<EventTableRecord> triggers,
                                std::vector<EventTableRecord> actions, bool single_player) {
    single_player_ = single_player;
    flag_67f52c_ = single_player;
    ended_ = false;
    end_mode_ = MissionResultMode::neutral;
    block_ = {};
    outputs_ = {};
    loopback_.clear();
    messages_.reset();
    messages_.queue().set_sequence_counter(1); // 0x470261 game init value
    messages_.set_single_player(single_player);
    messages_.set_networked(!single_player);
    events_.reset();
    events_.load_tables(std::move(events), std::move(triggers), std::move(actions));
}

MissionLogicOutput MissionLogic::inspection_step(const GameMessageUpdateInput& input) {
    outputs_ = {};
    // 0x46F5C0: receive drain + dispatch, weapons tick, queue flush.
    consume_actions(messages_.update(input));
    // Inspection event counter. The live frame owns its canonical counter.
    events_.frame_driver_step();
    // Flush this inspection bus for its next receive step.
    messages_.flush_queue();
    return outputs_;
}

MissionLogicLeave MissionLogic::leave() {
    // 0x401E70 calls 0x42EBA0 before the result check. Ends requested here are
    // queued but cannot reach 0x471010 before the profile decision (the bus
    // only delivers on the next update).
    events_.run_bit2_events();
    MissionLogicLeave out;
    out.single_player = single_player_;
    out.mission_ended = ended_;
    out.profile_win = single_player_ && block_.flag_67f718 == 1;
    out.objective_mask = events_.flags_67dbec();
    return out;
}

MissionLogicLeave MissionLogic::leave_and_apply(AssetBytes& profile) {
    auto out = leave();
    if (out.profile_win) {
        out.profile_update = apply_mission_result(
            profile, {campaign_, mission_index_, true, out.objective_mask});
        out.profile_applied = true;
    }
    return out;
}

void MissionLogic::dispatch_action(std::size_t action_index) {
    // Single path: the stage dispatches every kind and routes 8/9/10 through
    // the host hook into end_mission.
    events_.dispatch_action(action_index);
}

std::vector<std::uint8_t> MissionLogic::build_result_payload(MissionResultMode mode) const {
    if(!host_)throw std::runtime_error("Mission inspection needs result rows");
    MissionLogicClient clients[mission_result_max_clients]{};
    const std::size_t count = host_ ? std::min(host_->clients(clients, mission_result_max_clients),
                                               mission_result_max_clients)
                                    : 0;
    return build_result_payload(mode, clients, count);
}

std::vector<std::uint8_t> MissionLogic::build_result_payload(MissionResultMode mode,
                                                             const MissionLogicClient* clients,
                                                             std::size_t count) const {
    // 0x4403C0: payload[0] mode, payload[1] accepted client count, rows at
    // +3/+0x25/+0x69/+0x8B, squad rows at +0xAD..+0xCB.
    std::vector<std::uint8_t> payload(mission_result_payload_length, 0);
    std::size_t accepted = 0;
    for (std::size_t i = 0; i < count; ++i) {
        const auto& client = clients[i];
        if (!client.active) continue;
        if (client.byte_18d != 0 && !flag_67f52c_) continue; // 0x4403C0 filter
        ++accepted;
    }
    payload[0] = static_cast<std::uint8_t>(mode);
    payload[1] = static_cast<std::uint8_t>(accepted);
    std::size_t row = 0;
    for (std::size_t i = 0; i < count && row < mission_result_max_rows; ++i) {
        const auto& client = clients[i];
        if (!client.active) continue;
        if (client.byte_18d != 0 && !flag_67f52c_) continue;
        payload[3 + row] = static_cast<std::uint8_t>(client.actor_index);
        put_word(payload, 0x25 + 2 * row, client.score);
        payload[0x69 + row] = client.byte_69;
        payload[0x8B + row] = client.byte_8b;
        ++row;
    }
    MissionLogicSquadRow squad[mission_result_stat_rows]{};
    const std::size_t squad_count =
        host_ ? std::min(host_->squad_rows(squad, mission_result_stat_rows), mission_result_stat_rows)
              : 0;
    for (std::size_t i = 0; i < squad_count; ++i) {
        put_word(payload, 0xAD + 2 * i, squad[i].a);
        put_word(payload, 0xB3 + 2 * i, squad[i].b);
        put_word(payload, 0xB9 + 2 * i, squad[i].c);
        put_dword(payload, 0xBF + 4 * i, squad[i].d);
        payload[0xCB + i] = squad[i].e;
    }
    return payload;
}

void MissionLogic::end_mission(std::uint32_t mode) {
    if(!host_)throw std::runtime_error("Mission inspection needs its result host");
    if (ended_) return; // 0x444CA0 guard: one result per mission (0x67DBC0)
    MissionLogicClient clients[mission_result_max_clients]{};
    const std::size_t total = host_ ? std::min(host_->clients(clients, mission_result_max_clients),
                                               mission_result_max_clients)
                                    : 0;
    // 0x444CA0: rewrite client states 3 or 4 to 2, then one 0x1E message per
    // active client using the payload built from all accepted clients.
    for (std::size_t i = 0; i < total; ++i) {
        if (!clients[i].active) continue;
        const std::uint16_t state = clients[i].state;
        if (state != 5 && (state == 3 || state == 4)) {
            if (host_) host_->set_client_state(i, 2);
            outputs_.client_states_reset.push_back(i);
        }
    }
    const auto payload = build_result_payload(static_cast<MissionResultMode>(mode), clients, total);
    if (total > 0) {
        for (std::size_t i = 0; i < total; ++i) {
            if (!clients[i].active) continue;
            if (!messages_.queue().enqueue(messages_.table(), mission_result_message_id,
                                           payload.data(),
                                           static_cast<std::uint16_t>(payload.size()))) {
                break; // queue full: the original drops; keep the one-shot guard
            }
        }
        outputs_.messages_sent.push_back(mission_result_message_id);
    }
    outputs_.end_requests.push_back(static_cast<MissionResultMode>(mode));
    end_mode_ = static_cast<MissionResultMode>(mode);
    ended_ = true; // 0x67DBC0 = 1
}

void MissionLogic::triggered_text(std::uint32_t id) {
    outputs_.triggered_texts.push_back(id);
    logic_need(host_,"Mission inspection needs its text provider")->action_404dd0(id);
}

void MissionLogic::inspect_result_message(const std::uint8_t* payload,std::size_t length){apply_result_message(payload,length);}

void MissionLogic::apply_result_message(const std::uint8_t* payload, std::size_t length) {
    if(!payload||length!=mission_result_payload_length)throw std::runtime_error("Mission inspection needs its complete 206-byte result packet");
    // 0x471010 in single player/host: requires the local player actor.
    if(!host_)throw std::runtime_error("Mission inspection needs its local actor provider");
    if(!host_->local_player_present())return;
    block_.flag_67f718 = payload[0];
    block_.local_player_67f714 = host_->local_player_actor_6e() >> 16;
    block_.count_67f71c = payload[1];
    block_.count_clamped = payload[1] > mission_result_max_clients;
    const std::size_t count = std::min<std::size_t>(payload[1], mission_result_max_clients);
    block_.rows.assign(count, MissionLogicResultRow{});
    block_.stats.fill(MissionLogicStatRow{});
    block_.local_found = false;
    for (std::size_t i = 0; i < count; ++i) {
        auto& row = block_.rows[i];
        const std::uint8_t actor_index = length > 3 + i ? payload[3 + i] : 0;
        row.name = host_->client_name(actor_index);
        row.client_high = host_->client_actor_6e(actor_index) >> 16;
        row.score = length > 0x25 + 2 * i ? std::uint16_t(payload[0x25 + 2 * i] |
                                                          (payload[0x26 + 2 * i] << 8))
                                          : 0;
        row.byte_69 = length > 0x69 + i ? payload[0x69 + i] : 0;
        row.byte_8b = length > 0x8B + i ? payload[0x8B + i] : 0;
    }
    for (std::size_t i = 0; i < mission_result_stat_rows; ++i) {
        auto& stat = block_.stats[i];
        stat.a = length > 0xAD + 2 * i ? std::uint16_t(payload[0xAD + 2 * i] |
                                                       (payload[0xAE + 2 * i] << 8))
                                       : 0;
        stat.b = length > 0xB3 + 2 * i ? std::uint16_t(payload[0xB3 + 2 * i] |
                                                       (payload[0xB4 + 2 * i] << 8))
                                       : 0;
        stat.c = length > 0xB9 + 2 * i ? std::uint16_t(payload[0xB9 + 2 * i] |
                                                       (payload[0xBA + 2 * i] << 8))
                                       : 0;
        stat.byte_cb = length > 0xCB + i ? payload[0xCB + i] : 0;
        if (length > 0xBF + 4 * i + 3) {
            std::uint32_t value = 0;
            std::memcpy(&value, payload + 0xBF + 4 * i, 4);
            if (stat.c == 0) {
                stat.ratio = -1;
            } else {
                const std::int64_t numerator = std::int64_t(static_cast<std::int32_t>(value)) * 65536;
                stat.ratio = static_cast<std::int32_t>(numerator / static_cast<std::int32_t>(stat.c));
            }
        } else {
            stat.ratio = -1;
        }
    }
    // 0x471010: 0x67F720 gets the row of the local player when present, and the
    // 0x43E720(499) sound plays when the flag equals the local player word.
    const std::int32_t local_index = host_ ? host_->local_actor_index() : -1;
    if (local_index >= 0) {
        // 0x471010 keeps scanning: the last matching row wins.
        for (std::size_t i = 0; i < count; ++i) {
            const std::uint8_t actor_index = length > 3 + i ? payload[3 + i] : 0;
            if (static_cast<std::int32_t>(actor_index) == local_index) {
                block_.local_index_67f720 = static_cast<std::uint32_t>(i);
                block_.local_found = true;
            }
        }
    }
    if (block_.flag_67f718 == block_.local_player_67f714) {
        host_->play_sound_43e720(499);
    }
}

void MissionLogic::loopback_send(const std::uint8_t* frame, std::size_t length) {
    if (frame == nullptr || length == 0) return;
    messages_.submit_local_echo(frame,length);
}

std::size_t MissionLogic::loopback_receive(const std::uint8_t** frame) {
    // 0x461050: the receive buffer is handed to the dispatcher and cleared;
    // keep the window alive until the next receive call.
    if (loopback_.empty()) return 0;
    receive_window_ = std::move(loopback_);
    loopback_.clear();
    *frame = receive_window_.data();
    return receive_window_.size();
}

void MissionLogic::consume_actions(const std::vector<GameMessageAction>& actions) {
    for (const auto& action : actions) {
        outputs_.message_actions.push_back(action);
        if (action.kind == GameMessageActionKind::world_1e && !action.duplicate) {
            apply_result_message(action.bytes.data(), action.bytes.size());
            outputs_.result_dispatched = true;
        }
    }
}

std::uint32_t MissionLogic::game_status_67dbbc() noexcept {
    return messages_.dispatch().flag_67dbbc();
}

} // namespace dfre
