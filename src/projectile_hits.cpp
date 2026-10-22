#include "dfre/projectile_hits.hpp"
#include "dfre/actor_queries.hpp"
#include <algorithm>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace dfre {
namespace {
std::int32_t si(std::uint32_t x) { std::int32_t r; std::memcpy(&r,&x,4); return r; }
std::uint32_t word(const AiActorImage& a,std::size_t at) {
    return std::uint32_t(a[at])|std::uint32_t(a[at+1])<<8|std::uint32_t(a[at+2])<<16|std::uint32_t(a[at+3])<<24;
}
void put(AiActorImage& a,std::size_t at,std::uint32_t x) { for(unsigned i=0;i<4;++i)a[at+i]=std::uint8_t(x>>(8*i)); }
void health(AiActorImage& a,std::uint32_t damage) {
    const auto value=std::uint16_t((std::uint32_t(a[0x74])|std::uint32_t(a[0x75])<<8)-damage);
    a[0x74]=std::uint8_t(value);a[0x75]=std::uint8_t(value>>8);
}
std::int32_t add(std::int32_t a,std::int32_t b) { return si(std::uint32_t(a)+std::uint32_t(b)); }
std::int32_t neg(std::int32_t a) { return si(0u-std::uint32_t(a)); }
std::int32_t sub(std::int32_t a,std::int32_t b) { return add(a,neg(b)); }
std::int32_t absword(std::int32_t a) { return a<0?neg(a):a; }
std::int32_t product(std::int32_t a,std::int32_t b) { return si(std::uint32_t((std::int64_t(a)*b)>>16)); }
std::array<std::int32_t,3> position(const AiActorImage& a) { return {si(word(a,0x24)),si(word(a,0x28)),si(word(a,0x2c))}; }
std::int32_t along(const WorldHitQuery& q,const std::array<std::int32_t,3>& p) {
    std::int32_t r=0;for(unsigned i=0;i<3;++i)r=add(r,product(sub(p[i],q.origin[i]),q.direction[i]));return r;
}
std::array<std::int32_t,3> raypoint(const WorldHitQuery& q,std::int32_t distance) {
    std::array<std::int32_t,3> r{};for(unsigned i=0;i<3;++i)r[i]=add(q.origin[i],product(distance,q.direction[i]));return r;
}
std::int32_t separation(const std::array<std::int32_t,3>& a,const std::array<std::int32_t,3>& b) {
    return collision_approximate_length(absword(sub(a[0],b[0])),absword(sub(a[1],b[1])),absword(sub(a[2],b[2])));
}
AiActorImage& image(ProjectileHitActor actor) {
    if(!actor.image)throw std::invalid_argument("Projectile hit needs an actor image");return *actor.image;
}
template<class T> const T& require(const T& provider) {
    if(!provider)throw std::invalid_argument("Projectile hit needs a foreign binding");return provider;
}
std::int32_t divide(std::int64_t numerator,std::int32_t denominator) {
    if(denominator==0)throw std::invalid_argument("Projectile hit division by zero");
    auto q=numerator/denominator;
    if(q<std::numeric_limits<std::int32_t>::min()||q>std::numeric_limits<std::int32_t>::max())
        throw std::invalid_argument("Projectile hit division overflow");
    return std::int32_t(q);
}
}

std::uint32_t projectile_damage_amount(std::uint32_t threshold,const ProjectileEntity& p) noexcept {
    std::uint32_t exponent=p.field_80;
    if(exponent<threshold)return 0;
    if(p.field_7c) {
        const auto shift=((p.field_84-std::uint32_t(p.ttl))/p.field_7c)&31;
        exponent=std::uint32_t(si(exponent)>>shift);
    }
    return 1u<<(exponent&31);
}

