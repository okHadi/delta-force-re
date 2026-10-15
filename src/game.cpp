#include "dfre/game.hpp"

#include "dfre/assets.hpp"
#include "dfre/campaign.hpp"
#include "dfre/menu.hpp"
#include "dfre/platform.hpp"
#include "dfre/player.hpp"
#include "dfre/renderer.hpp"
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
        const std::array<std::pair<GameScreen, const char*>, 7> menus{{
            {GameScreen::main_menu, "menu1"}, {GameScreen::new_soldier, "NewSoldierMenu"},
            {GameScreen::campaign, "CampMenu"}, {GameScreen::mission_setup, "MissionMenu"},
            {GameScreen::multiplayer, "pmpMenu"},
            {GameScreen::options, "OptionsMenu"}, {GameScreen::credits, "CreditsMenu"}
        }};
        for (const auto& entry : menus) {
            auto& state = states_[entry.first];
            state.enter = [this, screen = entry.first, name = entry.second] {
                screen_ = screen;
                if (screen == GameScreen::new_soldier) soldier_.enter();
                else if (screen == GameScreen::campaign) {
                    campaign_ = std::make_unique<CampaignMenu>(menu_, options_.game_folder, soldier_.profiles().at(soldier_.selected()));
                    campaign_->enter();
                } else if (screen == GameScreen::mission_setup) {
                    if (!mission_selection_) throw std::runtime_error("Mission selection is missing");
                    setup_ = std::make_unique<MissionSetupMenu>(menu_, options_.game_folder, soldier_.profiles().at(soldier_.selected()));
                    setup_->enter(*mission_selection_);
                } else menu_.select_menu(name);
            };
            state.update = [this] { return update(); };
            state.render = [this] {
                if (screen_ == GameScreen::new_soldier) frame_ = soldier_.draw();
                else if (screen_ == GameScreen::campaign) frame_ = campaign_->draw();
                else if (screen_ == GameScreen::mission_setup) frame_ = setup_->draw();
                else frame_ = menu_.render();
                window_->present(frame_);
                if (options_.frame_limit && ++frames_ >= options_.frame_limit) close_ = true;
            };
        }
        auto& world = states_[GameScreen::game];
        world.enter = [this] {
            if (!mission_selection_) throw std::runtime_error("Mission selection is missing");
            const auto mission = load_mission(assets_, mission_selection_->filename);
            world_ = std::make_unique<TerrainScene32>(prepare_terrain_scene32(
                load_terrain_runtime(assets_, mission_selection_->filename, 32),
                load_terrain_gamma(options_.game_folder / "INTRO.KDV")));
            player_ = initial_player(mission, world_->terrain.geometry);
            if (!player_) throw std::runtime_error("Mission has no single-player start");
            const auto view = make_model_camera(player_->camera.position, player_->camera.angles, {0, 0, 639, 479}, 90);
            VoxelCamera camera;
            camera.position = player_->camera.position;
            camera.yaw = player_->camera.angles[0];
            camera.pitch = player_->camera.angles[1];
            camera.focal_distance = view.projection.focal_length;
            frame_ = render_terrain_frame32(*world_, camera).pixels;
            screen_ = GameScreen::game;
        };
        world.update = [this] {
            if (close_) return 1;
            for (const auto& event : window_->poll())
                if (event.kind == InputKind::quit) return 1;
            return 0;
        };
        world.render = [this] {
            window_->present(frame_);
            if (options_.frame_limit && ++frames_ >= options_.frame_limit) close_ = true;
        };
        world.leave = [this] { world_.reset(); player_.reset(); };
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

    void campaign_result(const CampaignResult& result) {
        if (result.mission) mission_selection_ = result.mission;
        // Original single-player PreMenu immediately requests Game (41D430).
        const auto next = result.next_state == 0x4e6878 ? GameScreen::game : static_cast<GameScreen>(result.next_state);
        if (states_.find(next) != states_.end()) request(next);
        else std::clog << "Pending game state: 0x" << std::hex << result.next_state << std::dec << '\n';
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
        if (screen_ == GameScreen::campaign) {
            if (const auto result = campaign_->actions(action)) campaign_result(*result);
            return;
        }
        if (screen_ == GameScreen::mission_setup) {
            if (const auto result = setup_->actions(action)) campaign_result(*result);
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
        if (screen_ == GameScreen::campaign) campaign_->update();
        if (screen_ == GameScreen::mission_setup) setup_->update();
        for (const auto& event : window_->poll()) {
            if (event.kind == InputKind::pointer_move || event.kind == InputKind::pointer_down || event.kind == InputKind::pointer_up) {
                if (screen_ == GameScreen::campaign) campaign_->pointer(event.x, event.y);
                if (screen_ == GameScreen::mission_setup) setup_->pointer(event.x, event.y);
            }
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
    std::unique_ptr<CampaignMenu> campaign_;
    std::unique_ptr<MissionSetupMenu> setup_;
    std::optional<MissionSelection> mission_selection_;
    std::unique_ptr<TerrainScene32> world_;
    std::optional<InitialPlayer> player_;
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
