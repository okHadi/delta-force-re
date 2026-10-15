#pragma once
#include "dfre/model_palette.hpp"
#include "dfre/models.hpp"
#include "dfre/renderer.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace dfre {

// One loaded texture record prepared for the 32-bit renderer.
// Fields follow original 485BA0 storage (0xc54488 + slot * 0x24) and the 486880 remap pass.
// The 485BA0 name/size share cache is off in the shipped executable: 0xc54484 is zero at load
// and has no writer (only the read at 0x485c74), so no record gets flag 0x80.
struct ModelPreparedTexture32 {
    std::string name;
    std::uint32_t pixel_count{};
    std::uint16_t sequence{}; // Full stored sequence; 485E30 matches this against the material key.
    std::uint8_t flags{};     // Stored flags byte: level bits, 0x10 skip, 0x20 cutout, 0x80 skip.
    std::uint8_t bank{};      // Loader bank byte written at record +0x17.
    std::uint16_t width{}, height{};
    std::uint8_t width_bits{}, height_bits{};      // Highest set bit, as material +0x20 / +0x22.
    std::array<std::uint8_t, 768> palette_rgb{};   // Stored source RGB (3-byte entries).
    std::array<std::uint8_t, 256> palette_remap{}; // 486880 table; zero when the record is skipped.
    std::uint8_t most_frequent{};                  // Most frequent nonzero remapped texel index.
    std::vector<std::uint8_t> pixels;              // Loaded texels; remapped in place by 486880.
    bool remapped{};                               // True when a 486880 pass rewrote these pixels.
};

// One material after original 485E30 load writes and the 486880 bank pass.
struct ModelPreparedMaterial32 {
    std::array<std::uint8_t, 76> raw{}; // Stored file bytes plus original runtime writes.
    std::uint32_t flags{};              // Copy of raw[16..19].
    std::uint16_t bank{};               // Copy of raw[26..27].
    std::optional<std::size_t> texture; // Slot in ModelPreparedModel32::textures.
    std::uint8_t flat_index{};          // raw[20] as 486880 leaves it.
    std::uint32_t bound_colour{};       // raw[20..23]: flat index or 0x8000 effect colour.
    bool has_texture() const noexcept { return (flags & 1u) != 0; }
    bool cutout() const noexcept { return (flags & 0x200u) != 0; }
    bool depth_bias() const noexcept { return (flags & 0x40u) != 0; }
    bool hidden() const noexcept { return (flags & 0x800u) != 0; }
    bool effect() const noexcept { return (flags & 0x8000u) != 0; }
};

struct ModelPreparedLod32 {
    std::vector<ModelPreparedMaterial32> materials;
};

struct ModelPreparedModel32 {
    std::uint8_t bank{};
    unsigned texture_level{};
    std::vector<ModelPreparedTexture32> textures; // Level-filtered load span in file order.
    std::vector<ModelPreparedLod32> lods;
    const ModelPaletteBank32* palette{}; // Bank colours and shade rows; must outlive this value.
    const ModelPreparedMaterial32* material(std::size_t lod, std::size_t index) const;
    // Copies one source LOD with this model's prepared material fields applied. Geometry, meshes,
    // faces, normals, and the source stay unchanged.
    ModelLod apply(std::size_t lod_index, const ModelLod& source) const;
    // Renderer view. The first call for a textured material copies that texture and the bank
    // shade rows into a cached RasterTexture32; returned pointers stay valid for this lifetime.
    // tiny_colour is the flat table at 0xbfc108 = shade row 32 (unlit), proved by 4809B0.
    RasterMaterial32 raster_material(std::size_t lod, std::size_t index) const;

private:
    mutable std::vector<std::optional<RasterTexture32>> raster_cache_;
};

// Copies one LOD with prepared runtime material fields applied. prepare_model_face() reads
// material flags, raw, frame_count, and animation_parameter from a LOD material list, so this
// replacement carries original 485E30/486880 cutout, missing-texture, effect, and animation
// state into face routing. Both material lists must have the same size. Geometry is preserved.
ModelLod apply_model_bindings32(const ModelLod& lod, const ModelPreparedLod32& prepared);

// Loaded texture slots for one level: original 485BA0 loads only flags bit 1 << level.
std::vector<std::size_t> model_texture_span32(const Model& model, unsigned texture_level);

// Original 485E30 material writes plus 486880 texture remap and flat bindings for one bank.
// The source model stays unchanged. The bank palette supplies matching_rgb (what 480600 builds
// from the display palette) and the 64 shade rows from 4809B0 (480490 rows 0..31 plus 4801B0
// rows 32..63, whose first row is the flat and tiny table at 0xbfc108 = light + 0x8000).
ModelPreparedModel32 prepare_model_bindings32(const Model& model, std::uint8_t bank,
    unsigned texture_level, const ModelPaletteBank32& palette, const ModelGammaTable& gamma);

} // namespace dfre
