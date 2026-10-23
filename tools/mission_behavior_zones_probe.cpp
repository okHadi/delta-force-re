#include "dfre/mission_behavior_zones.hpp"
#include <iostream>
#include <map>
#include <vector>
int main(){try{
    using U=std::uint32_t;U mode,gate,count,x,y,z,radius,type,flags;
    while(std::cin>>mode>>gate>>count>>x>>y>>z>>radius>>type>>flags){
        std::map<U,U> cells{{0x67f528,gate},{0x4f8d44,count}};const U row=0x8e48b0u+(count<<5);
        for(U i=0;i<8;++i)if(!cells.count(row+i*4))cells[row+i*4]=0xabc00000+i;
        std::vector<U> reads;std::vector<std::pair<U,U>> writes;
        dfre::MissionBehaviorZoneBindings b;
        b.read=[&](U at,U width){if(width!=4)throw std::runtime_error("Wrong read width");reads.push_back(at);return cells.at(at);};
        b.write=[&](U at,U width,U value){if(width!=4)throw std::runtime_error("Wrong store width");writes.push_back({at,value});cells[at]=value;};
        bool added=false;if(mode)dfre::mission_behavior_zones_reset(b);else added=dfre::mission_behavior_zone_add(b,x,y,z,radius,type,flags);
        std::cout<<"{\"added\":"<<added<<",\"count\":"<<cells[0x4f8d44]<<",\"row\":[";
        for(U i=0;i<8;++i){if(i)std::cout<<',';std::cout<<cells[row+i*4];}
        std::cout<<"],\"reads\":[";for(std::size_t i=0;i<reads.size();++i){if(i)std::cout<<',';std::cout<<reads[i];}
        std::cout<<"],\"writes\":[";for(std::size_t i=0;i<writes.size();++i){if(i)std::cout<<',';std::cout<<'['<<writes[i].first<<','<<writes[i].second<<']';}
        std::cout<<"]}\n";
    }
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
