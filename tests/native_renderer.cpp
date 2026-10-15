#include "dfre/renderer.hpp"
#include <iostream>
#include <stdexcept>
#include <utility>
namespace {void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}}
int main() {
    const dfre::RasterClip clip{0,0,639,479};std::array<dfre::RasterVertex,3> triangle{};
    auto result=dfre::classify_raster_triangle(triangle,clip);
    check(result.extent==1 && result.clip_flags==0,"one-pixel triangle changed");
    triangle[0].x=-1;triangle[1].x=65536;triangle[2].y=65536;
    result=dfre::classify_raster_triangle(triangle,clip);
    check(result.extent==3 && result.clip_flags==1,"subpixel clip changed");
    triangle[0].x=triangle[1].x=triangle[2].x=-65536;
    check(dfre::classify_raster_triangle(triangle,clip).extent==0,"outside triangle accepted");
    try{dfre::classify_raster_triangle(triangle,{2,0,1,1});check(false,"reversed clip accepted");}catch(const std::invalid_argument&){}
    triangle={dfre::RasterVertex{0,0,65536,0,0,0},dfre::RasterVertex{65536,65536,65536,0,0,0},dfre::RasterVertex{131072,0,65536,0,0,0}};
    auto rows=dfre::raster_triangle_spans(triangle,clip,2);
    check(rows.size()==1 && rows[0].x==1 && rows[0].y==0 && rows[0].length==1 && rows[0].depth==65536,"triangle edge fill changed");
    std::swap(triangle[1],triangle[2]);
    check(dfre::raster_triangle_spans(triangle,clip,2).empty(),"reversed triangle produced spans");
    triangle={dfre::RasterVertex{0,0,0,0,0,0},dfre::RasterVertex{10*65536,0,65536,0,0,0},dfre::RasterVertex{0,10*65536,0,0,0,0}};
    auto gradients=dfre::raster_triangle_gradients(triangle,2);
    check(gradients.depth==std::array<std::int32_t,2>{6554,0},"depth gradient rounding changed");
    check(gradients.u==std::array<std::int32_t,2>{0,0},"disabled texture gradients changed");
    try{dfre::raster_triangle_spans(triangle,{-1,0,639,479},2);check(false,"negative raster screen accepted");}catch(const std::invalid_argument&){}
    dfre::RasterTexture32 texture;texture.width_bits=1;texture.pixels={4,9};texture.shade_colours.resize(64*256);texture.shade_colours[4]=0x1234;texture.shade_colours[9]=0x5678;
    std::vector<std::uint32_t> colours(4);std::vector<std::uint16_t> depths{1,1,2,2};dfre::RasterSpan span;span.length=3;span.du=65536;span.ddepth=65536;
    check(dfre::draw_textured_span32(span,4,1,colours,depths,texture)==2,"textured depth write count changed");
    check(colours==std::vector<std::uint32_t>{0x1234,0x5678,0,0} && depths==std::vector<std::uint16_t>{0,0,2,2},"paired depth or texture wrap changed");
    depths={1,1,1,1};colours.assign(4,0);texture.shade_colours[2*256+4]=0xabcd;span.ddepth=0;span.dshade=65536;
    dfre::draw_textured_span32(span,4,1,colours,depths,texture,dfre::RasterTextureMode::varying_shade);
    check(colours==std::vector<std::uint32_t>{0x1234,0x5678,0xabcd,0},"paired shade step changed");
    depths={1,1,1,1};colours.assign(4,0);texture.pixels[0]=0;
    check(dfre::draw_textured_span32(span,4,1,colours,depths,texture,dfre::RasterTextureMode::cutout)==1 && depths==std::vector<std::uint16_t>{1,0,1,1},"zero texel changed colour or depth");
    depths={1,1,1,1};colours.assign(4,0);span.length=4;texture.shade_colours[2*256+9]=0x9abc;
    check(dfre::draw_textured_span32(span,4,1,colours,depths,texture,dfre::RasterTextureMode::varying_cutout)==2 && colours==std::vector<std::uint32_t>{0,0x5678,0,0x9abc} && depths==std::vector<std::uint16_t>{1,0,1,0},"varying cutout changed paired shade or transparent depth");
    span.x=3;span.length=2;
    try{dfre::draw_textured_span32(span,4,1,colours,depths,texture);check(false,"out-of-range texture span accepted");}catch(const std::invalid_argument&){}
    texture.shade_colours.pop_back();
    try{dfre::draw_textured_span32({},4,1,colours,depths,texture);check(false,"short shade table accepted");}catch(const std::invalid_argument&){}
    dfre::ModelNormal normal{{16384,0,0},0};dfre::ModelMaterial material;
    dfre::ModelLighting lighting{{4194304,0,0},{4194304,0,0},65536,65536,65536};
    check(dfre::shade_model_normal(normal,material,lighting)==0x3c0000,"normal shade upper clamp changed");
    lighting.ambient=0;
    check(dfre::shade_model_normal(normal,material,lighting,true)==0x80000,"back-facing light attenuation changed");
    material.flags=0x20;material.raw[62]=1;
    check(dfre::shade_model_normal(normal,material,lighting)==0x200000,"constant material shade changed");
    material.raw[62]=0;material.raw[58]=1;
    check(dfre::shade_model_normal(normal,material,lighting)==0x200000,"specular factor changed");
    std::vector<std::array<std::int32_t,3>> camera{{-65536,0,0},{65536,0,0},{65536,65536,0}};dfre::ModelFace face{};face.vertices={0,1,2};face.u={0,65536,65536};face.flags=7;face.material=12;
    auto clipped=dfre::clip_model_geometry(camera,{face},{{{4194304,0,0},0}});
    check(clipped.positions.size()==5 && clipped.faces.size()==2 && clipped.positions[3]==std::array<std::int32_t,3>{0,0,0} && clipped.positions[4]==std::array<std::int32_t,3>{0,32768,0},"clip intersections changed");
    check(clipped.faces[0].face.vertices==std::array<std::uint32_t,3>{4,1,2} && clipped.faces[1].face.vertices==std::array<std::uint32_t,3>{3,1,4} && clipped.faces[1].face.u[0]==32768,"clip triangle split order or UV changed");
    check(clipped.faces[1].face.flags==7 && clipped.faces[1].face.material==12 && camera[0][0]==-65536,"clip source fields changed");
    clipped=dfre::clip_model_geometry(camera,{face},{{{4194304,0,0},131072}});
    check(clipped.faces[0].rejected && clipped.positions.size()==3,"outside face not marked rejected");
    face.vertices[2]=3;
    try{dfre::clip_model_geometry(camera,{face},{});check(false,"out-of-range clip vertex accepted");}catch(const std::invalid_argument&){}
    triangle={dfre::RasterVertex{0,0,0,0,0,0},dfre::RasterVertex{0,0,0x10000,0,0,0},dfre::RasterVertex{0,0,0,0,0,0}};colours.assign(4,0);depths.assign(4,1);
    check(dfre::draw_tiny_triangle32(triangle,{0,0,3,0},0x12345678,0,4,1,colours,depths)==1 && colours[0]==0x12345678 && depths[0]==0,"one-pixel flat colour path changed");
    triangle[1].x=65536;depths.assign(4,1);colours.assign(4,0);
    check(dfre::draw_tiny_triangle32(triangle,{0,0,3,0},7,0,4,1,colours,depths)==1 && colours[1]==0,"two-pixel strict depth test changed");
    dfre::ModelMatrix camera_identity{};camera_identity[0]=camera_identity[5]=camera_identity[10]=4194304;camera_identity[3]=123456;
    const auto local_light=dfre::localize_model_lighting(camera_identity,lighting,32768);
    check(local_light.light_direction==lighting.light_direction && local_light.ambient==lighting.ambient/2 && local_light.diffuse==lighting.diffuse/2,"local light conversion changed");
    try{dfre::localize_model_lighting({},lighting);check(false,"singular light matrix accepted");}catch(const std::invalid_argument&){}
    const auto model_camera=dfre::make_model_camera({0,0,0},{0,0,0},{0,0,639,479},90);
    check(model_camera.projection.focal_length==320 && model_camera.projection.centre==std::array<std::int32_t,2>{320,240} && model_camera.projection.near_depth==4096,"camera projection changed");
    check(model_camera.planes.size()==5 && model_camera.planes[0].offset==4096 && model_camera.planes[3].normal[2]>0 && model_camera.planes[4].normal[2]<0,"camera plane order changed");
    try{dfre::make_model_camera({},{},{0,0,639,479},0);check(false,"zero camera field of view accepted");}catch(const std::invalid_argument&){}
    dfre::ModelFace assembled_face{};assembled_face.vertices={0,1,2};assembled_face.normals={0,1,2};
    std::vector<dfre::ModelProjectedVertex> projected(3);
    for(auto& point:projected){point.camera[0]=65536;point.projected=true;}
    projected[1].screen={0,2*65536};projected[2].screen={2*65536,0};
    std::vector<dfre::ModelNormal> face_normals{{{16384,0,0},0},{{-16384,0,0},0},{{0,0,0},0}};
    dfre::ModelLighting face_light{{4194304,0,0},{0,0,0},0,65536,0};
    std::vector<dfre::ModelMaterial> face_materials(3);face_materials[0].flags=0x41;
    auto prepared=dfre::prepare_model_face(assembled_face,projected,face_normals,0,face_materials,face_light);
    check(prepared && prepared->flags==0x14 && prepared->vertices[0].depth==0x3ffff0,"original material depth bias changed");
    face_materials[0].flags=0x801;
    check(!dfre::prepare_model_face(assembled_face,projected,face_normals,0,face_materials,face_light),"hidden material produced a triangle");
    face_materials[0].flags=1;assembled_face.flags=1;
    prepared=dfre::prepare_model_face(assembled_face,projected,face_normals,0,face_materials,face_light);
    check(prepared && prepared->flags==0x18 && prepared->vertices[0].shade!=prepared->vertices[1].shade,"smooth face lost its separate normal shades");
    std::swap(projected[1],projected[2]);
    check(!dfre::prepare_model_face(assembled_face,projected,face_normals,0,face_materials,face_light),"one-sided back face reached the raster path");
    face_materials[0].flags=0x4003;face_materials[0].animation_parameter=127;face_materials[1].flags=1;
    prepared=dfre::prepare_model_face(assembled_face,projected,face_normals,0,face_materials,face_light);
    check(prepared && prepared->material==1 && prepared->vertices[1].x==projected[2].screen[0],"reverse material or vertex order changed");
    face_materials[0].animation_parameter=0;std::array<std::uint32_t,11> counters{};counters[0]=3;
    try{dfre::prepare_model_face(assembled_face,projected,face_normals,0,face_materials,face_light,counters);check(false,"animated material outside table accepted");}catch(const std::invalid_argument&){}
    face_materials[0].flags=3;assembled_face.normals[0]=3;
    try{dfre::prepare_model_face(assembled_face,projected,face_normals,0,face_materials,face_light);check(false,"normal outside table accepted");}catch(const std::invalid_argument&){}
    std::cout<<"raster classification tests passed\n";
}
