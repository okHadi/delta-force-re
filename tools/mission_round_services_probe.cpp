#include "dfre/mission_round_services.hpp"
#include <iostream>
#include <sstream>
#include <map>
#include <iomanip>
using namespace dfre;
namespace {using U=std::uint32_t;U next(std::istream& s){std::uint64_t n;if(!(s>>n))throw MissionRoundServiceError("Short round row");return U(n);}std::vector<std::uint8_t> bytes(std::istream& s,std::size_t n){std::string t;if(!(s>>t)||t.size()!=n*2)throw MissionRoundServiceError("Short round image");std::vector<std::uint8_t> v(n);for(std::size_t i=0;i<n;++i)v[i]=std::uint8_t(std::stoul(t.substr(i*2,2),nullptr,16));return v;}std::string hex(const std::vector<std::uint8_t>& v){std::ostringstream s;for(auto n:v)s<<std::hex<<std::setw(2)<<std::setfill('0')<<unsigned(n);return s.str();}template<class V> U read(const V& v,U at,U width){U n=0;for(U i=0;i<width;++i)n|=U(v.at(at+i))<<(i*8);return n;}template<class V> void put(V& v,U at,U width,U n){for(U i=0;i<width;++i)v.at(at+i)=std::uint8_t(n>>(i*8));}}
int main(){try{std::string line;while(std::getline(std::cin,line)){if(line.empty())continue;std::istringstream in(line);const U mode=next(in),arg=next(in),first=next(in),second=next(in),edi=next(in),mutation=next(in),opened=next(in);std::map<U,U> cells;for(U a:{0x503428u,0x67f520u,0x67f53cu,0x67f52cu,0x6e5588u,0x6e5570u})cells[a]=next(in);auto squads=bytes(in,0x1a00);std::array<std::vector<std::uint8_t>,17> clients;for(auto& c:clients)c=bytes(in,512);std::ostringstream trace,access;U row_prints=0;MissionRoundServiceBindings b;
auto load=[&](U at,U width){access<<'R'<<at<<','<<width<<';';return at>=0x6e8090&&at<0x6e9a90?read(squads,at-0x6e8090,width):cells.at(at);};
auto save=[&](U at,U width,U value){access<<'W'<<at<<','<<width<<','<<value<<';';if(at>=0x6e8090&&at<0x6e9a90)put(squads,at-0x6e8090,width,value);else cells.at(at)=value;};b.cell=load;b.store=save;
b.client=[&](U slot,U at,U width){access<<'R'<<(0x6e9a90u+slot*0x9b84u+at)<<','<<width<<';';return read(clients.at(slot),at,width);};b.squad=[&](U slot,U at,U width){return load(0x6e8090u+slot*0x68u+at,width);};b.squad_store=[&](U slot,U at,U width,U value){save(0x6e8090u+slot*0x68u+at,width,value);};
b.text=[](U a){if(a!=0x67f478)throw MissionRoundServiceError("Bad game name address");return std::string("GAME");};b.client_name=[&](U slot){std::string name;for(U at=0xc;clients.at(slot).at(at);++at)name+=char(clients.at(slot).at(at));return name;};
b.file_open=[&](const std::string& path,const std::string& m){trace<<'O'<<path<<','<<m<<';';if(mutation&1){cells[0x67f53c]=(cells[0x67f53c]+1)%7;clients[0][0x18c]^=1;}return opened;};
b.local_time=[&](){trace<<"T;";return std::array<std::uint16_t,8>{{2026,10,4,22,18,3,4,500}};};
b.file_print=[&](U file,U format,const auto& args){trace<<'P'<<file<<','<<format;for(const auto& a:args){if(const auto* n=std::get_if<U>(&a))trace<<",I"<<*n;else trace<<",S"<<std::get<std::string>(a);}trace<<';';
if((mutation&2)&&format==0x4cc72c)cells[0x67f53c]=4;
if((mutation&4)&&format==0x4cc748)cells[0x67f53c]=5;
if((mutation&8)&&(format==0x4cc804||format==0x4cc81c||format==0x4cc830)){cells[0x67f53c]=2;put(squads,0xb4,4,123);}
if((mutation&16)&&(format==0x4cc840||format==0x4cc854||format==0x4cc86c)){cells[0x67f53c]=4;put(squads,0x114,4,0xffffffc3);}
if((mutation&32)&&format==0x4cc8c8)clients[16][0x18c]^=1;
if((mutation&64)&&(format==0x4cc8f8||format==0x4cc918)){if(row_prints++==0){clients[16][0x18c]=1;put(clients[16],0x194,4,0xffffffc3);cells[0x67f53c]=4;}}
};b.file_close=[&](U file){trace<<'X'<<file<<';';if(mutation&128)cells[0x6e5588]=17;};
std::int32_t result=0;bool fault=false;try{if(mode==0)mission_round_set_client_updates(b,arg);else if(mode==1)mission_round_collect_squads(b);else if(mode==2)result=mission_round_compare_clients(b,first,second);else if(mode==4)mission_round_clear_frame_flag(b);else mission_round_log(b,arg,edi==0xffffffffu?std::nullopt:std::optional<U>(edi));}catch(const MissionRoundServiceError&){fault=true;}
std::cout<<fault<<' '<<U(result);for(U a:{0x503428u,0x67f520u,0x67f53cu,0x67f52cu,0x6e5588u,0x6e5570u})std::cout<<' '<<cells.at(a);std::cout<<' '<<hex(squads);for(const auto& c:clients)std::cout<<' '<<hex(c);std::cout<<' '<<(trace.str().empty()?"-":trace.str())<<' '<<(access.str().empty()?"-":access.str())<<'\n';
}}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}return 0;}
