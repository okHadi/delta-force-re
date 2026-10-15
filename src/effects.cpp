#include "dfre/effects.hpp"
#include <algorithm>
#include <cstring>
#include <stdexcept>
namespace dfre {
namespace {
std::uint32_t read_u32(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 4) throw std::invalid_argument("effect data field outside input");
    return std::uint32_t(bytes[offset]) | (std::uint32_t(bytes[offset + 1]) << 8) |
        (std::uint32_t(bytes[offset + 2]) << 16) | (std::uint32_t(bytes[offset + 3]) << 24);
}
std::uint16_t read_u16(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 2) throw std::invalid_argument("effect data field outside input");
    return std::uint16_t(std::uint32_t(bytes[offset]) | (std::uint32_t(bytes[offset + 1]) << 8));
}
std::string read_name(const std::vector<std::uint8_t>& bytes, std::size_t offset, std::size_t length) {
    if (offset > bytes.size() || bytes.size() - offset < length) throw std::invalid_argument("effect name outside input");
    std::string name;
    for (std::size_t i = 0; i < length && bytes[offset + i]; ++i) name.push_back(char(bytes[offset + i]));
    return name;
}
bool is_record_name(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 12) return false;
    bool letters = false;
    for (std::size_t i = 0; i < 12; ++i) {
        const auto value = bytes[offset + i];
        if (!value) break;
        if (value < 0x20 || value > 0x7e) return false;
        if ((value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z')) letters = true;
    }
    return letters;
}
// Texture record layout shared with 3DI: name +0, pixel count +16,
// sequence +20, flags +22, width +24, height +26, pixels +36, then
// 1,024 palette bytes.
EffectTextureRecord read_texture(const std::vector<std::uint8_t>& bytes, std::size_t offset, bool require_area) {
    EffectTextureRecord record;
    record.offset = offset;
    record.name = read_name(bytes, offset, 12);
    const auto size = read_u32(bytes, offset + 16);
    record.sequence = read_u16(bytes, offset + 20);
    record.flags = read_u16(bytes, offset + 22);
    record.width = read_u16(bytes, offset + 24);
    record.height = read_u16(bytes, offset + 26);
    if (require_area && std::uint32_t(record.width) * record.height != size) throw std::invalid_argument("effect texture area differs from pixel count");
    if (offset + 36 > bytes.size() || size > bytes.size() - offset - 36 || bytes.size() - offset - 36 - size < 1024)
        throw std::invalid_argument("effect texture range outside input");
    record.pixels.assign(bytes.begin() + std::ptrdiff_t(offset + 36), bytes.begin() + std::ptrdiff_t(offset + 36 + size));
    std::copy_n(bytes.begin() + std::ptrdiff_t(offset + 36 + size), 1024, record.palette.begin());
    return record;
}
std::uint8_t saturate8(std::int32_t value) {
    return std::uint8_t(value < 0 ? 0 : value > 255 ? 255 : value);
}
}
EffectPass effect_pass_of(std::uint32_t face_flags) {
    switch (face_flags & effect_face_flag_mask) {
        case 0x100: return EffectPass::transparent;
        case 0x200: return EffectPass::smoke;
        case 0x400: return EffectPass::flame;
        default: return EffectPass::none;
    }
}
const char* effect_pass_name(EffectPass pass) {
    switch (pass) {
        case EffectPass::transparent: return "transparent";
        case EffectPass::smoke: return "smoke";
        case EffectPass::flame: return "flame";
        default: return "none";
    }
}
std::uint32_t effect_face_flags(std::uint32_t material_flags) {
    std::uint32_t flags = 0;
    if (material_flags & 8) flags |= 0x100;
    if (material_flags & 0x8000) flags |= 0x200;
    if (material_flags & 0x80) flags |= 0x400;
    return flags;
}
EffectRoute route_effect_face(const EffectRouteInput& input) {
    if (!input.draw_enabled) return EffectRoute::skip;
    const bool effect = (input.face_flags & effect_face_flag_mask) != 0;
    if (!input.effect_mode) return effect ? EffectRoute::deferred_no_context : EffectRoute::draw;
    if ((input.material_flags & 0x400) == 0) return effect ? EffectRoute::deferred_effect : EffectRoute::deferred;
    return EffectRoute::effect_plain;
}
const char* effect_route_name(EffectRoute route) {
    switch (route) {
        case EffectRoute::skip: return "skip";
        case EffectRoute::draw: return "draw";
        case EffectRoute::deferred_no_context: return "deferred_no_context";
        case EffectRoute::deferred: return "deferred";
        case EffectRoute::deferred_effect: return "deferred_effect";
        case EffectRoute::effect_plain: return "effect_plain";
    }
    return "unknown";
}
EffectRoute EffectQueues::add(std::uint32_t face_flags, std::uint32_t material_flags, std::size_t index) {
    const auto route = route_effect_face({face_flags, material_flags, effect_mode_, true});
    switch (route) {
        case EffectRoute::deferred: deferred_.push_back({face_flags, index}); break;
        case EffectRoute::deferred_effect: deferred_effect_.push_back({face_flags, index}); break;
        case EffectRoute::effect_plain: effect_plain_.push_back({face_flags, index}); break;
        case EffectRoute::deferred_no_context: no_context_.push_back({face_flags, index}); break;
        default: break;
    }
    return route;
}
EffectDrainPlan EffectQueues::deferred_plan() const {
    EffectDrainPlan plan;
    // 0x46be50 first loop drains 0xa165ac with the plain path.
    for (const auto& face : effect_plain_) plan.steps.push_back({EffectDrainOp::plain, &face});
    // Then 0xa1a42c: faces without flag 0x20 use the span builder.
    for (const auto& face : deferred_)
        if (!(face.flags & 0x20)) plan.steps.push_back({EffectDrainOp::deferred_span, &face});
    // Then 0x46cc30 and 0x46d190 run once per drain.
    plan.flushes = 2;
    // Then 0xa1a42c faces with flag 0x20 use the plain path.
    for (const auto& face : deferred_)
        if (face.flags & 0x20) plan.steps.push_back({EffectDrainOp::plain, &face});
    return plan;
}
std::vector<const EffectQueueFace*> EffectQueues::plain_plan() const {
    std::vector<const EffectQueueFace*> plan;
    // 0x46bf60 drains 0xaa8d6c, then 0x46bf10 drains 0xa184ec.
    for (const auto& face : no_context_) plan.push_back(&face);
    for (const auto& face : deferred_effect_) plan.push_back(&face);
    return plan;
}
void EffectQueues::clear() {
    deferred_.clear(); deferred_effect_.clear(); effect_plain_.clear(); no_context_.clear();
}

