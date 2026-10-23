#include "dfre/mission_banned_record.hpp"
#include <iostream>
#include <map>
#include <sstream>
int main(){using U=std::uint32_t;U seed;constexpr U frame=0x2008000-24,record=0x1300000,ip=record+0x100,label=record+0x200;while(std::cin>>seed){
 std::map<U,std::uint8_t> bytes;std::ostringstream trace;const auto put=[&](U at,U w,U value){for(U i=0;i<w;++i)bytes[at+i]=std::uint8_t(value>>(8*i));};const auto get=[&](U at,U w){U n=0;for(U i=0;i<w;++i)n|=U(bytes.at(at+i))<<(8*i);return n;};const auto text=[&](U at,const std::string& s){for(U i=0;i<=s.size();++i)put(at+i,1,i<s.size()?std::uint8_t(s[i]):0);};
 put(0x6e5888,4,seed%5);put(record,4,seed%3);put(record+4,4,ip);put(record+8,4,label);text(0x4cc6ec,"?");
 const std::string values[]={"192.168.1.2","1",".2..4","","1.2.3.4.5","-1.256.999.0"};text(ip,values[seed%6]);text(label,seed%4==0?std::string(16,'A'):seed%4==1?"":seed%4==2?"a":"fifteen letters");U calls=0;
 dfre::MissionLineFileBindings b;b.read=[&](U at,U w){const auto n=get(at,w);trace<<'R'<<at<<','<<w<<','<<n<<';';return n;};b.write=[&](U at,U w,U n){trace<<'W'<<at<<','<<w<<','<<n<<';';put(at,w,n);};b.call=[&](U site,const std::vector<U>& a){trace<<'C'<<site;for(auto n:a)trace<<','<<n;trace<<';';if(site!=0x49afa0||a.size()!=1)throw std::runtime_error("Unexpected atoi call");const U index=calls++;if(seed&1)put(0x6e5888,4,seed+index);if(index==1&&(seed&2))put(frame+4,4,0x12345678+seed);if(index==2&&(seed&4))put(record,4,1);return (seed*0x1234567u+index*0x89abcdefu);};
 const auto result=dfre::mission_banned_record(b,seed%13?record:0,frame);std::cout<<result<<' '<<trace.str()<<'\n';
}}
