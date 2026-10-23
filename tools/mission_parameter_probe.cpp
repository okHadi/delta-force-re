#include "dfre/mission_parameter.hpp"
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
using namespace dfre;using U=std::uint32_t;
int main(){try{std::string line;while(std::getline(std::cin,line)){
 U mode,index,value,table,initial,target,mutation,fail_read,fail_write;std::istringstream in(line);if(!(in>>mode>>index>>value>>table>>initial>>target>>mutation>>fail_read>>fail_write))throw std::runtime_error("Short parameter fixture");
 const U address=table+index*4u;std::map<U,U> words{{0x4b691c,table},{0x4b69f0,initial}};if(table)words[address]=target;words[0x4b691c]=table;
 U reads=0,writes=0,result=0;bool failed=false;std::ostringstream trace;MissionDebriefBindings b;
 b.read=[&](U at,U width){if(++reads==fail_read){trace<<'F'<<at<<','<<width<<';';throw std::runtime_error("Read stopped");}const U n=words.at(at);trace<<'R'<<at<<','<<width<<','<<n<<';';if(at==0x4b691c){if(mutation&1)words.at(at)=0;if((mutation&8)&&table)words.at(address)^=0xfedcba98u;}if((mutation&2)&&at==address&&at!=0x4b691c)words.at(0x4b69f0)=0x12345678;return n;};
 b.write=[&](U at,U width,U n){if(++writes==fail_write){trace<<'E'<<at<<','<<width<<','<<n<<';';throw std::runtime_error("Write stopped");}words.at(at)=n;trace<<'W'<<at<<','<<width<<','<<n<<';';if(mutation&4)words.at(0x4b69f0)^=0xabcdef01u;};
 try{result=mode?mission_parameter_get(b,index):mission_parameter_set(b,index,value);}catch(const std::exception&){failed=true;result=0;}
 std::cout<<failed<<' '<<result<<' ';for(const auto& w:words)std::cout<<w.first<<':'<<w.second<<',';std::cout<<' '<<trace.str()<<'\n';
}return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
