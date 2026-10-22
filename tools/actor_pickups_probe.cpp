#include "dfre/actor_pickups.hpp"
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

using namespace dfre;
namespace {
constexpr std::uint32_t A=0x2201000,T=0x2202000,S=0x2203000;
std::uint32_t next(std::istream& in) {std::uint64_t n;if(!(in>>n))throw std::invalid_argument("Short pickup probe row");return std::uint32_t(n);}
std::uint32_t word(const AiActorImage& a,unsigned at) {return std::uint32_t(a[at])|std::uint32_t(a[at+1])<<8|std::uint32_t(a[at+2])<<16|std::uint32_t(a[at+3])<<24;}
void put(AiActorImage& a,unsigned at,std::uint32_t n) {for(unsigned i=0;i<4;++i)a[at+i]=std::uint8_t(n>>(i*8));}
template<class Bytes> std::string hex(const Bytes& a) {std::ostringstream out;out<<std::hex<<std::setfill('0');for(auto n:a)out<<std::setw(2)<<unsigned(n);return out.str();}
AiActorImage image(std::istream& in) {std::string raw;in>>raw;if(raw.size()!=0x228)throw std::invalid_argument("Bad pickup image");AiActorImage a{};for(unsigned i=0;i<a.size();++i)a[i]=std::uint8_t(std::stoul(raw.substr(i*2,2),nullptr,16));return a;}
}
int main(int argc,char** argv) {
    if(argc!=2)return 2;
    try {
        const auto site=std::stoul(argv[1],nullptr,16);std::string line;
        while(std::getline(std::cin,line)) {
            if(line.empty())continue;std::istringstream in(line);auto a=image(in),t=image(in),spawn=image(in);AiPersonState person;
            const auto have_person=next(in);for(auto& n:person.position)n=next(in);person.angle_yaw=next(in);
            const auto hp=std::uint16_t(next(in));const auto target_id=next(in),spawn_id=next(in);
            ActorPickupState state;state.host=next(in);state.rules_67f549=std::uint8_t(next(in));state.local_player=next(in);
            state.team_one_6e815c=next(in);state.team_two_6e81c4=next(in);
            std::array<ActorPickupClient,17> clients;for(auto& c:clients){c.active_18c=std::uint8_t(next(in));c.actor_08=next(in);c.deliveries_1bc=next(in);}state.clients=&clients;
            const auto mutation=next(in),mode=next(in),owner=next(in);Fixed3 position{std::int32_t(next(in)),std::int32_t(next(in)),std::int32_t(next(in))};
            const ActorPickupActor actor{&a,A,A,have_person?&person:nullptr},target{&t,T,T,nullptr},created{&spawn,S,S,nullptr};
            auto dump=[&]{std::ostringstream out;out<<hex(a)<<'|'<<hex(t)<<'|'<<hex(spawn)<<'|'<<state.host<<','<<unsigned(state.rules_67f549)<<','<<state.local_player<<','<<state.team_one_6e815c<<','<<state.team_two_6e81c4;
                for(const auto& c:clients)out<<','<<unsigned(c.active_18c)<<','<<c.actor_08<<','<<c.deliveries_1bc;return out.str();};
            std::ostringstream trace;auto snap=[&](const std::string& tag){trace<<tag<<':'<<dump()<<';';};ActorPickupBindings b;
            b.definition=[&](auto id){return ActorPickupDefinition{id==0x2240100?target_id:id==0x2240200?spawn_id:0,hp};};
            b.client_message=[&](auto client,auto message,auto arg1,auto arg2){snap("441750,"+std::to_string(client)+","+std::to_string(message)+","+std::to_string(arg1)+","+std::to_string(arg2));
                if(mutation&1)clients[5]={1,A,0};if(mutation&64){clients[client].active_18c=0;clients[client].actor_08=0;++clients[client].deliveries_1bc;}};
            b.resupply=[&](auto client){snap("447e80,"+std::to_string(client));clients[client].deliveries_1bc+=3;};
            b.sound=[&](auto id){snap("43e720,"+std::to_string(id));if(mutation&256)put(a,0x74,word(a,0x74)+1);};
            b.local_flag=[&]{snap("484ed0");};
            b.remove=[&](auto value){snap("45da30,"+std::to_string(value.identity));if(mutation&4){*value.image={};clients[3].deliveries_1bc=77;}};
            b.message=[&](auto transport,auto id,const auto& payload){std::ostringstream tag;tag<<std::hex<<std::uint32_t(transport)<<std::dec<<','<<id<<','<<hex(payload);snap(tag.str());
                if(mutation&1)clients[5]={1,A,0};if(mutation&2)state.host=0;
                if(mutation&8)put(t,0x24,word(t,0x24)+123);if(mutation&128)state.local_player=state.local_player?0:A;};
            b.objective=[&](auto value,auto pose,auto argument){snap("4606b0,"+std::to_string(value.identity)+","+std::to_string(pose.identity)+","+std::to_string(argument));};
            b.allocate=[&](auto value,auto point)->std::optional<ActorPickupActor>{snap("460d20,"+std::to_string(value)+","+std::to_string(std::uint32_t(point.x))+","+std::to_string(std::uint32_t(point.y))+","+std::to_string(std::uint32_t(point.z)));if(mutation&16)return std::nullopt;if(mutation&32)put(spawn,0x6c,0x87654321);return created;};
            std::uint32_t result=0;
            switch(site) {
            case 0x446f00:actor_pickup_refill(actor,state,b);break;
            case 0x446f60:actor_pickup_heal(actor,state,b);break;
            case 0x460470:actor_pickup_take(actor,target,state,b);break;
            case 0x4605d0:result=actor_pickup_restore_home(actor);break;
            case 0x447220:actor_pickup_flag_message(actor,state,b);break;
            case 0x4471d0:actor_pickup_return(target,state,b);break;
            case 0x447280:actor_pickup_deposit(actor,state,b);break;
            case 0x4472d0:case 0x447360:actor_pickup_deliver(actor,target,site==0x447360,state,b);break;
            case 0x446fb0:actor_pickup_objective(actor,{t.data()+0x24,T+0x24},mode,state,b);break;
            case 0x447610:actor_pickup_objective_spawn(owner,position,b);break;
            default:throw std::invalid_argument("Unknown pickup probe site");
            }
            std::cout<<result<<' '<<dump()<<' '<<(trace.str().empty()?"-":trace.str())<<'\n';
        }
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
