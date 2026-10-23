#include "dfre/mission_primary_actor_live.hpp"
#include <array>
#include <iostream>
#include <map>
#include <sstream>
using namespace dfre;using U=std::uint32_t;
int main(){try{std::string line;while(std::getline(std::cin,line)){
 U count,occupied,index,type,flags,person,mutation,fail;std::string hex;std::istringstream in(line);if(!(in>>count>>occupied>>index>>type>>flags>>person>>mutation>>fail>>hex)||hex.size()!=0x228||count>8||occupied>12)throw MissionTickError("Short primary actor fixture");
 constexpr U A=0x7dde80,D=0x18000000,S=0x1d000000;MissionTickState state;AiActorTables tables;tables.groups[0].resize(count);for(U i=0;i<count;++i)if(i!=occupied){tables.groups[0][i].active=true;tables.groups[0][i].definition_index=1;}init_mission_tick(state,{},std::move(tables),AiItemDefinitionTable(std::vector<AiItemDefinition>(3)),std::vector<AiPersonState>(1),0);
 std::map<U,std::uint8_t> bytes;for(U i=0;i<0x400;++i)bytes[D+i]=0;for(U i=0;i<0x114;++i)bytes[S+i]=std::uint8_t(std::stoul(hex.substr(i*2,2),nullptr,16));auto put=[&](U at,U n,U width=4){for(U i=0;i<width;++i)bytes.at(at+i)=std::uint8_t(n>>(8*i));};put(S,index);put(S+4,D+index*0x13c);put(S+0xc,0);put(S+0x18,0);put(D+index*0x13c+0x28,type);put(D+index*0x13c+0x24,flags,1);
 std::vector<AiActorImage> tail(1024-count);for(U i=count;i<occupied;++i){tail[i-count][0]=1;const U raw=D+0x13c;for(U byte=0;byte<4;++byte)tail[i-count][4+byte]=std::uint8_t(raw>>(byte*8));}
 MissionPrimaryActorLiveBindings host;host.initial_definition=[&](std::size_t slot){return slot==occupied?0u:D+0x13c;};host.definition_index=[](U raw){if(raw<D||raw>=D+3*0x13c||(raw-D)%0x13c)throw MissionTickError("Bad definition fixture");return (raw-D)/0x13c;};host.definition_address=[](U n){return D+n*0x13c;};host.delegated.read=[&](U at,U width){U n=0;for(U i=0;i<width;++i)n|=U(bytes.at(at+i))<<(i*8);return n;};host.delegated.write=[&](U at,U width,U n){put(at,n,width);};std::ostringstream trace;MissionPrimaryActorLive* owner=nullptr;
 host.delegated.person=[&](U actor){trace<<'C'<<0x44a0c0u<<','<<actor<<';';if(fail)throw MissionTickError("Fixture person stopped");if(mutation)owner->write(actor+0x24,4,0x12345678);if(person){state.tables.groups[0].at((actor-A)/0x114).person_slot=0;state.people[0].owner=actor_handle(0,(actor-A)/0x114);}return person?0x794680u:0u;};
 MissionPrimaryActorLive live(state,host,std::move(tail));owner=&live;auto memory=live.insertion_bindings();MissionActorInsertBindings observed;observed.read=[&](U at,U width){const U n=memory.read(at,width);trace<<'R'<<at<<','<<width<<','<<n<<';';return n;};observed.write=[&](U at,U width,U n){memory.write(at,width,n);trace<<'W'<<at<<','<<width<<','<<n<<';';};observed.person=memory.person;
 bool failed=false;U result=0;try{result=mission_actor_insert(observed,S,0);}catch(const std::exception&){failed=true;}
 std::uint64_t hash=1469598103934665603ull;for(U slot=0;slot<1024;++slot)for(U offset=0;offset<0x114;offset+=4){const U n=live.read(A+slot*0x114+offset,4);for(U i=0;i<4;++i){hash^=(n>>(i*8))&255;hash*=1099511628211ull;}}std::cout<<failed<<' '<<result<<' '<<live.read(0x8cfa1c,4)<<' '<<hash<<' '<<trace.str()<<'\n';
}return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
