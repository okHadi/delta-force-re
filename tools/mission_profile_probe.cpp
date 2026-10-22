#include "dfre/mission_results.hpp"
#include <algorithm>
#include <array>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using U=std::uint32_t;
std::int32_t signed_word(U n){return n<=0x7fffffffu?std::int32_t(n):std::int32_t(std::int64_t(n)-0x100000000LL);}
template<class A> U word(const A& a,std::size_t at){U n=0;for(unsigned i=0;i<4;++i)n|=U(a.at(at+i))<<(i*8);return n;}
template<class A> void put(A& a,std::size_t at,U n){for(unsigned i=0;i<4;++i)a.at(at+i)=std::uint8_t(n>>(i*8));}
std::vector<U> addresses(){
    std::vector<U> r{0x67f520,0x67f528,0x67f53c,0x67f718,0x67f70c,0x67dbec,0x68ba34,0x503134,0x503130};
    const std::array<U,23> source{0x67f4e4,0x67f4e8,0x67f4ec,0x67f4f0,0x67f4f4,0x67f4f8,0x67f4fc,0x67f500,0x67f504,0x67f510,0x67f514,0x67f508,0x67f50c,0x67f540,0x67f544,0x67f54c,0x67f548,0x67f534,0x67f550,0x67f554,0x67f558,0x67f55c,0x67f564};
    const std::array<U,23> target{0x4ced24,0x4ced28,0x4ced2c,0x4ced30,0x4ced34,0x4ced38,0x4ced3c,0x4ced44,0x4ced40,0x4ced4c,0x4ced48,0x4cf2d0,0x4cf2d4,0x4cee80,0x4cee84,0x4cee88,0x4cee8c,0x4cee94,0x4cee98,0x4cee9c,0x4ceea0,0x4ceea4,0x4cf2e0};
    r.insert(r.end(),source.begin(),source.end());r.insert(r.end(),target.begin(),target.end());std::sort(r.begin(),r.end());r.erase(std::unique(r.begin(),r.end()),r.end());return r;
}
template<class A> std::string hex(const A& a){std::ostringstream out;for(auto n:a)out<<std::hex<<std::setw(2)<<std::setfill('0')<<unsigned(n);return out.str();}
int stat_mode(){std::uint64_t ratio,count,cw;while(std::cin>>ratio>>count>>cw)std::cout<<dfre::mission_profile_stat(signed_word(U(ratio)),signed_word(U(count)),std::uint16_t(cw))<<'\n';return 0;}
int frame_mode(){
    const auto at=addresses();std::string line;while(std::getline(std::cin,line)){if(line.empty())continue;std::istringstream in(line);
        auto next=[&]{std::uint64_t n;if(!(in>>n))throw std::runtime_error("Short profile row");return U(n);};
        auto bytes=[&](std::size_t n){std::string s;if(!(in>>s)||s.size()!=2*n)throw std::runtime_error("Bad profile image");dfre::AssetBytes a(n);for(std::size_t i=0;i<n;++i)a[i]=std::uint8_t(std::stoul(s.substr(2*i,2),nullptr,16));return a;};
        const auto mode=next(),mutation=next(),cw=next();std::map<U,U> cells;for(auto p:at)cells[p]=next();auto profile=bytes(0xa90);auto alternate=profile;for(auto& n:alternate)n^=0xa5;put(alternate,0x28,word(profile,0x28)%5+1);bool alternate_current=false;auto raw=bytes(dfre::result_state_bytes);dfre::MissionResultImage result;std::copy(raw.begin(),raw.end(),result.begin());
        std::ostringstream trace;dfre::MissionProfileBindings b;b.cell=[&](U p){return p==0x67f718?word(result,4):cells.at(p);};b.store=[&](U p,U n){trace<<"S,"<<p<<','<<n<<';';cells.at(p)=n;if(p==0x4cf2e0&&(mutation&16))alternate_current=true;};b.profile=[&]()->dfre::AssetBytes&{return alternate_current?alternate:profile;};b.result=[&]{return result;};b.fp_control=[&]{return std::uint16_t(cw);};
        b.end_events=[&]{trace<<"E;";
            if(mutation&1){cells[0x67f520]^=1;put(result,4,word(result,4)==1?2:1);}
            if(mutation&2){const auto row=16-word(result,0xc);put(result,0xc,row);put(result,0x28+row*0x24,word(result,0x28+row*0x24)+17);put(result,0x30+row*0x24,word(result,0x30+row*0x24)^0x80010000u);}
            if(mutation&4){put(profile,0x28,word(profile,0x28)%5+1);cells[0x68ba34]=(cells[0x68ba34]+1)%16;put(profile,0x658,0xfffffff0);put(profile,0x674,0x7ffffff0);}
            if(mutation&8){cells[0x503134]^=0x80000000u;cells[0x503130]+=17;cells[0x67dbec]^=0xf0f0f0f0u;cells[0x67f4e4]^=0x12345678u;cells[0x67f564]^=0x87654321u;}
            cells[0x67f718]=word(result,4);
        };
        if(mode==0)dfre::mission_profile_record(b);else if(mode==1)dfre::mission_profile_settings(b);else dfre::mission_profile_leave(b);
        for(auto p:at)std::cout<<b.cell(p)<<' ';std::cout<<hex(profile)<<' '<<hex(alternate)<<' '<<alternate_current<<' '<<hex(result)<<' '<<trace.str()<<'\n';
    }return 0;
}
}
int main(int argc,char** argv){try{if(argc==2&&std::string(argv[1])=="--stat")return stat_mode();if(argc==2&&std::string(argv[1])=="--frame")return frame_mode();throw std::runtime_error("Use --stat or --frame");}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
