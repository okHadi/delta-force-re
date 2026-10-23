#include "dfre/mission_object_mine_move.hpp"
#include <iostream>
#include <sstream>
using namespace dfre;using U=std::uint32_t;
int main(){try{std::string line;while(std::getline(std::cin,line)){
    U mutation,fail_at;std::string hex;std::istringstream in(line);
    if(!(in>>mutation>>fail_at>>hex)||hex.size()!=0x228)throw MissionTickError("Short mine movement fixture");
    AiActorImage bytes{};for(std::size_t i=0;i<bytes.size();++i)bytes[i]=std::uint8_t(std::stoul(hex.substr(i*2,2),nullptr,16));
    constexpr U base=0x7dde80;std::ostringstream trace;U accesses=0;
    const auto put=[&](U offset,U n){for(U i=0;i<4;++i)bytes.at(offset+i)=std::uint8_t(n>>(8*i));};
    MissionObjectMineMoveBindings b;
    b.read=[&](U at,U width){if(width!=4)throw MissionTickError("Invalid mine read width");U n=0;for(U i=0;i<4;++i)n|=U(bytes.at(at-base+i))<<(8*i);trace<<'R'<<at<<','<<n<<';';if(++accesses==fail_at)throw MissionTickError("Fixture read stopped");return n;};
    b.write=[&](U at,U width,U n){if(width!=4)throw MissionTickError("Invalid mine store width");trace<<'W'<<at<<','<<n<<';';put(at-base,n);
        if(at==base+0x24&&(mutation&1))put(0x7c,0x30000);
        if(at==base+0x2c&&(mutation&2))put(0xe4,1);
        if(++accesses==fail_at)throw MissionTickError("Fixture store stopped");};
    bool failed=false;try{mission_object_mine_move(b,base);}catch(const MissionTickError&){failed=true;}
    std::uint64_t hash=1469598103934665603ull;for(auto byte:bytes){hash^=byte;hash*=1099511628211ull;}
    std::cout<<failed<<' '<<hash<<' '<<trace.str()<<'\n';
}return 0;}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
