#include "dfre/mission_client_outbox.hpp"
#include <iostream>
#include <stdexcept>
#define CHECK(x) do{if(!(x))throw std::runtime_error("client outbox: " #x);}while(false)
using namespace dfre;
namespace {
std::uint32_t get(const std::uint8_t* b,unsigned at,unsigned width=4){std::uint32_t value=0;for(unsigned i=0;i<width;++i)value|=std::uint32_t(b[at+i])<<(i*8);return value;}
void put(std::uint8_t* b,unsigned at,std::uint32_t value,unsigned width=4){for(unsigned i=0;i<width;++i)b[at+i]=std::uint8_t(value>>(i*8));}
}
int main(){try{
    GameMessageStage messages;MissionServerMessageBuffer buffer;
    std::vector<std::array<std::uint8_t,mission_client_record_size>> clients(17);
    auto& c=clients[0];c[0x18c]=1;put(c.data(),0,7);put(c.data(),0x1c4,5,2);put(c.data(),0x1b8,0xffffffff);
    std::uint32_t delay=0x8000,peer=7;MissionClientOutboxBindings b;
    b.client=[&](auto slot){return MissionClientRecordView{clients.at(slot).data(),clients.at(slot).size()};};
    b.cell=[&](auto address){return address==0x50321c?delay:peer;};
    MissionClientOutbox outbox(messages,buffer,b);std::array<std::uint8_t,16> packet{};
    c[0x204+10+30]=0xa5;CHECK(outbox.enqueue(0,0x0a,packet.data(),packet.size()));
    CHECK(get(c.data(),0x1b8)==0&&get(c.data(),0x204,2)==0xffff&&c[0x204+40]==0xa5);
    outbox.tick();CHECK(messages.pending_local_echo().size()==17&&c[0x18f]==1&&get(c.data(),0x6c)==17&&get(c.data(),0x7c)==1);
    CHECK(c[0x204+9]==0); // signed delay8000 sends immediately, then removes repeat1
    std::uint32_t received=0,idle=1;std::array<std::uint16_t,256> ring{};GameMessageLiveBindings live;
    live.read_global=[](auto address){return address==0x67f528?1u:0u;};live.dedup=&ring;live.received_bytes_bcfc24=&received;live.update_counter_bc5d20=&idle;
    messages.set_live_bindings(live);messages.pump_receive();CHECK(received==17&&idle==0&&messages.pending_local_echo().empty());
    CHECK(messages.queue().active_count()==0);
    // Aliased enqueue payload uses the original four-byte load/store order.
    c[0x204+10]=0xaa;c[0x204+11]=0xbb;
    CHECK(outbox.enqueue(0,0x0a,c.data()+0x204+8,4));
    CHECK(c[0x204+10]==0x0a&&c[0x204+11]==1&&c[0x204+12]==0xaa&&c[0x204+13]==0xbb);
    for(unsigned at=0x204;at<0x204+64*0x264;++at)c[at]=0;
    for(unsigned i=0;i<64;++i)c[0x204+i*0x264+9]=1;
    CHECK(!outbox.enqueue(0,0x0a,nullptr,999));
    for(unsigned at=0x204;at<0x204+64*0x264;++at)c[at]=0;
    // A full echo drops the packet but returns its length and resets the cursor.
    std::vector<std::uint8_t> echo(8191,0x55);messages.submit_local_echo(echo.data(),echo.size());buffer.count_8e81c0=1;buffer.bytes_8e61c0[0]=0x0a;
    CHECK(outbox.flush(7)==1&&buffer.count_8e81c0==0&&messages.pending_local_echo().size()==8191);
    // Missing remote transport preserves the completed frame and prior queue writes.
    messages.submit_local_echo(packet.data(),0);put(c.data(),0,8);delay=0;
    CHECK(outbox.enqueue(0,0x0a,packet.data(),16));bool failed=false;
    try{outbox.tick();}catch(const std::runtime_error&){failed=true;}CHECK(failed&&buffer.count_8e81c0==17&&c[0x204+9]==0&&get(c.data(),0x7c)==1);
    // Transport callback changes the current count and counters before source reads.
    b.transport_4977e0=[&](auto* data,auto size,auto target){CHECK(size==17&&target==8&&data[0]==0x0a);buffer.count_8e81c0=23;put(c.data(),0x6c,100);put(c.data(),0x7c,200);};
    MissionClientOutbox remote(messages,buffer,b);CHECK(remote.enqueue(0,0x0a,packet.data(),16));remote.tick();CHECK(get(c.data(),0x6c)==123&&get(c.data(),0x7c)==201);
    // Descriptor diagnostics run before the fresh sequence-flag read.
    b.diagnostic_4564b0=[&](auto id){CHECK(id==0x0a);std::array<std::uint8_t,128> patch{};patch[0x0a*4+1]=2;messages.table().patch(0,patch.data());buffer.count_8e81c0=2;};
    MissionClientOutbox diagnosed(messages,buffer,b);buffer.bytes_8e61c0.fill(0x55);diagnosed.begin();diagnosed.append(0x0a,0x1234,packet.data(),1);
    CHECK(buffer.count_8e81c0==6&&buffer.bytes_8e61c0[2]==0x0a&&buffer.bytes_8e61c0[3]==0x34&&buffer.bytes_8e61c0[4]==0x12);
    c[0x18c]=0;clients[1][0x18c]=1;put(clients[1].data(),0x7c,0xffffffff);
    diagnosed.tick();CHECK(get(clients[1].data(),0x7c)==0);
    GameMessageStage missing_messages;MissionServerMessageBuffer missing_buffer;
    MissionClientOutbox missing_diagnostic(missing_messages,missing_buffer,{});failed=false;
    try{missing_diagnostic.append(0x0a,0,packet.data(),1);}catch(const std::runtime_error&){failed=true;}
    CHECK(failed&&missing_buffer.count_8e81c0==0);
    std::cout<<"client outbox checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
