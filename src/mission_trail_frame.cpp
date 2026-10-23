#include "dfre/mission_trail_frame.hpp"
#include <algorithm>
#include <cstring>
#include <limits>
namespace dfre {namespace {
using U=std::uint32_t;
std::int32_t si(U n){std::int32_t value;std::memcpy(&value,&n,4);return value;}
U sar(U n,unsigned shift){return shift?((n>>shift)|((n&0x80000000u)?(~U(0)<<(32-shift)):0)):n;}
U product(U a,U b,unsigned shift){return U(std::uint64_t(std::int64_t(si(a))*si(b))>>shift);}
U absolute(U n){const auto sign=sar(n,31);return (n^sign)-sign;}
U divide(std::int64_t numerator,U denominator){const auto d=si(denominator);if(!d)throw MissionTrailError("Trail source division has a zero divisor");const auto q=numerator/d;if(q<std::numeric_limits<std::int32_t>::min()||q>std::numeric_limits<std::int32_t>::max())throw MissionTrailError("Trail source division exceeds int32");return U(q);}
U divide32(U numerator,U denominator){return divide(si(numerator),denominator);}
U read(const MissionTrailFrameBindings& b,U at){if(!b.read)throw MissionTrailError("Trail current memory reader is required");return b.read(at);}
void write(const MissionTrailFrameBindings& b,U at,U value){if(!b.write)throw MissionTrailError("Trail current memory writer is required");b.write(at,value);}
U blend(U target,U source,std::uint8_t alpha){const auto bits=std::uint16_t(U(alpha)<<8);std::int16_t factor;std::memcpy(&factor,&bits,2);U out=0;for(unsigned i=0;i<4;++i){const auto old=std::int32_t((target>>(8*i))&255);const auto value=std::int32_t((source>>(8*i))&255);const auto delta=si(sar(U((value-old)*factor),16));out|=U(std::max(0,std::min(255,old+delta)))<<(8*i);}return out;}
}
void mission_trail_clip_endpoint(MissionTrailPoint& outside,const MissionTrailPoint& inside,std::int32_t d0,std::int32_t d1){
 const auto magnitude=absolute(U(d0));auto fraction=divide(std::int64_t(si(magnitude))*65536,magnitude+absolute(U(d1)));if(si(fraction)>65536)fraction=65536;
 const std::array<U,3> delta{U(inside[0])-U(outside[0]),U(inside[1])-U(outside[1]),U(inside[2])-U(outside[2])};
 for(unsigned i=0;i<3;++i)outside[i]=si(U(outside[i])+product(fraction,delta[i],16));
}
bool mission_trail_clip(const std::function<U(U)>& current,U camera,MissionTrailPoint& from,MissionTrailPoint& to){
 if(!current)throw MissionTrailError("Trail clipping needs current camera reads");
 for(unsigned plane=0;plane<5;++plane){
  const auto at=camera+0x1880+plane*16;U da,db;
  if(plane==0){const auto a=current(at);const auto ax=U(from[0]);const auto av=product(a,ax,22);da=av-current(at+12);const auto b=current(at);const auto bx=U(to[0]);const auto bv=product(b,bx,22);db=bv-current(at+12);}
  else{const unsigned axis=plane<3?1:2;const auto a=current(at);const auto ax=U(from[0]);const auto first=product(a,ax,22);const auto ay=U(from[axis]);const auto b=current(at+axis*4);da=first+product(ay,b,22);const auto bx=U(to[0]);const auto c=current(at);const auto second=product(bx,c,22);const auto by=U(to[axis]);const auto d=current(at+axis*4);db=second+product(by,d,22);}
  const auto threshold=plane==0?16:0;const bool a=si(da)<threshold,b=si(db)<threshold;if(a&&b)return false;
  if(a!=b){if(si(da)<si(db))mission_trail_clip_endpoint(from,to,si(da),si(db));else mission_trail_clip_endpoint(to,from,si(db),si(da));}
 }
 return true;
}
void mission_trail_frame(const MissionTrailFrameBindings& b,U camera){
 const auto now=read(b,0x6e4724),previous=read(b,0xa158a0);write(b,0xa158a0,now);auto elapsed=now-previous;if(si(elapsed)<0)elapsed=0;if(read(b,0x6e4718))elapsed=0;
 for(U slot=0;slot<2048;++slot){const auto at=0x9e3098+slot*32;const auto life=read(b,at+24);if(si(life)<=0)continue;const auto remaining=life-elapsed,level=sar(remaining,2);write(b,at+24,remaining);if(si(level)<=0){write(b,at+24,0);continue;}
  if(!b.transform)throw MissionTrailError("Trail endpoint transform is required");auto from=b.transform(camera+0x1920,at);auto to=b.transform(camera+0x1920,at+12);
  if(!mission_trail_clip(b.read,camera,from,to))continue;
  MissionTrailLine line;for(unsigned i=0;i<2;++i){const auto& point=i?to:from;const auto reciprocal=divide(0x100000000ll,U(point[0]));const auto scale=product(reciprocal,read(b,camera+0x54)<<16,16);const auto x=product(U(point[1]),scale,16),y=product(U(point[2]),scale,16);line.vertices[i][0]=si((read(b,camera+0x58)<<16)-x);line.vertices[i][1]=si((read(b,camera+0x5c)<<16)-y);}
  const bool far=from[0]>0x3e80000||to[0]>0x3e80000;line.vertices[0][2]=si(far?0xfa000000u:U(from[0])<<6);line.vertices[1][2]=si(far?0xfa000000u:U(to[0])<<6);line.color=read(b,at+28);line.alpha=std::uint8_t(level<<3);
  if(!b.draw)throw MissionTrailError("Trail line draw provider is required");b.draw(line);
 }
}
MissionTrailFrameBindings mission_trail_frame_bindings(MissionProjectileTrailCells& trails,MissionTrailFrameClock& clock,U& tick,U& demo,ModelCameraFrame& camera,std::function<void(const MissionTrailLine&)> draw){
 const auto ring=mission_projectile_trail_bindings(trails);MissionTrailFrameBindings b;
 b.read=[ring,&clock,&tick,&demo,&camera](U at)->U{if(at==0x6e4724)return tick;if(at==0x6e4718)return demo;if(at==0xa158a0)return clock.previous_a158a0;if(at==0x8cfa74)return U(camera.projection.focal_length);if(at==0x8cfa78)return U(camera.projection.centre[0]);if(at==0x8cfa7c)return U(camera.projection.centre[1]);if(at>=0x8d12a0&&at<0x8d12f0&&!(at&3)){const auto off=at-0x8d12a0,index=off/16,word=off%16/4;if(index>=camera.planes.size())throw MissionTrailError("Trail camera plane is not bound");return U(word==3?camera.planes[index].offset:camera.planes[index].normal[word]);}if(at>=0x8d1340&&at<0x8d1370&&!(at&3))return U(camera.matrix[(at-0x8d1340)/4]);return ring.read(at);};
 b.write=[ring,&clock](U at,U value){if(at==0xa158a0){clock.previous_a158a0=value;return;}ring.write(at,value);};
 b.transform=[ring,&camera](U matrix,U at){if(matrix!=0x8d1340)throw MissionTrailError("Trail camera matrix address is invalid");const MissionTrailPoint point{si(ring.read(at)),si(ring.read(at+4)),si(ring.read(at+8))};return model_transform_point(camera.matrix,point);};b.draw=std::move(draw);return b;
}
std::size_t mission_trail_draw32(const MissionTrailLine& line,const RasterClip& clip,U width,U height,std::vector<U>& color,const std::vector<std::uint16_t>& depth){
 if(std::uint64_t(width)*height!=color.size()||color.size()!=depth.size()||clip.left<0||clip.top<0||clip.right<clip.left||clip.bottom<clip.top||U(clip.right)>=width||U(clip.bottom)>=height)throw MissionTrailError("Trail target and clip bounds are invalid");
 U x0=sar(U(line.vertices[0][0]),16),y0=sar(U(line.vertices[0][1]),16),z0=U(line.vertices[0][2])>>17,x1=sar(U(line.vertices[1][0]),16),y1=sar(U(line.vertices[1][1]),16),z1=U(line.vertices[1][2])>>17;
 const auto code=[&](U x,U y){return (si(x)<clip.left?8u:0u)|(si(x)>clip.right?4u:0u)|(si(y)<clip.top?2u:0u)|(si(y)>clip.bottom?1u:0u);};const auto a=code(x0,y0),b=code(x1,y1);if(a&b)return 0;
 const auto cut_x=[&](bool first,U edge){auto& x=first?x0:x1;auto& y=first?y0:y1;auto& z=first?z0:z1;const auto xx=first?x1:x0,yy=first?y1:y0,zz=first?z1:z0;const auto numerator=edge-x,denominator=xx-x;y+=divide32((yy-y)*numerator,denominator);z+=divide32((zz-z)*numerator,denominator);x=edge;};
 if((a&8)!=(b&8))cut_x((a&8)!=0,U(clip.left));if((a&4)!=(b&4))cut_x((a&4)!=0,U(clip.right));
 const auto top0=code(x0,y0)&3,top1=code(x1,y1)&3;if(top0&top1)return 0;
 const auto cut_y=[&](bool first,U edge){auto& x=first?x0:x1;auto& y=first?y0:y1;auto& z=first?z0:z1;const auto xx=first?x1:x0,yy=first?y1:y0,zz=first?z1:z0;const auto numerator=edge-y,denominator=yy-y;x+=divide32((xx-x)*numerator,denominator);z+=divide32((zz-z)*numerator,denominator);y=edge;};
 if((top0&2)!=(top1&2))cut_y((top0&2)!=0,U(clip.top));if((top0&1)!=(top1&1))cut_y((top0&1)!=0,U(clip.bottom));
 const U dx=x1-x0,dy=y1-y0,dz=z1-z0;if(!dx&&!dy)return 0;std::size_t writes=0;
 const auto pixel=[&](U x,U y,U z){if(si(x)<0||si(y)<0||x>=width||y>=height)throw MissionTrailError("Trail source pixel exceeds its target");const auto index=std::size_t(y)*width+x;if(si(sar(z,15))<=depth[index]){color[index]=blend(color[index],line.color,line.alpha);++writes;}};
 if(si(absolute(dy))<si(absolute(dx))){if(si(x1)<si(x0)){std::swap(x0,x1);y0=y1;z0=z1;}U y=(y0<<16)+0x8000,z=(z0<<16)+0x8000;for(U x=x0;si(x)<=si(x1);++x){pixel(x,sar(y,16),z);y+=divide32(dy<<16,dx);z+=divide32(dz<<16,dx);}}
 else{if(si(y1)<si(y0)){std::swap(y0,y1);x0=x1;z0=z1;}U x=(x0<<16)+0x8000,z=(z0<<16)+0x8000;for(U y=y0;si(y)<=si(y1);++y){pixel(sar(x,16),y,z);x+=divide32(dx<<16,dy);z+=divide32(dz<<16,dy);}}
 return writes;
}
}
