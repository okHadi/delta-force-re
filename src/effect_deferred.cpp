// Deferred translucent-object pass, transcribed from the original
// functions. Code addresses in comments are Df.exe addresses; state
// addresses are original data addresses inside the flat buffer.
#include "dfre/effect_deferred.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
namespace dfre {
namespace {
std::int32_t absolute32(std::int32_t value) {
    const auto converted = std::int64_t(value);
    const auto magnitude = converted < 0 ? -converted : converted;
    return magnitude > 0x7fffffffLL ? std::int32_t(0x80000000u) : std::int32_t(magnitude);
}
// x87 imul + shrd: signed 64-bit product, right shift, low 32 bits.
std::int32_t product_shift(std::int32_t a, std::int32_t b, unsigned bits) {
    const auto value = std::uint64_t(std::int64_t(a) * std::int64_t(b));
    return std::int32_t(std::uint32_t(value >> bits));
}
// Original 0x49BAE6 sets x87 rounding toward zero for FRNDINT.
std::int32_t truncate_i32(long double value) {
    const auto rounded = std::trunc(value);
    if (rounded >= 2147483648.0L || rounded < -2147483648.0L) return std::int32_t(0x80000000u);
    return std::int32_t(rounded);
}
constexpr long double inverse_65536 = 1.0L / 65536.0L;
constexpr long double area_epsilon = 0.001L;
constexpr std::uint64_t reciprocal_numerator = 0x100000000000ULL;
}
DeferredPass::DeferredPass() {
    memory_.assign(memory_size, 0);
    // 0x46c390: shared row sentinel x and the 1/n reciprocal table.
    word(node_sentinel + 0x18) = 0x7fffffff;
    word(0xbc5a24) = 0;
    word(reciprocal_table) = 0x10000;
    for (std::int32_t divisor = 1; divisor <= 0x80; ++divisor)
        word(reciprocal_table + std::size_t(divisor) * 4) = 0x10000 / divisor;
}
std::int32_t& DeferredPass::word(std::size_t address) {
    if (address < memory_base || address + 4 > memory_base + memory_size)
        throw std::out_of_range("deferred state address outside the flat buffer: " + std::to_string(address));
    return *reinterpret_cast<std::int32_t*>(&memory_[address - memory_base]);
}
std::int32_t DeferredPass::word(std::size_t address) const {
    if (address < memory_base || address + 4 > memory_base + memory_size)
        throw std::out_of_range("deferred state address outside the flat buffer: " + std::to_string(address));
    return *reinterpret_cast<const std::int32_t*>(&memory_[address - memory_base]);
}
std::uint16_t& DeferredPass::short_word(std::size_t address) {
    if (address < memory_base || address + 2 > memory_base + memory_size)
        throw std::out_of_range("deferred state address outside the flat buffer: " + std::to_string(address));
    return *reinterpret_cast<std::uint16_t*>(&memory_[address - memory_base]);
}
std::uint16_t DeferredPass::short_word(std::size_t address) const {
    if (address < memory_base || address + 2 > memory_base + memory_size)
        throw std::out_of_range("deferred state address outside the flat buffer: " + std::to_string(address));
    return *reinterpret_cast<const std::uint16_t*>(&memory_[address - memory_base]);
}
std::uint32_t DeferredPass::material_word(const std::vector<std::uint8_t>& material, std::size_t offset) {
    std::uint32_t value = 0;
    for (std::size_t i = 0; i < 4; ++i) value |= std::uint32_t(material[offset + i]) << (i * 8);
    return value;
}
void DeferredPass::reset(const DeferredFrame& frame) {
    if (frame.clip_left > frame.clip_right || frame.clip_top > frame.clip_bottom)
        throw std::invalid_argument("deferred clip is reversed");
    if (frame.clip_top < 0 || frame.clip_bottom > 0x7ffd)
        throw std::invalid_argument("deferred clip rows are outside the row tables");
    if (frame.colour_row.size() < std::size_t(frame.clip_bottom) + 1)
        throw std::invalid_argument("deferred colour row table is short");
    frame_ = frame;
    // 0x46c3e0: clip, row heads, column heads, cursors and counters.
    word(0xbc5a2c) = frame.clip_left;
    word(0xbc5a30) = frame.clip_top;
    word(0xbc5a34) = frame.clip_right;
    word(0xbc5a38) = frame.clip_bottom;
    for (std::int32_t row = frame.clip_top; row <= frame.clip_bottom; ++row) {
        word(row_head_base + std::size_t(row) * 0x40) = std::int32_t(node_sentinel);
        word(column_head_base + std::size_t(row + 1) * 4) = 0;
    }
    word(0xbc5c64) = std::int32_t(record_pool);
    word(0xbc5c60) = std::int32_t(node_pool);
    for (std::size_t offset = 0xbc5c50; offset <= 0xbc5c5c; offset += 4) word(offset) = 0;
    word(0xbc5a24) = 1;
    word(0xbc5a3c) = std::int32_t(frame.distance_scale);
    word(0xbc5a28) = std::int32_t(frame.mode_flag);
    word(0xbc5c48) = std::int32_t(frame.table_a_address);
    word(0xbc5c44) = std::int32_t(frame.target_address);
    word(0xbc5c4c) = std::int32_t(frame.table_b_address);
    record_cursor_ = 0;
    node_cursor_ = 0;
    segment_cursor_ = 0;
    plans_.clear();
}
void DeferredPass::add_material(std::uint32_t address, const std::uint8_t* data, std::size_t size,
    std::uint32_t table_a, std::uint32_t table_b) {
    if (size < 0x4c) throw std::invalid_argument("deferred material record is short");
    std::vector<std::uint8_t> copy(data, data + size);
    // Two extra words keep the resolved per-LOD table addresses with the
    // material; the original reads them from 0xbfc100/0xbfc104 instead.
    copy.resize(std::max<std::size_t>(size, 0x68));
    for (std::size_t i = 0; i < 4; ++i) {
        copy[0x60 + i] = std::uint8_t(table_a >> (i * 8));
        copy[0x64 + i] = std::uint8_t(table_b >> (i * 8));
    }
    materials_[address] = std::move(copy);
}
const std::vector<std::uint8_t>& DeferredPass::material_of(std::uint32_t address) const {
    const auto found = materials_.find(address);
    if (found == materials_.end()) throw std::invalid_argument("deferred record material was not registered");
    return found->second;
}

// 0x46c4c0: reciprocal scale per vertex, degenerate test, plane gradients.
bool DeferredPass::prepare_record(std::array<DeferredVertex, 3>& vertices, std::size_t record) {
    for (auto& vertex : vertices) {
        const auto divisor = std::uint32_t(vertex.field[2]);
        if (!divisor) throw std::domain_error("deferred vertex depth is zero");
        const auto quotient = reciprocal_numerator / divisor;
        if (quotient > 0xffffffffULL)
            throw std::overflow_error("deferred depth reciprocal overflows the original divide");
        vertex.field[2] = std::int32_t(std::uint32_t(quotient));
        for (std::size_t k = 3; k < 6; ++k)
            vertex.field[k] = product_shift(std::int32_t(std::uint32_t(quotient)), vertex.field[k], 22);
    }
    const auto s50 = (vertices[1].field[0] - vertices[0].field[0]) * inverse_65536;
    const auto s68 = (vertices[1].field[1] - vertices[0].field[1]) * inverse_65536;
    const auto s40 = (vertices[2].field[0] - vertices[0].field[0]) * inverse_65536;
    const auto s58 = (vertices[2].field[1] - vertices[0].field[1]) * inverse_65536;
    const auto area = s50 * s58 - s68 * s40;
    if (std::abs(area) < area_epsilon) return false;
    const auto reciprocal = 1.0L / area;
    // Gradient pair 0x46c73b/0x46c5c2/0x46c640 for value differences a, b.
    const auto gradients = [&](std::int32_t a, std::int32_t b) {
        return std::array<std::int32_t, 2>{
            truncate_i32((b * s50 - a * s40) * reciprocal),
            truncate_i32((a * s58 - b * s68) * reciprocal)};
    };
    const auto flags = memory_[record - memory_base];
    if (flags & 4) {
        const auto pair = gradients(vertices[1].field[2] - vertices[0].field[2],
            vertices[2].field[2] - vertices[0].field[2]);
        word(record + 0x38) = pair[0];
        word(record + 0x34) = pair[1];
    }
    if (flags & 8) {
        const auto pair = gradients(vertices[1].field[5] - vertices[0].field[5],
            vertices[2].field[5] - vertices[0].field[5]);
        word(record + 0x50) = pair[0];
        word(record + 0x4c) = pair[1];
    } else {
        word(record + 0x4c) = 0;
        word(record + 0x50) = 0;
    }
    if (flags & 2) {
        const auto u = gradients(vertices[1].field[3] - vertices[0].field[3],
            vertices[2].field[3] - vertices[0].field[3]);
        word(record + 0x40) = u[0];
        word(record + 0x3c) = u[1];
        const auto v = gradients(vertices[1].field[4] - vertices[0].field[4],
            vertices[2].field[4] - vertices[0].field[4]);
        word(record + 0x48) = v[0];
        word(record + 0x44) = v[1];
    } else {
        for (std::size_t offset = 0x3c; offset <= 0x48; offset += 4) word(record + offset) = 0;
    }
    return true;
}

// 0x46c7e0: one scanline edge, clamped to the top clip row.
bool DeferredPass::build_edge(const std::array<DeferredVertex, 3>& vertices, std::size_t first,
    std::size_t second, std::size_t record, std::int32_t& start_row, std::int32_t& end_row) {
    const auto node = std::size_t(std::uint32_t(word(0xbc5c60)));
    std::size_t upper = first, lower = second;
    if (vertices[lower].field[1] > vertices[upper].field[1]) {
        word(node) = 1;
    } else {
        word(node) = 0;
        std::swap(upper, lower);
    }
    word(node + 4) = std::int32_t(record);
    const auto start = (vertices[upper].field[1] + 0x7fff) >> 16;
    const auto end = (vertices[lower].field[1] + 0x7fff) >> 16;
    if (end == start) return false;
    const auto derivative_x = vertices[lower].field[0] - vertices[upper].field[0];
    const auto derivative_y = vertices[lower].field[1] - vertices[upper].field[1];
    std::int32_t slope;
    if ((absolute32(derivative_x) >> 15) >= derivative_y)
        slope = std::int32_t(((std::int64_t(derivative_x >> 10) * 65536) / derivative_y) * 1024);
    else
        slope = std::int32_t(std::int64_t(derivative_x) * 0x10000 / derivative_y);
    word(node + 0x2c) = slope;
    const auto offset_y = std::int32_t((std::uint32_t(start) << 16) + 0x7fff -
        std::uint32_t(vertices[upper].field[1]));
    if (offset_y <= 0x100) {
        word(node + 0x18) = vertices[upper].field[0];
        word(node + 0x1c) = vertices[upper].field[2];
        word(node + 0x20) = vertices[upper].field[3];
        word(node + 0x24) = vertices[upper].field[4];
        word(node + 0x28) = vertices[upper].field[5];
    } else {
        const auto offset_x = product_shift(offset_y, slope, 16);
        word(node + 0x18) = vertices[upper].field[0] + offset_x;
        word(node + 0x1c) = vertices[upper].field[2] + product_shift(word(record + 0x38), offset_y, 16) +
            product_shift(word(record + 0x34), offset_x, 16);
        word(node + 0x20) = vertices[upper].field[3] + product_shift(word(record + 0x40), offset_y, 16) +
            product_shift(word(record + 0x3c), offset_x, 16);
        word(node + 0x24) = vertices[upper].field[4] + product_shift(word(record + 0x48), offset_y, 16) +
            product_shift(word(record + 0x44), offset_x, 16);
        word(node + 0x28) = vertices[upper].field[5] + product_shift(word(record + 0x50), offset_y, 16) +
            product_shift(word(record + 0x4c), offset_x, 16);
    }
    word(node + 0x30) = product_shift(slope, word(record + 0x34), 16) + word(record + 0x38);
    word(node + 0x34) = product_shift(slope, word(record + 0x3c), 16) + word(record + 0x40);
    word(node + 0x38) = product_shift(slope, word(record + 0x44), 16) + word(record + 0x48);
    word(node + 0x3c) = product_shift(slope, word(record + 0x4c), 16) + word(record + 0x50);
    auto row = start;
    while (row < word(0xbc5a30)) {
        word(node + 0x18) += word(node + 0x2c);
        word(node + 0x1c) += word(node + 0x30);
        word(node + 0x20) += word(node + 0x34);
        word(node + 0x24) += word(node + 0x38);
        word(node + 0x28) += word(node + 0x3c);
        ++row;
    }
    if (row < end) {
        start_row = row;
        end_row = end;
        return true;
    }
    return false;
}
// 0x46cac0 row list insert: ascending x, shared sentinel terminator.
void DeferredPass::insert_node(std::size_t row, std::size_t node) {
    std::size_t cursor = row_head_base - 8 + row * 0x40; // 0xad3ef0 + row*0x40
    std::size_t next = std::size_t(std::uint32_t(word(cursor + 8)));
    while (word(node + 0x18) > word(std::int32_t(next) + 0x18)) {
        cursor = next;
        next = std::size_t(std::uint32_t(word(cursor + 8)));
    }
    word(node + 0x0c) = std::int32_t(cursor);
    word(node + 0x08) = std::int32_t(next);
    word(std::int32_t(next) + 0x0c) = std::int32_t(node);
    word(cursor + 8) = std::int32_t(node);
}
// 0x46cac0: record, gradient prep and up to three edges.
std::size_t DeferredPass::add_face(std::uint32_t flags, const std::array<DeferredVertex, 3>& raw,
    std::uint32_t material_address) {
    if (record_cursor_ >= record_capacity) return 0;
    const auto record = record_at(record_cursor_);
    ++record_cursor_;
    word(0xbc5c64) = std::int32_t(record + 0x54);
    memory_[record - memory_base] |= 4;
    if (flags & 0x10) memory_[record - memory_base] |= 2;
    if (flags & 8) memory_[record - memory_base] |= 8;
    word(record + 0x10) = 0;
    word(record + 0x14) = std::int32_t(material_address);
    word(record + 0x04) = std::int32_t(flags);
    word(record + 0x18) = raw[0].field[5];
    const auto& material = material_of(material_address);
    auto vertices = raw;
    for (auto& vertex : vertices) {
        vertex.field[3] = std::int32_t(std::uint32_t(vertex.field[3]) <<
            ((material_word(material, 0x1e) >> 16) & 0x1f));
        vertex.field[4] = std::int32_t(std::uint32_t(vertex.field[4]) <<
            ((material_word(material, 0x20) >> 16) & 0x1f));
    }
    if (!prepare_record(vertices, record)) return 0;
    std::size_t kept = 0;
    std::size_t edge = 0;
    while (edge < 3) {
        const std::size_t second = edge + 1 < 3 ? edge + 1 : 0;
        std::int32_t start_row = 0, end_row = 0;
        if (build_edge(vertices, edge, second, record, start_row, end_row) &&
            start_row <= word(0xbc5a38)) {
            const auto node = std::size_t(std::uint32_t(word(0xbc5c60)));
            insert_node(std::size_t(std::uint32_t(start_row)), node);
            ++kept;
            if (end_row <= word(0xbc5a38) + 1) {
                word(node + 0x10) = word(column_head_base + std::size_t(std::uint32_t(end_row)) * 4);
                word(column_head_base + std::size_t(std::uint32_t(end_row)) * 4) = std::int32_t(node);
            }
            const auto cursor = std::size_t(std::uint32_t(word(0xbc5c60)));
            if (cursor < node_pool_end) {
                word(0xbc5c60) += 0x40;
                if (cursor + 0x40 <= node_pool_end) ++node_cursor_;
                ++edge;
                if (edge >= 3) return kept;
                continue;
            }
            return kept;
        }
        ++edge;
    }
    return kept;
}

// 0x46cc30: merge one row's edges into the global list.
void DeferredPass::merge_row(std::size_t row) {
    std::size_t node = std::size_t(std::uint32_t(word(row_head_base + row * 0x40)));
    if (node == node_sentinel) return;
    std::size_t cursor = header_node;
    for (;;) {
        const std::size_t next = std::size_t(std::uint32_t(word(cursor + 8)));
        if (word(node + 0x18) > word(std::int32_t(next) + 0x18)) {
            cursor = next;
            continue;
        }
        word(node + 0x0c) = std::int32_t(cursor);
        const auto saved = std::size_t(std::uint32_t(word(node + 8)));
        word(node + 0x08) = std::int32_t(next);
        word(std::int32_t(next) + 0x0c) = std::int32_t(node);
        word(cursor + 8) = std::int32_t(node);
        cursor = node;
        if (word(node) != 0) {
            const auto record = std::size_t(std::uint32_t(word(node + 4)));
            for (std::size_t k = 0; k < 5; ++k)
                word(record + 0x20 + k * 4) = word(node + 0x18 + k * 4);
        }
        if (saved == node_sentinel) return;
        node = saved;
    }
}
void DeferredPass::flush() {
    std::size_t segment = segment_pool;
    word(0xbb1178) = std::int32_t(global_sentinel);
    word(0xbb117c) = 0;
    word(0xbb1188) = std::int32_t(0xff9c0000u);
    word(0xbb1170) = 1;
    word(0xbb1174) = std::int32_t(frame_sentinel);
    word(0xae3ef8) = 0;
    word(0xae3ef0) = 0;
    word(0xae3ef4) = std::int32_t(frame_sentinel);
    word(0xae3efc) = std::int32_t(header_node);
    word(0xae3f08) = 0x8640000;
    std::memset(&memory_[frame_sentinel - memory_base], 0, 0x54);
    word(0xbc59dc) = std::int32_t(frame_sentinel);
    word(0xbc59d8) = std::int32_t(frame_sentinel);
    std::int32_t row = word(0xbc5a30);
    const std::int32_t bottom = word(0xbc5a38);
    while (row <= bottom) {
        merge_row(std::size_t(std::uint32_t(row)));
        short_word(0xbc59ec) = 1;
        short_word(0xbc59ee) = 0xff9c;
        word(0xbc59f4) = std::int32_t(0xc0000001u);
        word(0xbc5a04) = 0;
        for (std::size_t node = std::size_t(std::uint32_t(word(0xbb1178))); node != 0;
             node = std::size_t(std::uint32_t(word(node + 8)))) {
            const auto record = std::size_t(std::uint32_t(word(node + 4)));
            if (word(node) != 0) {
                // 0x46cd8c: start edge, counter low word and x pixels high.
                const auto counter = std::int16_t(short_word(record + 0x1c) + 1);
                short_word(record + 0x1c) = std::uint16_t(counter);
                if (counter != 1) continue;
                std::size_t head = std::size_t(std::uint32_t(word(0xbc59d8)));
                bool emit = false;
                for (;;) {
                    const auto interpolated = word(head + 0x24) +
                        product_shift(word(head + 0x34), word(node + 0x18) - word(head + 0x20), 16);
                    const auto difference = word(record + 0x24) - interpolated;
                    const bool choose = absolute32(difference) >= (word(record + 0x24) >> 8) ?
                        (word(record + 0x24) >= interpolated) :
                        (word(record + 0x34) > word(head + 0x34));
                    if (choose) {
                        emit = true;
                        break;
                    }
                    const auto threshold = word(record + 0x24) >> 8;
                    head = std::size_t(std::uint32_t(word(head + 8)));
                    const auto interpolated_next = word(head + 0x24) +
                        product_shift(word(head + 0x34), word(node + 0x18) - word(head + 0x20), 16);
                    const auto difference_next = word(record + 0x24) - interpolated_next;
                    const bool keep_walking = absolute32(difference_next) >= threshold ?
                        (word(record + 0x24) < interpolated_next) :
                        (word(record + 0x34) <= word(head + 0x34));
                    if (!keep_walking) break;
                }
                if (emit) {
                    const auto this_x = (word(node + 0x18) + 0x7fff) >> 16;
                    if (segment <= segment_pool_end) {
                        const auto count = this_x - (word(head + 0x1c) >> 16);
                        word(std::int32_t(segment) + 4) = count;
                        if (count > 0) {
                            const auto delta = std::int32_t((std::uint32_t(word(head + 0x1c)) & 0xffff0000u) + 0x7fffu - std::uint32_t(word(head + 0x20)));
                            word(segment) = word(head + 0x10);
                            word(head + 0x10) = std::int32_t(segment);
                            word(segment + 8) = word(head + 0x1c) >> 16;
                            word(segment + 0x0c) = row;
                            word(segment + 0x10) = word(head + 0x24) + product_shift(delta, word(head + 0x34), 16);
                            word(segment + 0x14) = word(head + 0x28) + product_shift(delta, word(head + 0x3c), 16);
                            word(segment + 0x18) = word(head + 0x2c) + product_shift(delta, word(head + 0x44), 16);
                            word(segment + 0x1c) = word(head + 0x30) + product_shift(delta, word(head + 0x4c), 16);
                            ++segment_cursor_;
                            segment += 0x20;
                        }
                    }
                    word(record + 0x08) = std::int32_t(head);
                    short_word(record + 0x1e) = std::uint16_t(this_x);
                    word(head + 0x0c) = std::int32_t(record);
                    word(0xbc59d8) = std::int32_t(record);
                    word(record + 0x0c) = std::int32_t(frame_sentinel);
                } else {
                    word(record + 0x08) = std::int32_t(head);
                    const auto previous = std::size_t(std::uint32_t(word(head + 0x0c)));
                    word(record + 0x0c) = std::int32_t(previous);
                    word(previous + 8) = std::int32_t(record);
                    word(head + 0x0c) = std::int32_t(record);
                }
            } else {
                // 0x46cfc5: end edge.
                const auto counter = std::int16_t(short_word(record + 0x1c) - 1);
                short_word(record + 0x1c) = std::uint16_t(counter);
                if (counter != 0) continue;
                const std::size_t head = std::size_t(std::uint32_t(word(0xbc59d8)));
                if (record == head) {
                    const auto this_x = (word(node + 0x18) + 0x7fff) >> 16;
                    const auto count = this_x - (word(head + 0x1c) >> 16);
                    word(std::int32_t(segment) + 4) = count;
                    if (count > 0 && segment < segment_pool_end) {
                        const auto delta = std::int32_t((std::uint32_t(word(record + 0x1c)) & 0xffff0000u) + 0x7fffu - std::uint32_t(word(record + 0x20)));
                        word(segment) = word(record + 0x10);
                        word(record + 0x10) = std::int32_t(segment);
                        word(segment + 8) = word(record + 0x1c) >> 16;
                        word(segment + 0x0c) = row;
                        word(segment + 0x10) = word(record + 0x24) + product_shift(delta, word(record + 0x34), 16);
                        word(segment + 0x14) = word(record + 0x28) + product_shift(delta, word(record + 0x3c), 16);
                        word(segment + 0x18) = word(record + 0x2c) + product_shift(delta, word(record + 0x44), 16);
                        word(segment + 0x1c) = word(record + 0x30) + product_shift(delta, word(record + 0x4c), 16);
                        ++segment_cursor_;
                        segment += 0x20;
                    }
                    short_word(std::uint32_t(word(record + 8)) + 0x1e) = std::uint16_t(this_x);
                }
                const auto previous = std::size_t(std::uint32_t(word(record + 0x08)));
                const auto following = std::size_t(std::uint32_t(word(record + 0x0c)));
                word(previous + 0x0c) = std::int32_t(following);
                word(following + 0x08) = std::int32_t(previous);
            }
        }
        // 0x46ce47: unlink this row's ending edges from the global list.
        for (std::size_t column = std::size_t(std::uint32_t(word(
                 column_head_base + std::size_t(std::uint32_t(row + 1)) * 4)));
             column != 0; column = std::size_t(std::uint32_t(word(column + 0x10)))) {
            const auto previous = std::size_t(std::uint32_t(word(column + 0x0c)));
            const auto following = std::size_t(std::uint32_t(word(column + 8)));
            word(previous + 8) = std::int32_t(following);
            word(following + 0x0c) = std::int32_t(previous);
        }
        // 0x46ce6d: advance the list and bubble left, copy dir-1 state.
        for (std::size_t node = std::size_t(std::uint32_t(word(0xbb1178)));;) {
            if (node == global_sentinel) break;
            const auto next = std::size_t(std::uint32_t(word(node + 8)));
            word(node + 0x18) += word(node + 0x2c);
            if (word(node) != 0) {
                word(node + 0x1c) += word(node + 0x30);
                word(node + 0x20) += word(node + 0x34);
                word(node + 0x24) += word(node + 0x38);
                word(node + 0x28) += word(node + 0x3c);
                const auto record = std::size_t(std::uint32_t(word(node + 4)));
                word(record + 0x20) = word(node + 0x18);
                word(record + 0x24) = word(node + 0x1c);
                word(record + 0x28) = word(node + 0x20);
                word(record + 0x2c) = word(node + 0x24);
                word(record + 0x30) = word(node + 0x28);
            }
            for (;;) {
                const auto previous = std::size_t(std::uint32_t(word(node + 0x0c)));
                if (word(node + 0x18) >= word(previous + 0x18)) break;
                const auto following = std::size_t(std::uint32_t(word(node + 8)));
                word(previous + 8) = std::int32_t(following);
                word(following + 0x0c) = std::int32_t(previous);
                const auto earlier = std::size_t(std::uint32_t(word(previous + 0x0c)));
                word(earlier + 8) = std::int32_t(node);
                word(node + 0x0c) = std::int32_t(earlier);
                word(node + 0x08) = std::int32_t(previous);
                word(previous + 0x0c) = std::int32_t(node);
            }
            node = next;
        }
        ++row;
    }
    word(std::int32_t(segment) + 8) = -1;
}

// 0x46d190: build the span plan for every record segment.
const std::vector<DeferredSpan>& DeferredPass::draw_spans() {
    plans_.clear();
    std::int32_t last_du = 0, last_dv = 0, last_dz = 0, last_dw = 0;
    std::size_t record = record_pool;
    while (record != record_pool + record_cursor_ * 0x54) {
        ++word(0xbc5c50);
        if (word(record + 0x10) != 0) {
            const auto& material = material_of(std::uint32_t(word(record + 0x14)));
            EffectSpanContext context;
            context.colour = material_word(material, 0x14);
            context.field_04 = std::uint32_t(word(record + 0x18));
            context.column_mask = material_word(material, 0x22) >> 16;
            context.row_mask = material_word(material, 0x24) >> 16;
            context.row_shift = material_word(material, 0x1e) >> 16;
            context.field_18 = material_word(material, 0x20) >> 16;
            const auto texture = material_word(material, 0x44);
            const auto table_a = material_word(material, 0x60);
            context.texture = reinterpret_cast<const std::uint8_t*>(std::uintptr_t(texture));
            context.blend_table = reinterpret_cast<const std::uint32_t*>(std::uintptr_t(table_a));
            context.field_2c = material_word(material, 0x64);
            const std::array<std::int32_t, 4> steps{word(record + 0x34), word(record + 0x3c),
                word(record + 0x44), word(record + 0x4c)};
            for (std::size_t segment = std::size_t(std::uint32_t(word(record + 0x10))); segment != 0;
                 segment = std::size_t(std::uint32_t(word(segment)))) {
                auto z = word(segment + 0x10);
                auto u = word(segment + 0x14);
                auto v = word(segment + 0x18);
                auto w = word(segment + 0x1c);
                auto x = word(segment + 8);
                auto count = word(segment + 4);
                const auto segment_row = std::uint32_t(word(segment + 0x0c));
                while (x < word(0xbc5a2c)) {
                    z += steps[0];
                    u += steps[1];
                    v += steps[2];
                    w += steps[3];
                    ++x;
                    --count;
                }
                word(segment + 4) = count;
                if (count <= 0) continue;
                const auto divide = [](std::int32_t value) {
                    if (value <= 0x2000) return std::int32_t(0x80000000u);
                    return std::int32_t(std::uint32_t(reciprocal_numerator / std::uint32_t(value)));
                };
                const auto scaled = [](std::int32_t value, std::int32_t reciprocal) {
                    return product_shift(value, reciprocal, 22);
                };
                auto depth = divide(z);
                auto su = scaled(u, depth), sv = scaled(v, depth), sw = scaled(w, depth);
                // Original 0x46d488 keeps the raw segment x for the pixel
                // base; the left clamp only advances values and count.
                auto pixel = std::int32_t(frame_.colour_row[segment_row] + std::uint32_t(word(segment + 8)));
                auto remaining = count;
                while (remaining > 0x20) {
                    z = std::int32_t(std::uint32_t(z) + (std::uint32_t(steps[0]) << 5));
                    u = std::int32_t(std::uint32_t(u) + (std::uint32_t(steps[1]) << 5));
                    v = std::int32_t(std::uint32_t(v) + (std::uint32_t(steps[2]) << 5));
                    w = std::int32_t(std::uint32_t(w) + (std::uint32_t(steps[3]) << 5));
                    const auto next_depth = divide(z);
                    const auto next_u = scaled(u, next_depth);
                    const auto next_v = scaled(v, next_depth);
                    const auto next_w = scaled(w, next_depth);
                    last_du = (next_u - su) >> 5;
                    last_dv = (next_v - sv) >> 5;
                    last_dz = (next_depth - depth) >> 5;
                    last_dw = (next_w - sw) >> 5;
                    plans_.push_back({{pixel, 0x20, su, sv, depth, sw, last_du, last_dv, last_dz, last_dw},
                        context, std::uint32_t(word(record + 4)), segment_row,
                        std::uint32_t((record - record_pool) / 0x54)});
                    depth = next_depth;
                    su = next_u;
                    sv = next_v;
                    sw = next_w;
                    pixel += 0x20;
                    remaining -= 0x20;
                }
                if (remaining > 1) {
                    const auto factor = word(reciprocal_table + std::size_t(remaining) * 4);
                    const auto next_depth = divide(z + steps[0] * remaining);
                    last_du = product_shift(scaled(u + steps[1] * remaining, next_depth) - su, factor, 16);
                    last_dv = product_shift(scaled(v + steps[2] * remaining, next_depth) - sv, factor, 16);
                    last_dz = product_shift(next_depth - depth, factor, 16);
                    last_dw = product_shift(scaled(w + steps[3] * remaining, next_depth) - sw, factor, 16);
                }
                plans_.push_back({{pixel, remaining, su, sv, depth, sw, last_du, last_dv, last_dz, last_dw},
                    context, std::uint32_t(word(record + 4)), segment_row,
                    std::uint32_t((record - record_pool) / 0x54)});
                ++word(0xbc5c58);
                word(0xbc5c5c) += count;
            }
            ++word(0xbc5c54);
        }
        record += 0x54;
    }
    return plans_;
}
std::size_t DeferredPass::draw(std::uint32_t width, std::uint32_t height,
    const DeferredContextResolver& resolve) {
    if (plans_.empty()) draw_spans();
    std::size_t writes = 0;
    const auto plane = std::uint64_t(width) * height;
    for (const auto& plan : plans_) {
        const auto selection = effect_span_selection(plan.face_flags);
        if (selection == EffectSpanSelection::stateful_previous)
            throw std::invalid_argument("deferred span uses a stateful effect key");
        const auto mode = selection == EffectSpanSelection::no_write ? EffectSpanMode::no_write :
            selection == EffectSpanSelection::alpha_blend ? EffectSpanMode::alpha_blend :
            EffectSpanMode::half_blend;
        // The original kernels write without a plane check; a span that
        // runs past the plane only matters inside it, so clip to it.
        auto span = plan.span;
        if (span.pixel < 0) {
            const auto skip = std::min(std::int64_t(-span.pixel), std::int64_t(span.length));
            span.pixel = 0;
            span.length -= std::int32_t(skip);
            span.u = std::int32_t(std::uint32_t(span.u) + std::uint32_t(skip) * std::uint32_t(span.du));
            span.v = std::int32_t(std::uint32_t(span.v) + std::uint32_t(skip) * std::uint32_t(span.dv));
            span.depth = std::int32_t(std::uint32_t(span.depth) + std::uint32_t(skip) * std::uint32_t(span.ddepth));
            span.shade = std::int32_t(std::uint32_t(span.shade) + std::uint32_t(skip) * std::uint32_t(span.dshade));
        }
        if (std::uint64_t(std::uint32_t(span.pixel)) >= plane) continue;
        const auto available = std::int64_t(plane) - span.pixel;
        if (span.length > available) span.length = std::int32_t(available);
        writes += draw_effect_span32(span, mode, resolve(plan), width, height);
    }
    return writes;
}
} // namespace dfre
