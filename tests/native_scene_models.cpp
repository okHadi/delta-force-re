#include "dfre/scene_models.hpp"

#include <iostream>
#include <stdexcept>

// Silent native checks for the scene model join. Fixtures are hand-built
// models and actor images; the actual mission join is covered by
// tools/scene_models_probe.cpp against shipped assets. Original addresses in
// comments refer to Df.exe sha256
// abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff.
namespace {
void check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

constexpr std::uint32_t kTextureLevel = 0;

dfre::ModelGammaTable test_gamma() {
    dfre::ModelGammaTable gamma{};
    for (std::size_t i = 0; i < gamma.size(); ++i) gamma[i] = std::uint8_t(i);
    return gamma;
}

dfre::ModelPaletteBanks32 test_banks() {
    dfre::ModelPaletteBanks32 banks;
    for (auto& bank : banks) {
        bank.gamma_rgb.assign(768, 64);
        bank.display_rgb.assign(768, 80);
        bank.matching_rgb.assign(768, 64);
        bank.shades.assign(64 * 256, 0xff808080u);
    }
    return banks;
}

// One LOD with `meshes` meshes of one triangle each, a flat material, and
// camera-facing geometry at 256 file units (1.0 camera depth).
dfre::Model make_flat_model(std::size_t meshes, std::uint32_t render_type,
    std::int32_t bounding_radius) {
    dfre::Model model;
    model.name = "fixture";
    model.version = 5;
    model.lod_thresholds = {0, 0, 0, 0};
    model.render_types = {render_type, render_type, render_type, render_type};
    model.bounding_radius = bounding_radius;
    dfre::ModelLod lod;
    lod.raw_header.assign(0xb8, 0);
    for (std::size_t mesh = 0; mesh < meshes; ++mesh) {
        dfre::ModelMesh record;
        record.vertex_start = static_cast<std::uint32_t>(mesh * 3);
        record.vertex_count = 3;
        record.face_start = static_cast<std::uint32_t>(mesh);
        record.face_count = 1;
        record.normal_start = 0;
        record.normal_count = 1;
        lod.meshes.push_back(record);
    }
    for (std::size_t mesh = 0; mesh < meshes; ++mesh) {
        const std::int16_t base = static_cast<std::int16_t>(mesh * 200);
        lod.vertices.push_back(dfre::ModelVertex{{256, base, 0}, 0});
        lod.vertices.push_back(dfre::ModelVertex{{256, static_cast<std::int16_t>(base + 200), 0}, 0});
        lod.vertices.push_back(dfre::ModelVertex{{256, base, 200}, 0});
        dfre::ModelFace face;
        const auto start = std::uint32_t(mesh * 3);
        face.vertices = {start, start + 1, start + 2};
        face.normals = {0, 0, 0};
        face.u = {0, 0, 0};
        face.v = {0, 0, 0};
        face.material = 0;
        face.flags = 0;
        face.face_normal = 0;
        lod.faces.push_back(face);
    }
    lod.normals.push_back(dfre::ModelNormal{{0, 0, 0}, 0});
    dfre::ModelMaterial material;
    material.name = "flat";
    material.flags = 2;
    material.raw[0x10] = 2;
    lod.materials.push_back(material);
    lod.hardpoints = {{{0, 0, 0}}, {{0x100, 0, 0}}, {{0x200, 0, 0}}};
    model.lods.push_back(std::move(lod));
    return model;
}

dfre::ActorImage make_image(std::uint32_t model, std::uint32_t instance, std::uint32_t definition,
    std::int32_t x, std::int32_t y) {
    dfre::ActorImage image{};
    const auto put = [&](std::size_t offset, std::uint32_t value) {
        for (std::size_t i = 0; i < 4; ++i) image[offset + i] = std::uint8_t(value >> (i * 8));
    };
    put(0x00, definition);
    put(0x10, model);
    put(0x18, instance);
    put(0x24, static_cast<std::uint32_t>(x));
    put(0x28, static_cast<std::uint32_t>(y));
    return image;
}

dfre::SceneModelRecord make_record(const dfre::ActorImage& image, bool character,
    std::size_t group, std::size_t slot) {
    dfre::SceneModelRecord record;
    record.identity = dfre::scene_model_identity(0x76000000u + std::uint32_t(group * 0x10000 + slot));
    record.image = image;
    record.group = group;
    record.slot = slot;
    record.character = character;
    return record;
}

dfre::MissionRuntimeState make_state() {
    dfre::MissionRuntimeState state;
    state.definitions.resize(2);
    state.definitions[0].type = 6; // object
    state.definitions[1].type = 3; // person
    state.definitions[1].character = 0x72000000u;
    state.models.push_back({"fixture.3di", 0, 0x71000000u});
    state.character_instances.assign(1, 1);
    return state;
}

dfre::SceneModelCollectInputs collect_inputs(const dfre::SceneModelView& view) {
    dfre::SceneModelCollectInputs inputs;
    inputs.view = view;
    return inputs;
}

void expect_no_gap(const std::vector<dfre::SceneModelGap>& gaps, dfre::SceneModelGapKind kind,
    const char* message) {
    for (const auto& gap : gaps)
        if (gap.kind == kind) check(false, message);
}
void expect_gap(const std::vector<dfre::SceneModelGap>& gaps, dfre::SceneModelGapKind kind,
    const char* message) {
    for (const auto& gap : gaps)
        if (gap.kind == kind) return;
    check(false, message);
}
} // namespace

