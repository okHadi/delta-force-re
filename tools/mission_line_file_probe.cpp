#include "dfre/mission_line_file.hpp"
#include <array>
#include <iostream>
#include <sstream>
int main(){using U=std::uint32_t;U seed;constexpr U frame=0x2008000-0x3d0;while(std::cin>>seed){
 std::array<std::uint8_t,0x400> bytes{};for(U i=0;i<bytes.size();++i)bytes[i]=std::uint8_t(seed+i*13);std::ostringstream trace;
 const auto put=[&](U at,U width,U n){if(at<frame||std::uint64_t(at-frame)+width>bytes.size())throw std::runtime_error("Fixture write outside frame");for(U i=0;i<width;++i)bytes.at(at-frame+i)=std::uint8_t(n>>(8*i));};
 const auto get=[&](U at,U width){if(at==0x1300100&&width==1)return U((seed&1)?'/':'a');if(at<frame||std::uint64_t(at-frame)+width>bytes.size())throw std::runtime_error("Fixture read outside frame");U n=0;for(U i=0;i<width;++i)n|=U(bytes.at(at-frame+i))<<(8*i);return n;};
 put(frame+0x3d4,4,0x1300000);put(frame+0x3d8,4,0x1300200);U read_count=0;
 dfre::MissionLineFileBindings b;b.read=[&](U at,U width){const auto n=get(at,width);trace<<'R'<<at<<','<<width<<','<<n<<';';return n;};b.write=[&](U at,U width,U n){trace<<'W'<<at<<','<<width<<','<<n<<';';put(at,width,n);};
 b.call=[&](U site,const std::vector<U>& args){trace<<'C'<<site;for(auto n:args)trace<<','<<n;trace<<';';if(site==0x49a41c)return seed%8?0x12340000u:0u;
  if(site==0x49a550){if(++read_count>seed%5+1)return 0u;const char* text=read_count%3==1?"alpha beta\n":read_count%3==2?"\n":"// skipped\n";U offset=0;do{put(frame+offset,1,U(std::uint8_t(text[offset])));}while(text[offset++]);return frame;}
  if(site==0x49a840)return 0u;
  if(site!=0x1300200&&site!=0x1300204)throw std::runtime_error("Fixture call is not owned");put(frame+0x3bc,4,0x12340000+seed+args.at(1));put(frame+0x3d8,4,site^4);return args.at(0)?(seed%4==0?17u:0u):19u+seed;
 };
 b.tokenize=[&](U line,U output){trace<<'T'<<line<<','<<output<<';';for(U i=0;i<31;++i)put(output+i*4,4,i==0?(get(line,1)?2:0):i==1?0x1300100:seed+i);};
 const auto result=dfre::mission_line_file(b,frame);std::cout<<result<<' '<<trace.str()<<'\n';
}}
