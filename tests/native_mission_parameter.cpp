#include "dfre/mission_debrief_format.hpp"
#include "dfre/mission_parameter.hpp"
#include "dfre/mission_debrief_text.hpp"
#include "dfre/mission_rtxt.hpp"
#include "dfre/mission_sound_runtime.hpp"
#include "dfre/campaign.hpp"
#include "dfre/profiles.hpp"
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <stdexcept>
using namespace dfre;using U=std::uint32_t;
namespace {
void check(bool v,const char* text){if(!v)throw std::runtime_error(text);}
std::string number(U n){std::int32_t s;std::memcpy(&s,&n,4);return std::to_string(s);}
std::string string(const MissionDebriefBindings& b,U at){std::string out;for(U i=0;i<65536;++i){auto c=b.read(at+i,1);if(!c)return out;out+=char(c);}throw std::runtime_error("Test string exceeds mapped extent");}
void text(const MissionDebriefBindings& b,U at,const std::string& value){for(U i=0;i<=value.size();++i)b.write(at+i,1,i<value.size()?std::uint8_t(value[i]):0);}
void word(AssetBytes& bytes,U at,U n){for(U i=0;i<4;++i)bytes.at(at+i)=std::uint8_t(n>>(8*i));}
void guarded_checks(){
 std::map<U,U> cells{{0x4b691c,0},{0x4b69f0,73},{0x1100000,9},{0x1100008,11}};MissionDebriefBindings b;std::vector<U> reads,writes;
 b.read=[&](U at,U width){check(width==4,"Parameter width changed");reads.push_back(at);const U value=cells.at(at);if(at==0x4b691c)cells.at(at)=0;return value;};
 b.write=[&](U at,U width,U value){check(width==4,"Parameter width changed");writes.push_back(at);cells.at(at)=value;if(at==0x4b69f0)cells.at(at)^=0x55;};
 check(mission_parameter_set(b,0xffffffff,0x80000000)==0x80000000&&writes.empty(),"Null setter changed value or wrote memory");check(mission_parameter_get(b,7)==73,"Null getter lost retained return");cells.at(0x4b691c)=0x1100000;check(mission_parameter_set(b,2,41)==41&&cells.at(0x1100008)==41,"Setter did not use captured current pointer");cells.at(0x4b691c)=0x1100000;check(mission_parameter_get(b,2)==(41^0x55)&&reads.back()==0x4b69f0,"Getter did not reread current shared return");
 cells.at(0x4b691c)=0x1100004;mission_parameter_set(b,0xffffffff,17);check(cells.at(0x1100000)==17,"Unsigned index wrap changed");
 cells.at(0x4b691c)=0x1100000;cells.at(0x4b69f0)=81;b.write=[&](U,U,U){throw std::runtime_error("Rejected store");};bool failed=false;try{mission_parameter_get(b,0);}catch(const std::runtime_error&){failed=true;}check(failed&&cells.at(0x4b69f0)==81,"Rejected getter store lost prefix");
 MissionDebriefBindings missing;failed=false;try{mission_parameter_set(missing,0,2);}catch(const std::runtime_error&){failed=true;}check(failed,"Missing current pointer provider succeeded");
}
void supplied(const std::filesystem::path& game){
 AssetStore assets(game);std::ifstream file(game/"Df.exe",std::ios::binary);AssetBytes executable((std::istreambuf_iterator<char>(file)),{});Fixed3 listener{};U yaw=0;MissionAudioFrameCells audio{0,listener,yaw};const TerrainGeometry* terrain=nullptr;const MissionSoundAssetBindings* raw=nullptr;
 MissionSoundRuntimeBindings host;host.service=[&](U site,const std::vector<U>& args)->U{if(site==0x499f1f)return mission_rtxt_load(*raw,args.at(0));if(site==0x49a092)return mission_rtxt_lookup(*raw,args.at(0),args.at(1),args.at(2));throw std::runtime_error("Required asset backend is unbound");};MissionSoundRuntime sound(assets,executable,audio,terrain,host);raw=&sound.asset_bindings();
 PlayerProfile profile;constexpr U P=0x1e000000;U text_frame=0,formatted=0;bool saved=false;std::vector<U> descriptor;MissionDebriefBindings b;
 b.read=[&](U at,U width){if(at>=P&&std::uint64_t(at)+width<=P+profile.raw().size()){U n=0;for(U i=0;i<width;++i)n|=U(profile.raw().at(at-P+i))<<(8*i);return n;}return raw->read(at,width);};
 b.write=[&](U at,U width,U n){if(at>=P&&std::uint64_t(at)+width<=P+profile.raw().size()){for(U i=0;i<width;++i)profile.source_bytes().at(at-P+i)=std::uint8_t(n>>(8*i));return;}raw->write(at,width,n);};
 b.call=[&](U site,const std::vector<U>& args)->U{
  if(site==0x493a80){++formatted;return mission_debrief_format(b,args);}
  if(site==0x49a092)return mission_rtxt_lookup(*raw,args.at(0),args.at(1),args.at(2));
  if(site==0x41ef40){mission_debrief_campaign_audio(b);return 0;}if(site==0x41f320){mission_debrief_won_audio(b);return 0;}if(site==0x41ef80){mission_debrief_text(b,text_frame);return 0;}
  if(site==0x4c4f40){const auto value=mission_parameter_set(b,args.at(0),args.at(1));check(b.read(b.read(0x4b691c,4)+args.at(0)*4,4)==args.at(1),"Current Debrief parameter store differs");return value;}
  if(site==0x40bfd0){check(string(b,args.at(0))=="DebfMenu","Entry menu changed");return 0x77110000;}if(site==0x40d040)return 0x77220000;
  if(site==0x409180){check(args==std::vector<U>{P},"Selected profile changed");saved=true;return 0;}if(site==0x413460){descriptor=args;return 0;}
  if(site==0x407850||site==0x40d550||site==0x40bca0||site==0x40e4d0||site==0x413440||site==0x422ef0||site==0x4230a0||site==0x40dd80)return 0; // Declared menu/display fixtures.
  return raw->call(site,args);
 };
 const auto alloc=[&](U n){return raw->call(0x497f60,{n,4,0,0});};const U query=alloc(1024),destination=alloc(0x2710);text_frame=alloc(0x128);const U parameters=alloc(32);b.write(0x4b691c,4,parameters);b.write(0x4b69f0,4,0); // Declared4B6B50 table-producer boundary.
 const auto load=[&](const std::string& name){text(b,query,name);return raw->call(0x499f1f,{query});};const auto lookup=[&](U table,const std::string& group,const std::string& key){text(b,query+256,group);text(b,query+512,key);const auto entry=mission_rtxt_lookup(*raw,table,query+256,query+512);check(entry!=0,"Supplied text missing");return string(b,raw->read(entry,4));};
 const U menu=load("dfmenu.bin");b.write(0x503234,4,menu);const auto stats=lookup(menu,"GroupMainMenu","DEBFMISSIONSTATS"),kills_label=lookup(menu,"GroupMainMenu","DEBFKILLS");b.write(0x4cf2e4,4,P);b.write(0x68bef0,4,destination);
 unsigned cases=0,adds=0;CampaignCatalog catalog(assets);for(const auto& campaign:catalog.campaigns()){
  const U table=load(campaign.text_file);b.write(0x503238,4,table);word(profile.source_bytes(),0x28,campaign.index);b.write(0x4d0d08+campaign.index*4,4,U(campaign.missions.size()));
  for(U mission=0;mission<campaign.missions.size();++mission)for(bool won:{false,true}){
   for(U i=0;i<campaign.missions.size();++i)word(profile.source_bytes(),0x30+campaign.index*0x44+i*4,1);word(profile.source_bytes(),0x30+campaign.index*0x44+mission*4,won?1:0);b.write(0x68ba34,4,mission);b.write(0x67f718,4,won?1:0);mission_debrief_campaign_audio(b);mission_debrief_won_audio(b);
   const U values[]{0,7,0xffffffff,0x80000000};const U kills=values[mission%4],row=mission%2;b.write(0x4e7550,4,row);b.write(0x4e756c+row*0x24,4,kills);raw->call(0x49a620,{destination,0,0x2710});const auto group="Briefing"+std::to_string(mission+1),key=std::string(won?"Win":"Lose")+std::to_string(mission+1);const auto payload=lookup(table,group,key);const auto header=won?stats+" ~ "+kills_label+" : "+number(kills)+" ~ ~ "+payload:stats+" ~ ~ "+payload;std::string expected;
   for(std::size_t at=0;at<header.size();){if(header.compare(at,5,"{ADD}")==0){const auto start=at+5,stop=header.find(' ',start);expected+=lookup(table,group,header.substr(start,stop==std::string::npos?header.size()-start:stop-start));at=stop==std::string::npos?header.size():stop+1;++adds;}else expected+=header[at++];}
   mission_debrief_text(b,text_frame);check(string(b,destination)==expected,"Real format/RTXT/text chain bytes differ");++cases;
  }
 }
 check(cases==80&&adds==18,"Actual campaign corpus changed");word(profile.source_bytes(),0x28,1);b.write(0x68ba34,4,0);for(U i=0;i<0x514;++i)b.write(0x67f714+i,1,0);b.write(0x67f718,4,1);b.write(0x67f73c,4,7);mission_debrief_enter(b,alloc(0x64));const auto entered=b.read(0x68bef0,4);check(saved&&descriptor.size()==10&&descriptor[0]==entered&&!string(b,entered).empty()&&b.read(0x4e756c,4)==7,"Real formatter did not join frozen entry/result/text bodies");const auto table_after=b.read(0x503238,4);mission_debrief_leave(b);check(b.read(0x68bef0,4)==0&&!lookup(table_after,"Briefing1","Win1").empty(),"Leave lost current campaign RTXT");
 check(b.read(parameters+8,4)==0,"Won Debrief did not publish final parameter zero");std::cout<<"actual parameter/Text/Format calls "<<formatted<<" texts "<<cases<<" terminal ADD "<<adds<<" device output0\n";
}
}
int main(int argc,char** argv){try{guarded_checks();if(argc>1)supplied(argv[1]);std::cout<<"Parameter current memory and Debrief/Text/Format checks passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
