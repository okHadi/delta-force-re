#include "dfre/mission_post_menu.hpp"
#include <array>
#include <iostream>
#include <map>
#include <sstream>
namespace {
using U=std::uint32_t;
constexpr std::array<U,11> cells{0x67dbbc,0x67f520,0x67f528,0x67f534,0x67f53c,0x68d530,0x4e9bf0,0x4e9bdc,0x4e9c18,0x50322c,0x1300000};
}
int main(){std::string line;while(std::getline(std::cin,line)){
    if(line.empty())continue;std::istringstream input(line);std::array<U,16> row{};for(auto& v:row)if(!(input>>v))return 2;
    std::map<U,U> memory;for(U i=0;i<cells.size();++i)memory[cells[i]]=row[i+5];
    std::ostringstream trace;U reads=0,writes=0,calls=0;
    dfre::MissionPostMenuBindings b;
    b.read=[&](U at,U width){if(width!=4)throw std::runtime_error("Width");const U value=memory.at(at);trace<<'R'<<at<<",4,"<<value<<';';if(++reads==row[2])throw std::runtime_error("Read");return value;};
    b.write=[&](U at,U width,U value){if(width!=4)throw std::runtime_error("Width");trace<<'W'<<at<<",4,"<<value<<';';memory.at(at)=value;if(++writes==row[3])throw std::runtime_error("Write");if(row[1]&512)memory.at(at)^=0x40;};
    b.call=[&](U at,const auto& args)->U{
        trace<<'C'<<at;for(U v:args)trace<<','<<v;trace<<';';++calls;
        const U m=row[1];if(m&1)memory.at(0x67dbbc)^=3;if(m&2)memory.at(0x67f520)^=1;if(m&4)memory.at(0x67f528)^=1;if(m&8)memory.at(0x67f534)=(memory.at(0x67f534)+1)%4;if(m&16)memory.at(0x67f53c)^=1;if(m&32)memory.at(0x68d530)^=4;if(m&64)memory.at(0x4e9bf0)^=1;if(m&128)memory.at(0x4e9bdc)^=1;if(m&256)memory.at(0x50322c)^=0x40;
        if(calls==row[4])throw std::runtime_error("Call");
        if(at==0x40bfd0)return 0x1300100;if(at==0x49a092)return 0x1300000;return 0;
    };
    bool failed=false;U result=0;try{switch(row[0]){case 0:dfre::mission_post_menu_enter(b);break;case 1:result=dfre::mission_post_menu_update(b);break;case 2:dfre::mission_post_menu_render(b);break;case 3:dfre::mission_post_menu_leave(b);break;default:return 3;}}catch(const std::exception&){failed=true;}
    std::uint64_t hash=1469598103934665603ull;for(U at:cells)for(U i=0;i<4;++i){hash^=(memory.at(at)>>(8*i))&255;hash*=1099511628211ull;}
    std::cout<<failed<<' '<<(failed?0:result)<<' '<<hash<<' '<<trace.str()<<'\n';
}}
