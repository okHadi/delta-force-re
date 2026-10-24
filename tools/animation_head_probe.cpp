#include "../tests/animation_head_fixture.hpp"
#include <iostream>
int main(){try{std::uint32_t duration,pitch,last_y;while(std::cin>>duration>>pitch>>last_y)std::cout<<animation_head_fixture::run(duration,pitch,last_y)<<'\n';return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
