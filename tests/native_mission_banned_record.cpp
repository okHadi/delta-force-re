#include "dfre/mission_banned_record.hpp"
#include <map>
#include <stdexcept>
namespace {void check(bool n){if(!n)throw std::runtime_error("Banned record check failed");}}
int main(){using U=std::uint32_t;std::map<U,std::uint8_t> bytes;dfre::MissionLineFileBindings b;
 b.read=[&](U at,U n){U value=0;for(U i=0;i<n;++i)value|=U(bytes.at(at+i))<<(8*i);return value;};b.write=[&](U at,U n,U value){for(U i=0;i<n;++i)bytes[at+i]=std::uint8_t(value>>(8*i));};
 const auto put=[&](U at,U n){b.write(at,4,n);};const auto text=[&](U at,const char* s){do{bytes[at++]=std::uint8_t(*s);}while(*s++);};
 put(0x6e5888,0);check(dfre::mission_banned_record(b,0,0x21000000)==0);check(b.read(0x21000000,4)==0);
 put(0x22000000,2);put(0x22000004,0x22000100);put(0x22000008,0x22000200);text(0x22000100,"192.168.1.2");text(0x22000200,"local");text(0x4cc6ec,"*");
 U calls=0;b.call=[&](U site,const std::vector<U>& a){check(site==0x49afa0&&a.size()==1);++calls;U cursor=a[0],value=0;while(bytes.at(cursor))value=value*10+bytes.at(cursor++)-'0';if(calls==4)put(0x6e5888,9);return value;};
 check(dfre::mission_banned_record(b,0x22000000,0x21000000)==0);check(calls==4&&b.read(0x6e5890,4)==0x0201a8c0&&b.read(0x6e5888,4)==10);check(bytes.at(0x6e6090)=='l'&&bytes.at(0x6e6095)==0);check(bytes.at(0x22000103)==0&&bytes.at(0x22000107)==0);
 put(0x6e5888,0);put(0x22000000,1);text(0x22000100,"1");calls=0;check(dfre::mission_banned_record(b,0x22000000,0x21000000)==0);check(bytes.at(0x6e6090)=='*'&&bytes.at(0x6e6091)==0);
}