EffectSpanSelection effect_span_selection(std::uint32_t face_flags) {
    // Selector 0x4683b0 masks the key with 0x73c.
    switch (face_flags & 0x73cu) {
        case 0x100: case 0x104: case 0x108:
        case 0x200: case 0x204: case 0x208:
            return EffectSpanSelection::no_write;
        case 0x210: case 0x214: case 0x218:
            return EffectSpanSelection::alpha_blend;
        case 0x410: case 0x414: case 0x418:
        case 0x430: case 0x434: case 0x438:
            return EffectSpanSelection::half_blend;
        default:
            return EffectSpanSelection::stateful_previous;
    }
}
const char* effect_span_selection_name(EffectSpanSelection selection) {
    switch (selection) {
        case EffectSpanSelection::no_write: return "no_write";
        case EffectSpanSelection::alpha_blend: return "alpha_blend";
        case EffectSpanSelection::half_blend: return "half_blend";
        default: return "stateful_previous";
    }
}

std::size_t draw_effect_span32(const EffectSpan& span, EffectSpanMode mode,
    const EffectSpanContext& context, std::uint32_t width, std::uint32_t height) {
    const auto size = std::uint64_t(width) * height;
    const auto offset = std::uint64_t(std::uint32_t(span.pixel));
    if (span.pixel < 0 || span.length < 0 || height == 0 ||
        std::uint32_t(span.pixel) > size || std::uint64_t(span.length) > size - offset)
        throw std::invalid_argument("effect span exceeds target");
    if (mode == EffectSpanMode::no_write) return 0;
    if (!context.depth_plane || !context.colour_plane) throw std::invalid_argument("effect span context is missing target planes");
    if (!context.texture) throw std::invalid_argument("effect span context is missing a texture");
    if (context.row_shift > 16) throw std::invalid_argument("effect span context row shift exceeds 16");
    // Shared texel index step from both 32-bit kernels: row from v, column
    // from u. row_shift is the texture width bit count.
    const auto index_at = [&](std::int32_t u, std::int32_t v) {
        const auto row = (std::uint32_t(v) & (context.row_mask << 16)) >> (16 - context.row_shift);
        const auto column = (std::uint32_t(u) >> 16) & context.column_mask;
        return std::size_t(row + column);
    };
    if (mode == EffectSpanMode::alpha_blend) {
        const auto colour = context.colour;
        auto u = span.u, v = span.v, depth = span.depth;
        std::size_t blended = 0, i = 0;
        while (i < std::size_t(span.length)) {
            const auto texel = context.texture[index_at(u, v)];
            for (int pair = 0; pair < 2 && i < std::size_t(span.length); ++pair, ++i, depth = std::int32_t(std::uint32_t(depth) + std::uint32_t(span.ddepth))) {
                if (std::uint16_t(std::uint32_t(depth) >> 16) >= context.depth_plane[offset + i]) continue;
                const auto target = context.colour_plane[offset + i];
                std::uint32_t result = 0;
                for (unsigned byte = 0; byte < 4; ++byte) {
                    const auto target_byte = std::int32_t((target >> (byte * 8)) & 0xff);
                    const auto colour_byte = std::int32_t((colour >> (byte * 8)) & 0xff);
                    const auto delta = (colour_byte - target_byte) * texel;
                    result |= std::uint32_t(saturate8(target_byte + (delta >> 8))) << (byte * 8);
                }
                context.colour_plane[offset + i] = result;
                ++blended;
            }
            u = std::int32_t(std::uint32_t(u) + 2 * std::uint32_t(span.du));
            v = std::int32_t(std::uint32_t(v) + 2 * std::uint32_t(span.dv));
        }
        return blended;
    }
    if (mode == EffectSpanMode::half_blend) {
        if (!context.blend_table) throw std::invalid_argument("effect blend table is missing");
        auto u = span.u, v = span.v, depth = span.depth;
        std::size_t blended = 0;
        for (std::int32_t i = 0; i < span.length; ++i) {
            if (std::uint16_t(std::uint32_t(depth) >> 16) < context.depth_plane[offset + std::size_t(i)]) {
                const auto texel = context.texture[index_at(u, v)];
                if (texel) {
                    const auto value = context.blend_table[texel];
                    const auto target = context.colour_plane[offset + std::size_t(i)];
                    // Original 0x4682a0 masks the target with 0x00fefefe, so
                    // the target high byte is dropped before the halving.
                    context.colour_plane[offset + std::size_t(i)] =
                        ((target & 0x00fefefeu) >> 1) + ((value & 0xfefefefeu) >> 1);
                    ++blended;
                }
            }
            u = std::int32_t(std::uint32_t(u) + std::uint32_t(span.du));
            v = std::int32_t(std::uint32_t(v) + std::uint32_t(span.dv));
            depth = std::int32_t(std::uint32_t(depth) + std::uint32_t(span.ddepth));
        }
        return blended;
    }
    throw std::invalid_argument("unsupported effect span mode");
}

