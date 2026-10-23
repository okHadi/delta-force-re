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
#include "dfre/mission_projectile_spawn.hpp"
#include "dfre/mission_projectile_live.hpp"
#include "dfre/mission_peer_requests.hpp"
#include "dfre/crt_random.hpp"
#include "dfre/mission_crt_thread.hpp"
#include "dfre/mission_script_registry.hpp"
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
#include <algorithm>
#include <cstring>
#include <map>
#include <limits>
#include <type_traits>

namespace dfre {namespace {using U=std::uint32_t;
static_assert(std::is_nothrow_swappable<MissionTickWorld>::value,"World callback restore must not throw");
template<class F>const F& need(const F& value,const char* error){if(!value)throw MissionTickError(error);return value;}
struct SessionEventHost : dfre::EventHost {
    std::function<std::uint32_t(std::uint32_t,std::uint32_t)> object_read;
    std::function<std::uint32_t(std::size_t)> rect;
    std::function<std::uint32_t(std::uint32_t,std::uint32_t)> linked;
    std::function<bool()> multiplayer_zero;
    std::function<std::uint32_t(std::size_t)> fired_reference;
    std::uint32_t external_event_fired(std::size_t index) override{return fired_reference(index);}
    std::function<bool()> rule_bit1;
    bool world_67f549_bit1() override{return rule_bit1();}
    std::function<void(std::uint32_t,std::uint32_t)> orders;
    std::function<void(std::uint32_t)> flags,remove,script;
    std::function<void(std::size_t)> person_edit;
    void action_4601a0(std::size_t index) override{person_edit(index);}
    void action_43f500(std::uint32_t id) override{script(id);}
    void action_45f4c0(std::uint32_t side) override{flags(side);}
    void action_45f5b0(std::uint32_t side) override{remove(side);}
    void action_45f670(std::uint32_t side,std::uint32_t list) override{orders(side,list);}
    std::uint32_t object_index_dword(std::uint32_t address,std::uint32_t index) override{return object_read(address,index);}
    std::uint32_t probe_460330(std::size_t index) override{return rect(index);}
    std::uint32_t probe_460560(std::uint32_t a,std::uint32_t b) override{return linked(a,b);}
    bool world_67f520_is_zero() override{return multiplayer_zero();}
};
}
struct MissionSession::Impl {
    MissionSessionContext c; MissionSessionHost h;
    MissionTickWorld prior_world; bool active=false;
    std::map<U,MissionTickState::ActorCallback> prior_updates,prior_secondary;
    inline static constexpr std::array<U,8> update_ids{0x429bc0,0x4519a0,0x4534a0,0x45c3a0,0x45c340,0x453810,0x4533f0,0x453930};
    inline static constexpr std::array<U,2> secondary_ids{0x429ea0,0x451cb0};
    std::array<bool,8> installed_updates{};std::array<bool,2> installed_secondary{};
    MissionGeometryMatrixSource matrices;
    MissionPlayerCells player_cells; CrtRandomBindings crt;
    std::unique_ptr<MissionPlayerLive> player;
    std::function<void(U,U,MuzzleResult&)> request_projectile;
    MissionProfilerCells profiler{0,0,std::vector<U>(32)};
    std::array<U,3> timers{}; U statistic_bcfc24=0;
    MissionCameraCells camera;
    std::unique_ptr<MissionFrameServices> frame;
    GameMessageStage messages;GameMessageLiveBindings message_bindings;
    std::array<std::uint16_t,kGameMessageDedupSlots> sequence_ring{};
    std::uint8_t received_status_c23b78=0,status_6e5884=0;
    MissionAreaLiveBindings area;MissionAudioListenerBindings listener_binding;
    std::map<U,U> listener_extra,cells;
    MissionControlGateBindings gate;std::function<void()> logic_run,reset_zones;
    MissionAnimationEffectBindings animation_effect;
    SessionEventHost event_host;EventStage event_stage{&event_host};
    MissionEventActorOrderBindings order_binding;
    ProjectileDamageState shared_damage;
    MissionObjectLiveBindings object;MissionObjectTargetBindings target_binding;
    std::function<U(std::size_t,U,unsigned)> client_read;
    std::function<void(std::size_t,U,unsigned,U)> client_store;
    std::function<U(U)> cell;std::function<void(U,U)> cell_store;
    std::function<U(U,unsigned)> shared_read;std::function<void(U,unsigned,U)> shared_store;
    MissionServerMessageBuffer server;std::unique_ptr<MissionClientOutbox> outbox;
    std::unique_ptr<MissionActorLifecycle> lifecycle;MissionEventActorFlagBindings flag_bindings;
    MissionPeerFrameBindings peer;MissionPeerTimerBindings peer_timers;
    MissionPeerBudgetBindings budget;MissionRoundServiceBindings round;
    MissionRoundCheckBindings round_checks;MissionActorMessageBindings actor_message;
    MissionLogicFrameBindings logic;
    MissionBehaviorZones zone_owner;MissionBehaviorZoneBindings zone_binding;
    MissionProjectileSpawnCells spawn_cells;std::unique_ptr<MissionProjectileSpawn> spawn;
    std::array<U,900> shot_ledger{};MissionPeerRequestBindings shots;
    MissionProjectileTrailCells trails;MissionProjectileTrailBindings trail_bindings;
    std::array<std::uint8_t,256> terrain_types{};
    MissionTerrainMarkBindings mark;MissionActorInsertBindings insertion;
    U definition_base=0; static constexpr U insert_source=0x1b000000,color_base=0x1c000000;
    U terrain_color_pointer=color_base;
    std::uint8_t* insert_image=nullptr;std::size_t insert_bytes=0;
    std::vector<AssetBytes> height_bounds;std::array<AiActorImage,256> group3_tail{};
    MissionObjectSightBindings sight_bindings;
    MissionTickInputs input;

