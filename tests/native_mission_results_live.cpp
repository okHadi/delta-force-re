#include "dfre/world_runtime.hpp"
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
    std::map<std::uint32_t,std::uint8_t> memory;
    const auto read=[&](std::uint32_t at,unsigned width=4){if(at==0x67dbec&&width==4)return events.flags_67dbec();std::uint32_t n=0;for(unsigned i=0;i<width;++i)n|=std::uint32_t(memory[at+i])<<(8*i);return n;};
    const auto write=[&](std::uint32_t at,unsigned width,std::uint32_t n){if(at==0x67dbec&&width==4){events.flags_67dbec()=n;return;}for(unsigned i=0;i<width;++i)memory[at+i]=std::uint8_t(n>>(8*i));};
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
    std::vector<AssetBytes> clients(17,AssetBytes(mission_client_record_size));
    const auto client=[&](std::size_t slot,std::uint32_t at,unsigned width){std::uint32_t n=0;for(unsigned i=0;i<width;++i)n|=std::uint32_t(clients.at(slot).at(at+i))<<(8*i);return n;};
    const auto client_put=[&](std::size_t slot,std::uint32_t at,unsigned width,std::uint32_t n){for(unsigned i=0;i<width;++i)clients.at(slot).at(at+i)=std::uint8_t(n>>(8*i));};
    const auto local=actor_handle(3,0);
    write(0x67f528,4,1);write(0x501340,4,7);write(0x67f52c,4,1);write(0x67dbec,4,5);
    client_put(0,0,4,7);client_put(0,8,4,local);client_put(0,0x18c,1,1);
    client_put(0,0x1c4,2,5);client_put(0,0x1b8,4,1);
    client_put(0,0x1a0,4,5);client_put(0,0x1ac,4,2);client_put(0,0x1a8,4,3);
    GameMessageStage messages;MissionServerMessageBuffer buffer;
    MissionClientOutboxBindings transport;
    transport.client=[&](auto slot){return MissionClientRecordView{clients.at(slot).data(),clients.at(slot).size()};};
    transport.cell=[&](auto at){return read(at);};
    MissionClientOutbox outbox(messages,buffer,transport);
    MissionRoundServiceBindings round;
    round.cell=read;round.store=write;round.client=client;
    round.squad=[&](auto slot,auto at,auto width){return read(0x6e8090+slot*0x68+at,width);};
    round.squad_store=[&](auto slot,auto at,auto width,auto n){write(0x6e8090+slot*0x68+at,width,n);};
    MissionLogicResultBindings result;
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
    result.actor_source_address=[&](auto identity){check(identity==local,"Result used a different local actor");return 0x7cca80u;};
    result.actor_identity=[&](auto group,auto slot){check(group==3&&slot==0,"Result actor index changed");return local;};
    result.local_actor=[&](){return std::optional<std::uint32_t>(local);};
    result.actor_word=[&](auto identity,auto at,auto width){check(identity==local&&at==0x6e&&width==4,"Result actor field changed");return 0x10000u;};
    result.actor_name=[&](auto identity){check(identity==local,"Result name actor changed");return std::string("ALPHA");};
    result.store_name=[&](auto at,const auto& text){for(auto ch:text)memory[at++]=std::uint8_t(ch);memory[at]=0;};
    unsigned published=0,delivered=0;
    result.publish=[&](auto slot,auto id,auto& packet){check(outbox.enqueue(slot,std::int32_t(id),packet.data(),packet.size()),"Client outbox refused result");++published;};
    std::array<std::uint16_t,kGameMessageDedupSlots> dedup{};std::uint32_t received=0,idle=9;
    GameMessageLiveBindings live;live.dedup=&dedup;live.received_bytes_bcfc24=&received;live.update_counter_bc5d20=&idle;
    live.read_global=[&](auto at){return read(at);};
    live.call=[&](const auto& call){check(call.address==0x471010,"Unexpected result message body");mission_logic_receive_result(result,call.payload,call.length);++delivered;};
    messages.set_live_bindings(std::move(live));
    MissionLogicResultPacket packet{};mission_logic_request_result(result,1,packet);
    check(published==1&&read(0x67dbc0)==1&&read(0x67f718)==0,"Result request ran receive effects early");
    outbox.tick();check(!messages.pending_local_echo().empty(),"Client result did not reach shared echo");
    check(messages.check_receive(),"Client result echo was not received");
    check(messages.dispatch_received().empty(),"Live result produced inspection actions");
    check(delivered==1&&sounds==1&&read(0x67f718)==1&&read(0x67f71c)==1&&read(0x67f73c)==2&&read(0x67f740)==3,"Result display lost current client statistics");
    check(voice!=0&&playback.playing(voice),"Live result sound did not enter the voice queue");
    playback.publish();playback.advance_frames(200);check(!playback.playing(voice),"Live result sound did not finish");
    AssetBytes profile(0xa90);profile[0x28]=1;
    MissionProfileBindings save;save.cell=[&](auto at){return read(at);};save.store=[&](auto at,auto n){write(at,4,n);};
    save.profile=[&]()->AssetBytes&{return profile;};
    save.result=[&]{MissionResultImage image{};for(std::size_t i=0;i<image.size();++i)image[i]=memory[0x67f714+std::uint32_t(i)];return image;};
    save.fp_control=[] {return std::uint16_t(0x37f);};
    unsigned ends=0;save.end_events=[&]{++ends;check(delivered==1,"End events ran before live result receive");events.run_bit2_events();};
    mission_profile_leave(save);
    const auto saved=[&](std::size_t at){std::uint32_t n=0;for(unsigned i=0;i<4;++i)n|=std::uint32_t(profile.at(at+i))<<(8*i);return n;};
    check(ends==1&&saved(0x74)==1&&saved(0xb4)==0x15&&saved(0x664)==2&&saved(0x678)==3,"Live result did not save current completion and totals");
}
}
int main(){try{run();std::cout<<"live result outbox and profile checks passed\n";}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
