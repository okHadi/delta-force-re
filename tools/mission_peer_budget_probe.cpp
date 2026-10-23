#include "dfre/mission_peer_budget.hpp"
#include <iostream>
#include <sstream>
#include <map>
using namespace dfre;
namespace {using U=std::uint32_t;constexpr U C=0x6e9a90,A=0x1301000;const U keys[]={0x6e5570,0x6e5574,0x6e5578,0x6e557c,0x6e5580,0x6e5584};U next(std::istream& in){std::uint64_t n;if(!(in>>n))throw MissionPeerBudgetError("Short peer budget row");return U(n);}}
int main(){try{std::string line;while(std::getline(std::cin,line)){if(line.empty())continue;std::istringstream in(line);const U mutation=next(in);std::map<U,U> cells;for(U k:keys)cells[k]=next(in);for(U i=0;i<17;++i){const U client=C+i*0x9b84,actor=A+i*0x114;cells[client+0x18c]=next(in);cells[client+0x1c4]=next(in);const U present=next(in);cells[client+8]=present?actor:0;cells[actor+0x6c]=next(in);}
std::ostringstream trace;MissionPeerBudgetBindings b;b.read=[&](U at,U width){trace<<'R'<<at<<','<<width<<';';if((mutation&32)&&at==A+0x6c){cells[C+0x9b84+0x18c]=1;cells[C+0x9b84+0x1c4]=5;cells[C+0x9b84+8]=A+0x114;cells[A+0x114+0x6c]=0;}if((mutation&64)&&at==C+0x18c)cells[0x6e5570]=10;if((mutation&128)&&at==0x6e5578)cells[0x6e5584]=0xfffffffeu;const auto found=cells.find(at);if(found==cells.end())throw MissionPeerBudgetError("Peer budget address has no owner");return found->second;};
b.write=[&](U at,U width,U n){trace<<'W'<<at<<','<<width<<','<<n<<';';cells.at(at)=n;if((mutation&1)&&at==0x6e5580)cells[0x6e5570]=9;if((mutation&2)&&at==0x6e5584)cells[0x6e5574]=77;if((mutation&4)&&at==0x6e557c)cells[0x6e5580]=1;if((mutation&8)&&at==0x6e5578)cells[0x6e5584]=2;if((mutation&16)&&at==0x6e5578)cells[0x6e557c]=0x7fffffffu;};
bool fault=false;try{mission_peer_budget_update(b);}catch(const MissionPeerBudgetError&){fault=true;}
std::cout<<fault;for(U k:keys)std::cout<<' '<<cells.at(k);for(U i=0;i<17;++i){const U client=C+i*0x9b84,actor=A+i*0x114;std::cout<<' '<<cells.at(client+0x18c)<<' '<<cells.at(client+0x1c4)<<' '<<cells.at(client+8)<<' '<<cells.at(actor+0x6c);}std::cout<<' '<<trace.str()<<'\n';
}}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}return 0;}