    explicit Impl(MissionSessionContext context,MissionSessionHost host):c(context),h(std::move(host)),camera{c.camera_position_6ba3d0,{},0,0}{
        const auto& memory=c.sound.asset_bindings();
        for(U at=0x6e4730;at<=0x6e475c;at+=4)cells.emplace(at,memory.read(at,4));
        for(U at=0x6e5570;at<=0x6e558c;at+=4)cells.emplace(at,memory.read(at,4));
        for(U at=0x6e5778;at<=0x6e5880;at+=4)cells.emplace(at,memory.read(at,4));
        for(const U at:{0x50133cu,0x501340u,0x50321cu})cells.emplace(at,memory.read(at,4));
        for(const U at:{0x6e4708u,0x6e4704u,0x6e4700u,0x6e06b8u})listener_extra.emplace(at,memory.read(at,4));
        for(std::size_t i=0;i<shot_ledger.size();++i)shot_ledger[i]=memory.read(0x6e4760+U(i)*4,4);
        for(std::size_t i=0;i<sequence_ring.size();++i)sequence_ring[i]=std::uint16_t(memory.read(0xbcfc3c+U(i)*2,2));
        statistic_bcfc24=memory.read(0xbcfc24,4);received_status_c23b78=std::uint8_t(memory.read(0xc23b78,1));status_6e5884=std::uint8_t(memory.read(0x6e5884,1));
        profiler.current_4f8718=memory.read(0x4f8718,4);profiler.previous_4f871c=memory.read(0x4f871c,4);
        for(std::size_t i=0;i<profiler.totals_8e3f40.size();++i)profiler.totals_8e3f40[i]=memory.read(0x8e3f40+U(i)*4,4);
        for(std::size_t i=0;i<timers.size();++i)timers[i]=memory.read(0xbd4814+U(i)*4,4);
        shared_damage.last_kind=memory.read(0xa158c0,4);shared_damage.last_damage=memory.read(0xa158bc,4);spawn_cells.ring_index=memory.read(0xa158a4,4);trails.ring_4f9148=memory.read(0x4f9148,4);
        for(std::size_t row=0;row<trails.records_9e3098.size();++row)for(std::size_t word=0;word<8;++word)trails.records_9e3098[row][word]=memory.read(0x9e3098+U(row)*32+U(word)*4,4);
        definition_base=memory.read(0xc23810,4);if(!definition_base)throw MissionTickError("Mission definition source base is not bound");
        insertion=c.extra_memory;const auto raw_read=insertion.read;
        insertion.read=[this,raw_read](U at,U width){if(at>=insert_source&&std::uint64_t(at-insert_source)+width<=insert_bytes&&insert_image){U result=0;for(U i=0;i<width;++i)result|=U(insert_image[at-insert_source+i])<<(8*i);return result;}return need(raw_read,"Extra actor current read is not bound")(at,width);};
    }
    std::uint8_t* source_byte(U at){
        const auto field=[at](U base,auto& value)->std::uint8_t*{if(at<base||std::uint64_t(at-base)>=sizeof(value))return nullptr;return reinterpret_cast<std::uint8_t*>(&value)+(at-base);};
        for(auto& entry:cells)if(auto* p=field(entry.first,entry.second))return p;
        for(auto& entry:listener_extra)if(auto* p=field(entry.first,entry.second))return p;
        if(auto* p=field(0x6e4760,shot_ledger))return p;
        if(auto* p=field(0xbcfc3c,sequence_ring))return p;
        if(auto* p=field(0xbcfc24,statistic_bcfc24))return p;
        if(auto* p=field(0xc23b78,received_status_c23b78))return p;
        if(auto* p=field(0x6e5884,status_6e5884))return p;
        if(auto* p=field(0xa158bc,shared_damage.last_damage))return p;
        if(auto* p=field(0xa158c0,shared_damage.last_kind))return p;
        if(auto* p=field(0xa158a4,spawn_cells.ring_index))return p;
        if(auto* p=field(0x4f9148,trails.ring_4f9148))return p;
        if(auto* p=field(0x9e3098,trails.records_9e3098))return p;
        if(auto* p=field(0x4f8718,profiler.current_4f8718))return p;
        if(auto* p=field(0x4f871c,profiler.previous_4f871c))return p;
        if(at>=0x8e3f40&&std::uint64_t(at-0x8e3f40)<profiler.totals_8e3f40.size()*4)return reinterpret_cast<std::uint8_t*>(profiler.totals_8e3f40.data())+(at-0x8e3f40);
        if(auto* p=field(0xbd4814,timers))return p;
        if(auto* p=field(0xbd5248,c.state.world.water_level))return p;
        if(auto* p=field(0xbd52ac,terrain_color_pointer))return p;
        if(auto* p=field(0xbd5334,terrain_types))return p;
        if(at>=color_base&&std::uint64_t(at-color_base)<c.terrain.color.pixels.size())return const_cast<std::uint8_t*>(c.terrain.color.pixels.data())+(at-color_base);
        if(auto* p=field(0x8ee1d4,c.requested_8ee1d4))return p;
        return nullptr;
    }
    bool claims(U at,U width){if(width!=1&&width!=2&&width!=4)throw MissionTickError("Mission session memory width is invalid");if(std::uint64_t(at)+width>0x100000000ull)throw MissionTickError("Mission session memory wraps");U owned=0;for(U i=0;i<width;++i)owned+=source_byte(at+i)!=nullptr;if(owned&&owned!=width)throw MissionTickError("Mission session memory partially overlaps an owned cell");return owned==width;}
    void setup(){
auto& state=c.state;auto& initial=c.runtime;const auto& cache=c.cache;const auto& terrain=c.terrain;auto& animations=c.animations;auto& geometry=c.geometry;auto& muzzle=c.muzzle;auto& bridge=c.input;auto& sound=c.sound;auto& audio=c.audio;auto& listener=c.audio.listener_6e06a8;auto& listener_yaw=c.audio.yaw_6e06b4;auto& crt_cells=c.crt_thread;auto& clients=c.clients;auto& rules_word=c.rules_67f548;auto& types=c.runtime.live_types;
        matrices=initialized_mission_matrices(initial,cache,animations);
        bind_mission_linked_position(state);bind_mission_animations(state,animations);bind_mission_geometry(state,geometry,matrices);bind_mission_actor_collision(state,geometry,{},matrices);
        state.world.terrain=&terrain.geometry;state.world.water_level=terrain.water_height;

        state.world.trigger_held_47a290=[this](auto row){return c.input.query_active(row);};
    player_cells.camera_mode_6ba3ec=need(h.camera_mode_6ba3ec,"Camera mode provider is not bound")();

    crt.current_seed=[&]{return &crt_cells.seed;};
    MissionPlayerProviders player_bindings;player_bindings.crt_random=[&](auto&,auto&){return crt_random_next(crt);};player_bindings.projectile=[&](auto&,auto&,auto owner,auto,auto kind,auto& result){if(!request_projectile)throw MissionTickError("Canonical projectile owner is not installed");request_projectile(owner,kind,result);};
    player=std::make_unique<MissionPlayerLive>(state,player_cells,muzzle,player_bindings);player->install();


    camera.actor_6ba3f0=need(h.camera_actor_6ba3f0,"Camera actor provider is not bound")();

    MissionFrameServiceBindings services=c.scripts.frame_bindings();
    services.actor_word=[&](auto handle,auto offset){
        for(std::uint8_t group=0;group<4;++group)for(std::size_t slot=0;slot<state.tables.groups[group].size();++slot)
            if(actor_handle(group,slot)==handle){const auto image=snapshot_mission_actor(state,{group,slot});
                if(offset>image.size()-4)throw MissionTickError("Camera read exceeds actor");
                std::uint32_t value;std::memcpy(&value,image.data()+offset,4);return value;}
        throw MissionTickError("Camera actor has no current owner");};
    services.store_word=[&](auto at,auto value){if(at!=0x67dbdc)throw MissionTickError("Statistic output has no owner");state.statistic_67dbdc=value;};
    services.rdtsc=h.rdtsc;
    frame=std::make_unique<MissionFrameServices>(MissionFrameServiceCells{c.requested_8ee1d4,profiler,timers,statistic_bcfc24,camera,c.script_queue},services);



    message_bindings.dedup=&sequence_ring;message_bindings.received_bytes_bcfc24=&statistic_bcfc24;
    message_bindings.update_counter_bc5d20=&state.message_idle_bc5d20;
    message_bindings.read_global=[&](auto at){if(at==0x67f520)return state.globals.multiplayer_67f520;if(at==0x67f528)return state.globals.mission_67f528;if(at==0x67f548)return rules_word;throw MissionTickError("Missing message global owner");};
    message_bindings.write_global=[&](auto at,auto value){if(at==0xc23b78){received_status_c23b78=std::uint8_t(value);return;}if(at!=0x67f548)throw MissionTickError("Message global store has no owner");rules_word=value;};
    messages.set_live_bindings(message_bindings);


    listener_binding.write=[&](auto at,auto width,auto value){
        if(width!=4)throw MissionTickError("Wrong listener store width");
        if(at==0x6e06a8)std::memcpy(&listener.x,&value,4);
        else if(at==0x6e06ac)std::memcpy(&listener.y,&value,4);
        else if(at==0x6e06b0)std::memcpy(&listener.z,&value,4);
        else if(at==0x6e06b4)listener_yaw=value;
        else if(at==0x6e4708||at==0x6e4704||at==0x6e4700||at==0x6e06b8)listener_extra[at]=value;
        else throw MissionTickError("Missing listener store owner");};
    area.listener=[&](auto&,const MissionListenerPose& pose){mission_audio_listener(std::uint32_t(pose.position.x),std::uint32_t(pose.position.y),std::uint32_t(pose.position.z),pose.yaw,pose.pitch,listener_binding);};
    area.globals=[&](const auto&){MissionAreaLiveGlobals value;value.rules_67f549=std::uint8_t(rules_word>>8);return value;};

    gate.read=[&](auto at,auto width){if(width!=4)throw MissionTickError("Wrong control width");
        if(at==0x67f520)return state.globals.multiplayer_67f520;
        if(at==0x67f528)return state.globals.mission_67f528;
        throw MissionTickError("Missing remote delay owner");};



    state.world.external_stage=[&](auto&,const MissionStageCall& call)->std::optional<std::uint32_t>{

        if(call.address==0x42ab70)camera.actor_6ba3f0=need(h.camera_actor_6ba3f0,"Camera actor provider is not bound")();
        if(auto result=frame->stage(call))return result;
        if(call.address==0x45e0f0){mission_animation_effect(animation_effect,{call.args[0],call.args[1],call.args[2]});return 0;}
        if(call.address==0x490880)return need(h.pump_490880,"Mission host pump is not bound")()?0:1;
        if(call.address==0x479750){bridge.begin_frame(call.args[0]);return 0;}
        if(call.address==0x43f0b0){const auto count=need(h.elapsed_audio_frames,"Audio elapsed provider is not bound")();if(count)sound.advance_frames(count);sound.frame();return 0;}
        if(call.address==0x461050)return messages.check_receive()?1:0;
        if(call.address==0x46d9f0){messages.dispatch_received();return 0;}
        if(call.address==0x46f4b0){messages.flush_queue();return 0;}
        if(call.address==0x4718d0)return std::uint32_t(mission_control_gate(gate));
        if(call.address==0x443c50){if(!logic_run)throw MissionTickError("Logic frame is not bound");logic_run();return 0;}
        if(auto result=player->stage(call))return result;
        if(auto result=mission_area_live_stage(state,geometry,call,area))return result;
        return std::nullopt;
    };
    event_stage.load(c.mission);event_host.fired_reference=[&](auto index){return event_fired_reference(event_stage,index,h.event_references,h.event_diagnostic);};
    event_host.person_edit=[&](auto index){auto& action=event_stage.action(index);auto binding=mission_event_person_bindings(state,action,0x27000000,[&](auto address){return sound.asset_bindings().read(address,4);},[]()->std::uint16_t{throw MissionTickError("Current source x87 control has no owner");});mission_event_person_edits(binding,0x27000000);};
    order_binding=mission_event_actor_order_bindings(state);event_host.orders=[&](auto side,auto list){mission_event_actor_orders(order_binding,std::int32_t(side),list);};





    object.definition=[&](auto&,auto index,auto offset){if(index>=initial.definitions.size()||offset>sizeof(RuntimeItemDefinition)-4)throw MissionTickError("Object definition has no owner");std::uint32_t value;std::memcpy(&value,reinterpret_cast<const std::uint8_t*>(&initial.definitions[index])+offset,4);return value;};
    object.type_slot=[&](auto&,auto slot,auto offset){if(slot>=kAiTypeSlotCount||offset>kAiTypeSlotBytes-4)throw MissionTickError("Object type field has no owner");std::uint32_t value;std::memcpy(&value,types.bytes.data()+slot*kAiTypeSlotBytes+offset,4);return value;};
    object.global=[&](auto&,auto at){if(at==0xa158c0)return shared_damage.last_kind;if(at==0xa158bc)return shared_damage.last_damage;if(at>=0xbd0068&&at<0xbd0068+kSineTableQ22.size()*4&&(at-0xbd0068)%4==0)return std::uint32_t(kSineTableQ22[(at-0xbd0068)/4]);throw MissionTickError("Object global has no owner "+std::to_string(at));};
    object.call=[&](auto& current,const auto& ref,const ObjectBehaviorAction& action){std::array<std::uint32_t,6> out{};
        if(action.site==0x477820){const auto index=action.args[0]>>21;out[1]=std::uint32_t(kSineTableQ22[index]);out[2]=std::uint32_t(kSineTableQ22[index+512]);return out;}
        if(action.site==0x47df40){if(!current.world.terrain)throw MissionTickError("Object terrain has no owner");std::int32_t x,y;std::memcpy(&x,&action.args[0],4);std::memcpy(&y,&action.args[1],4);out[0]=std::uint32_t(ground_height_query(*current.world.terrain,x,y));return out;}
        if(action.site==0x43e7d0){Fixed3 position;std::memcpy(&position.x,&action.args[1],4);std::memcpy(&position.y,&action.args[2],4);std::memcpy(&position.z,&action.args[3],4);sound.positional(action.args[0],position);return out;}
        if(action.site==0x43ef80){sound.attach(mission_actor_source_address(ref.group,ref.index),action.args[1],action.args[2],action.args[3]);return out;}
        if(action.site==0x44f540){const auto answer=mission_object_target(current,ref,target_binding);out[0]=answer.source_actor;out[1]=answer.score;return out;}
        if(action.site==0x42edd0){mission_object_region_mark(event_stage,action.args[0],action.args[1]);return out;}
        if(action.site==0x477770){out[0]=query_angle_477770(action.args[0],action.args[1]);return out;}
        throw MissionTickError("Object body not bound "+std::to_string(action.site));};
    bind_mission_object_veh0(state,object);
    bind_mission_object_veh0_move(state,object);
    bind_mission_object_slgt_move(state,object);
    bind_mission_object_slgt_ai(state,object);
    bind_mission_object_cactus(state,object);
    bind_mission_object_sound(state,0x45c3a0,object);bind_mission_object_sound(state,0x45c340,object);
    bind_mission_object_wood_small(state,object);
    bind_mission_object_generic(state,object);
    bind_mission_object_wood_large(state,object);

    // Placement rows use the source-initialized shared client pool.
    client_store=[&](std::size_t slot,std::uint32_t at,unsigned width,std::uint32_t value){clients.write(MissionClientLive::source_address(slot,at),width,value);};
    client_read=[&](std::size_t slot,std::uint32_t at,unsigned width){return clients.read(MissionClientLive::source_address(slot,at),width);};
    cell=[&](std::uint32_t at){
        if(at==0x67f520)return state.globals.multiplayer_67f520;if(at==0x67f528)return state.globals.mission_67f528;
        if(at==0x67f53c)return state.game_type_67f53c;if(at==0x67dbe8)return state.mission_timer_67dbe8;
        if(at==0x8cfa1c)return std::uint32_t(state.tables.groups[0].size());if(at==0x8cfa18)return std::uint32_t(state.tables.groups[3].size());
        const auto found=cells.find(at);if(found==cells.end())throw MissionTickError("Logic cell has no owner "+std::to_string(at));return found->second;};
    cell_store=[&](std::uint32_t at,std::uint32_t n){const auto found=cells.find(at);if(found==cells.end())throw MissionTickError("Logic store has no owner "+std::to_string(at));found->second=n;};
    shared_read=[&](std::uint32_t at,unsigned width){
        if(at>=0x6e9a90&&std::uint64_t(at)<0x6e9a90+17ull*mission_client_record_size){const auto key=at-0x6e9a90;return client_read(key/mission_client_record_size,key%mission_client_record_size,width);}
        for(std::uint8_t group=0;group<4;++group){const auto base=mission_actor_source_address(group,0);
            if(at>=base && std::uint64_t(at-base)<state.tables.groups[group].size()*0x114ull){const auto key=at-base;const auto image=snapshot_mission_actor(state,{group,key/0x114});std::uint32_t value=0;for(unsigned i=0;i<width;++i)value|=std::uint32_t(image.at(key%0x114+i))<<(8*i);return value;}}
        if(width!=4)throw MissionTickError("Logic scalar width is not four");return cell(at);};
    shared_store=[&](std::uint32_t at,unsigned width,std::uint32_t n){if(width!=4)throw MissionTickError("Logic store width is not four");cell_store(at,n);};
    MissionClientOutboxBindings outbox_bindings;
    outbox_bindings.client=[&](auto slot){return clients.client(slot);};outbox_bindings.cell=cell;
    outbox=std::make_unique<MissionClientOutbox>(messages,server,outbox_bindings);

    MissionActorLifecycleBindings lifecycle_bindings;lifecycle_bindings.sincos_4776c0=[](auto angle,auto& sine,auto& cosine){sincos_q22(angle,sine,cosine);};
    lifecycle_bindings.terrain_47df40=[&](auto x,auto y){if(!state.world.terrain)throw MissionTickError("Lifecycle terrain has no owner");std::int32_t sx,sy;std::memcpy(&sx,&x,4);std::memcpy(&sy,&y,4);return std::uint32_t(ground_height_query(*state.world.terrain,sx,sy));};
    lifecycle=std::make_unique<MissionActorLifecycle>(state,animations,audio.registrations_6e0700,lifecycle_bindings);
    flag_bindings=mission_event_actor_flag_bindings(*lifecycle,*outbox,[&](const auto& ref,auto callback,auto message){if(callback==0x453930)mission_object_wood_large(state,ref,object,message);else mission_object_update(state,ref,callback,message,object);});
    event_host.flags=[&](auto side){mission_event_actor_flags(state,side,shared_damage.last_damage,flag_bindings);};
    event_host.remove=[&](auto side){mission_event_actor_remove(state,side,flag_bindings);};
    peer={cell,cell_store,{}};peer_timers.read=cell;peer_timers.write=cell_store;
    budget={shared_read,shared_store};round.store=shared_store;
     // Defined cold PE cell; full client startup remains separate.

    MissionRoundCheckBindings round_host;round_host.read=[&](auto at,auto width){if(at==0x6e5884&&width==1)return std::uint32_t(status_6e5884);return shared_read(at,width);};
    round_host.write=[&](auto at,auto width,auto value){if(at>=0x6e9a90&&std::uint64_t(at-0x6e9a90)+width<=17ull*mission_client_record_size){const auto delta=at-0x6e9a90;client_store(delta/mission_client_record_size,delta%mission_client_record_size,width,value);return;}shared_store(at,width,value);};
    round_host.enqueue_441750=[&](auto slot,auto type,auto* payload,auto length){outbox->enqueue(slot,std::int32_t(type),payload,length);};
    round_host.call=[&](auto site,const auto& args)->std::uint32_t{if(site==0x477620)return query_isqrt_477620(args.at(0));if(site==0x4423f0){std::uint8_t payload=std::uint8_t(args.at(1));mission_round_broadcast(round_checks,args.at(0),&payload,args.at(2));return 0;}throw MissionTickError("Periodic nested body is not bound "+std::to_string(site));};
    round_checks=mission_round_check_bindings(state,round_host);

    actor_message.broadcast=[&](auto type,auto& payload){mission_round_broadcast(round_checks,type,payload.data(),payload.size());};
    actor_message.read=[&](auto at,auto width){if(width!=4)throw MissionTickError("Actor phrase width is invalid");for(std::uint8_t group=0;group<4;++group){const auto base=mission_actor_source_address(group,0);if(at<base)continue;const auto delta=at-base,slot=delta/0x114,offset=delta%0x114;if(slot>=state.tables.groups[group].size()||offset+width>0x114)continue;const auto image=snapshot_mission_actor(state,{group,slot});std::uint32_t value;std::memcpy(&value,image.data()+offset,4);return offset==4?(value?definition_base+value*0x13cu:0u):value;}if(mission_extra_actor_record_base(at,width))return c.extra_memory.read(at,width);if(at>=definition_base&&std::uint64_t(at-definition_base)+width<=initial.definitions.size()*sizeof(RuntimeItemDefinition))return sound.asset_bindings().read(at,width);return sound.asset_bindings().read(at,width);};
    actor_message.positional=[&](auto id,Fixed3 position){sound.positional(id,position);};
    message_bindings.call=[&](const auto& call){if(call.address==0x43f5a0&&call.array==GameRecordArray::players_7cca80&&call.index>=0&&call.count==1){mission_actor_message_receive(actor_message,mission_actor_source_address(3,std::uint32_t(call.index)),call.args[0]);return;}throw MissionTickError("Message live call has no owner "+std::to_string(call.address));};messages.set_live_bindings(message_bindings);
    logic.cell=cell;logic.store=cell_store;logic.client=client_read;logic.client_store=client_store;
    logic.actor=[&](auto group,auto slot,auto at,auto width){const auto image=snapshot_mission_actor(state,{std::uint8_t(group),slot});std::uint32_t n=0;for(unsigned i=0;i<width;++i)n|=std::uint32_t(image.at(at+i))<<(8*i);return n;};
    logic.actor_byte_store=[&](auto group,auto slot,auto at,auto n){auto image=snapshot_mission_actor(state,{std::uint8_t(group),slot});image.at(at)=n;apply_mission_actor_image(state,{std::uint8_t(group),slot},image);};
    logic.client_actor_flags=[&](auto slot)->std::optional<std::uint8_t>{const auto raw=client_read(slot,8,4);if(!raw)return std::nullopt;for(std::size_t index=0;index<state.tables.groups[3].size();++index)if(mission_actor_source_address(3,index)==raw)return snapshot_mission_actor(state,{3,index})[0x6c];throw MissionTickError("Client actor has no current physical owner");};
    logic.send=[&](auto id,const auto& packet){outbox->broadcast(std::int32_t(id),packet.data(),std::uint32_t(packet.size()));};
    logic.call=[&](auto site,const auto&)->std::uint32_t{
        if(site==0x444630){mission_round_check_end(round_checks);return 0;}
        if(site==0x448590){mission_round_check_clients(round_checks);return 0;}
        if(site==0x447930){mission_round_publish_status(round_checks);return 0;}
        if(site==0x44a200){mission_type_counts(mission_type_count_bindings(state,types));return 0;}
        if(site==0x42eaf0){event_stage.tick();return 0;}
        if(site==0x45fd80){if(!reset_zones)throw MissionTickError("Zone reset has no owner");reset_zones();return 0;}
        if(site==0x460e40)return mission_peer_frame_receive(messages.peer_frame_buffer(),peer);
        if(site==0x4477f0){mission_peer_timers(peer_timers);return 0;}
        if(site==0x444e30){mission_peer_budget_update(budget);return 0;}
        if(site==0x444f60){mission_round_clear_frame_flag(round);return 0;}
        if(site==0x4421d0){outbox->tick();return 0;}
        throw MissionTickError("Logic body is not bound "+std::to_string(site));};
    logic_run=[&]{mission_logic_frame(logic);};
    zone_binding=zone_owner.bindings(state.globals.mission_67f528);
    if(state.projectiles.size()!=512)throw MissionTickError("Mission session needs the initialized512slot projectile pool");spawn_cells.rules_8cf982=c.mission.header[0x7a];

    shots.read=[&](auto at,auto width){if(at>=0x6e4760&&std::uint64_t(at-0x6e4760)+width<=shot_ledger.size()*4){if(width!=4||(at&3))throw MissionTickError("Shot ledger access is invalid");return shot_ledger.at((at-0x6e4760)/4);}return shared_read(at,width);};
    shots.write=[&](auto at,auto width,auto value){if(width!=4||(at&3))throw MissionTickError("Shot ledger store is invalid");if(at>=0x6e4760&&std::uint64_t(at-0x6e4760)+width<=shot_ledger.size()*4){shot_ledger.at((at-0x6e4760)/4)=value;return;}if(at>=0x6e9a90&&std::uint64_t(at-0x6e9a90)+width<=17ull*mission_client_record_size){const auto key=at-0x6e9a90;client_store(key/mission_client_record_size,key%mission_client_record_size,width,value);return;}for(std::uint8_t group=0;group<4;++group){const auto base=mission_actor_source_address(group,0);if(at>=base&&std::uint64_t(at-base)+width<=state.tables.groups[group].size()*0x114ull){const auto key=at-base;auto image=snapshot_mission_actor(state,{group,key/0x114});for(unsigned i=0;i<width;++i)image.at(key%0x114+i)=std::uint8_t(value>>(8*i));apply_mission_actor_image(state,{group,key/0x114},image);return;}}shared_store(at,width,value);};
    shots.call=[&](auto site,const auto& args){if(site!=0x47df40||args.size()!=2)throw MissionTickError("Shot ledger body has no owner");if(!state.world.terrain)throw MissionTickError("Shot ledger terrain is not loaded");std::int32_t x,y;std::memcpy(&x,&args[0],4);std::memcpy(&y,&args[1],4);return std::uint32_t(ground_height_query(*state.world.terrain,x,y));};
    MissionProjectileSpawnBindings spawn_bindings;spawn_bindings.ground=[&](auto&,auto x,auto y){return ground_height_query(*state.world.terrain,x,y);};
    spawn_bindings.host_message=[&](auto&,auto& bytes){mission_peer_record_shot(shots,bytes.data(),bytes.size());};
    spawn_bindings.queue=[&](auto&,auto id,const auto& bytes){messages.queue().enqueue(messages.table(),std::uint8_t(id),bytes.data(),std::uint16_t(bytes.size()));};
    spawn_bindings.attached_sound=[&](auto&,auto slot,auto id,auto scale,auto ceiling){sound.attach(0x9f3098+std::uint32_t(slot)*0x114,id,scale,ceiling);};
    spawn_bindings.positional_sound=[&](auto&,auto id,auto position){sound.positional(id,position);};spawn_bindings.loudness=[&](auto&,auto id){return std::int32_t(sound.loudness(id));};
    spawn_bindings.alert=[&](auto&,auto position,auto radius,auto type,auto flags){mission_behavior_zone_add(zone_binding,std::uint32_t(position.x),std::uint32_t(position.y),std::uint32_t(position.z),std::uint32_t(radius),std::uint32_t(type),flags);};
    spawn=std::make_unique<MissionProjectileSpawn>(state,spawn_cells,initial.definitions,spawn_bindings);request_projectile=[&](auto owner,auto kind,auto& result){spawn->request(owner,kind,result);};
    state.world.weapon_effect=[&](auto&,const WeaponEvent& event){if(event.kind==WeaponEventKind::sound){const auto& a=state.tables.groups[3][*state.config.local_actor];Fixed3 position;std::memcpy(&position.x,&a.position[0],4);std::memcpy(&position.y,&a.position[1],4);std::memcpy(&position.z,&a.position[2],4);sound.positional(std::uint32_t(event.value),position);return true;}if(event.kind==WeaponEventKind::sound2){sound.global(std::uint32_t(event.value));return true;}if(event.kind==WeaponEventKind::queue){messages.queue().enqueue(messages.table(),std::uint8_t(event.value),nullptr,0);return true;}throw MissionTickError("Weapon effect has no live owner "+std::to_string(unsigned(event.kind)));};
    mission_behavior_zones_reset(zone_binding); //45FD80 startup clear.
    reset_zones=[&]{mission_behavior_zones_reset(zone_binding);};
    std::vector<MissionProjectileMaterialBinding> material_bindings;std::uint32_t next_material=1;for(const auto& entry:initial.models){const auto& model=cache.model(entry.name);if(model.lods.empty())throw MissionTickError("Loaded projectile model has no first LOD");MissionProjectileMaterialBinding binding;binding.model=entry.handle;for(std::size_t i=0;i<model.lods.front().materials.size();++i)binding.identities.push_back(next_material++);material_bindings.push_back(std::move(binding));}
    trail_bindings=mission_projectile_trail_bindings(trails);mission_projectile_trail_clear(trails);
    terrain_types=mission_terrain_types(terrain.color,terrain.config);

    mark.read=[&](auto at,auto width){if(at==0xbd52ac&&width==4)return terrain_color_pointer;if(at>=color_base&&std::uint64_t(at-color_base)+width<=terrain.color.pixels.size()){if(width!=1)throw MissionTickError("Terrain color grid requires a byte read");return std::uint32_t(terrain.color.pixels[at-color_base]);}if(at>=0xbd5334&&at<0xbd5434&&width==1)return std::uint32_t(terrain_types[at-0xbd5334]);if(at==0xc23810&&width==4)return definition_base;for(const auto binding:std::array<std::pair<std::uint32_t,std::uint32_t>,6>{{{0x8e44c4,7001},{0x8e44c8,7002},{0x8e44cc,7003},{0x8e44d0,7004},{0x8e44d4,7005},{0x8e44e4,7009}}})if(at==binding.first&&width==4)return std::uint32_t(runtime_item_index(initial.definitions,binding.second));return sound.asset_bindings().read(at,width);};
    mark.call=[&](auto site,const auto& args,std::uint8_t* image,std::size_t length){if(site==0x43e7d0){Fixed3 position;std::memcpy(&position.x,&args[1],4);std::memcpy(&position.y,&args[2],4);std::memcpy(&position.z,&args[3],4);sound.positional(args[0],position);return;}if(site==0x45fcf0){mission_behavior_zone_add(zone_binding,args[0],args[1],args[2],args[3],args[4],args[5]);return;}if(site==0x45d970){if(!image||length!=0x114||insert_image)throw MissionTickError("Extra insertion image scope is invalid");struct Scope{Impl& owner;~Scope(){owner.insert_image=nullptr;owner.insert_bytes=0;}} scope{*this};insert_image=image;insert_bytes=length;mission_actor_insert(insertion,insert_source,args[1]);return;}throw MissionTickError("Terrain mark body has no owner");};
    animation_effect.read=[&](auto at,auto width)->std::uint64_t{if(at==0xbd5248&&width==4)return std::uint32_t(state.world.water_level);if(at==0x4ccb2c&&width==8){const auto memory=sound.asset_bindings();const auto low=memory.read(at,4);const auto high=memory.read(at+4,4);return std::uint64_t(low)|(std::uint64_t(high)<<32);}if((at>=0xbd5334&&at<0xbd5434)||at==0xbd52ac||(at>=color_base&&std::uint64_t(at-color_base)+width<=terrain.color.pixels.size()))return mark.read(at,width);return actor_message.read(at,width);};
    animation_effect.call=[&](auto site,const auto& args){if(site!=0x43e7d0)throw MissionTickError("Animation effect required body is not bound "+std::to_string(site));Fixed3 position;std::memcpy(&position.x,&args[1],4);std::memcpy(&position.y,&args[2],4);std::memcpy(&position.z,&args[3],4);sound.positional(args[0],position);};
    MissionProjectileLiveBindings projectile_live;projectile_live.globals=[&](const auto& current){MissionProjectileLiveGlobals globals;globals.rules_67f549=std::uint8_t(rules_word>>8);globals.alternate_move_filter=current.globals.multiplayer_67f520?(rules_word&1)!=0:(globals.rules_67f549&0x10)!=0;return globals;};
    projectile_live.update=[&](auto& current,const auto& ref,auto callback,auto message){mission_object_update(current,ref,callback,message,object);};
    projectile_live.move=[&](auto&,auto& current,const auto& event){const std::array<std::uint32_t,7> args{std::uint32_t(event.from.x),std::uint32_t(event.from.y),std::uint32_t(event.from.z),std::uint32_t(event.position.x),std::uint32_t(event.position.y),std::uint32_t(event.position.z),current.color};mission_projectile_trail_record(trail_bindings,args);};
    projectile_live.world_edge=[&](auto&,auto&,const auto& event){sound.positional(0x41,event.position);};
    projectile_live.impact=[&](auto&,auto&,const auto& effect){if(effect.site==ProjectileImpactSite::terrain_mark){mission_terrain_mark(state,effect.position,mark);return;}throw MissionTickError("Reached projectile impact body is not bound "+std::to_string(std::uint32_t(effect.site)));};
    state.world.projectile_query=mission_projectile_live_query(state,geometry,cache,initial,animations,std::move(material_bindings),shared_damage,projectile_live,matrices);
    state.world.projectile_query.current_world_center=[&]{Fixed3 center;const auto x=listener_extra.at(0x6e4708),y=listener_extra.at(0x6e4704),z=listener_extra.at(0x6e4700);std::memcpy(&center.x,&x,4);std::memcpy(&center.y,&y,4);std::memcpy(&center.z,&z,4);return center;};
    height_bounds=build_terrain_height_bounds(terrain.geometry);
    //449620 clears all256 physical rows. The counted prefix remains in state.

    target_binding.physical_slot=[&](auto slot){if(slot<state.tables.groups[3].size()||slot>=group3_tail.size())throw MissionTickError("Physical tail slot is outside its owner");return group3_tail[slot];};
    target_binding.frame_weight_10=[&](const auto& target){const auto image=snapshot_mission_actor(state,target);std::uint32_t action;std::memcpy(&action,image.data()+0x80,4);const auto& runtime=state.runtime_for(target);if(!runtime.driver)throw MissionTickError("Target frame has no driver");const auto* frame=runtime.driver->action(action);if(!frame)throw MissionTickError("Target action exceeds its frame table");return frame->weight_10;};
    sight_bindings=mission_object_sight_bindings(state,geometry,height_bounds.front(),matrices);
    target_binding.sight_line=[&](const auto& shooter,const auto& target){return mission_object_sight(state,shooter,target,sight_bindings);};
    event_host.object_read=[&](auto address,auto index){const auto offset=address-0x793680;if(index>=kAiTypeSlotCount||offset>kAiTypeSlotBytes-4)throw MissionTickError("Event type read exceeds table");std::uint32_t result;std::memcpy(&result,types.bytes.data()+index*kAiTypeSlotBytes+offset,4);return result;};
    event_host.rect=[&](auto index){const auto& t=event_stage.trigger(index);return mission_event_rect(state,{std::int32_t(t[3]),std::int32_t(t[4]),std::int32_t(t[5]),std::int32_t(t[6]),std::int32_t(t[7])});};
    event_host.linked=[&](auto first,auto second){return mission_event_linked_sides(state,std::int32_t(first),std::int32_t(second));};
    event_host.script=[&](auto id){c.scripts.start().event(id);};
    event_host.multiplayer_zero=[&]{return state.globals.multiplayer_67f520==0;};
    event_host.rule_bit1=[&]{return (std::uint8_t(rules_word>>8)&2)!=0;};
    for(const auto& actor:state.tables.groups[2]){
        BehaviorMarker marker;marker.object_id=actor.object_id;for(unsigned axis=0;axis<3;++axis)marker.position[axis]=actor.position[axis];marker.yaw=actor.angle_yaw;marker.radius=actor.field_7c;state.world.behavior_world.markers.push_back(marker);
    }
    for(const auto& row:c.mission.marker_lists){BehaviorMarkerList list;list.flags=row.flags;list.count=row.count;list.indices=row.marker_indices;state.world.behavior_world.lists.push_back(list);}
    state.world.behavior_world.resolve_target=[&](auto raw)->std::optional<BehaviorTargetView>{
        for(std::size_t slot=0;slot<state.tables.groups[3].size();++slot)if(mission_actor_source_address(3,slot)==raw){const auto image=snapshot_mission_actor(state,{3,slot});const auto get=[&](auto at){std::uint32_t n;std::memcpy(&n,image.data()+at,4);return n;};BehaviorTargetView result;result.field_70=get(0x70);result.field_6c=image[0x6c];result.field_f8=get(0xf8);for(unsigned axis=0;axis<3;++axis)result.position[axis]=get(0x24+axis*4);return result;}return std::nullopt;};
    state.world.behavior_world.resolve_marker=[&](auto raw)->std::optional<std::size_t>{for(std::size_t slot=0;slot<state.tables.groups[2].size();++slot)if(mission_actor_source_address(2,slot)==raw)return slot;return std::nullopt;};
    bind_mission_actor_queries(state,[&](const auto& ref){auto data=mission_actor_query_data(state,ref,geometry,height_bounds.front(),zone_owner.active_records(),(rules_word&0x80)!=0,matrices);data.sight_line=[&,ref](std::size_t slot){return mission_object_sight(state,ref,{3,slot},sight_bindings);};return data;},[&](const auto&,const auto& action,auto&){if(action.site==behavior_site::kRegionBit3){event_stage.set_flag_42ee30(action.args[0],action.args[1],action.args[2]);return true;}if(action.site==behavior_site::kMessage){mission_actor_message_send_index(actor_message,action.args[0],action.args[1]);return true;}if(action.site!=behavior_site::kTypeSlotMark)return false;auto binding=mission_type_count_bindings(state,types);mission_type_mark(binding,action.args[0]);return true;});

        input.next_command=[this](auto tick)->std::optional<MissionCommandEvent>{auto record=c.input.pop_due(tick);if(!record)return {};return MissionCommandEvent{record->id,std::int32_t(record->value)};};
        input.dispatch_command=[this](auto id,auto value){return dispatch_game_command(id,value,c.state.globals,c.state.dispatch_actor,c.input.command_table());};
    }
};
MissionSession::MissionSession(MissionSessionContext c,MissionSessionHost h):impl_(std::make_unique<Impl>(c,std::move(h))){}
MissionSession::~MissionSession(){uninstall();}
void MissionSession::install(){auto& p=*impl_;if(p.active)throw MissionTickError("Mission session is already installed");p.prior_world=p.c.state.world;p.prior_updates=p.c.state.update_callbacks;p.prior_secondary=p.c.state.secondary_callbacks;p.active=true;const auto remember=[&]{for(std::size_t i=0;i<p.update_ids.size();++i)p.installed_updates[i]=!p.prior_updates.count(p.update_ids[i])&&p.c.state.update_callbacks.count(p.update_ids[i]);for(std::size_t i=0;i<p.secondary_ids.size();++i)p.installed_secondary[i]=!p.prior_secondary.count(p.secondary_ids[i])&&p.c.state.secondary_callbacks.count(p.secondary_ids[i]);};try{p.setup();remember();}catch(...){remember();uninstall();throw;}}
void MissionSession::uninstall() noexcept {auto& p=*impl_;if(!p.active)return;if(p.player)p.player->uninstall();using std::swap;swap(p.c.state.world,p.prior_world);for(std::size_t i=0;i<p.update_ids.size();++i)if(p.installed_updates[i])p.c.state.update_callbacks.erase(p.update_ids[i]);for(std::size_t i=0;i<p.secondary_ids.size();++i)if(p.installed_secondary[i])p.c.state.secondary_callbacks.erase(p.secondary_ids[i]);p.installed_updates.fill(false);p.installed_secondary.fill(false);p.active=false;}
bool MissionSession::installed() const noexcept{return impl_->active;}
MissionTickReport MissionSession::step(){if(!installed())throw MissionTickError("Mission session is not installed");return mission_tick(impl_->c.state,impl_->input);}
void MissionSession::script_message(U id,const std::uint8_t* bytes,std::size_t length){if(!installed()||!impl_->outbox)throw MissionTickError("Mission script outbox is not installed");if(length>std::numeric_limits<U>::max())throw MissionTickError("Mission script message is too large");impl_->outbox->broadcast(std::int32_t(id),bytes,U(length));}
U MissionSession::camera_yaw() const{return impl_->camera.yaw_6ba3e8;}

std::optional<CommandBinding> mission_session_input_query_row(const MissionSoundRuntime& sound,std::size_t row){if(row!=0x8c)throw MissionTickError("Loaded input query row is not bound");const U address=0x4fb2fc+U(row)*0x2c;const auto& memory=sound.asset_bindings();return CommandBinding{memory.read(address,4),{},std::uint16_t(memory.read(address+0x24,2)),std::uint16_t(memory.read(address+0x26,2)),std::uint16_t(memory.read(address+0x28,2)),std::uint16_t(memory.read(address+0x2a,2))};}
bool MissionSession::claims(U at,U width) const{return impl_->claims(at,width);}
U MissionSession::read(U at,U width) const{if(!claims(at,width))throw MissionTickError("Mission session read has no cell owner");U result=0;for(U i=0;i<width;++i)result|=U(*impl_->source_byte(at+i))<<(i*8);return result;}
void MissionSession::write(U at,U width,U value){if(!claims(at,width))throw MissionTickError("Mission session write has no cell owner");if(std::uint64_t(at)<std::uint64_t(Impl::color_base)+impl_->c.terrain.color.pixels.size()&&std::uint64_t(at)+width>Impl::color_base)throw MissionTickError("Mission terrain color bytes are read only");for(U i=0;i<width;++i)*impl_->source_byte(at+i)=std::uint8_t(value>>(i*8));}
}
