#include "dfre/campaign.hpp"
#include <fstream>
#include <iostream>
#include <iomanip>
#include <sstream>
namespace {
std::string quote(std::string_view value){std::ostringstream out;out<<'"';for(unsigned char c:value){if(c=='"'||c=='\\')out<<'\\'<<char(c);else if(c<32||c>=127)out<<"\\u"<<std::hex<<std::setw(4)<<std::setfill('0')<<unsigned(c)<<std::dec;else out<<char(c);}out<<'"';return out.str();}
}
int main(int argc,char** argv){try{
 if(argc==6&&std::string(argv[1])=="briefing"){
  const dfre::AssetStore assets(argv[2]);const auto campaign=std::stoul(argv[3]),mission=std::stoul(argv[4]),mask=std::stoul(argv[5],nullptr,0);std::ostringstream filename;filename<<"dfcamp"<<std::setw(2)<<std::setfill('0')<<campaign+1<<".bin";const dfre::RtxtTable table(assets.read(filename.str()).bytes);const auto group="Briefing"+std::to_string(mission+1);
  const auto expanded=dfre::expand_briefing(table.get(group,"Mission"+std::to_string(mission+1)+"a"),[&](std::string_view key){return table.get(group,key);},std::uint32_t(mask));const auto parsed=dfre::parse_briefing(expanded);std::cout<<"{\"expanded\":"<<quote(expanded)<<",\"plain\":"<<quote(parsed.plain)<<",\"links\":"<<parsed.links.size()<<"}\n";return 0;
 }
 const bool setup=argc>1&&std::string(argv[1])=="setup";const int start=setup?2:1;
 if(argc!=start+2&&argc!=start+3)throw dfre::AssetError("campaign_probe GAME_FOLDER SAVE_FOLDER [OUTPUT.ppm] | briefing GAME_FOLDER CAMPAIGN MISSION MASK");
 dfre::AssetStore assets(argv[start]);dfre::ProfileStore profiles(argv[start],argv[start+1]);dfre::MenuRuntime menu(assets);dfre::CampaignMenu campaign(menu,argv[start],profiles.at(profiles.first_active()));campaign.enter();
 unsigned missions=0,available=0;for(const auto& c:campaign.catalog().campaigns())missions+=unsigned(c.missions.size());const auto& state=campaign.state();for(unsigned i=0;i<state.definition().missions.size();++i)available+=state.available(i);
 dfre::MissionSetupMenu mission_menu(menu,argv[start],profiles.at(profiles.first_active()));
 if(setup){const auto& chosen=state.definition().missions.at(state.mission());mission_menu.enter({state.campaign(),state.mission(),chosen.equipment_flags,chosen.prerequisites,chosen.filename,state.definition().text_file});}
 const auto& frame=setup?mission_menu.draw():campaign.draw();std::cout<<"{\"campaigns\":"<<campaign.catalog().campaigns().size()<<",\"missions\":"<<missions<<",\"campaign\":"<<state.campaign()<<",\"mission\":"<<state.mission()<<",\"available\":"<<available<<",\"pixels\":"<<frame.size()<<"}\n";
 if(argc==start+3){std::ofstream output(argv[start+2],std::ios::binary);output<<"P6\n640 480\n255\n";for(auto pixel:frame)for(unsigned shift:{16,8,0})output.put(char(pixel>>shift));if(!output)throw dfre::AssetError("Cannot write campaign image");}return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
