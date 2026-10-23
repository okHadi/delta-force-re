#include "dfre/mission_actor_insert.hpp"
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
namespace {
using U=std::uint32_t;
U hash(const std::map<U,std::uint8_t>& m){U h=2166136261u;for(const auto& p:m){h=(h^p.second)*16777619u;}return h;}
}
int main(){using namespace dfre;U mode,occupied,count,kind,flags,person,alias,mutation;
 while(std::cin>>mode>>occupied>>count>>kind>>flags>>person>>alias>>mutation){
  std::map<U,std::uint8_t> memory;const U base=mode?0x822e80:0x7dde80,count_at=mode?0x8cfa14:0x8cfa1c;
  const U source=alias==1?base+0x114*5:alias==2?base+0x114*2-4:alias==3?base+0x114*2+4:0x1200000;
  const auto put=[&](U at,U n,U value){for(U i=0;i<n;++i)memory[at+i]=std::uint8_t(value>>(i*8));};
  const auto get=[&](U at,U n){U value=0;for(U i=0;i<n;++i)value|=U(memory.at(at+i))<<(i*8);return value;};
  for(U i=0;i<0x45000;++i)memory[base+i]=0;
  for(U i=0;i<occupied;++i)put(base+i*0x114,4,i+1);
  for(U off=0;off<0x114;off+=4)put(source+off,4,0x10203040u^off);
  put(source,4,mutation&1?0:3);put(source+4,4,alias==2&&mode?0:0x1210000);put(0x1210028,4,kind);put(0x1210024,1,flags);put(count_at,4,count);
  std::ostringstream trace;MissionActorInsertBindings b;U writes=0;bool failed=false;
  b.read=[&](U at,U n){trace<<'R'<<at<<','<<n<<';';if((mutation&2)&&at==source+0x10)put(source+0x30,4,0xfedcba98);if((mutation&4)&&at==count_at)put(count_at,4,0xffffffff);return get(at,n);};
  b.write=[&](U at,U n,U value){trace<<'W'<<at<<','<<n<<','<<value<<';';if((mutation&16)&&++writes==14)throw std::runtime_error("store failure");put(at,n,value);if((mutation&8)&&at==base+occupied*0x114+0x20)put(source+0x40,4,0xabcdef01);};
  b.person=[&](U actor){trace<<'P'<<actor<<';';put(actor+0xc,4,0xf00df00d);put(count_at,4,0x12345678);if(mutation&32)throw std::runtime_error("person failure");return person;};
  U result=0;try{result=mission_actor_insert(b,source,mode);}catch(const std::exception&){failed=true;}
  std::cout<<failed<<' '<<result<<' '<<get(count_at,4)<<' '<<hash(memory)<<' '<<trace.str()<<'\n';
 }
}
