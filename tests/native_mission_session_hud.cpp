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
#include "dfre/campaign.hpp"
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
#include <cstdio>
int main(int argc,char** argv){try{
    if(argc==1){std::cout<<"Mission session requires supplied game assets\n";return 77;}
    using namespace dfre;
    if(argc!=3)throw std::invalid_argument("Use: native_mission_session_hud GAME_FOLDER MISSION");
    const bool event_reference_repair=false;
    if(argc>=4&&!event_reference_repair)throw MissionTickError("Unknown scenario input");
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
    bool mutate_laser_metadata=false;unsigned laser_lookup_calls=0;
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
        if(current_session)if(const auto value=current_session->hud_service(site,args))return *value;
        if(line_owner)if(const auto value=line_owner->service(site,args))return *value;
        if(registry){if(const auto value=registry->service(site,args))return *value;}
        if(site==0x404cf0){const auto& m=sound_owner->asset_bindings();if(mutate_laser_metadata){++laser_lookup_calls;if(!current_session)throw MissionTickError("Laser callback has no current session");current_session->write(0x67db54,4,0x50607080);}return mission_localized_text({m.read,[&](auto table,auto group,auto key){return mission_rtxt_lookup(m,table,group,key);}},args.at(0),args.at(1));}
        if(site==0x499f1f)return mission_rtxt_load(sound_owner->asset_bindings(),args.at(0));
        if(site==0x49a076)return mission_rtxt_entry(sound_owner->asset_bindings(),args.at(0),args.at(1));
        if(site==0x49a092)return mission_rtxt_lookup(sound_owner->asset_bindings(),args.at(0),args.at(1),args.at(2));
        throw MissionTickError("Sound service has no live owner "+std::to_string(site));};
    sound_owner=std::make_unique<MissionSoundRuntime>(store,executable,audio,state.world.terrain,sound_host);auto& sound=*sound_owner;const auto& sound_memory=sound.asset_bindings();
    socket_memory=std::make_unique<MissionDfSocketRuntimeMemory>(MissionDfSocketRuntimeMemory{sound_memory.read,sound_memory.write});sockets=std::make_unique<MissionDfSocketRuntime>(*socket_memory);
    MissionLineFileRuntimeBindings locks;locks.lock=[&](auto){file_mutex.lock();};locks.unlock=[&](auto){file_mutex.unlock();};const auto startup_scratch=sound_memory.call(0x497f60,{0x2000,4,0,0});line_owner=std::make_unique<MissionLineFileRuntime>(argv[1],sound_memory,startup_scratch,0x2000,locks);
    MissionClientStartupBindings startup;startup.read=sound_memory.read;startup.write=sound_memory.write;startup.call=[&](auto site,const auto& args)->std::uint32_t{if(site==0x49a620){const auto count=args.at(2),byte=args.at(1)&255,word=byte*0x01010101u;std::uint32_t i=0;for(;count-i>=4;i+=4)sound_memory.write(args.at(0)+i,4,word);for(;i<count;++i)sound_memory.write(args.at(0)+i,1,byte);return args.at(0);}if(site==0x404cf0){MissionLocalizationBindings localized;localized.read=sound_memory.read;localized.lookup_49a092=[](auto,auto,auto)->std::uint32_t{throw MissionTickError("Cold startup text table must be unloaded");};return mission_localized_text(localized,args.at(0),args.at(1));}if(site==0x454df0)return mission_df_socket_resolve(sockets->bindings(),args.at(0));if(site==0x454bb0)return mission_df_socket_start(sockets->bindings(),args.at(0),args.at(1),args.at(2));if(const auto result=line_owner->service(site,args))return *result;throw MissionTickError("Client startup required call has no owner "+std::to_string(site));};mission_client_startup(startup);std::cout<<"client_startup actual43FF30 canonical17 socket_count "<<sockets->socket_count()<<std::endl;
    for(const auto& source:std::array<std::pair<std::uint32_t,std::uint32_t>,3>{{{0x50322c,0x4c80a8},{0x503234,0x4c80b4},{0x503230,0x4c80dc}}}){const auto table=mission_rtxt_load(sound_memory,source.second);if(!table)throw MissionTickError("Required game text table failed to load");sound_memory.write(source.first,4,table);}
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
    const CampaignCatalog campaigns(store);std::string campaign_name;for(const auto& campaign:campaigns.campaigns())for(const auto& mission:campaign.missions)if(mission.filename==argv[2])campaign_name=campaign.text_file;if(campaign_name.empty())throw MissionTickError("Supplied mission has no current campaign text selection");const auto campaign_path=sound_memory.call(0x497f60,{std::uint32_t(campaign_name.size()+1),4,0,0});for(std::size_t i=0;i<=campaign_name.size();++i)sound_memory.write(campaign_path+std::uint32_t(i),1,i<campaign_name.size()?std::uint8_t(campaign_name[i]):0);if(!sound_memory.call(0x499040,{campaign_path}))throw MissionTickError("Selected campaign RTXT is absent");const auto campaign_table=mission_rtxt_load(sound_memory,campaign_path);sound_memory.call(0x4980d0,{campaign_path});if(!campaign_table)throw MissionTickError("Selected campaign RTXT failed to load");sound_memory.write(0x503238,4,campaign_table);
    WeaponState starting_weapons;starting_weapons.loadout({profile_word(0x680),profile_word(0x684),profile_word(0x688)});starting_weapons.select_slot(3);config.weapon_slot_value=starting_weapons.current_weapon();
    std::cout<<"profile new_stock_default loadout "<<profile_word(0x680)<<','<<profile_word(0x684)<<','<<profile_word(0x688)<<" weapon "<<unsigned(starting_weapons.current_weapon())<<std::endl;
    config.player_object_id=sound.asset_bindings().read(0x50133c,4);std::cout<<"local_peer_identity "<<config.player_object_id<<std::endl;
    auto initial=assemble_mission_runtime(resources,cache.items(),terrain.geometry,nullptr,config,sound_resolver);
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
    // Controlled prior HUD contents prove the source cold-clear boundary.
    sound_memory.write(0xbd34b0,1,'S');sound_memory.write(0xbd4810,4,99);sound_memory.write(0xbd4814,4,10);sound_memory.write(0xbd4818,4,20);sound_memory.write(0xbd481c,4,30);state.globals.command_tick_6e471c=0x12345678;
    MissionSession session({state,initial,resources.mission,cache,terrain,animations,geometry,muzzle,bridge,sound,*registry,audio,scripts,crt_cells,clients,*extra_memory,rules_word,requested,camera_position},host);
    if(sockets->claims(0x18000000,4)||sockets->claims(0x19000000,4)||sockets->claims(0x1b000000,4)||sockets->claims(0x1c000000,4)||clients.claims(0x19000000,4)||session.claims(0x18000000,4)||session.claims(0x19000000,4)||session.claims(0x1b000000,4)||!session.claims(0x1c000000,1))throw MissionTickError("Virtual source owners overlap");
    std::uint32_t first_definition_word;std::memcpy(&first_definition_word,initial.definitions.data(),4);
    if(sound_memory.read(0x1900000c,4)!=crt_cells.seed||sound_memory.read(0x18000000,4)!=first_definition_word)throw MissionTickError("Canonical CRT or definition route differs");
    current_session=&session;script_message=[&](auto id,const auto& packet){session.script_message(id,packet.data(),packet.size());};
    bridge.set_query_row_source([&](auto row){return mission_session_input_query_row(sound,row);});
    const auto fail=[&](auto body){bool failed=false;try{body();}catch(const MissionTickError&){failed=true;}if(!failed)throw MissionTickError("Expected session boundary error");};
    fail([&]{session.read(0x100,4);});fail([&]{session.claims(0xa158a3,2);});fail([&]{session.write(0xa158bc,3,1);});
    if(sound_memory.read(0xbd5248,4)!=std::uint32_t(state.world.water_level)||sound_memory.read(0xbd52ac,4)!=0x1c000000||sound_memory.read(0x1c000000,1)!=terrain.color.pixels[0])throw MissionTickError("Session terrain source alias differs");fail([&]{session.write(0x1c000000,1,0);});
    const auto initial_damage=session.read(0xa158bc,4);session.write(0xa158bd,1,0x12);if(sound_memory.read(0xa158bd,1)!=0x12)throw MissionTickError("Session sound source alias differs");session.write(0xa158bc,4,initial_damage);
    if(!bridge.stage().push_mouse_pulse(2))throw MissionTickError("HUD cold test could not queue its current fire control");fail([&]{session.install();});if(bridge.stage().queue().size()!=1||session.read(0xbd34b0,1)!='S'||session.read(0xbd4814,4)!=10)throw MissionTickError("Rejected cold install lost queued input or cleared HUD state");const auto held_command=bridge.pop_due(0);if(!held_command||held_command->id!=140||bridge.stage().queue().size())throw MissionTickError("Cold install did not preserve the current input command");
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
    state.update_callbacks.emplace(0x0badf00d,[](auto&,const auto&,const auto&){});
    const auto prior_updates=state.update_callbacks.size(),prior_secondary=state.secondary_callbacks.size();
    session.install();if(!session.installed())throw MissionTickError("Session did not install");
    for(std::uint32_t i=0;i<0x1360;++i)if(session.read(0xbd34b0+i,1))throw MissionTickError("Source cold HUD clear retained history");
    if(session.read(0xbd4810,4)||session.read(0xbd4814,4)!=0xffffffffu||session.read(0xbd4818,4)!=0xffffffffu||session.read(0xbd481c,4)!=0xffffffffu||bridge.stage().focus_flag()||state.globals.tick_base_bd4844!=state.globals.command_tick_6e471c)throw MissionTickError("Source cold clear/activate order differs");
    const auto source_text=[&](std::uint32_t pointer){std::string text;for(std::uint32_t i=0;i<65536;++i){const auto byte=sound_memory.read(pointer+i,1);if(!byte)return text;text.push_back(char(byte));}throw MissionTickError("Owned HUD test text has no terminator");};
    const auto assert_message=[&](std::uint32_t text,std::uint32_t metadata){const auto current=source_text(text);const auto count=std::min<std::size_t>(119,current.size());for(std::size_t i=0;i<count;++i)if(session.read(0xbd34b0+std::uint32_t(i),1)!=std::uint8_t(current[i]))throw MissionTickError("HUD bytes differ from current source text");if(session.read(0xbd34b0+std::uint32_t(count),1)||session.read(0xbd3528,4)!=metadata||session.read(0xbd4814,4)!=0x4d8)throw MissionTickError("HUD message metadata, terminator or shared timer differs");};
    const auto dialogue=registry->memory().read(0x8e3f3c,4);if(!dialogue)throw MissionTickError("Actual script RTXT has no current owner");std::uint32_t script_text=0;for(std::uint32_t i=0;i<sound_memory.read(dialogue+12,4);++i){const auto entry=mission_rtxt_entry(sound_memory,dialogue,i);const auto pointer=sound_memory.read(entry,4);if(pointer&&sound_memory.read(pointer,1)){script_text=pointer;break;}}if(!script_text)throw MissionTickError("Actual script RTXT has no nonempty text");if(!registry->service(0x43e470,{script_text}))throw MissionTickError("Actual43E470 text body is absent");assert_message(script_text,0xffffffffu);
    MissionStageCall timer{};timer.address=0x479ed0;if(!state.world.external_stage(state,timer)||session.read(0xbd4814,4)!=0x4d7)throw MissionTickError("HUD timer is separate from the frame owner");
    std::uint32_t triggered_id=0,triggered_text=0;const auto directory=sound_memory.read(campaign_table+4,4),group_count=sound_memory.read(directory,4),rows=directory+4;std::uint32_t group_name=rows+group_count*8;
    for(std::uint32_t group=0;group<group_count;++group){const auto group_string=source_text(group_name);const auto count=sound_memory.read(rows+group*8+4,4);if(group_string=="Triggered Text"){auto key=sound_memory.read(rows+group*8,4);for(std::uint32_t index=0;index<count;++index){const auto name=source_text(key);if(name.size()>2&&name.substr(0,2)=="ID"){std::size_t used=0;const auto value=std::stoul(name.substr(2),&used,10);char formatted[32];std::snprintf(formatted,sizeof(formatted),"ID%03i",int(value));if(used==name.size()-2&&name==formatted){const auto entry=mission_rtxt_lookup(sound_memory,campaign_table,group_name,key);const auto text=sound_memory.read(entry,4);if(text&&sound_memory.read(text,1)){triggered_id=std::uint32_t(value);triggered_text=text;break;}}}key+=std::uint32_t(name.size()+1);}}if(triggered_text)break;group_name+=std::uint32_t(group_string.size()+1);}
    const auto before_campaign=session.read(0xbd34b0,4),before_campaign_metadata=session.read(0xbd3528,4),before_campaign_timer=session.read(0xbd4814,4);if(!session.hud_service(0x404dd0,{triggered_id}))throw MissionTickError("Session triggered text service is absent");if(triggered_text)assert_message(triggered_text,0xffffffffu);else if(session.read(0xbd34b0,4)!=before_campaign||session.read(0xbd3528,4)!=before_campaign_metadata||session.read(0xbd4814,4)!=before_campaign_timer)throw MissionTickError("Empty real campaign lookup changed the HUD queue");if(session.claims(0x1b002000,1))throw MissionTickError("Triggered caller frame outlived its call");
    const auto old_flags=state.globals.flags_67dbc9;session.write(0x67dbc9,1,0x20);if(state.globals.flags_67dbc9!=0x20)throw MissionTickError("HUD suppression flag is not canonical");session.hud_service(0x404dd0,{triggered_id});assert_message(0x4ced1a,0xffffffffu);session.write(0x67dbc9,1,old_flags);
    const auto expected_laser=mission_localized_text({sound_memory.read,[&](auto table,auto group,auto key){return mission_rtxt_lookup(sound_memory,table,group,key);}},0x4cdc38,0x4cdc2c);std::uint32_t laser=0;for(std::uint32_t i=1;i<kWeaponCount;++i)if(weapon_definitions()[i].use_class==10){laser=i;break;}if(!laser)throw MissionTickError("Actual weapon table has no laser producer");std::array<std::uint8_t,kWeaponSlotCount> laser_slots{};laser_slots.fill(std::uint8_t(laser));state.weapons.set_slot_map(laser_slots);state.weapons.select_slot(3);session.write(0x67db54,4,0x10203040);mutate_laser_metadata=true;mission_dispatch_command(state,0x8c,0,bridge.command_table());mutate_laser_metadata=false;if(laser_lookup_calls!=1||session.read(0x67db54,4)!=0x50607080||state.weapons.fire_timer()!=std::int32_t(weapon_definitions()[laser].fire_delay))throw MissionTickError("Laser producer did not preserve current call order");assert_message(expected_laser,0x10203040);
    const auto history=session.read(0xbd34b0,4),metadata=session.read(0xbd3528,4);if(!bridge.stage().push_mouse_pulse(2))throw MissionTickError("Warm reinstall control did not queue");session.uninstall();if(state.update_callbacks.size()!=prior_updates||state.secondary_callbacks.size()!=prior_secondary)throw MissionTickError("HUD session left an owned actor callback");session.install();if(session.read(0xbd34b0,4)!=history||session.read(0xbd3528,4)!=metadata||bridge.stage().queue().size()!=1)throw MissionTickError("Warm reinstall repeated cold HUD/input initialization");session.uninstall();current_session=nullptr;if(state.update_callbacks.size()!=prior_updates||state.secondary_callbacks.size()!=prior_secondary)throw MissionTickError("Warm HUD session left an owned actor callback");
    std::cout<<"session_hud_pass sourceclear script43E470 campaign404DD0 laser483A30 sharedtimers capturedmetadata device0\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
