#include "dfre/mission_sound_runtime.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <iomanip>
namespace {using U=std::uint32_t;
std::string hex(const dfre::MissionSoundAssetBindings& b,U at,U size){std::ostringstream out;for(U i=0;i<size;++i)out<<std::hex<<std::setw(2)<<std::setfill('0')<<b.read(at+i,1);return out.str();}
void text(const dfre::MissionSoundAssetBindings& b,U at,const std::string& s){for(U i=0;i<=s.size();++i)b.write(at+i,1,i<s.size()?std::uint8_t(s[i]):0);}
}
int main(int argc,char** argv){try{
 if(argc!=2)throw std::runtime_error("Use: mission_sound_runtime_probe GAME_FOLDER");
 const std::filesystem::path game(argv[1]);dfre::AssetStore assets(game);std::ifstream source(game/"Df.exe",std::ios::binary);dfre::AssetBytes executable((std::istreambuf_iterator<char>(source)),{});
 dfre::Fixed3 listener{};U yaw=0;dfre::MissionAudioFrameCells frame{0,listener,yaw};const dfre::TerrainGeometry* terrain=nullptr;
 const dfre::MissionSoundAssetBindings* shared=nullptr;U diagnostics=0;std::string mode;U seed;
 dfre::MissionSoundRuntimeBindings bindings;bindings.script_key=[](){return 0x04960552u;};
 bindings.service=[&](U site,const std::vector<U>& args){if(mode!="lazy"||site!=0x499750||args.size()!=2||args[1]!=0x4ce314)throw std::runtime_error("Unexpected sound runtime foreign fixture");++diagnostics;if(seed%8==7)shared->write(0x4f2ea8+3*40,4,0x8f0050);return 0u;};
 dfre::MissionSoundRuntime owner(assets,executable,frame,terrain,bindings);const auto& b=owner.asset_bindings();shared=&b;
 dfre::AssetBytes initial_table(512*40);for(U i=0;i<initial_table.size();++i)initial_table[i]=std::uint8_t(b.read(0x4f2ea4+i,1));
 bool lookup_ready=false;
 while(std::cin>>mode>>seed){
  if(mode!="lookup")lookup_ready=false;
  const U record=0x21000000,start=0x21000100,row=0x8f0000+(seed%8)*40;for(U i=0;i<512;++i)b.write(0x8f0000+i,1,(seed*13+i*7)&255);
  b.write(0x6e06c4,4,0x8f0000);b.write(0x6e06c8,4,seed%8);b.write(0x6e06cc,4,0xaabbccdd);
  if(mode=="line"){
   std::vector<std::string> words={"ambient"+std::to_string(seed%10),"WIND",std::to_string(seed*3),std::to_string(seed*5),std::to_string(seed*7),"-1","257","unused","unused"};
   words.resize(30,"unused");
   if(seed%7==0)words[0]="range";if(seed%7==1)words[0]="version";
   for(U i=0;i<words.size();++i){b.write(record+4+i*4,4,start+i*32);text(b,start+i*32,words[i]);}
   if(seed%5==1){for(U i=0;i<16;++i){const U c=b.read(0x4cc0a0+i,1);b.write(start+i,1,c);if(!c)break;}}
   if(seed%3==0){for(U i=0;i<16;++i){const U c=b.read(0x4cc0a8+i,1);b.write(start+7*32+i,1,c);if(!c)break;}}
   if(seed%3==1){for(U i=0;i<16;++i){const U c=b.read(0x4cc0b0+i,1);b.write(start+8*32+i,1,c);if(!c)break;}}
   b.write(record,4,seed%13);const auto result=b.call(seed%2?0x43ec40:0x43ebf0,{seed%17?record:0});
   std::cout<<"{\"result\":"<<result<<",\"count\":"<<b.read(0x6e06c8,4)<<",\"range\":"<<b.read(0x6e06cc,4)<<",\"row\":\""<<hex(b,row,40)<<"\"}\n";
  }else if(mode=="token"){
   const std::string lines[]={"a b c", "a, b,", "\"a b\",c\t\"d\" ", "", "a\"b\"c", " \t, ", "//x", "x\t\"a,b\"\t"};
   for(U i=0;i<30;++i)b.write(0x8e4430+i*4,4,0x12300000+seed*30+i);
   for(U i=0;i<1004;++i)b.write(0x8e4040+i,1,(seed+i)&255);
   auto line=lines[seed%8];if(seed%11==0)line=std::string(1100,'x');text(b,start,line);b.call(0x4565f0,{start,record});
   std::cout<<"{\"record\":\""<<hex(b,record,124)<<"\",\"buffer\":\""<<hex(b,0x8e4040,1004)<<"\"}\n";
  }else if(mode=="lookup"){
   if(!lookup_ready){for(U i=0;i<initial_table.size();++i)b.write(0x4f2ea4+i,1,initial_table[i]);owner.initialize_definitions("LOOP.DEF");lookup_ready=true;}const auto name_at=0x4f2eda+(seed%511)*40;std::string name;for(U i=0;i<64;++i){const auto c=char(b.read(name_at+i,1));if(!c)break;name+=c;}
   if((seed/511)%3==0)for(auto& c:name)if(c>='A'&&c<='Z')c=char(c+32);if((seed/511)%3==1)for(auto& c:name)if(c>='a'&&c<='z')c=char(c-32);if(seed==1533)name="";if(seed==1534)name+="_MISSING";std::cout<<"{\"id\":"<<owner.definition_id(name)<<"}\n";
  }else if(mode=="resolve"){
   for(U i=0;i<initial_table.size();++i)b.write(0x4f2ea4+i,1,initial_table[i]);text(b,0x8cf96c,"NO_SCRIPT.MIS");owner.initialize_definitions("LOOP.DEF");owner.resolve_definitions("SNIPER.PWF",seed%2?"SNIPER.PWF":"NO_ALT.PWF");
   auto image=initial_table;for(U i=0;i<image.size();++i)image[i]=std::uint8_t(b.read(0x4f2ea4+i,1));
   for(U i=0;i<512;++i){U pointer=b.read(0x4f2ea4+i*40+4,4),normal=pointer;if(pointer){for(U j=0;j<2;++j){const U context=j?0x6e05e0:0x6e0518,base=b.read(context+0x38,4),count=b.read(context+0x28,4);if(pointer>=base&&std::uint64_t(pointer-base)<std::uint64_t(count)*80&&((pointer-base)%80)==0)normal=1+j*10000+(pointer-base)/80;}const auto base=b.read(0x6e06c4,4),count=b.read(0x6e06c8,4);if(pointer>=base&&std::uint64_t(pointer-base)<std::uint64_t(count)*40&&((pointer-base)%40)==0)normal=20001+(pointer-base)/40;}for(U j=0;j<4;++j)image[i*40+4+j]=std::uint8_t(normal>>(8*j));}
   std::ostringstream table;for(auto byte:image)table<<std::hex<<std::setw(2)<<std::setfill('0')<<unsigned(byte);
   std::cout<<"{\"table\":\""<<table.str()<<"\",\"primary\":"<<b.read(0x6e06bc,4)<<",\"alternate\":"<<b.read(0x6e06c0,4)<<",\"ambient\":"<<b.read(0x6e06c8,4)<<"}\n";
  }else if(mode=="stock"){
   owner.initialize_definitions("LOOP.DEF");text(b,0x8cf96c,"C05M07.BMS");owner.resolve_definitions("sniper.pwf","new.pwf");owner.global(499);
   const auto generation=b.read(0x4a2ae0,4);if(generation<=1)throw std::runtime_error("Stock production owner did not start its result voice");for(U i=10;i<16;++i)if(b.read(0x4a1703+i*0x34,1)!=1)throw std::runtime_error("Stock channels were not reserved");
   std::vector<U> handles;for(U i=0;i<16;++i)if(b.read(0x4a1a12+i*0x34+0x30,1))handles.push_back(b.read(0x4a1a12+i*0x34+0x10,4));
   owner.publish();U active=0;for(auto handle:handles)active+=b.call(0x494ff0,{handle})!=0;if(!active)throw std::runtime_error("Stock production voice is not live");
   owner.advance_frames(22050*30);for(auto handle:handles)if(b.call(0x494ff0,{handle}))throw std::runtime_error("Stock production voice did not finish");
   std::cout<<"{\"ambient\":"<<b.read(0x6e06c8,4)<<",\"samples\":"<<b.read(0x6e0520,4)<<",\"banks\":"<<b.read(0x6e0540,4)<<",\"generation\":"<<generation<<",\"active\":"<<active<<",\"device_output\":0}\n";
  }else if(mode=="lazy"){
   const U first=0x8f0000,second=first+40,third=second+40,id=3;diagnostics=0;for(U i=0;i<120;++i)b.write(first+i,1,0);
   text(b,first,"first");text(b,second,"second");text(b,third,"third");text(b,first+12,seed%4==3?"NO_WAVE":"MISSILE");text(b,second+12,seed%4==1?"MISSILE":"OTHER");text(b,third+12,"OTHER");b.write(second+0x18,4,0x12340000);b.write(first+0x20,1,seed&255);b.write(third+0x20,1,seed&255);b.write(0x6e06c4,4,first);b.write(0x6e06c8,4,2);b.write(0x4f2ea8+id*40,4,first);
   const auto result=b.call(0x43ee80,{id});std::uint64_t hash=14695981039346656037ull;if(result&&result!=0x12340000){const auto size=b.read(result+4,4)+16;for(U i=0;i<size;++i){hash^=b.read(result+i,1);hash*=1099511628211ull;}}
   std::cout<<"{\"result\":"<<(result==0x12340000?2:result?1:0)<<",\"hash\":"<<hash<<",\"flags\":"<<b.read(first+0x20,1)<<",\"target\":"<<b.read(0x4f2ea8+id*40,4)<<",\"other_flags\":"<<b.read(third+0x20,1)<<",\"diagnostics\":"<<diagnostics<<"}\n";
  }else if(mode=="channel"){
   const auto enabled=seed%2,slot=(seed/2)%16,value=seed*17;b.write(0xd08940,4,enabled);b.write(0x4a1703+slot*0x34,1,93);const auto result=b.call(0x4950a0,{slot,value});std::cout<<"{\"result\":"<<result<<",\"value\":"<<b.read(0x4a1703+slot*0x34,1)<<"}\n";b.write(0xd08940,4,1);
  }else if(mode=="extension"){
   const std::string names[]={"a.b.c","abc","",".cfg","many..dots","file.old"};const auto name=names[seed%6];text(b,start,name);text(b,start+64,"XYZ");U suffix=start+64;if(seed%3==0&&name.find('.')!=std::string::npos)suffix=start+U(name.find('.'))+1;b.call(0x456560,{start,suffix});std::cout<<"{\"bytes\":\""<<hex(b,start,128)<<"\"}\n";
  }else if(mode=="parse"){
   text(b,start,"LOOP.DEF");b.write(0x6e06cc,4,seed);b.call(0x43edb0,{start});
   const auto count=b.read(0x6e06c8,4);std::cout<<"{\"count\":"<<count<<",\"range\":"<<b.read(0x6e06cc,4)<<",\"rows\":\""<<hex(b,b.read(0x6e06c4,4),count*40)<<"\"}\n";
  }else throw std::runtime_error("Unknown sound runtime probe mode");
 }
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
