#include "dfre/mission_post_menu.hpp"
#include "dfre/state_loop.hpp"
#include <algorithm>
#include <iostream>
#include <map>
#include <set>
using U=std::uint32_t;
namespace {
void check(bool value,const char* text){if(!value)throw std::runtime_error(text);}
struct Fixture {
    std::map<U,U> memory{{0x67dbbc,0},{0x67f520,0},{0x67f528,0},{0x67f534,0},{0x67f53c,0},{0x68d530,0},{0x4e9bf0,0},{0x4e9bdc,0},{0x4e9c18,0x1300200},{0x50322c,0x1300300},{0x1300000,0x1300400}};
    std::vector<std::pair<U,std::vector<U>>> calls;
    std::function<void(U)> effect;
    U request=0xffffffff;
    bool change_store=false,fail_cleanup=false;
    dfre::MissionPostMenuBindings bindings(){
        dfre::MissionPostMenuBindings b;
        b.read=[&](U at,U width){check(width==4,"Current width differs");return memory.at(at);};
        b.write=[&](U at,U width,U value){check(width==4,"Current write width differs");memory.at(at)=change_store?value+4:value;};
        b.call=[&](U site,const std::vector<U>& args)->U{
            static const std::set<U> required{0x407850,0x40d550,0x40bfd0,0x40bca0,0x40e4d0,0x422ef0,0x4230a0,0x40dd80,0x42af00,0x40b960,0x461340,0x4c4f40,0x41e2e0,0x49a092,0x401d00,0x407fd0,0x40fa90,0x1300200,0x4230e0,0x496c50};
            check(required.count(site),"Unexpected foreign fixture");calls.emplace_back(site,args);
            if(site==0x461340)request=args.at(0);
            if(effect)effect(site);
            if(site==0x41e2e0&&fail_cleanup)throw std::runtime_error("Required cleanup error");
            if(site==0x40bfd0)return 0x1300100;
            if(site==0x49a092){check(args==std::vector<U>({memory.at(0x50322c),0x4cb4fc,0x4cb4ec}),"Current RTXT fixture args differ");return 0x1300000;}
            return 0;
        };return b;
    }
    bool called(U site)const{return std::any_of(calls.begin(),calls.end(),[&](const auto& c){return c.first==site;});}
};
void routing(){
    for(U status=0;status<8;++status)for(U mp=0;mp<2;++mp)for(U host=0;host<2;++host)for(U mode=0;mode<4;++mode){
        Fixture f;f.memory[0x67dbbc]=status;f.memory[0x67f520]=mp;f.memory[0x67f528]=host;f.memory[0x67f534]=mode;
        check(dfre::mission_post_menu_update(f.bindings())==1,"Post update must finish");
        const U expected=status==2||status==5?0:status==1?(!mp?0x4e7a58:host?0x4e6608:0x4d6b78):mp&&host?(mode==1||mode==2?0x4e6878:0x4e6608):0x4cf2ec;
        check(f.request==expected,"Post route differs");check(f.called(0x407fd0)==(status!=1&&status!=2&&status!=5&&mp&&host&&mode==2),"Pre Menu preparation differs");
        if(status==5)check(f.calls.at(1)==std::make_pair(U(0x401d00),std::vector<U>({0,0x1300400})),"Error message must use current text");
        if(status==1&&!mp)check(f.calls.at(0)==std::make_pair(U(0x4c4f40),std::vector<U>({0,1})),"Captured SP arguments differ");
    }
    Fixture f;f.memory[0x67dbbc]=1;f.fail_cleanup=true;bool failed=false;try{dfre::mission_post_menu_update(f.bindings());}catch(const std::runtime_error&){failed=true;}check(failed&&f.request==0x4e7a58,"Cleanup failure lost the selected request");
    Fixture captured;captured.memory[0x67dbbc]=1;captured.effect=[&](U at){if(at==0x4c4f40)captured.memory[0x67dbbc]=2;};dfre::mission_post_menu_update(captured.bindings());check(captured.request==0x4e7a58,"Captured status was replaced after callback");
}
void entry_and_render(){
    Fixture f;f.change_store=true;dfre::mission_post_menu_enter(f.bindings());
    check(f.calls.at(3)==std::make_pair(U(0x40bca0),std::vector<U>({0x1300100}))&&f.calls.at(4)==std::make_pair(U(0x40e4d0),std::vector<U>({0x1300104})),"Menu handle capture or reread differs");check(f.called(0x42af00)&&f.calls.back().first==0x40b960,"Source entry tail differs");
    f.calls.clear();f.memory[0x4e9bf0]=1;f.memory[0x4e9bdc]=0;dfre::mission_post_menu_render(f.bindings());check(f.calls.size()==2&&f.calls.front().first==0x1300200&&f.calls.back().first==0x496c50,"Current display callback route differs");
    f.calls.clear();f.memory[0x4e9bf0]=0;f.effect=[&](U at){if(at==0x40fa90)f.memory[0x4e9bf0]=1;};dfre::mission_post_menu_render(f.bindings());check(f.calls.back().first==0x496c50&&!f.called(0x4230e0),"Display gate was cached across draw");
    dfre::mission_post_menu_leave({});bool failed=false;try{dfre::mission_post_menu_update({});}catch(const dfre::MissionPostMenuError&){failed=true;}check(failed,"Missing current reader succeeded");
}
void state_transition(){
    Fixture f;f.memory[0x67dbbc]=1;dfre::StateLoop loop;dfre::GameState post,debrief;std::vector<U> order;U ticks=0;
    f.effect=[&](U site){if(site==0x461340){check(f.request==0x4e7a58,"Unknown destination");loop.request(&debrief);}};
    post.enter=[&]{order.push_back(1);dfre::mission_post_menu_enter(f.bindings());};post.update=[&]{order.push_back(2);return int(dfre::mission_post_menu_update(f.bindings()));};post.render=[] {throw std::runtime_error("Draw after state exit");};post.leave=[&]{order.push_back(3);dfre::mission_post_menu_leave(f.bindings());};
    debrief.enter=[&]{order.push_back(4);check(f.memory.at(0x67dbbc)==1,"Current status lost before next enter");};debrief.update=[&]{order.push_back(5);return 1;};debrief.leave=[&]{order.push_back(6);};
    loop.run(&post,{[&]{ticks+=16;return ticks;},[] {}});check(order==std::vector<U>({1,2,3,4,5,6}),"State leave/enter order differs");
}
}
int main(){try{routing();entry_and_render();state_transition();std::cout<<"Post Menu routing and state order checks passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
