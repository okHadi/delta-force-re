#include "dfre/mission_line_file.hpp"
#include <array>
#include <stdexcept>
namespace {void check(bool ok){if(!ok)throw std::runtime_error("Line file check failed");}template<class F>void rejects(F f){bool failed=false;try{f();}catch(const std::exception&){failed=true;}check(failed);}}
int main(){using U=std::uint32_t;constexpr U frame=0x21000000;std::array<std::uint8_t,0x400> bytes{};
 const auto get=[&](U at,U w){check(at>=frame&&std::uint64_t(at-frame)+w<=bytes.size());U n=0;for(U i=0;i<w;++i)n|=U(bytes.at(at-frame+i))<<(8*i);return n;};
 const auto put=[&](U at,U w,U n){check(at>=frame&&std::uint64_t(at-frame)+w<=bytes.size());for(U i=0;i<w;++i)bytes.at(at-frame+i)=std::uint8_t(n>>(i*8));};
 dfre::MissionLineFileBindings b;b.read=get;b.write=put;put(frame+0x3d4,4,13);put(frame+0x3d8,4,31);
 bool open=false,malformed=false;U reads=0,callbacks=0,closes=0,tokens=0,current_file=7;
 b.call=[&](U site,const std::vector<U>& a){if(site==0x49a41c){check(a==std::vector<U>({13,0x4ccac4}));return open?7u:0u;}
  if(site==0x49a550){check(a==std::vector<U>({frame,700,current_file}));if(++reads==2)return 0u;put(frame,1,'a');put(frame+1,1,malformed?0:10);put(frame+2,1,0);return frame;}
  if(site==0x49a840){check(a==std::vector<U>({current_file}));++closes;return 0u;}
  ++callbacks;if(callbacks==1){check(site==31&&a==std::vector<U>({frame+0x340,1}));check(get(frame+0x340,4)==2&&get(frame+0x340+120,4)==30);put(frame+0x3bc,4,19);current_file=19;put(frame+0x3d8,4,37);return 0u;}
  check(site==37&&a==std::vector<U>({0,2}));return 91u;};
 b.tokenize=[&](U line,U out){check(line==frame&&out==frame+0x2c4&&get(frame+1,1)==0);++tokens;for(U i=0;i<31;++i)put(out+i*4,4,i==0?2:i==1?frame:i);};
 check(dfre::mission_line_file(b,frame)==1&&reads==0&&callbacks==0&&closes==0);open=true;
 check(dfre::mission_line_file(b,frame)==91&&callbacks==2&&closes==1&&tokens==1);
 reads=callbacks=closes=tokens=0;current_file=7;put(frame+0x3d8,4,31);malformed=true;
 rejects([&]{dfre::mission_line_file(b,frame);});check(reads==1&&callbacks==0&&closes==0&&tokens==0);
 rejects([&]{dfre::mission_line_file({},frame);});
}
