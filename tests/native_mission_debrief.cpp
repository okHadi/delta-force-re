#include "dfre/mission_debrief.hpp"
#include "dfre/mission_results.hpp"
#include "dfre/menu.hpp"
#include "dfre/state_loop.hpp"
#include <algorithm>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
using namespace dfre;using U=std::uint32_t;
namespace {
void check(bool n,const char* text){if(!n)throw std::runtime_error(text);}
struct Fixture {
 static constexpr U profile=0x1100000,frame=0x1300000,text=0x1320000;
 std::map<U,std::uint8_t> memory;std::vector<U> calls,frees;MissionDebriefBindings b;
 U request=0,saved_profile=0,fail=0,indirect=0,formatted_campaign=0,final_text=0;bool null_alloc=false,mutate=false,copy_mutation=false;
 std::function<void()> poll,on_free;MenuRuntime* menu=nullptr;std::function<void(U)> requested;
 Fixture(){
  const auto reserve=[&](U at,U n){for(U i=0;i<n;++i)memory[at+i]=0;};reserve(0x67f714,0x514);reserve(0x4e7544,0x514);reserve(profile,0x200);reserve(frame,0x64);reserve(text,0x2710);
  for(U at:{0x68bef4u,0x503238u,0x4cf2e4u,0x68bef0u,0x4e6d00u,0x4e6d04u,0x68ba34u,0x4e9bf0u,0x4e9bdcu,0x4e9c1cu,0x4e9c18u,0xcc08ccu})reserve(at,4);
  put(0x4cf2e4,profile);put(profile+0x28,1);put(profile+0x128,4);put(0x503238,0x77770000);put(0x4e9bf0,1);put(0x4e9bdc,1);put(0x4e9c1c,0x4563a0);put(0x4e9c18,0x4563b0);put(0x68ba34,3);
  for(U i=0;i<0x514;++i)memory.at(0x67f714+i)=std::uint8_t(i*17);
  b.read=[this](U at,U width){const auto n=get(at,width);if(copy_mutation&&at==0x67f724){put(0x67f728,0x12345678);put(0x4e7544,0xabcdef01);}return n;};
  b.write=[this](U at,U width,U n){put(at,n,width);};
  b.call=[this](U site,const std::vector<U>& args)->U{
   calls.push_back(site);if(site==fail)throw std::runtime_error("Required debrief service is unbound");
   switch(site){
   case 0x40bfd0:check(args==std::vector<U>{0x4cae28},"Debrief menu name pointer changed");if(menu)menu->select_menu("DebfMenu");return 0x77110000;
   case 0x40bca0:check(args==std::vector<U>{0x77110000},"Captured menu handle changed");return 0;
   case 0x40e4d0:if(mutate)put(0x67f714,0x55aa1234);return 0;
   case 0x4980d0:frees.push_back(args.at(0));if(on_free)on_free();return 0;
   case 0x493a80:check(args.at(0)==frame&&args.at(1)==0x4cae34,"Debrief filename frame changed");formatted_campaign=args.at(2);return 0;
   case 0x499f1f:check(args==std::vector<U>{frame},"RTXT filename pointer changed");check(get(0x503238)==0,"Old campaign RTXT was not cleared before load");return 0x77770004;
   case 0x497f60:check(args==std::vector<U>({0x2710,4,0,0x4cae44}),"Debrief allocation changed");return null_alloc?0:text;
   case 0x49a620:check(args==std::vector<U>({text,0,0x2710}),"Text clear lost captured allocation");for(U i=0;i<args.at(2);++i)put(args.at(0)+i,args.at(1),1);return text;
   case 0x41f320:if(mutate)put(0x4cf2e4,profile+0x100);return 0;
   case 0x409180:saved_profile=args.at(0);return 0;
   case 0x40d040:check(args==std::vector<U>{0x4cae78},"Debrief font changed");if(mutate)put(0x68bef0,text+32);return 0x77880000;
   case 0x413460:final_text=args.at(0);check(args==std::vector<U>({final_text,86,104,468,272,0x77880000,0,0xff00ff00,0x4e6d08,0}),"Source text bounds changed");if(mutate){put(0x4cf2e4,profile);put(profile+0x28,5);put(0x68ba34,7);}return 0;
   case 0x40d9e0:if(poll)poll();return 0;
   case 0x4563a0:if(indirect==1)put(0xcc08cc,1);return 0;
   case 0x4563b0:return 0;
   case 0x461350:return request;
   case 0x461340:request=args.at(0);if(requested)requested(request);return 0;
   case 0x40e2a0:if(menu)menu->set_button_state(args.at(0)==0x100?"but_debf_actxt":"but_debf_qutxt",int(args.at(1)));return 0;
   case 0x40fa90:if(menu)check(menu->render().size()==640*480,"Existing debrief assets did not render");return 0;
   case 0x407850:case 0x40d550:case 0x413440:case 0x41ef40:case 0x41ef80:case 0x422ef0:case 0x4230a0:case 0x40dd80:case 0x4564b0:case 0x4132f0:case 0x40d820:case 0x4133c0:case 0x4230e0:case 0x496c50:case 0x40b9a0:return 0;
   default:throw std::runtime_error("Unexpected debrief service");
   }
  };
 }
 U get(U at,U width=4)const{U n=0;for(U i=0;i<width;++i)n|=U(memory.at(at+i))<<(8*i);return n;}
 void put(U at,U n,U width=4){for(U i=0;i<width;++i)memory.at(at+i)=std::uint8_t(n>>(8*i));}
};
void body_checks(){
 Fixture f;f.mutate=true;f.copy_mutation=true;mission_debrief_enter(f.b,Fixture::frame);
 check(f.get(0x4e7544)==0xabcdef01&&f.get(0x4e7558)==0x12345678,"Result copy froze source bytes or overwrote an observed alias");
 check(f.formatted_campaign==2&&f.saved_profile==Fixture::profile+0x100&&f.get(0x4e6d00)==5&&f.get(0x4e6d04)==7&&f.final_text==Fixture::text+32,"Entry cached current profile/text fields too early");
 check(f.frees==std::vector<U>{0x77770000}&&f.get(0x503238)==0x77770004,"Campaign RTXT replacement changed");
 f.on_free=[&]{f.put(0x68bef0,Fixture::text+64);};mission_debrief_leave(f.b);check(f.frees.back()==Fixture::text+32&&f.get(0x68bef0)==0&&f.get(0x503238)==0x77770004,"Leave lost captured text or campaign ownership");
 Fixture null;null.null_alloc=true;mission_debrief_enter(null.b,Fixture::frame);check(null.calls.back()==0x4564b0&&null.saved_profile==0&&null.get(0x68bef0)==0,"Null allocation ran later effects");
 Fixture stopped;stopped.fail=0x41ef80;bool failed=false;try{mission_debrief_enter(stopped.b,Fixture::frame);}catch(const std::runtime_error&){failed=true;}check(failed&&stopped.saved_profile==Fixture::profile&&stopped.get(0x68bef0)==Fixture::text&&stopped.get(0x503238)==0x77770004&&stopped.get(0x4e6d00)==0,"Missing text service lost its committed entry prefix");
 Fixture update;update.indirect=1;check(mission_debrief_update(update.b)==1&&update.request==0&&update.calls.back()==0x461340,"Update did not read shutdown after its callback");
 Fixture draw;draw.put(0x68bef4,1);draw.put(0x4e9bdc,0);mission_debrief_render(draw.b);check(draw.calls==std::vector<U>({0x4563b0,0x496c50}),"Alternate draw gate changed");
 auto missing=f.b;missing.call={};failed=false;try{mission_debrief_leave(missing);}catch(const std::runtime_error&){failed=true;}check(failed,"Missing debrief service silently succeeded");
 MissionDebriefBindings empty;mission_debrief_accept(empty,1,0xffffffff);mission_debrief_quit(empty,1,0);failed=false;try{mission_debrief_accept(empty,1,2);}catch(const std::runtime_error&){failed=true;}check(failed,"Reached button provider silently succeeded");
}
void state_flow(MenuRuntime* menu,int key){
 Fixture f;f.menu=menu;StateLoop loop;GameState debrief,next;std::vector<U> order;U accepted=0;bool sent=false;
 f.requested=[&](U state){check(state==(key==88?0x4e6784u:0x4d0cd4u),"Button selected a different source state");accepted=state;loop.request(&next);};
 f.poll=[&]{if(sent)return;sent=true;if(menu){for(const auto& a:menu->key(key)){if(a.handler=="mnu_debf_accept")mission_debrief_accept(f.b,0x100,U(a.action));else if(a.handler=="mnu_mm_exit")mission_debrief_quit(f.b,0x104,U(a.action));else throw std::runtime_error("Unowned DebfMenu callback");}}else mission_debrief_accept(f.b,0x100,8);};
 debrief.enter=[&]{order.push_back(1);mission_debrief_enter(f.b,Fixture::frame);};debrief.update=[&]{return int(mission_debrief_update(f.b));};debrief.render=[&]{order.push_back(99);mission_debrief_render(f.b);};debrief.leave=[&]{order.push_back(2);mission_debrief_leave(f.b);};next.enter=[&]{order.push_back(3);};next.update=[] {return 1;};U clock=0;loop.run(&debrief,{[&]{return clock+=16;},[]{}});
 check(accepted&&order==std::vector<U>({1,2,3})&&f.get(0x68bef0)==0,"StateLoop did not leave before the requested state or drew after transition");check(std::count(f.calls.begin(),f.calls.end(),0x40b9a0)==(key==88?1:0),"Quit service ran more than once");
}
}
int main(int argc,char** argv){try{body_checks();state_flow(nullptr,13);if(argc>1){AssetStore assets(argv[1]);MenuRuntime menu(assets);for(int key:{13,27,88})state_flow(&menu,key);}std::cout<<"Debrief source state/button/current owner checks passed"<<(argc>1?" with supplied menu assets":"")<<'\n';return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
