#include "dfre/mission_geometry.hpp"
#include "dfre/mission_tick.hpp"
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
    return {handle, &model, {131072,196608}, {{-65536, {-65536,65536,-65536,65536,65536,65536}}}, {},327680};
}
template<class Function> void rejects(Function function, const char* message) {
    bool rejected = false;
    try { function(); } catch (const std::invalid_argument&) { rejected = true; }
    check(rejected, message);
}
}
int main() {
    try {
        auto source=model();
        auto& lod=source.lods.front();
        lod.faces.push_back(lod.faces.front());
        lod.meshes.front().face_count=2;
        auto bind=binding(source,1);
        bind.faces.push_back(bind.faces.front());
        dfre::MissionGeometry geometry({bind});
        dfre::AiActorImage actor{};put(actor,0x10,1);
        std::vector<std::int32_t> heights;
        dfre::MissionTickState canonical;
        auto& current_ground = canonical.runtime_for({0,0}).ground_100;
        auto observe=[&](std::int32_t h){
            heights.push_back(h);
            if(h>=0 && h<=65536) current_ground=77;
        };
        auto built=geometry.ground_entity(actor,{0,0,65536},{},observe);
        check(heights==std::vector<std::int32_t>({65536,65536}),"surface publication order changed");
        check(built && built->entity.definition==built->definition.get(),"returned definition pointer is stale");
        auto moved=std::move(*built);
        check(moved.entity.definition==moved.definition.get(),"moved definition pointer is stale");
        check(moved.definition->pieces.at(0).volumes.size()==2,"returned surface storage is incomplete");
        std::vector<dfre::GroundEntity> view{moved.entity};
        check(dfre::ground_entity_height(0,{0,0,65536},0,view).height==65536,"returned surface view is not usable");
        heights.clear();
        lod.faces[1].vertices[0]=99;
        rejects([&]{geometry.ground_entity(actor,{0,0,65536},{},observe);},"invalid later geometry did not fail");
        check(heights==std::vector<std::int32_t>({65536}) && current_ground==77,
            "later failure lost earlier canonical ground publication");
        lod.faces[1].vertices[0]=0;
        heights.clear();
        bool failed=false;
        try { geometry.ground_entity(actor,{0,0,65536},{},[&](std::int32_t h){observe(h);throw std::runtime_error("observer");}); }
        catch(const std::runtime_error&){failed=true;}
        check(failed && heights.size()==1,"observer error did not stop at current surface");
        heights.clear();
        source.lods[0].materials[0].flags=0x01000000;
        geometry.ground_entity(actor,{0,0,65536},{},observe);
        check(heights.empty(),"skipped material published a surface");
        source.lods[0].materials[0].flags=0;
        // Keep publication from an earlier target when the next target matrix source fails.
        geometry.ground_entity(actor,{0,0,65536},{},observe);
        check(heights.size()==2,"first target did not publish");
        source.render_types[0]=0x74726565;
        bool matrix_failed=false;
        try { geometry.ground_entity(actor,{0,0,65536},[](const auto&,const auto&,const auto&)->dfre::MissionGeometryMatrices {throw std::runtime_error("matrix");},observe); }
        catch(const std::runtime_error&){matrix_failed=true;}
        check(matrix_failed && heights.size()==2,"later matrix failure changed prior publications");
        std::cout<<"mission geometry incremental checks passed\n";
        return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
