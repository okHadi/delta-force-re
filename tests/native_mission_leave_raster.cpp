#include "dfre/mission_leave_raster.hpp"
#include "dfre/mission_leave.hpp"
#include "dfre/mission_frame_services.hpp"
#include <cassert>
#include <map>
#include <stdexcept>
using namespace dfre;using U=std::uint32_t;
int main(){std::map<U,std::uint8_t> memory;auto write=[&](U at,U width,std::uint64_t n){for(U i=0;i<width;++i)memory[at+i]=std::uint8_t(n>>(i*8));};auto read=[&](U at,U width){std::uint64_t n=0;for(U i=0;i<width;++i)n|=std::uint64_t(memory[at+i])<<(i*8);return n;};
 constexpr U pixels=0x1100000,descriptor=0x1200000,raster=0x67dbf8;write(0x503138,4,1);write(0x50130c,4,pixels);write(0xd092c0,4,4);write(0xd092c4,4,2);write(0x67f400,4,4);write(0x67f404,4,2);write(0x67f41c,4,32);write(0xcc0884,4,1);write(0xd09300,4,1);write(0xd0924c,4,0x1200100);for(U i=0;i<8;++i)write(pixels+i*4,4,0xff804020);
 U requested=0,stat=0;MissionProfilerCells prof{2,0,std::vector<U>(16)};Fixed3 camera_position{};MissionCameraCells camera{camera_position,{},0,0};MissionScriptQueueCells scripts{};std::array<U,3> timers{};std::uint64_t clock=100;MissionFrameServiceBindings fb;fb.rdtsc=[&]{clock+=10;return clock;};MissionFrameServices services({requested,prof,timers,stat,camera,scripts},fb);
 MissionDebriefBindings b;b.read=[&](U at,U width){if(at==0x4f8718)return prof.current_4f8718;return U(read(at,width));};b.write=[&](U at,U width,U n){write(at,width,n);};unsigned presents=0;bool stop=false;
 b.call=[&](U site,const auto& args)->U{if(site==0x456e30){services.profile(args.at(0));return 0;}assert(site==0x1200100);assert(args.empty());++presents;assert(prof.current_4f8718==5);if(stop)throw std::runtime_error("Required display stopped");return 0;};
 MissionLeaveBindings leave;leave.read=read;leave.write=write;leave.call=[&](U site,const auto& args)->U{if(site==0x404540)return mission_leave_frame(b,args.at(0),descriptor);if(site==0x404610){mission_leave_present(b);return 0;}throw std::runtime_error("Stop after real leave raster");};bool stopped=false;try{mission_leave(leave);}catch(const std::runtime_error&){stopped=true;}assert(stopped&&presents==1&&prof.current_4f8718==2);for(U i=0;i<8;++i)assert(read(pixels+i*4,4)==0x7f402010);assert(read(raster+4,4)==pixels&&read(raster+8,4)==pixels+16);
 // A display failure keeps the real source profiling prefix.
 stop=true;stopped=false;try{mission_leave_present(b);}catch(const std::runtime_error&){stopped=true;}assert(stopped&&prof.current_4f8718==5);
 write(0x503138,4,2);const auto before=presents;mission_leave_present(b);assert(before==presents);assert(mission_leave_frame(b,raster,descriptor)==0);
 // Bits alone do not invalidate current row caches.
 write(0x503138,4,1);write(descriptor+20,4,0);mission_raster_rows(b,raster,descriptor);assert(read(raster+0x1824,4)==0);
 // Cache miss writes pixel rows before reaching signed divide-by-zero.
 write(raster+0x1808,4,0);stopped=false;try{mission_raster_rows(b,raster,descriptor);}catch(const std::runtime_error&){stopped=true;}assert(stopped&&read(raster+4,4)==pixels);
}
