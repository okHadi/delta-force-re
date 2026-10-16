#include "dfre/actor_queries.hpp"
#include "actor_record_probe.hpp"
#include <limits>
using namespace dfre_probe;
namespace {
std::int32_t signed_bits(std::uint32_t x){std::int32_t out;std::memcpy(&out,&x,4);return out;}
void run(const std::vector<std::string>& f){
    if(f.size()!=13)throw std::runtime_error("query case needs 13 fields");
    std::vector<dfre::AiActorState> actors;std::vector<dfre::AiPersonState> people;
    std::vector<dfre::BehaviorActorExtra> extras;std::vector<dfre::BehaviorPersonExtra> px;
    for(const auto& record:split(f[11],';')){
        const auto pair=split(record,',');if(pair.size()!=2)throw std::runtime_error("query record needs actor and person");
        auto parsed=parse_state(pair[0],pair[1]);parsed.actor.active=rd32(from_hex(pair[0]),4)!=0;parsed.actor.person_slot=actors.size();
        actors.push_back(parsed.actor);people.push_back(parsed.person);extras.push_back(parsed.actor_extra);px.push_back(parsed.person_extra);
    }
    dfre::ActorQueryTable table{&actors,&people,&extras,&px};dfre::ActorQueryData data;
    data.water_level=signed_bits(number(f[2]));data.double_score_67f548_bit80=number(f[3])!=0;data.local_player_excluded_67dbc9_bit8=number(f[4])!=0;
    if(number(f[5])!=0xffffffffu)data.local_player=number(f[5]);
    const auto terrain=signed_bits(number(f[8]));const auto terrain_mode=number(f[9]),sine_mode=number(f[10]);
    data.sin_cos_477820=data.sin_cos_4776c0=[=](std::uint32_t angle,std::int32_t& sine,std::int32_t& cosine){
        if(!sine_mode){constexpr std::int32_t s[]={0,4194304,0,-4194304};sine=s[angle>>30];cosine=s[((angle>>30)+1)&3];}
        else{sine=signed_bits((angle*1664525u+1013904223u)&0x7fffffu)-4194304;cosine=signed_bits((angle*22695477u+1u)&0x7fffffu)-4194304;}
    };
    data.terrain_height=[=](std::int32_t x,std::int32_t y){return signed_bits(std::uint32_t(terrain)+(terrain_mode?(((std::uint32_t(x>>8)^std::uint32_t(y>>8))&0x7ffffu)-0x40000u):0u));};
    data.raw_pointer=[](std::size_t i){return std::uint32_t(0x7cca80+i*0x114);};
    data.sight_line=[](std::size_t i){return std::int32_t(i%5);};
    data.weapon_frame_10=[](std::size_t i){return std::int32_t(32768+i*9137);};
    data.weapon_frame_14=[](std::size_t i){return std::int32_t(i%19)-2;};
    if(f[12]!="-")for(const auto& record:split(f[12],';')){
        auto v=split(record,',');if(v.size()!=8)throw std::runtime_error("zone needs 8 values");
        dfre::ActorZoneRect z;z.flags=number(v[0]);z.type=number(v[1]);z.x_min=signed_bits(number(v[2]));z.x_max=signed_bits(number(v[3]));z.y_min=signed_bits(number(v[4]));z.y_max=signed_bits(number(v[5]));z.z_min=signed_bits(number(v[6]));z.z_max=signed_bits(number(v[7]));data.zones.push_back(z);
    }
    dfre::BehaviorState state;state.rng=number(f[1]);std::string trace;
    state.on_effect=[&](const dfre::BehaviorAction& a){
        if(!trace.empty())trace+=';';trace+=std::to_string(a.site);
        for(auto value:a.args)trace+=','+std::to_string(value);
        for(auto value:{actors[0].angle_yaw,actors[0].angle_pitch,actors[0].position[2],std::uint32_t(people[0].field_9c),px[0].field_44,state.rng})trace+=','+std::to_string(value);
    };
    dfre::ActorQueryProvider provider(table,data);dfre::BehaviorActor view{{3,0},0x7cca80,actors[0],people[0],extras[0],px[0]};
    const auto site=number(f[0]);dfre::BehaviorAction request{dfre::BehaviorCallKind::query,site,{number(f[6]),number(f[7]),1}};
    dfre::BehaviorInput answer{site,0,0};
    if(site==dfre::behavior_site::kZoneStep || site==dfre::behavior_site::kZoneStep2 || site==dfre::behavior_site::kCombatStep){if(!provider.perform_effect({3,0},request,state))throw std::runtime_error("effect not handled");}
    else{const auto value=provider.answer({3,0},site,request,view,state);if(!value)throw std::runtime_error("query not handled");answer=*value;}
    std::cout<<answer.value<<' '<<answer.value2<<' '<<state.rng<<' '<<(trace.empty()?"-":trace)<<' ';
    for(std::size_t i=0;i<actors.size();++i){
        dfre::BehaviorActor item{{3,i},data.raw_pointer(i),actors[i],people[i],extras[i],px[i]};
        const auto ai=dfre::behavior_actor_image(item);const auto pi=dfre::behavior_person_image(item);
        if(i)std::cout<<';';std::cout<<to_hex(ai.data(),ai.size())<<','<<to_hex(pi.data(),pi.size());
    }
    std::cout<<'\n';
}
}
int main(int argc,char** argv){try{
    if(argc==2 && std::string(argv[1])=="angle"){
        std::int32_t x,y;while(std::cin>>x>>y)std::cout<<dfre::query_angle_477770(x,y)<<'\n';return 0;
    }
    if(argc!=1)throw std::runtime_error("actor_queries_probe [angle]");
    std::string line;while(std::getline(std::cin,line)){std::stringstream stream(line);std::vector<std::string> fields;std::string word;while(stream>>word)fields.push_back(word);if(!fields.empty())run(fields);}return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
