#include "dfre/profiles.hpp"
#include "dfre/soldier_menu.hpp"
#include <fstream>
#include <iostream>
int main(int argc,char** argv){try{
 if(argc==3&&std::string(argv[1])=="defaults"){
  const auto defaults=dfre::load_profile_defaults(argv[2]);std::cout<<"{\"controls\":[";
  for(std::size_t i=0;i<46;++i){if(i)std::cout<<',';std::cout<<'[';for(unsigned j=0;j<3;++j){if(j)std::cout<<',';const auto at=i*12+j*4;std::uint32_t value=0;for(unsigned k=0;k<4;++k)value|=std::uint32_t(defaults.controls.at(at+k))<<(k*8);std::cout<<value;}std::cout<<']';}std::cout<<"]}\n";return 0;
 }
 if(argc!=3&&argc!=4)throw dfre::AssetError("profiles_probe GAME_FOLDER SAVE_FOLDER [OUTPUT.ppm] | defaults GAME_FOLDER");
 dfre::AssetStore assets(argv[1]);dfre::MenuRuntime menu(assets);dfre::SoldierMenu soldier(menu,argv[1],argv[2]);soldier.enter();
 std::cout<<"{\"profiles\":9,\"selected\":"<<soldier.selected()<<",\"active\":[";
 for(unsigned i=0;i<9;++i){if(i)std::cout<<',';std::cout<<(soldier.profiles().at(i).active()?"true":"false");}
 const auto& pixels=soldier.draw();std::cout<<"],\"pixels\":"<<pixels.size()<<"}\n";
 if(argc==4){std::ofstream file(argv[3],std::ios::binary);file<<"P6\n640 480\n255\n";for(auto pixel:pixels)for(auto shift:{16,8,0})file.put(char(pixel>>shift));if(!file)throw dfre::AssetError("Cannot write profile screen");}
 return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
