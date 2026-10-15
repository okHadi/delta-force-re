#pragma once
#include "dfre/game.hpp"
#include "dfre/menu.hpp"
#include "dfre/profiles.hpp"
#include <optional>

namespace dfre {
class SoldierMenu {
public:
    SoldierMenu(MenuRuntime& menu,std::filesystem::path game_folder,std::filesystem::path save_folder);
    void enter();
    std::optional<GameScreen> actions(const MenuAction& action);
    bool key(int virtual_key);
    bool text(std::string_view utf8);
    void prepare_draw();
    const std::vector<std::uint32_t>& draw();
    ProfileStore& profiles() noexcept{return profiles_;}
    std::size_t selected() const noexcept{return selected_;}
private:
    MenuRuntime& menu_;
    AssetStore assets_;
    ProfileStore profiles_;
    std::array<PcxImage,PlayerProfile::portrait_count> portraits_;
    std::size_t selected_=0;
    std::optional<std::size_t> preview_;
    enum class Edit {none,create,rename};
    Edit edit_=Edit::none;
    bool confirm_=false;
    bool initialized_=false;
    std::string input_;
    void start_edit(Edit mode);
    void finish_edit(bool commit);
    void owner_draw(const MenuAction& action);
    void paragraph(int x,int y,int width,int height,std::string_view text);
};
}
