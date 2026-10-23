#include "dfre/mission_peer_budget.hpp"
#include <cassert>
#include <map>
#include <vector>
using namespace dfre;
namespace {using U=std::uint32_t;constexpr U C=0x6e9a90,A=0x1301000;
struct Fixture{MissionPeerBudgetBindings b;std::map<U,U> cells;std::vector<U> writes;
    Fixture(){b.read=[&](U at,U width){if(at>=C&&at<C+17*0x9b84){const U offset=(at-C)%0x9b84;assert(width==(offset==0x18c?1u:offset==0x1c4?2u:4u));}else if(at>=A&&at<A+17*0x114)assert(width==1);else assert(width==4);return cells[at];};b.write=[&](U at,U width,U n){assert(width==4);writes.push_back(at);cells[at]=n;};}
    void eligible(U i,bool active=true,U state=5,U flags=0){const U c=C+i*0x9b84,a=A+i*0x114;cells[c+0x18c]=active;cells[c+0x1c4]=state;cells[c+8]=a;cells[a+0x6c]=flags;}
};}
int main(){
    {MissionPeerBudgetBindings b;bool failed=false;try{mission_peer_budget_update(b);}catch(const MissionPeerBudgetError&){failed=true;}assert(failed);}
    {Fixture f;f.cells[0x6e5570]=1;f.cells[0x6e5574]=100;mission_peer_budget_update(f.b);assert(f.cells[0x6e5580]==0&&f.cells[0x6e557c]==0xffffffffu&&f.cells[0x6e5578]==1&&f.cells[0x6e5584]==1);}
    {Fixture f;f.cells[0x6e5570]=4;f.cells[0x6e5574]=100;for(U i=0;i<17;++i)f.eligible(i);f.eligible(1,false);f.eligible(2,true,4);f.eligible(3,true,5,2);mission_peer_budget_update(f.b);assert(f.cells[0x6e5580]==14&&f.cells[0x6e557c]==2&&f.cells[0x6e5578]==4);}
    {Fixture f;f.cells[0x6e5570]=10;f.cells[0x6e5574]=100;for(U i=0;i<5;++i)f.eligible(i);mission_peer_budget_update(f.b);assert(f.cells[0x6e557c]==2&&f.cells[0x6e5578]==4);}
    {Fixture f;f.cells[0x6e5570]=10;f.cells[0x6e5574]=100;f.eligible(0);f.eligible(1);mission_peer_budget_update(f.b);assert(f.cells[0x6e557c]==1&&f.cells[0x6e5578]==5);}
    {Fixture f;f.cells[0x6e5570]=0x80000000u;f.cells[0x6e5574]=0;f.eligible(0);mission_peer_budget_update(f.b);assert(f.cells[0x6e5584]==0x80000000u&&f.cells[0x6e5578]==0x80000000u&&f.cells[0x6e557c]==0);}
    {Fixture f;f.cells[0x6e5570]=10;f.cells[0x6e5574]=100;f.eligible(0);f.eligible(1);const auto store=f.b.write;
        f.b.write=[&](U at,U width,U value){store(at,width,value);if(at==0x6e5580)f.cells[0x6e5570]=99;if(at==0x6e5584)f.cells[0x6e5574]=77;if(at==0x6e5578){f.cells[0x6e5584]=2;f.cells[0x6e5580]=1;f.cells[0x6e557c]=0x7fffffffu;}};
        mission_peer_budget_update(f.b);assert(f.cells[0x6e5570]==99&&f.cells[0x6e5578]==2&&f.cells[0x6e5584]==2&&f.cells[0x6e557c]==0);assert(f.writes[0]==0x6e5580&&f.writes[1]==0x6e5584);}
    {Fixture f;const auto reader=f.b.read;f.b.read=[&](U at,U width){const auto value=reader(at,width);if(at==C+0x18c){assert(f.writes.empty());throw MissionPeerBudgetError("Missing client owner");}return value;};bool failed=false;try{mission_peer_budget_update(f.b);}catch(const MissionPeerBudgetError&){failed=true;}assert(failed&&f.writes.empty());}
    return 0;
}
