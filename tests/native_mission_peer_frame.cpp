#include "dfre/game_messages.hpp"
#include "dfre/mission_peer_frame.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <map>
#include <stdexcept>
#include <vector>
int main(){
    using U=std::uint32_t;
    dfre::GameMessageStage stage;
    dfre::GameMessageLiveBindings live;live.read_global=[](U a){assert(a==0x67f528);return 1u;};stage.set_live_bindings(live);
    stage.set_networked(true);stage.set_single_player(true);
    auto& buffer=stage.peer_frame_buffer();auto* stable=buffer.data();
    stage.serialize(0x28,0,nullptr,0);stage.flush_frame();
    assert(stage.network_packet().size()==3&&buffer.data()[0]==0x28);
    std::map<U,U> cells{{0x50133c,7},{0x67f520,0},{0x8e61b4,99},{0x8e61b8,88},{0x8e61bc,77}};
    std::vector<U> writes;unsigned received=0;
    dfre::MissionPeerFrameBindings b;
    b.cell=[&](U a){return cells.at(a);};b.store=[&](U a,U v){writes.push_back(a);cells[a]=v;};
    b.receive_497790=[&]{++received;return 0u;};
    assert(dfre::mission_peer_frame_receive(buffer,b)==0x8ec1cc);
    assert((writes==std::vector<U>{0x8e61b8,0x8e61bc,0x8e61b4}));
    assert(cells[0x8e61b8]==3&&cells[0x8e61bc]==7&&buffer.size()==0&&received==0);
    assert(buffer.data()==stable&&stable[0]==0x28);
    writes.clear();assert(dfre::mission_peer_frame_receive(buffer,b)==0&&writes.empty());
    stage.serialize(0x2c,0,nullptr,0);stage.flush_frame();assert(buffer.size()==3&&stable[0]==0x2c);
    stage.reset();assert(buffer.size()==0&&stable[0]==0x2c&&buffer.data()==stable);
    cells[0x67f520]=1;b.receive_497790={};bool failed=false;
    try{dfre::mission_peer_frame_receive(buffer,b);}catch(const std::runtime_error&){failed=true;}
    assert(failed&&writes.empty());
    b.receive_497790=[&]{++received;return 0u;};assert(dfre::mission_peer_frame_receive(buffer,b)==0);
    assert(received==1&&cells[0x8e61b4]==0&&cells[0x8e61b8]==0&&cells[0x8e61bc]==0);
    buffer.count_8ee1cc=3;writes.clear();b.store=[&](U a,U v){cells[a]=v;writes.push_back(a);if(a==0x8e61bc)throw std::runtime_error("fixture");};failed=false;
    try{dfre::mission_peer_frame_receive(buffer,b);}catch(const std::runtime_error&){failed=true;}
    assert(failed&&buffer.count_8ee1cc==3&&cells[0x8e61b8]==3&&cells[0x8e61bc]==7);
    // The producer retains its strict less-than-capacity append gate.
    stage.set_networked(true);buffer.count_8ee1cc=8191;
    stage.serialize(0x28,0,nullptr,0);stage.flush_frame();assert(buffer.size()==8191);
}
