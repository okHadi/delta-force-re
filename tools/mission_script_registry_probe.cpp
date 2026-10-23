#include "dfre/mission_script_registry.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
using U=std::uint32_t;using Bytes=std::vector<std::uint8_t>;
namespace {
U word(const Bytes& b,U at,U n=4){U v=0;for(U i=0;i<n;++i)v|=U(b.at(at+i))<<(8*i);return v;}
unsigned digit(char c){if(c>='0'&&c<='9')return unsigned(c-'0');if(c>='a'&&c<='f')return unsigned(c-'a'+10);throw std::runtime_error("Invalid hex");}
Bytes decode(const std::string& s){if(s=="-")return {};Bytes b;for(std::size_t i=0;i<s.size();i+=2)b.push_back(std::uint8_t(digit(s.at(i))*16+digit(s.at(i+1))));return b;}
struct Memory {
 std::map<U,Bytes> blocks;std::vector<U> watched;std::string calls,access;Bytes file;U offset=0,next=0,fail=0,mutation=0,reads=0,thread_calls=0,volume_reads=0;bool trace=false;
 std::uint8_t* pointer(U at,U n){auto it=blocks.upper_bound(at);if(it==blocks.begin())throw std::runtime_error("Unmapped registry byte");--it;if(std::uint64_t(at-it->first)+n>it->second.size())throw std::runtime_error("Registry span exceeded");return it->second.data()+at-it->first;}
 U read(U at,U n){auto* p=pointer(at,n);U v=0;for(U i=0;i<n;++i)v|=U(p[i])<<(8*i);return v;}
 void write(U at,U n,U v){auto* p=pointer(at,n);for(U i=0;i<n;++i)p[i]=std::uint8_t(v>>(8*i));}
 void text(U at,const std::string& s){auto* p=pointer(at,U(s.size()+1));std::copy(s.begin(),s.end(),p);p[s.size()]=0;}
 std::string text(U at){std::string s;while(auto c=read(at++,1))s+=char(c);return s;}
 U call(U site,const std::vector<U>& a){calls+=std::to_string(site);for(U v:a)calls+=","+std::to_string(v);calls+=";";
  switch(site){case 0x49a620:{auto* p=pointer(a.at(0),a.at(2));std::fill_n(p,a[2],std::uint8_t(a[1]));return a[0];}
  case 0x497f60:{++next;if(next==fail)return 0;const U p=0x2300000+next*0x1000;blocks[p]=Bytes(std::max<U>(a[0],1),0);watched.push_back(p);if((mutation&1)&&next==1)write(0x8e3d58,4,0x2208000);return p;}
  case 0x499120:offset=0;return mutation&2?0xffffffffu:0;
  case 0x499280:{auto* p=pointer(a[0],a[1]);if(std::uint64_t(offset)+a[1]>file.size())throw std::runtime_error("Short registry file");std::copy_n(file.data()+offset,a[1],p);offset+=a[1];++reads;if((mutation&4)&&reads==3)write(0x8e3d54,4,1);return a[1];}
  case 0x4992d0:offset=a[1]?offset+a[0]:a[0];return offset;
  case 0x499020:return 0;
  case 0x49ba00:{auto stem=text(a[0]);const auto dot=stem.find('.');if(dot!=std::string::npos)write(a[0]+U(dot),1,0);return stem.empty()?0:a[0];}
  case 0x493a80:{const auto s=text(a[2])+(a[1]==0x4cca6c?".pwf":".bin");text(a[0],s);return U(s.size());}
  case 0x49cf50:if(mutation&8)text(0x2201000,"changed");return 0;
  case 0x499f1f:return 0x2207000;
  case 0x4980d0:if((mutation&16)&&a[0]==0x2209000)write(0x2208004,4,0);return 0;
  case 0x49db20:case 0x495160:case 0x479f00:return 0;
  case 0x501d60:return 0x2206000+(((mutation&512)&&thread_calls++!=0)?32:0);
  case 0x49cfd0:return mutation&32?0:0x2209000;
  case 0x49d0e0:write(a[0]+0x23,1,1);write(a[0]+0x24,4,0x220a000);return 0;
  case 0x498230:if(mutation&64)write(0x2209024,4,0x220b000);return a[0]-16;
  default:throw std::runtime_error("Unknown registry fixture call");
  }
 }
};
}
int main(int argc,char** argv){try{if(argc!=2)throw std::runtime_error("Supply original executable");std::ifstream f(argv[1],std::ios::binary);Bytes source((std::istreambuf_iterator<char>(f)),{});const U pe=word(source,0x3c),opt=pe+24,table=opt+word(source,pe+20,2);Bytes image(word(source,opt+56));for(U i=0;i<word(source,pe+6,2);++i){const auto at=table+i*40;if(word(source,at+36)&0x80u)continue;const auto offset=word(source,at+20),size=word(source,at+16),rva=word(source,at+12);if(std::uint64_t(offset)+size>source.size()||std::uint64_t(rva)+size>image.size())throw std::runtime_error("Invalid executable section");std::copy_n(source.begin()+offset,size,image.begin()+rva);}
 std::string line;while(std::getline(std::cin,line)){std::istringstream in(line);U mode,arg1,arg2,mutation,fail,count;in>>mode>>arg1>>arg2>>mutation>>fail>>count;Memory m;m.blocks[0x400000]=image;m.mutation=mutation;m.fail=fail;for(U i=0;i<count;++i){U at;std::string hex;in>>at>>hex;m.blocks[at]=decode(hex);m.watched.push_back(at);}std::string file;in>>file;if(!in)throw std::runtime_error("Incomplete registry case");m.file=decode(file);dfre::MissionSoundBankBindings b{[&](U at,U n){const auto v=m.read(at,n);m.access+="R,"+std::to_string(at)+","+std::to_string(n)+","+std::to_string(v)+";";if(at==0x67f51c&&(m.mutation&128)&&m.volume_reads++==0)m.write(at,4,0xffffff80u);if(at==arg1&&(m.mutation&256)&&m.volume_reads++==0)m.write(arg2,1,m.read(arg2,1)^1);return v;},[&](U at,U n,U v){m.access+="W,"+std::to_string(at)+","+std::to_string(n)+","+std::to_string(v)+";";m.write(at,n,v);},[&](U site,const std::vector<U>& a){return m.call(site,a);}};U result=0,error=0;
 try{switch(mode){case 0:result=dfre::mission_script_registry_load(b,arg1,arg2);break;case 1:result=dfre::mission_script_registry_node(b);break;case 2:dfre::mission_script_registry_destroy(b);break;case 3:result=dfre::mission_script_registry_token(b,arg1,arg2,0x2204000);break;case 4:result=U(dfre::mission_script_registry_compare(b,arg1,arg2));break;case 5:result=dfre::mission_script_registry_id(b,arg1);break;case 6:result=dfre::mission_script_registry_step(b,arg1);break;case 7:dfre::mission_script_registry_text(b,arg1);break;case 8:result=dfre::mission_script_registry_audio(b,arg1,arg2);break;case 9:dfre::mission_script_registry_audio_destroy(b,arg1);break;default:throw std::runtime_error("Unknown mode");}}catch(const std::exception&){error=1;}
 std::uint64_t hash=1469598103934665603ull;for(U at:m.watched){for(auto c:m.blocks.at(at)){hash^=c;hash*=1099511628211ull;}}std::uint64_t access_hash=1469598103934665603ull;for(unsigned char c:m.access){access_hash^=c;access_hash*=1099511628211ull;}std::cout<<result<<' '<<error<<' '<<hash<<' '<<access_hash<<' '<<(m.calls.empty()?"-":m.calls)<<'\n';}
 return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
