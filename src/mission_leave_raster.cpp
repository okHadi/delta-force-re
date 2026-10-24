#include "dfre/mission_leave_raster.hpp"
#include <stdexcept>
namespace dfre {namespace {
using U=std::uint32_t;
template<class T>const T& need(const T& f){if(!f)throw std::runtime_error("Leave raster reached a missing provider");return f;}
U read(const MissionDebriefBindings& b,U at){return need(b.read)(at,4);}
void write(const MissionDebriefBindings& b,U at,U n){need(b.write)(at,4,n);}
U call(const MissionDebriefBindings& b,U at,const std::vector<U>& args={}){return need(b.call)(at,args);}
std::int64_t sign(U n){return n<0x80000000u?n:std::int64_t(n)-0x100000000ll;}
U divide(U n,U d){const auto a=sign(n),v=sign(d);if(!v||(a==-2147483648ll&&v==-1))throw std::runtime_error("Leave raster signed division fault");return U(a/v);}
}
U mission_frame_pointer(const MissionDebriefBindings& b){return read(b,0x50130c);}
U mission_frame_zero(){return 0;}
void mission_raster_rows(const MissionDebriefBindings& b,U out,U in){
    const auto width=read(b,in);bool same=width==read(b,out+0x1808);
    if(same){const auto height=read(b,in+4);same=height==read(b,out+0x180c);}
    if(same){const auto stride=read(b,in+8);same=stride==read(b,out+0x1810);}
    if(same){const auto pixels=read(b,in+12);same=pixels==read(b,out);}
    if(same){const auto auxiliary=read(b,in+16);same=auxiliary==read(b,out+0xc04);}
    if(!same){
        if(!read(b,in+8)){const auto current_width=read(b,in);const auto bits=read(b,in+20);write(b,in+8,current_width*bits);}
        U pixel=read(b,in+12);auto height=read(b,in+4);U index=0;
        if(sign(height)>0)do{write(b,out+4+index*4,pixel);++index;const auto stride=read(b,in+8);height=read(b,in+4);pixel+=stride;}while(sign(index)<sign(height));
        const auto stride=read(b,in+8);const auto bits=read(b,in+20);const auto quotient=divide(stride<<3,bits);
        U auxiliary=read(b,in+16);height=read(b,in+4);index=0;
        if(sign(height)>0){const U step=quotient*2;do{write(b,out+0xc08+index*4,auxiliary);++index;height=read(b,in+4);auxiliary+=step;}while(sign(index)<sign(height));}
    }
    auto n=read(b,in);write(b,out+0x1808,n);n=read(b,in+4);write(b,out+0x180c,n);n=read(b,in+8);write(b,out+0x1810,n);n=read(b,in+20);write(b,out+0x1824,n);n=read(b,in+12);write(b,out,n);n=read(b,in+16);write(b,out+0xc04,n);
}
U mission_leave_frame(const MissionDebriefBindings& b,U out,U descriptor){
    const auto mode=read(b,0x503138);if(mode!=0&&mode!=1)return 0;
    const auto pixels=mission_frame_pointer(b);(void)mission_frame_zero();
    auto n=read(b,0xd092c0);write(b,descriptor,n);n=read(b,0xd092c4);write(b,descriptor+4,n);n=read(b,descriptor);
    write(b,descriptor+8,mode?n*4:n);
    if(!mode)write(b,descriptor+16,0x503740);
    write(b,descriptor+12,pixels);
    if(mode)write(b,descriptor+16,0x503740);
    write(b,descriptor+20,mode?32:8);mission_raster_rows(b,out,descriptor);return pixels;
}
void mission_display_rows(const MissionDebriefBindings& b,U first,U last){
    if(!read(b,0xcc0884))return;if(!read(b,0xd09300))return;
    const auto height=read(b,0xd092c4);if(last>height)last=height;if(last<=first)return;
    const auto before=read(b,0xd09308);write(b,0xd0932c,first);write(b,0xd09330,last);
    if(before){const auto h=read(b,0xd092c4);const auto w=read(b,0xd092c0);call(b,read(b,0xd09308),{w,h});}
    call(b,read(b,0xd0924c));
    if(read(b,0xd0930c)){const auto h=read(b,0xd092c4);const auto w=read(b,0xd092c0);call(b,read(b,0xd0930c),{w,h});}
}
void mission_display_present(const MissionDebriefBindings& b){const auto height=read(b,0xd092c4);mission_display_rows(b,0,height);}
void mission_leave_present(const MissionDebriefBindings& b){if(read(b,0x503138)==2)return;const auto profile=read(b,0x4f8718);call(b,0x456e30,{5});mission_display_present(b);call(b,0x456e30,{profile});}
}
