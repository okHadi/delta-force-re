#include "dfre/game.hpp"

#include "dfre/assets.hpp"
#include "dfre/menu.hpp"
#include "dfre/platform.hpp"
#include "dfre/state_loop.hpp"
#include "dfre/soldier_menu.hpp"

#include <array>
#include <iostream>
#include <map>
#include <stdexcept>

namespace dfre {
namespace {

class Game {
public:
    explicit Game(const GameOptions& options)
        : options_(options), assets_(options.game_folder), menu_(assets_),
          soldier_(menu_, options.game_folder, options.save_folder),
          window_(create_game_window("Delta Force RE", 640, 480)) {
        const std::array<std::pair<GameScreen, const char*>, 6> menus{{
            {GameScreen::main_menu, "menu1"}, {GameScreen::new_soldier, "NewSoldierMenu"},
            {GameScreen::campaign, "CampMenu"}, {GameScreen::multiplayer, "pmpMenu"},
            {GameScreen::options, "OptionsMenu"}, {GameScreen::credits, "CreditsMenu"}
        }};
        for (const auto& entry : menus) {
            auto& state = states_[entry.first];
            state.enter = [this, screen = entry.first, name = entry.second] {
                screen_ = screen;
                if (screen == GameScreen::new_soldier) soldier_.enter();
                else menu_.select_menu(name);
            };
            state.update = [this] { return update(); };
            state.render = [this] {
                frame_ = screen_ == GameScreen::new_soldier ? soldier_.draw() : menu_.render();
                window_->present(frame_);
                if (options_.frame_limit && ++frames_ >= options_.frame_limit) close_ = true;
            };
        }
        auto& end = states_[GameScreen::end];
        end.enter = [this] { screen_ = GameScreen::end; soldier_.profiles().save_all(); };
        end.update = [this] {
            bool quit = end_counter_ < 0;
            --end_counter_;
            for (const auto& event : window_->poll())
                if (event.kind == InputKind::quit) quit = true;
            return quit ? 1 : 0;
        };
        end.render = [this] { if (!frame_.empty()) window_->present(frame_); };
    }

    void run() {
        const auto found = states_.find(options_.initial);
        if (found == states_.end()) throw std::invalid_argument("Unknown initial game screen");
        loop_.run(&found->second, {[this] { return window_->ticks(); }, [this] { window_->yield(); }});
        soldier_.profiles().save_all();
    }

private:
    void request(GameScreen screen) {
        const auto found = states_.find(screen);
        if (found == states_.end()) throw std::runtime_error("Missing game state");
        loop_.request(&found->second);
        change_ = true;
    }

    void action(const MenuAction& action) {
        auto& button = menu_.button(action.button);
        if (action.action == 1) button.state = 3;
        else if (action.action == 6) button.state = 2;
        else if (action.action == 7) button.state = 1;
        if (screen_ == GameScreen::new_soldier) {
            if (const auto target = soldier_.actions(action)) request(*target);
            return;
        }
        if (action.action != 2) return;
        if (screen_ == GameScreen::main_menu) {
            static const std::map<std::string, GameScreen> transitions{
                {"mnu_main_ns", GameScreen::new_soldier}, {"mnu_main_sp", GameScreen::campaign},
                {"mnu_main_mp", GameScreen::multiplayer}, {"mnu_main_option", GameScreen::options},
                {"mnu_main_credits", GameScreen::credits}, {"mnu_main_exit", GameScreen::end}
            };
            const auto target = transitions.find(action.handler);
            if (target != transitions.end()) { request(target->second); return; }
        }
        std::clog << "Pending game callback: " << action.handler << '\n';
    }

    int update() {
        if (close_) return 1;
        for (const auto& event : window_->poll()) {
            std::vector<MenuAction> actions;
            switch (event.kind) {
            case InputKind::quit: return 1;
            case InputKind::key_down:
                if (screen_ != GameScreen::new_soldier || !soldier_.key(event.key)) actions = menu_.key(event.key);
                break;
            case InputKind::text_input:
                if (screen_ == GameScreen::new_soldier) soldier_.text(event.text);
                break;
            case InputKind::pointer_move: actions = menu_.pointer_move(event.x, event.y); break;
            case InputKind::pointer_down: actions = menu_.pointer_down(event.x, event.y); break;
            case InputKind::pointer_up: actions = menu_.pointer_up(event.x, event.y); break;
            case InputKind::key_up: break;
            }
            for (const auto& item : actions) action(item);
            if (change_) { change_ = false; return 1; }
        }
        return 0;
    }

    GameOptions options_;
    AssetStore assets_;
    MenuRuntime menu_;
    SoldierMenu soldier_;
    std::unique_ptr<GameWindow> window_;
    StateLoop loop_;
    std::map<GameScreen, GameState> states_;
    GameScreen screen_ = GameScreen::main_menu;
    std::vector<std::uint32_t> frame_;
    std::uint32_t frames_ = 0;
    int end_counter_ = 10;
    bool close_ = false;
    bool change_ = false;
};
} // namespace

int run_game(const GameOptions& options) {
    Game game(options);
    game.run();
    return 0;
}
} // namespace dfre
