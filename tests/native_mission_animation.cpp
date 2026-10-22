#include "dfre/mission_animation.hpp"
#include <iostream>
#include <stdexcept>
#define CHECK(x) do{if(!(x))throw std::runtime_error("mission animation: " #x);}while(false)
int main(){try{
    using namespace dfre;
    CharacterModel character;character.runtime_prepared=true;character.bone_map={0};
    const auto base=kCharacterInstanceBase;
    MissionAnimations world({{base,&character,{}},{base+1,&character,{}}});
    CHECK(world.size()==2);CHECK(world.pose(0)==nullptr);CHECK(world.pose(base)->size()==1);
    auto* first=&world.controller(base);auto* second=&world.controller(base+1);
    CHECK(first!=second);CHECK(world.pose(base)==&first->pose());
    first->set_condition_state(0xffffffffu);CHECK(second->condition_current()==0);
    second->set_condition_state(7);CHECK(first->condition_current()==0xffffffffu);
    bool duplicate=false;try{MissionAnimations invalid({{base,&character,{}},{base,&character,{}}});}
    catch(const std::invalid_argument&){duplicate=true;}CHECK(duplicate);
    bool missing=false;try{world.controller(base+2);}catch(const std::out_of_range&){missing=true;}CHECK(missing);
    world.release(0);CHECK(world.size()==2);
    world.release(base);CHECK(world.size()==1&&world.pose(base)==nullptr&&world.pose(base+1)!=nullptr);
    missing=false;try{world.release(base);}catch(const std::out_of_range&){missing=true;}CHECK(missing);
    std::cout<<"mission animation checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
