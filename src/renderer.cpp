#include "dfre/renderer.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace dfre {
namespace {
std::int32_t signed_value(std::uint32_t value) {
    return value<=2147483647?std::int32_t(value):std::int32_t(std::int64_t(value)-4294967296LL);
}
std::int32_t pixel(std::int32_t value) {
    return value>=0?value/65536:std::int32_t(-((-std::int64_t(value)+65535)/65536));
}
std::int32_t add(std::int32_t a,std::int32_t b){return signed_value(std::uint32_t(a)+std::uint32_t(b));}
std::int32_t sub(std::int32_t a,std::int32_t b){return signed_value(std::uint32_t(a)-std::uint32_t(b));}
std::int32_t shift(std::int32_t value,unsigned bits) {
    const auto divisor=std::int64_t(1)<<bits;
    return std::int32_t(value>=0?value/divisor:-((-std::int64_t(value)+divisor-1)/divisor));
}
std::int32_t product(std::int32_t a,std::int32_t b,unsigned bits=16) {
    const auto value=std::int64_t(a)*b,divisor=std::int64_t(1)<<bits;
    return signed_value(std::uint32_t(value>=0?value/divisor:-((-value+divisor-1)/divisor)));
}
std::int32_t negate(std::int32_t value){return signed_value(0-std::uint32_t(value));}
std::int32_t divide(std::int64_t a,std::int32_t b) {
    if(!b)throw std::invalid_argument("raster division by zero");
    const auto value=a/b;
    if(value<(-2147483647LL-1) || value>2147483647)throw std::invalid_argument("raster division overflow");
    return std::int32_t(value);
}

}
RasterClassification classify_raster_triangle(const std::array<RasterVertex,3>& triangle,const RasterClip& clip) {
    if(clip.left>clip.right || clip.top>clip.bottom)throw std::invalid_argument("raster clip bounds are reversed");
    RasterClassification result;
    auto order=[&](bool y) {
        auto value=[&](std::uint32_t i){return y?triangle[i].y:triangle[i].x;};
        std::array<std::uint32_t,3> indexes{std::uint32_t(value(1)<value(0)),std::uint32_t(value(1)>=value(0)),2};
        if(value(2)<value(indexes[0])){indexes[2]=indexes[0];indexes[0]=2;}
        if(value(indexes[2])<value(indexes[1]))std::swap(indexes[1],indexes[2]);
        return indexes;
    };
    result.x_order=order(false);result.y_order=order(true);
    const auto minx=pixel(triangle[result.x_order[0]].x),maxx=pixel(triangle[result.x_order[2]].x);
    const auto miny=pixel(triangle[result.y_order[0]].y),maxy=pixel(triangle[result.y_order[2]].y);
    if(maxx<clip.left || clip.right<minx || maxy<clip.top || clip.bottom<miny)return result;
    if(minx<clip.left)result.clip_flags|=1;
    if(clip.right<maxx)result.clip_flags|=2;
    if(miny<clip.top)result.clip_flags|=4;
    if(clip.bottom<maxy)result.clip_flags|=8;
    const auto width=signed_value(std::uint32_t(maxx)-std::uint32_t(minx)+1);
    const auto height=signed_value(std::uint32_t(maxy)-std::uint32_t(miny)+1);
    result.extent=(width==0 && height==0)?1:std::max(width,height);
    return result;
}
RasterGradients raster_triangle_gradients(const std::array<RasterVertex,3>& triangle,std::uint32_t flags) {
    const auto dx1=shift(sub(triangle[1].x,triangle[0].x),8),dy1=shift(sub(triangle[1].y,triangle[0].y),8);
    const auto dx2=shift(sub(triangle[2].x,triangle[0].x),8),dy2=shift(sub(triangle[2].y,triangle[0].y),8);
    const auto denominator=sub(product(dx1,dy2),product(dy1,dx2));
    const auto absolute=denominator<0?negate(denominator):denominator;
    const auto reciprocal=absolute<3?(denominator<0?0x40000000:-0x40000000):divide(0x100000000LL,denominator);
    auto gradient=[&](std::int32_t a,std::int32_t b,std::int32_t c) {
        const auto da1=shift(sub(b,a),8),da2=shift(sub(c,a),8);
        return std::array<std::int32_t,2>{negate(product(sub(product(dy1,da2),product(da1,dy2)),reciprocal)),
            negate(product(sub(product(da1,dx2),product(dx1,da2)),reciprocal))};
    };
    RasterGradients result;
    if(flags&1){result.u=gradient(triangle[0].u,triangle[1].u,triangle[2].u);result.v=gradient(triangle[0].v,triangle[1].v,triangle[2].v);}
    if(flags&2)result.depth=gradient(triangle[0].depth,triangle[1].depth,triangle[2].depth);
    if(flags&4)result.shade=gradient(triangle[0].shade,triangle[1].shade,triangle[2].shade);
    return result;
}
std::vector<RasterSpan> raster_triangle_spans(const std::array<RasterVertex,3>& triangle,
    const RasterClip& clip,std::uint32_t gradient_flags) {
    const auto classification=classify_raster_triangle(triangle,clip);
    if(clip.left<0 || clip.top<0 || clip.right>32767 || clip.bottom>32767)throw std::invalid_argument("raster span clip exceeds screen range");
    std::vector<RasterSpan> spans;if(!classification.extent)return spans;
    const auto gradients=raster_triangle_gradients(triangle,gradient_flags);
    struct Edge {std::array<std::int32_t,5> value{},step{};};
    auto edge=[&](Edge& output,const RasterVertex& start,const RasterVertex& end) {
        const auto dy=shift(sub(end.y,start.y),6);if(!dy)return false;
        const auto dx=shift(sub(end.x,start.x),6);
        const auto slope=divide(signed_value(std::uint32_t(dx)<<10),dy);
        const auto absolute=slope<0?negate(slope):slope;if(absolute>=0x200001)return false;
        const auto dxstep=signed_value(std::uint32_t(slope)<<6);
        const auto sample=add(signed_value(std::uint32_t(sub(pixel(add(start.y,0x8000)),1))<<16),0x8000);
        const auto dyoffset=sub(sample,start.y),dxoffset=product(dyoffset,dxstep);
        output.value[0]=add(start.x,dxoffset);output.step[0]=dxstep;
        const std::array<std::int32_t,4> values{start.u,start.v,start.depth,start.shade};
        const std::array<std::array<std::int32_t,2>,4> changes{gradients.u,gradients.v,gradients.depth,gradients.shade};
        for(std::size_t i=0;i<4;++i){output.value[i+1]=add(add(values[i],product(changes[i][0],dxoffset)),product(changes[i][1],dyoffset));output.step[i+1]=add(product(dxstep,changes[i][0]),changes[i][1]);}
        return true;
    };
    const auto minimum=classification.y_order[0],middle=classification.y_order[1],maximum=classification.y_order[2];
    Edge long_edge,short_edge;if(!edge(long_edge,triangle[minimum],triangle[maximum]))return spans;
    const bool long_left=(minimum+1)%3==maximum;
    Edge* left=long_left?&long_edge:&short_edge;Edge* right=long_left?&short_edge:&long_edge;
    auto y=pixel(add(triangle[minimum].y,0x8000));auto sample=add(signed_value(std::uint32_t(y)<<16),0x8000);
    auto emit=[&] {
        if(y>clip.bottom)return;
        for(std::size_t i=0;i<5;++i)left->value[i]=add(left->value[i],left->step[i]);
        right->value[0]=add(right->value[0],right->step[0]);
        auto x=pixel(add(left->value[0],0x8000)),last=pixel(sub(right->value[0],0x8000));
        if(left->value[0]>=right->value[0] || y<clip.top || last<clip.left || x>clip.right)return;
        auto values=left->value;const auto skipped=std::max(0,clip.left-x);x=std::max(x,clip.left);last=std::min(last,clip.right);
        const std::array<std::int32_t,4> changes{gradients.u[0],gradients.v[0],gradients.depth[0],gradients.shade[0]};
        for(std::size_t i=0;i<4;++i)values[i+1]=add(values[i+1],signed_value(std::uint32_t(skipped)*std::uint32_t(changes[i])));
        const auto length=last-x+1;if(length>0)spans.push_back({x,y,length,values[1],values[2],values[3],values[4],changes[0],changes[1],changes[2],changes[3]});
    };
    auto half=[&](const RasterVertex& start,const RasterVertex& end) {
        if(!edge(short_edge,start,end))return;
        std::uint32_t rows=0;while(sample<end.y){if(++rows>65536)throw std::invalid_argument("raster span row limit exceeded");emit();sample=add(sample,0x10000);++y;}
    };
    half(triangle[minimum],triangle[middle]);half(triangle[middle],triangle[maximum]);return spans;
}

