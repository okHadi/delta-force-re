#include "dfre/mission_object_destruction.hpp"
#include <cstring>
namespace dfre {namespace {
using U=std::uint32_t;
U address(const AiActorRef& r){static constexpr U bases[]{0x7dde80,0x867e80,0x8ace80,0x7cca80};if(r.group>=4)throw MissionTickError("Destruction actor group is invalid");return bases[r.group]+U(r.index)*0x114;}
U get(const std::uint8_t* p,std::size_t at,unsigned width=4){U n=0;for(unsigned i=0;i<width;++i)n|=U(p[at+i])<<(i*8);return n;}
void put(std::uint8_t* p,std::size_t at,U n,unsigned width=4){for(unsigned i=0;i<width;++i)p[at+i]=std::uint8_t(n>>(i*8));}
struct Body {
 MissionTickState& state;const AiActorRef& ref;const MissionObjectDestructionBindings& b;
 U read(U off,unsigned width=4){if(b.before_actor_read)b.before_actor_read(address(ref)+off);const auto image=snapshot_mission_actor(state,ref);return get(image.data(),off,width);}
 U draw(){ObjectBehaviorState random;random.rng=state.behavior.rng;const auto n=object_behavior_rng_next(random);state.behavior.rng=random.rng;return n;}
 void call(U site,std::array<U,6> args={},std::uint8_t* data=nullptr,std::size_t size=0){if(!b.call)throw MissionTickError("Destruction foreign body is not bound "+std::to_string(site));b.call(site,args,data,size);}
 std::array<std::uint8_t,24> pose(){std::array<std::uint8_t,24> p{};for(U i=0;i<6;++i)put(p.data(),i*4,read(0x24+i*4));return p;}
 void sound(U sound){const auto z=read(0x2c);const auto y=read(0x28);const auto x=read(0x24);call(0x43e7d0,{sound,x,y,z,0,0});}
 U item(U at,U off){if(!b.item_word)throw MissionTickError("Destruction item record is not bound");return b.item_word(at,off);}
};
}
void mission_object_destroy_generic_raw(MissionTickState& state,const AiActorRef& ref,const MissionObjectDestructionBindings& b,U incoming){
 Body h{state,ref,b};const auto definition=h.read(4);if(!b.definition)throw MissionTickError("Destruction definition is not bound");const auto id=b.definition(definition,0x20);U sound=0,count=incoming;
 static constexpr U map[][3]{{0x3e9,0x135,1},{0x3ea,0x136,3},{0x40f,0x137,4},{0x410,0x138,1},{0x411,0x13f,3},{0x412,0x145,3},{0x414,0x146,7},{0x416,0x145,8},{0x418,0x138,1},{0x419,0x145,8},{0xbc0,0x13c,8},{0xfaf,0x13d,5},{0xfb3,0x13e,4}};
 for(const auto& row:map)if(id==row[0]){sound=row[1];count=row[2];}
 if(sound)h.sound(sound);
 auto p=h.pose();put(p.data(),10,get(p.data(),10,2)+1,2);
 h.call(0x460a30,{0,0,0x14,count<<14,0,0},p.data(),p.size());
 h.call(0x447610,{0,get(p.data(),0),get(p.data(),4),get(p.data(),8)-0x8000,0,0});
}
void mission_object_destroy_generic(MissionTickState& state,const AiActorRef& ref,const MissionObjectDestructionBindings& b){mission_object_destroy_generic_raw(state,ref,b,address(ref));}
void mission_object_destroy_wood(MissionTickState& state,const AiActorRef& ref,const MissionObjectDestructionBindings& b,bool large){
 Body h{state,ref,b};auto p=h.pose();
 put(p.data(),8,get(p.data(),8)+0x8000);
 if(large){const auto x=get(p.data(),0),y=get(p.data(),4),z=get(p.data(),8);for(unsigned i=0;i<20;++i){const auto dy=((h.draw()&0xffff)<<3)-0x40000;const auto dx=((h.draw()&0xffff)<<3)-0x40000;h.call(0x42a440,{x+dx,y+dy,z,0,0,0});}}
 else h.call(0x42a440,{get(p.data(),0),get(p.data(),4),get(p.data(),8),0,0,0});
 if(!b.lookup_4828c0||!b.item_table_base)throw MissionTickError("Destruction item lookup is not bound");
 const auto index1=b.lookup_4828c0(0xfe5);const auto ptr1=b.item_table_base()+index1*0x13c;
 const auto index2=b.lookup_4828c0(0xfe6);const auto ptr2=b.item_table_base()+index2*0x13c;
 p=h.pose();std::array<std::uint8_t,0x114> image{};const auto small_z=get(p.data(),8)+0x10000;
 for(unsigned i=0;i<(large?64u:6u);++i){
  const bool first=(h.draw()&0x100)!=0;const auto ptr=first?ptr1:ptr2;put(image.data(),0,first?index1:index2);put(image.data(),4,ptr);put(image.data(),0x10,h.item(ptr,0x6c));
  const auto rx=h.draw();put(image.data(),0x24,get(p.data(),0)+(large?((rx&0xffff)<<3)-0x40000:(rx&0x3fff)-0x2000));
  const auto ry=h.draw();put(image.data(),0x28,get(p.data(),4)+(large?((ry&0xffff)<<3)-0x40000:(ry&0x1fff)-0x1000));
  put(image.data(),0x2c,large?get(p.data(),8)+(h.draw()&0xffff)+0x8000:small_z);
  put(image.data(),0x3c,(h.draw()&(large?0x1fff:0xfff))-(large?0x1000:0x800));
  put(image.data(),0x40,(h.draw()&(large?0x1fff:0xfff))-(large?0x1000:0x800));
  put(image.data(),0x44,(h.draw()&(large?0x1fff:0xfff))+(large?0x800:0x600));
  put(image.data(),0x30,h.draw()<<16);put(image.data(),0x7c,(h.draw()&0x7fff)+0x20000);
  // Source uses the first definition's two callbacks for both debris choices.
  put(image.data(),0x110,h.item(ptr1,0x84));put(image.data(),0x10c,h.item(ptr1,0x90));
  h.call(0x45d970,{0,0,0,0,0,0},image.data(),image.size());
 }
 h.sound(large?0x13b:0x139);
 if(!h.read(0x72,2))return;
 const auto side=h.read(0x70)>>16;const auto model=h.read(0x10);if(!b.model)throw MissionTickError("Destruction model is not bound");const auto radius=b.model(model,0x64);
 const auto z=h.read(0x2c);const auto max_y=h.read(0x28)+radius;const auto max_x=h.read(0x24)+radius;const auto min_y=h.read(0x28)-radius;const auto min_x=h.read(0x24)-radius;
 h.call(0x44fcb0,{min_x,min_y,max_x,max_y,z,side>=0x8000?side|0xffff0000:side});
}
void mission_object_destroy_broadcast(MissionClientOutbox& outbox,U actor,U value){const U index=(actor-0x7cca80)/0x114;std::array<std::uint8_t,4> packet{};put(packet.data(),0,index,2);put(packet.data(),2,value,2);outbox.broadcast(0x39,packet.data(),4);}
} // namespace dfre
