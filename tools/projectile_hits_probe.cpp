#include "dfre/projectile_hits.hpp"
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <map>
#include <fstream>

using namespace dfre;
namespace {
std::uint32_t next(std::istream& in) {std::uint64_t n;if(!(in>>n))throw std::runtime_error("Short probe row");return std::uint32_t(n);}
std::int32_t integer(std::istream& in) {return static_cast<std::int32_t>(next(in));}
void put(AiActorImage& a,unsigned at,std::uint32_t x) {for(unsigned i=0;i<4;++i)a[at+i]=std::uint8_t(x>>(8*i));}
std::string hex(const AiActorImage& a) {std::ostringstream out;out<<std::hex<<std::setfill('0');for(auto n:a)out<<std::setw(2)<<unsigned(n);return out.str();}
AiActorImage readimage(std::istream& in) {std::string raw;in>>raw;if(raw.size()!=0x228)throw std::runtime_error("Bad image size");AiActorImage a{};for(unsigned i=0;i<a.size();++i)a[i]=std::uint8_t(std::stoul(raw.substr(i*2,2),nullptr,16));return a;}
WorldHitQuery query(std::istream& in) {
    WorldHitQuery q;for(auto& n:q.origin)n=integer(in);for(auto& n:q.end)n=integer(in);
    for(auto& n:q.direction)n=integer(in);q.length=integer(in);for(auto& n:q.bounds)n=integer(in);q.skip[0]=next(in);return q;
}
void result(std::int32_t code,const ProjectileHitResult& r) {std::cout<<code<<' '<<r.hit<<' '<<std::uint32_t(r.distance)<<' '<<r.material<<' '<<r.record<<' '<<r.mesh<<' '<<r.face;}
ProjectileEntity projectile(std::istream& in) {
    ProjectileEntity p;p.field_7c=next(in);p.field_80=next(in);p.field_84=next(in);p.ttl=integer(in);return p;
}
struct ProbeMesh {
    WorldHitMesh geometry;
    std::vector<ModelFace> faces;
    std::vector<ModelVertex> vertices;
    std::vector<ModelNormal> normals;
    std::vector<ModelMaterial> materials;
    std::vector<CollisionFaceBox> boxes;
    std::vector<std::uint32_t> material_ids;
    void read(std::istream& in) {
        geometry.state_flags=std::uint8_t(next(in));for(auto& n:geometry.matrix)n=integer(in);
        auto count=next(in);
        for(unsigned index=0;index<count;++index) {
            CollisionFaceBox box;box.plane_constant=integer(in);for(auto& n:box.bounds)n=integer(in);boxes.push_back(box);
            ModelNormal normal;for(auto& n:normal.direction)n=std::int16_t(next(in));normal.flags=std::uint16_t(next(in));normals.push_back(normal);
            ModelMaterial material;material.flags=next(in);materials.push_back(material);material_ids.push_back(next(in));
            ModelFace face{};face.material=index;face.face_normal=std::uint16_t(index);
            for(unsigned corner=0;corner<3;++corner) {
                ModelVertex vertex{};for(auto& n:vertex.position)n=std::int16_t(next(in));
                face.vertices[corner]=std::uint32_t(vertices.size());vertices.push_back(vertex);
            }
            faces.push_back(face);
        }
    }
    ProjectileHitMesh bind() {
        geometry.faces=&faces;geometry.vertices=&vertices;geometry.normals=&normals;geometry.materials=&materials;geometry.face_boxes=&boxes;
        return {geometry,material_ids};
    }
};
struct ProbeModel {std::int32_t radius=0;bool present=false;std::vector<ProbeMesh> meshes;std::vector<ProjectileHitMesh> views;};
}
int main(int argc,char** argv) {
    if(argc!=2&&argc!=3)return 2;
    try {
        std::optional<CharacterModel> character;
        if(argc==3) {
            std::ifstream file(argv[2],std::ios::binary);if(!file)throw std::runtime_error("Cannot open character");
            std::vector<std::uint8_t> data{std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};
            character=prepare_character_runtime(decode_character(data));
        }
        std::string line;while(std::getline(std::cin,line)) {
            if(line.empty())continue;std::istringstream in(line);std::string mode=argv[1];
            if(mode=="amount") {
                const auto threshold=next(in);const auto p=projectile(in);std::cout<<projectile_damage_amount(threshold,p);
            } else if(mode=="water") {
                auto water=integer(in);auto q=query(in);ProjectileHitResult r;r.distance=integer(in);
                auto code=projectile_water_hit(water,q,r);std::cout<<code<<' '<<std::uint32_t(r.distance);
            } else if(mode=="damage") {
                auto a=readimage(in),owner=readimage(in);auto p=projectile(in);
                p.velocity={integer(in),integer(in),integer(in)};p.class_flags=next(in);
                ProjectileDamageDefinition d{next(in),next(in),next(in)},second{next(in),next(in),next(in)};
                ProjectileDamageState state;state.host=next(in);state.rules_67f549=std::uint8_t(next(in));
                state.local_player=next(in);state.water=integer(in);state.last_kind=next(in);state.last_damage=next(in);
                auto ownerid=next(in),kind=next(in),mutation=next(in);std::uint8_t person=std::uint8_t(next(in));
                std::ostringstream trace;
                auto snap=[&](const std::string& tag) {trace<<tag<<':'<<hex(a)<<':'<<state.last_kind<<':'<<state.last_damage<<':'<<unsigned(person)<<';';};
                ProjectileDamageBindings b;
                b.definition=[&](auto id){return id==0x2203000?d:second;};
                b.owner=[&](auto)->const AiActorImage& {return owner;};
                b.person_alert=[&](auto)->std::uint8_t& {return person;};
                b.alert=[&](auto slot,auto direction) {snap("a,"+std::to_string(slot)+","+std::to_string(direction));if(mutation&1)put(a,4,0x2203100);};
                b.update=[&](auto callback,auto,auto message){snap("u,"+std::to_string(callback)+","+std::to_string(message));if(mutation&2)put(a,0x74,0x55667788);};
                b.local_hit=[&](){snap("l");};
                auto code=projectile_apply_damage(p,ownerid,{&a,0x2201000},kind,state,b);
                std::cout<<code<<' '<<hex(a)<<' '<<state.last_kind<<' '<<state.last_damage<<' '<<unsigned(person)<<' '<<(trace.str().empty()?"-":trace.str());
            } else if(mode=="impact") {
                auto a=readimage(in),owner=readimage(in);auto p=projectile(in);p.velocity={65536,0,0};p.class_flags=next(in);
                ProjectileDamageDefinition definition{next(in),next(in),next(in)};auto material=next(in),type=next(in),kind=next(in);
                ProjectileDamageState state;state.host=next(in);state.rules_67f549=std::uint8_t(next(in));
                state.local_player=next(in);state.water=integer(in);state.last_kind=next(in);state.last_damage=next(in);
                auto ownerid=next(in),mutation=next(in);std::uint8_t person=std::uint8_t(next(in));
                auto identity=type==2?0x7dde80u:type==3?0x7cca80u:0x867e80u;
                std::ostringstream trace;auto snap=[&](const std::string& tag) {
                    trace<<tag<<':'<<hex(a)<<':'<<state.last_kind<<':'<<state.last_damage<<':'<<unsigned(person)<<':'<<p.class_flags<<':'<<std::uint32_t(p.ttl)<<';';
                };
                ProjectileImpactBindings b;b.damage.definition=[&](auto){return definition;};
                b.damage.owner=[&](auto)->const AiActorImage& {return owner;};b.damage.person_alert=[&](auto)->std::uint8_t& {return person;};
                b.damage.alert=[&](auto slot,auto direction){snap("a,"+std::to_string(slot)+","+std::to_string(direction));};
                b.damage.update=[&](auto callback,auto,auto message){snap("u,"+std::to_string(callback)+","+std::to_string(message));if(mutation&2)put(a,0x74,0x55667788);};
                b.damage.local_hit=[&](){snap("l");};b.material_flags=[&](auto){return material;};
                b.effect=[&](const ProjectileImpactEffect& e) {
                    snap("e,"+std::to_string(std::uint32_t(e.site))+","+std::to_string(e.argument)+","+std::to_string(e.owner)+","+
                        std::to_string(std::uint32_t(e.position.x))+","+std::to_string(std::uint32_t(e.position.y))+","+std::to_string(std::uint32_t(e.position.z)));
                    if((mutation&1)&&e.site==ProjectileImpactSite::visual)p.class_flags^=0x20;
                };
                ProjectileHitResult hit;hit.mesh=kind;hit.material=0x2207000;
                projectile_impact_effects(p,ownerid,std::uint8_t(type),{0x1004000,0x2000000,0x640000},{&a,identity},hit,state,b);
                std::cout<<hex(a)<<' '<<state.last_kind<<' '<<state.last_damage<<' '<<unsigned(person)<<' '<<p.class_flags<<' '<<(trace.str().empty()?"-":trace.str());
            } else if(mode=="bones") {
                auto q=query(in);ProjectileHitResult r;r.material=next(in);r.record=next(in);r.mesh=next(in);r.face=next(in);
                const auto count=next(in);std::vector<AiActorImage> images;std::vector<ProjectileBoneState> states;
                for(unsigned i=0;i<count;++i) {
                    images.push_back(readimage(in));ProjectileBoneState state;state.graphic_present=next(in)!=0;
                    auto n=next(in);while(n--)state.bones.push_back({{integer(in),integer(in),integer(in)},integer(in)});states.push_back(std::move(state));
                }
                std::vector<ProjectileHitActor> actors;for(unsigned i=0;i<count;++i)actors.push_back({&images[i],0x7cca80+i*0x114});
                std::vector<unsigned> calls;auto code=projectile_bone_hit(actors,q,r,[&](auto actor){auto i=(actor.identity-0x7cca80)/0x114;calls.push_back(i);return states[i];});
                result(code,r);std::cout<<' '<<calls.size();for(auto n:calls)std::cout<<' '<<n;
            } else if(mode=="spheres") {
                if(!character)throw std::runtime_error("Character path required");
                auto a=readimage(in);auto animation=next(in),frame=next(in);
                const auto pose=character_frame_pose(character->animations.at(animation).frames.at(frame));
                const auto state=projectile_character_spheres(*character,pose,{&a,0x2201000});
                std::cout<<state.graphic_present<<' '<<state.bones.size();
                for(const auto& bone:state.bones) {for(auto n:bone.center)std::cout<<' '<<std::uint32_t(n);std::cout<<' '<<std::uint32_t(bone.radius);}
            } else if(mode=="model") {
                auto q=query(in);ProjectileHitResult r;r.material=next(in);r.record=next(in);r.mesh=next(in);r.face=next(in);
                const auto count=next(in);std::vector<AiActorImage> images;std::map<std::uint32_t,ProbeModel> models;
                for(unsigned i=0;i<count;++i) {
                    images.push_back(readimage(in));auto id=next(in);auto& model=models[id];model.radius=integer(in);model.present=next(in)!=0;
                    auto n=next(in);model.meshes.resize(n);for(auto& mesh:model.meshes)mesh.read(in);
                    for(auto& mesh:model.meshes)model.views.push_back(mesh.bind());
                }
                std::vector<ProjectileHitActor> actors;for(unsigned i=0;i<count;++i)actors.push_back({&images[i],0x7cca80+i*0x114});
                std::ostringstream trace;ProjectileModelBindings b;
                b.radius=[&](auto model){trace<<"r,"<<model<<';';return models.at(model).radius;};
                b.state=[&](auto model,auto)->const std::vector<ProjectileHitMesh>* {trace<<"s,"<<model<<';';auto& m=models.at(model);return m.present?&m.views:nullptr;};
                b.divide_error=[&](auto a,auto b){trace<<"d,"<<a<<','<<b<<';';};
                auto code=projectile_model_hit(actors,q,r,b);result(code,r);std::cout<<' '<<(trace.str().empty()?"-":trace.str());
            } else if(mode=="muzzle") {
                auto a=readimage(in);ProjectileMuzzleWeapon weapon{next(in),next(in)};Matrix4 slot,attachment;
                for(auto& n:slot.m)n=integer(in);for(auto& n:attachment.m)n=integer(in);
                Fixed3 hardpoint{integer(in),integer(in),integer(in)};MuzzleResult captured;unsigned calls=0;
                ProjectileMuzzleBindings b;b.attachment=attachment;b.weapon=[&](auto){return weapon;};
                b.slot_seven=[&](auto,auto)->Matrix4& {++calls;return slot;};b.hardpoint=[&](auto,auto){return hardpoint;};
                b.shot=[&](auto,const auto& shot){++calls;captured=shot;};
                auto code=projectile_animated_muzzle({&a,0x2201000},b);
                std::cout<<code<<' '<<hex(a)<<' '<<calls;for(auto n:slot.m)std::cout<<' '<<std::uint32_t(n);
                if(code)std::cout<<' '<<std::uint32_t(captured.origin.x)<<' '<<std::uint32_t(captured.origin.y)<<' '<<std::uint32_t(captured.origin.z)<<' '<<captured.aim.yaw<<' '<<captured.aim.pitch<<' '<<captured.aim.roll;
            } else throw std::runtime_error("Unknown probe mode");
            std::cout<<'\n';
        }
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
