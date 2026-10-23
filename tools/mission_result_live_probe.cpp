#include "dfre/mission_result_live.hpp"
#include <algorithm>
#include <array>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
std::string hex(const std::uint8_t* p,std::size_t n){const char* digits="0123456789abcdef";std::string out;for(std::size_t i=0;i<n;++i){out+=digits[p[i]>>4];out+=digits[p[i]&15];}return out;}
int run(){
    using U=std::uint32_t;const std::vector<U> addresses{0x67f520,0x67f528,0x67dbc0,0x67f534,0x6e558c,0x67f52c,0x67f53c,0x6e4758,0x6e5774,0x6e576c,0x6e5770,0xc23820};
    std::string line;while(std::getline(std::cin,line)){if(line.empty())continue;std::istringstream input(line);
        auto next=[&]{std::uint64_t n;if(!(input>>n))throw std::runtime_error("Short live result row");return U(n);};
        auto bytes=[&](std::size_t n){std::string s;if(!(input>>s)||s.size()!=n*2)throw std::runtime_error("Bad live result image");std::vector<std::uint8_t> r(n);for(std::size_t i=0;i<n;++i)r[i]=std::uint8_t(std::stoul(s.substr(i*2,2),nullptr,16));return r;};
        const auto mode=next(),flag=next(),mutation=next();std::map<U,U> cells;for(auto at:addresses)cells[at]=next();
        auto block=bytes(0x514);auto squads=bytes(0x138);std::array<std::vector<std::uint8_t>,17> clients,actors;for(auto& c:clients)c=bytes(512);for(auto& a:actors)a=bytes(0x114);auto raw=bytes(0xce);dfre::MissionLogicResultPacket packet;std::copy(raw.begin(),raw.end(),packet.begin());
        auto read=[](const auto& a,U at,U width){U n=0;for(U i=0;i<width;++i)n|=U(a.at(at+i))<<(i*8);return n;};auto put=[](auto& a,U at,U n,U width){for(U i=0;i<width;++i)a.at(at+i)=std::uint8_t(n>>(i*8));};
        auto load=[&](U at){if(at>=0x67f714&&at<0x67fc28)return read(block,at-0x67f714,4);if(at>=0x6e80c8&&at<0x6e8200)return read(squads,at-0x6e80c8,4);if(!cells.count(at))throw std::runtime_error("Missing result cell "+std::to_string(at));return cells.at(at);};
        auto save=[&](U at,U n){if(at>=0x67f714&&at<0x67fc28)put(block,at-0x67f714,n,4);else{if(!cells.count(at))throw std::runtime_error("Missing result store "+std::to_string(at));cells.at(at)=n;}};
        dfre::MissionTickState canonical;canonical.tables.groups[3].resize(17);canonical.people.resize(17);canonical.stored_actor_images[3].resize(17);
        for(std::size_t i=0;i<17;++i){auto& actor=canonical.tables.groups[3][i];actor.field_70=std::uint16_t(read(actors[i],0x70,2));canonical.stored_actor_images[3][i][0x6f]=std::uint8_t(read(actors[i],0x6f,1));canonical.runtime_for({3,i}).byte_6e=std::uint8_t(read(actors[i],0x6e,1));if(read(actors[i],0xc,4)){actor.person_slot=i;canonical.people[i].name="PERSON";}}
        if(cells[0xc23820])canonical.config.local_actor=(cells[0xc23820]-0x7cca80)/0x114;
        std::ostringstream trace;dfre::MissionResultLiveHost b;b.state.cell=load;b.state.store=save;
        b.state.client=[&](auto slot,U at,U width){return read(clients.at(slot),at,width);};b.state.client_store=[&](auto slot,U at,U width,U n){put(clients.at(slot),at,n,width);};
        b.store_name=[&](U at,const std::string& name){for(std::size_t i=0;i<=name.size();++i)block.at(at-0x67f714+i)=i==name.size()?0:std::uint8_t(name[i]);};
        b.fallback_name=[&](U actor){trace<<"N,"<<actor<<';';if(mutation&8){cells[0xc23820]=actor;canonical.config.local_actor=(actor-0x7cca80)/0x114;save(0x67f71c,1);packet[0x25]^=0x80;packet[0x8b]^=0x80;}return std::string("CLIENT");};
        b.state.call=[&](U site,const std::vector<U>& args){trace<<"C,"<<site;for(auto n:args)trace<<','<<n;trace<<';';
            if(site==0x446100&&(mutation&1))cells[0x67dbc0]=0;
            if(site==0x446da0&&(mutation&2)){cells[0x67f53c]=(cells[0x67f53c]+1)%6;clients[16][0x18c]^=1;}
            if(site==0x448130){const auto a=read(clients.at(args.at(0)),0x1a0,4),c=read(clients.at(args.at(1)),0x1a0,4);const U result=a<c?0xffffffffu:a>c?1u:0u;if(mutation&16){put(clients.at(args.at(1)),0x1a0,a+7,4);put(clients.at(args.at(0)),0x1a8,254,4);cells[0x67f53c]=(cells[0x67f53c]+1)%6;}return result;}return 0u;};
        b.publish=[&](auto slot,U id,auto& p){trace<<"P,"<<slot<<','<<id<<','<<hex(p.data(),p.size())<<';';if(mutation&4){clients.at((slot+1)%17)[0x18c]^=1;cells[0x67f53c]=(cells[0x67f53c]+1)%6;p[2]^=1;}};
        const auto live=dfre::mission_result_live_bindings(canonical,std::move(b));bool fault=false;try{if(mode==0)dfre::mission_logic_build_result(live,packet);else if(mode==1)dfre::mission_logic_request_result(live,flag,packet);else dfre::mission_logic_receive_result(live,packet.data(),packet.size());}catch(const std::runtime_error& e){if(std::string(e.what())!="Mission result ratio divides outside signed range")throw;fault=true;}
        std::cout<<fault;for(auto at:addresses)std::cout<<' '<<cells.at(at);std::cout<<' '<<hex(block.data(),block.size())<<' '<<hex(squads.data(),squads.size());for(const auto& c:clients)std::cout<<' '<<hex(c.data(),c.size());std::cout<<' '<<hex(packet.data(),packet.size())<<' '<<trace.str()<<'\n';
    }return 0;
}

int main(){try{return run();}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