std::size_t draw_textured_span32(const RasterSpan& span,std::uint32_t width,std::uint32_t height,
    std::vector<std::uint32_t>& colour,std::vector<std::uint16_t>& depth,const RasterTexture32& texture,RasterTextureMode mode,std::optional<std::uint32_t> uniform_shade) {
    if(mode!=RasterTextureMode::uniform && mode!=RasterTextureMode::varying_shade && mode!=RasterTextureMode::cutout && mode!=RasterTextureMode::varying_cutout)throw std::invalid_argument("unsupported textured raster mode");
    const auto size=std::uint64_t(width)*height;
    if(size!=colour.size() || size!=depth.size())throw std::invalid_argument("raster target size differs from bounds");
    if(texture.width_bits>16 || texture.height_bits>16)throw std::invalid_argument("texture coordinate bits exceed MMX range");
    const auto tw=std::uint64_t(1)<<texture.width_bits,th=std::uint64_t(1)<<texture.height_bits;
    if(tw*th!=texture.pixels.size() || texture.shade_colours.size()!=64*256)throw std::invalid_argument("texture or shade table size differs from bounds");
    if(span.x<0 || span.y<0 || std::uint32_t(span.y)>=height || span.length<0 || std::uint32_t(span.x)>width || std::uint32_t(span.length)>width-std::uint32_t(span.x))throw std::invalid_argument("raster span exceeds target");
    const auto uniform_row=(uniform_shade.value_or(texture.uniform_shade)>>16)&63;std::size_t writes=0;
    const auto offset=std::size_t(span.y)*width+std::uint32_t(span.x);
    for(std::int32_t i=0;i<span.length;++i) {
        const auto u=std::uint32_t(span.u)+std::uint32_t(i)*std::uint32_t(span.du);
        const auto v=std::uint32_t(span.v)+std::uint32_t(i)*std::uint32_t(span.dv);
        const auto z=std::uint32_t(span.depth)+std::uint32_t(i&~1)*std::uint32_t(span.ddepth);
        const auto value=std::uint16_t(z>>16);
        if(value<depth[offset+i]) {
            const auto index=((u>>16)&(tw-1))+(((v>>16)&(th-1))<<texture.width_bits);
            const auto texel=texture.pixels[std::size_t(index)];
            if((mode==RasterTextureMode::cutout || mode==RasterTextureMode::varying_cutout) && texel==0)continue;
            const auto shade=std::uint32_t(span.shade)+std::uint32_t(i&~1)*std::uint32_t(span.dshade);
            const auto row=(mode==RasterTextureMode::varying_shade || mode==RasterTextureMode::varying_cutout)?((shade>>16)&63):uniform_row;
            depth[offset+i]=value;colour[offset+i]=texture.shade_colours[row*256+texel];++writes;
        }
    }
    return writes;
}

