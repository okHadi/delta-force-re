#include "dfre/mission_session.hpp"
#include "dfre/mission_round_checks.hpp"
#include "dfre/mission_event_actor_flags.hpp"
#include "dfre/mission_object_update.hpp"
#include "dfre/projectile_hits.hpp"
#include "dfre/mission_type_counts.hpp"
#include "dfre/mission_type_mark.hpp"
#include "dfre/mission_actor_message.hpp"
#include "dfre/mission_animation_effect.hpp"
#include "dfre/mission_event_person.hpp"
#include "dfre/mission_client_live.hpp"
#include "dfre/mission_client_startup.hpp"
#include "dfre/mission_df_socket_runtime.hpp"
#include "dfre/mission_line_file_runtime.hpp"
#include "dfre/mission_localization.hpp"
#include <mutex>
#include "dfre/mission_object_generic.hpp"
#include "dfre/mission_object_wood_large.hpp"
#include "dfre/mission_live.hpp"
#include "dfre/mission_muzzle.hpp"
#include "dfre/player.hpp"
#include "dfre/mission_frame_services.hpp"
#include "dfre/mission_host_pump.hpp"
#include "dfre/mission_audio_frame.hpp"
#include "dfre/mission_audio_listener.hpp"
#include "dfre/mission_object_live.hpp"
#include "dfre/mission_object_cactus.hpp"
#include "dfre/mission_object_sound.hpp"
#include "dfre/mission_object_wood_small.hpp"
#include "dfre/mission_sound_runtime.hpp"
#include "dfre/mission_player_live.hpp"
#include "dfre/mission_area_live.hpp"
#include "dfre/mission_control_gate.hpp"
#include "dfre/input_bridge.hpp"
#include "dfre/profiles.hpp"
#include "dfre/mission_sound_live.hpp"
#include "dfre/mission_projectile_spawn.hpp"
#include "dfre/mission_projectile_live.hpp"
#include "dfre/mission_peer_requests.hpp"
#include "dfre/crt_random.hpp"
#include "dfre/mission_crt_thread.hpp"
#include "dfre/mission_script_registry.hpp"
#include "dfre/mission_rtxt.hpp"
#include "dfre/mission_projectile_trails.hpp"
#include "dfre/mission_terrain_types.hpp"
#include "dfre/mission_terrain_mark.hpp"
#include "dfre/mission_actor_insert_live.hpp"
#include "dfre/game_messages.hpp"
#include "dfre/mission_logic.hpp"
#include "dfre/mission_client_outbox.hpp"
#include "dfre/mission_peer_frame.hpp"
#include "dfre/mission_peer_timers.hpp"
#include "dfre/mission_peer_budget.hpp"
#include "dfre/mission_round_services.hpp"
#include "dfre/mission_behavior_zones.hpp"
#include "dfre/events.hpp"
#include "dfre/mission_object_queries.hpp"
#include "dfre/mission_object_sight.hpp"
#include "dfre/mission_event_actor_orders.hpp"
#include "dfre/mission_event_queries.hpp"
#include <chrono>
#include <algorithm>
#include <fstream>
#include <iterator>
#include <map>
#include <cstring>
#include <iostream>
#include <stdexcept>
int main(int argc,char** argv){try{
    if(argc==1){std::cout<<"Mission session requires supplied game assets\n";return 77;}
    using namespace dfre;
    if(argc!=3&&argc!=4&&argc!=5)throw std::invalid_argument("Use: native_mission_session GAME_FOLDER MISSION [missing-event-not-fired [frames]]");
    const bool event_reference_repair=argc>=4&&std::string(argv[3])=="missing-event-not-fired";
    if(argc>=4&&!event_reference_repair)throw MissionTickError("Unknown scenario input");
    const auto frame_count=argc==5?std::stoul(argv[4]):256ul;
    if(frame_count<256||frame_count>1000000)throw MissionTickError("Session frame count must be256..1000000");
    std::cout<<"event_reference_repair "<<event_reference_repair<<std::endl;
    const AssetStore store(argv[1]);MissionAssetCache cache(store);
    auto loaded_config=default_game_config(0);std::ifstream config_file(std::filesystem::path(argv[1])/"df.cfg",std::ios::binary);if(!config_file)throw MissionTickError("Supplied df.cfg is missing");
    const std::string config_text((std::istreambuf_iterator<char>(config_file)),{});load_game_config_text(loaded_config,config_text);std::uint32_t rules_word=std::uint32_t(loaded_config.attributes);
    std::cout<<"config_rules "<<rules_word<<" rules_byte "<<((rules_word>>8)&255)<<std::endl;
    auto resources=cache.load_runtime(argv[2]);cache.load_projectile_runtime(resources);
    const auto terrain=load_terrain_runtime(store,argv[2],32);
    const auto start=initial_player(resources.mission,terrain.geometry);
    if(!start)throw std::runtime_error("Mission has no single-player start");
    MissionInitConfig config;config.mode=mission_theme(resources.mission);config.night=mission_night(resources.mission);
    // Source peer identity is set after the shared cold memory owner exists.
    config.scale_health=(rules_word&0x100)!=0;config.squad_pass=(rules_word&0x400)!=0;config.powerup_pass=(rules_word&0x800)==0;
    MissionTickState state;state.world.terrain=&terrain.geometry;
    Fixed3 listener{};std::uint32_t listener_yaw=0;MissionAudioFrameCells audio{0,listener,listener_yaw};
    std::ifstream exe_file(std::filesystem::path(argv[1])/"Df.exe",std::ios::binary);if(!exe_file)throw MissionTickError("Original executable is missing");const AssetBytes executable((std::istreambuf_iterator<char>(exe_file)),{});
    const MissionRuntimeState* current_initial=nullptr;MissionSession* current_session=nullptr;
    std::optional<MissionActorInsertBindings> extra_memory;std::uint32_t extra_count=0;
    std::uint32_t script_key=0x04960552;MissionCrtThreadCells crt_cells;const std::uint32_t source_thread=0x19000000;
    MissionScriptQueueCells scripts{};MissionScriptRuntimeCells script_cells{scripts,state.globals.mission_67f528};
    MissionClientLive clients;std::unique_ptr<MissionDfSocketRuntimeMemory> socket_memory;std::unique_ptr<MissionDfSocketRuntime> sockets;std::unique_ptr<MissionLineFileRuntime> line_owner;std::recursive_mutex file_mutex;
    std::unique_ptr<MissionSoundRuntime> sound_owner;std::unique_ptr<MissionScriptRegistry> registry;
    std::function<void(std::uint32_t,const std::array<std::uint8_t,10>&)> script_message;
    auto sound_host=mission_crt_thread_bindings(mission_sound_live_bindings(state,[&]{return script_key;}),crt_cells,source_thread);
    const auto base_record=sound_host.record_base;const auto base_read=sound_host.record_read;const auto base_store=sound_host.record_store;const auto base_actor=sound_host.actor_word;
    sound_host.record_base=[&,base_record](auto at,auto width)->std::optional<std::uint32_t>{if(current_session&&current_session->claims(at,width))return at;if(at==0xc23810&&width==4)return at;if(current_initial&&at>=0x18000000&&std::uint64_t(at-0x18000000)+width<=current_initial->definitions.size()*sizeof(RuntimeItemDefinition))return at;if(clients.claims(at,width))return at;if(sockets&&sockets->claims(at,width))return at;if(const auto base=mission_extra_actor_record_base(at,width)){if(!extra_memory)throw MissionTickError("Canonical extra memory is not prepared");return base;}return base_record(at,width);};
    sound_host.record_read=[&,base_read](auto base,auto offset,auto width){if(current_session&&current_session->claims(base+offset,width))return current_session->read(base+offset,width);if(base+offset==0xc23810&&width==4)return 0x18000000u;if(current_initial&&base+offset>=0x18000000&&std::uint64_t(base+offset-0x18000000)+width<=current_initial->definitions.size()*sizeof(RuntimeItemDefinition)){std::uint32_t result=0;const auto* bytes=reinterpret_cast<const std::uint8_t*>(current_initial->definitions.data());for(unsigned i=0;i<width;++i)result|=std::uint32_t(bytes[base+offset-0x18000000+i])<<(8*i);return result;}if(clients.claims(base+offset,width))return clients.read(base+offset,width);if(sockets&&sockets->claims(base+offset,width))return sockets->read(base+offset,width);if(base>=0x822e80&&base<0x867e80){if(!extra_memory||offset>0x114||width>0x114-offset||mission_extra_actor_record_base(base,1)!=base)throw MissionTickError("Extra sound record range is invalid");return extra_memory->read(base+offset,width);}return base_read(base,offset,width);};
    sound_host.record_store=[&,base_store](auto base,auto offset,auto width,auto value){if(current_session&&current_session->claims(base+offset,width)){current_session->write(base+offset,width,value);return;}if(clients.claims(base+offset,width)){clients.write(base+offset,width,value);return;}if(sockets&&sockets->claims(base+offset,width)){sockets->write(base+offset,width,value);return;}if(base>=0x822e80&&base<0x867e80){if(!extra_memory||offset>0x114||width>0x114-offset||mission_extra_actor_record_base(base,1)!=base)throw MissionTickError("Extra sound record range is invalid");extra_memory->write(base+offset,width,value);return;}base_store(base,offset,width,value);};
    sound_host.actor_word=[&,base_actor](auto actor,auto offset){if(actor>=0x822e80&&actor<0x867e80){if(!extra_memory||offset>0x110||mission_extra_actor_record_base(actor,1)!=actor)throw MissionTickError("Extra sound actor range is invalid");return extra_memory->read(actor+offset,4);}return base_actor(actor,offset);};
    sound_host.service=[&](auto site,const auto& args)->std::uint32_t{
        if(site==0x42eba0){if(!args.empty()||!current_session)throw MissionTickError("End events lost their current Session");current_session->end_events();return 0;}
        if(current_session)if(const auto value=current_session->hud_service(site,args))return *value;
        if(line_owner)if(const auto value=line_owner->service(site,args))return *value;
        if(registry){if(const auto value=registry->service(site,args))return *value;}
        if(site==0x499f1f)return mission_rtxt_load(sound_owner->asset_bindings(),args.at(0));
        if(site==0x49a076)return mission_rtxt_entry(sound_owner->asset_bindings(),args.at(0),args.at(1));
        if(site==0x49a092)return mission_rtxt_lookup(sound_owner->asset_bindings(),args.at(0),args.at(1),args.at(2));
        throw MissionTickError("Sound service has no live owner "+std::to_string(site));};
    sound_owner=std::make_unique<MissionSoundRuntime>(store,executable,audio,state.world.terrain,sound_host);auto& sound=*sound_owner;const auto& sound_memory=sound.asset_bindings();
    socket_memory=std::make_unique<MissionDfSocketRuntimeMemory>(MissionDfSocketRuntimeMemory{sound_memory.read,sound_memory.write});sockets=std::make_unique<MissionDfSocketRuntime>(*socket_memory);
    MissionLineFileRuntimeBindings locks;locks.lock=[&](auto){file_mutex.lock();};locks.unlock=[&](auto){file_mutex.unlock();};const auto startup_scratch=sound_memory.call(0x497f60,{0x2000,4,0,0});line_owner=std::make_unique<MissionLineFileRuntime>(argv[1],sound_memory,startup_scratch,0x2000,locks);
    MissionClientStartupBindings startup;startup.read=sound_memory.read;startup.write=sound_memory.write;startup.call=[&](auto site,const auto& args)->std::uint32_t{if(site==0x49a620){const auto count=args.at(2),byte=args.at(1)&255,word=byte*0x01010101u;std::uint32_t i=0;for(;count-i>=4;i+=4)sound_memory.write(args.at(0)+i,4,word);for(;i<count;++i)sound_memory.write(args.at(0)+i,1,byte);return args.at(0);}if(site==0x404cf0){MissionLocalizationBindings localized;localized.read=sound_memory.read;localized.lookup_49a092=[](auto,auto,auto)->std::uint32_t{throw MissionTickError("Cold startup text table must be unloaded");};return mission_localized_text(localized,args.at(0),args.at(1));}if(site==0x454df0)return mission_df_socket_resolve(sockets->bindings(),args.at(0));if(site==0x454bb0)return mission_df_socket_start(sockets->bindings(),args.at(0),args.at(1),args.at(2));if(const auto result=line_owner->service(site,args))return *result;throw MissionTickError("Client startup required call has no owner "+std::to_string(site));};mission_client_startup(startup);std::cout<<"client_startup actual43FF30 canonical17 socket_count "<<sockets->socket_count()<<std::endl;
    const auto script_scratch=sound_memory.call(0x497f60,{0x4000,4,0,0});
    MissionScriptRegistryBindings script_host;script_host.crt_thread=[&]{return source_thread;};script_host.voice_volume=[&]{return std::uint32_t(loaded_config.dialog_volume);};
    script_host.message=[&](auto id,const auto& packet){if(!script_message)throw MissionTickError("Script message outbox is not installed");script_message(id,packet);};
    script_host.missing_audio=[](auto name){throw MissionTickError("Script audio is missing at "+std::to_string(name));};
    registry=std::make_unique<MissionScriptRegistry>(script_cells,sound.bank_bindings(),script_scratch,0x4000,script_host);
    for(std::size_t i=0;i<resources.mission.header.size();++i)sound_memory.write(0x8cf908+std::uint32_t(i),1,resources.mission.header[i]);
    std::cout<<"sound_header_source BMS268 dialogue "<<std::string(reinterpret_cast<const char*>(resources.mission.header.data()+0x64))<<std::endl;sound.initialize_definitions("LOOP.DEF");
    sound.resolve_definitions("SNIPER.PWF","NEW.PWF");
    const ItemSoundResolver sound_resolver=[&](std::string_view name){return sound.definition_id(name);};
    const auto profile=PlayerProfile::empty(load_profile_defaults(argv[1]),"DFRE.PLY");
    const auto profile_word=[&](std::size_t at){std::uint32_t value;std::memcpy(&value,profile.raw().data()+at,4);return value;};
    WeaponState starting_weapons;starting_weapons.loadout({profile_word(0x680),profile_word(0x684),profile_word(0x688)});starting_weapons.select_slot(3);config.weapon_slot_value=starting_weapons.current_weapon();
    std::cout<<"profile new_stock_default loadout "<<profile_word(0x680)<<','<<profile_word(0x684)<<','<<profile_word(0x688)<<" weapon "<<unsigned(starting_weapons.current_weapon())<<std::endl;
    config.player_object_id=sound.asset_bindings().read(0x50133c,4);std::cout<<"local_peer_identity "<<config.player_object_id<<std::endl;
    auto initial=assemble_mission_runtime(resources,cache.items(),terrain.geometry,nullptr,config,sound_resolver,[&](std::string_view name)->const Model&{return cache.model(name);});
    current_initial=&initial;
    std::size_t ambient_bound=0;for(const auto& item:initial.definitions)for(auto handle:item.ambient_handles)ambient_bound+=handle!=0;std::cout<<"ambient_bound "<<ambient_bound<<std::endl;if(!ambient_bound)throw MissionTickError("Stock ambient item names have no bindings");

    MissionAnimations animations(initial,cache);MissionGeometry geometry(initial,cache,store);
    MissionMuzzle muzzle(initial,cache,animations);
    for(std::uint32_t index=0;index<kWeaponCount;++index){
        const auto& source=weapon_definitions()[index];const auto& binding=muzzle.weapon(index);
        if(binding.primary_definition!=runtime_item_index(initial.definitions,source.primary_item) ||
           binding.secondary_definition!=runtime_item_index(initial.definitions,source.secondary_item))
            throw std::runtime_error("Muzzle weapon bindings differ from the loaded item table");
    }
    for(const auto& model:initial.models){
        const auto source=store.read(model.name).bytes;std::int32_t height;
        if(source.size()<0x6c)throw std::runtime_error("Model header is incomplete");
        std::memcpy(&height,source.data()+0x68,4);
        if(geometry.height_68(model.handle)!=height)throw std::runtime_error("Area model height differs from its source");
    }
    init_mission_tick(state,{},initial,0);
    prepare_mission_extra_actor_pool(state);
    constexpr std::uint32_t definition_base=0x18000000,insert_source=0x25000000;
    std::uint8_t* insert_image=nullptr;std::size_t insert_bytes=0;
    MissionActorInsertBindings extra_delegated;
    extra_delegated.read=[&](auto at,auto width){std::uint32_t value=0;if(at>=insert_source&&std::uint64_t(at-insert_source)+width<=insert_bytes&&insert_image){for(unsigned i=0;i<width;++i)value|=std::uint32_t(insert_image[at-insert_source+i])<<(i*8);return value;}if(at>=definition_base&&std::uint64_t(at-definition_base)+width<=initial.definitions.size()*sizeof(RuntimeItemDefinition)){const auto* bytes=reinterpret_cast<const std::uint8_t*>(initial.definitions.data());for(unsigned i=0;i<width;++i)value|=std::uint32_t(bytes[at-definition_base+i])<<(i*8);return value;}throw MissionTickError("Extra insertion delegated read has no owner");};
    extra_memory=mission_actor_insert_live_bindings(state,extra_count,extra_delegated,[&](auto pointer){if(pointer<definition_base||(pointer-definition_base)%sizeof(RuntimeItemDefinition))throw MissionTickError("Extra definition pointer is unaligned");const auto index=(pointer-definition_base)/sizeof(RuntimeItemDefinition);if(index>=initial.definitions.size())throw MissionTickError("Extra definition pointer exceeds current table");return std::uint32_t(index);});
    state.weapons=std::move(starting_weapons);
    bind_mission_linked_position(state);
    const auto matrices=initialized_mission_matrices(initial,cache,animations);
    bind_mission_animations(state,animations);bind_mission_geometry(state,geometry,matrices);bind_mission_actor_collision(state,geometry,{},matrices);
    state.world.terrain=&terrain.geometry;state.world.water_level=terrain.water_height;

    // Current script registry uses the supplied dialogue files. Music is disabled.
    // Loaded stock actor/resource/terrain records remain exact input data.
    ActionBindings keys{};for(std::size_t i=0;i<keys.size();++i){keys[i]={profile_word(0x6a0+i*12),profile_word(0x6a4+i*12),profile_word(0x6a8+i*12)};}InputBridge bridge(keys);
    const auto pe_word=[&](std::uint32_t at,unsigned width){std::uint32_t value=0;for(unsigned i=0;i<width;++i)value|=std::uint32_t(executable.at(at+i))<<(8*i);return value;};
    bridge.set_query_row_source([&](std::size_t row)->std::optional<CommandBinding>{if(row!=0x8c)throw MissionTickError("Unbound loaded input query row");const auto va=0x4fb2fcu+std::uint32_t(row)*0x2c;const auto pe=pe_word(0x3c,4),sections=pe+24+pe_word(pe+20,2);for(unsigned i=0;i<pe_word(pe+6,2);++i){const auto section=sections+i*40,rva=pe_word(section+12,4),size=pe_word(section+16,4);if(va-0x400000>=rva&&va-0x400000-rva+0x2c<=size){const auto off=pe_word(section+20,4)+va-0x400000-rva;return CommandBinding{pe_word(off,4),{},std::uint16_t(pe_word(off+0x24,2)),std::uint16_t(pe_word(off+0x26,2)),std::uint16_t(pe_word(off+0x28,2)),std::uint16_t(pe_word(off+0x2a,2))};}}throw MissionTickError("Loaded input query row has no executable bytes");});
    std::cout<<"input forwardkey "<<keys[2].key<<" firekey "<<keys[8].key<<" firemouse "<<keys[8].mouse<<std::endl;
    InputSourceFlags flags;flags.window_active=true;bridge.set_source_flags(flags);
    state.world.trigger_held_47a290=[&](auto row){return bridge.query_active(row);};

    for(std::size_t slot=0;slot<initial.placements.size();++slot){const auto& row=initial.placements[slot];const auto base=MissionClientLive::source_address(slot);clients.write(base,4,slot?std::uint32_t(slot):sound_memory.read(0x50133c,4));clients.write(base+0x18c,1,row.active);clients.write(base+0x1c4,2,row.team_code);if(row.actor)clients.write(base+8,4,mission_actor_source_address(3,*row.actor));}
    state.projectiles.resize(512);
    std::uint32_t requested=0;Fixed3 camera_position{};
    MissionSessionHost host;host.pump_490880=[](){return true;};host.rdtsc=[](){return std::uint64_t(std::chrono::steady_clock::now().time_since_epoch().count());};host.elapsed_audio_frames=[](){return 1u;};host.camera_actor_6ba3f0=[&](){return actor_handle(3,*state.config.local_actor);};host.camera_mode_6ba3ec=[](){return 0u;};
    std::size_t repaired=0;host.event_references=event_reference_repair?EventReferencePolicy::missing_not_fired:EventReferencePolicy::strict;host.event_diagnostic=[&](auto requested_index,auto loaded){if(requested_index!=74||loaded!=74)throw MissionTickError("Unexpected event repair");++repaired;};
    MissionResultImage retained_result{};
    auto session_owner=std::make_unique<MissionSession>(MissionSessionContext{state,initial,resources.mission,cache,terrain,animations,geometry,muzzle,bridge,sound,*registry,audio,scripts,crt_cells,clients,*extra_memory,rules_word,requested,camera_position},host,retained_result);
    auto& session=*session_owner;
    if(sockets->claims(0x18000000,4)||sockets->claims(0x19000000,4)||sockets->claims(0x1b000000,4)||sockets->claims(0x1c000000,4)||clients.claims(0x19000000,4)||session.claims(0x18000000,4)||session.claims(0x19000000,4)||session.claims(0x1b000000,4)||!session.claims(0x1c000000,1))throw MissionTickError("Virtual source owners overlap");
    std::uint32_t first_definition_word;std::memcpy(&first_definition_word,initial.definitions.data(),4);
    if(sound_memory.read(0x1900000c,4)!=crt_cells.seed||sound_memory.read(0x18000000,4)!=first_definition_word)throw MissionTickError("Canonical CRT or definition route differs");
    current_session=&session;script_message=[&](auto id,const auto& packet){session.script_message(id,packet.data(),packet.size());};
    bridge.set_query_row_source([&](auto row){return mission_session_input_query_row(sound,row);});
    const auto fail=[&](auto body){bool failed=false;try{body();}catch(const MissionTickError&){failed=true;}if(!failed)throw MissionTickError("Expected session boundary error");};
    fail([&]{session.read(0x100,4);});fail([&]{session.claims(0xa158a3,2);});fail([&]{session.write(0xa158bc,3,1);});
    if(sound_memory.read(0xbd5248,4)!=std::uint32_t(state.world.water_level)||sound_memory.read(0xbd52ac,4)!=0x1c000000||sound_memory.read(0x1c000000,1)!=terrain.color.pixels[0])throw MissionTickError("Session terrain source alias differs");fail([&]{session.write(0x1c000000,1,0);});
    const auto initial_damage=session.read(0xa158bc,4);session.write(0xa158bd,1,0x12);if(sound_memory.read(0xa158bd,1)!=0x12)throw MissionTickError("Session sound source alias differs");session.write(0xa158bc,4,initial_damage);
    const auto saved_world_stage=state.world.external_stage;unsigned restored_world=0,old_update_calls=0,old_secondary_calls=0;
    state.world.external_stage=[&](auto&,const MissionStageCall& call)->std::optional<std::uint32_t>{if(call.address==0x0badf010){++restored_world;return 123u;}return {};};
    MissionStageCall restore_probe{};restore_probe.address=0x0badf010;
    const auto base_updates=state.update_callbacks.size(),base_secondary=state.secondary_callbacks.size();
    state.update_callbacks.emplace(0x429bc0,[&](auto&,const auto&,const auto&){++old_update_calls;});
    fail([&]{session.install();});if(session.installed()||state.update_callbacks.size()!=base_updates+1||state.secondary_callbacks.size()!=base_secondary)throw MissionTickError("Same-ID update rejection changed callback owners");
    state.update_callbacks.at(0x429bc0)(state,{0,0},AiActorAction{});if(old_update_calls!=1||state.world.external_stage(state,restore_probe)!=123u)throw MissionTickError("Same-ID update rejection did not restore the old closure");state.update_callbacks.erase(0x429bc0);
    state.secondary_callbacks.emplace(0x429ea0,[&](auto&,const auto&,const auto&){++old_secondary_calls;});
    fail([&]{session.install();});if(session.installed()||state.update_callbacks.size()!=base_updates||state.secondary_callbacks.size()!=base_secondary+1||state.update_callbacks.count(0x429bc0))throw MissionTickError("Mid-install duplicate error left a session callback");
    state.secondary_callbacks.at(0x429ea0)(state,{0,0},AiActorAction{});if(old_secondary_calls!=1||state.world.external_stage(state,restore_probe)!=123u||restored_world!=2)throw MissionTickError("Mid-install duplicate error did not restore the old closure");state.secondary_callbacks.erase(0x429ea0);state.world.external_stage=saved_world_stage;
    const auto tokenizer_before=crt_cells.tokenizer_cursor;
    state.update_callbacks.emplace(0x0badf00d,[](auto&,const auto&,const auto&){});
    const auto prior_updates=state.update_callbacks.size(),prior_secondary=state.secondary_callbacks.size();
    session.install();if(!session.installed())throw MissionTickError("Session did not install");
    try{for(unsigned i=0;i<frame_count;++i){if(i==0)bridge.feed(InputEvent{InputKind::key_down,int(keys[2].key)});if(i==4)bridge.feed(InputEvent{InputKind::key_up,int(keys[2].key)});if(i==16)bridge.feed(InputEvent{InputKind::key_down,int(keys[8].key)});if(i==20)bridge.feed(InputEvent{InputKind::key_up,int(keys[8].key)});const auto report=session.step();std::cout<<"session_frame "<<report.tick<<" ammo "<<state.weapons.magazine(state.weapons.current_definition().weapon_class)<<std::endl;}}
    catch(const std::exception& error){std::cout<<"session_stop "<<state.tick_67dbf4<<" "<<error.what()<<std::endl;session.uninstall();current_session=nullptr;if(!event_reference_repair&&state.tick_67dbf4==48)return 0;throw;}
    std::cout<<"session_final ammo "<<state.weapons.magazine(state.weapons.current_definition().weapon_class)<<" repairs "<<repaired<<std::endl;
    if((frame_count==256&&crt_cells.seed!=662824084)||crt_cells.tokenizer_cursor!=tokenizer_before)throw MissionTickError("Session temporary views changed CRT cells");
    if(state.weapons.magazine(state.weapons.current_definition().weapon_class)!=27||(frame_count==256?repaired!=2:repaired<2))throw MissionTickError("Session final state differs");
    const auto shots_before_result=session.read(0xa158a4,4);
    if(frame_count>=2048&&shots_before_result<=3)throw MissionTickError("Long session did not create enemy projectiles");
    MissionLogicResultPacket result_packet{};result_packet.fill(0xa5);
    unsigned active_result_clients=0,expected_result_rows=0;for(std::size_t slot=0;slot<17;++slot)if(clients.read(MissionClientLive::source_address(slot,0x18c),1)){++active_result_clients;if(!clients.read(MissionClientLive::source_address(slot,0x18d),1)||session.read(0x67f52c,4))++expected_result_rows;}
    if(!active_result_clients)throw MissionTickError("Result request has no current active client for publication");
    session.request_result(1,result_packet);
    if(result_packet[1]!=expected_result_rows||result_packet[2]!=0xa5||session.read(0x67dbc0,4)!=1)throw MissionTickError("Session result request lost caller padding or menu state");
    session.step();session.step();
    if(session.read(0x67f718,4)!=1||session.read(0x67f71c,4)!=result_packet[1])throw MissionTickError("Session result did not reach the current result image");
    std::cout<<"session_result mode1 rows "<<session.read(0x67f71c,4)<<" source_projectile_ring "<<shots_before_result<<std::endl;
    sound_memory.call(0x42eba0,{});
    std::cout<<"session_end_events current_live_owners selected "<<std::count_if(resources.mission.events.begin(),resources.mission.events.end(),[](const auto& event){return (event.words[0]&4u)!=0;})<<"\n";
    state.update_callbacks.emplace(0x0badf00e,[](auto&,const auto&,const auto&){});
    session.uninstall();current_session=nullptr;if(state.update_callbacks.size()!=prior_updates+1||state.secondary_callbacks.size()!=prior_secondary||!state.update_callbacks.count(0x0badf00d)||!state.update_callbacks.count(0x0badf00e))throw MissionTickError("Session removed another callback owner");if(session.installed())throw MissionTickError("Session did not uninstall");
    bool stopped_end_events=false;try{session.end_events();}catch(const MissionTickError&){stopped_end_events=true;}if(!stopped_end_events)throw MissionTickError("End events used an uninstalled Session");
    MissionResultImage expected_result{};for(std::size_t i=0;i<expected_result.size();++i)expected_result[i]=std::uint8_t(session.read(result_state_block+std::uint32_t(i),1));
    if(retained_result!=expected_result)throw MissionTickError("Session result has a second byte owner");
    session_owner.reset();if(retained_result!=expected_result||retained_result[4]!=1)throw MissionTickError("Mission result did not survive Session teardown");
    // Explicit end-event input uses the same live actors, clients and message owners.
    auto end_fixture=resources.mission;MissionEvent end_event;end_event.words={4,0,0,0,0,1u<<16};
    end_fixture.events={end_event};end_fixture.triggers.clear();end_fixture.actions={{0,8,0,0,0,0,0,0}};
    auto end_session=std::make_unique<MissionSession>(MissionSessionContext{state,initial,end_fixture,cache,terrain,animations,geometry,muzzle,bridge,sound,*registry,audio,scripts,crt_cells,clients,*extra_memory,rules_word,requested,camera_position},host,retained_result);
    current_session=end_session.get();script_message=[&](auto id,const auto& packet){current_session->script_message(id,packet.data(),packet.size());};
    end_session->install();end_session->write(0x67dbc0,4,0);sound_memory.call(0x42eba0,{});
    if(end_session->read(0x67dbc0,4)!=1)throw MissionTickError("End event did not reach the live result publisher");
    end_session->uninstall();current_session=nullptr;end_session.reset();
    std::cout<<"session_end_event_fixture live_result_publisher\n";
    std::cout<<"session_pass"<<frame_count<<" ammo27 namedrepair retainedresult device0\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
