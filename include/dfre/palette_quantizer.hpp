#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace dfre {
// Original texture histogram weight before the six-bit histogram reduction.
std::int32_t palette_frequency_weight(std::uint32_t count,std::uint32_t total);
struct PaletteBox6 {
    double variance{};
    std::array<double,5> moments{}; // Weight, squared distance, R, G, B.
    std::array<std::int32_t,3> minimum{},maximum{};
    bool splittable{};
};
class PaletteQuantizer6 {
public:
    PaletteQuantizer6();
    // Original reset keeps prior boxes for the next palette bank.
    void clear();
    void add_color(const std::array<std::uint8_t,3>& gamma_rgb,std::int32_t weight=65536);
    std::vector<std::uint8_t> build(std::size_t colors=254);
    std::size_t active_boxes() const noexcept{return active_;}
    std::uint32_t occupied_bins() const noexcept{return occupied_;}
    std::uint32_t maximum_weight() const noexcept{return maximum_;}
    const std::array<PaletteBox6,256>& boxes() const noexcept{return boxes_;}
private:
    std::vector<std::uint32_t> histogram_;
    std::array<PaletteBox6,256> boxes_{};
    std::size_t active_{};
    std::uint32_t occupied_{},maximum_{};
    bool built_{};
    void trim(PaletteBox6& box) const;
    std::array<double,5> moments(const PaletteBox6& box) const;
    void split(std::size_t index);
};
}
