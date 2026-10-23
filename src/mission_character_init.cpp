#include "dfre/mission_character_init.hpp"
#include "dfre/models.hpp"
namespace dfre {namespace {
using U=std::uint32_t;
U read(const MissionCharacterAllocateBindings& b,U at,U width=4){if(!b.read)throw MissionCharacterAllocateError("Character setup needs current memory reads");return b.read(at,width);}
void write(const MissionCharacterAllocateBindings& b,U at,U n,U width=4){if(!b.write)throw MissionCharacterAllocateError("Character setup needs current memory writes");b.write(at,width,n);}
std::int32_t signed32(U n){return n<=0x7fffffffu?std::int32_t(n):std::int32_t(std::int64_t(n)-0x100000000LL);}
void advance(const MissionCharacterAllocateBindings& b,MissionCharacterAllocateRegisters& r){
    if(!r.ebx)return;
    for(;;){r.edi+=read(b,r.edi);const auto old=r.ebx;--r.ebx;if(signed32(old)<=1)return;}
}
}
void mission_character_clear_scalars(const MissionCharacterAllocateBindings& b,MissionCharacterAllocateRegisters& r){
    for(const U offset:{0x28u,0x2cu,0x30u,0x34u,0x38u,0x44u,0x1cu,0x20u,0x24u,0x60u})write(b,r.esi+offset,0);
    write(b,r.esi+0x64,0,1);write(b,r.esi+0x0c,0);write(b,r.esi+0x10,0);
}
void mission_character_seed_pose(const MissionCharacterAllocateBindings& b,MissionCharacterAllocateRegisters& r){
    r.ebp=read(b,r.esi);r.ebp=read(b,r.ebp+0x14);r.edi=read(b,r.ebp+r.eax*4);if(!r.edi)return;
    r.edx=read(b,r.edi+4);r.edi+=0x14;advance(b,r);r.edi+=read(b,r.esi+0x58);r.ebp=read(b,r.esi+0x80);
    for(;;){
        r.eax=0;r.ebx=0;r.ecx=0;r.eax=read(b,r.edi,1);r.ebx=read(b,r.edi+1,1);r.ecx=read(b,r.edi+2,1);
        CharacterFrame frame;frame.packed_angles.push_back({std::uint8_t(r.eax),std::uint8_t(r.ebx),std::uint8_t(r.ecx),0});
        const auto pose=character_frame_pose(frame).front();r.eax=U(pose.angles[0]);r.ebx=U(pose.angles[1]);r.ecx=U(pose.angles[2]);
        write(b,r.ebp,r.eax);write(b,r.ebp+4,r.ebx);write(b,r.ebp+8,r.ecx);
        for(U i=0;i<3;++i)write(b,r.ebp+12+i*4,U(pose.scale[i]));
        r.ebp+=0x18;r.edi+=4;const auto old=r.edx;--r.edx;if(signed32(old)<=1)return;
    }
}
bool mission_character_select_action(const MissionCharacterAllocateBindings& b,MissionCharacterAllocateRegisters& r){
    r.ebp=read(b,r.esi);r.ebp=read(b,r.ebp+0x14);r.edi=read(b,r.ebp+r.eax*4);if(!r.edi)return false;
    write(b,r.esi+0x0c,r.edi);write(b,r.esi+0x66,0,1);r.edi+=0x14;advance(b,r);
    if(!b.call)throw MissionCharacterAllocateError("Character frame decoder43A690 is not bound");
    b.call(0x43a690,r);return true;
}
} // namespace dfre
