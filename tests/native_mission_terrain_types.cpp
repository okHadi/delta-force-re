#include "dfre/mission_terrain_types.hpp"
#include <cassert>
#include <map>
using namespace dfre;
namespace {using U=std::uint32_t;template<class F>void rejects(F f){bool failed=false;try{f();}catch(const AssetError&){failed=true;}assert(failed);}}
int main(){
 PcxImage image;image.palette.resize(768);TerrainConfig config;
 auto types=mission_terrain_types(image,config);for(auto type:types)assert(type==1);
 config.fields={{"type",{"0","0","0","grass"}},{"type",{"0","0","0","sand"}},{"type",{"255","255","255","snow"}}};image.palette[3]=image.palette[4]=image.palette[5]=255;types=mission_terrain_types(image,config);assert(types[0]==2&&types[1]==3);
 config.fields={{"TyPe",{"-1","256","4294967297","CEMENT"}}};image.palette[0]=255;image.palette[1]=0;image.palette[2]=1;types=mission_terrain_types(image,config);assert(types[0]==4);
 config.fields={{"type",{"0","0","0","unknown"}}};types=mission_terrain_types(image,config);assert(types[0]==0);
 config.fields.assign(100,{"type",{"255","255","255","grass"}});config.fields.push_back({"type",{"255","0","1","snow"}});types=mission_terrain_types(image,config);assert(types[0]==2);
 image.palette.pop_back();rejects([&]{mission_terrain_types(image,config);});rejects([&]{mission_terrain_classify({},0);});
 std::map<U,std::uint8_t> bytes;auto put=[&](U at,U value,U width=4){for(U i=0;i<width;++i)bytes[at+i]=std::uint8_t(value>>(8*i));};auto get=[&](U at,U width){U n=0;for(U i=0;i<width;++i)n|=U(bytes[at+i])<<(8*i);return n;};
 put(0xbd527c,1);put(0xbd4f9b,6,1);MissionTerrainTypeBindings b;U count_reads=0,writes=0;bool blue_changed=false;
 b.read=[&](U at,U width){if(at==0xbd527c)++count_reads;if(at==0x1200002&&!blue_changed){put(0xbd527c,0);blue_changed=true;}return get(at,width);};b.write=[&](U at,U width,U value){assert(width==1&&at==0xbd5334+writes++);put(at,value,width);};mission_terrain_classify(b,0x1200000);assert(writes==256&&count_reads==513&&get(0xbd5334,1)==1);
 put(0xbd527c,0xffffffff);writes=0;count_reads=0;mission_terrain_classify(b,0x1200000);assert(writes==256&&count_reads==512&&get(0xbd5334,1)==6);
 U done=0;b.write=[&](U at,U width,U value){if(at==0xbd5338)throw AssetError("Terrain write test error");put(at,value,width);++done;};rejects([&]{mission_terrain_classify(b,0x1200000);});assert(done==4);
}
