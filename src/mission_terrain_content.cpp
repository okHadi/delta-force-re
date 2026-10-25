#include "dfre/mission_terrain_content.hpp"
#include "dfre/filename.hpp"
#include "dfre/network.hpp"
#include <algorithm>
namespace dfre {
std::uint32_t mission_terrain_crc(const MissionClientAddedBindings&b,std::uint32_t pointer,std::uint32_t count){
    if(!b.read)throw MissionClientAddedError("Terrain CRC memory reader is required");
    std::uint32_t crc=0xffffffffu;
    do{const auto byte=b.read(pointer,1)&255u;const auto index=((crc>>24)^byte)&255u;crc<<=8;const auto word=b.read(0x4f8720u+index*4,4);++pointer;crc^=word;}while(--count);
    return crc;
}
namespace {
bool same(std::string_view a,std::string_view b){return a.size()==b.size()&&std::equal(a.begin(),a.end(),b.begin(),[](char x,char y){return (x>='A'&&x<='Z'?x+'a'-'A':x)==(y>='A'&&y<='Z'?y+'a'-'A':y);});}
std::string name(const TerrainConfig&config,std::string_view key){const TerrainField*found=nullptr;for(const auto&f:config.fields)if(same(f.name,key)){if(found||f.values.size()!=1)throw AssetError("Invalid terrain content field: "+std::string(key));found=&f;}if(!found)throw AssetError("Missing terrain content field: "+std::string(key));return set_asset_extension(found->values[0],"PCX");}
std::uint32_t pixels(const AssetStore&assets,const TerrainConfig&config,std::string_view key,std::uint32_t width,bool square){const auto image=assets.load_image(name(config,key)).image;if(image.width!=width||(square&&image.height!=width)||image.pixels.empty()||image.pixels.size()!=std::uint64_t(image.width)*image.height)throw AssetError("Invalid terrain content image: "+std::string(key));return crc32_mpeg2(image.pixels.data(),image.pixels.size());}
}
std::array<std::uint32_t,9> mission_terrain_content(const AssetStore&assets,const TerrainRuntime&runtime){
    const auto&config=runtime.config;std::array<std::uint32_t,9>out{};
    out[1]=pixels(assets,config,"elev_map",1024,true);
    out[0]=pixels(assets,config,"color_map",1024,true);
    out[2]=pixels(assets,config,"detail_map",1024,true);
    out[3]=pixels(assets,config,"detail_color",16,false);
    out[5]=pixels(assets,config,"detail_shade",16,false);
    out[4]=pixels(assets,config,"detail_elev",16,false);
    const auto gradient=assets.load_image(name(config,"sky_palette")).image;
    if(gradient.palette.size()<54)throw AssetError("Terrain content gradient requires 54 palette bytes");
    out[8]=crc32_mpeg2(gradient.palette.data(),54);
    if(runtime.sky)out[7]=pixels(assets,config,"sky_map",512,false);
    if(runtime.water_height!=0){if(!runtime.water||!runtime.water_palette)throw AssetError("Current terrain water resources are required");out[6]=pixels(assets,config,"water_map",256,false);}
    return out;
}
}