EffectSpriteFile decode_effect_sprite(const std::vector<std::uint8_t>& bytes) {
    EffectSpriteFile file;
    file.file_size = bytes.size();
    if (bytes.size() < 7 || std::memcmp(bytes.data(), "kaboom", 6) != 0)
        throw std::invalid_argument("effect sprite magic is not kaboom");
    file.magic = "kaboom";
    file.marker = bytes[6];
    if (bytes.size() < 0x6c) throw std::invalid_argument("effect sprite header is short");
    file.header.assign(bytes.begin() + 7, bytes.begin() + 0x68);
    file.texture_count = read_u32(bytes, 0x68);
    if (file.texture_count > 64) throw std::invalid_argument("effect sprite texture count exceeds bound");
    auto position = std::size_t(0x6c);
    for (std::uint32_t i = 0; i < file.texture_count; ++i) {
        const auto record = read_texture(bytes, position, true);
        position += 36 + record.pixels.size() + 1024;
        file.textures.push_back(record);
    }
    file.texture_end = position;
    // Trailer records use the 76-byte 3DI material layout and end at the
    // file end. The scan stops at the first slot without a name.
    auto end = bytes.size();
    std::vector<EffectSpriteMaterial> reversed;
    while (end >= file.texture_end + 76 && is_record_name(bytes, end - 76)) {
        const auto offset = end - 76;
        EffectSpriteMaterial material;
        material.name = read_name(bytes, offset, 12);
        material.flags = read_u32(bytes, offset + 16);
        material.timer = bytes[offset + 30];
        material.frame = bytes[offset + 31];
        material.offset = offset;
        reversed.push_back(material);
        end = offset;
    }
    file.materials.assign(reversed.rbegin(), reversed.rend());
    file.unknown_begin = file.texture_end;
    file.unknown_end = file.materials.empty() ? bytes.size() : file.materials.front().offset;
    return file;
}

