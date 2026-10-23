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
U word(const AssetBytes& bytes,U at){U n=0;for(U i=0;i<4;++i)n|=U(bytes.at(at+i))<<(8*i);return n;}
void put(AssetBytes& bytes,U at,U n){for(U i=0;i<4;++i)bytes.at(at+i)=std::uint8_t(n>>(8*i));}
std::string number(U n){std::int32_t s;std::memcpy(&s,&n,4);return std::to_string(s);}
std::string string(const MissionDebriefBindings& b,U at){std::string out;for(U i=0;i<65536;++i){const auto c=b.read(at+i,1);if(!c)return out;out+=char(c);}throw std::runtime_error("Fixture string exceeds its mapped extent");}
// This closed format set is a declared foreign-service fixture, not a CRT port.
U format_fixture(const MissionDebriefBindings& b,const std::vector<U>& args){
 const auto fmt=args.at(1);std::string out;const auto numeric=[&](const char* prefix,const char* suffix=""){check(args.size()==3,"Numeric fixture argument count");return std::string(prefix)+number(args.at(2))+suffix;};
 switch(fmt){
 case 0x4cae34:{check(args.size()==3,"Entry filename argument count");auto n=number(args.at(2));if(n.size()<2)n="0"+n;out="dfcamp"+n+".bin";break;}
 case 0x4cad48:out=numeric("Win");break;case 0x4cadd0:out=numeric("Lose");break;case 0x4cad50:out=numeric("Briefing");break;
 case 0x4cad5c:out=numeric("Unable to find mission "," debriefing won text");break;case 0x4cadd8:out=numeric("Unable to find mission "," debriefing lose text");break;
 case 0x4cae08:check(args.size()==4,"Lose header argument count");out=string(b,args.at(2))+" ~ ~ "+string(b,args.at(3));break;
 case 0x4cadbc:check(args.size()==6,"Won header argument count");out=string(b,args.at(2))+" ~ "+string(b,args.at(3))+" : "+number(args.at(4))+" ~ ~ "+string(b,args.at(5));break;
 case 0x4cae14:check(args.size()==3,"Missing reference argument count");out="Unable to find '"+string(b,args.at(2))+"'";break;
 default:throw std::runtime_error("Required format fixture is unbound");
 }
 for(U i=0;i<=out.size();++i)b.write(args.at(0)+i,1,i<out.size()?std::uint8_t(out[i]):0);return U(out.size());
}
void current_audio(){
 constexpr U P=0x1e000000,Q=0x1e001000;PlayerProfile first,second;put(first.source_bytes(),0x28,1);put(first.source_bytes(),0x74,1);put(second.source_bytes(),0x28,2);put(second.source_bytes(),0xbc,1);
 std::map<U,U> cells{{0x4cf2e4,P},{0x67f718,0},{0x68ba34,0},{0x4e6cfc,0},{0x4d0d0c,3},{0x4d0d10,2}};std::vector<std::vector<U>> audio;MissionDebriefBindings b;bool switch_on_count=false;
 b.read=[&](U at,U width){check(width==4,"Audio wrapper changed field width");if(at>=P&&at<P+PlayerProfile::byte_size)return word(first.raw(),at-P);if(at>=Q&&at<Q+PlayerProfile::byte_size)return word(second.raw(),at-Q);const auto n=cells.at(at);if(switch_on_count&&at==0x4d0d0c)cells.at(0x4cf2e4)=Q;return n;};
 b.write=[&](U at,U width,U n){check(width==4,"Won store changed width");cells.at(at)=n;};
 b.call=[&](U site,const std::vector<U>& args){check(site==0x4c4f40,"Unexpected current audio service");audio.push_back(args);if(args==std::vector<U>({2,2})){cells.at(0x4cf2e4)=Q;cells.at(0x68ba34)=1;}return 0u;};
 mission_debrief_won_audio(b);check(audio==std::vector<std::vector<U>>({{2,2},{2,0}})&&cells.at(0x4e6cfc)==1,"Won wrapper cached profile before initial audio or ignored stored completion");
 audio.clear();cells.at(0x4cf2e4)=P;switch_on_count=true;mission_debrief_campaign_audio(b);check(audio==std::vector<std::vector<U>>({{2,3}}),"Campaign wrapper did not use captured first profile/current next profile");
 auto missing=b;missing.call={};cells.at(0x67f718)=1;bool failed=false;try{mission_debrief_won_audio(missing);}catch(const std::runtime_error&){failed=true;}check(failed&&cells.at(0x4e6cfc)==1,"Required audio failure lost its committed won store");
 std::map<U,U> frame_cells{{0x68ba34,0},{0x4e6cfc,1},{0x1300124,0}};std::vector<U> calls;MissionDebriefBindings stopped;stopped.read=[&](U at,U){return frame_cells.at(at);};stopped.write=[&](U at,U,U n){frame_cells.at(at)=n;};stopped.call=[&](U site,const std::vector<U>&){calls.push_back(site);if(site==0x497f60)return 0x1330000u;throw std::runtime_error("Required formatter is unbound");};failed=false;try{mission_debrief_text(stopped,0x1300000);}catch(const std::runtime_error&){failed=true;}check(failed&&calls==std::vector<U>({0x497f60,0x493a80})&&frame_cells.at(0x1300124)==0x1330000,"Missing formatter lost allocation/frame prefix");
}
void supplied(const std::filesystem::path& game){
 AssetStore assets(game);std::ifstream file(game/"Df.exe",std::ios::binary);AssetBytes executable((std::istreambuf_iterator<char>(file)),{});Fixed3 listener{};U yaw=0;MissionAudioFrameCells audio_frame{0,listener,yaw};const TerrainGeometry* terrain=nullptr;const MissionSoundAssetBindings* raw=nullptr;MissionSoundRuntimeBindings host;
 host.service=[&](U site,const std::vector<U>& args)->U{if(site==0x499f1f)return mission_rtxt_load(*raw,args.at(0));if(site==0x49a092)return mission_rtxt_lookup(*raw,args.at(0),args.at(1),args.at(2));throw std::runtime_error("Asset backend service is unbound");};
 MissionSoundRuntime sound(assets,executable,audio_frame,terrain,host);raw=&sound.asset_bindings();PlayerProfile profile;constexpr U P=0x1e000000;std::vector<std::vector<U>> audio_calls;std::vector<U> freed;U fail_site=0,text_frame=0;bool saved_profile=false;std::vector<U> descriptor;MissionDebriefBindings b;
 b.read=[&](U at,U width){if(at>=P&&std::uint64_t(at)+width<=P+profile.raw().size()){U n=0;for(U i=0;i<width;++i)n|=U(profile.raw().at(at-P+i))<<(8*i);return n;}return raw->read(at,width);};
 b.write=[&](U at,U width,U n){if(at>=P&&std::uint64_t(at)+width<=P+profile.raw().size()){for(U i=0;i<width;++i)profile.source_bytes().at(at-P+i)=std::uint8_t(n>>(8*i));return;}raw->write(at,width,n);};
 b.call=[&](U site,const std::vector<U>& args)->U{if(site==fail_site)throw std::runtime_error("Required text service stopped");if(site==0x41ef40){mission_debrief_campaign_audio(b);return 0;}if(site==0x41f320){mission_debrief_won_audio(b);return 0;}if(site==0x41ef80){check(text_frame!=0,"Text frame is not mapped");mission_debrief_text(b,text_frame);return 0;}if(site==0x40bfd0){check(string(b,args.at(0))=="DebfMenu","Entry selected another menu");return 0x77110000;}if(site==0x40d040){check(string(b,args.at(0))=="AVA10B","Entry selected another font");return 0x77220000;}if(site==0x409180){check(args==std::vector<U>{P},"Entry saved another profile");saved_profile=true;return 0;}if(site==0x413460){descriptor=args;return 0;}if(site==0x407850||site==0x40d550||site==0x40bca0||site==0x40e4d0||site==0x413440||site==0x422ef0||site==0x4230a0||site==0x40dd80)return 0;if(site==0x493a80)return format_fixture(b,args);if(site==0x49a092)return mission_rtxt_lookup(*raw,args.at(0),args.at(1),args.at(2));if(site==0x4c4f40){audio_calls.push_back(args);return 0;}if(site==0x4980d0)freed.push_back(args.at(0));return raw->call(site,args);};
 const auto alloc=[&](U size){return raw->call(0x497f60,{size,4,0,0});};const U query=alloc(1024),frame=alloc(0x128),text=alloc(0x2710);text_frame=frame;
 const auto set_text=[&](U at,const std::string& value){for(U i=0;i<=value.size();++i)b.write(at+i,1,i<value.size()?std::uint8_t(value[i]):0);};
 const auto load=[&](const std::string& name){set_text(query,name);return raw->call(0x499f1f,{query});};
 const auto lookup=[&](U table,const std::string& group,const std::string& key){set_text(query+256,group);set_text(query+512,key);const auto entry=mission_rtxt_lookup(*raw,table,query+256,query+512);check(entry!=0,"Actual debrief text is missing");return string(b,raw->read(entry,4));};
 const U general=load("dfmenu.bin");b.write(0x503234,4,general);const auto stats=lookup(general,"GroupMainMenu","DEBFMISSIONSTATS"),kills_label=lookup(general,"GroupMainMenu","DEBFKILLS");b.write(0x4cf2e4,4,P);b.write(0x68bef0,4,text);
 unsigned cases=0,adds=0;CampaignCatalog catalog(assets);
 for(const auto& campaign:catalog.campaigns()){
  const U table=load(campaign.text_file);b.write(0x503238,4,table);put(profile.source_bytes(),0x28,campaign.index);b.write(0x4d0d08+campaign.index*4,4,U(campaign.missions.size()));
  for(U mission=0;mission<campaign.missions.size();++mission)for(bool won:{false,true}){
   for(U i=0;i<campaign.missions.size();++i)put(profile.source_bytes(),0x30+campaign.index*0x44+i*4,1);put(profile.source_bytes(),0x30+campaign.index*0x44+mission*4,won?1:0);
   b.write(0x68ba34,4,mission);b.write(0x67f718,4,won?1:0);audio_calls.clear();mission_debrief_campaign_audio(b);mission_debrief_won_audio(b);check(b.read(0x4e6cfc,4)==U(won),"Real selected profile did not supply won state");check(audio_calls.back()==std::vector<U>({2,won?0u:1u}),"Current audio wrapper result changed");
   const U row=mission%2;const U amounts[]{0,7,0xffffffff,0x80000000};const U kills=amounts[mission%4];b.write(0x4e7550,4,row);b.write(0x4e756c+row*0x24,4,kills);raw->call(0x49a620,{text,0,0x2710});
   const auto group="Briefing"+std::to_string(mission+1);const auto payload=lookup(table,group,std::string(won?"Win":"Lose")+std::to_string(mission+1));std::string header=won?stats+" ~ "+kills_label+" : "+number(kills)+" ~ ~ "+payload:stats+" ~ ~ "+payload;std::string expected;
   for(std::size_t at=0;at<header.size();){if(header.compare(at,5,"{ADD}")==0){const auto begin=at+5;const auto stop=header.find(' ',begin);const auto key=header.substr(begin,stop==std::string::npos?header.size()-begin:stop-begin);expected+=lookup(table,group,key);at=stop==std::string::npos?header.size():stop+1;++adds;}else expected+=header[at++];}
   const auto before=freed.size();mission_debrief_text(b,frame);check(string(b,text)==expected&&freed.size()==before+1&&freed.back()==b.read(frame+0x124,4),"Actual RTXT text bytes or current scratch free changed");++cases;
  }
 }
 check(cases==80&&adds==18,"Supplied debrief corpus changed");
 // A reached lookup error keeps the real allocation and frame pointer until the host handles failure.
 fail_site=0x49a092;bool failed=false;const auto before=freed.size();try{mission_debrief_text(b,frame);}catch(const std::runtime_error&){failed=true;}const auto retained=b.read(frame+0x124,4);check(failed&&retained&&freed.size()==before,"Required RTXT failure freed or lost its prefix allocation");raw->call(0x4980d0,{retained});
 fail_site=0;put(profile.source_bytes(),0x28,1);b.write(0x68ba34,4,0);for(U i=0;i<0x514;++i)b.write(0x67f714+i,1,0);b.write(0x67f718,4,1);b.write(0x67f73c,4,7);
 const auto entry_frame=alloc(0x64);mission_debrief_enter(b,entry_frame);const auto entered_text=b.read(0x68bef0,4);check(saved_profile&&b.read(0x4e6cfc,4)==1&&descriptor==std::vector<U>({entered_text,86,104,468,272,0x77220000,0,0xff00ff00,0x4e6d08,0}),"Frozen entry did not join the real helper/text owners");check(!string(b,entered_text).empty()&&b.read(0x4e756c,4)==7,"Entry lost result-copy statistics");const auto campaign_after=b.read(0x503238,4);mission_debrief_leave(b);check(b.read(0x68bef0,4)==0&&b.read(0x503238,4)==campaign_after&&!lookup(campaign_after,"Briefing1","Win1").empty(),"Entry/leave lost live campaign RTXT ownership");
 std::cout<<"actual debrief texts "<<cases<<" terminal ADD "<<adds<<" device output0\n";
}
}
int main(int argc,char** argv){try{current_audio();if(argc>1)supplied(argv[1]);std::cout<<"Debrief text/current profile/audio checks passed; formatting is an explicit fixture\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
