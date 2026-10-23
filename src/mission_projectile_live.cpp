#include "dfre/mission_projectile_live.hpp"
#include "dfre/collision.hpp"
#include "dfre/mission_projectile_owner.hpp"
#include <exception>
#include <map>
#include <memory>
#include <set>
#include <cstring>

namespace dfre {
namespace {
template<class F> const F& need(const F& fn,const char* message){if(!fn)throw MissionTickError(message);return fn;}
std::uint32_t word(const AiActorImage& a,std::size_t at){std::uint32_t v;std::memcpy(&v,a.data()+at,4);return v;}
std::int32_t signed_word(std::uint32_t v){std::int32_t r;std::memcpy(&r,&v,4);return r;}
std::int32_t add(std::int32_t a,std::int32_t b){return signed_word(std::uint32_t(a)+std::uint32_t(b));}
struct RawActor {AiActorRef ref;AiActorImage image;};
class LiveProjectiles {
public:
    LiveProjectiles(MissionTickState& live,const MissionGeometry& geometry,const MissionAnimations& animations,
        MissionProjectileLiveResources resources,ProjectileDamageState& damage,MissionProjectileLiveBindings foreign,
        MissionGeometryMatrixSource matrices):live_(live),geometry_(geometry),animations_(animations),
        resources_(std::move(resources)),damage_(damage),foreign_(std::move(foreign)),matrices_(std::move(matrices)) {
        std::set<std::uint32_t> models,identities;
        for(const auto& binding:resources_.materials){
            if(!binding.model||!models.insert(binding.model).second||binding.identities.size()!=binding.flags.size())
                throw MissionTickError("Projectile material table is invalid");
            for(auto id:binding.identities)if(!id||!identities.insert(id).second)
                throw MissionTickError("Projectile material identity is missing or shared");
        }
        impact_.damage.definition=[&](std::uint32_t index){
            if(index>=live_.definitions.entries().size()||index>=resources_.damage_thresholds.size())
                throw MissionTickError("Projectile definition exceeds its table");
            const auto& d=live_.definitions.entries()[index];
            return ProjectileDamageDefinition{d.field_28,d.flags24,resources_.damage_thresholds[index]};
        };
        impact_.damage.owner=[&](std::uint32_t id)->const AiActorImage&{return *record(decode(id)).image;};
        impact_.damage.person_alert=[&](std::uint32_t id)->std::uint8_t&{
            std::size_t slot=id;
            if(id>=kPersonHandleBase){
                const auto offset=id-kPersonHandleBase;
                if(offset%kAiPersonRecordBytes)throw MissionTickError("Projectile person handle is not aligned");
                slot=offset/kAiPersonRecordBytes;
            }
            if(slot>=live_.people.size())throw MissionTickError("Projectile person slot exceeds its table");
            return live_.people[slot].field_9c;
        };
        impact_.damage.alert=[&](auto slot,auto direction){around([&]{need(foreign_.alert,"Projectile alert is not bound")(live_,slot,direction);});};
        impact_.damage.update=[&](auto callback,ProjectileHitActor actor,auto message){
            around([&]{need(foreign_.update,"Projectile actor update is not bound")(live_,decode(actor.identity),callback,message);});
        };
        impact_.damage.local_hit=[&]{around([&]{need(foreign_.local_hit,"Projectile local-hit effect is not bound")(live_);});};
        impact_.material_flags=[&](std::uint32_t id){
            for(const auto& binding:resources_.materials)for(std::size_t i=0;i<binding.identities.size();++i)
                if(binding.identities[i]==id)return binding.flags[i];
            throw MissionTickError("Projectile hit material is not bound");
        };
        impact_.effect=[&](const ProjectileImpactEffect& effect){
            around([&]{need(foreign_.impact,"Projectile impact effect is not bound")(live_,*current_,effect);});
        };
    }
    void begin(){records_.clear();meshes_.clear();hits_={};sync_globals();}
    std::int32_t terrain(const ProjectileEntity& p,const Fixed3& step,std::int32_t length){
        begin();ray_=ray(p,step,length);std::int32_t code=1;
        if(foreign_.terrain)around([&]{code=foreign_.terrain(live_,p,step,length);});
        else code=mission_projectile_terrain_query(live_,p,step,length);
        return code;
    }
    WorldHitQuery ray(const ProjectileEntity& p,const Fixed3& step,std::int32_t length) const {
        WorldHitQuery q;q.origin={p.position.x,p.position.y,p.position.z};
        q.end={add(p.position.x,p.velocity.x),add(p.position.y,p.velocity.y),add(p.position.z,p.velocity.z)};
        q.direction={step.x,step.y,step.z};q.length=length;
        for(std::size_t axis=0;axis<3;++axis){q.bounds[axis*2]=std::min(q.origin[axis],q.end[axis]);q.bounds[axis*2+1]=std::max(q.origin[axis],q.end[axis]);}
        const auto owner=owner_identity(p.owner_slot);q.skip[0]=(p.class_flags&0x20)?0:owner;return q;
    }
    std::int32_t model_query(unsigned source,const ProjectileEntity&,const Fixed3&,std::int32_t,std::int32_t& distance){
        auto actors=list(source==1?1:0);ProjectileModelBindings b;
        b.radius=[&](auto model){return geometry_.bounds(model).height_60;};
        b.state=[&](std::uint32_t model,ProjectileHitActor actor)->const std::vector<ProjectileHitMesh>*{
            const MissionProjectileMaterialBinding* materials=nullptr;
            for(const auto& binding:resources_.materials)if(binding.model==model){materials=&binding;break;}
            if(!materials)throw MissionTickError("Projectile model materials are not bound");
            auto& saved=meshes_[actor.identity];saved=geometry_.projectile_meshes(model,*actor.image,materials->identities,matrices_);return &saved;
        };
        b.divide_error=[&](auto n,auto d){around([&]{need(foreign_.divide_error,"Projectile divide-error effect is not bound")(live_,n,d);});};
        auto& hit=hits_[source];const auto code=projectile_model_hit(actors,ray_,hit,b);distance=hit.distance;return code;
    }
    std::int32_t bone_query(const ProjectileEntity&,const Fixed3&,std::int32_t,std::int32_t& distance){
        auto actors=list(3);
        const auto source=[&](ProjectileHitActor actor){
            if(!word(*actor.image,0x10))return ProjectileBoneState{};
            const auto instance=word(*actor.image,0x18);const CharacterModel* character=nullptr;
            for(const auto& binding:resources_.characters)if(binding.instance==instance){character=binding.character;break;}
            const auto* pose=animations_.pose(instance);
            if(!instance||!character||!pose)throw MissionTickError("Projectile character needs its current CHR pose");
            return projectile_character_spheres(*character,*pose,actor);
        };
        const auto code=projectile_bone_hit(actors,ray_,hits_[3],source);distance=hits_[3].distance;return code;
    }
    std::int32_t water(const ProjectileEntity&,const Fixed3&,std::int32_t,std::int32_t& distance){
        if(!live_.world.water_level)return 1;
        const auto code=projectile_water_hit(live_.world.water_level,ray_,hits_[4]);distance=hits_[4].distance;return code;
    }
    bool alternate_move_filter(){return need(foreign_.globals,"Projectile external globals are not bound")(live_).alternate_move_filter;}
    void event(ProjectileEntity& projectile,const ProjectileEvent& event){
        current_=&projectile;
        try {
            if(event.kind==ProjectileEventKind::move)around([&]{need(foreign_.move,"Projectile move effect is not bound")(live_,projectile,event);});
            else if(event.kind==ProjectileEventKind::world_edge)around([&]{need(foreign_.world_edge,"Projectile world-edge effect is not bound")(live_,projectile,event);});
            else {
                if(event.hit_type>4)throw MissionTickError("Projectile impact type exceeds its table");
                ProjectileHitActor target;
                if(event.hit_type>=1&&event.hit_type<=3){
                    const unsigned group=event.hit_type==1?1:event.hit_type==2?0:3;
                    if(!hits_[event.hit_type].hit)throw MissionTickError("Projectile impact has no selected hit");
                    target=record({std::uint8_t(group),hits_[event.hit_type].record});
                }
                sync_globals();
                projectile_impact_effects(projectile,owner_identity(projectile.owner_slot),event.hit_type,event.position,
                    target,hits_[event.hit_type],damage_,impact_);
            }
        } catch(...) {const auto error=std::current_exception();flush();current_=nullptr;std::rethrow_exception(error);}
        flush();current_=nullptr;
    }
private:
    std::uint32_t owner_identity(std::int32_t slot)const{
        if(slot==-1)return 0;
        const auto ref=mission_projectile_owner_ref(live_,slot);return actor_handle(ref.group,ref.index);
    }
    AiActorRef decode(std::uint32_t id)const{
        if(id<kActorHandleBase)throw MissionTickError("Projectile actor identity is invalid");
        const auto key=id-kActorHandleBase,group=key/0x10000u,slot=key%0x10000u;
        if(group>4)throw MissionTickError("Projectile actor identity group is invalid");
        const AiActorRef ref{std::uint8_t(group),slot};(void)mission_projectile_owner_index(live_,ref);return ref;
    }
    ProjectileHitActor record(const AiActorRef& ref){
        (void)mission_projectile_owner_index(live_,ref);
        const auto id=actor_handle(ref.group,ref.index);auto found=records_.find(id);
        if(found==records_.end())found=records_.emplace(id,RawActor{ref,snapshot_mission_actor(live_,ref)}).first;
        return {&found->second.image,id};
    }
    std::vector<ProjectileHitActor> list(unsigned group){
        std::vector<ProjectileHitActor> actors;const auto count=live_.tables.groups[group].size();actors.reserve(count);
        for(std::size_t i=0;i<count;++i)actors.push_back(record({std::uint8_t(group),i}));return actors;
    }
    void sync_globals(){
        const auto globals=need(foreign_.globals,"Projectile external globals are not bound")(live_);
        damage_.host=live_.globals.mission_67f528;damage_.rules_67f549=globals.rules_67f549;
        damage_.water=live_.world.water_level;damage_.local_player=live_.config.local_actor?actor_handle(3,*live_.config.local_actor):0;
    }
    void flush(){std::exception_ptr error;for(const auto& pair:records_)try{apply_mission_actor_image(live_,pair.second.ref,pair.second.image);}catch(...){if(!error)error=std::current_exception();}if(error)std::rethrow_exception(error);}
    void refresh(){for(auto& pair:records_)pair.second.image=snapshot_mission_actor(live_,pair.second.ref);}
    void around(const std::function<void()>& call){
        flush();try{call();}catch(...){const auto error=std::current_exception();refresh();sync_globals();std::rethrow_exception(error);}refresh();sync_globals();
    }
    MissionTickState& live_;const MissionGeometry& geometry_;const MissionAnimations& animations_;
    MissionProjectileLiveResources resources_;ProjectileDamageState& damage_;MissionProjectileLiveBindings foreign_;
    MissionGeometryMatrixSource matrices_;std::map<std::uint32_t,RawActor> records_;
    std::map<std::uint32_t,std::vector<ProjectileHitMesh>> meshes_;std::array<ProjectileHitResult,5> hits_{};
    WorldHitQuery ray_;ProjectileImpactBindings impact_;ProjectileEntity* current_{};
};
}
std::int32_t mission_projectile_terrain_query(const MissionTickState& state,
    const ProjectileEntity& projectile,const Fixed3& step,std::int32_t length){
    if(!state.world.terrain)throw MissionTickError("Projectile terrain geometry is not bound");
    const auto result=terrain_ray_march(*state.world.terrain,
        CollisionSegment{{projectile.position.x,projectile.position.y,projectile.position.z},
                         {step.x,step.y,step.z},length});
    return !result.hit&&result.lambda==0x10000?1:result.lambda;
}
ProjectileQuery mission_projectile_live_query(MissionTickState& state,const MissionGeometry& geometry,
    const MissionAnimations& animations,MissionProjectileLiveResources resources,ProjectileDamageState& damage,
    MissionProjectileLiveBindings bindings,MissionGeometryMatrixSource matrices){
    auto live=std::make_shared<LiveProjectiles>(state,geometry,animations,std::move(resources),damage,std::move(bindings),std::move(matrices));
    ProjectileQuery result;
    result.terrain=[live](const auto& p,const auto& s,auto n,auto& d){d=0;return live->terrain(p,s,n);};
    result.actors_a=[live](const auto& p,const auto& s,auto n,auto& d){return live->model_query(1,p,s,n,d);};
    result.actors_b=[live](const auto& p,const auto& s,auto n,auto& d){return live->model_query(2,p,s,n,d);};
    result.special_a=[live](const auto& p,const auto& s,auto n,auto& d){return live->bone_query(p,s,n,d);};
    result.special_b_enabled=true;
    result.special_b=[live](const auto& p,const auto& s,auto n,auto& d){return live->water(p,s,n,d);};
    result.on_event=[live](auto& p,const auto& e){live->event(p,e);};
    result.use_alternate_move_filter=[live]{return live->alternate_move_filter();};
    result.current_world_center=[&state]{return state.world.world_center;};
    return result;
}
ProjectileQuery mission_projectile_live_query(MissionTickState& state,const MissionGeometry& geometry,
    const MissionAssetCache& cache,const MissionRuntimeState& initial,const MissionAnimations& animations,
    std::vector<MissionProjectileMaterialBinding> materials,ProjectileDamageState& damage,
    MissionProjectileLiveBindings bindings,MissionGeometryMatrixSource matrices){
    MissionProjectileLiveResources resources;
    for(const auto& d:initial.definitions)resources.damage_thresholds.push_back(d.hp_secondary);
    for(auto& binding:materials){
        const Model* model=nullptr;for(const auto& entry:initial.models)if(entry.handle==binding.model){model=&cache.model(entry.name);break;}
        if(!model||model->lods.empty())throw MissionTickError("Projectile material model is not loaded");
        binding.flags.clear();for(const auto& material:model->lods.front().materials)binding.flags.push_back(material.flags);
    }
    resources.materials=std::move(materials);
    for(std::size_t slot=0;slot<initial.character_instances.size();++slot){
        const auto item=initial.character_instances[slot];const auto character=initial.definitions.at(item).character;
        const CharacterModel* model=nullptr;for(const auto& entry:initial.characters)if(entry.second==character){model=&cache.character(entry.first);break;}
        if(!model)throw MissionTickError("Projectile character resource is not loaded");
        resources.characters.push_back({kCharacterInstanceBase+std::uint32_t(slot),model});
    }
    return mission_projectile_live_query(state,geometry,animations,std::move(resources),damage,std::move(bindings),std::move(matrices));
}
} // namespace dfre
