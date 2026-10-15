#include "dfre/renderer.hpp"
#include <iostream>
#include <string>
int main(int argc,char** argv) {
    if(argc>2 || (argc==2 && std::string(argv[1])!="texture32" && std::string(argv[1])!="texture32-shade" && std::string(argv[1])!="texture32-cutout" && std::string(argv[1])!="texture32-shade-cutout" && std::string(argv[1])!="spans" && std::string(argv[1])!="lighting" && std::string(argv[1])!="clip" && std::string(argv[1])!="tiny32" && std::string(argv[1])!="local-lighting" && std::string(argv[1])!="camera" && std::string(argv[1])!="faces" && std::string(argv[1])!="flat32" && std::string(argv[1])!="triangle32" && std::string(argv[1])!="mesh32" && std::string(argv[1])!="scene-lighting"))return 2;
    if(argc==2 && std::string(argv[1])=="scene-lighting") {
        dfre::ModelMatrix matrix;std::array<std::int32_t,3> direction;std::int32_t ambient,diffuse,specular;
        while(std::cin>>matrix[0]) {
            for(std::size_t k=1;k<matrix.size();++k)if(!(std::cin>>matrix[k]))return 2;
            for(auto& value:direction)if(!(std::cin>>value))return 2;
            if(!(std::cin>>ambient>>diffuse>>specular))return 2;
            const auto lighting=dfre::make_model_scene_lighting(matrix,direction,ambient,diffuse,specular);
            for(auto value:lighting.light_direction)std::cout<<value<<' ';
            for(auto value:lighting.view_direction)std::cout<<value<<' ';
            std::cout<<lighting.ambient<<' '<<lighting.diffuse<<' '<<lighting.specular<<'\n';
        }return std::cin.eof()?0:2;
    }
    if(argc==2 && std::string(argv[1])=="mesh32") {
        std::uint32_t test,flags,smooth,bx,by,flat,math;std::int32_t specular,constant;std::uint32_t power;
        while(std::cin>>test>>flags>>smooth>>bx>>by>>flat>>math>>specular>>constant>>power) {
            if(bx>8 || by>8 || power>255 || math>1)return 2;
            dfre::ModelLod lod;lod.vertices.resize(3);lod.normals.resize(3);
            dfre::ModelFace face{};face.vertices={0,1,2};face.normals={0,1,2};face.flags=std::uint16_t(smooth);
            for(auto& vertex:lod.vertices)for(auto& value:vertex.position)if(!(std::cin>>value))return 2;
            for(auto& normal:lod.normals)for(auto& value:normal.direction)if(!(std::cin>>value))return 2;
            for(auto& value:face.u)if(!(std::cin>>value))return 2;
            for(auto& value:face.v)if(!(std::cin>>value))return 2;
            dfre::ModelLighting lighting;
            for(auto& value:lighting.light_direction)if(!(std::cin>>value))return 2;
            for(auto& value:lighting.view_direction)if(!(std::cin>>value))return 2;
            if(!(std::cin>>lighting.ambient>>lighting.diffuse>>lighting.specular))return 2;
            lod.faces={face};dfre::ModelMesh mesh;mesh.vertex_count=3;mesh.normal_count=3;mesh.face_count=1;lod.meshes={mesh};
            dfre::ModelMaterial material;material.flags=flags;
            for(std::size_t i=0;i<4;++i){material.raw[56+i]=std::uint8_t(std::uint32_t(specular)>>(i*8));material.raw[60+i]=std::uint8_t(std::uint32_t(constant)>>(i*8));}
            material.raw[64]=std::uint8_t(power);lod.materials={material};
            dfre::RasterTexture32 texture;texture.width_bits=std::uint8_t(bx);texture.height_bits=std::uint8_t(by);
            texture.pixels.resize(std::size_t(1)<<(bx+by));texture.shade_colours.resize(64*256);
            for(std::size_t i=0;i<texture.pixels.size();++i)texture.pixels[i]=std::uint8_t(i*37+test*13);
            for(std::size_t i=0;i<texture.shade_colours.size();++i)texture.shade_colours[i]=std::uint32_t(i)*0x01010101u^0xffa5c3e7u;
            std::vector<std::uint32_t> colours(32*24);std::vector<std::uint16_t> depths(32*24);
            for(std::size_t i=0;i<colours.size();++i){colours[i]=std::uint32_t(i)*1234567u^0xabcdef90u;depths[i]=std::uint16_t(i*317+test*17);}
            const dfre::RasterMaterial32 binding{&texture,&texture.shade_colours,std::uint8_t(flat),flat};
            const auto camera=dfre::make_model_camera({},{},{0,0,31,23},90);
            dfre::draw_model_mesh32(lod,0,{dfre::model_identity_matrix()},camera,lighting,{binding},32,24,colours,depths,{},math?dfre::ModelVertexMath::mmx:dfre::ModelVertexMath::scalar);
            for(std::size_t i=0;i<colours.size();++i)std::cout<<colours[i]<<' '<<depths[i]<<' ';
            std::cout<<'\n';
        }
        return std::cin.eof()?0:2;
    }
    if(argc==2 && std::string(argv[1])=="triangle32") {
        std::uint32_t test,bx,by,flat;dfre::ModelRasterTriangle face;
        while(std::cin>>test>>face.flags>>bx>>by>>flat) {
            if(bx>8 || by>8)return 2;
            for(auto& v:face.vertices)if(!(std::cin>>v.x>>v.y>>v.depth>>v.u>>v.v>>v.shade))return 2;
            dfre::RasterTexture32 texture;texture.width_bits=std::uint8_t(bx);texture.height_bits=std::uint8_t(by);texture.uniform_shade=0x12340000;
            texture.pixels.resize(std::size_t(1)<<(bx+by));texture.shade_colours.resize(64*256);
            for(std::size_t i=0;i<texture.pixels.size();++i)texture.pixels[i]=std::uint8_t(i*37+test*13);
            for(std::size_t i=0;i<texture.shade_colours.size();++i)texture.shade_colours[i]=std::uint32_t(i)*0x01010101u^0xffa5c3e7u;
            std::vector<std::uint32_t> colours(32*24);std::vector<std::uint16_t> depths(32*24);
            for(std::size_t i=0;i<colours.size();++i){colours[i]=std::uint32_t(i)*1234567u^0xabcdef90u;depths[i]=std::uint16_t(i*317+test*17);}
            const dfre::RasterMaterial32 binding{&texture,&texture.shade_colours,std::uint8_t(flat),flat};
            dfre::draw_model_triangle32(face,{0,0,31,23},binding,32,24,colours,depths);
            for(std::size_t i=0;i<colours.size();++i)std::cout<<colours[i]<<' '<<depths[i]<<' ';
            std::cout<<'\n';
        }
        return std::cin.eof()?0:2;
    }
    if(argc==2 && std::string(argv[1])=="flat32") {
        std::uint32_t test,index;dfre::RasterSpan span;
        while(std::cin>>test>>index>>span.x>>span.y>>span.length>>span.depth>>span.ddepth>>span.shade>>span.dshade) {
            if(index>255)return 2;
            std::vector<std::uint32_t> shades(64*256),colours(512);std::vector<std::uint16_t> depths(512);
            for(std::size_t i=0;i<shades.size();++i)shades[i]=std::uint32_t(i)*0x01010101u^0xffa5c3e7u;
            for(std::size_t i=0;i<colours.size();++i){colours[i]=std::uint32_t(i)*1234567u^0xabcdef90u;depths[i]=std::uint16_t(i*317+test*17);}
            dfre::draw_flat_span32(span,64,8,colours,depths,std::uint8_t(index),shades);
            for(std::size_t i=0;i<colours.size();++i)std::cout<<colours[i]<<' '<<depths[i]<<' ';
            std::cout<<'\n';
        }
        return std::cin.eof()?0:2;
    }
    if(argc==2 && std::string(argv[1])=="faces") {
        std::uint32_t flags,flat_normal,material_index,normal_start;
        while(std::cin>>flags>>flat_normal>>material_index>>normal_start) {
            dfre::ModelFace face{};face.flags=std::uint16_t(flags);face.face_normal=std::uint16_t(flat_normal);face.material=material_index;face.vertices={0,1,2};
            for(auto& value:face.normals)if(!(std::cin>>value))return 2;
            std::array<std::uint32_t,11> counters{};for(auto& value:counters)if(!(std::cin>>value))return 2;
            std::uint32_t count;if(!(std::cin>>count) || count>1024)return 2;
            std::vector<dfre::ModelMaterial> materials(count);
            for(auto& material:materials) {
                std::uint32_t parameter,power;std::int32_t specular,constant;
                if(!(std::cin>>material.flags>>parameter>>specular>>constant>>power) || parameter>255 || power>255)return 2;
                material.animation_parameter=std::uint8_t(parameter);material.raw[64]=std::uint8_t(power);
                for(std::size_t i=0;i<4;++i){material.raw[56+i]=std::uint8_t(std::uint32_t(specular)>>(i*8));material.raw[60+i]=std::uint8_t(std::uint32_t(constant)>>(i*8));}
            }
            if(!(std::cin>>count) || count>32768)return 2;
            std::vector<dfre::ModelNormal> normals(count);
            for(auto& normal:normals)for(auto& value:normal.direction)if(!(std::cin>>value))return 2;
            dfre::ModelLighting lighting;
            for(auto& value:lighting.light_direction)if(!(std::cin>>value))return 2;
            for(auto& value:lighting.view_direction)if(!(std::cin>>value))return 2;
            if(!(std::cin>>lighting.ambient>>lighting.diffuse>>lighting.specular))return 2;
            std::vector<dfre::ModelProjectedVertex> positions(3);
            for(std::size_t k=0;k<3;++k) {
                auto& point=positions[k];point.projected=true;
                if(!(std::cin>>point.screen[0]>>point.screen[1]>>point.camera[0]>>face.u[k]>>face.v[k]))return 2;
            }
            const auto result=dfre::prepare_model_face(face,positions,normals,normal_start,materials,lighting,counters);
            if(!result)std::cout<<"0\n";
            else {
                std::cout<<"1 "<<result->flags<<' '<<result->material;
                for(const auto& v:result->vertices)std::cout<<' '<<v.x<<' '<<v.y<<' '<<v.depth<<' '<<v.u<<' '<<v.v<<' '<<v.shade;
                std::cout<<'\n';
            }
        }
        return std::cin.eof()?0:2;
    }
    if(argc==2 && std::string(argv[1])=="camera") {
        std::array<std::int32_t,3> position;std::array<std::uint32_t,3> angles;dfre::RasterClip clip;std::uint32_t fov;
        while(std::cin>>clip.left>>clip.top>>clip.right>>clip.bottom>>fov>>position[0]>>position[1]>>position[2]>>angles[0]>>angles[1]>>angles[2]) {
            const auto camera=dfre::make_model_camera(position,angles,clip,fov);
            std::cout<<camera.projection.focal_length<<' '<<camera.projection.centre[0]<<' '<<camera.projection.centre[1]<<' '<<camera.projection.near_depth;
            for(auto value:camera.matrix)std::cout<<' '<<value;
            for(const auto& plane:camera.planes){for(auto value:plane.normal)std::cout<<' '<<value;std::cout<<' '<<plane.offset;}
            std::cout<<'\n';
        }
        return std::cin.eof()?0:2;
    }
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
    const auto mode=argc>1 && std::string(argv[1])=="texture32-shade-cutout"?dfre::RasterTextureMode::varying_cutout:argc>1 && std::string(argv[1])=="texture32-shade"?dfre::RasterTextureMode::varying_shade:argc>1 && std::string(argv[1])=="texture32-cutout"?dfre::RasterTextureMode::cutout:dfre::RasterTextureMode::uniform;
    const bool spans=argc>1;
    if(texture) {
        std::uint32_t test,bitsx,bitsy,shade;dfre::RasterSpan row;
        while(std::cin>>test>>bitsx>>bitsy>>shade>>row.x>>row.y>>row.length>>row.u>>row.v>>row.depth>>row.du>>row.dv>>row.ddepth) {
            if((mode==dfre::RasterTextureMode::varying_shade || mode==dfre::RasterTextureMode::varying_cutout) && !(std::cin>>row.shade>>row.dshade))return 2;
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