ModelPrefixFile decode_model_prefix(const std::vector<std::uint8_t>& bytes) {
    ModelPrefixFile file;
    file.file_size = bytes.size();
    if (bytes.size() < 128 || std::memcmp(bytes.data(), "3DI", 3) != 0)
        throw std::invalid_argument("model prefix magic is not 3DI");
    file.version = bytes[3];
    if (file.version < 2 || file.version > 5) throw std::invalid_argument("model prefix version is outside 2..5");
    file.name = read_name(bytes, 4, 12);
    file.lod_count = read_u32(bytes, 20);
    const std::uint32_t texture_count = read_u32(bytes, 124);
    if (texture_count > 4096) throw std::invalid_argument("model prefix texture count exceeds bound");
    auto position = std::size_t(128);
    for (std::uint32_t i = 0; i < texture_count; ++i) {
        const auto record = read_texture(bytes, position, false);
        position += 36 + record.pixels.size() + 1024;
        file.textures.push_back(record);
    }
    const std::size_t header_size = file.version == 5 ? 184 : 176;
    const std::size_t mesh_size = file.version == 5 ? 112 : file.version == 2 ? 88 : 104;
    if (position > bytes.size() || header_size > bytes.size() - position)
        throw std::invalid_argument("model prefix LOD header outside input");
    file.lod_header.assign(bytes.begin() + std::ptrdiff_t(position), bytes.begin() + std::ptrdiff_t(position + header_size));
    file.lod_data_offset = position + header_size;
    file.declared_lod_size = read_u32(bytes, position + 20);
    file.vertex_count = read_u32(bytes, position + 128);
    file.normal_count = read_u32(bytes, position + 136);
    file.face_count = read_u32(bytes, position + 144);
    file.mesh_count = read_u32(bytes, position + 152);
    file.hardpoint_count = read_u32(bytes, position + 160);
    file.material_count = read_u32(bytes, position + 168);
    std::uint64_t total = std::uint64_t(file.vertex_count) * 8 + std::uint64_t(file.normal_count) * 8 +
        std::uint64_t(file.face_count) * 72 + std::uint64_t(file.mesh_count) * mesh_size +
        std::uint64_t(file.hardpoint_count) * 12 + std::uint64_t(file.material_count) * 76;
    if (file.version == 5) total += std::uint64_t(read_u32(bytes, position + 176)) * 72;
    if (file.declared_lod_size != total)
        throw std::invalid_argument("model prefix LOD size differs from its counts");
    file.available_lod_bytes = bytes.size() - file.lod_data_offset;
    file.truncated = file.declared_lod_size > file.available_lod_bytes;
    return file;
}

} // namespace dfre
