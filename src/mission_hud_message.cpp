#include "dfre/mission_hud_message.hpp"
#include <algorithm>
#include <stdexcept>
#include <string>
namespace dfre {namespace {
using U=std::uint32_t;
struct Access {
    const MissionHudMessageBindings& b;
    U read(U at,U width=4)const{if(!b.read)throw std::runtime_error("HUD message current read is not bound");const U n=b.read(at,width);return width==1?n&255u:width==2?n&65535u:n;}
    void write(U at,U n,U width=4)const{if(!b.write)throw std::runtime_error("HUD message current write is not bound");b.write(at,width,n);}
    U call(U site,const std::vector<U>& args)const{if(!b.call)throw std::runtime_error("HUD message reached body is not bound");return b.call(site,args);}
};
U scan(const Access& a,U pointer){U remaining=0xffffffffu;while(remaining){const U byte=a.read(pointer,1);++pointer;--remaining;if(!byte)break;}return ~remaining-1;}
}
void mission_hud_message(const MissionHudMessageBindings& b,const MissionHudMessageCall& call){
    const U text=call.text;Access a{b};U offset=0x1268;
    for(;;){U destination=0xbd352c+offset,source=0xbd34b0+offset;offset-=0x7c;for(U i=0;i<0x1f;++i){const U value=a.read(source);a.write(destination,value);source+=4;destination+=4;}if(offset==0xffffff84u)break;}
    a.call(0x49a620,{0xbd34b0,0,0x7c});const U second=a.read(0xbd4818);a.write(0xbd481c,second);const U first=a.read(0xbd4814);a.write(0xbd4818,first);a.write(0xbd4814,0x4d8);
    U count=scan(a,text);if(count<0x80000000u&&count>=0x78)count=0x77;
    if(count&&count<0x80000000u)for(U i=0;i<count;++i){const U value=a.read(text+i,1);a.write(0xbd34b0+i,value,1);}
    a.write(0xbd3528,call.metadata);
}
void mission_hud_message_clear(const MissionHudMessageBindings& b){Access a{b};a.call(0x49a620,{0xbd34b0,0,0x1360});a.write(0xbd4810,0);a.write(0xbd481c,0xffffffffu);a.write(0xbd4818,0xffffffffu);a.write(0xbd4814,0xffffffffu);}
U mission_hud_triggered_lookup(const MissionHudMessageBindings& b,U group,U key){Access a{b};const U table=a.read(0x503238);if(table){if(a.read(0x67dbc9,1)&0x20)return 0x4ced1a;const U entry=a.call(0x49a092,{table,group,key});if(entry){const U text=a.read(entry);if(text)return text;}}return 0x4ced1c;}
void mission_hud_triggered_text(const MissionHudMessageBindings& b,U id,U frame){Access a{b};a.call(0x493a80,{frame,0x4c82fc,id});const U text=a.call(0x404d80,{0x4c8304,frame});if(scan(a,text))a.call(0x479f00,{text,0xffffffffu});}
U mission_hud_message_format_id(const MissionHudMessageBindings& b,U destination,U format,U value){
    Access a{b};constexpr char expected[]="ID%03i";if(format!=0x4c82fc)throw std::runtime_error("HUD message format is not ID%03i");
    if(std::uint64_t(destination)<std::uint64_t(format)+sizeof(expected)&&std::uint64_t(destination)+14>format)throw std::runtime_error("HUD message format output overlaps its literal");
    for(U i=0;i<sizeof(expected);++i)if(a.read(format+i,1)!=U(expected[i]))throw std::runtime_error("HUD message format literal changed");
    const bool negative=(value&0x80000000u)!=0;U magnitude=negative?0u-value:value;std::string digits;
    do{digits.push_back(char('0'+magnitude%10));magnitude/=10;}while(magnitude);std::reverse(digits.begin(),digits.end());
    const U signed_length=U(digits.size())+U(negative);if(signed_length<3)digits.insert(0,3-signed_length,'0');const auto text=std::string("ID")+(negative?"-":"")+digits;
    for(U i=0;i<=text.size();++i)a.write(destination+i,i<text.size()?std::uint8_t(text[i]):0,1);return U(text.size());
}
} // namespace dfre
