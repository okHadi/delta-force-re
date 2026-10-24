#include "dfre/mission_leave_raster.hpp"
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
using namespace dfre;using U=std::uint32_t;
int main(){try{std::string line;while(std::getline(std::cin,line)){
 U body,width,height,stride,bits,mode,hit,mutation,fail;std::istringstream in(line);if(!(in>>body>>width>>height>>stride>>bits>>mode>>hit>>mutation>>fail))throw std::runtime_error("Short leave raster fixture");
 const U d=body==0?0x2007fd8:0x1210000;constexpr U o=0x1300000,p=0x1100000;std::map<U,U>w{{0x503138,mode},{0x50130c,p},{0xd092c0,width},{0xd092c4,height},{0x4f8718,2},{0xcc0884,(mutation&16)?0u:1u},{0xd09300,(mutation&32)?0u:1u},{0xd09308,(mutation&64)?0u:0x1200100u},{0xd0924c,0x1200200},{0xd0930c,(mutation&128)?0u:0x1200300u},{0xd0932c,0},{0xd09330,0}};
 const U data[6]={width,height,stride,p,0x503740,bits};for(U i=0;i<6;++i)w[d+i*4]=data[i];for(U i=0;i<8;++i){w[o+4+i*4]=0;w[o+0xc08+i*4]=0;}w[o]=hit?p:0;w[o+0xc04]=hit?0x503740:0;w[o+0x1808]=hit?width:0;w[o+0x180c]=hit?height:0;w[o+0x1810]=hit?stride:0;w[o+0x1824]=8;
 U count=0,result=0;bool failed=false;std::ostringstream trace;auto step=[&]{if(++count==fail)throw std::runtime_error("Stopped");};MissionDebriefBindings b;
 b.read=[&](U at,U size){step();auto n=w.at(at);trace<<'R'<<at<<','<<size<<','<<n<<';';if((mutation&1)&&at==0x50130c)w[at]=p+0x100;if((mutation&4)&&at==0xd092c0)w[0xd09308]=0x1200300;return n;};
 b.write=[&](U at,U size,U n){step();w.at(at)=n;trace<<'W'<<at<<','<<size<<','<<n<<';';if((mutation&2)&&at==o+4)w[d+8]=stride+4;};
 b.call=[&](U site,const auto& args){step();trace<<'C'<<site;for(auto n:args)trace<<','<<n;trace<<';';if(site==0x456e30)w[0x4f8718]=args.at(0);if((mutation&8)&&site==0x1200200)w[0xd0930c]=0x1200100;return 0u;};
 try{switch(body){case 0:result=mission_leave_frame(b,o,d);break;case 1:mission_leave_present(b);break;case 2:mission_raster_rows(b,o,d);break;case 3:mission_display_rows(b,width,height);break;case 4:mission_display_present(b);break;case 5:result=mission_frame_pointer(b);break;default:result=mission_frame_zero();break;}}catch(const std::exception&){failed=true;result=0;}
 std::cout<<failed<<' '<<result<<' ';for(const auto& x:w)std::cout<<x.first<<':'<<x.second<<',';std::cout<<' '<<trace.str()<<'\n';
}return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
