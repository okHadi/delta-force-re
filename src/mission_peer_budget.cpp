#include "dfre/mission_peer_budget.hpp"

namespace dfre {
namespace {
using U=std::uint32_t;
std::int32_t signed32(U v){return v<=0x7fffffffu?std::int32_t(v):std::int32_t(std::int64_t(v)-0x100000000LL);}
U read(const MissionPeerBudgetBindings& b,U at,U width=4){if(!b.read)throw MissionPeerBudgetError("Peer budget reader is not bound");return b.read(at,width);}
void write(const MissionPeerBudgetBindings& b,U at,U value){if(!b.write)throw MissionPeerBudgetError("Peer budget writer is not bound");b.write(at,4,value);}
}
void mission_peer_budget_update(const MissionPeerBudgetBindings& b){
    const U initial=read(b,0x6e5570);U eligible=0;
    for(U i=0;i<17;++i){const U client=0x6e9a90u+i*0x9b84u;
        if(!read(b,client+0x18c,1)||read(b,client+0x1c4,2)!=5)continue;
        const U actor=read(b,client+8);if(!(read(b,actor+0x6cu,1)&2u))++eligible;
    }
    write(b,0x6e5580,eligible);
    const U others=eligible-1u,other_cost=others*14u,own_cost=initial*15u;
    write(b,0x6e5584,initial);const U budget=read(b,0x6e5574);
    if(other_cost+own_cost<budget){write(b,0x6e557c,others);write(b,0x6e5578,initial);}
    else if(signed32(initial)<=4){write(b,0x6e5578,initial);write(b,0x6e557c,(budget-own_cost)/14u);}
    else{
        const U limit=(budget-60u)/14u;
        if(signed32(limit)>signed32(others)){write(b,0x6e557c,others);write(b,0x6e5578,(budget-other_cost)/15u);}
        else{write(b,0x6e557c,limit);write(b,0x6e5578,4);}
    }
    const U requested=read(b,0x6e5578),current=read(b,0x6e5584);
    if(signed32(requested)>signed32(current))write(b,0x6e5578,current);
    const U maximum=read(b,0x6e5580)-1u,selected=read(b,0x6e557c);
    if(signed32(maximum)<signed32(selected))write(b,0x6e557c,maximum);
}
} // namespace dfre
