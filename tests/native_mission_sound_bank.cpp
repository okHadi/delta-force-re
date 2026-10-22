#include "dfre/mission_sound_bank.hpp"
#include <array>
#include <iostream>
#include <map>
#include <stdexcept>
#include <vector>

namespace {
using U=std::uint32_t;constexpr U bank=0x3100000,params=0x3200000,context=0x6e0518,scratch=dfre::mission_sound_bank_scratch;
void require(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
struct Fixture{
    std::map<U,std::uint8_t> bytes;dfre::MissionSoundBankBindings b;std::vector<std::vector<U>> calls;
    U get(U at,U width=4)const{U n=0;for(U i=0;i<width;++i){const auto it=bytes.find(at+i);if(it!=bytes.end())n|=U(it->second)<<(i*8);}return n;}
    void put(U at,U n,U width=4){for(U i=0;i<width;++i)bytes[at+i]=std::uint8_t(n>>(i*8));}
    Fixture(){b.read=[&](U at,U width){return get(at,width);};b.write=[&](U at,U width,U n){put(at,n,width);};b.call=[&](U site,const std::vector<U>& args){std::vector<U> row{site};row.insert(row.end(),args.begin(),args.end());calls.push_back(std::move(row));if(site==0x49d050){put(args.at(0)+0x23,1,1);put(args.at(0)+0x24,0x70000000);}return 0x999u;};put(0x501500,2);put(params,0x10000);put(params+8,255);put(params+0xc,0xffffffff);}
    void layers(U count){put(bank+0x24,count);put(bank+0x1c,0x10000);for(U i=0;i<count;++i){const U layer=bank+0x100+i*0x40,variant=bank+0x1000+i*0x40,asset=bank+0x2000+i*0x40;put(bank+0x28+i*4,layer);put(layer,1,2);put(layer+8,3);put(layer+0x10,variant);put(variant,asset);put(variant+4,0x10000);put(asset+0x23,1,1);put(asset+0x24,0x70000000+i);}}
};
void helpers(){Fixture f;f.put(0x5014fc,1);require(dfre::mission_sound_bank_random(f.b,0)==0&&f.get(0x5014fc)==0x4008,"zero range advances sound RNG");require(!dfre::mission_sound_bank_filter(f.b,2)&&dfre::mission_sound_bank_filter(f.b,4),"current sound filter");require(dfre::mission_sound_bank_quality(f.b,0x20),"quality mismatch");f.put(0x501500,0x22);require(!dfre::mission_sound_bank_quality(f.b,0x20),"quality reads current shared flags");}
void global_request(){
    Fixture f;std::size_t writes=0;f.b.write=[&](U at,U width,U n){++writes;f.put(at,n,width);};
    dfre::mission_sound_global_request(f.b,499,params);require(writes==0&&f.calls.empty(),"absent global bank returns before parameter writes");
    bool failed=false;try{dfre::mission_sound_global_request(f.b,512,params);}catch(const dfre::MissionSoundBankError&){failed=true;}require(failed&&writes==0,"global table boundary precedes writes");
    f.layers(1);f.put(0x4f2ea8+499*0x28,bank);f.put(0x4f2ec8+499*0x28,4,1);f.put(bank+0x2023,0,1);
    dfre::mission_sound_global_request(f.b,499,params);
    require(f.get(params)==0x10000&&f.get(params+4)==0&&f.get(params+8)==255&&f.get(params+0xc)==0xffffffff&&f.get(params+0x18)==0&&f.get(params+0x1c)==0,"complete global parameter bytes");
    require(f.calls.size()==5&&f.calls[0]==std::vector<U>{0x499120,0x6e05e0+0x3c}&&f.calls[4][0]==0x494dd0,"global context and lazy call order");
}
void mutations(){
    Fixture f;f.layers(1);f.b.call=[&](U site,const std::vector<U>& args){f.calls.push_back({site});if(f.calls.size()==1){require(site==0x494dd0&&args[0]==0x70000000,"first voice copies current arguments");f.put(params+0xc,3);f.put(scratch+0x14+4,0x70000001);f.put(scratch+0x14+8,0x10000);f.put(scratch+0x14+0xc,7);f.put(scratch+0x14+0x10,13);}else require(site==0x494e80&&args==std::vector<U>{3,0x70000001,13,7,0x10000},"next voice reads callback changes");return U(f.calls.size()+100);};
    require(dfre::mission_sound_bank_play(f.b,bank,params,context)==2&&f.calls.size()==2,"play loop sees appended candidate");require(f.get(scratch)==101&&f.get(scratch+0x14)==102,"source handle writes follow callbacks");
}
void alias(){Fixture f;f.layers(1);require(dfre::mission_sound_bank_play(f.b,bank,scratch,context)==1,"aliased parameter block runs");require(f.calls.size()==1&&f.calls[0]==std::vector<U>{0x494e80,0x10000,0x70000000,0x70000000,0x10000,0x10000},"scratch writes stay visible through aliased parameters");}
void boundaries(){
    {Fixture f;f.layers(9);bool failed=false;try{dfre::mission_sound_bank_play(f.b,bank,params,context);}catch(const dfre::MissionSoundBankError&){failed=true;}require(failed&&f.get(scratch+7*0x14+4)==0x70000007&&f.get(scratch+0xa0)==0,"ninth candidate stops before its first scratch write");}
    {Fixture f;f.layers(1);f.put(bank+0x108,0x12);f.put(bank+0x102,1,2);bool failed=false;try{dfre::mission_sound_bank_play(f.b,bank,params,context);}catch(const dfre::MissionSoundBankError&){failed=true;}require(failed&&f.calls.empty()&&f.get(scratch+4)==0,"invalid round-robin index stops before array access");}
    {Fixture f;f.layers(1);f.put(bank+0x2023,0,1);f.b.call={};bool failed=false;try{dfre::mission_sound_bank_play(f.b,bank,params,context);}catch(const dfre::MissionSoundBankError&){failed=true;}require(failed&&f.get(scratch+4)==0&&f.get(bank+0x2023,1)==0,"missing lazy body stops at its source site");}
    {Fixture f;f.layers(1);f.b.call={};bool failed=false;try{dfre::mission_sound_bank_play(f.b,bank,params,context);}catch(const dfre::MissionSoundBankError&){failed=true;}require(failed&&f.get(scratch+4)==0x70000000&&f.get(scratch+8)==0x10000,"missing playback keeps prepared candidate writes");}
    {Fixture f;f.layers(1);f.put(context+0xc0,256);f.b.call={};require(dfre::mission_sound_bank_play(f.b,bank,params,context)==1&&f.get(scratch)==0,"below-threshold voice needs no backend call");}
    {dfre::MissionSoundBankBindings empty;require(dfre::mission_sound_bank_play(empty,0,0,0)==0,"null bank needs no providers");}
}
void attenuation(){
    {Fixture f;f.layers(1);f.put(bank+0x104,1);f.put(bank+0x100c,256);f.put(bank+0x1010,0xffffffff);f.put(0x501504,255);dfre::mission_sound_bank_play(f.b,bank,params,context);require(f.get(scratch+0xc)==0xffffffff&&f.calls.at(0).at(3)==0xffffffff,"attenuation preserves full DWORD scale and signed ceiling");}
    for(const auto range:{0x10000u,0xffffu}){Fixture f;f.layers(1);f.put(bank+0x104,range);if(range==0xffff)f.put(params+0x14,0x80000000);bool failed=false;try{dfre::mission_sound_bank_play(f.b,bank,params,context);}catch(const dfre::MissionSoundBankError&){failed=true;}require(failed&&f.get(scratch+4)==0x70000000&&f.get(scratch+8)==0x10000&&f.get(scratch+0xc)==0,"attenuation fault retains earlier candidate writes");}
}
}
int main(){try{helpers();global_request();mutations();alias();boundaries();attenuation();std::cout<<"mission sound bank checks passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
