#pragma once

#include "dfre/assets.hpp"

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace dfre {

struct MenuFont {
    std::string name, filename, normal_color, lit_color, disabled_color;
};
struct MenuImage {
    std::string name, filename;
    int flags = 0, frames = 0, x = 0, y = 0;
    bool preload = false;
};
struct MenuButton {
    std::string name, text_key, handler, linked_button, hover_handler, font, image, draw_handler;
    int type = 0, attributes = 0, x = 0, y = 0;
    int slider_size = 0, slider_position = 0, width = 0, height = 0;
    int state = 1, control_state = 0, hotkey = 0, user_id = 0;
};
struct MenuDefinition {
    std::string name, background, font, accept_handler, accept_button, back_handler, back_button;
    std::array<std::string, 4> arrow_buttons;
    std::string string_group;
    std::vector<std::string> buttons;
};
struct MenuData {
    std::unordered_map<std::string, std::uint32_t> colors;
    std::unordered_map<std::string, MenuFont> fonts;
    std::unordered_map<std::string, MenuImage> images;
    std::unordered_map<std::string, MenuButton> buttons;
    std::unordered_map<std::string, MenuDefinition> menus;
    std::vector<std::string> menu_order;
};

MenuData parse_menu(const AssetBytes& data);

class RtxtTable {
public:
    explicit RtxtTable(const AssetBytes& data);
    std::string get(std::string_view group, std::string_view key) const;
    std::string get(std::string_view key) const;
    std::size_t size() const noexcept { return count_; }
private:
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> groups_;
    std::vector<std::string> order_;
    std::size_t count_ = 0;
};

struct FontGlyph {
    std::uint16_t x = 0, width = 0;
    std::uint8_t height = 0, valid = 0;
    std::uint16_t y = 0;
};
struct FontAtlas {
    PcxImage image;
    std::array<FontGlyph, 256> glyphs{};
    std::uint16_t baseline = 0;
    std::uint8_t glyph_count = 0;
};
FontAtlas scan_font(PcxImage image);

struct MenuAction {
    std::string button, handler;
    int action = 0;
};

// Pixel words are 0xFFRRGGBB; game behavior is supplied through returned actions.
class MenuRuntime {
public:
    explicit MenuRuntime(const AssetStore& assets);
    ~MenuRuntime();
    MenuRuntime(MenuRuntime&&) noexcept;
    MenuRuntime& operator=(MenuRuntime&&) noexcept;
    MenuRuntime(const MenuRuntime&) = delete;
    MenuRuntime& operator=(const MenuRuntime&) = delete;

    const MenuData& definitions() const noexcept;
    const RtxtTable& strings() const noexcept;
    void select_menu(std::string_view name);
    const MenuDefinition& menu() const;
    MenuButton& button(std::string_view name);
    std::string button_text(std::string_view name) const;
    void set_button_text(std::string_view name, std::string text);
    void set_button_state(std::string_view name, int state);
    void set_button_value(std::string_view name, int value);
    std::string hit_test(int x, int y);

    std::vector<MenuAction> pointer_move(int x, int y);
    std::vector<MenuAction> pointer_down(int x, int y);
    std::vector<MenuAction> pointer_up(int x, int y);
    std::vector<MenuAction> key(int virtual_key);
    std::vector<MenuAction> draw_actions() const;
    const std::vector<std::uint32_t>& render();
    const std::vector<std::uint32_t>& render(const std::function<void(const MenuAction&)>& owner_draw);
    void draw_image(std::string_view name, int x, int y, int frame = 0,
                    int frame_count = 1, bool blend = false, bool transparent = false, bool half_blend = false);
    const FontAtlas& font_atlas(std::string_view name);
    void draw_pcx(const PcxImage& image,int x,int y,bool transparent=false);
    void draw_text(std::string_view font,int x,int y,std::string_view text,std::uint32_t color);
    void draw_line(int x1,int y1,int x2,int y2,std::uint32_t color);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace dfre
