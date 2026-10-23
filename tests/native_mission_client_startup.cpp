#include "dfre/mission_client_startup.hpp"
#include <cassert>
#include <map>
#include <string>
using namespace dfre;
namespace {
using U=std::uint32_t;
struct Memory {std::map<U,std::uint8_t> bytes;void put(U a,U width,U n){for(U i=0;i<width;++i)bytes[a+i]=std::uint8_t(n>>(8*i));}U get(U a,U width=4){U n=0;for(U i=0;i<width;++i)n|=U(bytes[a+i])<<(8*i);return n;}void text(U a,const std::string& s){for(U i=0;i<=s.size();++i)put(a+i,1,i<s.size()?std::uint8_t(s[i]):0);}MissionClientStartupBindings binding(){MissionClientStartupBindings b;b.read=[this](auto a,auto w){if(a<0x1000)throw MissionClientStartupError("Read outside memory");return get(a,w);};b.write=[this](auto a,auto w,auto n){if(a<0x1000)throw MissionClientStartupError("Write outside memory");put(a,w,n);};return b;}};
}
int main(){
 {Memory m;auto b=m.binding();m.put(0x6e9a98,4,0x1200000);auto original=b.write;b.write=[&](auto a,auto w,auto n){original(a,w,n);if(a==0x6e9b2c)m.put(0x6e9a98,4,0x1200114);};mission_client_assign_team(b,0x6e9a90,0);assert(m.get(0x6e9b2c)==1&&m.get(0x1200070,2)==1&&m.get(0x1200072,2)==0&&m.get(0x1200186,2)==1);}
 {Memory m;auto b=m.binding();m.put(0x6e9a98,4,0x1200000);auto original=b.write;b.write=[&](auto a,auto w,auto n){original(a,w,n);if(a==0x1200070)m.put(0x6e9a98,4,0);};bool failed=false;try{mission_client_assign_team(b,0x6e9a90,0);}catch(const MissionClientStartupError&){failed=true;}assert(failed&&m.get(0x6e9b2c)==1&&m.get(0x1200070,2)==1);}
 {Memory m;auto b=m.binding();m.put(0x67f520,4,1);m.put(0x67f53c,4,3);m.put(0x6e9a98+16*0x9b84,4,0x1200000);mission_client_assign_team(b,0x6e9a90+16*0x9b84,2);assert(m.get(0x6e9b2c+16*0x9b84)==19&&m.get(0x1200070,2)==19&&m.get(0x1200072,2)==0);}
 {Memory m;auto b=m.binding();m.put(0xbc5c6c,4,0x12345678);mission_client_register_input(b,0xbc5c6c);assert(m.get(0xbc5c70)==0x12345678&&m.get(0xbc5c74)==0x12345678&&m.get(0xbc5c78,2)==0x5678);
  for(U i=0;i<16;++i){m.put(0xbc5c70+i*10,1,i+1);m.put(0xbc5c71+i*10,1,1);}m.put(0x1300000,4,255);const auto before=m.bytes;mission_client_register_input(b,0x1300000);assert(m.get(0xbc5c70,1)==1&&m.get(0xbc5c70+15*10,1)==16);(void)before;}
 {Memory m;auto b=m.binding();m.text(0x1300000,"Local");m.put(0x50133c,4,77);std::vector<U> writes;auto original=b.write;b.write=[&](auto a,auto w,auto n){writes.push_back(a);original(a,w,n);};mission_client_setup_local(b,0x1300000,8,2);assert(m.get(0x6e9a90)==77&&m.get(0x6e9b30)==8&&m.get(0x6e9b34)==2);assert(writes[writes.size()-3]==0x6e9b30&&writes[writes.size()-2]==0x6e9b2c);}
 {Memory m;auto b=m.binding();m.text(0x4cc1e5,"nw2.novaworld.net");m.text(0x4cc1f8,"test");m.text(0x1300000,"Server");std::vector<U> calls;for(U i=0;i<6;++i){m.put(0x4f81b0+i*16,4,(1u<<8)|(i+1));m.put(0x4f81b4+i*16,4,i+50);m.put(0x4f81b8+i*16,2,i+100);}m.put(0x6e9a90+17*0x9b84-1,1,255);
  b.call=[&](auto at,const auto& args)->U{calls.push_back(at);if(at==0x49a620){assert(args.at(0)==0x6e9a90&&args.at(1)==0&&args.at(2)==17*0x9b84);for(auto& byte:m.bytes)if(U(byte.first-args[0])<args[2])byte.second=0;return args[0];}if(at==0x404cf0)return 0x1300000;if(at==0x454df0)return 0x12345678;if(at==0x454bb0){assert((args==std::vector<U>{2,0,0xf28}));return 0;}assert(at==0x4568e0&&args.at(0)==0x4cc6f0&&args.at(1)==0x448690);return 1;};mission_client_startup(b);
  assert(m.get(0x6e9a90+17*0x9b84-1,1)==0&&m.get(0x6e9a98)==0&&m.get(0x6e9c1c,1)==1&&m.get(0x6e9c1d,1)==1&&m.get(0x6e9c20,1)==3&&m.get(0x6e9c48)==1&&m.get(0x6e9b2c)==1&&m.get(0x6e5574)==0xd7);assert(m.get(0x501374)==0x43f830&&m.get(0x501378)==0x43fd60&&m.get(0x50137c)==0x43fef0&&m.get(0x501380)==0x43ff20);assert(m.get(0xbc5c70+5*10,1)==6&&m.get(0x6e573c+4,1)=='P'&&m.get(0x6e573c+5,1)==0);assert(calls.size()==5);
 }
 {Memory m;auto b=m.binding();bool failed=false;try{mission_client_reset_banned(b);}catch(const MissionClientStartupError&){failed=true;}assert(failed&&m.get(0x6e5888)==0&&m.get(0x6e588c)==0);failed=false;try{mission_client_register_callback(b,0,77);}catch(const MissionClientStartupError&){failed=true;}assert(failed);}
}
