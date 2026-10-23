#include "dfre/mission_type_mark.hpp"
#include <iostream>
int main(){using namespace dfre;std::uint32_t index;while(std::cin>>index){MissionTypeCountBindings bindings;bindings.write=[](auto at,auto width,auto value){std::cout<<at<<' '<<width<<' '<<value<<'\n';};mission_type_mark(bindings,index);}}
