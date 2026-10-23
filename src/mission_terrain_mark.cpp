#include "dfre/mission_terrain_mark.hpp"
namespace dfre {namespace {
using U=std::uint32_t;
void put(AiActorImage& image,std::size_t at,U value){for(unsigned i=0;i<4;++i)image[at+i]=std::uint8_t(value>>(i*8));}
}
void mission_terrain_mark(MissionTickState& state,Fixed3 position,const MissionTerrainMarkBindings& b){
 AiActorImage image{};const U x=U(position.x),y=U(position.y),z=U(position.z);
 const auto read=[&](U at,U width=4){if(!b.read)throw MissionTickError("Terrain mark source is not bound");return b.read(at,width);};
 const auto call=[&](U site,const std::array<U,6>& args,std::uint8_t* data=nullptr,std::size_t length=0){if(!b.call)throw MissionTickError("Terrain mark body is not bound "+std::to_string(site));b.call(site,args,data,length);};
 const U grid_offset=((x&0x3ff0000u)>>16)+((y&0x3ff0000u)>>6);
 const auto grid=read(0xbd52ac);const auto cell=read(grid+grid_offset,1)&255u;const auto type=read(0xbd5334+cell,1)&255u;
 static constexpr std::array<U,6> indices{0x8e44c4,0x8e44cc,0x8e44d4,0x8e44e4,0x8e44d0,0x8e44c8};
 static constexpr std::array<U,6> sounds{0x3c,0x40,0x3a,0x3d,0x3a,0x3c};
 static constexpr std::array<U,6> radii{5,2,2,10,5,10};
 if(type<1||type>6)return;const auto slot=type-1;const auto index=read(indices[slot]);
 call(0x43e7d0,{sounds[slot],x,y,z,0,0});call(0x45fcf0,{x,y,z,radii[slot],0,0});
 const auto base=read(0xc23810);const auto definition=base+index*0x13cu;
 put(image,0,index);put(image,4,definition);const auto model=read(definition+0x6c);put(image,0x10,model);
 put(image,0x24,x);put(image,0x28,y);put(image,0x2c,z);
 const auto yaw=behavior_rng_next(state.behavior);put(image,0x30,yaw<<16);
 const auto lifetime=behavior_rng_next(state.behavior);put(image,0x7c,lifetime+0x8000u);put(image,0x6c,0);
 const auto update=read(definition+0x84);put(image,0x110,update);
 const auto secondary=read(definition+0x90);put(image,0x10c,secondary);
 call(0x45d970,{0,1,0,0,0,0},image.data(),image.size());
}
} // namespace dfre