std::int32_t projectile_apply_damage(const ProjectileEntity& p,std::uint32_t owner,
    ProjectileHitActor target,std::uint32_t kind,ProjectileDamageState& state,const ProjectileDamageBindings& b) {
    auto& a=image(target);
    if(word(a,4)==0)return 0;
    const auto definition=require(b.definition)(word(a,4));
    if(definition.type==3) {
        if(a[0x6c]&4)return 1;
        const auto direction=(query_angle_477770(p.velocity.x,p.velocity.y)-word(a,0x30)+0x9ffffffeu)>>30;
        if(state.host) {
            if((state.rules_67f549&0x40)&&owner) {
                const auto& from=require(b.owner)(owner);
                if((word(a,0x70)&0xffff)==(word(from,0x70)&0xffff)&&target.identity!=owner&&!(a[0x6c]&0x80))return 1;
            }
            if(!(a[0x6c]&1)) {
                require(b.person_alert)(word(a,0xc))=2;
                require(b.alert)(si(word(a,0x70))>>16,direction);
            }
            const auto amount=projectile_damage_amount(require(b.definition)(word(a,4)).threshold,p);
            if(amount) {
                health(a,kind==2?amount<<2:amount);
                if(kind==2)a[0x6d]|=0x80;
                state.last_kind=kind;state.last_damage=amount;
                put(a,0xf8,owner);
                AiActorState animation;animation.position[2]=word(a,0x2c);
                put(a,0x84,query_anim_id_460410(animation,kind,direction,1,state.water));
                a[0x6d]|=4;
                require(b.update)(word(a,0x110),target,1);
            }
        }
        if(target.identity==state.local_player)require(b.local_hit)();
    } else if(!(p.class_flags&0x100)||!(definition.flags&0x80)) {
        const auto amount=projectile_damage_amount(definition.threshold,p);
        if(amount) {
            state.last_kind=kind;state.last_damage=amount;put(a,0xf8,owner);health(a,amount);
            require(b.update)(word(a,0x110),target,1);
        }
    }
    return 1;
}

std::int32_t projectile_water_hit(std::int32_t water,const WorldHitQuery& q,ProjectileHitResult& out) {
    if(water<q.origin[2]&&water<q.end[2])return 1;
    if(water<q.origin[2]&&sub(q.origin[2],water)>0x200)
        out.distance=neg(divide(std::int64_t(sub(q.origin[2],water))*65536,q.direction[2]));
    else out.distance=0;
    return 0;
}

bool projectile_animated_muzzle(ProjectileHitActor actor,const ProjectileMuzzleBindings& b) {
    auto& a=image(actor);
    const auto weapon=require(b.weapon)(word(a,0x94));
    const auto graphic=word(a,0x10);
    if(!word(a,0x94)||!weapon.model||!graphic)return false;
    const auto pitch=word(a,0x34);put(a,0x34,0);
    const auto world=compose_pose({si(word(a,0x24)),si(word(a,0x28)),si(word(a,0x2c))},
        {word(a,0x30),0,word(a,0x38)});
    put(a,0x34,pitch);
    auto& matrix=require(b.slot_seven)(graphic,actor);
    matrix=matrix_multiply(world,matrix);matrix=matrix_multiply(matrix,b.attachment);
    MuzzleResult result{transform_point(matrix,require(b.hardpoint)(weapon.model,weapon.hardpoint)),
        {word(a,0x30),word(a,0x34),0}};
    if(a[0x6c]&4) {
        const auto v=world_hit_local_direction(matrix.m,{0x4000,0,0});
        result.aim.yaw=query_angle_477770(v[0],v[1]);
        const auto square=std::uint32_t(v[0])*std::uint32_t(v[0])+std::uint32_t(v[1])*std::uint32_t(v[1]);
        result.aim.pitch=query_angle_477770(si(std::uint32_t(collision_sqrt(si(square)))<<7),v[2]);
    }
    require(b.shot)(actor,result);return true;
}

ProjectileBoneState projectile_character_spheres(const CharacterModel& prepared,
    const std::vector<CharacterBonePose>& pose,ProjectileHitActor actor) {
    const auto& a=image(actor);const auto p=position(a);
    const auto world=compose_pose({p[0],p[1],p[2]},{word(a,0x30),word(a,0x34),word(a,0x38)});
    const auto matrices=character_bone_matrices(prepared,pose,0,&world.m);
    ProjectileBoneState state;state.graphic_present=word(a,0x10)!=0;
    for(std::size_t i=0;i<matrices.size();++i) {
        const auto& bone=prepared.bones.at(prepared.bone_map.at(i));
        if(bone.raw.size()<0x38)throw std::invalid_argument("Projectile character bone is short");
        auto read=[&](std::size_t at) {return si(std::uint32_t(bone.raw[at])|std::uint32_t(bone.raw[at+1])<<8|
            std::uint32_t(bone.raw[at+2])<<16|std::uint32_t(bone.raw[at+3])<<24);};
        state.bones.push_back({model_transform_point(matrices[i],{read(0x28),read(0x2c),read(0x30)}),read(0x34)});
    }
    return state;
}