std::size_t draw_flat_span32(const RasterSpan& span,std::uint32_t width,std::uint32_t height,
    std::vector<std::uint32_t>& colour,std::vector<std::uint16_t>& depth,
    std::uint8_t palette_index,const std::vector<std::uint32_t>& shade_colours) {
    const auto size=std::uint64_t(width)*height;
    if(size!=colour.size() || size!=depth.size() || shade_colours.size()!=64*256)throw std::invalid_argument("flat raster target or shade table size differs from bounds");
    if(span.x<0 || span.y<0 || std::uint32_t(span.y)>=height || span.length<0 || std::uint32_t(span.x)>width || std::uint32_t(span.length)>width-std::uint32_t(span.x))throw std::invalid_argument("flat raster span exceeds target");
    const auto offset=std::size_t(span.y)*width+std::uint32_t(span.x);std::size_t writes=0;
    for(std::int32_t i=0;i<span.length;++i) {
        const auto value=std::uint16_t((std::uint32_t(span.depth)+std::uint32_t(i)*std::uint32_t(span.ddepth))>>16);
        if(value<depth[offset+i]) {
            const auto row=((std::uint32_t(span.shade)+std::uint32_t(i)*std::uint32_t(span.dshade))>>16)&63;
            colour[offset+i]=shade_colours[row*256+palette_index];depth[offset+i]=value;++writes;
        }
    }
    return writes;
}

