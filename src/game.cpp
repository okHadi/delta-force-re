#include "dfre/mission_actor_resources.hpp"
#include "dfre/mission_profile_start.hpp"
#include "dfre/mission_owner_cleanup.hpp"
#include "dfre/mission_debrief_format.hpp"
#include "dfre/mission_parameter.hpp"
#include "dfre/mission_weapon_startup.hpp"
#include "dfre/mission_terrain_content.hpp"
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
#include "dfre/mission_primary_actor_live.hpp"
#include "dfre/mission_scene_transient.hpp"
#include "dfre/mission_scene_projectiles.hpp"
#include "dfre/mission_client_live.hpp"
#include "dfre/mission_client_startup.hpp"
#include "dfre/mission_df_socket_runtime.hpp"
#include "dfre/mission_line_file_runtime.hpp"
#include "dfre/mission_localization.hpp"
#include "dfre/mission_session.hpp"
#include "dfre/mission_input_live.hpp"
#include "dfre/mission_profile_memory.hpp"

#include <array>
#include <chrono>
#include <immintrin.h>
#include <limits>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <mutex>
#include <stdexcept>

namespace dfre {
namespace {

class Game {
public:
    explicit Game(const GameOptions& options)
        : options_(options), assets_(options.game_folder), menu_(assets_),
          soldier_(menu_, options.game_folder, options.save_folder),
          profile_memory_([this](auto slot)->AssetBytes&{return soldier_.profiles().at(slot).source_bytes();},[this]{return soldier_.selected();}),
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
            mission_result_retained_=false;
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
            const auto& source_memory=mission_sound_->asset_bindings();
            source_memory.write(0x4cee8c,4,mission_rules_67f548_);
            source_memory.write(0x67f548,4,mission_rules_67f548_);
            source_memory.write(0x4cee90,4,std::uint32_t(settings.replay));
            const MissionDebriefBindings profile_start{source_memory.read,source_memory.write,source_memory.call};
            mission_profile_start_mask(profile_start);
            mission_profile_start_result(profile_start);
            source_memory.write(0x68ba34,4,mission_selection_->mission);
            const auto name=profile.name();
            const auto name_at=source_memory.call(0x497f60,{std::uint32_t(name.size()+1),4,0,0});
            for(std::size_t i=0;i<=name.size();++i)source_memory.write(name_at+std::uint32_t(i),1,i<name.size()?std::uint8_t(name[i]):0);
            MissionClientStartupBindings client_setup{source_memory.read,source_memory.write,source_memory.call,{}};
            mission_client_setup_local(client_setup,name_at,config.player_face_id,config.player_side);
            source_memory.call(0x4980d0,{name_at});
            config.player_object_id=mission_sound_->asset_bindings().read(0x50133c,4);
            config.player_team=mission_clients_->read(mission_client_source_base+0x1c4,2);
            config.local_player_active=mission_clients_->read(mission_client_source_base+0x18c,1)!=0;
            mission_runtime_ = std::make_unique<MissionRuntimeState>(assemble_mission_runtime(
                *mission_resources_, mission_assets_->items(), world_->terrain.geometry,
                names ? &*names : nullptr, config,
                [this](std::string_view name){return mission_sound_->definition_id(name);},
                [this](std::string_view name)->const Model&{return mission_assets_->model(name);}));
            const auto& startup_memory=mission_sound_->asset_bindings();
            MissionClientAddedBindings weapon_startup{startup_memory.read,startup_memory.write,{}};
            weapon_startup.call=[this,&startup_memory](auto site,const auto& args)->std::uint32_t{
                if(site==0x4828c0){
                    if(args.size()!=1)throw MissionTickError("Weapon graphic lookup needs one item ID");
                    return std::uint32_t(runtime_item_index(mission_runtime_->definitions,args[0]));
                }
                return startup_memory.call(site,args);
            };
            //Separate mapped caller frame; Sound request frames end at2100DFFF.
            mission_weapon_startup(weapon_startup,0x2100e200,0x20);
            const auto content=mission_terrain_content(assets_,world_->terrain);
            for(std::uint32_t i=0;i<content.size();++i)startup_memory.write(0xbd67d4+i*4,4,content[i]);
            if (!mission_runtime_->local_player || !mission_runtime_->actors)
                throw std::runtime_error("Mission has no local player actor");
            for(const auto& spawn:mission_runtime_->spawns)
                mission_clients_->write(MissionClientLive::source_address(spawn.placement,8),4,
                    mission_actor_source_address(3,spawn.actor));
            init_mission_tick(*mission_tick_,{},*mission_runtime_,0);
            mission_tick_->projectiles.assign(512,ProjectileEntity{});
            MissionPrimaryActorLiveBindings primary_memory;
            primary_memory.delegated.read=[this](auto at,auto width){return mission_sound_->asset_bindings().read(at,width);};
            primary_memory.delegated.write=[this](auto at,auto width,auto value){mission_sound_->asset_bindings().write(at,width,value);};
            primary_memory.initial_definition=[this](auto slot){
                const auto image=snapshot_mission_actor(*mission_tick_,{0,slot});
                std::uint32_t index;std::memcpy(&index,image.data()+4,4);
                return index?definition_base+index*std::uint32_t(runtime_item_size):0u;
            };
            primary_memory.definition_index=[this](auto pointer){
                const auto offset=pointer-definition_base;
                if(pointer<definition_base||offset%runtime_item_size||offset/runtime_item_size>=mission_runtime_->definitions.size())
                    throw MissionTickError("Primary actor definition is outside its current table");
                return std::uint32_t(offset/runtime_item_size);
            };
            primary_memory.definition_address=[this](auto index){
                if(index>=mission_runtime_->definitions.size())throw MissionTickError("Primary definition index exceeds its current table");
                return definition_base+index*std::uint32_t(runtime_item_size);
            };
            if(mission_tick_->tables.groups[0].size()>1024)throw MissionTickError("Primary actor count exceeds its physical pool");
            //449620 clears the full physical pool before this fresh mission loads its counted prefix.
            mission_primary_=std::make_unique<MissionPrimaryActorLive>(*mission_tick_,std::move(primary_memory),
                std::vector<AiActorImage>(1024-mission_tick_->tables.groups[0].size()));
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
            mission_tick_->globals.mouse_disabled_503130=std::uint32_t(settings.windowed);
            mission_tick_->globals.mouse_scale_67f500=std::uint32_t(settings.mousescale);
            mission_tick_->globals.toggle_67f4fc=std::uint32_t(settings.flipmouse);
            mission_input_=std::make_unique<InputBridge>(controls);
            mission_input_sources_={};
            mission_input_sources_.read=[this](auto at,auto width){return mission_sound_->asset_bindings().read(at,width);};
            mission_input_sources_.window_active=[this]{return window_->active();};
            mission_input_sources_.dispatch=[this](auto id){mission_dispatch_command(*mission_tick_,id,0,mission_input_->command_table());};
            mission_input_->set_live_sources(
                [this]{return mission_input_live_sources(*mission_tick_,mission_input_sources_);},
                [this](const auto& result){mission_input_live_global(*mission_tick_,mission_input_sources_,result);},
                [this](const auto& event){mission_input_live_chat(mission_input_sources_,event);});
            mission_input_->set_query_row_source([this](auto row){
                return mission_session_input_query_row(*mission_sound_,row);
            });
            mission_requested_8ee1d4_=source_memory.read(0x8ee1d4,4);
            mission_camera_mode_6ba3ec_=source_memory.read(0x6ba3ec,4);
            const std::array<std::int32_t*,3> camera_fields{{&mission_camera_position_6ba3d0_.x,
                &mission_camera_position_6ba3d0_.y,&mission_camera_position_6ba3d0_.z}};
            for(std::size_t i=0;i<camera_fields.size();++i){
                const auto word=source_memory.read(0x6ba3d0+std::uint32_t(i)*4,4);
                std::memcpy(camera_fields[i],&word,4);
            }
            const auto audio_origin=std::chrono::steady_clock::now();
            MissionSessionHost session_host;
            session_host.primary_source_records=true;
            session_host.source_event_file=mission_selection_->filename;
            session_host.pump_490880=[this]{return poll_mission_input();};
            session_host.rdtsc=[](){return std::uint64_t(__rdtsc());};
            session_host.elapsed_audio_frames=[audio_origin,previous=std::uint64_t(0)]()mutable{
                const auto elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-audio_origin).count();
                const auto total=std::uint64_t(elapsed*22050);
                const auto count=total-previous;
                if(count>std::numeric_limits<std::uint32_t>::max())throw MissionTickError("Audio frame interval exceeds its current range");
                previous=total;return std::uint32_t(count);
            };
            session_host.camera_actor_6ba3f0=[this]{return actor_handle(3,*mission_tick_->config.local_actor);};
            session_host.camera_mode_6ba3ec=[this]{return mission_camera_mode_6ba3ec_;};
            mission_session_=std::make_unique<MissionSession>(MissionSessionContext{
                *mission_tick_,*mission_runtime_,mission_resources_->mission,*mission_assets_,world_->terrain,
                *mission_animations_,*mission_geometry_,*mission_muzzle_,*mission_input_,*mission_sound_,
                *mission_scripts_,*mission_audio_frame_,mission_script_queue_,mission_crt_thread_,*mission_clients_,
                mission_extra_memory_,mission_rules_67f548_,mission_requested_8ee1d4_,mission_camera_position_6ba3d0_
            },std::move(session_host),mission_result_);
            mission_session_->install();
            mission_scene_ = std::make_unique<MissionScene32>(*mission_runtime_, *mission_assets_, *world_, gamma);
            draw_mission(true);
            screen_ = GameScreen::game;
        };
        world.update = [this] {
            return poll_mission_input()?0:1;
        };
        world.render = [this] {
            draw_mission(false);
            window_->present(frame_);
            if (options_.frame_limit && ++frames_ >= options_.frame_limit) close_ = true;
        };
        world.leave = [this] {
            mission_result_retained_=bool(mission_session_);
            if(mission_session_)mission_session_->reset_events();
            mission_session_.reset();
            mission_scene_.reset();
            mission_input_.reset();
            if(mission_scripts_)mission_scripts_->service(0x455ea0,{});
            mission_scripts_.reset();
            mission_script_cells_.reset();
            mission_extra_memory_={};
            mission_primary_.reset();
            mission_files_.reset();
            mission_sockets_.reset();
            mission_sound_.reset();
            mission_clients_.reset();
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

    bool poll_mission_input() {
        if(close_)return false;
        for(const auto& event:window_->poll()){
            mission_input_->feed(event);
            if(event.kind==InputKind::quit)close_=true;
        }
        return !close_;
    }
    bool definition_claim(std::uint32_t at,std::uint32_t width) const {
        const auto end=std::uint64_t(at)+width;
        if(at<0xc23818&&end>0xc23810){
            const auto cell=at<0xc23814?0xc23810u:0xc23814u;
            if(at<cell||end>std::uint64_t(cell)+4)throw MissionTickError("Definition table cell access crosses its field");
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
        if(at>=0xc23810&&at<0xc23818){
            const auto cell=at<0xc23814?0xc23810u:0xc23814u;
            const auto value=cell==0xc23810?definition_base:std::uint32_t(mission_runtime_->definitions.size());
            return (value>>((at-cell)*8))&(width==4?0xffffffffu:((1u<<(width*8))-1));
        }
        const auto* bytes=reinterpret_cast<const std::uint8_t*>(mission_runtime_->definitions.data());
        std::uint32_t value=0;for(std::uint32_t i=0;i<width;++i)value|=std::uint32_t(bytes[at-definition_base+i])<<(8*i);
        return value;
    }
    void definition_clear(std::uint32_t at,std::uint32_t width,std::uint32_t value) {
        if(!mission_runtime_||!mission_tick_||width!=4||value!=0||at<definition_base)
            throw MissionTickError("Definition resource clear requires a current aligned zero DWORD");
        const auto offset=at-definition_base;
        const auto slot=offset/runtime_item_size,field=offset%runtime_item_size;
        if(slot>=mission_runtime_->definitions.size()||(field!=0x6c&&field!=0x70&&field!=0x74))
            throw MissionTickError("Definition resource clear is outside its current fields");
        if(mission_tick_->definitions.entries().size()!=mission_runtime_->definitions.size())
            throw MissionTickError("Definition model view has a different current extent");
        auto& item=mission_runtime_->definitions.at(slot);
        if(field==0x6c){mission_tick_->definitions.clear_model(slot);item.model=0;}
        else if(field==0x70)item.husk_model=0;
        else item.character=0;
    }
    bool extra_count_claim(std::uint32_t at,std::uint32_t width) const {
        const auto end=std::uint64_t(at)+width;
        if(at>=0x8cfa18||end<=0x8cfa14)return false;
        if(at<0x8cfa14||end>0x8cfa18)throw MissionTickError("Extra actor count access crosses its field");
        return true;
    }
    bool retained_result_claim(std::uint32_t at,std::uint32_t width) const {
        if(mission_session_||!mission_result_retained_)return false;
        const auto end=std::uint64_t(at)+width;
        if(at>=result_state_block+result_state_bytes||end<=result_state_block)return false;
        if((width!=1&&width!=2&&width!=4)||at<result_state_block||end>result_state_block+result_state_bytes)
            throw MissionTickError("Retained result access crosses its owner");
        return true;
    }
    void prepare_mission_sound() {
        mission_tick_=std::make_unique<MissionTickState>();
        mission_clients_=std::make_unique<MissionClientLive>();
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
        const auto actor_word=bindings.actor_word;
        bindings.actor_word=[this,actor_word](std::uint32_t actor,std::uint32_t offset){
            if(mission_primary_){
                if(const auto base=mission_primary_->record_base(actor,1)){
                    if(*base!=actor||offset>0x110)throw MissionTickError("Primary sound actor needs a complete current word");
                    return mission_primary_->read(actor+offset,4);
                }
            }
            if(mission_extra_memory_.read){
                if(const auto base=mission_extra_actor_record_base(actor,1)){
                    if(*base!=actor||offset>0x110)throw MissionTickError("Extra sound actor needs a complete current word");
                    return mission_extra_memory_.read(actor+offset,4);
                }
            }
            return actor_word(actor,offset);
        };
        bindings.record_base=[this,record_base](std::uint32_t at,std::uint32_t width)->std::optional<std::uint32_t>{
            if(const auto base=profile_memory_.record_base(at,width))return base;
            if(retained_result_claim(at,width))return at;
            if(mission_primary_&&mission_primary_->claims(at,width)){
                if(const auto base=mission_primary_->record_base(at,width))return base;
                return at;
            }
            if(mission_session_&&mission_session_->claims(at,width))return at;
            if(mission_clients_->claims(at,width))return at;
            if(mission_sockets_&&mission_sockets_->claims(at,width))return at;
            if(mission_scripts_&&mission_scripts_->owns(at,width))return at;
            if(mission_extra_memory_.read){if(const auto base=mission_extra_actor_record_base(at,width))return base;if(extra_count_claim(at,width))return at;}
            if(definition_claim(at,width))return at;
            return record_base(at,width);
        };
        bindings.record_read=[this,record_read](std::uint32_t base,std::uint32_t offset,std::uint32_t width){
            const auto at=base+offset;
            if(profile_memory_.claims(at,width))return profile_memory_.read(at,width);
            if(retained_result_claim(at,width)){std::uint32_t n=0;for(std::uint32_t i=0;i<width;++i)n|=std::uint32_t(mission_result_.at(at-result_state_block+i))<<(8*i);return n;}
            if(mission_primary_&&mission_primary_->claims(at,width)){
                if(const auto record=mission_primary_->record_base(at,width))
                    if(*record!=base)throw MissionTickError("Primary sound record base is not aligned");
                return mission_primary_->read(at,width);
            }
            if(mission_session_&&mission_session_->claims(at,width))return mission_session_->read(at,width);
            if(mission_clients_->claims(at,width))return mission_clients_->read(at,width);
            if(mission_sockets_&&mission_sockets_->claims(at,width))return mission_sockets_->read(at,width);
            if(mission_scripts_&&mission_scripts_->owns(at,width))return mission_scripts_->memory().read(at,width);
            if(mission_extra_memory_.read&&(mission_extra_actor_record_base(at,width)||extra_count_claim(at,width)))return mission_extra_memory_.read(at,width);
            if(definition_claim(at,width))return definition_read(at,width);
            return record_read(base,offset,width);
        };
        bindings.record_store=[this,record_store](std::uint32_t base,std::uint32_t offset,std::uint32_t width,std::uint32_t value){
            const auto at=base+offset;
            if(profile_memory_.claims(at,width)){profile_memory_.write(at,width,value);return;}
            if(retained_result_claim(at,width)){for(std::uint32_t i=0;i<width;++i)mission_result_.at(at-result_state_block+i)=std::uint8_t(value>>(8*i));return;}
            if(mission_primary_&&mission_primary_->claims(at,width)){
                if(const auto record=mission_primary_->record_base(at,width))
                    if(*record!=base)throw MissionTickError("Primary sound record base is not aligned");
                mission_primary_->write(at,width,value);return;
            }
            if(mission_session_&&mission_session_->claims(at,width)){mission_session_->write(at,width,value);return;}
            if(mission_clients_->claims(at,width)){mission_clients_->write(at,width,value);return;}
            if(mission_sockets_&&mission_sockets_->claims(at,width)){mission_sockets_->write(at,width,value);return;}
            if(mission_scripts_&&mission_scripts_->owns(at,width)){mission_scripts_->memory().write(at,width,value);return;}
            if(mission_extra_memory_.write&&(mission_extra_actor_record_base(at,width)||extra_count_claim(at,width))){mission_extra_memory_.write(at,width,value);return;}
            if(definition_claim(at,width)){definition_clear(at,width,value);return;}
            record_store(base,offset,width,value);
        };
        bindings.service=[this](std::uint32_t site,const std::vector<std::uint32_t>& args){
            if(site==0x401e70){
                if(!args.empty()||!mission_session_)throw MissionTickError("Profile leave requires the current Session");
                mission_session_->profile_leave([this]() ->AssetBytes&{return soldier_.profiles().at(soldier_.selected()).source_bytes();});
                return 0u;
            }
            if(site==0x42eba0){if(!args.empty()||!mission_session_)throw MissionTickError("Mission end events require the current Session");mission_session_->end_events();return 0u;}
            if(mission_session_){if(const auto result=mission_session_->leave_service(site,args))return *result;if(const auto result=mission_session_->hud_service(site,args))return *result;}
            if(mission_scripts_){const auto result=mission_scripts_->service(site,args);if(result)return *result;}
            const auto& memory=mission_sound_->asset_bindings();
            if(site==0x482910){if(!args.empty())throw MissionTickError("Item resource clear takes no arguments");mission_item_resource_links_clear({memory.read,memory.write,memory.call});return 0u;}
            if(site==0x43e6d0){if(!args.empty())throw MissionTickError("Sound cleanup takes no arguments");mission_sound_->cleanup();return 0u;}
            if(site==0x49db20){if(args.size()!=1)throw MissionTickError("Bank cleanup needs its current context");mission_sound_->release_banks(args[0]);return 0u;}
            if(site==0x402d40){if(!args.empty())throw MissionTickError("Resource cleanup takes no arguments");mission_resource_cleanup_global({memory.read,memory.write,memory.call});return 0u;}
            if(site==0x493a80)return mission_debrief_format({memory.read,memory.write,{}},args);
            if(site==0x4c4f40){if(args.size()!=2)throw MissionTickError("Parameter setter needs two arguments");return mission_parameter_set({memory.read,memory.write,{}},args[0],args[1]);}
            if(site==0x4c4f60){if(args.size()!=1)throw MissionTickError("Parameter getter needs one argument");return mission_parameter_get({memory.read,memory.write,{}},args[0]);}
            if(mission_files_){if(const auto result=mission_files_->service(site,args))return *result;}
            if(site==0x49a620){
                const auto target=args.at(0),count=args.at(2),byte=args.at(1)&255u,word=byte*0x01010101u;
                std::uint32_t i=0;for(;count-i>=4;i+=4)memory.write(target+i,4,word);
                for(;i<count;++i)memory.write(target+i,1,byte);
                return target;
            }
            if(site==0x404cf0)return mission_localized_text({memory.read,
                [&memory](auto table,auto group,auto key){return mission_rtxt_lookup(memory,table,group,key);}},args.at(0),args.at(1));
            if(site==0x454df0&&mission_sockets_)return mission_df_socket_resolve(mission_sockets_->bindings(),args.at(0));
            if(site==0x454bb0&&mission_sockets_)return mission_df_socket_start(mission_sockets_->bindings(),args.at(0),args.at(1),args.at(2));
            if(site==0x499f1f)return mission_rtxt_load(memory,args.at(0));
            if(site==0x49a092)return mission_rtxt_lookup(memory,args.at(0),args.at(1),args.at(2));
            if(site==0x49a076)return mission_rtxt_entry(memory,args.at(0),args.at(1));
            throw MissionSoundAssetError("Mission sound service is not bound "+std::to_string(site));
        };
        mission_sound_=std::make_unique<MissionSoundRuntime>(assets_,executable,
            *mission_audio_frame_,mission_sound_terrain_,std::move(bindings));
        mission_sound_->bind_fill_handler([this](auto target,auto byte,auto count){
            if(target!=0x7dde80||byte!=0||count!=0x45000)return false;
            if(!mission_primary_)throw MissionTickError("Primary clear requires its current owner");
            mission_primary_->clear_physical_pool();return true;
        });
        mission_script_queue_={};
        mission_script_cells_=std::make_unique<MissionScriptRuntimeCells>(
            MissionScriptRuntimeCells{mission_script_queue_,mission_tick_->globals.mission_67f528});
        const auto scratch=mission_sound_->asset_bindings().call(0x497f60,{0x4000,4,0,0});
        MissionScriptRegistryBindings script_host;
        script_host.message=[this](auto id,const auto& packet){
            if(!mission_session_)throw MissionTickError("Script packet requires a current mission session");
            mission_session_->script_message(id,packet.data(),packet.size());
        };
        script_host.crt_thread=[](){return 0x19000000u;};
        script_host.voice_volume=[this](){return mission_voice_volume_67f51c_;};
        mission_scripts_=std::make_unique<MissionScriptRegistry>(*mission_script_cells_,
            mission_sound_->bank_bindings(),scratch,0x4000,std::move(script_host));
        const auto& mission=mission_resources_->mission;
        (void)mission_dialogue_archive(mission);
        const auto& memory=mission_sound_->asset_bindings();
        mission_socket_memory_={memory.read,memory.write};
        mission_sockets_=std::make_unique<MissionDfSocketRuntime>(mission_socket_memory_);
        const auto file_scratch=memory.call(0x497f60,{0x2000,4,0,0});
        MissionLineFileRuntimeBindings file_locks;
        file_locks.lock=[this](auto){mission_file_mutex_.lock();};
        file_locks.unlock=[this](auto){mission_file_mutex_.unlock();};
        mission_files_=std::make_unique<MissionLineFileRuntime>(options_.game_folder,memory,
            file_scratch,0x2000,std::move(file_locks));
        mission_client_startup({memory.read,memory.write,memory.call,{}});
        for(const auto& table:std::array<std::pair<std::uint32_t,std::uint32_t>,3>{{
            {0x50322c,0x4c80a8},{0x503234,0x4c80b4},{0x503230,0x4c80dc}}}){
            const auto loaded=mission_rtxt_load(memory,table.second);
            if(!loaded)throw MissionSoundAssetError("Game text table failed to load");
            memory.write(table.first,4,loaded);
        }
        const auto& campaign_file=mission_selection_->text_file;
        if(!campaign_file.empty()&&mission_asset_present(assets_,campaign_file)){
            const auto filename=memory.call(0x497f60,{std::uint32_t(campaign_file.size()+1),4,0,0});
            for(std::size_t i=0;i<=campaign_file.size();++i)memory.write(filename+std::uint32_t(i),1,i<campaign_file.size()?std::uint8_t(campaign_file[i]):0);
            const auto table=mission_rtxt_load(memory,filename);
            memory.call(0x4980d0,{filename});
            if(!table)throw MissionSoundAssetError("Campaign text table failed to load");
            memory.write(0x503238,4,table);
        }
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
    MissionProfileMemory profile_memory_;
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
    std::unique_ptr<MissionPrimaryActorLive> mission_primary_;
    std::uint32_t mission_extra_count_8cfa14_=0;
    std::uint32_t mission_rules_67f548_=0;
    std::unique_ptr<MissionAnimations> mission_animations_;
    std::unique_ptr<MissionGeometry> mission_geometry_;
    std::unique_ptr<MissionMuzzle> mission_muzzle_;
    MissionInputLiveBindings mission_input_sources_;
    std::unique_ptr<InputBridge> mission_input_;
    Fixed3 mission_listener_{};
    std::uint32_t mission_listener_yaw_=0;
    std::uint32_t mission_script_key_503228_=0x04960552;
    MissionCrtThreadCells mission_crt_thread_;
    std::uint32_t mission_voice_volume_67f51c_=0;
    const TerrainGeometry* mission_sound_terrain_=nullptr;
    std::unique_ptr<MissionAudioFrameCells> mission_audio_frame_;
    std::unique_ptr<MissionClientLive> mission_clients_;
    std::unique_ptr<MissionSoundRuntime> mission_sound_;
    MissionDfSocketRuntimeMemory mission_socket_memory_;
    std::recursive_mutex mission_file_mutex_;
    std::unique_ptr<MissionDfSocketRuntime> mission_sockets_;
    std::unique_ptr<MissionLineFileRuntime> mission_files_;
    MissionScriptQueueCells mission_script_queue_{};
    std::unique_ptr<MissionScriptRuntimeCells> mission_script_cells_;
    std::unique_ptr<MissionScriptRegistry> mission_scripts_;
    std::uint32_t mission_requested_8ee1d4_=0;
    std::uint32_t mission_camera_mode_6ba3ec_=0;
    Fixed3 mission_camera_position_6ba3d0_{};
    MissionResultImage mission_result_{};
    bool mission_result_retained_=false;
    std::unique_ptr<MissionSession> mission_session_;
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
