#include "dfre/mission_leave.hpp"
#include "dfre/mission_results.hpp"
#include "dfre/mission_tick.hpp"
#include "dfre/mission_live.hpp"
#include "dfre/events.hpp"
#include <algorithm>
#include <iostream>
#include <map>
#include <stdexcept>
using namespace dfre;using U=std::uint32_t;using Q=std::uint64_t;
namespace {void check(bool v,const char* text){if(!v)throw std::runtime_error(text);}
struct LiveHost final:EventHost {
 MissionTickState& state;MissionResultImage& result;unsigned actions=0;
 LiveHost(MissionTickState& s,MissionResultImage& r):state(s),result(r){}
 void action_444ca0(U mode) override {check(mode==1&&state.tables.groups[3][0].active,"End event lost its live actor");++actions;result[4]=1;state.tables.groups[3][255].field_18=0x75550000;}
};
void native_live(){
 AiActorTables tables;tables.groups[3].resize(256);for(U i=0;i<256;++i){auto& a=tables.groups[3][i];a.active=i==0;a.definition_index=i==0?1:0;a.field_18=i%31==0?0x71000000+i:0;}
 std::vector<AiItemDefinition> definitions(2);MissionTickState state;init_mission_tick(state,{},std::move(tables),AiItemDefinitionTable(definitions),{},1);
 MissionResultImage result{};const auto put_result=[&](U at,U n){for(U i=0;i<4;++i)result.at(at+i)=std::uint8_t(n>>(8*i));};put_result(0x28,4);put_result(0x2c,7);put_result(0x30,98304);
 LiveHost host(state,result);EventStage events(&host);MissionEvent end;end.words={4,0,0,0,0,1u<<16};events.load_tables({end},{},{{0,8,0,0,0,0,0,0}});events.flags_67dbec()=0x17;
 AssetBytes profile(0xa90);profile[0x28]=1;std::map<U,U> cells{{0x67f528,1},{0x67f520,0},{0x68ba34,2},{0x503134,3},{0x503130,1},{0x67dbe4,0x101},{0x67f548,0x222}};
 const auto cell=[&](U at){if(at==0x67dbec)return events.flags_67dbec();if(at==0x67f718)return U(result[4]);return cells[at];};
 MissionProfileBindings save;save.cell=cell;save.store=[&](U at,U value){cells[at]=value;};save.profile=[&]()->AssetBytes&{return profile;};save.result=[&]{check(host.actions==1&&state.tables.groups[3][0].active,"Profile captured before end events or after teardown");return result;};save.fp_control=[]{return std::uint16_t(0x37f);};save.end_events=[&]{check(cells.at(0x67f548)==0x101,"Source rules restore ran after profile effects");events.run_bit2_events();};
 std::vector<U> calls,releases;unsigned physical_reads=0;bool cleared=false,stop=false;MissionLeaveBindings b;
 b.read=[&](U at,U width)->Q{check(width==4,"Unexpected native leave width");if(at>=0x7cca98&&at<0x7cca80+0x11400){check((at-0x7cca98)%0x114==0,"Physical CHR field is not aligned");++physical_reads;const auto image=snapshot_mission_actor(state,{3,(at-0x7cca98)/0x114});U n=0;for(U i=0;i<4;++i)n|=U(image[0x18+i])<<(8*i);return n;}return cell(at);};
 b.write=[&](U at,U width,Q value){check(width==4&&at==0x67f548,"Unexpected native leave store");cells[at]=U(value);};
 b.call=[&](U site,const std::vector<U>& args)->U{calls.push_back(site);if(site==0x404540){check(args==std::vector<U>{0x67dbf8},"Capture frame changed");return 0;}if(site==0x401e70){mission_profile_leave(save);return 0;}if(site==0x4401e0&&stop)throw std::runtime_error("Required transport cleanup is unbound");if(site==0x42cd30){check(!cleared&&state.tables.groups[3][0].active,"Release ran after actor teardown");releases.push_back(args.at(0));return 0;}if(site==0x449620){check(physical_reads==256&&releases.back()==0x75550000,"Cleanup missed the current physical tail");cleared=true;return 0;}return 0;};
 // Required foreign cleanup can stop after real profile effects. The prefix stays live.
 stop=true;bool failed=false;try{mission_leave(b);}catch(const std::runtime_error&){failed=true;}check(failed&&host.actions==1&&result[4]==1&&calls.back()==0x4401e0&&physical_reads==0&&!cleared,"Cleanup error lost the profile prefix");
 const auto profile_word=[&](U at){U n=0;for(U i=0;i<4;++i)n|=U(profile.at(at+i))<<(8*i);return n;};check(profile_word(0x7c)==1&&profile_word(0xb4)==0x17&&profile_word(0x664)==4&&profile_word(0x678)==7&&profile_word(0x674)==6,"Leave did not save current profile totals");
 // A fresh event pass completes the ordered path with the same current owners.
 host.actions=0;result[4]=0;events.load_tables({end},{},{{0,8,0,0,0,0,0,0}});events.flags_67dbec()=0x17;stop=false;calls.clear();releases.clear();mission_leave(b);
 const std::vector<U> prefix{0x404540,0x47a640,0x47a660,0x403a50,0x43e6d0,0x401e70,0x4401e0,0x402d40,0x47a690,0x486660,0x47ec50};check(std::equal(prefix.begin(),prefix.end(),calls.begin()),"Cleanup call order changed");check(cleared&&physical_reads==256&&host.actions==1&&result[4]==1&&calls[calls.size()-3]==0x42c950&&calls[calls.size()-2]==0x449620&&calls.back()==0x482910,"Final cleanup order changed");
 // Result storage belongs to this caller and remains current after the body.
 result[0x513]=0xa5;const auto final=result;check(final[4]==1&&final.back()==0xa5&&state.tables.groups[3][0].active,"Leave invented a result or actor owner");
 auto missing=b;missing.call={};failed=false;try{mission_leave(missing);}catch(const std::runtime_error&){failed=true;}check(failed,"Missing foreign callback silently succeeded");
}
void qword_prefix(){
 constexpr U P=0x1300000;std::map<U,std::uint8_t> data;for(U i=0;i<16;++i)data[P+i]=std::uint8_t(0xff-i);
 U reads=0,writes=0;MissionLeaveBindings b;b.call=[](U site,const std::vector<U>&){check(site==0x404540,"Dim continued after a failed qword");return 1u;};
 b.read=[&](U at,U width)->Q{if(at==0x67f41c)return 32;if(at==0x67f404)return 1;if(at==0x67f400)return 4;if(at==0x67dbfc)return P;check(width==8,"Dim qword was split");++reads;if(reads==2)throw std::runtime_error("Next surface bytes are unbound");Q n=0;for(U i=0;i<8;++i)n|=Q(data.at(at+i))<<(8*i);return n;};
 b.write=[&](U at,U width,Q n){check(width==8,"Dim qword write was split");++writes;for(U i=0;i<8;++i)data.at(at+i)=std::uint8_t(n>>(8*i));};
 bool failed=false;try{mission_leave(b);}catch(const std::runtime_error&){failed=true;}check(failed&&reads==2&&writes==1&&data.at(P)==127&&data.at(P+7)==124&&data.at(P+8)==247,"Qword error lost its exact committed prefix");
}
}
int main(){try{native_live();qword_prefix();std::cout<<"Mission leave/profile/end-event/current physical actor checks passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
