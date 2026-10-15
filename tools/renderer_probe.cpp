#include "dfre/renderer.hpp"
#include <iostream>
#include <string>
int main(int argc,char** argv) {
    if(argc>2 || (argc==2 && std::string(argv[1])!="texture32" && std::string(argv[1])!="texture32-shade" && std::string(argv[1])!="texture32-cutout" && std::string(argv[1])!="spans"))return 2;
    const bool texture=argc>1 && std::string(argv[1])!="spans";
    const auto mode=argc>1 && std::string(argv[1])=="texture32-shade"?dfre::RasterTextureMode::varying_shade:argc>1 && std::string(argv[1])=="texture32-cutout"?dfre::RasterTextureMode::cutout:dfre::RasterTextureMode::uniform;
    const bool spans=argc>1;
    if(texture) {
        std::uint32_t test,bitsx,bitsy,shade;dfre::RasterSpan row;
        while(std::cin>>test>>bitsx>>bitsy>>shade>>row.x>>row.y>>row.length>>row.u>>row.v>>row.depth>>row.du>>row.dv>>row.ddepth) {
            if(mode==dfre::RasterTextureMode::varying_shade && !(std::cin>>row.shade>>row.dshade))return 2;
            if(bitsx>8 || bitsy>8)return 2;
            dfre::RasterTexture32 image;image.width_bits=std::uint8_t(bitsx);image.height_bits=std::uint8_t(bitsy);image.uniform_shade=shade;
            image.pixels.resize(std::size_t(1)<<(bitsx+bitsy));image.shade_colours.resize(64*256);
            for(std::size_t i=0;i<image.pixels.size();++i)image.pixels[i]=std::uint8_t(i*37+test*13);
            for(std::size_t i=0;i<image.shade_colours.size();++i)image.shade_colours[i]=std::uint32_t(i)*0x01010101u^0xffa5c3e7u;
            std::vector<std::uint32_t> colours(64*8);std::vector<std::uint16_t> depths(64*8);
            for(std::size_t i=0;i<colours.size();++i){colours[i]=std::uint32_t(i)*1234567u^0xabcdef90u;depths[i]=std::uint16_t(i*317+test*17);}
            dfre::draw_textured_span32(row,64,8,colours,depths,image,mode);
            for(std::size_t i=0;i<colours.size();++i)std::cout<<colours[i]<<' '<<depths[i]<<' ';
            std::cout<<'\n';
        }
        return std::cin.eof()?0:2;
    }
    dfre::RasterClip clip;std::array<dfre::RasterVertex,3> triangle{};
    while(std::cin>>clip.left>>clip.top>>clip.right>>clip.bottom) {
        for(auto& vertex:triangle)if(!(std::cin>>vertex.x>>vertex.y))return 2;
        if(spans) {
            for(auto& vertex:triangle)if(!(std::cin>>vertex.depth>>vertex.u>>vertex.v>>vertex.shade))return 2;
            std::uint32_t flags;if(!(std::cin>>flags))return 2;
            const auto gradients=dfre::raster_triangle_gradients(triangle,flags);
            for(const auto& pair:{gradients.u,gradients.v,gradients.depth,gradients.shade})for(auto value:pair)std::cout<<value<<' ';
            const auto rows=dfre::raster_triangle_spans(triangle,clip,flags);std::cout<<rows.size();
            for(const auto& row:rows)std::cout<<' '<<row.x<<' '<<row.y<<' '<<row.length<<' '<<row.u<<' '<<row.v<<' '<<row.depth<<' '<<row.shade<<' '<<row.du<<' '<<row.dv<<' '<<row.ddepth<<' '<<row.dshade;
            std::cout<<'\n';continue;
        }
        const auto result=dfre::classify_raster_triangle(triangle,clip);
        std::cout<<result.extent<<' '<<result.clip_flags;
        for(auto index:result.x_order)std::cout<<' '<<index;
        for(auto index:result.y_order)std::cout<<' '<<index;
        std::cout<<'\n';
    }
    return std::cin.eof()?0:2;
}
