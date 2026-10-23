#include "dfre/mission_logic.hpp"
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace dfre;
namespace {
void check(bool value){if(!value)throw std::runtime_error("Event reference policy check failed");}
template<class F>void throws(F f){bool failed=false;try{f();}catch(const EventError&){failed=true;}check(failed);}
struct Host : MissionLogicHost {
    EventStage* stage=nullptr;EventReferencePolicy policy=EventReferencePolicy::strict;
    std::vector<std::pair<std::size_t,std::size_t>> diagnostics;std::vector<std::uint32_t> dialogues;
    std::uint32_t lights=3,trans=2,power=1,raw=0x1a5;unsigned external_calls=0;bool use_policy=true;
    std::uint32_t external_event_fired(std::size_t index) override{
        ++external_calls;if(!use_policy)return raw;check(stage!=nullptr);
        return event_fired_reference(*stage,index,policy,[&](auto requested,auto loaded){diagnostics.emplace_back(requested,loaded);});
    }
    std::uint32_t object_index_dword(std::uint32_t address,std::uint32_t index) override{
        check(address==event_object_field_7936ac);if(index==43)return lights;if(index==25)return trans;if(index==53)return power;throw std::runtime_error("Unexpected count input");
    }
    bool world_67f520_is_zero()override{return true;}
    bool world_67f549_bit1()override{return false;}
    void action_43f500(std::uint32_t id)override{dialogues.push_back(id);}
};
MissionData fixture(){MissionData mission;mission.events.resize(74);mission.triggers.resize(125);mission.actions.resize(153);
 mission.events[66].words={0,106,147,0,32768000,66560};
 mission.triggers[106]={1,1,12,43,2,0,0,0};mission.triggers[107]={0,1,12,43,1,0,0,0};mission.triggers[108]={1,2,0,74,0,0,0,0};mission.triggers[109]={0,5,0xffffffffu,0,0,0,0,0};mission.actions[147]={0,7,0,706,0,0,0,0};
 mission.events[14].words={0,16,19,0,0,131584};mission.triggers[16]={0,1,4,25,0,0,0,0};mission.triggers[17]={0,1,4,53,0,0,0,0};mission.actions[19]={0,8,0,0,0,0,0,0};mission.actions[20]={0,5,0,7,0,0,0,0};return mission;
}
void run(MissionData mission){
 check(mission.events.size()==74&&mission.triggers.at(108)==EventTableRecord{1,2,0,74,0,0,0,0});
 MissionLogic logic;logic.begin_mission(mission,true);Host host;host.stage=&logic.events();logic.set_host(&host);auto& stage=logic.events();
 throws([&]{event_fired_reference(stage,74);});throws([&]{event_fired_reference(stage,74,EventReferencePolicy::missing_not_fired);});
 throws([&]{stage.evaluate_trigger(108);});check(host.external_calls==1&&host.diagnostics.empty());
 host.use_policy=false;check(stage.evaluate_trigger(108)==0xa5&&host.external_calls==2);host.raw=0x100;check(stage.evaluate_trigger(108)==0);host.use_policy=true;
 host.policy=EventReferencePolicy::missing_not_fired;check(stage.evaluate_trigger(108)==0&&host.diagnostics.back()==std::make_pair(std::size_t(74),std::size_t(74)));
 const auto calls=host.external_calls;const auto diagnostics=host.diagnostics.size();stage.event(14).words[5]=(stage.event(14).words[5]&0xffffff00u)|0xab;
 check(event_fired_reference(stage,14)==0xab);check(event_fired_reference(stage,14,host.policy)==0xab);check(host.external_calls==calls&&host.diagnostics.size()==diagnostics);
 stage.event(14).words[5]&=0xffffff00u;check(stage.evaluate_event_triggers(66)==0);check(host.diagnostics.size()==diagnostics+1);stage.process_event(66);check(stage.event_fired(66)==0&&host.dialogues.empty());
 host.lights=1;stage.process_event(66);check(stage.event_fired(66)==1&&(stage.event(66).words[4]&0xffff)==500&&host.dialogues.empty());
 for(unsigned i=0;i<3;++i)stage.process_event(66);check(host.dialogues.empty()&&(stage.event(66).words[4]&0xffff)==116);stage.process_event(66);check(host.dialogues==std::vector<std::uint32_t>{706});stage.process_event(66);check(host.dialogues.size()==1);
 const auto before_win=host.diagnostics.size();check(stage.evaluate_event_triggers(14)==0);host.trans=0;host.power=0;check(stage.evaluate_event_triggers(14)==1&&host.diagnostics.size()==before_win);
 stage.event(66)=mission.events[66];host.dialogues.clear();host.lights=1;stage.process_event(66);host.lights=0;for(unsigned i=0;i<4;++i)stage.process_event(66);check(host.dialogues.empty()&&stage.event_fired(66)==0);
 Host replacement;replacement.stage=&stage;replacement.use_policy=false;replacement.raw=0x17f;logic.set_host(&replacement);check(stage.evaluate_trigger(108)==0x7f&&replacement.external_calls==1);logic.set_host(nullptr);throws([&]{stage.evaluate_trigger(108);});
 EventStage raw;raw.load(mission);throws([&]{raw.evaluate_trigger(108);});
}
}
int main(int argc,char** argv){try{run(fixture());if(argc==2){AssetStore assets(argv[1]);run(load_mission(assets,"C05M07.BMS"));std::cout<<"Real C05M07 policy/delegation checks passed\n";}else check(argc==1);std::cout<<"Event reference reconstruction policy checks passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