std::size_t draw_model_triangle32(const ModelRasterTriangle& face,const RasterClip& clip,
    const RasterMaterial32& material,std::uint32_t width,std::uint32_t height,
    std::vector<std::uint32_t>& colour,std::vector<std::uint16_t>& depth) {
    const auto classification=classify_raster_triangle(face.vertices,clip);
    if(classification.extent<=0)return 0;
    if(classification.extent<=2)return draw_tiny_triangle32(face.vertices,clip,material.tiny_colour,face.flags,width,height,colour,depth);
    if(face.flags&0x700)throw std::invalid_argument("model face requires an effect pass");
    const auto mode=face.flags&0x3c;
    const bool textured=mode==0x14 || mode==0x18 || mode==0x34 || mode==0x38;
    if(!textured && mode!=4 && mode!=8)throw std::invalid_argument("unsupported model face pixel mode");
    auto triangle=face.vertices;
    if(textured) {
        if(!material.texture || material.texture->width_bits>16 || material.texture->height_bits>16)throw std::invalid_argument("model face texture is missing or invalid");
        for(auto& vertex:triangle) {
            vertex.u=signed_value(std::uint32_t(vertex.u)<<material.texture->width_bits);
            vertex.v=signed_value(std::uint32_t(vertex.v)<<material.texture->height_bits);
        }
    } else if(!material.shade_colours)throw std::invalid_argument("model face shade table is missing");
    const auto spans=raster_triangle_spans(triangle,clip,2u|(textured?1u:0u)|((mode&8)?4u:0u));
    std::size_t writes=0;
    for(const auto& span:spans) {
        if(textured)writes+=draw_textured_span32(span,width,height,colour,depth,*material.texture,static_cast<RasterTextureMode>(mode),std::uint32_t(triangle[0].shade));
        else writes+=draw_flat_span32(span,width,height,colour,depth,material.flat_index,*material.shade_colours);
    }
    return writes;
}

