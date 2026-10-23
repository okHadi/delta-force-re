#include "dfre/mission_scoreboard.hpp"
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
using U=std::uint32_t;
int main(){try{std::string line;while(std::getline(std::cin,line)){
    std::istringstream input(line);U seed,count;input>>seed>>count;
    std::unordered_map<U,std::uint8_t> bytes;
    const auto put=[&](U at,U width,U value){for(U i=0;i<width;++i)bytes[at+i]=std::uint8_t(value>>(i*8));};
    const auto get=[&](U at,U width){U n=0;for(U i=0;i<width;++i)n|=U(bytes[at+i])<<(i*8);return n;};
    constexpr U frame=0x1308000,packet=0x1200000,name=0x1210000;
    for(U i=0;i<0x400;++i)put(packet+i,1,(i*37+seed*11)&255);
    put(packet+0x8e,1,count);put(frame+0x44,4,packet);
    for(U i=0;i<256;++i){put(0x7cca8c+i*0x114,4,(seed+i)%3?name:0);}
    put(name+4,1,'P');put(name+5,1,0);put(name+0x100,1,'C');put(name+0x101,1,0);
    std::ostringstream trace;U lookups=0;dfre::MissionScoreboardBindings b;
    b.read=[&](U at,U width){const auto value=get(at,width);trace<<'R'<<at<<','<<width<<','<<value<<';';return value;};
    b.write=[&](U at,U width,U value){trace<<'W'<<at<<','<<width<<','<<value<<';';put(at,width,value);};
    b.call=[&](U site,const std::vector<U>& args)->U{
        trace<<'C'<<site;for(auto n:args)trace<<','<<n;trace<<';';++lookups;
        if(site!=0x404cf0||args!=std::vector<U>({0x4ccccc,0x4cccc0}))throw std::runtime_error("Unexpected scoreboard body");
        if(seed&1)put(0xbcfbe8,4,0);
        if(seed&2)put(frame+0x60,4,get(frame+0x60,4)+2);
        if(seed&4)put(frame+0x64,4,get(frame+0x64,4)+4);
        if((seed&8)&&lookups==1)put(frame+0x68,4,7);
        if(seed&16)put(frame+0x5c,4,packet+16);
        if(seed&32)put(packet+0x4b,4,0x89abcdef);
        if(seed&64)throw std::runtime_error("Scoreboard fixture lookup fault");
        return name+0x100;
    };
    bool failed=false;try{dfre::mission_scoreboard_receive(b,frame);}catch(const std::runtime_error&){failed=true;}
    trace<<'F'<<failed<<';';std::uint64_t hash=1469598103934665603ull;
    for(U i=0;i<0x3000;++i){hash^=get(0xbcf664+i,1);hash*=1099511628211ull;}
    for(U i=0;i<0x70;++i){hash^=get(frame+i,1);hash*=1099511628211ull;}
    trace<<'H'<<hash<<';';std::cout<<trace.str()<<'\n';
}}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
