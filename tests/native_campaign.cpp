#include "dfre/campaign.hpp"
#include <iostream>
#include <stdexcept>
namespace {
void require(bool yes,const char* message){if(!yes)throw std::runtime_error(message);}
void put(dfre::AssetBytes& b,std::size_t at,std::uint32_t v){for(unsigned i=0;i<4;++i)b.at(at+i)=std::uint8_t(v>>(i*8));}
std::uint32_t word(const dfre::AssetBytes& b,std::size_t at){return b[at]|(std::uint32_t(b[at+1])<<8)|(std::uint32_t(b[at+2])<<16)|(std::uint32_t(b[at+3])<<24);}
}
int main(){try{
 dfre::CampaignDefinition c;c.index=1;c.name="Campaign";c.text_file="dfcamp02.bin";c.missions={{"C02M01.BMS","One","",0x1011e1e,0,20,30},{"C02M02.BMS","Two","",0x1011e1e,3,40,50}};
 auto other=c;other.index=2;other.missions[0].equipment_flags=0;
 dfre::CampaignCatalog catalog({c,other});dfre::ProfileDefaults defaults;defaults.controls.resize(552);defaults.empty_name="Empty";
 auto profile=dfre::PlayerProfile::empty(defaults,"player01.ply");profile.set_name("Soldier");profile.set_active(true);auto raw=profile.encoded();raw[0x300]=91;put(raw,0xb4,1);profile=dfre::PlayerProfile::decode(raw,defaults);
 dfre::CampaignState state(catalog,profile);require(state.campaign()==1&&state.mission()==0,"First campaign selection");require(state.available(0)&&!state.available(1),"All prerequisite bits required");require(!state.select_mission(1)&&state.mission()==0,"Locked mission cannot be selected");
 raw=profile.encoded();put(raw,0xb4,3);put(raw,0x74+4,9);profile=dfre::PlayerProfile::decode(raw,defaults);require(state.available(1)&&state.completed(1),"Unlock and completion fields");require(state.select_mission(1),"Unlocked mission selection");
 const auto selected=state.accept();require(selected.filename=="C02M02.BMS"&&selected.text_file=="dfcamp02.bin","Mission files");require(word(profile.raw(),0x684)==9&&word(profile.raw(),0x688)==1&&word(profile.raw(),0x680)==5,"Default equipment restriction path");require(profile.raw()[0x300]==91,"Unknown profile fields preserved");
 state.select_campaign(2);state.accept();require(word(profile.raw(),0x698)==3&&word(profile.raw(),0x69c)==3&&word(profile.raw(),0x694)==1,"Last clear restriction bit wins");require(word(profile.raw(),0x684)==15&&word(profile.raw(),0x688)==20&&word(profile.raw(),0x680)==5,"Equipment ID lookup tables");state.select_campaign(1);require(state.mission()==1,"Campaign remembers selected mission");state.enter();require(state.campaign()==1&&state.mission()==1,"Re-entry retains campaign and mission");
 bool refused=false;try{state.select_campaign(0);}catch(const dfre::AssetError&){refused=true;}require(refused,"Invalid campaign rejected");
 const std::unordered_map<std::string,std::string> fragments{{"tail","two {ADD}leaf"},{"leaf","three"},{"yes","passed"},{"no","new"}};
 const auto lookup=[&](std::string_view key){const auto found=fragments.find(std::string(key));return found==fragments.end()?std::string{}:found->second;};
 require(dfre::expand_briefing("one {ADD}tail",lookup,0)=="one two three","Recursive ADD expansion");
 require(dfre::expand_briefing("{CV:0}yes {CV:!0}no end",lookup,1)=="passed end","Positive and negated completion conditions");
 require(dfre::expand_briefing("{CV:0}yes {CV:!0}no end",lookup,0)=="new end","Condition whitespace at adjacent directives");
 const auto linked=dfre::parse_briefing("go {M1L2}point ~ {M1L3}next");require(linked.plain=="go point ~ next"&&linked.links.size()==2&&linked.links[0].word==1&&linked.links[1].word==3,"Original link word indices");
 std::cout<<"campaign checks passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
