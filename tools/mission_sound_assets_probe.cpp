#include "dfre/mission_sound_assets.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <map>
using namespace dfre;
namespace {using U=std::uint32_t;constexpr U B=0x1300000,C=B,N=B+0x100,S=B+0x200,ALT=B+0xc00,AB=B+0x1000,P=B+0x2000,Q=B+0x2200,R=B+0x2400;const U keys[]={0xd096e4,0xd096d8,0xd0973c,0xd096e8,0xd096dc,0xd09730,0xd09734,0xd09738,0xd096e0,0xd096f0,0xd096f4,0xd096f8,0xd096fc,0xd09700,0xd09704,0xd09708,0xd0970c,0xd09710,0xd09714,0xd09718,0xd0971c,0xd09720,0xd09724,0xd09728,0xd0972c,0x5014f8};U next(std::istream& s){std::uint64_t n;if(!(s>>n))throw MissionSoundAssetError("Short sound asset row");return U(n);}std::vector<std::uint8_t> image(std::istream& s,std::size_t n){std::string t;if(!(s>>t)||t.size()!=n*2)throw MissionSoundAssetError("Short sound asset image");std::vector<std::uint8_t> b(n);for(std::size_t i=0;i<n;++i)b[i]=std::uint8_t(std::stoul(t.substr(i*2,2),nullptr,16));return b;}std::string hex(const std::vector<std::uint8_t>& b,U at,U size){std::ostringstream s;for(U i=0;i<size;++i)s<<std::hex<<std::setw(2)<<std::setfill('0')<<unsigned(b.at(at+i));return s.str();}}
int main(){try{std::string line;while(std::getline(std::cin,line)){if(line.empty())continue;std::istringstream in(line);const U mode=next(in),mutation=next(in),fail_at=next(in),query=next(in),tag=next(in),scenario=next(in),success=next(in),edi=next(in);std::map<U,U> cells;for(U k:keys)cells[k]=next(in);auto memory=image(in,0xa000),file=image(in,0x1200);U pos=0,heap=B+0x4000,allocations=0,last_layer=0,cache_calls=0,open_calls=0;std::ostringstream trace;
auto read=[&](U at,U width){if(at>=B&&at+width<=B+memory.size()){U n=0;for(U i=0;i<width;++i)n|=U(memory.at(at-B+i))<<(8*i);return n;}if(width==4&&cells.count(at))return cells.at(at);throw MissionSoundAssetError("Unbound sound asset read");};
auto write=[&](U at,U width,U n){if(at>=B&&at+width<=B+memory.size()){for(U i=0;i<width;++i)memory.at(at-B+i)=std::uint8_t(n>>(8*i));return;}if(width==4&&cells.count(at)){cells.at(at)=n;return;}throw MissionSoundAssetError("Unbound sound asset write");};
MissionSoundAssetBindings b;b.read=read;b.write=write;b.call=[&](U site,const auto& args){trace<<'C'<<site;for(U v:args)trace<<','<<v;trace<<';';
if(site==0x497f60){++allocations;if(allocations==fail_at)return 0u;const U out=heap+16;heap=(out+args.at(0)+15u)&~15u;if(heap>B+0x8000)throw MissionSoundAssetError("Fixture heap full");if((mutation&32)&&args.at(3)==0x4ce458&&last_layer){write(last_layer,2,1);write(C+0x1c,4,ALT);}return out;}
if(site==0x499120){pos=0;if((mutation&1)&&++open_calls==1){write(C+0x1c,4,ALT);write(C+8,4,1);}return scenario==6?0xffffffffu:123u;}
if(site==0x4992d0){pos=args.at(0);if((mutation&4)&&pos==0x500)write(C+0x38,4,AB);return pos;}
if(site==0x499280){const U destination=args.at(0),count=args.at(1);if(count>0x1200||pos+count>file.size())throw MissionSoundAssetError("Fixture file range exceeded");if(!(mutation&512)){for(U i=0;i<count;++i)write(destination+i,1,file.at(pos+i));pos+=count;}if((mutation&2)&&destination==S)write(C+0x10,4,0x440);if((mutation&64)&&destination==C)write(C+8,4,1);if(count==0x30)last_layer=destination;return (mutation&512)?0u:count;}
if(site==0x499020){if(mutation&1024){cells[0x5014f8]=B+0x9020;write(C+0xc4,4,0x77);}return 0u;}
if(site==0x49d050){const U asset=args.at(0);write(asset+0x23,1,1);write(asset+0x24,4,B+0x9000+cache_calls++*16);if(mutation&8){write(C+0xc4,4,0x42);write(C+8,4,1);}if(mutation&16)write(C+0x1c,4,ALT);return 0u;}
if(site==0x498230){if(args.at(0)&&args.at(1)<0x1000000)write(args.at(0)-12,4,args.at(1));return args.at(0)?args.at(0)-16:0;}
if(site==0x495160)return 0u;
if(site==0x498cf0){cells[0xd096e8]=(scenario==0?0:1);if(mutation&1)cells[0xd096d8]=P;return scenario==0?123u:0xffffffffu;}
if(site==0x498fb0){if(mutation&128){cells[0xd096d8]=Q;cells[0xd096f0]=R;}return N+32;}
if(site==0x49e630){const U archive=args.at(1);++open_calls;write(archive+0xac,4,archive==P?(scenario==1?0:1):(open_calls==success?0:1));if(mutation&256){cells[0xd096d8]=Q;write(Q+0xac,4,0);cells[0xd096f0]=R;write(R+0xac,4,0);}return 200u+open_calls;}
if(site==0x499750)return 0u;
throw MissionSoundAssetError("Unexpected sound asset fixture");};
U result=0;bool fault=false;try{switch(mode){case 0:result=mission_sound_context_load(b,N,C,tag,S);break;case 1:mission_sound_fixup(b,scenario==7?0:C,S);break;case 2:result=mission_sound_find_sample(b,query,C);break;case 3:result=mission_sound_find_bank(b,query,C);break;case 4:result=mission_sound_mark_sample(b,query,C);break;case 5:result=mission_sound_preload(b,C);break;case 6:result=mission_sound_allocate(b,query,4,0,0x55,tag);break;case 7:result=mission_sound_file_open(b,N,edi);break;default:throw MissionSoundAssetError("Bad sound asset mode");}}catch(const MissionSoundAssetError&){fault=true;}
U crc=0xffffffffu;for(auto v:memory){crc^=v;for(U bit=0;bit<8;++bit)crc=(crc>>1)^((crc&1)?0xedb88320u:0);}crc^=0xffffffffu;
std::cout<<fault<<' '<<result;for(U k:keys)std::cout<<' '<<cells.at(k);std::cout<<' '<<crc<<' '<<hex(memory,0,0xc8)<<' '<<hex(memory,0x4000,0x1800)<<' '<<(trace.str().empty()?"-":trace.str())<<'\n';
}}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}return 0;}
