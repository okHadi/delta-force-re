#include "dfre/mission_client_finalize.hpp"
#include <array>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
using namespace dfre;using U=std::uint32_t;
int main(){try{std::string line;while(std::getline(std::cin,line)){
    U seed,count;std::istringstream in(line);in>>seed>>count;std::unordered_map<U,std::uint8_t> memory;std::vector<std::array<U,3>> initial;
    auto put=[&](U at,U width,U value){for(U i=0;i<width;++i)memory[at+i]=std::uint8_t(value>>(i*8));};
    auto get=[&](U at,U width){U value=0;for(U i=0;i<width;++i){const auto p=memory.find(at+i);if(p!=memory.end())value|=U(p->second)<<(i*8);}return value;};
    auto text=[&](U at,const std::string& s){for(U i=0;i<=s.size();++i)put(at+i,1,i<s.size()?std::uint8_t(s[i]):0);};
    for(U i=0;i<count;++i){std::array<U,3> row;if(!(in>>row[0]>>row[1]>>row[2]))throw MissionClientAddedError("Short finalization fixture");put(row[0],row[1],row[2]);initial.push_back(row);}
    std::ostringstream trace;U client=0;MissionClientAddedBindings b;
    b.read=[&](U at,U width){if(at<0x1000)throw MissionClientAddedError("Fixture null read");const auto value=get(at,width);trace<<'R'<<at<<','<<width<<','<<value<<';';return value;};
    b.write=[&](U at,U width,U value){trace<<'W'<<at<<','<<width<<','<<value<<';';if(at<0x1000)throw MissionClientAddedError("Fixture null write");put(at,width,value);
        if(at==client+8&&(seed&4))put(at,4,0x7cca80+0x114);
        if(at==0x7cca80+8&&(seed&8))put(client+0x9c,2,0x5678);
    };
    b.call=[&](U site,const std::vector<U>& args)->U{
        trace<<'C'<<site;for(auto n:args)trace<<','<<n;trace<<';';
        if(site==0x45d520){client=0x6e9a90+(args.at(0)%17)*0x9b84;if(seed&1)put(client,4,88);if(seed&2)put(0x6e9a90+0x9b84+0x18c,1,1);return seed&512?0:0x7cca80;}
        if(site==0x43f610){if(seed&16)put(client+8,4,0x7cca80+2*0x114);if(seed&32)put(get(client+8,4)+0x0c,4,0x794680+0xb4);if(seed&64)text(client+0x0c,"current");if(seed&128)put(0x6e9a90+0x9b84+0x18c,1,0);if(seed&256)throw MissionClientAddedError("Fixture pose fault");return 0;}
        throw MissionClientAddedError("Unexpected finalization body");
    };
    bool failed=false;try{mission_client_finalize(b);}catch(const MissionClientAddedError&){failed=true;}trace<<'F'<<failed<<';';
    for(const auto& row:initial)trace<<'V'<<row[0]<<','<<row[1]<<','<<get(row[0],row[1])<<';';
    for(U i=0;i<3*0x114;++i)trace<<'A'<<get(0x7cca80+i,1)<<',';
    for(U i=0;i<2*0xb4;++i)trace<<'P'<<get(0x794680+i,1)<<',';
    std::cout<<trace.str()<<'\n';
}}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
