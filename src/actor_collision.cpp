#include "dfre/actor_collision.hpp"
#include "dfre/actor_queries.hpp"
#include "dfre/projectiles.hpp"
#include "dfre/world_hits.hpp"
#include <cstring>
#include <stdexcept>

namespace dfre {
namespace {
std::uint32_t word(const AiActorImage& a,unsigned at) {return std::uint32_t(a[at])|std::uint32_t(a[at+1])<<8|std::uint32_t(a[at+2])<<16|std::uint32_t(a[at+3])<<24;}
void put(AiActorImage& a,unsigned at,std::uint32_t n) {for(unsigned i=0;i<4;++i)a[at+i]=std::uint8_t(n>>(8*i));}
std::int32_t si(std::uint32_t x) {std::int32_t r;std::memcpy(&r,&x,4);return r;}
std::int32_t add(std::int32_t a,std::int32_t b) {return si(std::uint32_t(a)+std::uint32_t(b));}
std::int32_t sub(std::int32_t a,std::int32_t b) {return si(std::uint32_t(a)-std::uint32_t(b));}
std::int32_t absolute(std::int32_t a) {return a<0?sub(0,a):a;}
std::int32_t product(std::int32_t a,std::int32_t b,unsigned shift=16) {return si(std::uint32_t((std::int64_t(a)*b)>>shift));}
AiActorImage& image(ActorCollisionActor actor) {if(!actor.image)throw std::invalid_argument("Actor collision needs an image");return *actor.image;}
template<class T> const T& required(const T& fn) {if(!fn)throw std::invalid_argument("Actor collision needs a foreign binding");return fn;}
ActorCollisionDefinition definition(const AiActorImage& a,const ActorCollisionBindings& b) {return required(b.definition)(word(a,4));}
std::array<std::int32_t,3> position(const AiActorImage& a) {return {si(word(a,0x24)),si(word(a,0x28)),si(word(a,0x2c))};}
bool overlaps(const std::array<std::int32_t,3>& p,const std::array<std::int32_t,6>& bounds) {
    return sub(p[0],0xc000)<=bounds[1]&&bounds[0]<=add(p[0],0xc000)&&
        sub(p[1],0xc000)<=bounds[3]&&bounds[2]<=add(p[1],0xc000)&&
        sub(p[2],0xc000)<=bounds[5]&&bounds[4]<=add(p[2],0x10000);
}
}

std::uint32_t actor_target_collision(ActorCollisionActor actor,ActorCollisionActor target,const ActorCollisionBindings& b) {
    if(!target.image)return 0;
    auto& t=image(target);const auto handle=(t[0x6c]&8)&&word(t,0x14)?word(t,0x14):word(t,0x10);
    if(!handle)return 0;
    const auto model=required(b.model)(handle);const auto& a=image(actor);
    const auto p=position(a),q=position(t);const auto radius=model.radius_64;
    for(unsigned i=0;i<2;++i)
        if(sub(p[i],0xc000)>add(q[i],radius)||add(p[i],0xc000)<sub(q[i],radius))return 0;
    if(!model.collision_present)return 0;
    const auto* meshes=required(b.state)(handle,target);if(!meshes)return 0;
    std::uint32_t hit=0;
    for(const auto& mesh:*meshes) {
        if(mesh.volumes.empty())continue;
        auto inverse=mesh.matrix;
        for(unsigned row=0;row<3;++row) {
            for(unsigned column=0;column<3;++column)inverse[row*4+column]=mesh.matrix[column*4+row];
            inverse[row*4+3]=sub(0,mesh.matrix[row*4+3]);
        }
        auto local=world_hit_local_point(inverse,position(image(actor)));
        if(!overlaps(local,mesh.bounds))continue;
        bool moved=false;
        for(const auto& volume:mesh.volumes) {
            if(volume.type!=1&&volume.type!=3)continue;
            if(!overlaps(local,volume.bounds))continue;
            if(volume.type==1) {
                std::array<std::int32_t,3> delta{};
                for(unsigned axis=0;axis<3;++axis) {
                    const auto low=sub(local[axis],0xc000),high=add(local[axis],axis==2?0x10000:0xc000);
                    if(local[axis]<volume.center[axis]) {
                        if(volume.bounds[axis*2]<high)delta[axis]=sub(volume.bounds[axis*2],high);
                    } else if(low<volume.bounds[axis*2+1])delta[axis]=sub(volume.bounds[axis*2+1],low);
                }
                unsigned axis;
                if(absolute(delta[1])>=absolute(delta[0])&&absolute(delta[2])>=absolute(delta[0]))axis=0;
                else if(absolute(delta[0])>=absolute(delta[1])&&absolute(delta[2])>=absolute(delta[1]))axis=1;
                else axis=2;
                local[axis]=add(local[axis],delta[axis]);moved=true;
            } else {
                const auto dx=sub(local[0],volume.center[0]),dy=sub(local[1],volume.center[1]);
                const auto distance=si(std::uint32_t(collision_sqrt(add(product(dx,dx),product(dy,dy))))<<8);
                const auto extent=add(volume.horizontal_radius,0xc000);
                if(distance>extent)continue;
                const auto radius_out=product(extent,0x1028f);
                std::int32_t sine,cosine;sincos_q22(query_angle_477770(dx,dy),sine,cosine);
                local[0]=add(volume.center[0],product(cosine,radius_out,22));
                local[1]=add(volume.center[1],product(sine,radius_out,22));moved=true;
            }
        }
        if(moved) {
            if(!(definition(image(target),b).flags&0x10)) {
                const auto pushed=model_transform_point(mesh.matrix,local);
                for(unsigned i=0;i<3;++i)put(image(actor),0x24+i*4,std::uint32_t(pushed[i]));
            }
            hit=1;
        }
    }
    if(b.host&&hit&&(definition(image(target),b).flags&0x10))actor_collision_trigger(actor,target,b);
    return hit;
}

std::uint32_t actor_collision_trigger(ActorCollisionActor actor,ActorCollisionActor target,
    const ActorCollisionBindings& b,std::uint32_t incoming_eax) {
    if(!b.host)return incoming_eax;
    auto& a=image(actor);auto& t=image(target);
    auto id=definition(t,b).id;
    auto call=[&](ActorCollisionSite site,std::initializer_list<std::uint32_t> values) {
        ActorCollisionCall request{site};request.count=std::uint32_t(values.size());
        unsigned i=0;for(auto value:values)request.arguments[i++]=value;
        return required(b.call)(request);
    };
    auto live=[&](){return (a[0x6c]&1)&&!(a[0x6c]&4);};
    auto pickup=[&](){return call(ActorCollisionSite::pickup,{actor.identity,target.identity});};
    auto refill=[&]() {if(!(a[0x6c]&1))return id;call(ActorCollisionSite::refill,{actor.identity});return call(ActorCollisionSite::remove,{target.identity});};
    if(id==0xfaa||id==0xfd7)return refill();
    if(id==0xfcd) {if(!(a[0x6c]&1))return id;call(ActorCollisionSite::heal,{actor.identity});return call(ActorCollisionSite::remove,{target.identity});}
    if(id==0xfc3||id==0xfc4) {
        auto result=id==0xfc4?(id&0xffff00ffu)|(std::uint32_t(a[0x6c])<<8):id;
        if(!live())return result;
        const auto side=word(a,0x70)&0xffff;
        if(side==(id==0xfc4?1u:2u)) {
            auto allowed=call(ActorCollisionSite::can_use,{target.identity});
            if(!allowed)return 0;
            return call(ActorCollisionSite::deposit,{actor.identity});
        }
        if(word(a,0x78)||word(a,0x90))return result;
        return pickup();
    }
    if(id==0xfcb||id==0xfcc) {
        if(!live()||(word(a,0x70)&0xffff)!=(id==0xfcb?1u:2u))return id;
        const auto carried=word(a,0x90);if(!carried)return id==0xfcb?0:id;
        const auto carried_actor=required(b.actor)(carried);const auto& c=image(carried_actor);
        if(!word(c,4)) {put(a,0x90,0);return carried;}
        if(definition(c,b).id!=(id==0xfcb?0xfc3u:0xfc4u))return carried;
        auto result=call(id==0xfcb?ActorCollisionSite::deliver_side_one:ActorCollisionSite::deliver_side_two,{actor.identity,carried_actor.identity});
        put(a,0x90,0);return result;
    }
    if(id==0xfd6) {
        if(std::int16_t(word(t,0x74)&0xffff)<1)return id;
        call(ActorCollisionSite::objective,{actor.identity,target.identity+0x24,0});
        const auto p=position(t);
        auto result=call(ActorCollisionSite::objective_effect,{target.identity,std::uint32_t(p[0]),std::uint32_t(p[1]),std::uint32_t(add(p[2],0x8000))});
        t[0x74]=t[0x75]=t[0x76]=t[0x77]=0;
        result=(result&0xffff00ffu)|(std::uint32_t(t[0x6c])<<8)|0x400;
        t[0x6c]=std::uint8_t(result>>8);return result;
    }
    if(id==0xfa8||id==0xfb2||id==0xfb5||id==0xfc2||id==0xfc6||id==0xfed) {
        if(live()&&!word(a,0x90)) {pickup();return call(ActorCollisionSite::sound,{0x166});}
        return (id&0xffffff00u)|a[0x6c];
    }
    return id;
}
} // namespace dfre
