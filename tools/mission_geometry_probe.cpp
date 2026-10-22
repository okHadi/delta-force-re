#include "dfre/mission_geometry.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <algorithm>

namespace {
template<class Bytes> void put(Bytes& bytes, std::size_t at, std::uint32_t value) {
    for (std::size_t i = 0; i < 4; ++i) bytes.at(at+i) = std::uint8_t(value >> (i*8));
}
template<class Bytes> std::int32_t integer(const Bytes& bytes,std::size_t at) {
    const auto value = std::uint32_t(bytes[at]) | (std::uint32_t(bytes[at+1])<<8) |
        (std::uint32_t(bytes[at+2])<<16) | (std::uint32_t(bytes[at+3])<<24);
    return value <= 0x7fffffff ? std::int32_t(value) : std::int32_t(std::int64_t(value)-0x100000000LL);
}
void print_volume(const dfre::CollisionVolume& volume) {
    std::cout << ' ' << volume.type << ' ' << volume.flags;
    for (const auto value : volume.center) std::cout << ' ' << value;
    std::cout << ' ' << volume.radius << ' ' << volume.horizontal_radius << ' ' << volume.horizontal_radius_copy;
    for (const auto value : volume.half_extent) std::cout << ' ' << value;
    for (const auto value : volume.bounds) std::cout << ' ' << value;
}
}
// Input: translation[3] angles[3] query[3] terrain depth flags primary alternate.
// Output: radius_64 height_60 returned_height entity_hit piece_count.
int main(int argc, char** argv) {
    try {
        if (argc < 2 || argc > 4)
            throw std::invalid_argument("usage: mission_geometry_probe [--world|--callback] MODEL.3DI [CHARACTER.CHR]");
        const std::string mode = argc >= 3 ? argv[1] : "--ground";
        std::ifstream file(argv[argc >= 3 ? 2 : 1], std::ios::binary);
        if (!file) throw std::invalid_argument("cannot open mission geometry model");
        const dfre::AssetBytes bytes(std::istreambuf_iterator<char>(file), {});
        auto model = dfre::decode_model(bytes);
        for (auto& lod : model.lods) lod = dfre::localize_model_lod(lod);
        std::optional<dfre::CharacterModel> character;
        if (argc == 4) {
            if (mode != "--callback") throw std::invalid_argument("CHR path requires callback mode");
            std::ifstream chr(argv[3], std::ios::binary);
            if (!chr) throw std::invalid_argument("cannot open mission geometry character");
            const dfre::AssetBytes raw(std::istreambuf_iterator<char>(chr), {});
            character = dfre::prepare_character_runtime(dfre::decode_character(raw));
        }
        dfre::MissionGeometry geometry({dfre::mission_geometry_model(1, model, bytes),
            dfre::mission_geometry_model(2, model, bytes)});
        if (mode == "--volumes") {
            const auto descriptor = geometry.collision_model(1);
            const dfre::MissionGeometryMatrixSource identity = [](const auto& binding,const auto&,const auto&) {
                return dfre::MissionGeometryMatrices{true,
                    std::vector<dfre::ModelMatrix>(binding.model->lods[0].meshes.size(),dfre::model_identity_matrix()),{}};
            };
            const auto state = geometry.collision_state(1,dfre::AiActorImage{},identity);
            std::cout << descriptor.radius_64 << ' ' << descriptor.collision_present << ' ' << (state ? state->size() : 0);
            if (state) for (const auto& mesh : *state) {
                for (const auto bound : mesh.bounds) std::cout << ' ' << bound;
                std::cout << ' ' << mesh.volumes.size();
                for (const auto& volume : mesh.volumes) print_volume(volume);
            }
            std::cout << '\n';
            return 0;
        }
        std::string line;
        while (std::getline(std::cin, line)) {
            std::istringstream fields(line);
            dfre::AiActorImage image{};
            if (mode == "--callback") {
                for (std::size_t at = 0; at < image.size(); at += 4) {
                    std::uint32_t value{};
                    if (!(fields >> value)) throw std::invalid_argument("callback actor image is incomplete");
                    put(image, at, value);
                }
                std::vector<std::uint8_t> prior;
                for (std::size_t mesh = 0; mesh < model.lods[0].meshes.size(); ++mesh) {
                    unsigned flag{};
                    if (!(fields >> flag)) throw std::invalid_argument("callback scratch flags are incomplete");
                    prior.push_back(std::uint8_t(flag));
                }
                unsigned stack_flag{};
                if (!(fields >> stack_flag)) throw std::invalid_argument("callback stack flag is missing");
                std::vector<dfre::CharacterBonePose> pose;
                if (character) {
                    pose.resize(model.lods[0].meshes.size());
                    for (auto& bone : pose) {
                        for (auto& angle : bone.angles)
                            if (!(fields >> angle)) throw std::invalid_argument("callback character pose is incomplete");
                        for (auto& scale : bone.scale)
                            if (!(fields >> scale)) throw std::invalid_argument("callback character scale is incomplete");
                    }
                }
                const auto binding = dfre::mission_geometry_model(1,model,bytes);
                const auto state = dfre::mission_geometry_callback_matrices(binding,image,
                    character ? &*character : nullptr,character ? &pose : nullptr,&prior,std::uint8_t(stack_flag));
                std::cout << state.present << ' ' << state.matrices.size();
                for (std::size_t slot = 0; slot < state.matrices.size(); ++slot) {
                    std::cout << ' ' << unsigned(state.flags.at(slot));
                    for (const auto value : state.matrices[slot]) std::cout << ' ' << value;
                }
                std::cout << '\n';
                continue;
            }
            for (std::size_t i = 0; i < 6; ++i) {
                std::int64_t value{};
                if (!(fields >> value)) throw std::invalid_argument("mission geometry transform is incomplete");
                put(image, 0x24+i*4, std::uint32_t(value));
            }
            const dfre::MissionGeometryMatrixSource source = [](const auto& binding,const auto& actor,const auto&) {
                const std::vector<std::uint8_t> prior(32);
                return dfre::mission_geometry_callback_matrices(binding,actor,nullptr,nullptr,&prior,std::uint8_t(0));
            };
            if (mode == "--collision") {
                dfre::AiActorImage actor{};
                std::array<std::int32_t,3> point{};
                std::uint32_t flags{},primary{},alternate{},definition_flags{};
                if (!(fields >> point[0] >> point[1] >> point[2] >> flags >> primary >> alternate >> definition_flags))
                    throw std::invalid_argument("actor collision query is incomplete");
                for (std::size_t axis = 0; axis < 3; ++axis) put(actor,0x24+axis*4,std::uint32_t(point[axis]));
                put(image,0x6c,flags);put(image,0x10,primary);put(image,0x14,alternate);
                dfre::ActorCollisionBindings bindings;
                unsigned callbacks = 0;
                std::uint32_t selected = 0;
                std::optional<std::vector<dfre::ActorCollisionMesh>> state;
                bindings.model = [&](auto handle) { selected=handle;return geometry.collision_model(handle); };
                bindings.state = [&](auto handle,auto target) -> const std::vector<dfre::ActorCollisionMesh>* {
                    ++callbacks;state=geometry.collision_state(handle,*target.image,source);return state ? &*state : nullptr;
                };
                bindings.definition = [&](auto) { return dfre::ActorCollisionDefinition{0,definition_flags}; };
                const auto hit = dfre::actor_target_collision({&actor,1},{&image,2},bindings);
                std::cout << hit;
                for (std::size_t axis = 0; axis < 3; ++axis) std::cout << ' ' << integer(actor,0x24+axis*4);
                std::cout << ' ' << callbacks << ' ' << selected << '\n';
                continue;
            }
            if (mode == "--world") {
                dfre::WorldHitQuery query;
                for (auto& value : query.origin) fields >> value;
                for (auto& value : query.end) fields >> value;
                for (auto& value : query.direction) fields >> value;
                std::uint32_t flags{},primary{},alternate{},active{},identity{};
                if (!(fields >> query.length >> flags >> primary >> alternate >> active >> identity >> query.skip[0] >> query.skip[1]))
                    throw std::invalid_argument("world geometry query is incomplete");
                for (std::size_t axis = 0; axis < 3; ++axis) {
                    query.bounds[axis*2] = std::min(query.origin[axis],query.end[axis]);
                    query.bounds[axis*2+1] = std::max(query.origin[axis],query.end[axis]);
                }
                put(image,0,active);put(image,0x6c,flags);put(image,0x10,primary);put(image,0x14,alternate);
                auto records = geometry.world_hit_records({{identity,&image}},source);
                unsigned callback_count = 0;
                for (auto& record : records) {
                    if (!record.mesh_source) continue;
                    const auto resolve = record.mesh_source;
                    record.mesh_source = [resolve,&callback_count] { ++callback_count;return resolve(); };
                }
                dfre::WorldModelHit hit;
                const auto code = dfre::world_model_list_hit(records,query,hit);
                std::cout << code << ' ' << hit.hit << ' ' << hit.distance << ' ' << hit.record << ' '
                    << hit.mesh << ' ' << hit.material_flags << ' ' << hit.masked << ' ' << callback_count << '\n';
                continue;
            }
            if (mode != "--ground") throw std::invalid_argument("unknown mission geometry probe mode");
            std::array<std::int32_t,3> query{};
            std::int32_t terrain{}, depth{};
            std::uint32_t flags{}, primary{}, alternate{};
            if (!(fields >> query[0] >> query[1] >> query[2] >> terrain >> depth >> flags >> primary >> alternate))
                throw std::invalid_argument("mission geometry query is incomplete");
            put(image, 0x6c, flags); put(image, 0x10, primary); put(image, 0x14, alternate);
            auto built = geometry.ground_entities({{11, &image}}, query,source);
            const auto result = dfre::ground_entity_height(terrain, query, depth, built.entities);
            const auto bounds = geometry.bounds(1);
            std::cout << bounds.radius_64 << ' ' << bounds.height_60 << ' ' << result.height << ' '
                << (result.entity ? 1 : 0) << ' '
                << (built.storage.empty() ? 0 : built.storage[0].definition->pieces.size()) << '\n';
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
