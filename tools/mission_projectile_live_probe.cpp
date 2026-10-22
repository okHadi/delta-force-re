#include "dfre/mission_projectile_live.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
namespace {
template<class B> void put(B& b,std::size_t at,std::uint32_t v){for(unsigned i=0;i<4;++i)b[at+i]=std::uint8_t(v>>(i*8));}

}
// Rows: group actor_position[3] actor_angles[3] origin[3] velocity[3]
//       projectile_flags ttl exponent threshold material_identity_base [owner_slot].
int main(int argc,char** argv){try{
    if(argc==4&&std::string(argv[1])=="--terrain"){
        const dfre::AssetStore assets(argv[2]);const auto terrain=dfre::load_terrain_runtime(assets,argv[3]);
        dfre::MissionTickState state;state.world.terrain=&terrain.geometry;
        std::int32_t length;dfre::ProjectileEntity p;dfre::Fixed3 step;
        while(std::cin>>length>>p.position.x>>p.position.y>>p.position.z>>step.x>>step.y>>step.z)
            std::cout<<dfre::mission_projectile_terrain_query(state,p,step,length)<<'\n';
        return 0;
    }
    if(argc!=2)throw std::invalid_argument("usage: mission_projectile_live_probe MODEL.3DI");
    std::ifstream file(argv[1],std::ios::binary);if(!file)throw std::invalid_argument("model file missing");
    const dfre::AssetBytes bytes(std::istreambuf_iterator<char>(file),{});
    auto model=dfre::decode_model(bytes);for(auto& lod:model.lods)lod=dfre::localize_model_lod(lod);
    dfre::MissionGeometry geometry({dfre::mission_geometry_model(17,model,bytes)});
    dfre::MissionAnimations animations(std::vector<dfre::MissionAnimationBinding>{});std::string line;
    while(std::getline(std::cin,line)){
        std::istringstream in(line);unsigned group;in>>group;if(group>1)throw std::invalid_argument("probe requires model group zero or one");
        dfre::AiActorTables tables;tables.groups[group].resize(1);auto& actor=tables.groups[group][0];
        actor.active=true;actor.definition_index=1;actor.field_10=17;actor.field_110=0x6000000;
        for(auto& v:actor.position){std::int64_t x;in>>x;v=std::uint32_t(x);}
        std::uint32_t angles[3];for(auto& v:angles)in>>v;
        dfre::ProjectileEntity p;in>>p.position.x>>p.position.y>>p.position.z>>p.velocity.x>>p.velocity.y>>p.velocity.z;
        std::uint32_t threshold,material_base;in>>p.class_flags>>p.ttl>>p.field_80>>threshold>>material_base;
        if(!in)throw std::invalid_argument("probe row incomplete");
        p.field_84=std::uint32_t(p.ttl);p.item_main=1;p.owner_slot=-1;
        if(in>>p.owner_slot){
            if(p.owner_slot!=0&&p.owner_slot!=-1)throw std::invalid_argument("probe owner must be slot zero or null");
        }
        if(p.owner_slot==0){auto owner=actor;owner.position[0]=0x7000000;tables.groups[3]={owner};}
        std::vector<dfre::AiItemDefinition> defs(2);defs[1].field_84=0x6000000;
        dfre::MissionTickState state;dfre::init_mission_tick(state,{},std::move(tables),dfre::AiItemDefinitionTable(defs),{},123);
        state.globals.mission_67f528=1;state.world.world_center={0x4000000,0x4000000,0};state.extra_for({std::uint8_t(group),0}).field_74=100;
        auto image=dfre::snapshot_mission_actor(state,{std::uint8_t(group),0});for(unsigned axis=0;axis<3;++axis)put(image,0x30+axis*4,angles[axis]);
        dfre::apply_mission_actor_image(state,{std::uint8_t(group),0},image);
        dfre::MissionProjectileLiveResources resources;resources.damage_thresholds={0,threshold};
        dfre::MissionProjectileMaterialBinding materials;materials.model=17;
        for(std::size_t i=0;i<model.lods[0].materials.size();++i){materials.identities.push_back(material_base+std::uint32_t(i)*76);materials.flags.push_back(model.lods[0].materials[i].flags);}
        resources.materials.push_back(materials);dfre::ProjectileDamageState damage;
        std::vector<std::pair<std::uint32_t,std::uint32_t>> events;
        dfre::MissionProjectileLiveBindings b;b.globals=[](const auto&){return dfre::MissionProjectileLiveGlobals{};};
        b.terrain=[](auto&,const auto&,const auto&,auto){return 1;};
        b.update=[&](auto&,const auto&,auto callback,auto message){events.emplace_back(callback,message);};
        b.impact=[&](auto&,auto&,const auto& effect){events.emplace_back(std::uint32_t(effect.site),effect.argument);};
        b.move=[&](auto&,auto&,const auto&){events.emplace_back(0x463350,0);};
        b.world_edge=[&](auto&,auto&,const auto&){events.emplace_back(0x43e7d0,0x41);};
        auto query=dfre::mission_projectile_live_query(state,geometry,animations,std::move(resources),damage,b);
        std::vector<dfre::ProjectileEvent> emitted;dfre::tick_projectile(p,{0x4000000,0x4000000,0},query,emitted);
        std::cout<<p.position.x<<' '<<p.position.y<<' '<<p.position.z<<' '<<p.velocity.x<<' '<<p.velocity.y<<' '<<p.velocity.z<<' '<<p.ttl<<' '<<p.item_main<<' '<<state.extra_for({std::uint8_t(group),0}).field_74<<' '<<damage.last_kind<<' '<<damage.last_damage<<' '<<state.extra_for({std::uint8_t(group),0}).field_f8<<' '<<events.size();
        for(const auto& e:events)std::cout<<' '<<e.first<<' '<<e.second;std::cout<<'\n';
    }
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
