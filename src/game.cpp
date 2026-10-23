#include "dfre/game.hpp"

#include "dfre/assets.hpp"
#include "dfre/campaign.hpp"
#include "dfre/menu.hpp"
#include "dfre/platform.hpp"
#include "dfre/player.hpp"
#include "dfre/renderer.hpp"
#include "dfre/resources.hpp"
#include "dfre/world_runtime.hpp"
#include "dfre/weapons.hpp"
#include "dfre/scene_models.hpp"
#include "dfre/state_loop.hpp"
#include "dfre/soldier_menu.hpp"
#include "dfre/input_bridge.hpp"
#include "dfre/input_config.hpp"
#include "dfre/mission_live.hpp"
#include "dfre/mission_muzzle.hpp"
#include "dfre/mission_sound_live.hpp"
#include "dfre/mission_crt_thread.hpp"
#include "dfre/mission_script_registry.hpp"
#include "dfre/mission_rtxt.hpp"
#include "dfre/mission_actor_insert_live.hpp"
#include "dfre/mission_scene_transient.hpp"
#include "dfre/mission_scene_projectiles.hpp"

#include <array>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
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
            mission_assets_ = std::make_unique<MissionAssetCache>(assets_);
            mission_resources_ = mission_assets_->load_runtime(mission_selection_->filename);
            mission_assets_->load_projectile_runtime(*mission_resources_);
            const auto& mission = mission_resources_->mission;
            const auto gamma = load_terrain_gamma(options_.game_folder / "INTRO.KDV");
            world_ = std::make_unique<TerrainScene32>(prepare_terrain_scene32(
                load_terrain_runtime(assets_, mission_selection_->filename, 32),
                gamma));
            player_ = initial_player(mission, world_->terrain.geometry);
            if (!player_) throw std::runtime_error("Mission has no single-player start");
            const auto& profile = soldier_.profiles().at(soldier_.selected());
            MissionInitConfig config;
            auto settings=default_game_config(0);
            std::ifstream settings_file(options_.game_folder/"df.cfg",std::ios::binary);
            if(settings_file){
                const std::string text((std::istreambuf_iterator<char>(settings_file)),{});
                load_game_config_text(settings,text);
            }
            mission_rules_67f548_=std::uint32_t(settings.attributes); //407A90 copies4CEE8C.
            mission_voice_volume_67f51c_=std::uint32_t(settings.dialog_volume);
            config.scale_health=(mission_rules_67f548_&0x100)!=0;
            config.squad_pass=(mission_rules_67f548_&0x400)!=0;
            config.powerup_pass=(mission_rules_67f548_&0x800)==0;
            config.mode = mission_theme(mission);
            config.night = mission_night(mission);
            config.player_object_id = player_->marker_object_id;
            config.player_name = profile.name();
            config.player_face_id = profile.portrait();
            config.campaign_text_file = mission_selection_->text_file;
            const auto profile_word = [&](std::size_t offset) {
                std::uint32_t value;
                std::memcpy(&value,profile.raw().data()+offset,sizeof(value));
                return value;
            };
            WeaponState weapons;
            weapons.loadout({profile_word(0x680),profile_word(0x684),profile_word(0x688)});
            weapons.select_slot(3); // Original 4829F0 calls 484970, then 483DD0(3).
            config.weapon_slot_value = weapons.current_weapon();
            std::optional<RtxtTable> names;
            if (!config.campaign_text_file.empty() && mission_asset_present(assets_, config.campaign_text_file))
                names.emplace(assets_.read(config.campaign_text_file).bytes);
            prepare_mission_sound();
            mission_runtime_ = std::make_unique<MissionRuntimeState>(assemble_mission_runtime(
                *mission_resources_, mission_assets_->items(), world_->terrain.geometry,
                names ? &*names : nullptr, config,
                [this](std::string_view name){return mission_sound_->definition_id(name);}));
            if (!mission_runtime_->local_player || !mission_runtime_->actors)
                throw std::runtime_error("Mission has no local player actor");
            init_mission_tick(*mission_tick_,{},*mission_runtime_,0);
            mission_tick_->projectiles.assign(512,ProjectileEntity{});
            prepare_mission_extra_actor_pool(*mission_tick_);
            MissionActorInsertBindings extra_memory;
            extra_memory.read=[this](auto at,auto width){return mission_sound_->asset_bindings().read(at,width);};
            extra_memory.write=[this](auto at,auto width,auto value){mission_sound_->asset_bindings().write(at,width,value);};
            mission_extra_memory_=mission_actor_insert_live_bindings(*mission_tick_,mission_extra_count_8cfa14_,
                std::move(extra_memory),[this](auto pointer){
                    const auto offset=pointer-definition_base;
                    if(pointer<definition_base||offset%runtime_item_size||offset/runtime_item_size>=mission_runtime_->definitions.size())
                        throw MissionTickError("Extra actor definition is outside its current table");
                    return std::uint32_t(offset/runtime_item_size);
                });
            bind_mission_linked_position(*mission_tick_);
            mission_tick_->weapons=std::move(weapons);
            mission_animations_=std::make_unique<MissionAnimations>(*mission_runtime_,*mission_assets_);
            mission_geometry_=std::make_unique<MissionGeometry>(*mission_runtime_,*mission_assets_,assets_);
            mission_muzzle_=std::make_unique<MissionMuzzle>(*mission_runtime_,*mission_assets_,*mission_animations_);
            const auto matrices=initialized_mission_matrices(*mission_runtime_,*mission_assets_,*mission_animations_);
            bind_mission_animations(*mission_tick_,*mission_animations_);
            bind_mission_geometry(*mission_tick_,*mission_geometry_,matrices);
            mission_tick_->world.terrain=&world_->terrain.geometry;
            mission_tick_->world.water_level=world_->terrain.water_height;
            ActionBindings controls{};
            for(std::size_t i=0;i<controls.size();++i)
                controls[i]={profile_word(0x6a0+i*12),profile_word(0x6a4+i*12),profile_word(0x6a8+i*12)};
            mission_input_=std::make_unique<InputBridge>(controls);
            mission_scene_ = std::make_unique<MissionScene32>(*mission_runtime_, *mission_assets_, *world_, gamma);
            draw_mission(true);
            screen_ = GameScreen::game;
        };
        world.update = [this] {
            if (close_) return 1;
            for (const auto& event : window_->poll()) {
                mission_input_->feed(event);
                if (event.kind == InputKind::quit) return 1;
            }
            return 0;
        };
        world.render = [this] {
            draw_mission(false);
            window_->present(frame_);
            if (options_.frame_limit && ++frames_ >= options_.frame_limit) close_ = true;
        };
        world.leave = [this] {
            mission_scene_.reset();
            mission_input_.reset();
            if(mission_scripts_)mission_scripts_->service(0x455ea0,{});
            mission_scripts_.reset();
            mission_script_cells_.reset();
            mission_extra_memory_={};
            mission_sound_.reset();
            mission_audio_frame_.reset();
            mission_sound_terrain_=nullptr;
            mission_tick_.reset();
            mission_muzzle_.reset();
            mission_geometry_.reset();
            mission_animations_.reset();
            mission_runtime_.reset();
            mission_resources_.reset();
            mission_assets_.reset();
            world_.reset();
            player_.reset();
        };
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

    bool definition_claim(std::uint32_t at,std::uint32_t width) const {
        const auto end=std::uint64_t(at)+width;
        if(at<0xc23814&&end>0xc23810){
            if(at<0xc23810||end>0xc23814)throw MissionTickError("Definition pointer access crosses its field");
            return true;
        }
        if(!mission_runtime_)return false;
        const auto limit=std::uint64_t(definition_base)+mission_runtime_->definitions.size()*runtime_item_size;
        if(at<limit&&end>definition_base){
            if(at<definition_base||end>limit)throw MissionTickError("Definition access crosses its table");
            return true;
        }
        return false;
    }
    std::uint32_t definition_read(std::uint32_t at,std::uint32_t width) const {
        if(!mission_runtime_)throw MissionTickError("Mission definition table is not ready");
        if(at>=0xc23810&&at<0xc23814)return (definition_base>>((at-0xc23810)*8))&(width==4?0xffffffffu:((1u<<(width*8))-1));
        const auto* bytes=reinterpret_cast<const std::uint8_t*>(mission_runtime_->definitions.data());
        std::uint32_t value=0;for(std::uint32_t i=0;i<width;++i)value|=std::uint32_t(bytes[at-definition_base+i])<<(8*i);
        return value;
    }
    bool extra_count_claim(std::uint32_t at,std::uint32_t width) const {
        const auto end=std::uint64_t(at)+width;
        if(at>=0x8cfa18||end<=0x8cfa14)return false;
        if(at<0x8cfa14||end>0x8cfa18)throw MissionTickError("Extra actor count access crosses its field");
        return true;
    }
    void prepare_mission_sound() {
        mission_tick_=std::make_unique<MissionTickState>();
        mission_sound_terrain_=&world_->terrain.geometry;
        mission_listener_={};mission_listener_yaw_=0;
        mission_audio_frame_=std::make_unique<MissionAudioFrameCells>(
            MissionAudioFrameCells{0,mission_listener_,mission_listener_yaw_});
        std::ifstream executable_file(options_.game_folder/"Df.exe",std::ios::binary);
        if(!executable_file)throw std::runtime_error("Original Df.exe is missing");
        const AssetBytes executable((std::istreambuf_iterator<char>(executable_file)),{});
        auto bindings=mission_sound_live_bindings(*mission_tick_,[this]{return mission_script_key_503228_;});
        bindings=mission_crt_thread_bindings(std::move(bindings),mission_crt_thread_,0x19000000);
        const auto record_base=bindings.record_base;
        const auto record_read=bindings.record_read;
        const auto record_store=bindings.record_store;
        bindings.record_base=[this,record_base](std::uint32_t at,std::uint32_t width)->std::optional<std::uint32_t>{
            if(mission_scripts_&&mission_scripts_->owns(at,width))return at;
            if(mission_extra_memory_.read){if(const auto base=mission_extra_actor_record_base(at,width))return base;if(extra_count_claim(at,width))return at;}
            if(definition_claim(at,width))return at;
            return record_base(at,width);
        };
        bindings.record_read=[this,record_read](std::uint32_t base,std::uint32_t offset,std::uint32_t width){
            const auto at=base+offset;
            if(mission_scripts_&&mission_scripts_->owns(at,width))return mission_scripts_->memory().read(at,width);
            if(mission_extra_memory_.read&&(mission_extra_actor_record_base(at,width)||extra_count_claim(at,width)))return mission_extra_memory_.read(at,width);
            if(definition_claim(at,width))return definition_read(at,width);
            return record_read(base,offset,width);
        };
        bindings.record_store=[this,record_store](std::uint32_t base,std::uint32_t offset,std::uint32_t width,std::uint32_t value){
            const auto at=base+offset;
            if(mission_scripts_&&mission_scripts_->owns(at,width)){mission_scripts_->memory().write(at,width,value);return;}
            if(mission_extra_memory_.write&&(mission_extra_actor_record_base(at,width)||extra_count_claim(at,width))){mission_extra_memory_.write(at,width,value);return;}
            if(definition_claim(at,width))throw MissionTickError("Mission definitions are read-only after initialization");
            record_store(base,offset,width,value);
        };
        bindings.service=[this](std::uint32_t site,const std::vector<std::uint32_t>& args){
            if(mission_scripts_){const auto result=mission_scripts_->service(site,args);if(result)return *result;}
            const auto& memory=mission_sound_->asset_bindings();
            if(site==0x499f1f)return mission_rtxt_load(memory,args.at(0));
            if(site==0x49a092)return mission_rtxt_lookup(memory,args.at(0),args.at(1),args.at(2));
            if(site==0x49a076)return mission_rtxt_entry(memory,args.at(0),args.at(1));
            throw MissionSoundAssetError("Mission sound service is not bound "+std::to_string(site));
        };
        mission_sound_=std::make_unique<MissionSoundRuntime>(assets_,executable,
            *mission_audio_frame_,mission_sound_terrain_,std::move(bindings));
        mission_script_queue_={};
        mission_script_cells_=std::make_unique<MissionScriptRuntimeCells>(
            MissionScriptRuntimeCells{mission_script_queue_,mission_tick_->globals.mission_67f528});
        const auto scratch=mission_sound_->asset_bindings().call(0x497f60,{0x4000,4,0,0});
        MissionScriptRegistryBindings script_host;
        script_host.crt_thread=[](){return 0x19000000u;};
        script_host.voice_volume=[this](){return mission_voice_volume_67f51c_;};
        mission_scripts_=std::make_unique<MissionScriptRegistry>(*mission_script_cells_,
            mission_sound_->bank_bindings(),scratch,0x4000,std::move(script_host));
        const auto& mission=mission_resources_->mission;
        (void)mission_dialogue_archive(mission);
        const auto& memory=mission_sound_->asset_bindings();
        for(std::size_t i=0;i<mission.header.size();++i)
            memory.write(0x8cf908+std::uint32_t(i),1,mission.header[i]);
        mission_sound_->initialize_definitions("LOOP.DEF");
        mission_sound_->resolve_definitions("SNIPER.PWF","NEW.PWF");
    }
    void draw_mission(bool report_gaps) {
        const auto slot=mission_tick_->config.local_actor;
        if(!slot)throw std::runtime_error("Mission has no live local actor");
        const auto& actor=mission_tick_->tables.groups[3].at(*slot);
        for(std::size_t i=0;i<3;++i)std::memcpy(&player_->camera.position[i],&actor.position[i],4);
        player_->camera.angles={actor.angle_yaw,actor.angle_pitch,mission_tick_->runtime_for({3,*slot}).roll_38};
        const auto eye=actor.position[2]+0xb333u;
        std::memcpy(&player_->camera.position[2],&eye,4);
        const auto view=make_model_camera(player_->camera.position,player_->camera.angles,{0,0,639,479},90);
        VoxelCamera camera;
        camera.position=player_->camera.position;camera.yaw=player_->camera.angles[0];
        camera.pitch=player_->camera.angles[1];camera.focal_distance=view.projection.focal_length;
        const auto images=snapshot_mission_actors(*mission_tick_);
        const auto transient=mission_scene_transient_records(*mission_runtime_,*mission_tick_);
        const auto effects=mission_scene_projectile_records(mission_tick_->projectiles);
        MissionSceneLiveInputs32 live;
        live.actors=&images;live.pose=[this](auto instance){return mission_animations_->pose(instance);};
        live.runtime_records=&transient;live.effects=&effects;
        auto scene=mission_scene_->render(*world_,camera,{},live);
        if(report_gaps)for(const auto& gap:scene.gaps)
            std::clog<<"Scene input: "<<scene_model_gap_name(gap.kind)<<" "<<gap.note<<'\n';
        frame_=std::move(scene.terrain.pixels);
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
            if ((event.kind == InputKind::pointer_down || event.kind == InputKind::pointer_up) && event.button > 1) continue;
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
            case InputKind::key_up: case InputKind::pointer_delta: case InputKind::focus_lost: break;
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
    std::unique_ptr<MissionAssetCache> mission_assets_;
    std::unique_ptr<MissionScene32> mission_scene_;
    std::optional<MissionRuntimeResources> mission_resources_;
    std::unique_ptr<MissionRuntimeState> mission_runtime_;
    std::optional<InitialPlayer> player_;
    std::unique_ptr<MissionTickState> mission_tick_;
    static constexpr std::uint32_t definition_base=0x18000000;
    MissionActorInsertBindings mission_extra_memory_;
    std::uint32_t mission_extra_count_8cfa14_=0;
    std::uint32_t mission_rules_67f548_=0;
    std::unique_ptr<MissionAnimations> mission_animations_;
    std::unique_ptr<MissionGeometry> mission_geometry_;
    std::unique_ptr<MissionMuzzle> mission_muzzle_;
    std::unique_ptr<InputBridge> mission_input_;
    Fixed3 mission_listener_{};
    std::uint32_t mission_listener_yaw_=0;
    std::uint32_t mission_script_key_503228_=0x04960552;
    MissionCrtThreadCells mission_crt_thread_;
    std::uint32_t mission_voice_volume_67f51c_=0;
    const TerrainGeometry* mission_sound_terrain_=nullptr;
    std::unique_ptr<MissionAudioFrameCells> mission_audio_frame_;
    std::unique_ptr<MissionSoundRuntime> mission_sound_;
    MissionScriptQueueCells mission_script_queue_{};
    std::unique_ptr<MissionScriptRuntimeCells> mission_script_cells_;
    std::unique_ptr<MissionScriptRegistry> mission_scripts_;
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