std::optional<ModelRasterTriangle> prepare_model_face(const ModelFace& face,
    const std::vector<ModelProjectedVertex>& positions,const std::vector<ModelNormal>& normals,
    std::size_t normal_start,const std::vector<ModelMaterial>& materials,const ModelLighting& lighting,
    const std::array<std::uint32_t,11>& animation_counters) {
    auto index=std::size_t(face.material);
    if(index>=materials.size())throw std::invalid_argument("face material outside table");
    const auto& initial=materials[index];
    if((initial.flags&0x4000) && initial.animation_parameter<11) {
        const auto delta=animation_counters[initial.animation_parameter];
        if(delta>=materials.size()-index)throw std::invalid_argument("animated face material outside table");
        index+=delta;
    }
    if(materials[index].flags&0x800)return {};
    ModelRasterTriangle result;
    for(std::size_t k=0;k<3;++k) {
        if(face.vertices[k]>=positions.size())throw std::invalid_argument("face vertex outside table");
        const auto& point=positions[face.vertices[k]];
        if(!point.projected)throw std::invalid_argument("face vertex is not projected");
        result.vertices[k]={point.screen[0],point.screen[1],signed_value(std::uint32_t(point.camera[0])<<6),face.u[k],face.v[k],0};
    }
    const auto& t=result.vertices;
    const auto determinant=sub(product(shift(sub(t[1].x,t[0].x),4),shift(sub(t[2].y,t[0].y),4)),
        product(shift(sub(t[1].y,t[0].y),4),shift(sub(t[2].x,t[0].x),4)));
    const bool reverse=determinant>0;
    if(reverse) {
        const auto& material=materials[index];
        if(!(material.flags&2))return {};
        if((material.flags&0x4000) && material.animation_parameter==127) {
            if(++index>=materials.size())throw std::invalid_argument("reverse face material outside table");
        }
    }
    const auto& material=materials[index];result.material=index;
    if(material.flags&0x40) {
        const auto depth=sub(std::min({t[0].depth,t[1].depth,t[2].depth}),16);
        for(auto& vertex:result.vertices)vertex.depth=depth;
    }
    if(material.flags&2)result.flags|=0x40;
    if(material.flags&1)result.flags|=0x10;
    if((material.flags&0x200) && !(material.flags&0x8000))result.flags|=0x20;
    if(material.flags&8)result.flags|=0x100;
    if(material.flags&0x8000)result.flags|=0x200;
    if(material.flags&0x80)result.flags|=0x400;
    auto shade=[&](std::size_t normal) {
        if(normal>=normals.size())throw std::invalid_argument("face normal outside table");
        return shade_model_normal(normals[normal],material,lighting,reverse);
    };
    if(face.flags&1) {
        for(std::size_t k=0;k<3;++k)result.vertices[k].shade=shade(face.normals[k]);
        const auto a=std::int64_t(t[0].shade)-t[1].shade,b=std::int64_t(t[1].shade)-t[2].shade;
        if(std::abs(a)<65536 && std::abs(b)<65536) {
            result.vertices[1].shade=result.vertices[2].shade=t[0].shade;result.flags|=4;
        } else result.flags|=8;
    } else {
        if(normal_start>normals.size() || face.face_normal>=normals.size()-normal_start)throw std::invalid_argument("flat face normal outside table");
        const auto value=shade(normal_start+face.face_normal);
        for(auto& vertex:result.vertices)vertex.shade=value;
        result.flags|=4;
    }
    if(reverse)std::swap(result.vertices[1],result.vertices[2]);
    return result;
}

ModelLighting localize_model_lighting(const ModelMatrix& matrix,const ModelLighting& camera,std::int32_t scale) {
    const auto a=product(matrix[0],product(matrix[5],matrix[10],22),22);
    const auto b=product(matrix[1],product(matrix[6],matrix[8],22),22);
    const auto c=product(matrix[2],product(matrix[4],matrix[9],22),22);
    const auto d=product(matrix[0],product(matrix[6],matrix[9],22),22);
    const auto e=product(matrix[1],product(matrix[4],matrix[10],22),22);
    const auto f=product(matrix[2],product(matrix[5],matrix[8],22),22);
    const auto determinant=sub(sub(sub(add(add(a,b),c),d),e),f);
    const auto reciprocal=divide(0x100000000000LL,determinant);
    ModelMatrix inverse{};
    for(std::size_t row=0;row<3;++row)for(std::size_t column=0;column<3;++column)inverse[row*4+column]=product(reciprocal,matrix[column*4+row],22);
    auto direction=[&](const std::array<std::int32_t,3>& input) {
        std::array<std::int32_t,3> output{};
        for(std::size_t row=0;row<3;++row)for(std::size_t k=0;k<3;++k)output[row]=add(output[row],product(inverse[row*4+k],input[k],22));
        return output;
    };
    return {direction(camera.light_direction),direction(camera.view_direction),product(scale,camera.ambient),product(scale,camera.diffuse),product(scale,camera.specular)};
}

