#include "dfre/effects.hpp"
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <utility>
namespace {void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}}
namespace {
std::vector<std::uint8_t> synthetic_sprite() {
    std::vector<std::uint8_t> bytes(0x68 + 4 + 36 + 4 + 1024 + 16 + 76, 0);
    std::memcpy(bytes.data(), "kaboom", 6);
    bytes[6] = 0;
    bytes[0x68] = 1; // texture count
    std::memcpy(&bytes[0x6c], "TEST01", 6);
    bytes[0x7c] = 4;            // pixel count
    bytes[0x82] = 0x2f;         // flags
    bytes[0x84] = 2; bytes[0x86] = 2; // width and height
    bytes[0x6c + 36] = 9; bytes[0x6c + 37] = 7;
    std::memcpy(&bytes[bytes.size() - 76], "MAT1", 4);
    bytes[bytes.size() - 76 + 16] = 0x83;
    return bytes;
}
std::vector<std::uint8_t> synthetic_prefix(std::size_t lod_bytes) {
    std::vector<std::uint8_t> bytes(176 + 128 + lod_bytes, 0);
    std::memcpy(bytes.data(), "3DI\x03", 4);
    std::memcpy(&bytes[4], "unit", 4);
    bytes[0x14] = 1; // one LOD
    std::uint8_t* header = &bytes[128];
    const std::uint32_t counts[6] = {1, 1, 1, 1, 0, 1};
    for (std::size_t i = 0; i < 6; ++i) {
        const std::uint32_t value = counts[i];
        for (std::size_t b = 0; b < 4; ++b) header[128 + i * 8 + b] = std::uint8_t(value >> (b * 8));
    }
    const std::uint32_t total = 1 * 8 + 1 * 8 + 1 * 72 + 1 * 104 + 1 * 76;
    for (std::size_t b = 0; b < 4; ++b) header[20 + b] = std::uint8_t(total >> (b * 8));
    return bytes;
}
}
int main() {
    // Effect bits come from material bytes +0x10 bit 3, +0x11 bit 7 and +0x10 bit 7.
    check(dfre::effect_face_flags(0) == 0, "plain material gained effect bits");
    check(dfre::effect_face_flags(8) == 0x100, "transparent bit mapping changed");
    check(dfre::effect_face_flags(0x8000) == 0x200, "smoke bit mapping changed");
    check(dfre::effect_face_flags(0x80) == 0x400, "flame bit mapping changed");
    check(dfre::effect_face_flags(0x8088) == 0x700, "combined effect bits changed");
    check(dfre::effect_pass_of(0x414) == dfre::EffectPass::flame, "flame pass detection changed");
    check(std::string(dfre::effect_pass_name(dfre::effect_pass_of(0x214))) == "smoke", "smoke pass name changed");
    // Original route table: draw enabled, effect mode and material bit 0x400.
    const auto route = [](std::uint32_t flags, std::uint32_t material, bool mode, bool enabled) {
        return dfre::route_effect_face({flags, material, mode, enabled});
    };
    check(route(0x104, 0, true, true) == dfre::EffectRoute::deferred_effect, "effect face in mode route changed");
    check(route(0x4, 0, true, true) == dfre::EffectRoute::deferred, "plain face in mode route changed");
    check(route(0x104, 0, false, true) == dfre::EffectRoute::deferred_no_context, "effect face without mode route changed");
    check(route(0x4, 0, false, true) == dfre::EffectRoute::draw, "plain face without mode route changed");
    check(route(0x4, 0x400, true, true) == dfre::EffectRoute::effect_plain, "material bit 0x400 route changed");
    check(route(0x104, 0x400, true, true) == dfre::EffectRoute::effect_plain, "bit 0x400 must win over effect bits");
    check(route(0x104, 0, false, false) == dfre::EffectRoute::skip, "draw-disabled route changed");
    // Queue drain order: C plain, A span builder, two flushes, A smooth plain.
    dfre::EffectQueues queues;
    queues.set_effect_mode(true);
    queues.add(0x4, 0, 0);        // A, non-smooth
    queues.add(0x104, 0, 1);      // B
    queues.add(0x24, 0, 2);       // A with flag 0x20
    queues.add(0x4, 0x400, 3);    // C
    check(queues.deferred().size() == 2 && queues.deferred_effect().size() == 1 &&
          queues.effect_plain().size() == 1 && queues.no_context().empty(), "queue membership changed");
    const auto plan = queues.deferred_plan();
    check(plan.steps.size() == 3, "deferred plan size changed");
    check(plan.steps[0].op == dfre::EffectDrainOp::plain && plan.steps[0].face->index == 3, "queue C must drain first");
    check(plan.steps[1].op == dfre::EffectDrainOp::deferred_span && plan.steps[1].face->index == 0, "queue A span order changed");
    check(plan.steps[2].op == dfre::EffectDrainOp::plain && plan.steps[2].face->index == 2, "queue A smooth tail changed");
    check(plan.flushes == 2, "flush count changed");
    queues.set_effect_mode(false);
    queues.add(0x104, 0, 4);
    const auto plain = queues.plain_plan();
    check(plain.size() == 2 && plain[0]->index == 4 && plain[1]->index == 1, "plain drain order (no context then effect) changed");
    check(dfre::effect_span_selection(0x104) == dfre::EffectSpanSelection::no_write, "0x104 selection changed");
    check(dfre::effect_span_selection(0x214) == dfre::EffectSpanSelection::alpha_blend, "0x214 selection changed");
    check(dfre::effect_span_selection(0x414) == dfre::EffectSpanSelection::half_blend, "0x414 selection changed");
    check(dfre::effect_span_selection(0x438) == dfre::EffectSpanSelection::half_blend, "0x438 selection changed");
    check(dfre::effect_span_selection(0x114) == dfre::EffectSpanSelection::stateful_previous, "0x114 must stay stateful");
    // 32-bit effect spans: 4x1 target, one texture row of four shade bytes.
    constexpr std::uint32_t width = 4, height = 1;
    std::vector<std::uint32_t> colours{0x11223344u, 0x11223344u, 0x11223344u, 0x11223344u};
    std::vector<std::uint16_t> depths{5, 5, 5, 5};
    const std::array<std::uint8_t, 4> texture{4, 0, 128, 255};
    const std::array<std::uint32_t, 256> table{0x01020304u, 0x11111111u, 0x22222222u, 0x33333333u,
        0x01020304u, 0x05060708u, 0x090a0b0cu, 0x0d0e0f10u};
    dfre::EffectSpanContext context;
    context.column_mask = 3;
    context.row_mask = 0;
    context.row_shift = 2;
    context.depth_plane = depths.data();
    context.colour_plane = colours.data();
    context.texture = texture.data();
    context.blend_table = table.data();
    dfre::EffectSpan span;
    span.pixel = 0; span.length = 4; span.depth = 4 << 16; span.du = 65536;
    check(dfre::draw_effect_span32(span, dfre::EffectSpanMode::no_write, context, width, height) == 0 &&
          colours[0] == 0x11223344u, "no-write kernel changed the target");
    check(dfre::draw_effect_span32(span, dfre::EffectSpanMode::half_blend, context, width, height) == 3,
          "half blend write count changed");
    check(colours[0] == (((0x11223344u & 0x00fefefeu) >> 1) + ((0x01020304u & 0xfefefefeu) >> 1)),
          "half blend formula changed");
    check(colours[1] == 0x11223344u, "zero texel must keep the target");
    // Alpha blend moves the target toward the context colour by texel/256.
    // Pixels come in pairs: both share the texel at the pair start, and u
    // advances by 2*du per pair.
    const std::array<std::uint8_t, 4> alpha_texture{200, 0, 100, 0};
    context.texture = alpha_texture.data();
    context.colour = 0x00000000u;
    colours.assign(4, 0x80808080u);
    depths.assign(4, 5);
    dfre::EffectSpan alpha_span;
    alpha_span.pixel = 0; alpha_span.length = 4; alpha_span.depth = 0; alpha_span.du = 65536;
    check(dfre::draw_effect_span32(alpha_span, dfre::EffectSpanMode::alpha_blend, context, width, height) == 4,
          "alpha blend write count changed");
    check(colours[0] == 0x1c1c1c1cu && colours[1] == 0x1c1c1c1cu, "alpha blend first pair changed");
    check(colours[2] == 0x4e4e4e4eu && colours[3] == 0x4e4e4e4eu, "alpha blend pair coordinate step changed");
    // A failed depth test keeps the target.
    colours.assign(4, 0x80808080u);
    depths.assign(4, 0);
    span.depth = 0; span.length = 2; span.du = 65536;
    check(dfre::draw_effect_span32(alpha_span, dfre::EffectSpanMode::alpha_blend, context, width, height) == 0 &&
          colours[0] == 0x80808080u, "alpha blend depth test changed");
    // The blend table is only needed by the half blend kernel.
    context.blend_table = nullptr;
    colours.assign(4, 0x80808080u);
    try { dfre::draw_effect_span32(alpha_span, dfre::EffectSpanMode::half_blend, context, width, height); check(false, "missing blend table accepted"); }
    catch (const std::invalid_argument&) {}
    colours.assign(4, 0);
    try { dfre::draw_effect_span32({0, 5, 0, 0, 0, 0, 0, 0, 0, 0}, dfre::EffectSpanMode::half_blend, context, width, height); check(false, "out-of-range span accepted"); }
    catch (const std::invalid_argument&) {}
    // KABOOM sprite decode: texture record plus named trailer record.
    const auto sprite_bytes = synthetic_sprite();
    const auto sprite = dfre::decode_effect_sprite(sprite_bytes);
    check(sprite.magic == "kaboom" && sprite.texture_count == 1 && sprite.textures.size() == 1, "sprite header decode changed");
    check(sprite.textures[0].name == "TEST01" && sprite.textures[0].width == 2 && sprite.textures[0].height == 2 &&
          sprite.textures[0].pixels.size() == 4 && sprite.textures[0].pixels[0] == 9, "sprite texture decode changed");
    check(sprite.materials.size() == 1 && sprite.materials[0].name == "MAT1" && sprite.materials[0].flags == 0x83,
          "sprite trailer decode changed");
    check(sprite.unknown_begin == sprite.texture_end && sprite.unknown_end == sprite.materials[0].offset &&
          sprite.unknown_end - sprite.unknown_begin == 16, "sprite unknown range changed");
    auto broken = sprite_bytes; broken[1] = 'X';
    try { dfre::decode_effect_sprite(broken); check(false, "wrong sprite magic accepted"); }
    catch (const std::invalid_argument&) {}
    // Standard model prefix decode with a short first LOD.
    const auto short_prefix = dfre::decode_model_prefix(synthetic_prefix(100));
    check(short_prefix.version == 3 && short_prefix.lod_count == 1 && short_prefix.textures.empty(), "prefix header decode changed");
    check(short_prefix.declared_lod_size == 268 && short_prefix.available_lod_bytes == 100 && short_prefix.truncated,
          "prefix truncation report changed");
    check(short_prefix.vertex_count == 1 && short_prefix.face_count == 1 && short_prefix.material_count == 1,
          "prefix LOD counts changed");
    check(!dfre::decode_model_prefix(synthetic_prefix(268)).truncated, "complete prefix marked truncated");
    auto bad_version = synthetic_prefix(100); bad_version[3] = 1;
    try { dfre::decode_model_prefix(bad_version); check(false, "unsupported prefix version accepted"); }
    catch (const std::invalid_argument&) {}
    std::cout << "native effects tests passed\n";
    return 0;
}
