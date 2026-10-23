#include "dfre/mission_character_allocate.hpp"
#include <algorithm>
#include <utility>
namespace dfre {namespace {
using U=std::uint32_t;
std::uint8_t cold_byte(std::size_t offset){
    for(const auto& cell:std::array<std::pair<U,U>,3>{{{0x436ea4,0x00019010},{0x436ea8,0x0066d564},{0x436eac,0x0069683c}}}){
        const auto first=cell.first-mission_character_instance_base;
        if(offset>=first&&offset<first+4)return std::uint8_t(cell.second>>((offset-first)*8));
    }return 0;
}
U read(const MissionCharacterAllocateBindings& b,U at,U width){if(!b.read)throw MissionCharacterAllocateError("Character allocation needs current memory reads");return b.read(at,width);}
void write(const MissionCharacterAllocateBindings& b,U at,U width,U n){if(!b.write)throw MissionCharacterAllocateError("Character allocation needs current memory writes");b.write(at,width,n);}
void call(const MissionCharacterAllocateBindings& b,U site,MissionCharacterAllocateRegisters& r){if(!b.call)throw MissionCharacterAllocateError("Character initialization body is not bound");b.call(site,r);}
}
MissionCharacterAllocateState::MissionCharacterAllocateState(const std::uint8_t* image,std::size_t bytes){
    if(!image||bytes!=bytes_.size())throw MissionCharacterAllocateError("Character cold source image is incomplete");
    for(std::size_t i=0;i<bytes;++i)if(image[i]!=cold_byte(i))throw MissionCharacterAllocateError("Character cold source image does not match the supplied game");
    std::copy(image,image+bytes,bytes_.begin());
}
bool MissionCharacterAllocateState::claims(U at,U width) const{
    if(width!=1&&width!=2&&width!=4)throw MissionCharacterAllocateError("Character access width must be 1, 2 or 4");
    const std::uint64_t end=std::uint64_t(at)+width,first=mission_character_instance_base,last=first+bytes_.size();
    if(end>0x100000000ull)throw MissionCharacterAllocateError("Character access wraps its address");
    if(at>=first&&end<=last)return true;
    if(at<last&&end>first)throw MissionCharacterAllocateError("Character access crosses the source pool boundary");return false;
}
U MissionCharacterAllocateState::read(U at,U width) const{if(!claims(at,width))throw MissionCharacterAllocateError("Character address has no current owner");U n=0;for(U i=0;i<width;++i)n|=U(bytes_[at-mission_character_instance_base+i])<<(i*8);return n;}
void MissionCharacterAllocateState::write(U at,U width,U n){if(!claims(at,width))throw MissionCharacterAllocateError("Character address has no current owner");for(U i=0;i<width;++i)bytes_[at-mission_character_instance_base+i]=std::uint8_t(n>>(i*8));}
MissionCharacterInstanceView MissionCharacterAllocateState::instance(std::size_t slot){if(slot>=256)throw MissionCharacterAllocateError("Character slot exceeds its physical pool");return {bytes_.data()+slot*0x9c,0x9c,mission_character_instance_base+U(slot)*0x9c};}
void mission_character_allocate_raw(const MissionCharacterAllocateBindings& b,MissionCharacterAllocateRegisters& r){
    r.eax=0;r.ebx=0;while(read(b,mission_character_occupied_base+r.ebx,1))if(++r.ebx>255)return;
    write(b,mission_character_occupied_base+r.ebx,1,1);r.eax=mission_character_instance_base+r.ebx*0x9c;
    const auto saved=r;r.esi=r.eax;call(b,0x439e48,r);
    r.esi=saved.eax;r.eax=0;r.ebx=0;call(b,0x43a0e0,r);
    r.esi=saved.eax;r.eax=0;r.ebx=0;call(b,0x43a150,r);r=saved;
}
U mission_character_allocate(const MissionCharacterAllocateBindings& b,U resource,MissionCharacterAllocateRegisters& r,U frame){
    const auto caller=r;r.eax=resource;if(!resource)return 0;
    if(frame<8)throw MissionCharacterAllocateError("Character source frame is incomplete");
    r.ebp=frame;r.eax=read(b,resource+0x60,4);write(b,frame-8,4,r.eax);const auto saved_wrapper=r;
    r.edi=read(b,frame-8,4);mission_character_allocate_raw(b,r);r.ebp=frame;
    write(b,frame-8,4,r.eax);write(b,frame-4,4,r.ebx);
    r=saved_wrapper;r.eax=read(b,frame-8,4);const auto result=r.eax;r=caller;r.eax=result;return result;
}
} // namespace dfre
