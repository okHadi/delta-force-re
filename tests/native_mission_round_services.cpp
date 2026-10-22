#include "dfre/mission_round_services.hpp"
#include <cassert>
#include <map>
#include <array>
using namespace dfre;
namespace {
using U=std::uint32_t;
struct Fixture {
    MissionRoundServiceBindings b;std::map<U,U> cells;
    std::array<std::array<U,26>,64> squads{};std::array<std::map<U,U>,17> clients;
    std::vector<U> formats;U opened=0,closed=0;
    Fixture(){
        b.cell=[&](U at,U width){assert(width==4);if(at>=0x6e8090 && at<0x6e9a90){const U relative=at-0x6e8090;return squads.at(relative/0x68).at((relative%0x68)/4);}return cells[at];};
        b.store=[&](U at,U width,U n){assert(width==4);cells[at]=n;};
        b.client=[&](U i,U at,U width){assert(width==((at==0x18c||at==0x18d)?1u:at==0x1c4?2u:4u));return clients.at(i)[at];};
        b.squad=[&](U i,U at,U width){assert(width==4);return squads.at(i).at(at/4);};b.squad_store=[&](U i,U at,U width,U n){assert(width==4);squads.at(i).at(at/4)=n;};
        b.text=[](U at){assert(at==0x67f478);return std::string("GAME");};b.client_name=[](U){return std::string("CLIENT");};
        b.file_open=[&](const auto& path,const auto& mode){assert(path=="scores.txt"&&mode=="a");++opened;return 123;};
        b.file_print=[&](U file,U format,const auto&){assert(file==123);formats.push_back(format);assert(mission_round_log_format(format));};
        b.file_close=[&](U file){assert(file==123);++closed;};b.local_time=[](){return std::array<std::uint16_t,8>{{2026,10,4,22,18,3,4,500}};};
    }
};
template<class F> bool fails(F&& f){try{f();}catch(const MissionRoundServiceError&){return true;}return false;}
}
int main(){
    {MissionRoundServiceBindings b;assert(fails([&]{mission_round_set_client_updates(b,1);}));assert(fails([&]{mission_round_clear_frame_flag(b);}));assert(fails([&]{mission_round_collect_squads(b);}));}
    {Fixture f;mission_round_set_client_updates(f.b,0xffffffffu);assert(f.cells[0x6e5588]==0xffffffffu);f.cells[0x6e5570]=91;mission_round_clear_frame_flag(f.b);assert(f.cells[0x6e5570]==0);}
    {Fixture f;for(auto& s:f.squads)s.fill(0xa5a5a5a5u);for(U i=0;i<2;++i){f.clients[i][0x18c]=1;f.clients[i][0x1c4]=5;f.clients[i][0x9c]=63;for(U off=0x19c;off<=0x1b4;off+=4)f.clients[i][off]=i?2u:0xffffffffu;}
        f.clients[2][0x18c]=1;f.clients[2][0x1c4]=4;mission_round_collect_squads(f.b);
        for(U i=0;i<64;++i){for(U off=0x48;off<=0x60;off+=4)assert(f.squads[i][off/4]==(i==63?1u:0u));assert(f.squads[i][0x44/4]==0xa5a5a5a5u && f.squads[i][0x64/4]==0xa5a5a5a5u);}}
    {Fixture f;for(U i=0;i<2;++i){f.clients[i][0x18c]=1;f.clients[i][0x1c4]=5;f.clients[i][0x19c]=1;}const auto store=f.b.squad_store;
        f.b.squad_store=[&](U i,U at,U width,U n){store(i,at,width,n);if(at==0x48 && n==1){f.clients[0][0x1a0]=91;f.clients[1][0x18c]=0;}};
        mission_round_collect_squads(f.b);assert(f.squads[0][0x48/4]==1 && f.squads[0][0x4c/4]==91);}
    {Fixture f;f.cells[0x67f53c]=0;f.clients[0][0x1a0]=0x7fffffffu;f.clients[1][0x1a0]=0x80000000u;assert(mission_round_compare_clients(f.b,0,1)==1);f.clients[1][0x1a0]=0x7fffffffu;f.clients[0][0x1ac]=0x80000000u;assert(mission_round_compare_clients(f.b,0,1)==-1);f.clients[1][0x1ac]=0x80000000u;f.clients[0][0x1a8]=0x80000000u;assert(mission_round_compare_clients(f.b,0,1)==1);f.clients[1][0x1a8]=0x80000000u;assert(mission_round_compare_clients(f.b,0,1)==0);f.cells[0x67f53c]=0xffffffffu;f.b.client={};assert(mission_round_compare_clients(f.b,0,1)==0);}
    {MissionRoundServiceBindings b;U reads=0;b.cell=[&](U at,U width){assert(width==4);++reads;assert(at==0x503428);return 0u;};mission_round_log(b,1);assert(reads==1);b.cell=[](U at,U){return at==0x503428?1u:0u;};mission_round_log(b,1);}
    {Fixture f;f.cells[0x503428]=f.cells[0x67f520]=1;f.squads[0][0x48/4]=91;f.b.file_open=[](const auto&,const auto&){return 0u;};f.b.file_print={};f.b.local_time={};mission_round_log(f.b,1);assert(f.squads[0][0x48/4]==0 && f.closed==0);}
    {Fixture f;f.cells[0x503428]=f.cells[0x67f520]=1;f.b.file_print={};assert(fails([&]{mission_round_log(f.b,1);}));assert(f.opened==1 && f.closed==0);}
    {Fixture f;f.cells[0x503428]=f.cells[0x67f520]=1;f.cells[0x67f53c]=2;assert(fails([&]{mission_round_log(f.b,1);}));assert(f.formats.back()==0x4cc804 && f.closed==0);}
    {Fixture f;f.cells[0x503428]=f.cells[0x67f520]=1;f.cells[0x67f53c]=2;f.clients[0][0x18c]=1;f.clients[0][0x1c4]=5;f.clients[0][0x9c]=1;f.clients[0][0x1a0]=100;f.clients[0][0x1b0]=3;f.clients[0][0x1b4]=4;
        bool blue=false,red=false;const auto print=f.b.file_print;f.b.file_print=[&](U file,U fmt,const std::vector<MissionRoundLogValue>& args){print(file,fmt,args);if(fmt==0x4cc840){assert(std::get<U>(args.at(0))==84);blue=true;f.cells[0x67f53c]=4;f.squads[2][0x44/4]=0xffffffc3u;}if(fmt==0x4cc898){assert(std::get<U>(args.at(0))==0xffffffffu && std::get<U>(args.at(1))==0xffffffffu);red=true;}};
        mission_round_log(f.b,1,1);assert(blue&&red&&f.closed==1);}
    {Fixture f;f.cells[0x503428]=f.cells[0x67f520]=1;f.clients[0][0x18c]=1;f.b.client_name=[](U){return std::string(32,'X');};assert(fails([&]{mission_round_log(f.b,0);}));assert(f.formats.back()==0x4cc8c8 && f.closed==0);}
    assert(fails([]{mission_round_log_format(0);}));return 0;
}
