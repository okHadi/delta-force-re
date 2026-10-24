#include "dfre/mission_character_resource.hpp"
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <stdexcept>
using namespace dfre;using U=std::uint32_t;using Q=std::uint64_t;
int main(){try{std::string line;while(std::getline(std::cin,line)){
 U body,count,match,mutation,fail;std::string hex;std::istringstream input(line);if(!(input>>body>>count>>match>>mutation>>fail>>hex)||hex.size()%2)throw std::runtime_error("Short character resource fixture");
 constexpr U B=0x3000000,N=0x1200000,F=0x2007ff0,R=0x6ba440;std::map<U,std::uint8_t> m;for(U i=0;i<hex.size()/2+16;++i)m[B-16+i]=0;for(U i=0;i<hex.size()/2;++i)m[B+i]=std::uint8_t(std::stoul(hex.substr(i*2,2),nullptr,16));for(U i=0;i<0x1904;++i)m[R+i]=0;for(U i=0;i<128;++i)m[N+i]=0;for(U i=0;i<0x98;++i)m[F-0x80+i]=0;
 auto get=[&](U at,U width){U n=0;for(U i=0;i<width;++i)n|=U(m.at(at+i))<<(i*8);return n;};auto put=[&](U at,U width,U n){for(U i=0;i<width;++i)m.at(at+i)=std::uint8_t(n>>(i*8));};auto text=[&](U at,const std::string& s){for(U i=0;i<=s.size();++i)m.at(at+i)=i<s.size()?std::uint8_t(s[i]):0;};text(N,"fixture");text(N+64,"changed");put(F+0x14,4,N);put(0x6bbd40,4,count);put(R+0x60,4,B);for(U i=0;i<64;++i)text(R+i*100,i==match?"fixture":"other");
 Q trace=1469598103934665603ull;U events=0;std::set<U>written;bool active=true,prepared=false;auto hash=[&](Q& h,U n){for(U i=0;i<4;++i){h^=std::uint8_t(n>>(8*i));h*=1099511628211ull;}};auto step=[&]{if(++events==fail)throw std::runtime_error("Stopped");};MissionDebriefBindings b;
 b.read=[&](U at,U width){if(active)step();U n=get(at,width);if(active){hash(trace,'R');hash(trace,at);hash(trace,width);hash(trace,n);if((mutation&1)&&at==F+0x14)put(at,4,N+64);if((mutation&2)&&prepared&&at==B+0x38)put(body==2?F-8:F-0x7c,4,B+0x100);if((mutation&8)&&at==B+8&&!prepared)put(at,4,0);if((mutation&16)&&prepared&&at==B+0x1300)put((body==2?F-8:F-0x7c)+4,4,0);}return n;};
 b.write=[&](U at,U width,U n){if(active)step();put(at,width,n);if(active){hash(trace,'W');hash(trace,at);hash(trace,width);hash(trace,n);for(U i=0;i<width;++i)written.insert(at+i);if(at==(body==2?F-8:F-0x7c))prepared=true;}};
 b.call=[&](U site,const auto& args){if(active){step();hash(trace,'C');hash(trace,site);hash(trace,U(args.size()));for(U n:args)hash(trace,n);}if(site==0x499410){if((mutation&4)&&active)put(F-4,4,R+100);return B;}if(site==0x499e5e){U i=0;for(;;++i){auto a=get(args[0]+i,1),c=get(args[1]+i,1);if(a>='a'&&a<='z')a-=32;if(c>='a'&&c<='z')c-=32;if(a!=c)return 1u;if(!a)return 0u;}}if(site==0x456560){U i=0;while(get(args[0]+i,1)&&get(args[0]+i,1)!='.')++i;text(args[0]+i,".chr");return 0u;}throw std::runtime_error("Unbound character resource service");};
 if(body==2){active=false;mission_character_resource_relocate(b,N,R+0x60);active=true;}
 bool failed=false;U result=0;try{if(body==0)result=mission_character_resource_load(b,F);else if(body==1)mission_character_resource_relocate(b,N,R+0x60);else if(body==2)mission_character_resource_prepare(b,0,F-8);else result=mission_character_resource_label(b,B,N);}catch(const std::exception&){failed=true;result=0;}
 Q image=1469598103934665603ull;for(const auto& cell:m)if(cell.first<F-0x80||cell.first>=F+0x18||written.count(cell.first)){hash(image,cell.first);hash(image,cell.second);}std::cout<<failed<<' '<<result<<' '<<events<<' '<<trace<<' '<<image<<'\n';
}return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
