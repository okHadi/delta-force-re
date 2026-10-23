#include "dfre/mission_sound_registration.hpp"
#include "dfre/mission_audio_frame.hpp"
#include <cstring>
#include <iostream>
#include <map>
namespace {void check(bool ok){if(!ok)throw std::runtime_error("Sound registration check failed");}}
int main(){try{
    using U=std::uint32_t;dfre::Fixed3 listener{};U yaw=0;dfre::MissionAudioFrameCells frame{7,listener,yaw};
    static_assert(sizeof(dfre::MissionAudioRegistration)==16);
    std::map<U,std::uint8_t> bytes;auto* registrations=reinterpret_cast<std::uint8_t*>(frame.registrations_6e0700.data());
    const auto put=[&](U at,U width,U n){for(U i=0;i<width;++i){if(at+i>=0x6e0700&&at+i<0x6e4700)registrations[at+i-0x6e0700]=std::uint8_t(n>>(i*8));else bytes[at+i]=std::uint8_t(n>>(i*8));}};
    const auto get=[&](U at,U width){U n=0;for(U i=0;i<width;++i){const U value=at+i>=0x6e0700&&at+i<0x6e4700?registrations[at+i-0x6e0700]:bytes[at+i];n|=value<<(i*8);}return n;};
    constexpr U actor=0x3100000,definition=0x3200000,id=5,entry=0x4f2ea8+id*40;
    put(entry+0x20,1,2);put(entry,4,definition);put(definition+0x18,4,99);put(definition+0x48,4,0x12345);put(0x4f7ea4,4,0xffffffff);
    dfre::MissionSoundBankBindings b{get,put,{}};
    dfre::mission_sound_attach(b,actor,id,0x123,0x456);
    check(frame.registrations_6e0700[0].definition==definition&&frame.registrations_6e0700[0].actor==actor&&frame.registrations_6e0700[0].identity==0xffffffff&&frame.registrations_6e0700[0].reserved==0x5623);
    check(get(actor+0x1c,4)==0x6e0700&&get(0x4f7ea4,4)==0&&dfre::mission_sound_loudness(b,id)==0x23450000);
    dfre::MissionAudioFrameBindings audio;
    audio.actor_word=[&](U handle,U offset){check(handle==actor);return offset?0u:1u;};
    audio.definition_word=[&](U handle,U offset){check(handle==definition);return offset==0x18?99u:offset==0x1c?65536u:255u;};
    unsigned starts=0;audio.start=[&](U sample,U pan,U volume,U rate,U priority){check(sample==99&&pan==255&&volume==254&&rate==65536&&priority==10);++starts;return 42u;};
    dfre::mission_audio_frame(frame,audio);check(starts==1&&frame.identities_6e06d0[0]==0xffffffff&&frame.handles_6e06e8[0]==42);
    put(definition+0x18,4,0);unsigned loads=0;b.call=[&](U site,const std::vector<U>& args){check(site==0x43ee80&&args==std::vector<U>{id});++loads;put(entry,4,definition+0x100);return 1;};
    dfre::mission_sound_attach(b,actor,id,1,2);check(loads==1&&frame.registrations_6e0700[1].definition==definition+0x100);
    for(auto& row:frame.registrations_6e0700)row.actor=actor;
    const auto generation=get(0x4f7ea4,4);put(definition+0x118,4,7);dfre::mission_sound_attach(b,actor,id,1,2);check(get(0x4f7ea4,4)==generation);
    frame.registrations_6e0700[100].actor=0;b.write=[&](U at,U width,U n){if(at==actor+0x1c)throw std::runtime_error("Fixture actor store fault");put(at,width,n);};
    bool failed=false;try{dfre::mission_sound_attach(b,actor,id,1,2);}catch(const std::runtime_error&){failed=true;}
    check(failed&&frame.registrations_6e0700[100].actor==actor&&get(0x4f7ea4,4)==generation);
    put(entry,4,0);check(dfre::mission_sound_loudness(b,id)==0);
    std::cout<<"sound registration checks passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
