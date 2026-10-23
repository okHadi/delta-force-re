#include "dfre/world_runtime.hpp"
#include "dfre/mission_result_live.hpp"
#include "dfre/mission_client_live.hpp"
#include "dfre/mission_client_outbox.hpp"
#include "dfre/mission_logic.hpp"
#include "dfre/mission_results.hpp"
#include "dfre/mission_round_services.hpp"
#include "dfre/mission_sound_bank.hpp"
#include "dfre/mission_audio_playback.hpp"
#include <iostream>
#include <map>
#include <stdexcept>

using namespace dfre;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void run(){
    EventStage events;MissionEvent end_event;end_event.words={4,0,0,0,0,1u<<16};
    events.load_tables({end_event},{},{{0,5,0,4,0,0,0,0}});
    MissionTickState canonical;
    std::map<std::uint32_t,std::uint8_t> memory;
    const auto read=[&](std::uint32_t at,unsigned width=4){if(at==0x67dbec&&width==4)return events.flags_67dbec();if(width==4){if(at==0x67dbc0)return canonical.globals.menu_flag_67dbc0;if(at==0x67f520)return canonical.globals.multiplayer_67f520;if(at==0x67f528)return canonical.globals.mission_67f528;if(at==0x67f52c)return canonical.globals.mission2_67f52c;if(at==0x67f53c)return canonical.game_type_67f53c;}std::uint32_t n=0;for(unsigned i=0;i<width;++i)n|=std::uint32_t(memory[at+i])<<(8*i);return n;};
    const auto write=[&](std::uint32_t at,unsigned width,std::uint32_t n){if(at==0x67dbec&&width==4){events.flags_67dbec()=n;return;}if(width==4){std::uint32_t* field=nullptr;if(at==0x67dbc0)field=&canonical.globals.menu_flag_67dbc0;if(at==0x67f520)field=&canonical.globals.multiplayer_67f520;if(at==0x67f528)field=&canonical.globals.mission_67f528;if(at==0x67f52c)field=&canonical.globals.mission2_67f52c;if(at==0x67f53c)field=&canonical.game_type_67f53c;if(field){*field=n;return;}}for(unsigned i=0;i<width;++i)memory[at+i]=std::uint8_t(n>>(8*i));};
    // One preloaded sample fixture. Sound selection and voice state use real bodies.
    const std::uint32_t bank=0x30000000,layer=bank+0x100,member=bank+0x200,asset=bank+0x300,sample=bank+0x400;
    write(0x4f2ea8+499*0x28,4,bank);write(bank+0x1c,4,0x10000);write(bank+0x24,4,1);write(bank+0x28,4,layer);
    write(layer,2,1);write(layer+8,4,2);write(0x501500,4,2);write(layer+0x10,4,member);write(member,4,asset);write(member+4,4,0x10000);
    write(asset+0x23,1,1);write(asset+0x24,4,sample);
    AssetBytes pcm(20);pcm[0]='S';pcm[1]='A';pcm[2]='M';pcm[3]='P';pcm[4]=4;pcm[11]=0x10;pcm[13]=1;
    pcm[16]=128;pcm[17]=130;pcm[18]=126;pcm[19]=128;
    std::uint32_t audio_enabled=1,voice=0;MissionAudioPlaybackCells voice_cells{audio_enabled};MissionAudioPlaybackBindings audio;
    audio.sample=[&](auto address){check(address==sample,"Result selected another sample");return MissionAudioSampleView{pcm.data(),pcm.size(),sample};};
    audio.pcm_byte=[&](auto address){check(address>=sample&&address-sample<pcm.size(),"Result sound read outside sample");return pcm.at(address-sample);};
    MissionAudioPlayback playback(voice_cells,audio);playback.initialize();
    MissionSoundBankBindings sound;
    sound.read=read;sound.write=write;
    sound.call=[&](auto site,const auto& args){check(site==0x494dd0,"Unexpected result sound service");voice=playback.play(args.at(0),args.at(1),args.at(2),args.at(3));return voice;};
    MissionClientLive clients;
    const auto client=[&](std::size_t slot,std::uint32_t at,unsigned width){return clients.read(MissionClientLive::source_address(slot,at),width);};
    const auto client_put=[&](std::size_t slot,std::uint32_t at,unsigned width,std::uint32_t n){clients.write(MissionClientLive::source_address(slot,at),width,n);};
    const auto local=mission_actor_source_address(3,0);
    canonical.config.local_actor=0;canonical.tables.groups[3].resize(1);canonical.tables.groups[3][0].field_70=1;canonical.tables.groups[3][0].person_slot=0;canonical.people.resize(1);canonical.people[0].name="ALPHA";
    write(0x67f528,4,1);write(0x501340,4,7);write(0x67f52c,4,1);write(0x67dbec,4,5);
    client_put(0,0,4,7);client_put(0,8,4,local);client_put(0,0x18c,1,1);
    client_put(0,0x1c4,2,5);client_put(0,0x1b8,4,1);
    client_put(0,0x1a0,4,5);client_put(0,0x1ac,4,2);client_put(0,0x1a8,4,3);
    GameMessageStage messages;MissionServerMessageBuffer buffer;
    MissionClientOutboxBindings transport;
    transport.client=[&](auto slot){return clients.client(slot);};
    transport.cell=[&](auto at){return read(at);};
    MissionClientOutbox outbox(messages,buffer,transport);
    MissionRoundServiceBindings round;
    round.cell=read;round.store=write;round.client=client;
    round.squad=[&](auto slot,auto at,auto width){return read(0x6e8090+slot*0x68+at,width);};
    round.squad_store=[&](auto slot,auto at,auto width,auto n){write(0x6e8090+slot*0x68+at,width,n);};
    MissionResultLiveHost result;
    result.state.cell=[&](auto at){return read(at);};
    result.state.store=[&](auto at,auto n){write(at,4,n);};
    result.state.client=client;result.state.client_store=client_put;
    unsigned sounds=0;
    result.state.call=[&](auto site,const auto& args){
        switch(site){
        case 0x446100:mission_round_set_client_updates(round,args.at(0));break;
        case 0x448d20:mission_round_log(round,args.at(0));break;
        case 0x446da0:mission_round_collect_squads(round);break;
        case 0x448130:return std::uint32_t(mission_round_compare_clients(round,args.at(0),args.at(1)));
        case 0x43e720:check(args==std::vector<std::uint32_t>{499},"Result sound changed");mission_sound_global_request(sound,499,bank+0x500);++sounds;break;
        default:throw std::runtime_error("Unexpected result service");
        }
        return 0u;
    };
    result.store_name=[&](auto at,const auto& text){for(auto ch:text)memory[at++]=std::uint8_t(ch);memory[at]=0;};
    unsigned published=0,delivered=0;
    result.publish=[&](auto slot,auto id,auto& packet){check(outbox.enqueue(slot,std::int32_t(id),packet.data(),packet.size()),"Client outbox refused result");++published;};
    auto live_result=mission_result_live_bindings(canonical,result);
    std::array<std::uint16_t,kGameMessageDedupSlots> dedup{};std::uint32_t received=0,idle=9;
    GameMessageLiveBindings live;live.dedup=&dedup;live.received_bytes_bcfc24=&received;live.update_counter_bc5d20=&idle;
    live.read_global=[&](auto at){return read(at);};
    live.call=[&](const auto& call){check(call.address==0x471010,"Unexpected result message body");mission_logic_receive_result(live_result,call.payload,call.length);++delivered;};
    messages.set_live_bindings(std::move(live));
    MissionLogicResultPacket packet{};mission_logic_request_result(live_result,1,packet);
    check(published==1&&read(0x67dbc0)==1&&read(0x67f718)==0,"Result request ran receive effects early");
    outbox.tick();check(!messages.pending_local_echo().empty(),"Client result did not reach shared echo");
    check(messages.check_receive(),"Client result echo was not received");
    check(messages.dispatch_received().empty(),"Live result produced inspection actions");
    check(delivered==1&&sounds==1&&read(0x67f718)==1&&read(0x67f71c)==1&&read(0x67f73c)==2&&read(0x67f740)==3,"Result display lost current client statistics");
    check(voice!=0&&playback.playing(voice),"Live result sound did not enter the voice queue");
    playback.publish();playback.advance_frames(200);check(!playback.playing(voice),"Live result sound did not finish");
    check(read(0x67f724,1)=='A',"Result did not copy canonical person name");
    canonical.tables.groups[3][0].field_70=2;check(live_result.actor_word(local,0x6e,4)==0x20000,"Result adapter cached an actor snapshot");
    bool failed=false;try{live_result.actor_word(local+1,0x6e,4);}catch(const MissionTickError&){failed=true;}check(failed,"Misaligned actor identity succeeded");
    failed=false;try{live_result.actor_word(local,0x113,2);}catch(const MissionTickError&){failed=true;}check(failed,"Cross-record actor read succeeded");
    failed=false;try{live_result.actor_name(mission_actor_source_address(3,1));}catch(const MissionTickError&){failed=true;}check(failed,"Unowned physical tail silently succeeded");
    canonical.tables.groups[3][0].person_slot.reset();failed=false;try{live_result.actor_name(local);}catch(const MissionTickError&){failed=true;}check(failed,"Unbound fallback name silently succeeded");
    unsigned names=0;result.fallback_name=[&](auto identity){check(identity==local,"Fallback lost current actor context");++names;canonical.tables.groups[3][0].field_70=3;return std::string("CURRENT");};
    live_result=mission_result_live_bindings(canonical,result);check(live_result.actor_name(local)=="CURRENT"&&names==1&&live_result.actor_word(local,0x6e,4)==0x30000,"Fallback mutation did not reach canonical fields");
    check(live_result.actor_source_address(0)==0,"Null wire pointer was replaced with a default actor");
    const auto before=read(0x67f718);failed=false;try{mission_logic_receive_result(live_result,nullptr,0);}catch(const std::runtime_error&){failed=true;}check(failed&&read(0x67f718)==before,"Invalid receive changed the current result");
    auto missing=result;missing.publish={};auto missing_live=mission_result_live_bindings(canonical,missing);write(0x67dbc0,4,0);MissionLogicResultPacket failure_packet{};failure_packet.fill(0xa5);failed=false;try{mission_logic_request_result(missing_live,1,failure_packet);}catch(const std::runtime_error&){failed=true;}check(failed&&canonical.globals.menu_flag_67dbc0==0&&failure_packet[2]==0xa5,"Missing publish lost the source failure prefix or SP caller padding");
    canonical.config.local_actor.reset();check(!live_result.local_actor(),"Missing local actor became a default identity");
    result.physical_tail=[](const AiActorRef& ref){check(ref.group==3&&ref.index==1,"Tail identity differs");AiActorImage image{};image[0x70]=7;return image;};live_result=mission_result_live_bindings(canonical,result);check(live_result.actor_word(mission_actor_source_address(3,1),0x6e,4)==0x70000,"Explicit physical tail was not read");
}
}
int main(){try{run();std::cout<<"Canonical live result/outbox/receive/audio checks passed\n";}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
