#include "dfre/mission_trail_frame.hpp"
#include <cassert>
#include <memory>
using namespace dfre;
namespace {using U=std::uint32_t;template<class F>void rejects(F f){bool failed=false;try{f();}catch(const MissionTrailError&){failed=true;}assert(failed);}}
int main(){
 const RasterClip clip{0,0,31,23};auto camera=make_model_camera({0,0,0},{0,0,0},clip,90);auto trails=std::make_unique<MissionProjectileTrailCells>();MissionTrailFrameClock clock;U tick=1,demo=0;std::vector<U> pixels(32*24);std::vector<std::uint16_t> depth(pixels.size(),65535);const auto old_depth=depth;std::size_t draws=0,writes=0;MissionTrailLine captured;
 trails->ring_4f9148=2046;assert(mission_projectile_trail_record(mission_projectile_trail_bindings(*trails),{10u<<16,0,0,10u<<16,2u<<16,0,0xb9ffa0a0})==2047);
 auto b=mission_trail_frame_bindings(*trails,clock,tick,demo,camera,[&](const MissionTrailLine& line){++draws;captured=line;writes+=mission_trail_draw32(line,clip,32,24,pixels,depth);});mission_trail_frame(b);assert(draws==1&&writes>0&&depth==old_depth&&trails->records_9e3098[2047][6]==29&&clock.previous_a158a0==1&&captured.alpha==56&&captured.color==0xb9ffa0a0);
 draws=0;mission_trail_frame(b);assert(draws==1&&trails->records_9e3098[2047][6]==29);demo=1;tick=200;mission_trail_frame(b);assert(clock.previous_a158a0==200&&trails->records_9e3098[2047][6]==29);demo=0;tick=201;mission_trail_frame(b);assert(trails->records_9e3098[2047][6]==28);tick=229;draws=0;mission_trail_frame(b);assert(draws==0&&trails->records_9e3098[2047][6]==0);
 auto& last=trails->records_9e3098[2047];last[6]=4;tick=230;mission_trail_frame(b);assert(last[6]==0);last[6]=0xffffffff;tick=231;mission_trail_frame(b);assert(last[6]==0xffffffff);
 last[6]=30;tick=0x800000e7;mission_trail_frame(b);assert(last[6]==30&&clock.previous_a158a0==tick);mission_projectile_trail_clear(*trails);assert(last[6]==30&&trails->ring_4f9148==2047);
 last[6]=30;++tick;auto missing=b;missing.draw={};rejects([&]{mission_trail_frame(missing);});assert(last[6]==29&&clock.previous_a158a0==tick);last[6]=30;++tick;missing=b;missing.transform={};rejects([&]{mission_trail_frame(missing);});assert(last[6]==29);const auto previous=clock.previous_a158a0;missing=b;missing.write={};rejects([&]{mission_trail_frame(missing);});assert(clock.previous_a158a0==previous);rejects([&]{mission_trail_frame({});});
 last[6]=30;trails->records_9e3098[0]=last;last[6]=0;draws=0;++tick;std::vector<MissionTrailLine> lines;auto current=mission_trail_frame_bindings(*trails,clock,tick,demo,camera,[&](const MissionTrailLine& line){lines.push_back(line);if(++draws==1){last=trails->records_9e3098[0];last[6]=30;last[7]=0x11223344;camera.projection.focal_length*=2;}});mission_trail_frame(current);assert(draws==2&&last[6]==29&&lines[1].color==0x11223344&&lines[0].vertices!=lines[1].vertices);
 trails->records_9e3098[0][6]=0;last[0]=1001u<<16;last[3]=10u<<16;last[6]=30;++tick;draws=0;mission_trail_frame(b);assert(draws==1&&U(captured.vertices[0][2])==0xfa000000&&U(captured.vertices[1][2])==0xfa000000);
 MissionTrailPoint outside{0,0,0},inside{65536,131072,196608};mission_trail_clip_endpoint(outside,inside,-1,1);assert((outside==MissionTrailPoint{32768,65536,98304}));rejects([&]{mission_trail_clip_endpoint(outside,inside,0,0);});
 MissionTrailLine flat;flat.vertices={MissionTrailPoint{2<<16,3<<16,0},MissionTrailPoint{4<<16,3<<16,0}};flat.color=0xffffffff;flat.alpha=56;std::fill(pixels.begin(),pixels.end(),0);std::fill(depth.begin(),depth.end(),0);depth[3*32+2]=1;auto saved=depth;assert(mission_trail_draw32(flat,clip,32,24,pixels,depth)==1&&pixels[3*32+2]==0x37373737&&depth==saved);
 flat.vertices[1]=flat.vertices[0];assert(mission_trail_draw32(flat,clip,32,24,pixels,depth)==0);rejects([&]{mission_trail_draw32(flat,{0,0,32,23},32,24,pixels,depth);});
}
