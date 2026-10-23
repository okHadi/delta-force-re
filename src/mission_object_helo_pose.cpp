#include "dfre/mission_object_helo_pose.hpp"
namespace dfre {namespace {
using U=std::uint32_t;
struct Body {
    const MissionObjectHeloBindings& b;
    U read(U at)const{if(!b.read)throw MissionTickError("Helicopter pose read owner is not bound");return b.read(at,4);}
    void write(U at,U n)const{if(!b.write)throw MissionTickError("Helicopter pose write owner is not bound");b.write(at,4,n);}
    void call(U site,std::vector<U> args)const{if(!b.call)throw MissionTickError("Helicopter pose matrix service is not bound "+std::to_string(site));b.call(site,args);}
};
void valid(U frame,std::size_t bytes,U required){if(!frame||(frame&3)||bytes<required||std::uint64_t(frame)+required>0x100000000ull)throw MissionTickError("Helicopter pose needs a current aligned source frame");}
}
void mission_object_helo_smoke_pose(const MissionObjectHeloBindings& b,U actor,U xyz,U frame,std::size_t bytes){
    valid(frame,bytes,0x40);Body h{b};const auto graphic=h.read(actor+0x10),lod=h.read(graphic+0x50);
    h.call(0x45d1e0,{frame,actor+0x24});
    auto points=h.read(lod+0xa4);h.write(xyz,h.read(points+0x18));
    points=h.read(lod+0xa4);h.write(xyz+4,h.read(points+0x1c));
    points=h.read(lod+0xa4);h.write(xyz+8,h.read(points+0x20));
    h.call(0x45cda0,{frame,xyz,xyz});
}
void mission_object_helo_weapon_pose(const MissionObjectHeloBindings& b,U actor,U pose,U frame,std::size_t bytes){
    valid(frame,bytes,0x80);Body h{b};const auto graphic=h.read(actor+0x10),lod=h.read(graphic+0x50);
    h.call(0x45d1e0,{frame+0x40,actor+0x24});
    const auto yaw=h.read(actor+0xc4);h.write(pose+0xc,yaw);
    const auto pitch=h.read(actor+0xc8);h.write(pose+0x14,0);h.write(pose+0x10,pitch);
    auto points=h.read(lod+0xa4);auto value=h.read(points+0x30);h.write(pose,value);h.write(pose,0u-value);
    points=h.read(lod+0xa4);value=h.read(points+0x34);h.write(pose+4,value);h.write(pose+4,0u-value);
    points=h.read(lod+0xa4);value=h.read(points+0x38);h.write(pose+8,value);h.write(pose+8,0u-value);
    h.call(0x45d400,{frame,pose});h.call(0x45cb40,{frame+0x40,frame,frame});
    points=h.read(lod+0xa4);h.write(pose,h.read(points+0x60));
    points=h.read(lod+0xa4);h.write(pose+4,h.read(points+0x64));
    points=h.read(lod+0xa4);h.write(pose+8,h.read(points+0x68));
    h.call(0x45cda0,{frame,pose,pose});
}
} // namespace dfre
