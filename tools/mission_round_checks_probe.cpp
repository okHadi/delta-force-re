#include "dfre/mission_round_checks.hpp"
#include "dfre/actor_queries.hpp"
#include <iostream>
#include <sstream>
#include <unordered_map>
using namespace dfre;
int main(){try{std::string line;while(std::getline(std::cin,line)){if(line.empty())continue;std::istringstream in(line);std::uint32_t h[8];for(auto& x:h){std::uint64_t n;if(!(in>>n))throw MissionTickError("Short round-check fixture");x=std::uint32_t(n);}std::unordered_map<std::uint32_t,std::uint8_t> memory;const auto put=[&](auto at,auto width,auto n){for(unsigned i=0;i<width;++i)memory[at+i]=std::uint8_t(n>>(8*i));};const auto get=[&](auto at,auto width){std::uint32_t n=0;for(unsigned i=0;i<width;++i)n|=std::uint32_t(memory[at+i])<<(8*i);return n;};for(unsigned i=0;i<h[7];++i){std::uint32_t at,width,n;if(!(in>>at>>width>>n))throw MissionTickError("Short round memory");put(at,width,n);}unsigned reads=0;std::ostringstream trace;MissionRoundCheckBindings b;
b.read=[&](auto at,auto width){if(at==h[1]&&reads++==h[2])put(at,width,h[3]);const auto n=get(at,width);trace<<'R'<<at<<','<<width<<','<<n<<';';return n;};
b.write=[&](auto at,auto width,auto n){trace<<'W'<<at<<','<<width<<','<<n<<';';put(at,width,n);};
b.call=[&](auto site,const auto& args)->std::uint32_t{trace<<'C'<<site;for(auto n:args)trace<<','<<n;trace<<';';if(site==h[4])put(h[5],4u,h[6]);if(site==0x477620)return query_isqrt_477620(args.at(0));return 0;};
b.enqueue_441750=[&](auto slot,auto type,auto* payload,auto length){trace<<'C'<<0x441750u<<','<<slot<<','<<type<<','<<unsigned(payload[0])<<','<<length<<';';if(h[4]==0x441750){put(h[5],4u,h[6]);if(h[5]==0x1300000)payload[0]=std::uint8_t(h[6]);}};
if(h[0]==3){std::uint8_t byte=std::uint8_t(get(0x1300000u,1u));mission_round_broadcast(b,0x30,&byte,1);}else if(h[0]==0)mission_round_check_end(b);else if(h[0]==1)mission_round_check_clients(b);else mission_round_publish_status(b);std::cout<<trace.str()<<'\n';}}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
