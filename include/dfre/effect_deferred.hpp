#pragma once
// Deferred translucent-object pass.
// Anchors: Df.exe 0x46c390 (one-time init), 0x46c3e0 (frame reset),
// 0x46cac0 (add face), 0x46c4c0 (gradient prep), 0x46c7e0 (edge build),
// 0x46cc30 (row merge and span build), 0x46d190 (draw plan).
// Original executable SHA-256
// abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff.
#include "dfre/effects.hpp"
#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <vector>

namespace dfre {

// One edge node, 0x40 bytes in the original node pool.
struct DeferredNode {
    std::int32_t direction{};              // +0x00
    std::uint32_t record_address{};        // +0x04 pool address of the record
    std::int32_t next{}, prev{};           // +0x08, +0x0c list links
    std::int32_t column_next{};            // +0x10
    std::int32_t field14{};                // +0x14
    std::int32_t x{};                      // +0x18
    std::array<std::int32_t, 4> value{};   // +0x1c..+0x28
    std::array<std::int32_t, 5> step{};    // +0x2c..+0x3c
};
// One face record, 0x54 bytes in the original record pool.
struct DeferredRecord {
    std::uint8_t flags{};                  // +0x00
    std::uint32_t face_flags{};            // +0x04
    std::int32_t next{}, prev{};           // +0x08, +0x0c
    std::int32_t span_head{};              // +0x10 segment chain (pool address)
    std::uint32_t material{};              // +0x14 stored material address
    std::uint32_t field18{};               // +0x18 first vertex field 5
    std::uint16_t counter{}, x_pixel{};    // +0x1c low word, +0x1e high word
    std::int32_t x{};                      // +0x20
    std::array<std::int32_t, 4> value{};   // +0x24..+0x30
    std::array<std::int32_t, 8> gradient{}; // +0x34..+0x50
};
// One span segment, 8 dwords, value order zrecip, u, v, w.
struct DeferredSegment {
    std::int32_t next{};
    std::int32_t count{};
    std::int32_t x{};
    std::int32_t row{};
    std::array<std::int32_t, 4> value{};
};
// One raw face vertex: x, y, z, u, v, w (six int32).
struct DeferredVertex {
    std::array<std::int32_t, 6> field{};
};
// Frame limits taken from the original target record: clip at +0x34,
// depth mode at +0x50, colour row table at +0x68.
struct DeferredFrame {
    std::int32_t clip_left{}, clip_top{}, clip_right{}, clip_bottom{};
    std::uint32_t depth_mode{};            // 0x20 selects four-byte pixels
    std::vector<std::uint32_t> colour_row; // row start in colour pixels
    // Values the original 0x46c3e0 stores for the draw stage.
    std::uint32_t target_address{};
    std::uint32_t distance_scale{0x10000};
    std::uint32_t mode_flag{};
    std::uint32_t table_a_address{};
    std::uint32_t table_b_address{};
};
// One emitted span with the raw original context values and face flags.
struct DeferredSpan {
    EffectSpan span;
    EffectSpanContext context; // texture and blend_table keep raw addresses
    std::uint32_t face_flags{};
    std::uint32_t row{};
    std::uint32_t record{};
};
// Resolves raw context addresses to real pointers for image buffers.
using DeferredContextResolver = std::function<EffectSpanContext(const DeferredSpan&)>;

// Port of the original deferred pass. Pools, lists and counters keep the
// original sizes: 1000 records, 3000 nodes, one shared row sentinel.
class DeferredPass {
public:
    static constexpr std::size_t record_capacity = 1000;
    static constexpr std::size_t node_capacity = 3000;
    // Original sentinel address values, exposed for replay comparisons.
    static constexpr std::uint32_t frame_sentinel_address = 0xbc59d0;
    static constexpr std::uint32_t node_sentinel_address = 0xb82330;
    static constexpr std::uint32_t global_sentinel_address = 0xae3ef0;
    static constexpr std::uint32_t header_node_address = 0xbb1170;
    static constexpr std::uint32_t record_pool_address = 0xbb11b0;
    static constexpr std::uint32_t node_pool_address = 0xb82370;
    static constexpr std::uint32_t segment_pool_address = 0xae3f30;
    static constexpr std::uint32_t segment_pool_end = 0xb80330;
    static constexpr std::uint32_t state_base = 0xad3ef0;
    static constexpr std::uint32_t state_size = 0xbc5c68 - 0xad3ef0;