std::int32_t projectile_bone_hit(const std::vector<ProjectileHitActor>& actors,const WorldHitQuery& q,
    ProjectileHitResult& out,const ProjectileBoneProvider& provider) {
    out.hit=false;out.distance=q.length;
    for(std::size_t index=0;index<actors.size();++index) {
        auto actor=actors[index];const auto& a=image(actor);const auto p=position(a);
        if(!word(a,0)||actor.identity==q.skip[0]||(a[0x6c]&2)||word(a,0x78))continue;
        bool inside=true;for(unsigned i=0;i<3;++i)
            inside=inside&&(p[i]>=sub(q.bounds[2*i],0x20000)&&p[i]<=add(q.bounds[2*i+1],0x20000));
        if(!inside)continue;
        const auto distance=along(q,p);
        if(distance<0||separation(raypoint(q,distance),p)>0x18000)continue;
        const auto state=require(provider)(actor);
        if(!state.graphic_present||state.bones.empty())continue;
        if(state.bones.size()<3)throw std::invalid_argument("Projectile character needs three bone slots");
        for(std::size_t i=0;i<state.bones.size();++i) {
            const auto bone=i==0?2:i==2?0:i;const auto& sphere=state.bones[bone];
            const auto candidate=along(q,sphere.center);
            if(candidate<=out.distance&&separation(raypoint(q,candidate),sphere.center)<=sphere.radius) {
                out.hit=true;out.distance=candidate;out.mesh=bone;out.record=index;
            }
        }
    }
    return out.hit?0:1;
}

std::int32_t projectile_model_hit(const std::vector<ProjectileHitActor>& actors,const WorldHitQuery& q,
    ProjectileHitResult& out,const ProjectileModelBindings& b) {
    out.hit=false;out.distance=q.length;
    for(std::size_t index=0;index<actors.size();++index) {
        auto actor=actors[index];const auto& a=image(actor);const auto p=position(a);
        if(!word(a,0)||actor.identity==q.skip[0]||(a[0x6c]&2))continue;
        if(p[0]<sub(q.bounds[0],0x800000)||p[0]>add(q.bounds[1],0x800000)||
           p[1]<sub(q.bounds[2],0x800000)||p[1]>add(q.bounds[3],0x800000))continue;
        const auto model=(a[0x6c]&8)&&word(a,0x14)?word(a,0x14):word(a,0x10);
        if(!model)continue;
        const auto radius=require(b.radius)(model);bool overlap=true;
        for(unsigned i=0;i<3;++i)overlap=overlap&&(q.bounds[2*i]<=add(p[i],radius)&&sub(p[i],radius)<=q.bounds[2*i+1]);
        if(!overlap||separation(raypoint(q,along(q,p)),p)>radius)continue;
        const auto* state=require(b.state)(model,actor);if(!state)continue;
        for(std::size_t mesh_index=0;mesh_index<state->size();++mesh_index) {
            const auto& bound=(*state)[mesh_index];const auto& mesh=bound.geometry;
            if(mesh.state_flags&3)continue;
            if(!mesh.faces||!mesh.face_boxes||!mesh.vertices||!mesh.normals||!mesh.materials||mesh.faces->size()!=mesh.face_boxes->size())
                throw std::invalid_argument("Projectile hit needs complete mesh tables");
            const auto origin=world_hit_local_point(mesh.matrix,q.origin),end=world_hit_local_point(mesh.matrix,q.end);
            const auto direction=world_hit_local_direction(mesh.matrix,q.direction);
            for(std::size_t face_index=0;face_index<mesh.faces->size();++face_index) {
                const auto& face=(*mesh.faces)[face_index];const auto& box=(*mesh.face_boxes)[face_index];
                bool covers=true;for(unsigned i=0;i<3;++i)covers=covers&&std::min(origin[i],end[i])<=box.bounds[2*i+1]&&box.bounds[2*i]<=std::max(origin[i],end[i]);
                if(!covers)continue;
                if(face.material>=mesh.materials->size()||face.material>=bound.material_ids.size()||std::size_t(mesh.normal_start)+face.face_normal>=mesh.normals->size())
                    throw std::invalid_argument("Projectile hit mesh index outside table");
                if((*mesh.materials)[face.material].flags&0x01000000)continue;
                const auto& normal=(*mesh.normals)[mesh.normal_start+face.face_normal];
                auto plane=[&](const auto& point) {auto r=box.plane_constant;for(unsigned i=0;i<3;++i)r=add(r,product(normal.direction[i]*4,point[i]));return r;};
                const auto d0=plane(origin),d1=plane(end);
                if(!((d0>0||d1>0)&&(d0<=0||d1<=0)))continue;
                const auto magnitude=absword(d0),total=add(magnitude,absword(d1));
                std::int32_t candidate;
                if(total<magnitude) {require(b.divide_error)(magnitude,total);candidate=0x40000000;}
                else candidate=product(q.length,divide(std::int64_t(magnitude)*65536,total));
                if(candidate>out.distance)continue;
                std::array<std::int32_t,3> point{};
                for(unsigned i=0;i<3;++i)point[i]=add(origin[i],product(candidate,direction[i]));
                std::array<std::array<std::int16_t,3>,3> corners{};
                for(unsigned i=0;i<3;++i) {
                    if(face.vertices[i]>=mesh.vertices->size())throw std::invalid_argument("Projectile hit vertex outside table");
                    corners[i]=(*mesh.vertices)[face.vertices[i]].position;
                }
                if(!collision_point_in_face(point,corners,normal.flags))continue;
                out.hit=true;out.distance=candidate;out.material=bound.material_ids[face.material];
                out.record=index;out.mesh=mesh_index;out.face=face_index;
            }
        }
    }
    return out.hit?0:1;
}

