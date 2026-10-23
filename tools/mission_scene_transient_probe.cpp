#include "dfre/mission_scene_transient.hpp"
#include <iostream>
#include <memory>
using namespace dfre;
namespace {using U=std::uint32_t;void put(AiActorImage& image,std::size_t at,U value){for(U i=0;i<4;++i)image[at+i]=std::uint8_t(value>>(8*i));}}
int main(){try{U seed,active,characters,high_water,mutation;while(std::cin>>seed>>active>>characters>>high_water>>mutation){
 (void)high_water;MissionRuntimeState world;world.definitions.resize(3);world.definitions[1].type=6;world.definitions[2].type=3;auto images=std::make_unique<std::array<AiActorImage,1024>>();const U slots[]={0,1,511,1023};
 for(U i=0;i<4;++i){auto& image=(*images)[slots[i]];for(U j=0;j<0x114;++j)image[j]=std::uint8_t(seed+i*31+j*17);put(image,0,(active&(1u<<i))?7u:0u);put(image,4,(characters&(1u<<i))?2u:1u);put(image,0x10,0x71000000);for(U axis=0;axis<3;++axis)put(image,0x24+axis*4,0);image[0x6c]=0;}
 std::size_t reads=0;const auto records=mission_scene_transient_records(world,[&](std::size_t slot){if(slot==0&&mutation){put((*images)[1023],0,7);put((*images)[1023],0x30,0xdeadbeef);put((*images)[1023],4,2);}++reads;return (*images)[slot];});std::cout<<reads<<' '<<records.size();for(const auto& record:records){std::uint64_t hash=1469598103934665603ull;for(auto n:record.image){hash^=n;hash*=1099511628211ull;}std::cout<<' '<<record.slot<<' '<<record.character<<' '<<hash;}std::cout<<'\n';}
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
