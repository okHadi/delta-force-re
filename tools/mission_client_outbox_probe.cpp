#include "dfre/mission_client_outbox.hpp"
#include <iostream>
#include <sstream>
using namespace dfre;
namespace {
void put(std::uint8_t* b,unsigned at,std::uint32_t value,unsigned width=4){for(unsigned i=0;i<width;++i)b[at+i]=std::uint8_t(value>>(i*8));}
std::string hex(const std::uint8_t* b,std::size_t n){const char* d="0123456789abcdef";std::string r;for(std::size_t i=0;i<n;++i){r+=d[b[i]>>4];r+=d[b[i]&15];}return r;}
constexpr std::uint32_t sequences[]={0,32000,32001,0xffffffff,0x7fffffff,0x80000000};
constexpr std::uint16_t delays[]={0,1,0x7fff,0x8000,0xffff};
constexpr std::uint16_t repeats[]={0,1,2,3,0x7fff,0x8000,0x8001,0xffff};
}
int main(){std::string line;while(std::getline(std::cin,line)){try{
 std::istringstream input(line);std::string op;unsigned seed;input>>op>>seed;
 GameMessageStage messages;MissionServerMessageBuffer buffer;buffer.bytes_8e61c0.fill(0xcc);
 std::vector<std::array<std::uint8_t,mission_client_record_size>> clients(17);
 for(unsigned slot=0;slot<2;++slot){auto* c=clients[slot].data();clients[slot].fill(0x5a);for(unsigned i=0;i<64;++i)c[0x204+i*0x264+9]=0;
  c[0x18c]=1;c[0x18d]=std::uint8_t(slot&&(seed&1));put(c,0,slot?7:8);put(c,0x1c4,5,2);put(c,0x1b8,sequences[seed%6]);put(c,0x6c,10+seed);put(c,0x7c,20+seed);
 }
 std::vector<std::uint8_t> echo((op=="flush"||op=="tick")?std::array<unsigned,4>{0,8187,8190,8191}[seed%4]:0,0xee);if(!echo.empty())messages.submit_local_echo(echo.data(),echo.size());
 std::vector<std::vector<std::uint32_t>> calls;std::vector<std::string> packets;
 MissionClientOutboxBindings b;b.client=[&](auto slot){return MissionClientRecordView{clients.at(slot).data(),clients.at(slot).size()};};
 b.cell=[&](auto address){return address==0x50321c?std::uint32_t(delays[seed%5]):7u;};
 b.transport_4977e0=[&](auto* bytes,auto size,auto peer){calls.push_back({0x4977e0,size,peer});packets.push_back(hex(bytes,size));if(seed&1){buffer.count_8e81c0=33;put(clients[0].data(),0x6c,100);put(clients[0].data(),0x7c,200);clients[1][0x18c]=0;clients[0][0x204+5*0x264+9]=0;}};
 b.diagnostic_4564b0=[&](auto id){calls.push_back({0x4564b0,std::uint32_t(id)});if(seed&1){std::array<std::uint8_t,128> patch{};patch[0xa*4+1]=2;messages.table().patch(0,patch.data());buffer.count_8e81c0=2;}};
 MissionClientOutbox outbox(messages,buffer,b);std::array<std::uint8_t,16> payload{};for(unsigned i=0;i<16;++i)payload[i]=std::uint8_t(seed+i*7);
 std::uint32_t result=0;
 if(op=="enqueue")result=outbox.enqueue(0,0xa,payload.data(),16);
 else if(op=="broadcast")outbox.broadcast(0xa,payload.data(),16);
 else if(op=="tick"){
  for(unsigned slot=0;slot<2;++slot)for(unsigned i=0;i<3;++i){const unsigned index=std::array<unsigned,3>{0,2,5}[i],at=0x204+index*0x264;auto* c=clients[slot].data();
   const auto id=std::array<unsigned,3>{0xa,0x1a,0x30}[i];put(c,at,7+i,2);put(c,at+2,repeats[(seed+i)%8],2);put(c,at+4,messages.table().descriptor(std::uint8_t(id)).length,2);put(c,at+6,delays[(seed+i)%5],2);c[at+8]=std::uint8_t(id);c[at+9]=1;
  }
  outbox.tick();
 }else if(op=="append"){
  buffer.count_8e81c0=std::array<unsigned,3>{0,4,8175}[seed%3];
  outbox.append(0xa,0x1234,(seed%5)==4?nullptr:payload.data(),std::array<std::int32_t,5>{0,1,16,-1,16}[seed%5]);
 }else if(op=="flush"){
  buffer.count_8e81c0=1+seed%9;result=outbox.flush((seed&2)?7:8);
 }else throw std::runtime_error("Unknown outbox probe operation");
 std::cout<<"{\"clients\":[\""<<hex(clients[0].data(),clients[0].size())<<"\",\""<<hex(clients[1].data(),clients[1].size())<<"\"],\"buffer\":\""<<hex(buffer.bytes_8e61c0.data(),buffer.bytes_8e61c0.size())<<"\",\"count\":"<<buffer.count_8e81c0<<",\"echo\":\""<<hex(messages.pending_local_echo().data(),messages.pending_local_echo().size())<<"\",\"result\":"<<result<<",\"calls\":[";
 for(std::size_t i=0;i<calls.size();++i){std::cout<<(i?",[":"[");for(std::size_t j=0;j<calls[i].size();++j)std::cout<<(j?",":"")<<calls[i][j];std::cout<<']';}
 std::cout<<"],\"packets\":[";for(std::size_t i=0;i<packets.size();++i)std::cout<<(i?",\"":"\"")<<packets[i]<<'"';std::cout<<"]}\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}}