void projectile_impact_effects(ProjectileEntity& p,std::uint32_t owner,std::uint8_t type,Fixed3 position,
    ProjectileHitActor target,const ProjectileHitResult& hit,ProjectileDamageState& state,const ProjectileImpactBindings& b) {
    auto effect=[&](ProjectileImpactSite site,std::uint32_t argument=0) {
        require(b.effect)(ProjectileImpactEffect{site,position,argument,site==ProjectileImpactSite::explosion?owner:0});
    };
    auto flags=[&](){return std::uint8_t(p.class_flags);};
    auto aged=[&](){return p.field_84-std::uint32_t(p.ttl)>8;};
    auto fragments=[&](std::uint32_t count){effect(ProjectileImpactSite::fragments,count);};
    auto explosion=[&](std::uint32_t kind){effect(ProjectileImpactSite::explosion,kind);};
    auto visual=[&](std::uint32_t kind){effect(ProjectileImpactSite::visual,kind);};
    auto sound=[&](std::uint32_t kind){effect(ProjectileImpactSite::sound,kind);};
    switch(type) {
    case 0:
        if(flags()&0x40)explosion(3);
        else if(!(flags()&0x11)||(!aged()&&!(flags()&0x10))) {
            effect(ProjectileImpactSite::terrain_mark);return;
        } else explosion(1);
        visual(3);sound(0x32);effect(ProjectileImpactSite::ground,0);fragments(200);return;
    case 1:
    case 2: {
        projectile_apply_damage(p,owner,target,std::uint32_t(hit.mesh),state,b.damage);
        if(flags()&0x40) {explosion(3);visual(3);sound(0x32);fragments(200);return;}
        if(flags()&0x11) {
            if(aged()||(flags()&0x10)) {explosion(1);visual(3);sound(0x32);fragments(200);}
            else visual(5);
            return;
        }
        const auto& actor=image(target);
        if(!word(actor,4))return;
        if(require(b.damage.definition)(word(actor,4)).flags&0x40) {visual(0x10);sound(0x39);fragments(5);return;}
        const auto material=require(b.material_flags)(hit.material);
        std::uint32_t count;
        if(material&0x00010000) {visual(0xb);sound(0x37);count=0xf;}
        else if(material&0x00020000) {visual(10);sound(0x36);count=5;}
        else if(material&0x00040000) {visual(0xd);sound(flags()&0x20?0x4e:0x3d);count=0xf;}
        else if(material&0x00080000) {visual(0xf);sound(0x38);count=3;}
        else if(material&0x04000000) {visual(0xc);sound(flags()&0x20?0x4d:0x3e);count=0xf;}
        else if(material&0x08000000) {visual(0xe);sound(0x3f);count=3;}
        else {visual(5);count=5;}
        fragments(count);return;
    }
    case 3:
        projectile_apply_damage(p,owner,target,std::uint32_t(hit.mesh),state,b.damage);
        if(flags()&0x40) {visual(3);explosion(3);sound(0x32);fragments(200);return;}
        if(!(flags()&0x11)) {visual(0x10);sound(0x39);fragments(5);return;}
        if(!aged()&&!(flags()&0x10))return;
        visual(3);explosion(1);sound(0x32);fragments(200);return;
    case 4:
        if((flags()&0x51)&&(aged()||(flags()&0x50))) {explosion(1);visual(4);sound(0x35);fragments(200);}
        else {visual(2);sound(0x3b);fragments(10);}
        return;
    default:throw std::invalid_argument("Projectile impact type outside range");
    }
}
} // namespace dfre
