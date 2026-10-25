#include "dfre/mission_debrief_format.hpp"
#include "dfre/mission_debrief_text.hpp"
#include "dfre/mission_rtxt.hpp"
#include "dfre/mission_sound_runtime.hpp"
#include "dfre/campaign.hpp"
#include "dfre/profiles.hpp"
#include "dfre/mission_weapon_startup.hpp"
#include "dfre/mission_localization.hpp"
#include "dfre/item_runtime.hpp"
#include "dfre/items.hpp"
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
 constexpr U A=0x1200000,D=0x1300000;std::map<U,std::uint8_t> bytes;
 for(U at:{A,D})for(U i=0;i<512;++i)bytes[at+i]=0xa5;
 const std::pair<U,const char*> formats[]{
 {0x4cae34,"dfcamp%02i.bin"},{0x4cad48,"Win%d"},{0x4cadd0,"Lose%d"},{0x4cad50,"Briefing%d"},
 {0x4cad5c,"Unable to find mission %d debriefing won text"},{0x4cadd8,"Unable to find mission %d debriefing lose text"},
 {0x4cae08,"%s ~ ~ %s"},{0x4cadbc,"%s ~ %s : %d ~ ~ %s"},{0x4cae14,"Unable to find '%s'"},
 {0x4cc67c,"WPN%03i"},{0x4cc570,"%i"},{0x4cc278,"%s %s"},{0x4ccc60,"STR_PWNAME%02i"},{0x4ccc80,"STR_SPWNAME%02i"}};
 for(const auto& f:formats)for(U i=0;i<=std::strlen(f.second);++i)bytes[f.first+i]=std::uint8_t(f.second[i]);
 U reads=0,writes=0,fail=0,start_reads=0;bool mutate=false;MissionDebriefBindings b;
 b.read=[&](U at,U width){check(width==1,"Format read width changed");++reads;const U n=bytes.at(at);if(mutate&&at==A&&++start_reads==2)bytes.at(A+1)=0;return n;};
 b.write=[&](U at,U width,U n){check(width==1,"Format store width changed");if(++writes==fail)throw std::runtime_error("Mapped write failed");bytes.at(at)=std::uint8_t(n);};
 check(mission_debrief_format(b,{D,0x4cad48,0x80000000})==14&&string(b,D)=="Win-2147483648","Signed INT_MIN output/count changed");check(mission_debrief_format(b,{D,0x4cae34,1})==12&&string(b,D)=="dfcamp01.bin","Filename zero padding changed");
 const std::string expected="Win-2147483648";for(U stop=1;stop<=expected.size()+1;++stop){for(U i=0;i<64;++i)bytes.at(D+i)=0xa5;writes=0;fail=stop;bool failed=false;try{mission_debrief_format(b,{D,0x4cad48,0x80000000});}catch(const std::runtime_error&){failed=true;}check(failed&&writes==stop,"Write failure did not stop at the rejected byte");for(U i=0;i<=expected.size();++i)check(bytes.at(D+i)==(i<stop-1?std::uint8_t(expected[i]):0xa5),"Write failure lost the committed prefix");}fail=0;
 text(b,A,"ABC");text(b,A+128,"D");mutate=true;start_reads=0;const auto count=mission_debrief_format(b,{D,0x4cae08,A,A+128});const std::string interior("A\0C ~ ~ D\0",10);check(count==9,"Captured string length changed after inserted NUL");for(U i=0;i<interior.size();++i)check(bytes.at(D+i)==std::uint8_t(interior[i]),"String emission used a snapshot or stopped at a new NUL");mutate=false;
 text(b,A,"STATS");text(b,A+128,"KILLS");check(mission_debrief_format(b,{A+1,0x4cae08,A,A+128})==15&&string(b,A+1)=="SSSSS ~ ~ KILLS","Accepted source/destination overlap changed");
 check(mission_debrief_format(b,{D,0x4cae08,0,0})==5&&string(b,D)==" ~ ~ ","CRT null-string behavior changed");
 const auto reject=[&](const std::vector<U>& args){writes=0;bool failed=false;try{mission_debrief_format(b,args);}catch(const std::runtime_error&){failed=true;}check(failed&&writes==0,"Format guard wrote output");};
 reject({D,0x4cad48});reject({D,0x12345678,1});bytes.at(0x4cad49)='x';reject({D,0x4cad48,1});bytes.at(0x4cad49)='i';reject({0x4cad48,0x4cad48,1});

 for(const auto& f:formats){
  reject({f.first,0x4cad48,1});reject({f.first+U(std::strlen(f.second)),0x4cad48,1});
  std::vector<U> args{D,f.first,1};if(f.first==0x4cae08||f.first==0x4cc278)args.push_back(A+128);if(f.first==0x4cadbc){args.push_back(A+128);args.push_back(1);args.push_back(A+256);}
  const auto first=bytes.at(f.first);bytes.at(f.first)^=1;reject(args);bytes.at(f.first)=first;
  const U end=f.first+U(std::strlen(f.second));bytes.at(end)=1;reject(args);bytes.at(end)=0;
 }
 for(U value:{0u,1u,9u,10u,99u,100u,0x7fffffffu,0x80000000u,0xffffffffu}){
  const auto digits=number(value);const auto padded=[&](U width){const bool negative=digits.front()=='-';auto magnitude=digits.substr(negative?1:0);while(magnitude.size()+U(negative)<width)magnitude="0"+magnitude;return std::string(negative?"-":"")+magnitude;};
  for(const auto& f:std::vector<std::pair<U,std::string>>{{0x4cc67c,"WPN"+padded(3)},{0x4cc570,digits},{0x4ccc60,"STR_PWNAME"+padded(2)},{0x4ccc80,"STR_SPWNAME"+padded(2)}}){check(mission_debrief_format(b,{D,f.first,value})==f.second.size()&&string(b,D)==f.second,"Startup/Ready signed padding output changed");}
 }
 text(b,A,"Ready");text(b,A+128,"Player");check(mission_debrief_format(b,{D,0x4cc278,A,A+128})==12&&string(b,D)=="Ready Player","Ready current two-string output changed");
 auto missing=b;missing.write={};bool failed=false;try{mission_debrief_format(missing,{D,0x4cad48,1});}catch(const std::runtime_error&){failed=true;}check(failed,"Missing output provider silently succeeded");
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
  if(site==0x4c4f40)return 0; // Declared required audio fixture; no device output.
  if(site==0x40bfd0){check(string(b,args.at(0))=="DebfMenu","Entry menu changed");return 0x77110000;}if(site==0x40d040)return 0x77220000;
  if(site==0x409180){check(args==std::vector<U>{P},"Selected profile changed");saved=true;return 0;}if(site==0x413460){descriptor=args;return 0;}
  if(site==0x407850||site==0x40d550||site==0x40bca0||site==0x40e4d0||site==0x413440||site==0x422ef0||site==0x4230a0||site==0x40dd80)return 0; // Declared menu/display fixtures.
  return raw->call(site,args);
 };
 const auto alloc=[&](U n){return raw->call(0x497f60,{n,4,0,0});};const U query=alloc(1024),destination=alloc(0x2710);text_frame=alloc(0x128);
 const auto load=[&](const std::string& name){text(b,query,name);return raw->call(0x499f1f,{query});};const auto lookup=[&](U table,const std::string& group,const std::string& key){text(b,query+256,group);text(b,query+512,key);const auto entry=mission_rtxt_lookup(*raw,table,query+256,query+512);check(entry!=0,"Supplied text missing");return string(b,raw->read(entry,4));};

 const U game_table=load("DFGAME.BIN");b.write(0x503230,4,game_table);b.write(0x67dbc9,1,0);
 const auto definitions=build_runtime_items(load_item_catalog(assets));MissionLocalizationBindings localization;
 localization.read=b.read;localization.lookup_49a092=[&](U table,U group,U key){return mission_rtxt_lookup(*raw,table,group,key);};
 U startup_names=0,startup_formats=0;std::vector<std::string> names;
 MissionClientAddedBindings startup;startup.read=b.read;startup.write=b.write;
 startup.call=[&](U site,const std::vector<U>& args)->U{
  if(site==0x493a80){++startup_formats;return mission_debrief_format(b,args);}
  if(site==0x404cf0){++startup_names;const auto result=mission_localized_text(localization,args[0],args[1]);names.push_back(string(b,result));return result;}
  if(site==0x4828c0)return U(runtime_item_index(definitions,args[0]));
  throw std::runtime_error("Startup required service is not bound");
 };
 const U startup_frame=alloc(32);mission_weapon_startup(startup,startup_frame,32);
 check(startup_formats==78&&startup_names==78&&b.read(0xad3edc,4)==39,"Actual startup row/name count changed");
 U named=0;for(U i=0;i<39;++i){const U row=0x4f9934+i*0xa8;check(string(b,row+4)==names.at(i*2)&&string(b,row+0x44)==names.at(i*2+1),"Actual startup localized name differs");named+=!names.at(i*2).empty();}check(named>0,"Actual startup has no localized primary names");
 std::cout<<"actual startup39rows78names through closed formatter/RTXT\n";
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
 std::cout<<"actual closed format calls "<<formatted<<" texts "<<cases<<" terminal ADD "<<adds<<" device output0\n";
}
}
int main(int argc,char** argv){try{guarded_checks();if(argc>1)supplied(argv[1]);std::cout<<"Closed Debrief format/current bytes/guard checks passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
