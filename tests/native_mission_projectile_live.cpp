#include "dfre/mission_projectile_live.hpp"
#include <stdexcept>
#include <iostream>
namespace {
using namespace dfre;
void check(bool v,const char* text){if(!v)throw std::runtime_error(text);}
template<class B> void put(B& b,std::size_t at,std::uint32_t v){for(unsigned i=0;i<4;++i)b[at+i]=std::uint8_t(v>>(i*8));}
Model model(){
    Model m;m.version=5;m.render_types[0]=0x676e7263;m.lods.resize(1);auto& lod=m.lods[0];
    lod.vertices={{{-256,-256,0},0},{{256,-256,0},0},{{0,256,0},0}};lod.normals={{{0,0,16384},1}};
    ModelFace face{};face.vertices={0,1,2};lod.faces={face};lod.materials.resize(1);
    ModelMesh mesh{};mesh.face_count=1;mesh.vertex_count=3;mesh.normal_count=1;mesh.raw.resize(112);
    for(auto at:{0x40u,0x48u})put(mesh.raw,at,-65536);
    for(auto at:{0x44u,0x4cu})put(mesh.raw,at,65536);
    lod.meshes={mesh};return m;
}
MissionGeometry geometry(Model& m){MissionGeometryModel b;b.handle=17;b.model=&m;b.bounds={0,131072};b.faces={{0,{-65536,65536,-65536,65536,0,0}}};return MissionGeometry({b});}
void init(MissionTickState& state,bool human=false){
    AiActorTables tables;tables.groups[0].resize(1);auto& actor=tables.groups[0][0];actor.active=true;actor.definition_index=1;actor.field_10=17;actor.field_110=0x1234;actor.field_70=0;
    std::vector<AiPersonState> people;
    if(human){actor.person_slot=0;people.resize(1);people[0].owner=actor_handle(0,0);}
    std::vector<AiItemDefinition> defs(2);defs[1].field_28=human?3:0;defs[1].field_84=0x1234;
    init_mission_tick(state,{},std::move(tables),AiItemDefinitionTable(defs),std::move(people),123);
    state.globals.mission_67f528=1;state.world.world_center={0x4000000,0x4000000,0};state.extra_for({0,0}).field_74=100;
}
ProjectileEntity projectile(){ProjectileEntity p;p.position={16384,0,65536};p.velocity={0,0,-131072};p.ttl=10;p.field_84=10;p.field_80=3;p.item_main=1;p.owner_slot=-1;return p;}
MissionProjectileLiveResources resources(){MissionProjectileLiveResources r;r.damage_thresholds={0,0};r.materials={{17,{901},{0x10000}}};return r;}
void test(bool human,bool fail){
    MissionTickState state;init(state,human);auto m=model();auto shape=geometry(m);
    MissionAnimations animations(std::vector<MissionAnimationBinding>{});ProjectileDamageState damage;
    MissionProjectileLiveGlobals globals;MissionProjectileLiveBindings b;unsigned alerts=0,updates=0,effects=0,moves=0;
    b.globals=[&](const auto&){return globals;};
    b.terrain=[](auto&,const auto&,const auto&,auto){return 1;};
    b.alert=[&](auto& live,auto slot,auto){check(slot==0&&live.people[0].field_9c==2,"alert did not see canonical person state");++alerts;live.extra_for({0,0}).field_74=88;};
    b.update=[&](auto& live,const auto& ref,auto callback,auto message){
        check(ref.group==0&&ref.index==0&&callback==0x1234&&message==1,"wrong selected actor callback");
        check(live.extra_for(ref).field_74==(human?80:92),"damage did not flush or alert writes were lost");
        check(damage.last_damage==8&&damage.last_kind==0,"damage cells not current at update");
        ++updates;live.extra_for(ref).field_74=66;live.tables.groups[0][0].position[0]=333;
        if(fail)throw std::runtime_error("update stopped");
    };
    b.impact=[&](auto& live,auto& p,const auto&){check(live.extra_for({0,0}).field_74==66,"impact lost update writes");check(p.ttl==10&&p.item_main==1,"impact ran after projectile removal");++effects;};
    b.move=[&](auto&,auto& p,const auto& event){check(p.ttl==0&&p.item_main==0&&event.position.z==16384,"move source order changed");++moves;};
    auto q=mission_projectile_live_query(state,shape,animations,resources(),damage,b);
    auto p=projectile();std::vector<ProjectileEvent> events;bool stopped=false;
    try{tick_projectile(p,{0x4000000,0x4000000,0},q,events);}catch(const std::runtime_error& e){stopped=std::string(e.what())=="update stopped";if(!stopped)throw;}
    check(stopped==fail&&updates==1&&alerts==unsigned(human),"live callback counts differ");
    check(state.extra_for({0,0}).field_74==66&&state.tables.groups[0][0].position[0]==333,"canonical callback writes were overwritten");
    if(fail)check(p.ttl==10&&p.item_main==1&&effects==0&&moves==0,"exception advanced projectile tail");
    else check(effects==3&&moves==1&&p.ttl==-1,"impact/move dispatch differs");
}
void missing(){
    MissionTickState state;init(state);auto m=model();auto shape=geometry(m);MissionAnimations a(std::vector<MissionAnimationBinding>{});ProjectileDamageState d;
    MissionProjectileLiveBindings b;b.globals=[](const auto&){return MissionProjectileLiveGlobals{};};b.terrain=[](auto&,const auto&,const auto&,auto){return 1;};
    auto q=mission_projectile_live_query(state,shape,a,resources(),d,b);auto p=projectile();std::vector<ProjectileEvent> events;bool rejected=false;
    try{tick_projectile(p,{0x4000000,0x4000000,0},q,events);}catch(const MissionTickError&){rejected=true;}
    check(rejected&&state.extra_for({0,0}).field_74==92&&p.ttl==10,"missing update lost reached damage or advanced tail");
    auto r=resources();r.materials[0].identities={0};rejected=false;
    try{mission_projectile_live_query(state,shape,a,r,d,b);}catch(const MissionTickError&){rejected=true;}
    check(rejected,"missing material identity was accepted");
}
void effect_mutation(bool fail){
    MissionTickState state;init(state);auto m=model();auto shape=geometry(m);
    MissionAnimations a(std::vector<MissionAnimationBinding>{});ProjectileDamageState d;
    auto r=resources();r.materials[0].flags[0]=0x40000;
    MissionProjectileLiveBindings b;b.globals=[](const auto&){return MissionProjectileLiveGlobals{};};
    b.terrain=[](auto&,const auto&,const auto&,auto){return 1;};b.update=[](auto&,const auto&,auto,auto){};
    unsigned effects=0;b.impact=[&](auto& live,auto& p,const auto& e){
        ++effects;
        if(effects==1){check(e.site==ProjectileImpactSite::visual&&e.argument==13,"wrong first impact effect");
            p.class_flags|=0x20;p.position.x=700;live.extra_for({0,0}).field_74=55;
            if(fail)throw std::runtime_error("impact stopped");
        }else if(effects==2)check(e.site==ProjectileImpactSite::sound&&e.argument==0x4e,"impact branch did not read current flags");
    };
    b.move=[](auto&,auto&,const auto& e){check(e.from.x==700,"move did not read current projectile position");};
    auto q=mission_projectile_live_query(state,shape,a,r,d,b);auto p=projectile();std::vector<ProjectileEvent> events;
    bool stopped=false;try{tick_projectile(p,state.world.world_center,q,events);}catch(const std::runtime_error& e){stopped=std::string(e.what())=="impact stopped";if(!stopped)throw;}
    check(stopped==fail&&state.extra_for({0,0}).field_74==55,"impact writes lost after return or error");
    check(effects==(fail?1u:3u)&&p.ttl==(fail?10:-1),"impact exception advanced later stages");
}
void alert_error(){
    MissionTickState state;init(state,true);auto m=model();auto shape=geometry(m);
    MissionAnimations a(std::vector<MissionAnimationBinding>{});ProjectileDamageState d;
    MissionProjectileLiveBindings b;b.globals=[](const auto&){return MissionProjectileLiveGlobals{};};
    b.terrain=[](auto&,const auto&,const auto&,auto){return 1;};
    b.alert=[](auto& live,auto,auto){live.extra_for({0,0}).field_74=77;throw std::runtime_error("alert stopped");};
    auto q=mission_projectile_live_query(state,shape,a,resources(),d,b);auto p=projectile();std::vector<ProjectileEvent> events;
    bool stopped=false;try{tick_projectile(p,state.world.world_center,q,events);}catch(const std::runtime_error& e){stopped=std::string(e.what())=="alert stopped";if(!stopped)throw;}
    check(stopped&&state.extra_for({0,0}).field_74==77&&state.people[0].field_9c==2&&p.ttl==10,"alert exception lost writes or reached damage");
}

void owner_contract(bool bypass_team){
    MissionTickState initial;init(initial,true);
    AiActorTables tables;tables.groups[0]=initial.tables.groups[0];
    tables.groups[0][0].field_6c=bypass_team?0x80:0;
    auto owner=tables.groups[0][0];owner.person_slot=1;owner.field_6c=0;
    tables.groups[3]={owner};std::vector<AiPersonState> people(2);
    std::vector<AiItemDefinition> defs(2);defs[1].field_28=3;defs[1].field_84=0x1234;
    MissionTickState state;init_mission_tick(state,{},std::move(tables),AiItemDefinitionTable(defs),std::move(people),123);
    state.globals.mission_67f528=1;state.world.world_center={0x4000000,0x4000000,0};
    state.extra_for({0,0}).field_74=100;
    auto m=model();auto shape=geometry(m);MissionAnimations a(std::vector<MissionAnimationBinding>{});ProjectileDamageState d;
    MissionProjectileLiveBindings b;b.globals=[](const auto&){MissionProjectileLiveGlobals g;g.rules_67f549=0x40;return g;};
    b.terrain=[](auto&,const auto&,const auto&,auto){return 1;};unsigned updates=0;
    b.update=[&](auto& live,const auto& ref,auto,auto){++updates;check(live.extra_for(ref).field_f8==actor_handle(3,0),"damage owner did not map slot zero");};
    b.alert=[](auto&,auto,auto){};b.impact=[](auto&,auto&,const auto&){};b.move=[](auto&,auto&,const auto&){};
    auto q=mission_projectile_live_query(state,shape,a,resources(),d,b);auto p=projectile();p.owner_slot=0;std::vector<ProjectileEvent> events;
    tick_projectile(p,state.world.world_center,q,events);
    check(updates==unsigned(bypass_team)&&state.extra_for({0,0}).field_74==(bypass_team?92u:100u),"owner slot zero did not control team damage");
    // The owner occupies the ray and has no CHR pose. Exclusion prevents that
    // provider from being reached. Null ownership reaches it explicitly.
    p=projectile();p.owner_slot=-1;events.clear();bool missing_pose=false;
    try{tick_projectile(p,state.world.world_center,q,events);}catch(const MissionTickError& e){missing_pose=std::string(e.what())=="Projectile character needs its current CHR pose";}
    check(missing_pose,"null owner silently excluded group 3 slot zero");
    for(auto invalid:{-2,1}){p=projectile();p.owner_slot=invalid;events.clear();bool rejected=false;
        try{tick_projectile(p,state.world.world_center,q,events);}catch(const MissionTickError&){rejected=true;}
        check(rejected,"invalid owner slot was accepted");}
}

void self_damage(){
    CharacterModel character;character.runtime_prepared=true;character.bone_map={0,1,2};character.raw.resize(180);character.bones.resize(3);
    for(unsigned i=0;i<3;++i){auto& bone=character.bones[i];bone.offset=i*60;bone.raw.resize(60);put(character.raw,i*60+56,0xffffffff);put(bone.raw,0x34,0x20000);}
    MissionAnimations animations({{kCharacterInstanceBase,&character,{}}});
    AiActorTables tables;tables.groups[3].resize(1);auto& actor=tables.groups[3][0];actor.active=true;actor.definition_index=1;
    actor.person_slot=0;actor.field_10=17;actor.field_18=kCharacterInstanceBase;actor.field_110=0x1234;actor.field_6c=1;
    std::vector<AiItemDefinition> defs(2);defs[1].field_28=3;defs[1].field_84=0x1234;
    MissionTickConfig config;config.local_actor=0;MissionTickState state;
    init_mission_tick(state,config,std::move(tables),AiItemDefinitionTable(defs),std::vector<AiPersonState>(1),123);
    state.globals.mission_67f528=1;state.world.world_center={0x4000000,0x4000000,0};state.extra_for({3,0}).field_74=100;
    auto m=model();auto shape=geometry(m);auto r=resources();r.characters={{kCharacterInstanceBase,&character}};ProjectileDamageState d;
    MissionProjectileLiveBindings b;b.globals=[](const auto&){MissionProjectileLiveGlobals g;g.rules_67f549=0x40;return g;};
    b.terrain=[](auto&,const auto&,const auto&,auto){return 1;};unsigned updates=0,local=0;
    b.update=[&](auto& live,const auto& ref,auto,auto){++updates;check(ref.group==3&&live.extra_for(ref).field_f8==actor_handle(3,0),"self damage lost canonical owner");};
    b.local_hit=[&](auto&){++local;};b.impact=[](auto&,auto&,const auto&){};b.move=[](auto&,auto&,const auto&){};
    auto q=mission_projectile_live_query(state,shape,animations,r,d,b);auto p=projectile();p.owner_slot=0;p.class_flags=0x20;
    std::vector<ProjectileEvent> events;tick_projectile(p,state.world.world_center,q,events);
    check(updates==1&&local==1&&state.extra_for({3,0}).field_74==92,"same-team rule blocked self damage");
}

void live_terrain(){
    TerrainGeometry low(AssetBytes(1024*1024,0),AssetBytes(1024*1024,0),std::vector<std::uint32_t>(65536));
    TerrainGeometry high(AssetBytes(1024*1024,100),AssetBytes(1024*1024,0),std::vector<std::uint32_t>(65536));
    MissionTickState state;auto p=projectile();const Fixed3 step{0,0,-65536};
    bool missing=false;try{mission_projectile_terrain_query(state,p,step,131072);}catch(const MissionTickError&){missing=true;}
    check(missing,"missing terrain geometry was accepted");
    state.world.terrain=&low;check(mission_projectile_terrain_query(state,p,step,131072)==-69632,"terrain hit distance mapping changed");
    state.world.terrain=&high;check(mission_projectile_terrain_query(state,p,step,131072)==61440,"terrain query did not read current geometry or kept only negative hits");
    p.position.z=0x400000;check(mission_projectile_terrain_query(state,p,{65536,0,0},65536)==1,"terrain clear lambda did not map to code one");
    MissionGeometry shape(std::vector<MissionGeometryModel>{});MissionAnimations animations(std::vector<MissionAnimationBinding>{});ProjectileDamageState damage;
    init_mission_tick(state,{},AiActorTables{},AiItemDefinitionTable{}, {},123);state.world.terrain=&low;state.world.world_center={0x4000000,0x4000000,0};
    MissionProjectileLiveBindings b;b.globals=[](const auto&){return MissionProjectileLiveGlobals{};};
    unsigned impacts=0;b.impact=[&](auto&,auto&,const auto& e){check(e.site==ProjectileImpactSite::terrain_mark,"default terrain used wrong effect");++impacts;};b.move=[](auto&,auto&,const auto&){};
    auto q=mission_projectile_live_query(state,shape,animations,{},damage,b);p=projectile();std::vector<ProjectileEvent> events;
    state.world.terrain=&high;std::int32_t ignored=0;
    check(q.terrain(p,step,131072,ignored)==61440,"live query captured an old terrain binding");
    state.world.terrain=&low;
    tick_projectile(p,state.world.world_center,q,events);
    check(impacts==1&&p.position.z==12288&&p.ttl==-1,"live default terrain did not join the projectile pass");
}

void group_order(){
    MissionTickState state;init(state);auto actor=state.tables.groups[0][0];
    AiActorTables tables;tables.groups[0]={actor};tables.groups[1]={actor};
    std::vector<AiItemDefinition> defs(2);defs[1].field_84=0x1234;
    init_mission_tick(state,{},std::move(tables),AiItemDefinitionTable(defs),{},123);
    state.globals.mission_67f528=1;state.world.world_center={0x4000000,0x4000000,0};
    state.extra_for({0,0}).field_74=100;state.extra_for({1,0}).field_74=100;
    auto m=model();auto shape=geometry(m);MissionAnimations a(std::vector<MissionAnimationBinding>{});ProjectileDamageState d;
    MissionProjectileLiveBindings b;b.globals=[](const auto&){return MissionProjectileLiveGlobals{};};
    b.terrain=[](auto&,const auto&,const auto&,auto){return 1;};
    b.update=[](auto&,const auto& ref,auto,auto){check(ref.group==1,"equal-distance query chose7DDE80 before867E80");};
    b.impact=[](auto&,auto&,const auto&){};b.move=[](auto&,auto&,const auto&){};
    auto q=mission_projectile_live_query(state,shape,a,resources(),d,b);auto p=projectile();std::vector<ProjectileEvent> events;
    tick_projectile(p,state.world.world_center,q,events);
    check(state.extra_for({1,0}).field_74==92&&state.extra_for({0,0}).field_74==100,"physical group mapping changed selected damage");
}
}
int main(){try{test(false,false);test(false,true);test(true,false);test(true,true);missing();group_order();owner_contract(false);owner_contract(true);self_damage();live_terrain();effect_mutation(false);effect_mutation(true);alert_error();std::cout<<"mission projectile live checks passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
