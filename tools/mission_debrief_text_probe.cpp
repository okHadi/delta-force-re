#include "dfre/mission_debrief_text.hpp"
#include <cstring>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
using namespace dfre;using U=std::uint32_t;using Q=std::uint64_t;
int main(){try{std::string line;while(std::getline(std::cin,line)){
 U mode,seed,scenario,mutation,fail_call,fail_read,fail_write;std::istringstream in(line);if(!(in>>mode>>seed>>scenario>>mutation>>fail_call>>fail_read>>fail_write))throw std::runtime_error("Short debrief text fixture");
 constexpr U P=0x1100000,F=0x1300000,T=0x1320000,S=0x1330000,A=0x1340000;std::map<U,std::uint8_t> bytes;
 const auto reserve=[&](U at,U n){for(U i=0;i<n;++i)bytes[at+i]=0;};reserve(P,0x500);reserve(F,0x128);reserve(T,0x2710);reserve(S,0x1000);reserve(A,0x3000);reserve(0x4d0d08,24);
 for(U at:{0x4cf2e4u,0x67f718u,0x68ba34u,0x4e6cfcu,0x503238u,0x503234u,0x68bef0u,0x4e7550u,0x4e756cu})reserve(at,4);
 const auto put=[&](U at,U n,U width=4){for(U i=0;i<width;++i)bytes.at(at+i)=std::uint8_t(n>>(8*i));};
 const auto get=[&](U at,U width=4){U n=0;for(U i=0;i<width;++i)n|=U(bytes.at(at+i))<<(8*i);return n;};
 const auto text=[&](U at,const std::string& s){for(U i=0;i<=s.size();++i)put(at+i,i<s.size()?std::uint8_t(s[i]):0,1);};
 const auto string=[&](U at){std::string s;for(U i=0;;++i){const auto c=get(at+i,1);if(!c)return s;s+=char(c);}};
 const auto number=[](U n){std::int32_t v;std::memcpy(&v,&n,4);return std::to_string(v);};
 put(0x4cf2e4,P);put(P+0x28,1);put(P+0x228,2);for(U c=0;c<2;++c)for(U i=0;i<4;++i)put(P+c*0x200+(c+1)*0x44+0x30+i*4,(seed&(1u<<i))?1:0);
 const U counts[]{0,1,3,4,0xffffffff,0x80000000};put(0x4d0d0c,counts[scenario%6]);put(0x4d0d10,2);put(0x67f718,seed&1);put(0x68ba34,0);put(0x4e6cfc,seed&1);put(0x503238,0x77770000);put(0x503234,0x77780000);put(0x68bef0,T);put(0x4e7550,0);put(0x4e756c,seed);
 const std::string payloads[]{"plain","a{ADD}A","{ADD}A tail","{ADD}M","{ADD}A {ADD}B ","{ADD} A","{AD}x",std::string("{ADD}")+std::string(70,'A'),std::string(4200,'x')};text(A+0x100,payloads[scenario%9]);text(A+0x1200,"ALPHA");text(A+0x1300,"BETA");text(A+0x1400,"STATS");text(A+0x1500,"KILLS");text(A+0x1600,"{ADD}A");
 for(U i=0;i<6;++i)put(A+i*4,A+0x100+i*0x100);put(A+4,A+0x1200);put(A+8,A+0x1300);put(A+12,A+0x1400);put(A+16,A+0x1500);put(A+20,A+0x1600);
 U calls=0,reads=0,writes=0,lookups=0;bool failed=false;std::ostringstream trace;MissionDebriefBindings b;
 b.read=[&](U at,U width){if(++reads==fail_read){trace<<'F'<<at<<','<<width<<';';throw std::runtime_error("Read stopped");}U n;try{n=get(at,width);}catch(const std::out_of_range&){trace<<'F'<<at<<','<<width<<';';throw std::runtime_error("Read unowned");}trace<<'R'<<at<<','<<width<<','<<n<<';';if((mutation&1)&&at==0x4d0d0c)put(0x4cf2e4,P+0x200);if((mutation&32)&&at==A+4)put(A+4,A+0x1300);return n;};
 b.write=[&](U at,U width,U n){if(++writes==fail_write){trace<<'E'<<at<<','<<width<<','<<n<<';';throw std::runtime_error("Write stopped");}for(U i=0;i<width;++i)if(!bytes.count(at+i)){trace<<'E'<<at<<','<<width<<','<<n<<';';throw std::runtime_error("Write unowned");}put(at,n,width);trace<<'W'<<at<<','<<width<<','<<n<<';';if((mutation&256)&&at==F+0x124)put(at,S+32);};
 b.call=[&](U site,const std::vector<U>& args)->U{trace<<'C'<<site;for(U n:args)trace<<','<<n;trace<<';';if(++calls==fail_call)throw std::runtime_error("Call stopped");
 if(site==0x4c4f40){if(mutation&2){put(0x4cf2e4,P+0x200);put(0x68ba34,1);}return 0;}
 if(site==0x497f60)return scenario==15?0:S;
 if(site==0x4564b0||site==0x4980d0)return 0;
 if(site==0x49a620){for(U i=0;i<args.at(2);++i)put(args.at(0)+i,args.at(1),1);return args.at(0);}
 if(site==0x49a092){++lookups;if(lookups==1){if(mutation&8)put(0x68bef0,T+32);return scenario==14?0:A;}if(args.at(2)==0x4cada8)return A+12;if(args.at(2)==0x4cad8c)return A+16;const auto key=string(args.at(2));if(mutation&128)put(F+0x124,S+64);return key=="A"?A+4:key=="B"?A+8:key=="N"?A+20:0;}
 if(site==0x493a80){const U dest=args.at(0),format=args.at(1);std::string value;switch(format){case 0x4cad48:value="Win"+number(args.at(2));break;case 0x4cadd0:value="Lose"+number(args.at(2));break;case 0x4cad50:value="Briefing"+number(args.at(2));break;case 0x4cad5c:value="Unable to find mission "+number(args.at(2))+" debriefing won text";break;case 0x4cadd8:value="Unable to find mission "+number(args.at(2))+" debriefing lose text";break;case 0x4cae08:value=string(args.at(2))+" ~ ~ "+string(args.at(3));break;case 0x4cadbc:value=string(args.at(2))+" ~ "+string(args.at(3))+" : "+number(args.at(4))+" ~ ~ "+string(args.at(5));break;case 0x4cae14:value="Unable to find '"+string(args.at(2))+"'";break;default:throw std::runtime_error("Unknown format fixture");}text(dest,value);if((mutation&4)&&dest==F)put(0x68ba34,2);if((mutation&16)&&(format==0x4cae08||format==0x4cadbc))put(F+0x124,S+32);if((mutation&64)&&(format==0x4cae08||format==0x4cadbc)){put(dest+U(value.size())+1,'Z',1);put(dest+U(value.size())+2,0,1);}return U(value.size());}
 throw std::runtime_error("Unknown debrief text fixture call");};
 try{if(mode==0)mission_debrief_text(b,F);else if(mode==1)mission_debrief_campaign_audio(b);else if(mode==2)mission_debrief_won_audio(b);else throw std::runtime_error("Unknown text fixture mode");}catch(const std::exception&){failed=true;}
 Q hash=1469598103934665603ull;for(const auto& entry:bytes){hash^=entry.second;hash*=1099511628211ull;}std::cout<<failed<<' '<<hash<<' '<<trace.str()<<'\n';
}return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