std::int32_t shade_model_normal(const ModelNormal& normal,const ModelMaterial& material,
    const ModelLighting& lighting,bool reverse) {
    auto field=[&](std::size_t offset) {
        std::uint32_t value=0;for(std::size_t i=0;i<4;++i)value|=std::uint32_t(material.raw[offset+i])<<(i*8);
        return signed_value(value);
    };
    auto dot=[&](const std::array<std::int32_t,3>& direction) {
        std::int32_t value=0;for(std::size_t i=0;i<3;++i)value=add(value,product(normal.direction[i],direction[i],22));
        const auto scaled=signed_value(std::uint32_t(value)<<2);return reverse?negate(scaled):scaled;
    };
    std::int32_t shade;
    if(material.flags&0x20)shade=field(60);
    else {
        auto light=dot(lighting.light_direction);if(light<0)light=shift(negate(light),2);
        shade=add(lighting.ambient,product(lighting.diffuse,light));
    }
    const auto factor=field(56);
    if(factor) {
        auto view=dot(lighting.view_direction);
        if(view>0) {
            for(std::uint32_t i=0;i<material.raw[64];++i)view=product(view,view);
            shade=add(shade,product(lighting.specular,product(view,factor)));
        }
    }
    shade=signed_value(std::uint32_t(shade)<<5);
    return std::max(0x10000,std::min(0x3c0000,shade));
}


