#include "dfre/mission_hud_message.hpp"
#include <array>
#include <iostream>
#include <map>
#include <string>
using U=std::uint32_t;using Q=std::uint64_t;
struct Fixture {
 std::map<U,std::uint8_t> memory;U mode,seed,mutation,reads=0,writes=0;Q hash=1469598103934665603ull;bool tracing=true;dfre::MissionHudMessageCall* current=nullptr;dfre::MissionHudMessageBindings b;
 U get(U at,U width=4){U n=0;for(U i=0;i<width;++i)n|=U(memory[at+i])<<(i*8);return n;}
 void put(U at,U n,U width=4){for(U i=0;i<width;++i)memory[at+i]=std::uint8_t(n>>(i*8));}
 void word(Q n){for(unsigned i=0;i<8;++i){hash^=std::uint8_t(n>>(8*i));hash*=1099511628211ull;}}
 void event(U kind,U at,U width,U value){word(kind);word(at);word(width);word(value);}
 void text(U at,const std::string& s){for(U i=0;i<=s.size();++i)put(at+i,i<s.size()?std::uint8_t(s[i]):0,1);}
 explicit Fixture(U m,U s,U x):mode(m),seed(s),mutation(x){
  for(U i=0;i<0x1360;++i)put(0xbd34b0+i,(i*37+s)&255,1);for(U i=0;i<40;++i)put(0xbd34b0+i*0x7c+119,0,1);
  put(0xbd4810,s);put(0xbd4814,s+1);put(0xbd4818,s+2);put(0xbd481c,s+3);put(0x503238,s&1?0x22000000:0);put(0x67dbc9,s&2?0x20:0,1);put(0x1303000,s&4?0:0x1302000);
  text(0x1302000,std::string(s%160,'a'+s%26));text(0x4c82fc,"ID%03i");text(0x4c8304,"Triggered Text");text(0x4ced1a,"&");text(0x4ced1c,"");for(U i=0;i<32;++i){put(0x2008000-0x24+i,0xa5,1);put(0x1304000+i,0xa5,1);}
  b.read=[&](U at,U width){const U n=get(at,width);if(tracing){event(1,at,width,n);if(++reads==7&&(mutation&16)){put(0x1302032,'X',1);if(current)current->metadata^=0x100;}}return n;};
  b.write=[&](U at,U width,U n){if(tracing){event(2,at,width,n);if(++writes==7&&(mutation&32))put(0xbd34b0,0x41424344);}put(at,n,width);};
  b.call=[&](U site,const std::vector<U>& args)->U{word(3);word(site);word(args.size());for(U n:args)word(n);
   if(site==0x49a620){for(U i=0;i<args.at(2);++i)put(args.at(0)+i,args.at(1)&255,1);if(mutation&1){if(current){current->metadata^=0x80;current->text=0x1302001;}put(0xbd4814,get(0xbd4814)+5);}if(mutation&2)put(0xbd34b0,'Z',1);return args.at(0);}
   if(site==0x49a092){if(mutation&4){put(0x503238,0);put(0x1303000,0x1302001);}return seed&8?0:0x1303000;}
   if(site==0x404d80)return dfre::mission_hud_triggered_lookup(b,args.at(0),args.at(1));
   if(site==0x479f00){dfre::MissionHudMessageCall call{args.at(0),args.at(1)};auto* prior=current;current=&call;dfre::mission_hud_message(b,call);current=prior;return 0;}
   if(site==0x493a80){const bool old=tracing;tracing=false;const U n=dfre::mission_hud_message_format_id(b,args.at(0),args.at(1),args.at(2));tracing=old;return n;}
   throw std::runtime_error("Unbound HUD fixture call");};
 }
 std::string hex(U at,U size){const char* digits="0123456789abcdef";std::string s;for(U i=0;i<size;++i){const auto n=get(at+i,1);s+=digits[n>>4];s+=digits[n&15];}return s;}
 void run(){U returned=0;dfre::MissionHudMessageCall call{seed&0x100?0xbd34b0+(seed%4)*0x7c:0x1302000,0xfeed0000|seed};current=&call;
  if(mode==0)dfre::mission_hud_message(b,call);else if(mode==1)dfre::mission_hud_message_clear(b);else if(mode==2)returned=dfre::mission_hud_triggered_lookup(b,0x4c8304,0x1304000);else if(mode==3)dfre::mission_hud_triggered_text(b,seed,0x2008000-0x24);else {tracing=false;returned=dfre::mission_hud_message_format_id(b,0x1304000,0x4c82fc,seed);hash=0;}
  std::cout<<returned<<' '<<(mode==0?call.text:0)<<' '<<(mode==0?call.metadata:0)<<' '<<get(0x503238)<<' '<<get(0x67dbc9,1)<<' '<<hash<<' '<<hex(0xbd34b0,0x1370)<<' '<<hex(mode==3?0x2008000-0x24:0x1304000,32)<<'\n';
 }
};
int main(){U mode,seed,mutation;try{while(std::cin>>mode>>seed>>mutation)Fixture(mode,seed,mutation).run();}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
