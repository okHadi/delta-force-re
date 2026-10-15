#include "dfre/item_callbacks.hpp"
#include <cstring>
#include <stdexcept>
int main(){
    if(dfre::resolve_item_ai_callback("ORG1")!=0x45b5b0||dfre::resolve_item_ai_callback("missing")!=0x45b590||dfre::resolve_item_move_callback("NULL")!=0x4563a0) throw std::runtime_error("Callback table mismatch");
    std::vector<dfre::RuntimeItemDefinition> items(1);std::strcpy(items[0].ai_function,"plyr");std::strcpy(items[0].move_function,"helo");
    for(unsigned i=0;i<4;++i){items[0].ambient[i][0]=char('A'+i);}
    unsigned count=0;dfre::bind_runtime_item_callbacks(items,[&](std::string_view name){if(name.size()!=1||name[0]!=char('A'+count))throw std::runtime_error("Sound order changed");return ++count;});
    if(items[0].ai_data!=0x45bdf0||items[0].move_data!=0x451590||count!=4)throw std::runtime_error("Binding mismatch");
    for(unsigned i=0;i<4;++i){std::uint32_t value;std::memcpy(&value,reinterpret_cast<const std::uint8_t*>(&items[0])+0x118+i*4,4);if(value!=i+1)throw std::runtime_error("Sound offset mismatch");}
}