ModelClipGeometry clip_model_geometry(const std::vector<std::array<std::int32_t,3>>& positions,
    const std::vector<ModelFace>& faces,const std::vector<ModelClipPlane>& planes) {
    if(positions.size()>32768 || faces.size()>65536 || planes.size()>5)throw std::invalid_argument("model clip input exceeds bounds");
    ModelClipGeometry output;output.positions=positions;
    for(const auto& face:faces){for(auto index:face.vertices)if(index>=positions.size())throw std::invalid_argument("clip face vertex outside table");output.faces.push_back({face,false});}
    for(const auto& plane:planes) {
        const auto count=output.faces.size();
        for(std::size_t index=0;index<count;++index) {
            if(output.faces[index].rejected)continue;
            const auto source=output.faces[index].face;std::array<std::int32_t,3> distance{};unsigned outside=0;
            for(std::size_t corner=0;corner<3;++corner) {
                const auto& point=output.positions[source.vertices[corner]];std::int32_t value=0;
                for(std::size_t k=0;k<3;++k)value=add(value,product(point[k],plane.normal[k],22));
                distance[corner]=sub(value,plane.offset);if(distance[corner]<0)outside|=1u<<corner;
            }
            if(!outside)continue;
            if(outside==7){output.faces[index].rejected=true;continue;}
            auto intersection=[&](std::size_t inside,std::size_t outside_corner) {
                if(output.positions.size()>=32768)throw std::invalid_argument("model clip output vertex limit exceeded");
                const auto a=signed_value(std::uint32_t(distance[inside])<<6),b=signed_value(std::uint32_t(distance[outside_corner])<<6);
                const auto absolute_a=a<0?negate(a):a,absolute_b=b<0?negate(b):b;
                const auto fraction=std::min(0x400000,divide(std::int64_t(absolute_a)*0x400000,add(absolute_a,absolute_b)));
                std::array<std::int32_t,3> point{};for(std::size_t k=0;k<3;++k)point[k]=add(output.positions[source.vertices[inside]][k],product(fraction,sub(output.positions[source.vertices[outside_corner]][k],output.positions[source.vertices[inside]][k]),22));
                const auto vertex=std::uint32_t(output.positions.size());output.positions.push_back(point);
                return std::array<std::int32_t,3>{std::int32_t(vertex),add(source.u[inside],product(fraction,sub(source.u[outside_corner],source.u[inside]),22)),add(source.v[inside],product(fraction,sub(source.v[outside_corner],source.v[inside]),22))};
            };
            auto replace=[&](std::size_t face,std::size_t corner,const std::array<std::int32_t,3>& vertex) {
                output.faces[face].face.vertices[corner]=std::uint32_t(vertex[0]);output.faces[face].face.u[corner]=vertex[1];output.faces[face].face.v[corner]=vertex[2];
            };
            if(outside==1 || outside==2 || outside==4) {
                if(output.faces.size()>=65536)throw std::invalid_argument("model clip output face limit exceeded");
                const auto appended=output.faces.size();output.faces.push_back({source,false});
                if(outside==1){const auto a=intersection(1,0);replace(appended,0,a);const auto b=intersection(2,0);replace(index,0,b);replace(appended,2,b);}
                else if(outside==2){const auto a=intersection(2,1);replace(appended,1,a);const auto b=intersection(0,1);replace(index,1,b);replace(appended,0,b);}
                else {const auto a=intersection(0,2);replace(appended,2,a);const auto b=intersection(1,2);replace(index,2,b);replace(appended,1,b);}
            } else if(outside==3){replace(index,0,intersection(2,0));replace(index,1,intersection(2,1));}
            else if(outside==5){replace(index,0,intersection(1,0));replace(index,2,intersection(1,2));}
            else if(outside==6){replace(index,1,intersection(0,1));replace(index,2,intersection(0,2));}
        }
    }
    return output;
}
std::size_t draw_tiny_triangle32(const std::array<RasterVertex,3>& triangle,const RasterClip& clip,
    std::uint32_t flat_colour,std::uint32_t flags,std::uint32_t width,std::uint32_t height,
    std::vector<std::uint32_t>& colour,std::vector<std::uint16_t>& depth) {
    if(std::uint64_t(width)*height!=colour.size() || colour.size()!=depth.size())throw std::invalid_argument("tiny raster target size differs from bounds");
    if(clip.left<0 || clip.top<0 || clip.right<0 || clip.bottom<0 || std::uint32_t(clip.right)>=width || std::uint32_t(clip.bottom)>=height)throw std::invalid_argument("tiny raster clip exceeds target");
    const auto classification=classify_raster_triangle(triangle,clip);
    if(!classification.extent || (flags&0x600))return 0;
    if(classification.extent>2)throw std::invalid_argument("tiny triangle extent exceeds two pixels");
    if(classification.extent==1 && classification.clip_flags)return 0;
    const auto count=classification.extent==1?1u:3u;std::size_t writes=0;
    for(std::size_t i=0;i<count;++i) {
        const auto x=pixel(triangle[i].x),y=pixel(triangle[i].y);
        if(x<clip.left || x>clip.right || y<clip.top || y>clip.bottom)continue;
        const auto offset=std::size_t(y)*width+std::uint32_t(x);const auto value=std::uint16_t(std::uint32_t(triangle[i].depth)>>16);
        if(value<depth[offset]){colour[offset]=flat_colour;depth[offset]=value;++writes;}
    }
    return writes;
}


