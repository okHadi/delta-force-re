#include "dfre/mission_sound_assets.hpp"
#include <iostream>
#include <map>
#include <stdexcept>

using namespace dfre;
namespace {
using U=std::uint32_t;
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
void run(){
    std::map<U,std::uint8_t> bytes;MissionSoundAssetBindings b;
    b.read=[&](U at,U width){U n=0;for(U i=0;i<width;++i)n|=U(bytes[at+i])<<(8*i);return n;};
    b.write=[&](U at,U width,U n){for(U i=0;i<width;++i)bytes[at+i]=std::uint8_t(n>>(8*i));};
    const auto put=[&](U at,U n){b.write(at,4,n);};
    const auto text=[&](U at,const std::string& s){for(auto c:s)bytes[at++]=std::uint8_t(c);bytes[at]=0;};
    unsigned calls=0;
    b.call=[&](U site,const auto& args){++calls;check(site==0x499120&&args==std::vector<U>{0x103c},"Unexpected context open");
        check(bytes[0x103c]==0&&b.read(0x10c4,4)==0,"Context clear must precede aliased filename open");return 0xffffffffu;};
    text(0x103c,"same-context.pwf");put(0x10c4,17);
    check(mission_sound_context_load(b,0x103c,0x1000,9,0x2000)==0&&calls==1,"Failed open changed context load flow");
    bool failed=false;MissionSoundAssetBindings missing=b;missing.call={};
    try{mission_sound_allocate(missing,8,4,0,0,7);}catch(const MissionSoundAssetError&){failed=true;}
    check(failed,"Missing allocator was accepted");
    b.call=[&](U site,const auto& args){check(site==0x497f60&&args==std::vector<U>({8,4,0,0}),"Allocation arguments changed");return 0x3000u;};
    put(0x2ff4,99);check(mission_sound_allocate(b,8,4,0,0,0xffffffffu)==0x3000&&b.read(0x2ff4,4)==99,"Unclassified allocation tag changed");
    mission_sound_allocate(b,8,4,0,0,7);check(b.read(0x2ff4,4)==7,"Allocation class did not reach header");
    put(0x101c,0x4000);put(0x1008,2);text(0x4000,"sample");text(0x4034,"SAMPLE");text(0x2000,"SAMPLE");
    put(0x401c,5);put(0x4050,9);
    check(mission_sound_find_sample(b,0x2000,0x1000)==0x4034,"Sample lookup changed case or stride");
    check(mission_sound_mark_sample(b,0x2000,0x1000)==1&&bytes[0x4056]==1,"Sample preload mark is missing");
    put(0x4050,0);check(mission_sound_find_sample(b,0x2000,0x1000)==0,"Empty sample was accepted");
    put(0xd096e4,0);put(0xd096d8,0x7000);put(0xd0973c,1);put(0xd096dc,1);put(0xd096f0,0x7100);
    put(0x70ac,1);put(0x71ac,0);unsigned opens=0;
    b.call=[&](U site,const auto& args){
        if(site==0x498fb0)return 0x8000u;
        check(site==0x49e630&&args[0]==0x8000,"Archive open did not use current name mapping");++opens;return opens==1?0xffffffffu:3u;
    };
    check(mission_sound_file_open(b,0x2000)==3&&opens==2&&b.read(0xd09738,4)==0xd096f0&&b.read(0xd09730,4)==1,"Secondary archive selection changed");
    put(0xd096d8,0);put(0xd096e8,1);put(0xd096f0,0);put(0xd0973c,0);unsigned failures=0;
    b.call=[&](U site,const auto&){if(site==0x498cf0)return 0xffffffffu;check(site==0x499750,"Open failure diagnostic changed");++failures;return 0u;};
    check(mission_sound_file_open(b,0x2000)==0xffffffffu&&failures==1,"Open failure result changed");
    // The archive list can exist with no actual records and no EDI assignment.
    put(0xd0973c,1);failed=false;
    try{mission_sound_file_open(b,0x2000);}catch(const MissionSoundAssetError&){failed=true;}
    check(failed&&failures==2,"Undefined incoming EDI was invented");
    check(mission_sound_file_open(b,0x2000,42)==42&&failures==3,"Explicit incoming EDI was lost");
}
}
int main(){try{run();std::cout<<"sound asset checks passed\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
