#include "dfre/mission_debrief_format.hpp"
#include <array>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
using namespace dfre;using U=std::uint32_t;using Q=std::uint64_t;
int main(){try{std::string line;while(std::getline(std::cin,line)){
 U index,value,layout,mutation,fail_read,fail_write;std::array<std::string,3> hex;std::istringstream in(line);if(!(in>>index>>value>>layout>>mutation>>fail_read>>fail_write>>hex[0]>>hex[1]>>hex[2]))throw std::runtime_error("Short format fixture");
 constexpr U A=0x1200000,D=0x1300000;const U pointers[]{0x4cae34,0x4cad48,0x4cadd0,0x4cad50,0x4cad5c,0x4cadd8,0x4cae08,0x4cadbc,0x4cae14,0x4cc67c,0x4cc570,0x4cc278,0x4ccc60,0x4ccc80};const char* formats[]{"dfcamp%02i.bin","Win%d","Lose%d","Briefing%d","Unable to find mission %d debriefing won text","Unable to find mission %d debriefing lose text","%s ~ ~ %s","%s ~ %s : %d ~ ~ %s","Unable to find '%s'","WPN%03i","%i","%s %s","STR_PWNAME%02i","STR_SPWNAME%02i"};
 std::map<U,std::uint8_t> bytes;for(U base:{A,D})for(U i=0;i<4096;++i)bytes[base+i]=0xa5;for(U i=0;i<14;++i)for(U j=0;j<=std::string(formats[i]).size();++j)bytes[pointers[i]+j]=std::uint8_t(formats[i][j]);
 const U source=layout==2?A+16:A,dest=layout==0?D:layout==1?A+1:A;std::array<U,3> sources{source,A+128,A+256};std::array<U,3> lengths{};
 for(U k=0;k<3;++k){const auto& h=hex[k];const U size=h=="-"?0:U(h.size()/2);lengths[k]=size;for(U i=0;i<size;++i)bytes.at(sources[k]+i)=std::uint8_t(std::stoul(h.substr(i*2,2),nullptr,16));bytes.at(sources[k]+size)=0;}
 std::vector<U> args{dest,pointers[index]};if(index<6||index==9||index==10||index==12||index==13)args.push_back(value);else if(index==6||index==11){args.push_back((mutation&16)?0:sources[0]);args.push_back(sources[1]);}else if(index==7){args.push_back((mutation&16)?0:sources[0]);args.push_back(sources[1]);args.push_back(value);args.push_back(sources[2]);}else args.push_back((mutation&16)?0:sources[0]);
 U reads=0,writes=0,start_reads=0;bool failed=false;std::ostringstream trace;MissionDebriefBindings b;
 b.read=[&](U at,U width){if(at>=0x4c0000&&at<0x500000)return U(bytes.at(at));if(++reads==fail_read){trace<<'F'<<at<<','<<width<<';';throw std::runtime_error("Read stopped");}U n;try{n=bytes.at(at);}catch(const std::out_of_range&){trace<<'F'<<at<<','<<width<<';';throw std::runtime_error("Read unowned");}trace<<'R'<<at<<','<<width<<','<<n<<';';if(at==sources[0]){++start_reads;if(start_reads==2&&(mutation&2))bytes.at(sources[0]+1)=0;}if(at==sources[0]+lengths[0]&&n==0){if(mutation&1)bytes.at(sources[0])='Q';if(mutation&8){bytes.at(at)='Y';bytes.at(at+1)=0;}}return n;};
 b.write=[&](U at,U width,U n){if(++writes==fail_write){trace<<'E'<<at<<','<<width<<','<<n<<';';throw std::runtime_error("Write stopped");}if(!bytes.count(at)){trace<<'E'<<at<<','<<width<<','<<n<<';';throw std::runtime_error("Write unowned");}bytes.at(at)=std::uint8_t(n);trace<<'W'<<at<<','<<width<<','<<n<<';';if(writes==1&&(mutation&4))bytes.at(sources[1])='X';};
 U count=0;try{count=mission_debrief_format(b,args);}catch(const std::exception&){failed=true;}Q hash=1469598103934665603ull;for(const auto& entry:bytes){hash^=entry.second;hash*=1099511628211ull;}std::cout<<failed<<' '<<count<<' '<<hash<<' '<<trace.str()<<'\n';
}return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
