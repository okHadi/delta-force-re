#include "dfre/item_callbacks.hpp"
#include <iostream>
#include <string>
#include <cstring>
int main(){
    std::string ai,move;
    while(std::cin>>ai>>move){
        std::vector<dfre::RuntimeItemDefinition> items(1);
        if(ai!="-") std::strncpy(items[0].ai_function,ai.c_str(),7);
        if(move!="-") std::strncpy(items[0].move_function,move.c_str(),7);
        unsigned count=0;
        dfre::bind_runtime_item_callbacks(items,[&](std::string_view){return 0x80000000u+count++;});
        std::cout<<items[0].ai_data<<' '<<items[0].move_data;
        for(unsigned i=0;i<4;++i){std::uint32_t value;std::memcpy(&value,reinterpret_cast<const std::uint8_t*>(&items[0])+0x118+i*4,4);std::cout<<' '<<value;}
        std::cout<<'\n';
    }
}
