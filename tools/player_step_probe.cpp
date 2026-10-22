#include "dfre/player_step.hpp"
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
namespace {
std::vector<std::string> split(const std::string& s,char separator){
    std::stringstream stream(s);std::vector<std::string> out;std::string item;
    while(std::getline(stream,item,separator))out.push_back(item);return out;
}
std::uint32_t number(const std::string& s){
    std::size_t end=0;const auto value=std::stoull(s,&end);if(end!=s.size() || value>0xffffffffu)throw std::runtime_error("invalid word");return std::uint32_t(value);
}
void run(const std::string& line){
    const auto input=split(line,'|');if(input.size()!=2)throw std::runtime_error("state|prefix mutations required");
    dfre::PlayerStepState state;
    std::uint32_t flags=0,lx=0,ly=0,px=0,py=0,radius=0;
    std::vector<std::uint32_t*> fields{&state.local_actor_c23820,&state.recoil_c239e8[0],&state.recoil_c239e8[1],&state.recoil_c239e8[2],&state.eye_c23a18,&state.prone_c23828,&state.game_type_67f53c,&state.waypoint_count_c23b7c,&state.waypoint_index_c23b80,&flags,&lx,&ly,&px,&py,&radius};
    const auto values=split(input[0],',');if(values.size()!=fields.size())throw std::runtime_error("15 state words required");
    for(std::size_t i=0;i<fields.size();++i)*fields[i]=number(values[i]);
    state.waypoints_c23b84={0x2002000,0x2002000};unsigned prefix_calls=0;
    dfre::PlayerStepWorld world;
    world.weapon_prefix=[&](auto&){
        ++prefix_calls;if(input[1]=="-")return;
        for(const auto& mutation:split(input[1],';')){
            const auto change=split(mutation,',');if(change.size()!=2)throw std::runtime_error("invalid prefix mutation");
            fields.at(number(change[0]))[0]=number(change[1]);
        }
    };
    world.actor=[&](std::uint32_t handle)->std::optional<dfre::PlayerStepActor>{
        if(handle==0x2000000 || handle==0x2001000)return dfre::PlayerStepActor{{lx,ly},std::uint8_t(flags),0};
        if(handle==0x2002000)return dfre::PlayerStepActor{{px,py},0,radius};
        return std::nullopt;
    };
    dfre::step_player(state,world);
    for(std::size_t i=0;i<fields.size();++i){if(i)std::cout<<',';std::cout<<*fields[i];}
    std::cout<<','<<prefix_calls<<'\n';
}
}
int main(){
    std::string line;try{while(std::getline(std::cin,line))if(!line.empty())run(line);}
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
