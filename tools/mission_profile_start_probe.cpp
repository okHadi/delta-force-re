#include "dfre/mission_profile_start.hpp"
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
using namespace dfre;using U=std::uint32_t;
int main(){try{std::string line;while(std::getline(std::cin,line)){
 U mode,profile,campaign,mask,rules,branch,mutation,fail;std::istringstream in(line);if(!(in>>mode>>profile>>campaign>>mask>>rules>>branch>>mutation>>fail))throw std::runtime_error("Short fixture");
 std::map<U,U>w{{0x4cee90,0x12345678},{0x4cf2e4,profile},{profile+0x28,campaign},{profile+0x70+campaign*0x44,mask},{0x67f54c,0},{0x67f70c,mask},{0x67dbec,0},{0x67f548,rules},{0x503328,branch},{0x67dbe4,0}};
 U count=0,result=0;bool failed=false;std::ostringstream trace;auto step=[&]{if(++count==fail)throw std::runtime_error("Stopped");};MissionDebriefBindings b;
 b.read=[&](U at,U width){step();auto n=w.at(at);trace<<'R'<<at<<','<<width<<','<<n<<';';if((mutation&1)&&at==0x4cf2e4)w[at]=profile+0x400;if((mutation&2)&&at==0x67f548)w[0x503328]^=0x87654321;return n;};
 b.write=[&](U at,U width,U n){step();w.at(at)=n;trace<<'W'<<at<<','<<width<<','<<n<<';';};
 b.call=[&](U site,const auto& args){step();if(site!=0x49a620||args!=std::vector<U>{0x67f714,0,0x514})throw std::runtime_error("Bad clear");trace<<'C'<<site<<';';if(mutation&4){w[0x67f548]^=0xfedcba98;w[0x67f70c]^=0xabcdef01;}return 0u;};
 try{if(mode)result=mission_profile_start_result(b);else mission_profile_start_mask(b);}catch(const std::exception&){failed=true;result=0;}
 std::cout<<failed<<' '<<result<<' ';for(const auto& x:w)std::cout<<x.first<<':'<<x.second<<',';std::cout<<' '<<trace.str()<<'\n';
}return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