ModelCameraFrame make_model_camera(const std::array<std::int32_t,3>& position,
    const std::array<std::uint32_t,3>& angles,const RasterClip& clip,std::uint32_t horizontal_fov) {
    if(horizontal_fov<1 || horizontal_fov>179 || clip.left<0 || clip.top<0 || clip.right<clip.left || clip.bottom<clip.top || clip.right>32767 || clip.bottom>32767)throw std::invalid_argument("model camera bounds or field of view are invalid");
    const auto width=clip.right-clip.left+1,height=clip.bottom-clip.top+1;
    const long double half_angle=static_cast<long double>((horizontal_fov+1)/2)*static_cast<long double>(0.01745329);
    const auto focal=std::int32_t((static_cast<long double>(width)*0.5L)/std::tan(half_angle)+0.5L);
    const auto horizontal_sine=std::int32_t(std::sin(half_angle)*4194304.0L),horizontal_cosine=std::int32_t(std::cos(half_angle)*4194304.0L);
    const auto vertical_angle=std::atan2(static_cast<long double>((height+1)/2),static_cast<long double>(focal));
    const auto vertical_sine=std::int32_t(std::sin(vertical_angle)*4194304.0L),vertical_cosine=std::int32_t(std::cos(vertical_angle)*4194304.0L);
    ModelCameraFrame result;result.clip=clip;result.projection={focal,{clip.left+width/2,clip.top+height/2},4096};
    result.planes={{{4194304,0,0},4096},{{horizontal_sine,-horizontal_cosine,0},0},{{horizontal_sine,horizontal_cosine,0},0},{{vertical_sine,0,vertical_cosine},0},{{vertical_sine,0,-vertical_cosine},0}};
    auto translation=model_identity_matrix();for(std::size_t k=0;k<3;++k)translation[k*4+3]=negate(position[k]);result.matrix=translation;
    for(std::size_t i=0;i<3;++i) {
        const auto axis=2-i;const auto sc=fixed_sine_cosine(angles[i]);const auto cosine=sc[1],sine=negate(sc[0]);auto rotation=model_identity_matrix();
        if(axis==0){rotation[5]=cosine;rotation[6]=negate(sine);rotation[9]=sine;rotation[10]=cosine;}
        else if(axis==1){rotation[0]=cosine;rotation[2]=negate(sine);rotation[8]=sine;rotation[10]=cosine;}
        else{rotation[0]=cosine;rotation[1]=negate(sine);rotation[4]=sine;rotation[5]=cosine;}
        result.matrix=model_multiply_matrix(rotation,result.matrix);
    }
    return result;
}
std::size_t draw_model_mesh32(const ModelLod& lod,std::size_t mesh_index,
    const std::vector<ModelMatrix>& camera_matrices,const ModelCameraFrame& camera,
    const ModelLighting& camera_lighting,const std::vector<RasterMaterial32>& materials,
    std::uint32_t width,std::uint32_t height,std::vector<std::uint32_t>& colour,
    std::vector<std::uint16_t>& depth,const std::array<std::uint32_t,11>& animation_counters,ModelVertexMath math) {
    if(mesh_index>=lod.meshes.size() || mesh_index>=camera_matrices.size() || materials.size()!=lod.materials.size())throw std::invalid_argument("mesh draw table size differs from geometry");
    if(std::uint64_t(width)*height!=colour.size() || colour.size()!=depth.size() || camera.clip.left<0 || camera.clip.top<0 || camera.clip.right<camera.clip.left || camera.clip.bottom<camera.clip.top || std::uint32_t(camera.clip.right)>=width || std::uint32_t(camera.clip.bottom)>=height)throw std::invalid_argument("mesh draw target or clip is invalid");
    const auto& mesh=lod.meshes[mesh_index];
    if(mesh.face_start>lod.faces.size() || mesh.face_count>lod.faces.size()-mesh.face_start)throw std::invalid_argument("mesh face range outside table");
    auto positions=model_mesh_vertices(lod,mesh_index,camera_matrices,math);
    std::vector<ModelFace> faces;faces.reserve(mesh.face_count);
    for(std::size_t i=0;i<mesh.face_count;++i) {
        auto face=lod.faces[mesh.face_start+i];
        for(auto& index:face.vertices) {
            if(index<mesh.vertex_start || std::size_t(index-mesh.vertex_start)>=positions.size())throw std::invalid_argument("mesh face vertex outside mesh");
            index-=mesh.vertex_start;
        }
        faces.push_back(face);
    }
    const auto lighting=localize_model_lighting(camera_matrices[mesh_index],camera_lighting);
    const auto geometry=clip_model_geometry(positions,faces,camera.planes);
    std::vector<ModelProjectedVertex> projected;projected.reserve(geometry.positions.size());
    for(const auto& point:geometry.positions)projected.push_back(model_project_vertex(point,camera.projection));
    std::size_t writes=0;
    for(const auto& face:geometry.faces) {
        if(face.rejected)continue;
        const auto prepared=prepare_model_face(face.face,projected,lod.normals,mesh.normal_start,lod.materials,lighting,animation_counters);
        if(prepared)writes+=draw_model_triangle32(*prepared,camera.clip,materials[prepared->material],width,height,colour,depth);
    }
    return writes;
}

}
