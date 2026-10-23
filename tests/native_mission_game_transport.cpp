#include "dfre/mission_game_transport.hpp"
#include "dfre/mission_client_outbox.hpp"
#include "dfre/mission_peer_frame.hpp"
#include <cassert>
#include <map>
#include <stdexcept>

namespace {
using U=std::uint32_t;
struct Memory {
    std::map<U,U> words;U depth=0,allocations=0,releases=0;
    dfre::MissionGameTransportBindings bindings(){dfre::MissionGameTransportBindings b;
        b.read=[&](U at,U n){assert(n==4);return words[at];};b.write=[&](U at,U n,U value){assert(n==4);words[at]=value;};
        b.frame=[&](U site,U size){assert(site==0x4b9530&&size==4);++allocations;return 0x1300000u+depth++*0x100;};
        b.release=[&](U site,U,U size){assert(site==0x4b9530&&size==4&&depth);--depth;++releases;};return b;}
    void netsock(){words[0x501320]=1;words[0x501398]=0x501ab8;words[0x501ab8+0x54]=0x4b9530;words[0x501ab8+0x58]=0x4b95d0;words[0x501ab8+0x60]=0x4b96b0;words[0xd0a540]=0x1208000;}
};
template<class F>bool fails(F f){try{f();}catch(const std::runtime_error&){return true;}return false;}
}
int main(){using namespace dfre;
    Memory m;m.netsock();auto b=m.bindings();
    assert(fails([&]{mission_game_transport_receive(b);}));assert(m.depth==0&&m.allocations==1&&m.releases==1&&m.words[0x50135c]==0);
    m.words[0x501320]=0;assert(mission_game_transport_receive(b)==0&&m.depth==0&&m.words[0x1300000]==0x8000);
    auto missing_frame=b;missing_frame.frame={};assert(fails([&]{mission_game_transport_receive(missing_frame);}));assert(m.depth==0);
    assert(mission_game_transport_send_client(b,0,0,0)==0);
    m.netsock();m.words[0x501ab8+0x60]=0x4b85b0;assert(fails([&]{mission_game_transport_send_client(b,1,2,3);}));m.netsock();
    // Retry uses the shared counter after the actual synchronous pump boundary.
    U sends=0,pumps=0;b.call=[&](U site,const auto& args)->U{if(site==0x4c550e){assert(args==std::vector<U>({0x1201000,29,8}));++sends;return 0x10;}assert(site==0x490880);++pumps;return 0;};
    assert(mission_game_transport_send_client(b,0x1201000,29,8)==2&&sends==2049&&pumps==2049&&m.words[0x501364]==2049);
    sends=pumps=0;b.call=[&](U site,const auto&)->U{if(site==0x4c5514){++sends;return 0x10;}assert(site==0x490880);++pumps;m.words[0x501364]=0xffffffffu;return 0;};
    assert(mission_game_transport_send_server(b,7,11)==2&&sends==1&&pumps==1&&m.words[0x501364]==0xffffffffu);
    // One system message is consumed, then a fresh frame and current buffer are used.
    U receives=0,systems=0;m.words[0x501354]=0xffffffffu;
    b.call=[&](U site,const auto& args)->U{if(site==0x4c551a){++receives;assert(m.words[args[1]]==0x8000&&args[2]==0x501344);if(receives==1){assert(args[0]==0x1208000);m.words[args[1]]=7;m.words[args[2]]=0;}else{assert(args[0]==0x1209000&&m.depth==2);m.words[args[1]]=13;m.words[args[2]]=9;m.words[0xd0a540]=0x120a000;}return 0;}assert(site==0x4b9400&&args[0]==0x1208000);++systems;m.words[0xd0a540]=0x1209000;return 0;};
    assert(mission_game_transport_receive(b)==0x120a000&&receives==2&&systems==1&&m.depth==0&&m.words[0x501354]==1&&m.words[0x50135c]==0x120a000&&m.words[0x501360]==13);
    // The captured counter precedes the peer read and its callback mutation.
    auto ordered=b;ordered.read=[&](U at,U n){assert(n==4);if(at==0x501344)m.words[0x501354]=100;return m.words[at];};ordered.call=[&](U site,const auto& args)->U{assert(site==0x4c551a);m.words[args[1]]=13;m.words[args[2]]=9;return 0;};m.words[0x501354]=10;assert(mission_game_transport_receive(ordered)==0x120a000&&m.words[0x501354]==11);
    // An error still publishes the current buffer and returned local length.
    b.call=[&](U site,const auto& args)->U{assert(site==0x4c551a);m.words[args[1]]=17;m.words[0xd0a540]=0x120b000;return 12;};
    assert(mission_game_transport_receive(b)==0&&m.words[0x50135c]==0x120b000&&m.words[0x501360]==17&&m.depth==0);
    // Init preserves the source's unchecked null allocation result.
    std::vector<U> calls;b.call=[&](U site,const auto& args)->U{calls.push_back(site);if(site==0x4c5562){assert(args.empty());return 9;}assert(site==0x4983b0&&args==std::vector<U>({0x8000,0x4ce53c}));assert(m.words[0x501320]==1);return 0;};
    assert(mission_game_transport_initialize_netsock(b)==0&&m.words[0x501320]==1&&m.words[0xd0a540]==0&&calls==std::vector<U>({0x4c5562,0x4983b0}));
    m.words[0x501328]=0x1200000;assert(fails([&]{mission_game_transport_close_netsock(MissionGameTransportBindings{b.read,b.write,{},{},{},{}});}));assert(m.words[0x501320]==1);
    U removals=0;m.words[0xd0a540]=0x1208000;b.call=[&](U site,const auto& args)->U{if(site==0x4b8da0){assert(args[0]==0x1200000);++removals;m.words[0x501328]=0;return 0;}if(site==0x4c555c){m.words[0xd0a540]=0x1209000;return 0;}assert(site==0x498400&&args[0]==0x1209000);return 0;};
    mission_game_transport_close_netsock(b);assert(removals==1&&m.words[0x501320]==0&&m.words[0xd0a540]==0);

    // Actual SP local echo and the receive gate keep the DLL boundary unreached.
    GameMessageStage messages;MissionServerMessageBuffer buffer;m.words[0x50133c]=7;m.words[0x501340]=7;m.words[0x67f520]=0;m.words[0x67f528]=1;U network_calls=0;GameMessageLiveBindings live;live.read_global=[&](U at){return m.words[at];};messages.set_live_bindings(live);
    MissionClientOutboxBindings out;out.cell=[&](U at){return m.words[at];};out.transport_4977e0=[&](std::uint8_t* data,U length,U peer){assert(data==buffer.bytes_8e61c0.data());++network_calls;mission_game_transport_send_client(b,0x8e61c0,length,peer);};MissionClientOutbox outbox(messages,buffer,out);
    std::array<std::uint8_t,16> payload{};outbox.append(0x0a,0,payload.data(),16);assert(outbox.flush(7)==17&&messages.pending_local_echo().size()==17&&network_calls==0);
    MissionPeerFrameBindings peer;peer.cell=out.cell;peer.store=[&](U at,U value){m.words[at]=value;};peer.receive_497790=[&]{++network_calls;return mission_game_transport_receive(b);};
    assert(mission_peer_frame_receive(messages.peer_frame_buffer(),peer)==0&&network_calls==0);
    messages.enqueue_shot();messages.flush_queue();assert(mission_peer_frame_receive(messages.peer_frame_buffer(),peer)==0x8ec1cc&&network_calls==0);
    // Remote paths use the bridge and the same published parser cells.
    m.netsock();b.call=[&](U site,const auto& args)->U{if(site==0x4c550e){assert(args==std::vector<U>({0x8e61c0,17,8})&&buffer.bytes_8e61c0[0]==0x0a);return 0;}assert(site==0x4c551a);m.words[args[1]]=13;m.words[args[2]]=9;return 0;};
    outbox.append(0x0a,0,payload.data(),16);assert(outbox.flush(8)==17&&network_calls==1);m.words[0x67f520]=1;
    assert(mission_peer_frame_receive(messages.peer_frame_buffer(),peer)==0x1208000&&network_calls==2&&m.words[0x8e61b4]==0x1208000&&m.words[0x8e61b8]==13&&m.words[0x8e61bc]==9);
    assert(m.depth==0&&m.allocations==m.releases);
}
