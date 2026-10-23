#include "dfre/mission_peer_parser.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <map>
#include <stdexcept>
#include <vector>
namespace {
using U=std::uint32_t;
U word(const std::vector<std::uint8_t>& a,U at,U width=4){U v=0;for(U i=0;i<width;++i)v|=U(a[at+i])<<(8*i);return v;}
void put(std::vector<std::uint8_t>& a,U at,U v,U width=4){for(U i=0;i<width;++i)a[at+i]=std::uint8_t(v>>(8*i));}
}
int main(){
    std::vector<std::uint8_t> clients(17*0x9b84),packet{0x28,0x34,0x12,0x32};
    put(clients,0,7);put(clients,8,0x1500000);put(clients,0x1c4,5,2);put(clients,0x70,8);put(clients,0x80,9);put(clients,0x98,10);
    std::map<U,U> cells{{0x8e61b4,0x10000000},{0x8e61b8,4},{0x8e61bc,7},{0x67f538,100},{0x150006c,0},{0x50321c,1}};
    dfre::GameMessageStage messages;dfre::MissionServerMessageBuffer buffer;
    dfre::MissionClientOutboxBindings ob;ob.client=[&](std::size_t i){return dfre::MissionClientRecordView{clients.data()+i*0x9b84,0x9b84};};ob.cell=[&](U a){return cells.at(a);};
    dfre::MissionClientOutbox outbox(messages,buffer,ob);dfre::MissionPeerParserBindings b;b.client=ob.client;b.read=[&](U a,U){return cells.at(a);};b.write=[&](U a,U,U value){cells[a]=value;};
    b.resolve=[&](U a,std::size_t n)->std::uint8_t*{if(a<0x10000000||std::uint64_t(a-0x10000000)+n>packet.size())return nullptr;return packet.data()+(a-0x10000000);};
    unsigned handlers=0,diagnostics=0;bool fault=false,mutate=true;
    b.call=[&](const dfre::MissionPeerParserCall& c){
        if(c.address==0x493a80){assert(c.args[1]==0x4cc36c&&c.args[2]==99&&c.payload_argument==0&&c.length==128);++diagnostics;return 0u;}
        if(c.address==0x4564b0){++diagnostics;return 0u;}
        assert(c.address==0x447150&&c.args[0]==0x1500000);++handlers;
        assert(clients[0x204+9]&&clients[0x204+8]==0x14&&word(clients,0x204+10,2)==word(packet,1,2));
        assert(word(clients,0x9b04,2)==word(packet,1,2));
        if(mutate)packet[3]=0;
        if(fault)throw std::runtime_error("fixture handler fault");
        return 0u;
    };
    auto run=[&]{dfre::mission_peer_parser_parse(outbox,messages.table(),b);};
    run();assert(handlers==1&&word(clients,0x1c4,2)==5&&word(clients,0x70)==12&&word(clients,0x80)==10&&word(clients,0x98)==0);
    const auto history=std::vector<std::uint8_t>(clients.begin()+0x9b04,clients.begin()+0x9b84);
    put(clients,0x1c4,5,2);run();assert(handlers==1&&word(clients,0x1c4,2)==5&&clients[0x204+0x264+9]);
    assert(std::equal(history.begin(),history.end(),clients.begin()+0x9b04));
    packet={0x14,0,0};cells[0x8e61b8]=3;run();assert(!clients[0x204+9]&&clients[0x204+0x264+9]);
    const auto saved=clients;cells[0x8e61bc]=99;run();assert(diagnostics==2&&clients==saved);cells[0x8e61bc]=7;
    packet={0x28,0x35,0x12,0x32};cells[0x8e61b8]=4;fault=true;mutate=false;put(clients,0x1c4,5,2);
    bool failed=false;try{run();}catch(const std::runtime_error&){failed=true;}
    assert(failed&&handlers==2&&word(clients,0x1c4,2)==5&&word(clients,0x9b04,2)==0x1235);
    packet={0x28};cells[0x8e61b8]=1;const auto old_bytes=word(clients,0x70);const auto old_packets=word(clients,0x80);failed=false;
    try{run();}catch(const std::runtime_error&){failed=true;}
    assert(failed&&word(clients,0x70)==old_bytes+1&&word(clients,0x80)==old_packets+1&&word(clients,0x9b04,2)==0x1235);
    // A repeated variable message retains its real ACK before a bad cursor fails.
    packet={0x12,0x35,0x12,0xfd,0xff};cells[0x8e61b8]=5;const auto sequence=word(clients,0x1b8);failed=false;
    try{run();}catch(const std::runtime_error&){failed=true;}
    assert(failed&&word(clients,0x1b8)==sequence+1&&word(clients,0x9b04,2)==0x1235);
    packet={0x28,0x36,0x12};cells[0x8e61b8]=3;b.call={};failed=false;
    try{run();}catch(const std::runtime_error&){failed=true;}
    assert(failed&&word(clients,0x9b04,2)==0x1236);
    put(clients,8,0);packet={0x28,0x37,0x12};run();assert(word(clients,0x9b04,2)==0x1237);
    cells[0x8e61b4]=0;b.client={};b.resolve={};run();
}
