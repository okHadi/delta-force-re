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
    std::cout<<"raster classification tests passed\n";
}
