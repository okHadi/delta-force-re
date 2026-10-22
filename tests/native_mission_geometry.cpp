#include "dfre/mission_geometry.hpp"
#include <iostream>
#include <stdexcept>
#include <utility>

namespace {
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class Bytes> void put(Bytes& bytes, std::size_t at, std::uint32_t value) {
    for (std::size_t i = 0; i < 4; ++i) bytes.at(at+i) = std::uint8_t(value >> (i*8));
}
dfre::Model model() {
    dfre::Model result;
    result.version = 5; result.render_types[0] = 0x676e7263;
    result.lods.resize(1);
    auto& lod = result.lods.front();
    lod.vertices = {{{-256,-256,256},0}, {{256,-256,256},0}, {{0,256,256},0}};
    lod.normals = {{{0,0,16384},1}};
    dfre::ModelFace face{};
    face.vertices = {0,1,2}; lod.faces = {face};
    lod.materials.resize(1);
    dfre::ModelMesh mesh;
    mesh.face_count = 1; mesh.vertex_count = 3; mesh.normal_count = 1;
    mesh.raw.resize(112);
    put(mesh.raw, 0x40, -65536); put(mesh.raw, 0x44, 65536);
    put(mesh.raw, 0x48, -65536); put(mesh.raw, 0x4c, 65536);
    lod.meshes = {mesh};
    return result;
}
dfre::MissionGeometryModel binding(const dfre::Model& model, std::uint32_t handle) {
    return {handle, &model, {131072,196608}, {{-65536, {-65536,65536,-65536,65536,65536,65536}}}, {}};
}
template<class Function> void rejects(Function function, const char* message) {
    bool rejected = false;
    try { function(); } catch (const std::invalid_argument&) { rejected = true; }
    check(rejected, message);
}
}
int main() {
    try {
        auto source = model();
        dfre::MissionGeometry geometry({binding(source,1),binding(source,2)});
        dfre::AiActorImage actor{}; put(actor,0x10,1);
        check(geometry.bounds(1).radius_64 == 131072 && geometry.bounds(1).height_60 == 196608,
            "model header fields changed");
        check(geometry.bounds(0).radius_64 == 0, "null bounds changed");
        auto built = geometry.ground_entities({{77,&actor}}, {0,0,65536});
        auto result = dfre::ground_entity_height(0,{0,0,65536},0,built.entities);
        check(result.height == 65536 && result.entity == &built.entities[0] && built.identities[0] == 77,
            "stored face plane did not produce the ground surface");
        check(built.storage[0].definition->pieces[0].volumes.size() == 1 &&
            built.storage[0].definition->pieces[0].volumes[0].accepted,
            "ground query did not test stored face geometry");
        check(!geometry.ground_entity(actor,{131073,0,65536}), "model radius gate changed");
        check(!geometry.ground_entity(dfre::AiActorImage{}, {0,0,0}), "null model was accepted");
        built = geometry.ground_entities({{77,&actor}}, {60000,60000,65536});
        result = dfre::ground_entity_height(0,{60000,60000,65536},0,built.entities);
        check(result.height == 0 && !result.entity, "face triangle test accepted a point outside the triangle");
        source.lods[0].materials[0].flags = 0x01000000;
        built = geometry.ground_entities({{77,&actor}}, {0,0,65536});
        check(built.storage[0].definition->pieces[0].volumes[0].skip,
            "material ground skip bit changed");
        source.lods[0].materials[0].flags = 0;
        source.lods[0].normals[0].direction[2] = 63;
        built = geometry.ground_entities({{77,&actor}}, {0,0,65536});
        check(!built.storage[0].definition->pieces[0].volumes[0].accepted,
            "normal threshold accepted a near-vertical face");
        source.lods[0].normals[0].direction[2] = 16384;
        put(actor,0x6c,8); put(actor,0x14,2);
        auto alternate = geometry.ground_entity(actor,{0,0,65536});
        check(alternate && !alternate->entity.definition && alternate->entity.alternate == alternate->definition.get(),
            "alternate model selection changed");
        auto moved = std::move(*alternate);
        check(moved.entity.alternate == moved.definition.get(), "moved ground view lost its owned definition");
        put(actor,0x14,0);
        auto fallback = geometry.ground_entity(actor,{0,0,65536});
        check(fallback && fallback->entity.definition == fallback->definition.get(), "null alternate fallback changed");
        source.render_types[0] = 0x74726565;
        rejects([&] { geometry.ground_entity(actor,{0,0,65536}); }, "special tag accepted missing matrix state");
        const dfre::MissionGeometryMatrixSource absent = [](const auto&,const auto&,const auto&) {
            dfre::MissionGeometryMatrices value; value.present = false; return value;
        };
        auto empty = geometry.ground_entity(actor,{0,0,65536},absent);
        check(empty && empty->definition->pieces.empty(), "null matrix callback produced geometry");
        const dfre::MissionGeometryMatrixSource hidden = [](const auto&,const auto&,const auto&) {
            return dfre::MissionGeometryMatrices{true,{dfre::model_identity_matrix()},{3}};
        };
        auto skipped = geometry.ground_entity(actor,{0,0,65536},hidden);
        check(skipped && skipped->definition->pieces[0].volumes.empty(), "instance skip flags changed");
        source.render_types[0] = 0x676e7263;
        put(actor,0,1);put(actor,0x10,1);put(actor,0x14,999);put(actor,0x6c,8);
        const dfre::AiActorImage inactive{};
        const auto return_records = [&] {
            auto records = geometry.world_hit_records({{10,&inactive},{20,&actor}});
            return records;
        };
        auto records = return_records();
        check(records.size() == 2 && records[0].active == 0 && records[1].identity == 20 &&
            records[1].radius == 196608 && records[1].meshes.empty() && bool(records[1].mesh_source),
            "world hit list changed slot order, primary selection, or radius");
        dfre::WorldHitQuery ray;
        ray.origin = {0,0,131072};ray.end = {0,0,0};ray.direction = {0,0,-65536};
        ray.length = 131072;ray.bounds = {0,0,0,0,0,131072};
        dfre::WorldModelHit hit;
        check(dfre::world_model_list_hit(records,ray,hit) == 0 && hit.hit && hit.record == 1 &&
            hit.mesh == 0 && hit.distance == 65536,
            "returned world records lost their face backing or changed surface hit");
        ray.skip[0] = 20;
        check(dfre::world_model_list_hit(records,ray,hit) == 1 && !hit.hit,
            "world hit identity skip changed");
        {
            auto counted_model = model();
            counted_model.render_types[0] = 0x74726565;
            dfre::MissionGeometry counted_geometry({binding(counted_model,1)});
            unsigned calls = 0;
            const dfre::MissionGeometryMatrixSource counted = [&](const auto&,const auto& image,const auto&) {
                ++calls;
                check(image[0x24] == 0 && image[0x28] == 0, "deferred source lost its actor snapshot");
                return dfre::MissionGeometryMatrices{true,{dfre::model_identity_matrix()},{0}};
            };
            const auto returned = [&] {
                dfre::AiActorImage local{};put(local,0,1);put(local,0x10,1);
                return counted_geometry.world_hit_record(30,local,counted);
            }();
            check(calls == 0, "world builder ran the matrix source before query gates");
            auto far = returned;far.position = {10000000,0,0};
            auto sphere = returned;sphere.position = {150000,150000,65536};
            auto disabled = returned;disabled.active = 0;
            auto clear_ray = ray;clear_ray.skip = {0,0};
            check(dfre::world_model_list_hit({disabled,far,sphere},clear_ray,hit) == 1 && !hit.hit && calls == 0,
                "inactive, bounds, or sphere rejection ran the matrix source");
            check(dfre::world_model_list_hit({disabled,far,sphere,returned},clear_ray,hit) == 0 &&
                hit.hit && hit.record == 3 && calls == 1,
                "near world record did not run its deferred source after the gates");
            clear_ray.skip[0] = 30;calls = 0;
            check(dfre::world_model_list_hit({returned},clear_ray,hit) == 1 && !hit.hit && calls == 0,
                "identity rejection ran the matrix source");
        }
        source.lods[0].raw_header.resize(184);
        source.render_types[0] = 0x74726565;
        const auto tree = dfre::mission_geometry_callback_matrices(binding(source,1),actor);
        check(tree.present && tree.matrices.size() == 1 && tree.flags[0] == 0,
            "tree callback slot changed");
        source.render_types[0] = 0x6e756c6c;
        check(!dfre::mission_geometry_callback_matrices(binding(source,1),actor).present,
            "null callback returned mesh matrices");
        source.render_types[0] = 0x6f726730;
        rejects([&] { dfre::mission_geometry_callback_matrices(binding(source,1),actor); },
            "org0 callback accepted missing scratch flags");
        const std::vector<std::uint8_t> prior_flags{0x9b};
        const auto character_identity = dfre::mission_geometry_callback_matrices(binding(source,1),actor,
            nullptr,nullptr,&prior_flags);
        check(character_identity.matrices[0] == dfre::model_identity_matrix() && character_identity.flags == prior_flags,
            "null-character callback changed its identity matrix or scratch flags");
        put(actor,0x18,1);
        rejects([&] { dfre::mission_geometry_callback_matrices(binding(source,1),actor,nullptr,nullptr,&prior_flags); },
            "org0 callback accepted a missing current pose");
        source.render_types[0] = 0x74697265;
        source.lods[0].hardpoints.resize(3);
        const std::vector<std::uint8_t> tire_prior{0x20,0x30,0x40,0x50};
        rejects([&] { dfre::mission_geometry_callback_matrices(binding(source,1),actor); },
            "tire callback accepted missing scratch flags");
        const auto tire = dfre::mission_geometry_callback_matrices(binding(source,1),actor,nullptr,nullptr,&tire_prior);
        check(tire.flags == std::vector<std::uint8_t>{0x20,1,0x40,0x50},
            "tire callback changed retained scratch flags");
        source.render_types[0] = 0x70616c6d;
        put(actor,0xe0,0x10);
        rejects([&] { dfre::mission_geometry_callback_matrices(binding(source,1),actor); },
            "palm callback accepted its missing stack flag");
        const auto palm = dfre::mission_geometry_callback_matrices(binding(source,1),actor,
            nullptr,nullptr,nullptr,std::uint8_t(0xa6));
        check(palm.flags == std::vector<std::uint8_t>{1,0xa6,0xa6,1,0xa6,1},
            "palm callback changed its copied stack flag");
        rejects([&] { geometry.bounds(3); }, "unknown model handle was accepted");
        std::cout << "mission geometry tests passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
