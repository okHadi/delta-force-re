#include "dfre/renderer.hpp"
#include <iostream>
#include <string>
int main(int argc,char** argv) {
    if(argc>2 || (argc==2 && std::string(argv[1])!="texture32" && std::string(argv[1])!="texture32-shade" && std::string(argv[1])!="texture32-cutout" && std::string(argv[1])!="spans" && std::string(argv[1])!="lighting" && std::string(argv[1])!="clip" && std::string(argv[1])!="tiny32" && std::string(argv[1])!="local-lighting"))return 2;
    if(argc==2 && std::string(argv[1])=="local-lighting") {
        dfre::ModelMatrix matrix;dfre::ModelLighting lighting;std::int32_t scale;
        while(std::cin>>matrix[0]) {
            for(std::size_t i=1;i<12;++i)if(!(std::cin>>matrix[i]))return 2;
            for(auto& value:lighting.light_direction)if(!(std::cin>>value))return 2;
            for(auto& value:lighting.view_direction)if(!(std::cin>>value))return 2;
            if(!(std::cin>>lighting.ambient>>lighting.diffuse>>lighting.specular>>scale))return 2;
            const auto local=dfre::localize_model_lighting(matrix,lighting,scale);
            for(auto value:local.light_direction)std::cout<<value<<' ';
            for(auto value:local.view_direction)std::cout<<value<<' ';
            std::cout<<local.ambient<<' '<<local.diffuse<<' '<<local.specular<<'\n';
        }
        return std::cin.eof()?0:2;
    }
    if(argc==2 && std::string(argv[1])=="tiny32") {
        std::uint32_t test,flags,flat;
        while(std::cin>>test>>flags>>flat) {
            std::array<dfre::RasterVertex,3> triangle;
            for(auto& vertex:triangle)if(!(std::cin>>vertex.x>>vertex.y>>vertex.depth))return 2;
            std::vector<std::uint32_t> colours(512);std::vector<std::uint16_t> depths(512);
            for(std::size_t i=0;i<512;++i){colours[i]=std::uint32_t(i)*1234567u^0xabcdef90u;depths[i]=std::uint16_t(i*317+test*17);}
            dfre::draw_tiny_triangle32(triangle,{0,0,63,7},flat,flags,64,8,colours,depths);
            for(std::size_t i=0;i<512;++i)std::cout<<colours[i]<<' '<<depths[i]<<' ';
            std::cout<<'\n';
        }
        return std::cin.eof()?0:2;
    }
    if(argc==2 && std::string(argv[1])=="clip") {
        std::uint32_t count;
        while(std::cin>>count) {
            if(count>5)return 2;
            std::vector<std::array<std::int32_t,3>> positions(3);dfre::ModelFace face{};face.vertices={0,1,2};
            for(auto& point:positions)for(auto& value:point)if(!(std::cin>>value))return 2;
            for(auto& value:face.u)if(!(std::cin>>value))return 2;
            for(auto& value:face.v)if(!(std::cin>>value))return 2;
            std::vector<dfre::ModelClipPlane> planes(count);
            for(auto& plane:planes) {
                for(auto& value:plane.normal)if(!(std::cin>>value))return 2;
                if(!(std::cin>>plane.offset))return 2;
            }
            const auto result=dfre::clip_model_geometry(positions,{face},planes);
            std::cout<<result.positions.size()<<' '<<result.faces.size();
            for(const auto& point:result.positions)for(auto value:point)std::cout<<' '<<value;
            for(const auto& clipped:result.faces) {
                std::cout<<' '<<clipped.rejected;
                for(auto value:clipped.face.vertices)std::cout<<' '<<value;
                for(auto value:clipped.face.u)std::cout<<' '<<value;
                for(auto value:clipped.face.v)std::cout<<' '<<value;
            }
            std::cout<<'\n';
        }
        return std::cin.eof()?0:2;
    }
    if(argc==2 && std::string(argv[1])=="lighting") {
        dfre::ModelNormal normal;dfre::ModelMaterial material;dfre::ModelLighting lighting;std::int32_t specular,constant;std::uint32_t power,reverse;
        while(std::cin>>normal.direction[0]>>normal.direction[1]>>normal.direction[2]>>material.flags>>specular>>constant>>power>>reverse
            >>lighting.light_direction[0]>>lighting.light_direction[1]>>lighting.light_direction[2]>>lighting.view_direction[0]>>lighting.view_direction[1]>>lighting.view_direction[2]
            >>lighting.ambient>>lighting.diffuse>>lighting.specular) {
            if(power>255)return 2;
            for(std::size_t i=0;i<4;++i){material.raw[56+i]=std::uint8_t(std::uint32_t(specular)>>(i*8));material.raw[60+i]=std::uint8_t(std::uint32_t(constant)>>(i*8));}
            material.raw[64]=std::uint8_t(power);
            std::cout<<dfre::shade_model_normal(normal,material,lighting,reverse!=0)<<'\n';
        }
        return std::cin.eof()?0:2;
    }
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
