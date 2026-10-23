#include "dfre/mission_hud_message.hpp"
#include "dfre/mission_frame_services.hpp"
#include "dfre/mission_rtxt.hpp"
#include <array>
#include <iostream>
#include <map>
#include <stdexcept>
using namespace dfre;using U=std::uint32_t;
namespace {void check(bool n,const char* e){if(!n)throw std::runtime_error(e);}template<class F>void fails(F f){bool failed=false;try{f();}catch(const std::exception&){failed=true;}check(failed,"Missing HUD owner silently succeeded");}
void run(){
 std::array<std::uint8_t,0x1360> queue{};U counter=99;std::array<U,3> timers{};std::map<U,std::uint8_t> memory;
 MissionHudMessageBindings b;const auto read=[&](U at,U width){U value=0;for(U i=0;i<width;++i){const U address=at+i;std::uint8_t byte;if(address>=0xbd34b0&&address<0xbd4810)byte=queue.at(address-0xbd34b0);else if(address>=0xbd4810&&address<0xbd4814)byte=std::uint8_t(counter>>((address-0xbd4810)*8));else if(address>=0xbd4814&&address<0xbd4820){const U delta=address-0xbd4814;byte=std::uint8_t(timers[delta/4]>>((delta%4)*8));}else byte=memory[address];value|=U(byte)<<(i*8);}return value;};
 const auto write=[&](U at,U width,U value){for(U i=0;i<width;++i){const U address=at+i;const auto byte=std::uint8_t(value>>(i*8));if(address>=0xbd34b0&&address<0xbd4810)queue.at(address-0xbd34b0)=byte;else if(address>=0xbd4810&&address<0xbd4814){const U shift=(address-0xbd4810)*8;counter=(counter&~(255u<<shift))|(U(byte)<<shift);}else if(address>=0xbd4814&&address<0xbd4820){const U delta=address-0xbd4814,shift=(delta%4)*8;timers[delta/4]=(timers[delta/4]&~(255u<<shift))|(U(byte)<<shift);}else memory[address]=byte;}};
 const auto text=[&](U at,const std::string& s){for(U i=0;i<=s.size();++i)write(at+i,1,i<s.size()?std::uint8_t(s[i]):0);};b.read=read;b.write=write;
 constexpr U table=0x1200000,frame=0x1304000;text(0x4c82fc,"ID%03i");text(0x4c8304,"Triggered Text");text(0x4ced1a,"&");text(0x4ced1c,"");write(table,4,0x54585452);write(table+4,4,0x60);write(table+12,4,1);write(table+16,4,0);text(table+32,"source message");write(table+0x60,4,1);write(table+0x64,4,0x30);write(table+0x68,4,1);text(table+0x6c,"Triggered Text");text(table+0x94,"ID007");MissionSoundAssetBindings rtxt;rtxt.read=read;rtxt.write=write;mission_rtxt_fixup(rtxt,table);write(0x503238,4,table);
 unsigned lookups=0;MissionHudMessageCall* current=nullptr;bool mutate=false;
 b.call=[&](U site,const std::vector<U>& args)->U{switch(site){case 0x49a620:for(U i=0;i<args.at(2);++i)write(args.at(0)+i,1,args.at(1));if(mutate&&current){current->metadata=0x12345678;current->text=0;}return args.at(0);case 0x49a092:++lookups;return mission_rtxt_lookup(rtxt,args.at(0),args.at(1),args.at(2));case 0x493a80:return mission_hud_message_format_id(b,args.at(0),args.at(1),args.at(2));case 0x404d80:return mission_hud_triggered_lookup(b,args.at(0),args.at(1));case 0x479f00:{MissionHudMessageCall call{args.at(0),args.at(1)};mission_hud_message(b,call);return 0;}default:throw std::runtime_error("Unbound HUD service");}};
 mission_hud_message_clear(b);check(counter==0&&timers==std::array<U,3>{0xffffffff,0xffffffff,0xffffffff},"HUD clear did not use shared timer cells");mission_hud_triggered_text(b,7,frame);check(lookups==1&&queue[0]=='s'&&read(0xbd3528,4)==0xffffffff&&timers[0]==0x4d8,"Triggered text did not use real RTXT and shared queue");
 U requested=0,stat=0;MissionProfilerCells profiler{0,0,{}};Fixed3 position{};MissionCameraCells camera{position,{},0,0};MissionScriptQueueCells scripts{};MissionFrameServices services({requested,profiler,timers,stat,camera,scripts},{});
 MissionStageCall timer_call{};timer_call.address=0x479ed0;for(unsigned i=0;i<0x4d8;++i)services.stage(timer_call);check(timers[0]==0&&queue[0]=='s',"Timer expiry changed message history");services.stage(timer_call);check(timers[0]==0xffffffff,"Shared timer stopped wrapping");
 text(frame,std::string(200,'L'));MissionHudMessageCall call{frame,99};current=&call;mutate=true;mission_hud_message(b,call);mutate=false;current=nullptr;check(queue[0]=='L'&&queue[118]=='L'&&queue[119]==0&&read(0xbd3528,4)==0x12345678&&queue[0x7c]=='s',"HUD truncation, copied text, or late metadata differs");
 call={0xbd34b0,11};mission_hud_message(b,call);check(queue[0]==0&&queue[0x7c]=='L'&&read(0xbd3528,4)==11,"Aliased text was captured before source clear");
 write(0x67dbc9,1,0x20);check(mission_hud_triggered_lookup(b,0x4c8304,frame)==0x4ced1a,"Triggered suppression sentinel differs");write(0x503238,4,0);check(mission_hud_triggered_lookup(b,0x4c8304,frame)==0x4ced1c,"Missing campaign sentinel differs");
 const auto required=b.call;b.call={};const auto prior_timers=timers;fails([&]{mission_hud_message(b,call);});check(timers==prior_timers,"Missing clear wrote later timers");b.call=required;
 for(U value:{0u,1u,99u,100u,0xffffffffu,0x80000000u,0x7fffffffu}){const U length=mission_hud_message_format_id(b,frame,0x4c82fc,value);check(length>=5&&length<=13&&read(frame+length,1)==0,"Bounded format output differs");}write(0x4c82fc,1,'X');fails([&]{mission_hud_message_format_id(b,frame,0x4c82fc,1);});
}
}
int main(){try{run();std::cout<<"HUD shared queue, timer, RTXT, alias and error checks passed\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
