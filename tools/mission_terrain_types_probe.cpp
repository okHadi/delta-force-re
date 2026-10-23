#include "dfre/mission_terrain_types.hpp"
#include "dfre/terrain.hpp"
#include <iomanip>
#include <iostream>
#include <sstream>
#include <unordered_map>
using namespace dfre;
namespace {using U=std::uint32_t;using H=std::uint64_t;
void hash(H& h,U kind,U at,U width,U value=0){for(auto n:{kind,at,width,value})for(U i=0;i<4;++i){h^=(n>>(i*8))&255;h*=1099511628211ull;}}
void print(const std::array<std::uint8_t,256>& result){for(auto n:result)std::cout<<' '<<U(n);}
}
int main(int argc,char** argv){try{
 if(argc==4&&std::string(argv[1])=="--stock-fixture"){AssetStore store(argv[2]);const auto t=load_terrain_runtime(store,argv[3]);std::cout<<1<<' ';for(auto n:t.color.palette)std::cout<<std::hex<<std::setw(2)<<std::setfill('0')<<unsigned(n);std::cout<<std::dec<<' '<<t.config.fields.size();for(const auto& f:t.config.fields){std::cout<<' '<<std::quoted(f.name)<<' '<<f.values.size();for(const auto& value:f.values)std::cout<<' '<<std::quoted(value);}std::cout<<'\n';return 0;}
 std::string line;while(std::getline(std::cin,line)){std::istringstream in(line);U mode;if(!(in>>mode))continue;
 if(mode==1){std::string hex;U count;in>>hex>>count;if(hex.size()!=1536)throw AssetError("Short terrain palette fixture");PcxImage image;for(U i=0;i<768;++i)image.palette.push_back(std::uint8_t(std::stoul(hex.substr(i*2,2),nullptr,16)));TerrainConfig config;for(U i=0;i<count;++i){TerrainField f;U n;in>>std::quoted(f.name)>>n;for(U j=0;j<n;++j){std::string value;in>>std::quoted(value);f.values.push_back(value);}config.fields.push_back(f);}std::cout<<0;print(mission_terrain_types(image,config));std::cout<<'\n';continue;}
 U seed,count,mutation;in>>seed>>count>>mutation;std::unordered_map<U,std::uint8_t> memory;auto put=[&](U at,U n,U w=4){for(U i=0;i<w;++i)memory[at+i]=std::uint8_t(n>>(8*i));};auto get=[&](U at,U w=4){U n=0;for(U i=0;i<w;++i)n|=U(memory[at+i])<<(8*i);return n;};for(U i=0;i<768;++i)put(0x1200000+i,(seed+i*37+(i>>2)*11)&255,1);for(U i=0;i<16;++i)put(0xbd4f98+i,(seed*3+i*53)&255,1);put(0xbd527c,count);for(U i=0;i<256;++i)put(0xbd5334+i,0xa5,1);
 H h=1469598103934665603ull;U count_reads=0;bool used=false,failed=false;MissionTerrainTypeBindings b;
 b.read=[&](U at,U width){hash(h,1,at,width);if(at==0xbd527c){++count_reads;if((mutation==1&&count_reads==1)||(mutation==7&&count_reads==2))put(at,0);if(mutation==9&&count_reads==1)put(at,0x80000000);}if(!used){if((mutation==2&&at==0x1200002)||(mutation==10&&at==0xbd4f98)){put(0xbd527c,mutation==10?3u:0u);used=true;}if((mutation==3&&at==0xbd4f98)||(mutation==4&&at==0xbd4f99)){put(0x1200002,231,1);used=true;}if(mutation==5&&at>=0xbd4f9b&&at<0xbd4fa8&&(at-0xbd4f9b)%4==0){put(at,99,1);used=true;}if(mutation==11&&at==0x1200000){put(0xbd4f98,42,1);used=true;}}return get(at,width);};
 b.write=[&](U at,U width,U n){hash(h,2,at,width,n);if(mutation==8&&at==0xbd533b)throw AssetError("Terrain write test error");put(at,n,width);if(mutation==6&&at==0xbd5334)put(0x1200003,254,1);};try{mission_terrain_classify(b,0x1200000);}catch(const AssetError&){failed=true;}std::cout<<failed;std::array<std::uint8_t,256> out{};for(U i=0;i<256;++i)out[i]=std::uint8_t(get(0xbd5334+i,1));print(out);std::cout<<' '<<h<<'\n';}
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
