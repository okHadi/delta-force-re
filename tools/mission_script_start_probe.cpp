#include "dfre/mission_script_start.hpp"
#include <cstring>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
using U=std::uint32_t;
namespace {
constexpr U base=0x8e3d74,count=0x8e3d70,nodebase=0x2202000;
unsigned digit(char c){if(c>='0'&&c<='9')return unsigned(c-'0');if(c>='a'&&c<='f')return unsigned(c-'a'+10);throw std::runtime_error("bad script hex");}
std::string hextext(std::string_view text){std::string out;static constexpr char digits[]="0123456789abcdef";for(unsigned char c:text){out+=digits[c>>4];out+=digits[c&15];}return out;}
struct Memory {
 std::map<U,std::uint8_t> bytes;U mutation=0,match=0,compares=0;std::string trace;
 void load(U at,const std::string& hex,U size){if(hex.size()!=size*2)throw std::runtime_error("incomplete script image");for(U i=0;i<size;++i)bytes[at+i]=std::uint8_t(digit(hex[i*2])*16+digit(hex[i*2+1]));}
 U raw(U at){U value=0;for(U i=0;i<4;++i)value|=U(bytes.at(at+i))<<(i*8);return value;}
 void put(U at,U value){for(U i=0;i<4;++i)bytes.at(at+i)=std::uint8_t(value>>(i*8));}
 std::string image(U at,U size){std::string text;for(U i=0;i<size;++i)text+=char(bytes.at(at+i));return hextext(text);}
 U read(U at){const auto value=raw(at);trace+="R,"+std::to_string(at)+","+std::to_string(value)+";";return value;}
 void write(U at,U value){put(at,value);trace+="W,"+std::to_string(at)+","+std::to_string(value)+";";}
 void mutate(){if(compares++!=0)return;if(mutation&1)put(count,1);if(mutation&2)put(nodebase,0x2207000);if(mutation&4)put(nodebase+4,nodebase+16);if(mutation&8)put(base+15*16+12,0xdeadbeef);}
};
}
int main(){try{std::string line;while(std::getline(std::cin,line)){
 std::istringstream input(line);U mode,arg,match,mutation,active;std::string queuehex,nodeshex;input>>mode>>arg>>match>>mutation>>active>>queuehex>>nodeshex;if(!input)throw std::runtime_error("bad script-start case");Memory m;m.match=match;m.mutation=mutation;m.load(base,queuehex,256);m.load(nodebase,nodeshex,24);for(auto at:{count,0x8e3d58u})for(U i=0;i<4;++i)m.bytes[at+i]=0;m.put(count,active);m.put(0x8e3d58,nodebase);
 dfre::MissionScriptQueueBindings raw;raw.read=[&](U at){return m.read(at);};raw.write=[&](U at,U value){m.write(at,value);};raw.zero16=[&](U at){m.trace+="Z,"+std::to_string(at)+";";for(U i=0;i<16;i+=4)m.put(at+i,0);};raw.compare=[&](U left,U right){const auto comparison=m.compares;m.trace+="C,"+std::to_string(left)+","+std::to_string(right)+";";m.mutate();return comparison==m.match?0:1;};U result=0;
 if(mode==0)result=dfre::mission_script_queue_start(raw,arg);
 else if(mode==1)dfre::mission_script_queue_compact(raw);
 else if(mode==2)result=dfre::mission_script_queue_remove(raw,arg);
 else if(mode==3)result=dfre::mission_script_registry_start(raw,arg);
 else {
  if(active>16)throw std::runtime_error("bad initialized queue count");dfre::MissionScriptQueueCells queue{};for(U i=0;i<active;++i)queue.entries_8e3d74.push_back({m.raw(base+i*16),m.raw(base+i*16+4),m.raw(base+i*16+8),m.raw(base+i*16+12)});
  dfre::MissionScriptStartBindings b;b.registry_head=[&](){m.trace+="H;";return m.raw(0x8e3d58);};b.node_word=[&](U node,U offset){const auto value=m.raw(node+offset);m.trace+="N,"+std::to_string(node)+","+std::to_string(offset)+","+std::to_string(value)+";";return value;};b.compare=[&](U left,const dfre::MissionScriptStartName& name){const auto comparison=m.compares;if(const auto* text=std::get_if<std::string_view>(&name))m.trace+="T,"+std::to_string(left)+","+hextext(*text)+";";else m.trace+="C,"+std::to_string(left)+","+std::to_string(std::get<U>(name))+";";m.mutate();return comparison==m.match?0:1;};dfre::MissionScriptStart start(queue,b);
  if(mode==4)result=start.start(arg);else if(mode==5){std::int32_t id;std::memcpy(&id,&arg,4);start.event(id);}else if(mode==6)result=start.start_name(arg);else if(mode==7){U host=0;dfre::MissionScriptRuntimeCells cells{queue,host};dfre::MissionScriptRuntimeBindings runtime_bindings;runtime_bindings.string=[](U at){return std::to_string(at);};dfre::MissionScriptRuntime runtime(cells,runtime_bindings);result=runtime.remove(arg);}else throw std::runtime_error("unknown script-start mode");
  m.put(count,U(queue.entries_8e3d74.size()));for(U i=0;i<16;++i){dfre::MissionScriptQueueEntry entry{};if(i<queue.entries_8e3d74.size())entry=queue.entries_8e3d74[i];for(U field=0;field<4;++field){U value=0;std::memcpy(&value,reinterpret_cast<const std::uint8_t*>(&entry)+field*4,4);m.put(base+i*16+field*4,value);}}
 }
 std::cout<<result<<' '<<m.raw(count)<<' '<<m.image(base,256)<<' '<<m.image(nodebase,24)<<' '<<(m.trace.empty()?"-":m.trace)<<'\n';
}return 0;}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
