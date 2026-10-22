#include "dfre/mission_audio_cache.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <map>
using namespace dfre;
namespace {using U=std::uint32_t;constexpr U A=0x1300000,D=0x1302000,E=0x1301000;const U keys[]={0xd09730,0xd096e0,0xd096d8,0xd09738,0xd09398,0x5013a0,0xd096d0,0x4cdff0,E};U next(std::istream& s){std::uint64_t n;if(!(s>>n))throw MissionAudioCacheError("Short cache row");return U(n);}std::vector<std::uint8_t> image(std::istream& s,std::size_t n){std::string t;if(!(s>>t)||t.size()!=n*2)throw MissionAudioCacheError("Short cache image");std::vector<std::uint8_t> b(n);for(std::size_t i=0;i<n;++i)b[i]=std::uint8_t(std::stoul(t.substr(i*2,2),nullptr,16));return b;}std::string hex(const std::vector<std::uint8_t>& b){std::ostringstream s;for(auto n:b)s<<std::hex<<std::setw(2)<<std::setfill('0')<<unsigned(n);return s.str();}}
int main(){try{std::string line;while(std::getline(std::cin,line)){if(line.empty())continue;std::istringstream in(line);const U mode=next(in),arg=next(in),arg2=next(in),mutation=next(in),allocated=next(in),returned=next(in);std::map<U,U> cells;for(U a:keys)cells[a]=next(in);auto asset=image(in,64),data=image(in,272),incoming=image(in,256);std::ostringstream trace,access;
auto rawread=[&](U at,U width){U n=0;for(U i=0;i<width;++i){std::uint8_t value=0;if(at+i>=A&&at+i<A+64)value=asset.at(at+i-A);else if(at+i>=D-16&&at+i<D+256)value=data.at(at+i-(D-16));else{auto it=cells.find(at);if(it==cells.end())throw MissionAudioCacheError("Unbound cache word");return it->second;}n|=U(value)<<(i*8);}return n;};
auto rawwrite=[&](U at,U width,U n){if(at>=A&&at<A+64){for(U i=0;i<width;++i)asset.at(at+i-A)=std::uint8_t(n>>(8*i));}else if(at>=D-16&&at<D+256){for(U i=0;i<width;++i)data.at(at+i-(D-16))=std::uint8_t(n>>(8*i));}else{if(!cells.count(at))throw MissionAudioCacheError("Unbound cache store");cells[at]=n;}};
MissionAudioCacheBindings b;b.read=[&](U at,U width){access<<'R'<<at<<','<<width<<';';return rawread(at,width);};b.write=[&](U at,U width,U n){access<<'W'<<at<<','<<width<<','<<n<<';';rawwrite(at,width,n);};
b.call=[&](U site,const auto& args){trace<<'C'<<site;for(U v:args)trace<<','<<v;trace<<';';
if(site==0x497f60){if(mutation&1){rawwrite(A+0x18,4,77);rawwrite(A+0x1c,4,100);}if(mutation&2)cells[0x5013a0]=91;return allocated;}
if(site==0x498d60||site==0x49e6e0){if(mutation&4){rawwrite(A+0x1c,4,88);cells[0xd09730]^=1;}return returned;}
if(site==0x498d30||site==0x49e6b0){for(U i=0;i<args.at(1)&&i<incoming.size();++i)rawwrite(args.at(0)+i,1,incoming[i]);if(mutation&8){rawwrite(A+0x23,1,9);rawwrite(A+0x24,4,91);}if(mutation&16)++cells[0x4cdff0];return returned;}
if(site==0x4c6854){if(mutation&32)cells[0xd096e0]=77;return returned;}
throw MissionAudioCacheError("Unexpected cache service");};
U result=0;bool fault=false;try{switch(mode){case 0:mission_audio_cache_load(b,A,arg);break;case 1:result=mission_audio_cache_convert(b,D);break;case 2:result=mission_audio_cache_allocate_alternate(b,arg);break;case 3:result=mission_audio_cache_tag(b,arg,arg2);break;case 4:result=mission_audio_cache_seek(b,arg,arg2);break;case 5:result=mission_audio_cache_read(b,D,arg);break;case 6:result=mission_audio_cache_close(b);break;case 7:result=mission_audio_cache_close_loose(b);break;default:throw MissionAudioCacheError("Bad cache mode");}}catch(const MissionAudioCacheError&){fault=true;}
std::cout<<fault<<' '<<result;for(U a:keys)std::cout<<' '<<cells.at(a);std::cout<<' '<<hex(asset)<<' '<<hex(data)<<' '<<(trace.str().empty()?"-":trace.str())<<' '<<(access.str().empty()?"-":access.str())<<'\n';
}}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}return 0;}