int main() {
    using namespace dfre;
    try {
        // Tag dispatch: every proven row key and the unknown-key fallback.
        check(scene_model_tag(0x676e7263u).kind == SceneModelTagKind::generic_crng, "gnrc dispatch");
        check(scene_model_tag(0x6f726730u).kind == SceneModelTagKind::character_org0, "org0 dispatch");
        check(scene_model_tag(0x6e756c6cu).kind == SceneModelTagKind::null_row, "llun dispatch");
        const auto bird = scene_model_tag(0x62697264u);
        check(bird.kind == SceneModelTagKind::special && !bird.default_row &&
            bird.special == ModelSpecialTag::bird, "drib dispatch");
        const auto tree = scene_model_tag(0x74726565u);
        check(tree.special == ModelSpecialTag::tree && !tree.default_row, "tree dispatch");
        const auto unknown = scene_model_tag(0x31323334u);
        check(unknown.kind == SceneModelTagKind::special && unknown.default_row &&
            unknown.special == ModelSpecialTag::bird, "0x4818f0 default row");
        check(std::string(scene_model_gap_name(SceneModelGapKind::missing_pose)) == "missing_pose",
            "gap name");

        const auto banks = test_banks();
        const auto gamma = test_gamma();
        auto model = make_flat_model(1, 0x676e7263u, 0x10000);
        SceneModelAssets assets;
        assets.models.push_back({0x71000000u, 0, &model});

        // Catalog checks.
        SceneModelCatalog catalog(assets, banks, gamma, kTextureLevel);
        check(catalog.model(0x71000000u) == &model, "catalog model lookup");
        check(catalog.applied_lod(0x71000000u, 0) != nullptr, "catalog applied lod");
        check(catalog.applied_lod(0x71000000u, 1) == nullptr, "catalog lod bounds");
        check(catalog.raster_materials(0x71000000u, 0).size() == 1, "catalog raster materials");
        try {
            SceneModelAssets duplicate = assets;
            duplicate.models.push_back({0x71000000u, 0, &model});
            SceneModelCatalog bad(duplicate, banks, gamma);
            check(false, "duplicate handle accepted");
        } catch (const std::invalid_argument&) {}
        try {
            SceneModelAssets bad_bank;
            bad_bank.models.push_back({0x71000000u, 6, &model});
            SceneModelCatalog bad(bad_bank, banks, gamma);
            check(false, "bank 6 accepted");
        } catch (const std::invalid_argument&) {}
        try {
            SceneModelCatalog bad(assets, banks, gamma, 4);
            check(false, "texture level 4 accepted");
        } catch (const std::invalid_argument&) {}

        // View: renderer camera and collection state must agree.
        const RasterClip clip{0, 0, 63, 63};
        const auto view = make_scene_model_view({0, 0, 0}, {0, 0, 0}, clip, 90);
        check(view.camera.projection.focal_length == 32, "camera focal");
        check(view.camera.planes.size() == 5, "camera plane count");
        check(view.view.world_to_camera == view.camera.matrix, "collection matrix");
        check(view.view.projection.focal == view.camera.projection.focal_length, "collection focal");
        check(view.view.projection.centre_x == (32 << 16) && view.view.projection.centre_y == (32 << 16),
            "collection centre");
        check(view.view.projection.planes[0].offset == 4096, "near plane offset");
        check(view.bounds.left == 0 && view.bounds.bottom == 63, "collection bounds");
        const auto lighting = make_scene_model_lighting(view);
        check(lighting.ambient == scene_model_light_ambient &&
            lighting.diffuse == scene_model_light_diffuse &&
            lighting.specular == scene_model_light_specular, "scene light strengths");

        // Row table views.
        std::vector<std::uint16_t> depth(64 * 64, 0xffff);
        const auto rows = scene_model_depth_rows(depth, 64, 64);
        check(rows.size() == 64 && rows[1] == depth.data() + 64, "depth row pointers");
        std::vector<std::uint32_t> keys(64, 0x7fffffffu);
        const auto table = scene_model_row_table(keys, rows);
        check(table.row_keys != nullptr && table.row_words == rows.data(), "row table view");
        try {
            scene_model_depth_rows(depth, 32, 64);
            check(false, "depth rows accepted a smaller image");
        } catch (const std::invalid_argument&) {}

        // Collect: near list, generic draw plan and pixels.
        const auto state = make_state();
        SceneModels32 renderer(catalog, state);
        renderer.begin_frame();
        auto inputs = collect_inputs(view);
        inputs.records.push_back(make_record(make_image(0x71000000u, 0, 0, 0x20000, 0), false, 0, 0));
        auto collected = renderer.collect(inputs);
        check(collected.near.size() == 1 && collected.large.empty(), "near collect");
        check(collected.character.empty(), "empty character collect");
        expect_gap(collected.gaps, SceneModelGapKind::runtime_pool_not_supplied, "runtime pool gap");
        expect_gap(collected.gaps, SceneModelGapKind::effect_table_not_supplied, "effect gap");

        SceneModelPassInputs pass;
        pass.view = view;
        pass.time = 7;
        pass.rows = table;
        auto plan = renderer.plan_after_terrain(pass);
        check(plan.calls.size() == 1 && plan.entries == 1, "near plan");
        const auto& call = plan.calls[0];
        check(call.kind == SceneModelTagKind::generic_crng, "generic call kind");
        check(call.items.size() == 1 && call.matrices.size() == 1, "generic call items");
        std::vector<std::uint32_t> colour(64 * 64, 0);
        std::vector<std::uint16_t> target_depth(64 * 64, 0xffff);
        SceneModelDrawInputs draw;
        draw.width = 64;
        draw.height = 64;
        draw.colour = &colour;
        draw.depth = &target_depth;
        draw.lighting = lighting;
        const auto stats = renderer.draw_plan(plan, draw);
        check(stats.calls == 1 && stats.items == 1, "draw stats");
        check(stats.pixel_writes > 0, "generic draw wrote no pixels");

        ModelDeferredBridge bridge;
        bridge.reset(clip,64,64);
        bridge.set_effect_mode(true);
        std::vector<std::uint32_t> queued_colour(64*64,0);
        std::vector<std::uint16_t> queued_depth(64*64,0xffff);
        auto queued_draw=draw;
        queued_draw.colour=&queued_colour;
        queued_draw.depth=&queued_depth;
        queued_draw.deferred=&bridge;
        check(renderer.draw_plan(plan,queued_draw).pixel_writes==0,
            "large mode wrote pixels before the terrain pass");
        check(!bridge.queue(EffectRoute::deferred).empty(),"large mode queued no faces");
        check(bridge.draw_mixed(clip,64,64,queued_colour,queued_depth)>0,
            "mixed drain wrote no model pixels");
        bridge.clear_queues();
        bridge.set_effect_mode(false);
        std::fill(queued_colour.begin(),queued_colour.end(),0);
        std::fill(queued_depth.begin(),queued_depth.end(),0xffff);
        renderer.draw_plan(plan,queued_draw);
        check(queued_colour==colour && queued_depth==target_depth,
            "direct bridge changed opaque model pixels");

        // Large list: 0x640000 split and the separate before-terrain pass.
        auto big_model = make_flat_model(1, 0x676e7263u, 0x100000);
        SceneModelAssets big_assets;
        big_assets.models.push_back({0x71000001u, 0, &big_model});
        SceneModelCatalog big_catalog(big_assets, banks, gamma);
        SceneModels32 big_renderer(big_catalog, state);
        auto big_inputs = collect_inputs(view);
        big_inputs.records.push_back(make_record(make_image(0x71000001u, 0, 0, 0x20000, 0), false, 0, 0));
        const auto big_collected = big_renderer.collect(big_inputs);
        check(big_collected.large.size() == 1 && big_collected.near.empty(), "large collect");
        const auto large_plan = big_renderer.plan_large_pass(pass);
        check(large_plan.calls.size() == 1, "large plan");
        const auto after_plan = big_renderer.plan_after_terrain(pass);
        check(after_plan.calls.empty(), "large object leaked into the near plan");

        // Collect filters: hidden flag, null handle, missing handle, camera range.
        auto hidden = make_image(0x71000000u, 0, 0, 0x10000, 0);
        hidden[0x6c] = 2;
        auto filtered = collect_inputs(view);
        filtered.records.push_back(make_record(hidden, false, 0, 0));
        filtered.runtime_records.push_back(make_record(ActorImage{}, false, 9, 0));
        filtered.effects.push_back({scene_model_identity(0x9f3098u), {0, 0, 0}, 0x71000000u, 1});
        const auto filtered_result = renderer.collect(filtered);
        check(filtered_result.near.size() == 1, "effect is collected after hidden and null objects are skipped");
        expect_no_gap(filtered_result.gaps, SceneModelGapKind::runtime_pool_not_supplied,
            "runtime pool reported missing when supplied");
        expect_no_gap(filtered_result.gaps, SceneModelGapKind::effect_table_not_supplied,
            "effect table reported missing when supplied");
        check(filtered_result.effect_records == 1, "effect record count");
        auto missing = collect_inputs(view);
        missing.records.push_back(make_record(make_image(0x71000099u, 0, 0, 0, 0), false, 0, 0));
        const auto missing_result = renderer.collect(missing);
        expect_gap(missing_result.gaps, SceneModelGapKind::missing_model, "missing model gap");
        auto far = collect_inputs(view);
        far.records.push_back(make_record(make_image(0x71000000u, 0, 0, 0x400 << 16, 0), false, 0, 0));
        check(renderer.collect(far).near.empty(), "camera range filter");

        // Effect gate [+0xfc] > 0.
        auto gated = collect_inputs(view);
        gated.effects.push_back({scene_model_identity(0x9f3098u), {0, 0, 0}, 0x71000000u, 0});
        check(renderer.collect(gated).near.empty(), "effect gate 0 collected");

        // Cull: row key below the depth clears the queue entry.
        std::vector<std::uint32_t> zero_keys(64, 0);
        const auto cull_table = scene_model_row_table(zero_keys, rows);
        auto cull_inputs = collect_inputs(view);
        cull_inputs.records.push_back(make_record(make_image(0x71000000u, 0, 0, 0x20000, 0), false, 0, 0));
        renderer.collect(cull_inputs);
        auto cull_pass = pass;
        cull_pass.rows = cull_table;
        const auto culled = renderer.plan_after_terrain(cull_pass);
        check(culled.calls.empty() && culled.culled == 1, "row cull");

        // Frame flag gate (0x485610 param_1 & 4) and the missing row table.
        auto gated_pass = pass;
        gated_pass.frame_flags = 0;
        renderer.collect(cull_inputs);
        check(renderer.plan_large_pass(gated_pass).calls.empty(), "large pass flag gate");
        check(renderer.plan_after_terrain(gated_pass).calls.empty(), "after-terrain flag gate");
        auto no_rows_pass = pass;
        no_rows_pass.rows = {};
        renderer.collect(cull_inputs);
        const auto no_rows = renderer.plan_after_terrain(no_rows_pass);
        expect_gap(no_rows.gaps, SceneModelGapKind::row_table_not_supplied, "row table gap");

        // llun: no draw callback, no items.
        auto llun_model = make_flat_model(1, 0x6e756c6cu, 0x10000);
        SceneModelAssets llun_assets;
        llun_assets.models.push_back({0x71000002u, 0, &llun_model});
        SceneModelCatalog llun_catalog(llun_assets, banks, gamma);
        SceneModels32 llun_renderer(llun_catalog, state);
        auto llun_inputs = collect_inputs(view);
        llun_inputs.records.push_back(make_record(make_image(0x71000002u, 0, 0, 0x20000, 0), false, 0, 0));
        llun_renderer.collect(llun_inputs);
        const auto llun_plan = llun_renderer.plan_after_terrain(pass);
        check(llun_plan.calls.size() == 1 && llun_plan.calls[0].items.empty(), "llun items");

        // Unknown tag: row 0 fallback reported, bird steps planned.
        auto unknown_model = make_flat_model(3, 0x31323334u, 0x10000);
        SceneModelAssets unknown_assets;
        unknown_assets.models.push_back({0x71000003u, 0, &unknown_model});
        SceneModelCatalog unknown_catalog(unknown_assets, banks, gamma);
        SceneModels32 unknown_renderer(unknown_catalog, state);
        auto unknown_inputs = collect_inputs(view);
        unknown_inputs.records.push_back(make_record(make_image(0x71000003u, 0, 0, 0x20000, 0), false, 0, 0));
        unknown_renderer.collect(unknown_inputs);
        const auto unknown_plan = unknown_renderer.plan_after_terrain(pass);
        expect_gap(unknown_plan.gaps, SceneModelGapKind::unsupported_tag, "unknown tag gap");
        check(unknown_plan.calls.size() == 1 &&
            unknown_plan.calls[0].items.size() == 3, "bird fallback items");

        // Specials: a bird row builds one step per mesh from the actor image.
        auto special_model = make_flat_model(3, 0x62697264u, 0x10000);
        SceneModelAssets special_assets;
        special_assets.models.push_back({0x71000004u, 0, &special_model});
        SceneModelCatalog special_catalog(special_assets, banks, gamma);
        SceneModels32 special_renderer(special_catalog, state);
        auto special_image = make_image(0x71000004u, 0, 0, 0x20000, 0);
        special_image[0xe0] = 0x40; // object +0xe0 angle source
        auto special_inputs = collect_inputs(view);
        special_inputs.records.push_back(make_record(special_image, false, 0, 0));
        special_renderer.collect(special_inputs);
        const auto special_plan = special_renderer.plan_after_terrain(pass);
        check(special_plan.calls.size() == 1 &&
            special_plan.calls[0].kind == SceneModelTagKind::special, "special call");
        check(special_plan.calls[0].items.size() == 3, "special step items");
        check(special_plan.calls[0].special.tag == ModelSpecialTag::bird, "special plan tag");
        check(!special_plan.calls[0].matrices.empty(), "special step matrices");
        std::vector<std::uint32_t> special_colour(64 * 64, 0);
        std::vector<std::uint16_t> special_depth(64 * 64, 0xffff);
        auto special_draw = draw;
        special_draw.colour = &special_colour;
        special_draw.depth = &special_depth;
        const auto special_stats = special_renderer.draw_plan(special_plan, special_draw);
        check(special_stats.items == 3 && special_stats.pixel_writes > 0, "special draw");

        // Characters: org0 through the model_characters API.
        CharacterModel character_fixture;
        character_fixture.raw.assign(64, 0);
        character_fixture.runtime_prepared = true;
        character_fixture.bones.push_back(CharacterBone{});
        character_fixture.bone_map.push_back(0);
        std::vector<CharacterBonePose> pose(1);
        auto character_model_data = make_flat_model(1, 0x6f726730u, 0x10000);
        SceneModelAssets character_assets;
        character_assets.models.push_back({0x71000005u, 0, &character_model_data});
        character_assets.characters.push_back({0x72000000u, &character_fixture});
        SceneModelCatalog character_catalog(character_assets, banks, gamma);
        SceneModels32 character_renderer(character_catalog, state);
        const auto instance = kCharacterInstanceBase; // state.character_instances[0] -> definition 1
        auto character_image = make_image(0x71000005u, instance, 1, 0x20000, 0);
        auto character_inputs = collect_inputs(view);
        character_inputs.records.push_back(make_record(character_image, true, 3, 0));
        const auto character_collected = character_renderer.collect(character_inputs);
        check(character_collected.character.size() == 1, "character collect");
        check(character_collected.character_records == 1, "character record count");
        // Missing pose first: explicit gap, no call.
        const auto no_pose_plan = character_renderer.plan_after_terrain(pass);
        check(no_pose_plan.calls.empty(), "org0 planned without a pose");
        expect_gap(no_pose_plan.gaps, SceneModelGapKind::missing_pose, "missing pose gap");
        // With the pose: one actor item through the plan.
        character_renderer.set_character_pose(instance, &pose);
        character_renderer.collect(character_inputs);
        const auto character_plan = character_renderer.plan_after_terrain(pass);
        check(character_plan.calls.size() == 1 &&
            character_plan.calls[0].kind == SceneModelTagKind::character_org0, "org0 call");
        check(character_plan.calls[0].items.size() == 1, "org0 items");
        check(character_plan.calls[0].tables.size() == 1 &&
            character_plan.calls[0].tables[0].size() == 1, "org0 matrix table");
        check(character_renderer.character_pose(instance) == &pose, "pose stored");
        std::vector<std::uint32_t> character_colour(64 * 64, 0);
        std::vector<std::uint16_t> character_depth(64 * 64, 0xffff);
        auto character_draw = draw;
        character_draw.colour = &character_colour;
        character_draw.depth = &character_depth;
        const auto character_stats = character_renderer.draw_plan(character_plan, character_draw);
        check(character_stats.items == 1 && character_stats.pixel_writes > 0, "org0 draw");

        // Asset join through resolvers, including unresolved entries.
        auto resolver_state = make_state();
        resolver_state.characters.emplace("player01.chr", 0x72000000u);
        resolver_state.characters.emplace("missing.chr", 0x72000001u);
        const auto resolved = scene_model_assets(resolver_state,
            [&](std::string_view name) -> const Model* {
                return name == "fixture.3di" ? &model : nullptr;
            },
            [&](std::string_view name) -> const CharacterModel* {
                return name == "player01.chr" ? &character_fixture : nullptr;
            });
        check(resolved.models.size() == 1 && resolved.unresolved_models == 0, "asset models");
        check(resolved.characters.size() == 1 && resolved.unresolved_characters == 1,
            "asset characters");

        // Character secondary write resolution (0x485040 actor +0x8c).
        auto resolver_inputs = collect_inputs(view);
        resolver_inputs.secondary_definition = [](std::uint32_t slot) { return slot * 2; };
        auto write_image = character_image;
        write_image[0x94] = 3;
        resolver_inputs.records.push_back(make_record(write_image, true, 3, 0));
        character_renderer.collect(resolver_inputs);
        const auto& writes = character_renderer.queues().writes;
        check(writes.size() == 1 && writes[0].offset == 0x8c && writes[0].value == 6,
            "actor +0x8c write");

        std::cout << "scene models ok\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