    DeferredPass();
    // Original 0x46c3e0. Throws when the clip or row table is invalid.
    void reset(const DeferredFrame& frame);
    // Material record bytes plus the two per-LOD table pointers the
    // original reads from 0xbfc100/0xbfc104 through material +0x18.
    void add_material(std::uint32_t address, const std::uint8_t* data, std::size_t size,
        std::uint32_t table_a, std::uint32_t table_b);
    // Original 0x46cac0. Returns the number of edges kept (0 to 3).
    std::size_t add_face(std::uint32_t flags, const std::array<DeferredVertex, 3>& vertices,
        std::uint32_t material_address);
    // Original 0x46cc30.
    void flush();
    // Original 0x46d190. Stores and returns the span plan; no pixels.
    const std::vector<DeferredSpan>& draw_spans();
    // Plan plus pixel writes through the public effect span kernels.
    // Non-effect modes are refused: those kernels stay with the renderer.
    // The resolver maps the raw context values to real buffers.
    std::size_t draw(std::uint32_t width, std::uint32_t height, const DeferredContextResolver& resolve);

    // State for tests and the replay check.
    const std::vector<std::uint8_t>& state() const { return memory_; }
    std::size_t record_count() const { return record_cursor_; }
    std::size_t node_count() const { return node_cursor_; }
    std::size_t segment_count() const { return segment_cursor_; }
    std::size_t span_records() const { return std::size_t(std::uint32_t(word(0xbc5c58))); }
    std::size_t span_pixels() const { return std::size_t(std::uint32_t(word(0xbc5c5c))); }
    std::size_t record_visits() const { return std::size_t(std::uint32_t(word(0xbc5c54))); }
    std::size_t slot_visits() const { return std::size_t(std::uint32_t(word(0xbc5c50))); }
    const std::vector<DeferredSpan>& plan() const { return plans_; }
    const DeferredFrame& frame() const { return frame_; }

private:
    static constexpr std::size_t memory_base = state_base;
    static constexpr std::size_t memory_size = state_size;
    static constexpr std::size_t row_head_base = 0xad3ef8;    // +0x40 per row
    static constexpr std::size_t column_head_base = 0xb8132c; // +4 per row
    static constexpr std::size_t node_pool = node_pool_address;
    static constexpr std::size_t node_pool_end = header_node_address;
    static constexpr std::size_t header_node = header_node_address;
    static constexpr std::size_t record_pool = record_pool_address;
    static constexpr std::size_t frame_sentinel = frame_sentinel_address;
    static constexpr std::size_t global_sentinel = global_sentinel_address;
    static constexpr std::size_t node_sentinel = node_sentinel_address;
    static constexpr std::size_t segment_pool = segment_pool_address;
    static constexpr std::size_t reciprocal_table = 0xbc5a40;

    std::int32_t& word(std::size_t address);
    std::int32_t word(std::size_t address) const;
    std::uint16_t& short_word(std::size_t address);
    std::uint16_t short_word(std::size_t address) const;
    std::size_t record_at(std::size_t index) const { return record_pool + index * 0x54; }
    std::size_t node_at(std::size_t index) const { return node_pool + index * 0x40; }
    std::size_t segment_at(std::size_t index) const { return segment_pool + index * 32; }
    static std::uint32_t material_word(const std::vector<std::uint8_t>& material, std::size_t offset);
    const std::vector<std::uint8_t>& material_of(std::uint32_t address) const;
    bool prepare_record(std::array<DeferredVertex, 3>& vertices, std::size_t record);
    bool build_edge(const std::array<DeferredVertex, 3>& vertices, std::size_t first,
        std::size_t second, std::size_t record, std::int32_t& start_row, std::int32_t& end_row);
    void insert_node(std::size_t row, std::size_t node);
    void merge_row(std::size_t row);
    void append_plan(std::vector<DeferredSpan>& plan, const DeferredSpan& span) const;

    std::vector<std::uint8_t> memory_;
    std::vector<DeferredSpan> plans_;
    DeferredFrame frame_;
    std::map<std::uint32_t, std::vector<std::uint8_t>> materials_;
    std::size_t record_cursor_{};
    std::size_t node_cursor_{};
    std::size_t segment_cursor_{};
};

} // namespace dfre
