#include "dfre/mission_scoreboard.hpp"
#include "dfre/mission_live.hpp"
#include "dfre/game_messages.hpp"
#include "dfre/mission_client_outbox.hpp"
#include <array>
#include <cstring>
#include <map>
#include <stdexcept>
#include <string>

namespace {using U=std::uint32_t;
void verify(bool n,int line){if(!n)throw std::runtime_error("Scoreboard check failed at "+std::to_string(line));}
#define check(value) verify(bool(value),__LINE__)
}
int main(){using namespace dfre;
    MissionTickState state;state.tables.groups[3].resize(1);state.people.resize(1);
    state.tables.groups[3][0].active=true;state.tables.groups[3][0].person_slot=0;
    state.people[0].name="Current";
    std::map<U,std::uint8_t> bytes;
    const auto put=[&](U at,U width,U value){for(U i=0;i<width;++i)bytes[at+i]=std::uint8_t(value>>(i*8));};
    const auto get=[&](U at,U width){U value=0;for(U i=0;i<width;++i)value|=U(bytes[at+i])<<(i*8);return value;};
    constexpr U frame=0x1308000,packet=0x1200000;
    MissionScoreboardBindings b;
    b.read=[&](U at,U width){
        if(at==0x7cca8c&&width==4){const auto person=state.tables.groups[3][0].person_slot;return person?0x794680u+U(*person)*0xb4u:0u;}
        if(at>=0x7cca80&&std::uint64_t(at)+width<=0x7ccb94){const auto image=snapshot_mission_actor(state,{3,0});U n=0;for(U i=0;i<width;++i)n|=U(image.at(at-0x7cca80+i))<<(i*8);return n;}
        if(at>=0x794680&&std::uint64_t(at)+width<=0x794734){const auto image=ai_person_image(state.people[0]);U n=0;for(U i=0;i<width;++i)n|=U(image.at(at-0x794680+i))<<(i*8);return n;}
        return get(at,width);
    };
    b.write=[&](U at,U width,U value){put(at,width,value);};
    U lookups=0;b.call=[&](U site,const auto& args)->U{check(site==0x404cf0&&args==std::vector<U>({0x4ccccc,0x4cccc0}));++lookups;put(0x1210000,4,0x00495041);return 0x1210000;};
    std::array<std::uint8_t,143> payload{};payload[0x8e]=1;payload[0x8d]=7;
    payload[0x4d]=0xfe;payload[0x4e]=0xff;payload[0]=0xff;payload[1]=0xff;payload[6]=8;payload[9]=9;
    GameMessageStage messages;GameMessageLiveBindings live;U received=0,dispatches=0;
    live.received_bytes_bcfc24=&received;live.read_global=[](U at){return at==0x67f528?1u:0u;};
    live.message=[&](auto id,const std::uint8_t* data,std::size_t length){if(id!=0x15)return false;check(length==payload.size());for(U i=0;i<length;++i)put(packet+i,1,data[i]);put(frame+0x44,4,packet);mission_scoreboard_receive(b,frame);++dispatches;return true;};
    messages.set_live_bindings(live);
    MissionServerMessageBuffer server;MissionClientOutboxBindings outbox_bindings;
    outbox_bindings.cell=[](U at){return at==0x501340?9u:0u;};
    MissionClientOutbox outbox(messages,server,outbox_bindings);
    const auto echo=[&]{outbox.append(0x15,0,payload.data(),payload.size());check(outbox.flush(9)==144);check(messages.check_receive());messages.dispatch_received();};
    echo();check(dispatches==1&&received==144&&lookups==0&&get(0xbcfbe8,4)==1&&get(0xbcfbe4,4)==7);
    check(get(0xbcf664,4)==0x72727543);
    check(get(0xbcf684,4)==0xfffffffe);
    check(get(0xbcfbf0,4)==0xffffffff);
    check(get(0xbcfbfc,4)==9);
    check(get(0xbcfc08,4)==8);
    state.people[0].name="New";echo();check(get(0xbcf664,4)==0x0077654e&&lookups==0);
    state.tables.groups[3][0].person_slot.reset();echo();check(lookups==1&&get(0xbcf664,4)==0x00495041);
    b.call={};bool failed=false;try{echo();}catch(const std::runtime_error&){failed=true;}check(failed&&get(0xbcfbe8,4)==1);
    payload[0x8e]=0;echo();check(dispatches==4&&get(0xbcfbe8,4)==0&&get(0xbcfbf0,4)==0xffffffff);
}
