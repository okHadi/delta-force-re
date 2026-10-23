#include "dfre/mission_event_person.hpp"
#include <cstring>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
using U=std::uint32_t;
namespace {
constexpr U action=0x2201000,person=0x794680;
constexpr std::array<U,3> bases{0x867e80,0x7cca80,0x7dde80},counts{0x8cf904,0x8cfa18,0x8cfa1c};
unsigned digit(char c){if(c>='0'&&c<='9')return unsigned(c-'0');if(c>='a'&&c<='f')return unsigned(c-'a'+10);throw std::runtime_error("invalid hex image");}
struct Memory {
 std::map<U,std::uint8_t> bytes;std::map<U,U> reads;U mutation=0;std::string trace;
 U raw(U at,U n){U value=0;for(U i=0;i<n;++i)value|=U(bytes.at(at+i))<<(i*8);return value;}
 void put(U at,U n,U value){for(U i=0;i<n;++i)bytes.at(at+i)=std::uint8_t(value>>(i*8));}
 void load(U at,const std::string& hex,std::size_t size){if(hex.size()!=size*2)throw std::runtime_error("incomplete person image");for(std::size_t i=0;i<size;++i)bytes[at+U(i)]=std::uint8_t(digit(hex[i*2])*16+digit(hex[i*2+1]));}
 std::string image(U at,U size){std::string result;static constexpr char chars[]="0123456789abcdef";for(U i=0;i<size;++i){const auto v=bytes.at(at+i);result+=chars[v>>4];result+=chars[v&15];}return result;}
 U read(U at,U n){
  const auto occurrence=++reads[at];
  if((mutation&1)&&at==action+0x10&&occurrence==2)put(at,4,0);
  if((mutation&2)&&at>=0x4fb2cc&&at<0x4fb30c){put(action+8,4,2);put(action+0x10,4,0);put(bases[0]+0xc,4,person+0xb4);}
  if((mutation&4)&&at==bases[0]+0xc&&occurrence==2)put(at,4,person+0xb4);
  if((mutation&8)&&at==counts[0]&&occurrence==2)put(at,4,1);
  const auto value=raw(at,n);trace+="R,"+std::to_string(at)+","+std::to_string(n)+","+std::to_string(value)+";";return value;
 }
 void write(U at,U n,U value){put(at,n,value);trace+="W,"+std::to_string(at)+","+std::to_string(n)+","+std::to_string(value)+";";}
};
}
int main(int argc,char** argv){try{
 if(argc==2&&std::string(argv[1])=="rate"){U value,control;while(std::cin>>value>>control){std::int32_t signed_value;std::memcpy(&signed_value,&value,4);std::cout<<dfre::mission_event_person_rate(signed_value,std::uint16_t(control))<<'\n';}return 0;}
 if(argc!=2||std::string(argv[1])!="body")throw std::runtime_error("unknown person probe mode");
 std::string line;while(std::getline(std::cin,line)){
  std::istringstream row(line);U mode,mutation,control;row>>mode>>mutation>>control;Memory memory;memory.mutation=mutation;
  for(auto at:counts){U value;row>>value;for(U i=0;i<4;++i)memory.bytes[at+i]=std::uint8_t(value>>(i*8));}
  std::string hex;row>>hex;memory.load(action,hex,32);
  for(auto base:bases)for(U slot=0;slot<3;++slot){row>>hex;memory.load(base+slot*0x114,hex,0x114);}
  for(U slot=0;slot<3;++slot){row>>hex;memory.load(person+slot*0xb4,hex,0xb4);}
  if(!row)throw std::runtime_error("incomplete person case");
  for(U i=0;i<16;++i)for(U n=0;n<4;++n)memory.bytes[0x4fb2cc+i*4+n]=std::uint8_t((0x12345678u^(i*0x112233u))>>(n*8));
  dfre::MissionEventPersonBindings b;b.read=[&](U at,U n){return memory.read(at,n);};b.write=[&](U at,U n,U value){memory.write(at,n,value);};b.fp_control=[control]{return std::uint16_t(control);};
  if(mode==0)dfre::mission_event_person_edit(b,action,bases[0]);else dfre::mission_event_person_edits(b,action);
  for(auto at:counts)std::cout<<memory.raw(at,4)<<' ';std::cout<<memory.image(action,32);
  for(auto base:bases)for(U slot=0;slot<3;++slot)std::cout<<' '<<memory.image(base+slot*0x114,0x114);
  for(U slot=0;slot<3;++slot)std::cout<<' '<<memory.image(person+slot*0xb4,0xb4);
  std::cout<<' '<<memory.trace<<'\n';
 }return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
