#include "dfre/mission_heap.hpp"
#include "dfre/mission_audio_cache.hpp"
#include "dfre/mission_script_registry.hpp"
#include <algorithm>
#include <array>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
using namespace dfre;using U=std::uint32_t;
namespace {
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
struct Arena {
 static constexpr U base=0x28000000,context=0xd09384,callbacks=0x1b008000,frame=0x1b010000,callback=0x405000;
 std::vector<std::uint8_t> bytes=std::vector<std::uint8_t>(0x4000,0xa5);std::array<std::uint8_t,0x14> state{};std::array<std::uint8_t,0x4000> table{};std::array<std::uint8_t,0x100> scratch{};
 std::map<U,U> globals{{0x5013a0,context},{0xcc0870,0x4000},{0x4cdff0,0x504d4153}};
 MissionPeriodicBindings b;unsigned notified=0;bool fail_callback=false;U last_backend=0;
 std::uint8_t* span(U at,U length){if(at>=base&&std::uint64_t(at-base)+length<=bytes.size())return bytes.data()+at-base;if(at>=context&&std::uint64_t(at-context)+length<=state.size())return state.data()+at-context;if(at>=callbacks&&std::uint64_t(at-callbacks)+length<=table.size())return table.data()+at-callbacks;if(at>=frame&&std::uint64_t(at-frame)+length<=scratch.size())return scratch.data()+at-frame;throw std::runtime_error("Native heap address is unowned");}
 U read(U at,U width=4){if(globals.count(at)){check(width==4,"Heap global width differs");return globals.at(at);}const auto* p=span(at,width);U n=0;for(U i=0;i<width;++i)n|=U(p[i])<<(i*8);return n;}
 void write(U at,U width,U n){if(globals.count(at)){check(width==4,"Heap global width differs");globals.at(at)=n;return;}auto* p=span(at,width);for(U i=0;i<width;++i)p[i]=std::uint8_t(n>>(i*8));}
 Arena(){b.read=[&](auto at,auto width){return read(at,width);};b.write=[&](auto at,auto width,auto n){write(at,width,n);};b.call=[&](U site,const std::vector<U>& args)->U{
  if(site==0x49a620){std::fill_n(span(args.at(0),args.at(2)),args.at(2),std::uint8_t(args.at(1)));return 0;}
  if(site==0x49dc80){last_backend=args.at(0);return last_backend<=bytes.size()?base:0;}
  if(site==callback||site==callback+4){++notified;check(!mission_heap_find_previous(b,args.at(0)-16),"Free callback ran before unlink");check(read(args.at(0))==globals.at(0x4cdff0),"Free callback lost current sample bytes");if(fail_callback)throw std::runtime_error("Required callback failed");write(callbacks+2047*8,4,args.at(0));write(callbacks+2047*8+4,4,callback+4);return 0;}
  throw std::runtime_error("Heap child is unbound");};}
 U allocate(U size,U alignment=16,U label=0x12345678){write(frame+0x20,4,size);write(frame+0x24,4,alignment);write(frame+0x28,4,0xfedcba98);write(frame+0x2c,4,label);return mission_heap_allocate(b,frame,0x9abcdef0);}
 U release(U payload){write(frame+0x80+8,4,payload);return mission_heap_release(b,frame+0x80);}
};
void retained_cache(){Arena a;check(mission_heap_initialize(a.b)==0,"Heap initialization failed");check(a.last_backend==0x4000&&a.read(Arena::context+8)==Arena::base&&a.read(Arena::base+8)==0x4ceb74&&a.read(Arena::context+0xc)==0x1000001,"Sentinel or generation differs");check(a.read(Arena::base+0x10)==0xa5a5a5a5,"Initializer cleared arena payload");
 const U owner=a.allocate(0x80),sample=a.allocate(0x80),tail=a.allocate(0x80);check(owner==Arena::base+0x20&&sample>owner&&tail>sample,"Allocation order differs");
 a.write(sample,4,0x46464952);a.write(sample+0x18,4,8000);a.write(sample+0x22,2,8);a.write(sample+0x28,4,4);for(U i=0;i<4;++i)a.write(sample+0x2c+i,1,128+i);
 MissionAudioCacheBindings cache;cache.read=a.b.read;cache.write=a.b.write;check(mission_audio_cache_convert(cache,sample)==sample&&a.read(sample)==0x504d4153&&a.read(sample+0x10)==0x83828180,"Real sample conversion failed");check(mission_audio_cache_tag(cache,sample,123)==sample-16&&a.read(sample-12)==123,"Real cache tag did not use heap header");
 const std::vector<std::uint8_t> retained(a.span(sample,0x80),a.span(sample,0x80)+0x80);a.write(Arena::context+0x10,4,Arena::callbacks);a.write(Arena::callbacks,4,sample);a.write(Arena::callbacks+4,4,Arena::callback);
 check(a.release(sample)==0&&a.notified==2,"Current callback scan missed mutated final row");check(std::equal(retained.begin(),retained.end(),a.span(sample,0x80))&&mission_audio_cache_convert(cache,sample)==sample,"Freed PCM bytes are not readable by the real cache consumer");
 const U reused=a.allocate(0x80,16,0xaabbccdd);check(reused==sample&&std::equal(retained.begin(),retained.end(),a.span(reused,0x80))&&a.read(reused-8)==0xaabbccdd,"Hole allocation did not reuse retained bytes and publish new header");
 a.write(owner+0x1c,4,reused);a.write(owner+0x20,4,tail);a.write(owner+0x38,4,1);MissionSoundBankBindings destroy;destroy.read=a.b.read;destroy.write=a.b.write;bool reached=false;
 destroy.call=[&](U site,const std::vector<U>& args)->U{if(site==0x4980d0)return a.release(args.at(0));if(site==0x49db20){reached=true;check(a.read(owner+0x1c)==0&&a.read(owner+0x20)==0&&a.read(reused)==0x504d4153&&!mission_heap_find_previous(a.b,reused-16)&&!mission_heap_find_previous(a.b,tail-16),"Real49CF70 lost retained freed-array prefix before49DB20");throw std::runtime_error("Required49DB20 boundary");}throw std::runtime_error("Destroy child is unbound");};
 bool stopped=false;try{mission_script_registry_audio_destroy(destroy,owner);}catch(const std::runtime_error& e){check(std::string(e.what())=="Required49DB20 boundary","Destroy failed before required49DB20");stopped=true;}check(stopped&&reached,"Real audio destroy skipped required array consumer");
 const U prior_next=a.read(owner-4);check(!a.allocate(0x4000)&&a.read(owner-4)==prior_next,"Arena overflow changed a live link");
}
void reuse_sequences(){for(U size:{0u,1u,15u,16u,31u,64u,128u,512u})for(U align:{4u,16u}){Arena a;check(mission_heap_initialize(a.b)==0,"Sequence init failed");const auto first=a.allocate(size,align),middle=a.allocate(size,align),last=a.allocate(size,align);check(first&&middle&&last,"Sequence allocation failed");for(U i=0;i<size;++i)a.write(middle+i,1,(i*29+7)&255);check(a.release(middle)==0,"Sequence free failed");const auto reused=a.allocate(size,align);check(reused==middle,"Sequence did not reuse middle hole");for(U i=0;i<size;++i)check(a.read(reused+i,1)==((i*29+7)&255),"Sequence lost retained payload");}}
void errors_and_budget(){Arena a;a.globals.at(0xcc0870)=0x1800000;check(mission_heap_initialize(a.b)==0xffffffff&&a.last_backend==0x1800000&&a.read(Arena::context+4)==0x1800000&&a.read(Arena::context+8)==0,"24MiB backend failure prefix differs");a.globals.at(0xcc0870)=0;check(mission_heap_initialize(a.b)==0xffffffff&&a.last_backend==0xf00000&&a.read(Arena::context+4)==0xf00000,"Zero budget fallback differs");
 Arena f;check(mission_heap_initialize(f.b)==0,"Error fixture init failed");const U p=f.allocate(0x80);f.write(p,4,0x504d4153);f.write(Arena::context+0x10,4,Arena::callbacks);f.write(Arena::callbacks,4,p);f.write(Arena::callbacks+4,4,Arena::callback);f.fail_callback=true;bool rejected=false;try{f.release(p);}catch(const std::runtime_error&){rejected=true;}check(rejected&&!mission_heap_find_previous(f.b,p-16)&&f.read(p)==0x504d4153,"Callback failure lost unlink or retained payload");check(f.release(p)==0xffffffff,"Repeated free found an unlinked header");
 Arena missing;missing.b.call={};rejected=false;try{mission_heap_initialize(missing.b);}catch(const std::runtime_error&){rejected=true;}check(rejected&&missing.read(Arena::context+0xc)==0,"Missing required backend changed a later field");
}
}
int main(){try{retained_cache();reuse_sequences();errors_and_budget();std::cout<<"Arena retained reuse, real PCM/cache/audio-destroy prefix and required callback checks passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
