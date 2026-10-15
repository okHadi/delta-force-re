#include "dfre/effect_deferred.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <utility>
namespace {void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}}
namespace {
std::vector<std::uint8_t> material_bytes() {
    std::vector<std::uint8_t> material(0x60, 0);
    material[0x1e] = 0; // u bit count 0
    material[0x20] = 0; // v bit count 0
    material[0x14] = 0x33; // context colour
    material[0x44] = 0x00; material[0x45] = 0x00; material[0x46] = 0x00; material[0x47] = 0x03; // texture 0x3000000
    return material;
}
dfre::DeferredFrame make_frame(std::int32_t left,std::int32_t top,std::int32_t right,std::int32_t bottom) {
    dfre::DeferredFrame frame;
    frame.clip_left=left;frame.clip_top=top;frame.clip_right=right;frame.clip_bottom=bottom;
    frame.depth_mode=0x20;
    for(std::int32_t row=0;row<=bottom;++row)frame.colour_row.push_back(std::uint32_t(row)*64);
    frame.target_address=0x18001000;
    frame.table_a_address=0x3010000;
    frame.table_b_address=0x3024000;
    return frame;
}
std::array<dfre::DeferredVertex,3> triangle(std::int32_t x,std::int32_t y) {
    std::array<dfre::DeferredVertex,3> vertices{};
    vertices[0].field={x<<16,y<<16,1<<20,0,0,1<<16};
    vertices[1].field={(x+12)<<16,(y+6)<<16,1<<20,0,0,1<<16};
    vertices[2].field={(x+20)<<16,(y+1)<<16,1<<20,0,0,1<<16};
    return vertices;
}
std::uint32_t table_entry(std::size_t index) {
    return std::uint32_t(index)*0x01010101u^0xffa5c3e7u;
}
}
int main() {
    {
        dfre::DeferredPass pass;
        const auto& state=pass.state();
        check(state[dfre::DeferredPass::node_sentinel_address-0xad3ef0+0x1b]==0x7f, "sentinel x changed");
        check(state[0xbc5a40-0xad3ef0]==0x00 && state[0xbc5a41-0xad3ef0]==0x00,
            "reciprocal table entry 0 changed");
        check(state[0xbc5a44-0xad3ef0]==0x00 && state[0xbc5a46-0xad3ef0]==0x01,
            "reciprocal table entry 1 changed");
    }
    {
        dfre::DeferredPass pass;
        try{pass.reset(make_frame(2,0,1,7));check(false,"reversed clip accepted");}catch(const std::invalid_argument&){}
        auto frame=make_frame(0,0,63,7);
        frame.colour_row.resize(3);
        try{pass.reset(frame);check(false,"short row table accepted");}catch(const std::invalid_argument&){}
    }
    {
        dfre::DeferredPass pass;
        auto material=material_bytes();
        pass.add_material(0x18004000,material.data(),material.size(),0x3010000,0x3024000);
        pass.reset(make_frame(0,0,63,7));
        auto flat=triangle(4,1);
        for(auto& vertex:flat)vertex.field[1]=1<<16;      // colinear top edge
        check(pass.add_face(0x414,flat,0x18004000)==0,"colinear face kept an edge");
        check(pass.record_count()==1 && pass.node_count()==0,"colinear face changed the node pool");
    }
    {
        dfre::DeferredPass pass;
        auto material=material_bytes();
        pass.add_material(0x18004000,material.data(),material.size(),0x3010000,0x3024000);
        pass.reset(make_frame(0,0,63,7));
        const auto vertices=triangle(4,1);
        check(pass.add_face(0x414,vertices,0x18004000)==3,"triangle edge count changed");
        check(pass.record_count()==1 && pass.node_count()==3,"triangle pool counts changed");
        pass.flush();
        check(pass.segment_count()==20,"segment pool count changed");
        const auto& plan=pass.draw_spans();
        check(plan.size()==6,"span count changed");
        const std::array<std::array<std::int32_t,4>,6> expected{{
            {399,2,1048576,65536},{333,5,1048576,65536},{267,9,1048576,65536},
            {201,13,1048576,65536},{135,16,1048576,65536},{69,9,1048576,65536}}};
        for(std::size_t index=0;index<plan.size();++index) {
            check(plan[index].span.pixel==expected[index][0],"span pixel changed");
            check(plan[index].span.length==expected[index][1],"span length changed");
            check(plan[index].span.depth==expected[index][2],"span depth changed");
            check(plan[index].span.shade==expected[index][3],"span shade changed");
            check(plan[index].row==std::uint32_t(6-index),"span row changed");
            check(plan[index].face_flags==0x414,"span face flags changed");
        }
        check(pass.slot_visits()==1 && pass.record_visits()==1,"draw visit counters changed");
        check(pass.span_records()==6 && pass.span_pixels()==54,"draw span counters changed");
    }
    {
        dfre::DeferredPass pass;
        auto material=material_bytes();
        pass.add_material(0x18004000,material.data(),material.size(),0x3010000,0x3024000);
        pass.reset(make_frame(0,0,63,7));
        for(std::size_t index=0;index<dfre::DeferredPass::record_capacity;++index)
            pass.add_face(0x414,triangle(4,1),0x18004000);
        check(pass.record_count()==dfre::DeferredPass::record_capacity,"record pool did not fill");
        check(pass.node_count()==dfre::DeferredPass::node_capacity,"node pool did not fill");
        check(pass.add_face(0x414,triangle(4,1),0x18004000)==0,"full record pool accepted a face");
    }
    {
        dfre::DeferredPass pass;
        auto material=material_bytes();
        pass.add_material(0x18004000,material.data(),material.size(),0x3010000,0x3024000);
        pass.reset(make_frame(0,0,63,7));
        auto zero=triangle(4,1);
        for(auto& vertex:zero)vertex.field[2]=0;
        try{pass.add_face(0x414,zero,0x18004000);check(false,"zero depth accepted");}
        catch(const std::domain_error&){}
        auto shallow=triangle(4,1);
        for(auto& vertex:shallow)vertex.field[2]=0x1000;
        try{pass.add_face(0x414,shallow,0x18004000);check(false,"overflowing depth accepted");}
        catch(const std::overflow_error&){}
    }
    {
        dfre::DeferredPass pass;
        auto material=material_bytes();
        pass.add_material(0x18004000,material.data(),material.size(),0x3010000,0x3024000);
        pass.reset(make_frame(0,0,63,7));
        pass.add_face(0x414,triangle(4,1),0x18004000);
        pass.flush();
        const auto& plan=pass.draw_spans();
        check(!plan.empty(),"no spans for the draw test");
        std::vector<std::uint32_t> colour(64*8,0x80808080u);
        std::vector<std::uint16_t> depth(64*8,0xffffu);
        std::vector<std::uint8_t> image(0x19000,0);
        image[0]=4; // texel 4 is non-zero
        // The kernel reads the blend table at the raw table address plus
        // 0x8000, so the image keeps the tested entries there.
        for(std::size_t index=0;index<256;++index) {
            const auto value=table_entry(index);
            for(std::size_t byte=0;byte<4;++byte)
                image[0x18000+index*4+byte]=std::uint8_t(value>>(byte*8));
        }
        const auto resolver=[&](const dfre::DeferredSpan& span) {
            auto context=span.context;
            context.texture=image.data()+(std::uintptr_t(context.texture)-0x3000000);
            context.blend_table=reinterpret_cast<const std::uint32_t*>(
                image.data()+(std::uintptr_t(context.blend_table)+0x8000-0x3000000));
            context.colour_plane=colour.data();
            context.depth_plane=depth.data();
            return context;
        };
        const auto writes=pass.draw(64,8,resolver);
        check(writes>0,"deferred draw wrote nothing");
        const auto blended=((0x80808080u&0x00fefefeu)>>1)+((table_entry(4)&0xfefefefeu)>>1);
        const auto pixel=std::size_t(plan[0].span.pixel);
        check(pixel+std::size_t(plan[0].span.length)<=colour.size(),"first span outside the test plane");
        check(colour[pixel]==blended,"deferred blend value changed");
        check(colour[pixel+std::size_t(plan[0].span.length)-1]==blended,"deferred blend tail changed");
    }
    std::cout<<"native effect deferred tests passed\n";
    return 0;
}
