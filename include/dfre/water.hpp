#pragma once

#include "dfre/terrain.hpp"

#include <array>
#include <vector>

namespace dfre {

// Original 32-bit water rendering (Df.exe 0x48D040 and 0x48D570) with its
// palette, mip and row helpers.
// Df.exe sha256 abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff.
//
// Original 0x485610 runs the water stage at the end of the frame, after sky,
// terrain and model passes:
//   0x48570C CALL 0x48D040   water plane below the camera (0xC971A4 horizon start)
//   0x485721 CALL 0x48D570   water plane at or above the camera (camera top start)
// render_water_frame32 keeps that order. Water pixels never write the depth
// buffer.
//
// The blend table ("water CLUT", pointer 0xBF67F8) is built by the water
// section of the 32-bit palette builder 0x47C6A0: a 16-color ramp from the
// mission water palette, mixed by water opacity against the existing pixel
// luminance byte. The wave tables at 0xC92AA0 (sine) and 0x500D58 (cosine
// window) are built by 0x489440.

// 0x48D040 row depth table entries: 0x7D00000 marks a row without terrain.
constexpr std::uint32_t water_row_no_terrain = 0x7d00000;

// Sine table of 0x489440 at 0xC92AA0, 1280 entries, truncated like the
// original integer conversion. The scale shift is the original 0xBD5270
// value (default 4): value = trunc(sin(i * 2*pi/1024) * 2^(22 - shift)).
// Shifts above 22 wrap the original x86 SHL; one table entry then differs
// between x87 FSIN and host libm. Stock terrain configs use the default 4.
std::array<std::int32_t, 1280> build_water_wave_table(unsigned scale_shift = 4);

// 0x47C6A0 water CLUT: 64 texel columns by 256 luminance rows, index
// texel * 256 + luminance. Values pack luminance, red, green, blue.
struct WaterPalette32 {
    std::vector<std::uint32_t> blends;
};

// Water palette asset use: the original copies the first 48 bytes (16 colors)
// of the mission water palette; opacity is the fixed 16.16 water_opacity value
// from the terrain config. Brightness, saturation and filter are the same
// control used for build_terrain_palettes32.
WaterPalette32 build_water_palette32(const AssetBytes& water_palette_rgb, std::uint32_t opacity,
    const TerrainColorControl& control = {});

struct WaterScene32 {
    std::array<AssetBytes, 8> texels; // 256, 128, ..., 2 square texel maps, values 0 through 63
    WaterPalette32 palette;
    std::int32_t level = 0; // Original fixed-point water plane (water_height * 16384); zero disables water.
    std::uint32_t opacity = 0; // Original 16.16 water_opacity.
    unsigned scroll_shift = 23; // Low byte of the original turbidity value; default 0x17.
    unsigned wave_scale_shift = 4; // Original 0xBD5270 sine table scale; default 4.
    std::int32_t horizon_scale = 65536; // Original horizon scale used for the start row.
};

// Builds water texel mips with the original 0x47D920 kernel and the water CLUT.
// The original water CLUT does not use the gamma table: it feeds raw water
// palette bytes into the 0x480CA0 color transform.
WaterScene32 prepare_water_scene32(const TerrainScene32& scene, const TerrainColorControl& control = {});

struct WaterPass32 {
    // Original 0xC93EA0 row values, at least camera.height entries. Rows above
    // the visible window or without terrain use water_row_no_terrain. The
    // exact table comes from build_water_row_depths (original 0x489260).
    std::vector<std::uint32_t> row_depths;
    std::uint32_t time = 0; // Original 0xCBEDDC scene frame time (0x48D040 scroll).
    std::uint32_t timer = 0; // Original 0x491014 millisecond clock (0x48D570 scroll).
};

// Original terrain cache row fields consumed by 0x489260: the cache entry row
// top [entry+0xCB89F0] and plane depth [entry+0xCB89D0]. Entries arrive in
// terrain cache order.
struct WaterTerrainRowDepth {
    std::int32_t top_row = 0;
    std::uint32_t depth = 0;
};

// 0x489260: fills the 0xC93EA0 row depth table from ordered terrain cache
// entries for the camera window, then marks rows above the covered region as
// water_row_no_terrain.
std::vector<std::uint32_t> build_water_row_depths(const std::vector<WaterTerrainRowDepth>& entries,
    const VoxelCamera& camera);

// Original 0x48D040: draws the water plane below the camera, from the window
// bottom up to the horizon row.
void render_water_pass32(const WaterScene32& scene, const VoxelCamera& camera, const WaterPass32& pass,
    std::vector<std::uint32_t>& pixels, std::vector<std::uint16_t>& depths);

// Original 0x48D570: draws the water plane at or above the camera, from the
// window top down. Pixels behind the plane and rows past an early stop get the
// flat blend table column 0, like the original.
void render_water_companion32(const WaterScene32& scene, const VoxelCamera& camera, const WaterPass32& pass,
    std::vector<std::uint32_t>& pixels, std::vector<std::uint16_t>& depths);

// Original frame order (0x48D040 at 0x48570C, then 0x48D570 at 0x485721).
void render_water_frame32(const WaterScene32& scene, const VoxelCamera& camera, const WaterPass32& pass,
    std::vector<std::uint32_t>& pixels, std::vector<std::uint16_t>& depths);

} // namespace dfre
