#include "dfre/mission_audio_frame.hpp"
#include <iostream>
#include <stdexcept>
using namespace dfre;
#define CHECK(x) do{if(!(x))throw std::runtime_error("audio frame: " #x);}while(false)
int main(){try{
    Fixed3 listener{};std::uint32_t yaw=0;MissionAudioFrameCells c{0,listener,yaw};MissionAudioFrameBindings b;
    mission_audio_frame(c,b);CHECK(c.counter_4f7ea8==1);c.counter_4f7ea8=0x7fffffff;mission_audio_frame(c,b);CHECK(c.counter_4f7ea8==0x80000000);
    c.counter_4f7ea8=7;c.registrations_6e0700[0]={1,1,22,99};
    b.actor_word=[](auto actor,auto offset){return actor==1&&offset==0?1u:0u;};
    b.definition_word=[](auto,auto offset){return offset==0x1c?65536u:offset==0x18?31u:255u;};
    bool failed=false;try{mission_audio_frame(c,b);}catch(const std::runtime_error&){failed=true;}
    CHECK(failed&&c.counter_4f7ea8==0&&c.selected_6e0470[0].identity==22&&c.selected_6e0470[0].rate==0x10000);
    b.start=[&](auto sample,auto pan,auto volume,auto rate,auto priority){CHECK(sample==31&&pan==255&&volume==254&&rate==0x10000&&priority==10);CHECK(c.identities_6e06d0[0]==22);return 123u;};
    c.counter_4f7ea8=7;mission_audio_frame(c,b);CHECK(c.handles_6e06e8[0]==123);
    b.update=[&](auto handle,auto,auto,auto){CHECK(handle==123&&c.selected_6e0470[0].played==0);c.handles_6e06e8[1]=444;c.counter_4f7ea8=92;};
    b.stop=[&](auto handle){CHECK(handle==444);c.handles_6e06e8[1]=555;};
    c.counter_4f7ea8=7;mission_audio_frame(c,b);CHECK(c.selected_6e0470[0].played==1&&c.counter_4f7ea8==92&&c.handles_6e06e8[1]==0);
    c.registrations_6e0700[0].actor=2;c.counter_4f7ea8=7;b.stop=[&](auto handle){CHECK(handle==123);throw std::runtime_error("stop failure");};
    failed=false;try{mission_audio_frame(c,b);}catch(const std::runtime_error&){failed=true;}CHECK(failed&&c.registrations_6e0700[0].definition==0&&c.handles_6e06e8[0]==123);
    CHECK(mission_audio_attenuation(0,1,255,255)==254);CHECK(mission_audio_attenuation(2,1,255,255)==0);
    failed=false;try{mission_audio_attenuation(0,0,1,1);}catch(const std::runtime_error&){failed=true;}CHECK(failed);
    MissionAudioFrameCells capped{7,listener,yaw};
    for(std::uint32_t i=0;i<65;++i)capped.registrations_6e0700[i]={1,i+1,i+1,99};
    b.actor_word=[](auto actor,auto offset){return offset==0&&actor!=65?1u:0u;};
    b.definition_word=[](auto,auto offset){return offset==0x1c?1u:offset==0x18?31u:255u;};
    unsigned starts=0;b.start=[&](auto,auto,auto,auto,auto){++starts;return 0u;};
    mission_audio_frame(capped,b);CHECK(starts==6&&capped.candidates_6e0270[63].registration==63&&capped.registrations_6e0700[64].definition==1);
    std::cout<<"mission audio frame checks passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
