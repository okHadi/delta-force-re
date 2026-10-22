#include "dfre/projectile_hits.hpp"
#include <stdexcept>

using namespace dfre;
namespace {
void check(bool good) {if(!good)throw std::runtime_error("Projectile hit test failed");}
void put(AiActorImage& a,unsigned at,std::uint32_t x) {for(unsigned i=0;i<4;++i)a[at+i]=std::uint8_t(x>>(8*i));}
std::uint32_t get(const AiActorImage& a,unsigned at) {return std::uint32_t(a[at])|std::uint32_t(a[at+1])<<8|std::uint32_t(a[at+2])<<16|std::uint32_t(a[at+3])<<24;}
void amount() {
    ProjectileEntity p;p.field_80=8;p.field_84=100;p.ttl=68;p.field_7c=4;
    check(projectile_damage_amount(8,p)==1);check(projectile_damage_amount(9,p)==0);
    p.field_7c=0;p.field_80=31;check(projectile_damage_amount(31,p)==0x80000000);
    p.field_80=0x80000000;p.field_7c=1;p.field_84=1;p.ttl=0;
    check(projectile_damage_amount(0x80000000,p)==1);check(projectile_damage_amount(0x80000001,p)==0);
}
void damage() {
    AiActorImage a{},owner{};put(a,4,1);put(a,0xc,3);put(a,0x110,4);put(a,0x70,0xffff0002);put(a,0x74,100);
    ProjectileEntity p;p.velocity.x=65536;p.field_80=3;
    ProjectileDamageState state;state.host=1;state.local_player=10;
    unsigned effects=0;std::uint8_t alert=0;ProjectileDamageBindings b;
    b.definition=[&](auto id){return ProjectileDamageDefinition{3,0,id==1?3u:4u};};
    b.person_alert=[&](auto person)->std::uint8_t& {check(person==3);return alert;};
    b.alert=[&](auto slot,auto direction) {check(slot==-1&&direction==2&&alert==2&&get(a,0x74)==100);++effects;put(a,4,2);};
    b.update=[&](auto,auto,auto){throw std::runtime_error("Threshold should block this hit");};
    b.local_hit=[&](){check(effects==1&&get(a,0x74)==100);++effects;};
    check(projectile_apply_damage(p,0,{&a,10},2,state,b)==1);check(effects==2&&alert==2);
    put(a,4,1);a[0x6c]=1;
    b.update=[&](auto callback,auto actor,auto message){
        check(callback==4&&actor.image==&a&&message==1&&a[0x74]==68&&a[0x6d]==0x84);
        check(state.last_damage==8&&state.last_kind==2&&get(a,0xf8)==20);++effects;a[0x75]=9;
    };
    b.local_hit=[&](){check(a[0x75]==9);++effects;};
    check(projectile_apply_damage(p,20,{&a,10},2,state,b)==1);check(effects==4);
    state.rules_67f549=0x40;put(owner,0x70,2);b.owner=[&](auto)->const AiActorImage& {return owner;};
    check(projectile_apply_damage(p,20,{&a,10},2,state,b)==1);check(effects==4);
    put(a,4,0);check(projectile_apply_damage(p,0,{&a,10},2,state,{})==0);
}
void water() {
    WorldHitQuery q;q.origin[2]=1000;q.end[2]=999;q.direction[2]=-65536;
    ProjectileHitResult r;r.distance=123;check(projectile_water_hit(0,q,r)==1&&r.distance==123);
    q.end[2]=-1;check(projectile_water_hit(0,q,r)==0&&r.distance==1000);
    q.origin[2]=512;check(projectile_water_hit(0,q,r)==0&&r.distance==0);
    q.origin[2]=513;q.direction[2]=0;
    try {projectile_water_hit(0,q,r);check(false);}catch(const std::invalid_argument&){}
}
void bones() {
    AiActorImage a{};put(a,0,1);put(a,0x24,0x10000);
    WorldHitQuery q;q.end={0x40000,0,0};q.direction={65536,0,0};q.length=0x40000;q.bounds={0,0x40000,0,0,0,0};
    ProjectileHitResult r;r.material=77;unsigned calls=0;
    auto provider=[&](auto){++calls;return ProjectileBoneState{true,{{{0x20000,0,0},100},{{0x20000,0,0},100},{{0x20000,0,0},100}}};};
    check(projectile_bone_hit({{&a,10}},q,r,provider)==0);check(r.mesh==0&&r.record==0&&r.material==77&&calls==1);
    q.skip[0]=10;check(projectile_bone_hit({{&a,10}},q,r,{})==1&&r.distance==q.length&&r.material==77);
    q.skip[0]=0;
    check(projectile_bone_hit({{&a,10}},q,r,[&](auto){return ProjectileBoneState{true,{{{-1,0,0},100},{{0x20000,0,0},100},{{0x20000,0,0},100}}};})==0);
    check(r.distance==-1&&r.mesh==0);
}
void muzzle() {
    AiActorImage a{};put(a,0x94,7);put(a,0x10,9);put(a,0x24,1000);put(a,0x28,2000);put(a,0x2c,3000);put(a,0x34,123);
    Matrix4 slot=matrix_identity();slot.m[3]=10;
    ProjectileMuzzleBindings b;b.attachment=matrix_identity();b.attachment.m[7]=20;
    unsigned calls=0;b.weapon=[&](auto index){check(index==7);return ProjectileMuzzleWeapon{8,2};};
    b.slot_seven=[&](auto graphic,auto actor)->Matrix4& {check(graphic==9&&actor.image==&a&&get(a,0x34)==123);++calls;return slot;};
    b.hardpoint=[&](auto model,auto index){check(model==8&&index==2&&calls==1);return Fixed3{1,2,3};};
    b.shot=[&](auto,const auto& result){check(result.origin.x==1011&&result.origin.y==2022&&result.origin.z==3003&&result.aim.pitch==123);++calls;};
    check(projectile_animated_muzzle({&a,1},b)&&calls==2&&slot.m[3]==1010&&slot.m[7]==2020);
    put(a,0x94,0);b.weapon=[](auto){return ProjectileMuzzleWeapon{};};check(!projectile_animated_muzzle({&a,1},b)&&calls==2);
}
void model() {
    AiActorImage a{};put(a,0,1);put(a,0x10,10);
    std::vector<ModelVertex> vertices{{{{0,-256,-256}},0},{{{0,256,-256}},0},{{{0,0,256}},0}};
    ModelFace f{};f.vertices={0,1,2};std::vector<ModelFace> faces{f,f};
    std::vector<ModelNormal> normals{{{16384,0,0},4}};
    std::vector<ModelMaterial> materials(1);
    std::vector<CollisionFaceBox> boxes(2);for(auto& box:boxes)box.bounds={0,0,-65536,65536,-65536,65536};
    WorldHitMesh geometry;geometry.matrix=model_identity_matrix();geometry.faces=&faces;geometry.vertices=&vertices;
    geometry.normals=&normals;geometry.materials=&materials;geometry.face_boxes=&boxes;
    std::vector<ProjectileHitMesh> meshes{{geometry,{77}}};
    ProjectileModelBindings b;b.radius=[](auto){return 0x10000;};b.state=[&](auto,auto){return &meshes;};
    WorldHitQuery q;q.origin={-32768,0,0};q.end={32768,0,0};q.direction={65536,0,0};q.length=65536;q.bounds={-32768,32768,0,0,0,0};
    ProjectileHitResult r;check(projectile_model_hit({{&a,1}},q,r,b)==0&&r.face==1&&r.distance==32768&&r.material==77);
    std::swap(q.origin,q.end);q.direction[0]=-65536;
    check(projectile_model_hit({{&a,1}},q,r,b)==0&&r.distance==32768);
    meshes[0].geometry.state_flags=2;check(projectile_model_hit({{&a,1}},q,r,b)==1);
    meshes[0].geometry.state_flags=0;materials[0].flags=0x1000000;check(projectile_model_hit({{&a,1}},q,r,b)==1);
}
void impacts() {
    ProjectileEntity p;p.field_84=10;p.ttl=10;p.class_flags=0x40;
    ProjectileDamageState state;ProjectileImpactBindings b;std::vector<ProjectileImpactEffect> effects;
    b.effect=[&](const auto& effect){effects.push_back(effect);};
    projectile_impact_effects(p,123,0,{1,2,3},{},{},state,b);
    check(effects.size()==5&&effects[0].site==ProjectileImpactSite::explosion&&effects[0].argument==3&&effects[0].owner==123);
    check(effects[3].site==ProjectileImpactSite::ground&&effects[4].argument==200);
    effects.clear();p.class_flags=1;
    projectile_impact_effects(p,123,0,{1,2,3},{},{},state,b);
    check(effects.size()==1&&effects[0].site==ProjectileImpactSite::terrain_mark);
    AiActorImage a{};put(a,4,1);
    b.damage.definition=[](auto){return ProjectileDamageDefinition{0,0,99};};
    b.material_flags=[](auto){return 0x40000;};effects.clear();p.class_flags=0;
    b.effect=[&](const auto& effect){effects.push_back(effect);if(effect.site==ProjectileImpactSite::visual)p.class_flags|=0x20;};
    ProjectileHitResult hit;hit.material=2;
    projectile_impact_effects(p,123,1,{1,2,3},{&a,1},hit,state,b);
    check(effects.size()==3&&effects[0].argument==0xd&&effects[1].argument==0x4e&&effects[2].argument==15);
}
}
int main() {try{amount();damage();water();bones();muzzle();model();impacts();}catch(...){return 1;}return 0;}
