#include "dfre/mission_audio_cache.hpp"
#include <cassert>
#include <map>
#include <string>
using namespace dfre;
namespace {
using U=std::uint32_t;constexpr U A=0x1300000,D=0x1302000,E=0x1301000;
struct Fixture {
    MissionAudioCacheBindings b;std::map<U,std::uint8_t> bytes;std::vector<U> calls;
    void put(U at,U width,U value){for(U i=0;i<width;++i)bytes[at+i]=std::uint8_t(value>>(8*i));}
    U get(U at,U width=4){U n=0;for(U i=0;i<width;++i)n|=U(bytes.at(at+i))<<(8*i);return n;}
    Fixture(){
        for(U i=0;i<64;++i)bytes[A+i]=0;for(U i=0;i<272;++i)bytes[D-16+i]=0;
        for(U at:{0xd09730u,0xd096e0u,0xd096d8u,0xd09738u,0xd09398u,0x5013a0u,0xd096d0u,0x4cdff0u,E})put(at,4,0);
        put(0x4cdff0,4,0x12345678);put(A+0x1c,4,64);put(A+0x18,4,31);put(A+0x24,4,91);
        put(D+0x18,4,22050);put(D+0x22,2,8);put(D+0x28,4,4);put(D+0x2c,4,0x44332211);
        b.read=[&](U at,U width){assert(width==1||width==2||width==4);return get(at,width);};b.write=[&](U at,U width,U value){assert(width==1||width==2||width==4);put(at,width,value);};
        b.call=[&](U site,const auto& args){calls.push_back(site);if(site==0x497f60){assert(args.at(1)==4&&args.at(2)==0);return D;}if(site==0x498d60||site==0x498d30)return 0u;throw MissionAudioCacheError("Unexpected cache backend");};
    }
};
template<class F> bool fails(F&& f){try{f();}catch(const MissionAudioCacheError&){return true;}return false;}
}
int main(){
    {MissionAudioCacheBindings b;assert(fails([&]{mission_audio_cache_load(b,A,0);}));assert(mission_audio_cache_tag(b,0,7)==0);assert(mission_audio_cache_tag(b,D,0x1000000)==D);}
    {Fixture f;f.put(A+0x23,1,1);f.b.call={};f.b.write={};mission_audio_cache_load(f.b,A,0);assert(f.calls.empty());}
    {Fixture f;f.b.call={};assert(fails([&]{mission_audio_cache_load(f.b,A,0);}));assert(f.get(A+0x23,1)==0&&f.get(A+0x24)==91);}
    {Fixture f;f.b.call=[](U,const auto&){return 0u;};mission_audio_cache_load(f.b,A,0);assert(f.get(A+0x24)==0&&f.get(A+0x23,1)==0);}
    {Fixture f;mission_audio_cache_load(f.b,A,0);assert((f.calls==std::vector<U>{0x497f60,0x498d60,0x498d30}));assert(f.get(A+0x24)==D&&f.get(A+0x23,1)==1);assert(f.get(D)==0x12345678&&f.get(D+4)==4&&f.get(D+8)==0x10000000&&f.get(D+0xc,2)==0x100&&f.get(D+0x10)==0x44332211);}
    {Fixture f;f.b.call=[&](U site,const auto& args){f.calls.push_back(site);if(site==0x497f60){f.put(A+0x18,4,77);return D;}if(site==0x498d60){assert(args.at(0)==77);f.put(A+0x1c,4,88);f.put(0xd09730,4,1);f.put(0xd09738,4,E);f.put(E,4,55);return 0u;}assert(site==0x49e6b0&&args.at(0)==D&&args.at(1)==88&&args.at(2)==55);f.put(A+0x23,1,9);f.put(A+0x24,4,777);return 0u;};mission_audio_cache_load(f.b,A,0);assert(f.get(A+0x23,1)==1&&f.get(A+0x24)==D);}
    {Fixture f;f.put(0xd09398,4,1);f.b.call=[&](U site,const auto& args){assert(site==0x497f60&&(args==std::vector<U>{9,4,0,0}));assert(f.get(0x5013a0)==0xd09398);f.put(0x5013a0,4,91);return D;};assert(mission_audio_cache_allocate_alternate(f.b,9)==D&&f.get(0x5013a0)==0xd09384);f.b.call={};assert(fails([&]{mission_audio_cache_allocate_alternate(f.b,9);}));assert(f.get(0x5013a0)==0xd09398);}
    {Fixture f;f.b.call={};f.b.write={};assert(mission_audio_cache_allocate_alternate(f.b,9)==0);}
    {Fixture f;assert(mission_audio_cache_tag(f.b,D,0xffffff)==D-16);assert(f.get(D-12)==0xffffff);}
    {Fixture f;f.put(D,4,0x12345678);const auto read=f.b.read;U reads=0;f.b.read=[&](U at,U width){++reads;assert(at==D||at==0x4cdff0);return read(at,width);};assert(mission_audio_cache_convert(f.b,D)==D&&reads==2);}
    {Fixture f;f.put(D+0x28,4,0);assert(fails([&]{mission_audio_cache_convert(f.b,D);}));assert(f.get(D)==0x12345678&&f.get(D+4)==0&&f.get(D+0xc,2)==0x100);}
    {Fixture f;f.put(D+0x22,2,16);f.put(D+0x28,4,4);f.put(D+0x2c,4,0);f.put(D+0x10,1,0xcc);assert(fails([&]{mission_audio_cache_convert(f.b,D);}));assert(f.get(D+4)==2&&f.get(D+0x10,1)==0xcc);}
    {Fixture f;f.put(D+0x22,2,16);f.put(D+0x28,4,4);f.put(D+0x2c,4,0x80007fff);mission_audio_cache_convert(f.b,D);assert(f.get(D+4)==2&&f.get(D+0x10,1)==0x80&&f.get(D+0x11,1)==0x80);}
    {Fixture f;f.put(0xd09730,4,1);f.b.call={};assert(mission_audio_cache_close(f.b)==0);f.put(0xd09730,4,0);assert(fails([&]{mission_audio_cache_close(f.b);}));f.b.call=[&](U site,const auto& args){assert(site==0x4c6854&&args.at(0)==0);f.put(0xd096e0,4,77);return 0xffffffffu;};assert(mission_audio_cache_close(f.b)==0xffffffffu&&f.get(0xd096e0)==77);}
    return 0;
}
