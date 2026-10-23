#include "dfre/mission_client_added.hpp"
#include <array>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
using namespace dfre;
using U=std::uint32_t;
int main(){try{std::string line;while(std::getline(std::cin,line)){
    std::istringstream in(line);U seed,count;if(!(in>>seed>>count))throw MissionClientAddedError("Short client-added fixture");
    std::unordered_map<U,std::uint8_t> memory;std::vector<std::array<U,3>> initial;
    auto put=[&](U at,U width,U value){for(U i=0;i<width;++i)memory[at+i]=std::uint8_t(value>>(8*i));};
    auto get=[&](U at,U width){U value=0;for(U i=0;i<width;++i){const auto found=memory.find(at+i);if(found!=memory.end())value|=U(found->second)<<(8*i);}return value;};
    auto text=[&](U at,const std::string& value){for(U i=0;i<=value.size();++i)put(at+i,1,i<value.size()?std::uint8_t(value[i]):0);};
    for(U i=0;i<count;++i){std::array<U,3> row;if(!(in>>row[0]>>row[1]>>row[2]))throw MissionClientAddedError("Short client-added memory");put(row[0],row[1],row[2]);initial.push_back(row);}
    std::ostringstream trace;unsigned diagnostics=0;MissionClientAddedBindings b;
    b.read=[&](U at,U width){const auto value=get(at,width);trace<<'R'<<at<<','<<width<<','<<value<<';';return value;};
    b.write=[&](U at,U width,U value){trace<<'W'<<at<<','<<width<<','<<value<<';';put(at,width,value);};
    b.call=[&](U site,const std::vector<U>& args)->U{
        trace<<'C'<<site;for(auto n:args)trace<<','<<n;trace<<';';
        if((seed&0x100000)&&site==0x497870)throw MissionClientAddedError("Fixture callback fault");
        if((seed&0x200000)&&site==0x4406e0)throw MissionClientAddedError("Fixture callback fault");
        if(site==0x493a80){text(args.at(0),"join");return 4;}
        if(site==0x4564b0){if(++diagnostics==1&&(seed&8)){put(0x501344,4,99);put(0x50136c,4,0);put(0x67f528,4,1);}return 0;}
        if(site==0x49a620){for(U i=0;i<args.at(2);++i)put(args.at(0)+i,1,args.at(1));if(seed&16)put(0x67f530,4,3);return args.at(0);}
        if(site==0x442450){if(seed&32)put(args.at(0)+0x94,4,999);return 0;}
        if(site==0x45da30){if(seed&64)put(0x6e9a98+0x9b84,4,0);return 0;}
        if(site==0x4423f0){if(seed&128)put(0x1301000,1,0x85);return 0;}
        if(site==0x497870){if(seed&256)put(0x6e5588,4,1);return seed&2?0:0x1302000;}
        if(site==0x45d520){if(seed&512)put(0x2007f60+0x6c,4,88);return seed&4?0:0x7cca80;}
        if(site==0x4406e0){put(args.at(0)+0x9c,4,args.at(1)+10);return 0;}
        if(site==0x43f610){if(seed&1024)text(args.at(0)+0x0c,"changed");return 0;}
        if(site==0x404cf0)return 0x1302100;
        if(site==0x43f770)return 0x1302200;
        throw MissionClientAddedError("Unexpected client-added body");
    };
    bool failed=false;try{mission_client_added(b,0x2007f60,0x200);}catch(const MissionClientAddedError&){failed=true;}trace<<'F'<<failed<<';';
    for(const auto& row:initial)trace<<'V'<<row[0]<<','<<row[1]<<','<<get(row[0],row[1])<<';';
    // Full current client bytes and local frame, including fields absent from fixture rows.
    std::uint64_t hash=1469598103934665603ull;
    for(auto region:std::vector<std::pair<U,U>>{{0x6e9a90,17*0x9b84},{0x2007f60,0x90},{0x7cca80,0x114},{0x1200000,0x100}})
        for(U i=0;i<region.second;++i){hash^=get(region.first+i,1);hash*=1099511628211ull;}
    trace<<'H'<<hash<<';';std::cout<<trace.str()<<'\n';
}}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
