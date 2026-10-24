#include "dfre/mission_peer_leave.hpp"
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
#include "dfre/mission_scoreboard.hpp"
#include "dfre/mission_periodic_services.hpp"
#include "dfre/mission_ai_shot.hpp"
#include "dfre/mission_result_live.hpp"
#include "dfre/mission_hud_message.hpp"
#include "dfre/mission_object_helo.hpp"
#include "dfre/mission_object_mine_move.hpp"
#include "dfre/mission_object_barrel.hpp"
#include "dfre/mission_object_mine_update.hpp"
#include "dfre/mission_object_helo_update.hpp"
#include "dfre/mission_object_helo_condition.hpp"
#include "dfre/mission_object_helo_home.hpp"
#include "dfre/mission_projectile_owner.hpp"
#include "dfre/mission_object_tree_spawn.hpp"
#include "dfre/mission_object_seagull.hpp"
#include "dfre/mission_animation_table_live.hpp"
#include "dfre/world_hits.hpp"
#include "dfre/mission_logic.hpp"
#include "dfre/mission_client_outbox.hpp"
#include "dfre/mission_peer_frame.hpp"
#include "dfre/mission_peer_parser.hpp"
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
    std::function<void(std::uint32_t)> flags,remove,script,result,text;
    void action_404dd0(std::uint32_t id) override{need(text,"Mission triggered text is not bound")(id);}
    void action_444ca0(std::uint32_t mode) override{need(result,"Mission result host is not bound")(mode);}
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
    //490EEF ->4A123C ->4A1217 resets the source x87 control with FNINIT.
    std::uint16_t fp_control_word=0x037f;
    MissionTickWorld prior_world; bool active=false;
    std::map<U,MissionTickState::ActorCallback> prior_updates,prior_secondary;
    inline static constexpr std::array<U,13> update_ids{0x429bc0,0x4519a0,0x4534a0,0x45c3a0,0x45c340,0x453810,0x4533f0,0x453930,0x45bf80,0x451d40,0x450e40,0x451e70,0x451fd0};
    inline static constexpr std::array<U,5> secondary_ids{0x429ea0,0x451cb0,0x451590,0x451dd0,0x452310};
    std::array<bool,update_ids.size()> installed_updates{};
    std::array<bool,secondary_ids.size()> installed_secondary{};
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
    MissionObjectHeloBindings helo;
    std::unique_ptr<MissionAiShot> ai_shot;
    std::function<U(std::size_t,U,unsigned)> client_read;
    std::function<void(std::size_t,U,unsigned,U)> client_store;
    std::function<U(U)> cell;std::function<void(U,U)> cell_store;
    std::function<U(U,unsigned)> shared_read;std::function<void(U,unsigned,U)> shared_store;
    MissionServerMessageBuffer server;std::unique_ptr<MissionClientOutbox> outbox;
    std::unique_ptr<MissionActorLifecycle> lifecycle;MissionEventActorFlagBindings flag_bindings;
    MissionPeerParserBindings parser;
    MissionPeerFrameBindings peer;MissionPeerTimerBindings peer_timers;
    MissionPeerBudgetBindings budget;MissionRoundServiceBindings round;
    MissionRoundCheckBindings round_checks;MissionActorMessageBindings actor_message;
    MissionLogicFrameBindings logic;
    MissionLogicResultBindings result_bindings;
    MissionLogicResultPacket result_caller_packet{};
    static constexpr U peer_leave_source=0x1b001500;
    MissionLogicResultPacket peer_leave_packet{};
    std::unique_ptr<MissionResultImage> owned_result_image;
    MissionResultImage& result_image;
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
    std::array<std::uint8_t,0x1360> hud_history{};
    U hud_counter_bd4810=0;
    std::array<std::uint8_t,32> hud_frame{};
    static constexpr U hud_frame_address=0x1b002000;
    bool hud_frame_active=false,hud_input_initialized=false;
    MissionHudMessageBindings hud;
    void triggered_text(U id){
        if(hud_frame_active)throw MissionTickError("Triggered text caller frame is already active");
        hud_frame_active=true;struct Scope{bool& active;~Scope(){active=false;}} scope{hud_frame_active};
        mission_hud_triggered_text(hud,id,hud_frame_address);
    }
    std::optional<U> hud_service(U site,const std::vector<U>& args){
        if(site!=0x479f00&&site!=0x479e90&&site!=0x404d80&&site!=0x404dd0&&!(site==0x493a80&&args.size()>1&&args[1]==0x4c82fc))return {};
        if(!active||!hud.read)throw MissionTickError("HUD service requires an installed session");
        if(site==0x479f00){const MissionHudMessageCall call{args.at(0),args.at(1)};mission_hud_message(hud,call);return 0;}
        if(site==0x479e90){mission_hud_message_clear(hud);return 0;}
        if(site==0x404d80)return mission_hud_triggered_lookup(hud,args.at(0),args.at(1));
        if(site==0x404dd0){triggered_text(args.at(0));return 0;}
        return mission_hud_message_format_id(hud,args.at(0),args.at(1),args.at(2));
    }

    static constexpr U scoreboard_base=0xbcf664,score_frame=0x1b000200,score_packet=0x1b000300;
    std::array<std::uint8_t,0x5b0> scoreboard{};
    bool receiving_scoreboard=false;
    std::array<std::uint8_t,64*0x68> squads{};
    static constexpr U periodic_source=0x1b001000;
    std::array<std::uint8_t,0x500> periodic_frame{};
    MissionPeriodicBindings periodic;
    MissionObjectBarrelBindings barrel;
    MissionObjectMineUpdateBindings mine_update;
    std::function<void(const AiActorRef&,U,U)> dispatch_object;
    static constexpr U mine_source=0x1b003100;
    std::array<std::uint8_t,8> mine_frame{};
    static constexpr U helo_source=0x1b004000;
    std::array<std::uint8_t,0x44> helo_frame{};
    static constexpr U helo_condition_source=0x1b004100;
    std::array<std::uint8_t,16> helo_condition_frame{};
    bool helo_condition_active=false;
    static constexpr U helo_altitude_source=0x1b004200;
    std::array<std::uint8_t,0x40> helo_altitude_frame{};
    bool helo_altitude_active=false;
    MissionObjectTreeBindings tree;
    std::unique_ptr<MissionAnimationTableLive> animation_tables;
    static constexpr U animation_table_base=0x20800000,tree_source=0x1b006000;
    std::array<std::uint8_t,0x114> tree_frame{};
    bool tree_active=false;
    MissionObjectSeagullBindings seagull;
    static constexpr U seagull_source=0x1b006200;
    std::array<std::uint8_t,28> seagull_frame{};
    bool seagull_active=false;
    void run_seagull(U actor){
        if(seagull_active)throw MissionTickError("Seagull caller frame is already active");
        seagull_active=true;struct Scope{bool& active;~Scope(){active=false;}} scope{seagull_active};
        mission_object_seagull_ai(seagull,actor,seagull_source);
    }
    void run_tree(U actor,U message){
        if(tree_active)throw MissionTickError("Tree caller frame is already active");
        tree_active=true;struct Scope{bool& active;~Scope(){active=false;}} scope{tree_active};
        mission_object_tree_spawn(tree,actor,tree_source,message);
    }
    static constexpr U barrel_source=0x1b003000;
    std::array<std::uint8_t,24> barrel_frame{};
    bool running_periodic=false;
    void run_periodic(U site){
        if(running_periodic)throw MissionTickError("Periodic frame is already active");
        running_periodic=true;struct Scope{bool& active;~Scope(){active=false;}} scope{running_periodic};
        if(site==0x4438c0)mission_periodic_scoreboard(periodic,periodic_source);
        else if(site==0x448c30)mission_periodic_poll_clients(periodic);
        else if(site==0x445c80)mission_periodic_clear_actor_flags(periodic);
        else if(site==0x446200)mission_periodic_advertise(periodic,periodic_source);
        else if(site==0x4482e0)mission_periodic_publish_flag(periodic,periodic_source);
        else throw MissionTickError("Unknown periodic source body");
    }

    void receive_scoreboard(const std::uint8_t* packet,std::size_t length){
        if(receiving_scoreboard||!packet||length!=0x8f)throw MissionTickError("Scoreboard receive scope is invalid");
        receiving_scoreboard=true;struct Scope{bool& active;~Scope(){active=false;}} scope{receiving_scoreboard};
        std::array<std::uint8_t,0x70> locals{};
        const auto store_local=[&](U at,U width,U value){for(U i=0;i<width;++i)locals.at(at-score_frame+i)=std::uint8_t(value>>(i*8));};
        store_local(score_frame+0x44,4,score_packet);
        MissionScoreboardBindings b;
        b.read=[&](U at,U width){
            const auto bytes=[&](const auto& image,std::size_t offset){U n=0;for(U i=0;i<width;++i)n|=U(image.at(offset+i))<<(i*8);return n;};
            if(at>=score_frame&&std::uint64_t(at-score_frame)+width<=locals.size())return bytes(locals,at-score_frame);
            if(at>=score_packet&&std::uint64_t(at-score_packet)+width<=length){U n=0;for(U i=0;i<width;++i)n|=U(packet[at-score_packet+i])<<(i*8);return n;}
            if(at>=score_packet&&at<score_packet+0x100)throw MissionTickError("Scoreboard payload read exceeds its current frame");
            if(at>=0x7cca80&&std::uint64_t(at)+width<=0x7dde80){
                const auto slot=(at-0x7cca80)/0x114,offset=(at-0x7cca80)%0x114;
                if(offset+width>0x114)throw MissionTickError("Scoreboard actor read crosses its source record");
                auto image=slot<c.state.tables.groups[3].size()?snapshot_mission_actor(c.state,{3,slot}):group3_tail.at(slot);
                if(slot<c.state.tables.groups[3].size()){
                    const auto person=c.state.tables.groups[3][slot].person_slot;
                    const U physical=person?0x794680+U(*person)*0xb4:0;
                    std::memcpy(image.data()+0x0c,&physical,4);
                }
                return bytes(image,offset);
            }
            if(at>=0x794680&&std::uint64_t(at)+width<=0x7cca80){
                const auto slot=(at-0x794680)/0xb4,offset=(at-0x794680)%0xb4;
                if(offset+width>0xb4||slot>=c.state.people.size())throw MissionTickError("Scoreboard person read has no current owner");
                return bytes(ai_person_image(c.state.people[slot]),offset);
            }
            return c.sound.asset_bindings().read(at,width);
        };
        b.write=[&](U at,U width,U value){
            if(at>=score_frame&&std::uint64_t(at-score_frame)+width<=locals.size()){store_local(at,width,value);return;}
            c.sound.asset_bindings().write(at,width,value);
        };
        b.call=[&](U site,const auto& args){return c.sound.asset_bindings().call(site,args);};
        mission_scoreboard_receive(b,score_frame);
    }

    explicit Impl(MissionSessionContext context,MissionSessionHost host,MissionResultImage* result=nullptr):c(context),h(std::move(host)),camera{c.camera_position_6ba3d0,{},0,0},owned_result_image(result?nullptr:std::make_unique<MissionResultImage>()),result_image(result?*result:*owned_result_image){
        const auto& memory=c.sound.asset_bindings();
        event_stage.flags_67dbec()=memory.read(0x67dbec,4);
        for(std::size_t i=0;i<hud_history.size();++i)hud_history[i]=std::uint8_t(memory.read(0xbd34b0+U(i),1));
        hud_counter_bd4810=memory.read(0xbd4810,4);cells.emplace(0x67db54,memory.read(0x67db54,4));
        for(std::size_t i=0;i<result_image.size();++i)result_image[i]=std::uint8_t(memory.read(0x67f714+U(i),1));
        for(U at=0x6e576c;at<=0x6e5774;at+=4)cells.emplace(at,memory.read(at,4));
        for(std::size_t i=0;i<squads.size();++i)squads[i]=std::uint8_t(memory.read(0x6e8090+U(i),1));
        for(U at=0x8e61b4;at<=0x8e61bc;at+=4)cells.emplace(at,memory.read(at,4));
        cells.emplace(0x67f524,memory.read(0x67f524,4));
        for(std::size_t i=0;i<scoreboard.size();++i)scoreboard[i]=std::uint8_t(memory.read(scoreboard_base+U(i),1));
        for(U at=0x6e4730;at<=0x6e475c;at+=4)cells.emplace(at,memory.read(at,4));
        for(U at=0x6e5570;at<0x6e558c;at+=4)cells.emplace(at,memory.read(at,4));
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
        if(auto* p=field(0x8ec1cc,messages.peer_frame_buffer().bytes_8ec1cc))return p;
        if(auto* p=field(0x8ee1cc,messages.peer_frame_buffer().count_8ee1cc))return p;
        if(auto* p=field(0x67f538,c.state.send_interval_67f538))return p;
        if(auto* p=field(0x67f714,result_image))return p;
        if(auto* p=field(peer_leave_source,peer_leave_packet))return p;
        if(auto* p=field(0x67dbc0,c.state.globals.menu_flag_67dbc0))return p;
        if(auto* p=field(0x503428,c.state.globals.toggle_503428))return p;
        if(auto* p=field(0x67dbec,event_stage.flags_67dbec()))return p;
        if(auto* p=field(0xbd34b0,hud_history))return p;
        if(auto* p=field(0xbd4810,hud_counter_bd4810))return p;
        if(hud_frame_active)if(auto* p=field(hud_frame_address,hud_frame))return p;
        if(auto* p=field(0x67dbc9,c.state.globals.flags_67dbc9))return p;
        if(auto* p=field(scoreboard_base,scoreboard))return p;
        if(auto* p=field(0x6e8090,squads))return p;
        if(auto* p=field(0x67f520,c.state.globals.multiplayer_67f520))return p;
        if(auto* p=field(0x67f528,c.state.globals.mission_67f528))return p;
        if(auto* p=field(0x67f52c,c.state.globals.mission2_67f52c))return p;
        if(auto* p=field(0x67f548,c.rules_67f548))return p;
        if(auto* p=field(0x67f53c,c.state.game_type_67f53c))return p;
        if(auto* p=field(0x67dbe8,c.state.mission_timer_67dbe8))return p;
        if(auto* p=field(0x6e558c,c.state.globals.toggle_6e558c))return p;
        if(auto* p=field(0x6e5590,c.state.globals.toggle_6e5590))return p;
        if(auto* p=field(0x6e5594,c.state.globals.toggle_6e5594))return p;
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

        hud.read=[this](U at,U width){if(claims(at,width)){U value=0;for(U i=0;i<width;++i)value|=U(*source_byte(at+i))<<(8*i);return value;}return c.sound.asset_bindings().read(at,width);};
        hud.write=[this](U at,U width,U value){if(std::uint64_t(at)<std::uint64_t(color_base)+c.terrain.color.pixels.size()&&std::uint64_t(at)+width>color_base)throw MissionTickError("Mission terrain color bytes are read only");if(claims(at,width)){for(U i=0;i<width;++i)*source_byte(at+i)=std::uint8_t(value>>(8*i));return;}c.sound.asset_bindings().write(at,width,value);};
        hud.call=[this](U site,const std::vector<U>& args)->U{
            if(site==0x49a620){for(U i=0;i<args.at(2);++i)hud.write(args.at(0)+i,1,args.at(1)&255u);return args.at(0);}
            if(site==0x404d80)return mission_hud_triggered_lookup(hud,args.at(0),args.at(1));
            if(site==0x493a80&&args.at(1)==0x4c82fc)return mission_hud_message_format_id(hud,args.at(0),args.at(1),args.at(2));
            if(site==0x479f00){const MissionHudMessageCall call{args.at(0),args.at(1)};mission_hud_message(hud,call);return 0;}
            return c.sound.asset_bindings().call(site,args);
        };
        event_host.text=[this](U id){triggered_text(id);};
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
    event_host.person_edit=[&](auto index){auto& action=event_stage.action(index);auto binding=mission_event_person_bindings(state,action,0x27000000,[&](auto address){return sound.asset_bindings().read(address,4);},[this]{return fp_control_word;});mission_event_person_edits(binding,0x27000000);};
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
        if(claims(at,4))return c.sound.asset_bindings().read(at,4);
        if(at==0x67f520)return state.globals.multiplayer_67f520;if(at==0x67f528)return state.globals.mission_67f528;
        if(at==0x67f53c)return state.game_type_67f53c;if(at==0x67dbe8)return state.mission_timer_67dbe8;
        if(at==0x8cfa1c)return std::uint32_t(state.tables.groups[0].size());if(at==0x8cfa18)return std::uint32_t(state.tables.groups[3].size());
        const auto found=cells.find(at);if(found==cells.end())throw MissionTickError("Logic cell has no owner "+std::to_string(at));return found->second;};
    cell_store=[&](std::uint32_t at,std::uint32_t n){if(claims(at,4)){c.sound.asset_bindings().write(at,4,n);return;}const auto found=cells.find(at);if(found==cells.end())throw MissionTickError("Logic store has no owner "+std::to_string(at));found->second=n;};
    shared_read=[&](std::uint32_t at,unsigned width){
        if(claims(at,width))return c.sound.asset_bindings().read(at,width);
        if(at>=0x6e9a90&&std::uint64_t(at)<0x6e9a90+17ull*mission_client_record_size){const auto key=at-0x6e9a90;return client_read(key/mission_client_record_size,key%mission_client_record_size,width);}
        for(std::uint8_t group=0;group<4;++group){const auto base=mission_actor_source_address(group,0);
            if(at>=base && std::uint64_t(at-base)<state.tables.groups[group].size()*0x114ull){const auto key=at-base;const auto image=snapshot_mission_actor(state,{group,key/0x114});std::uint32_t value=0;for(unsigned i=0;i<width;++i)value|=std::uint32_t(image.at(key%0x114+i))<<(8*i);return value;}}
        if(width!=4)throw MissionTickError("Logic scalar width is not four");return cell(at);};
    shared_store=[&](std::uint32_t at,unsigned width,std::uint32_t n){if(claims(at,width)){c.sound.asset_bindings().write(at,width,n);return;}if(width!=4)throw MissionTickError("Logic store width is not four");cell_store(at,n);};
    MissionClientOutboxBindings outbox_bindings;
    outbox_bindings.client=[&](auto slot){return clients.client(slot);};outbox_bindings.cell=cell;
    outbox=std::make_unique<MissionClientOutbox>(messages,server,outbox_bindings);

    MissionActorLifecycleBindings lifecycle_bindings;lifecycle_bindings.sincos_4776c0=[](auto angle,auto& sine,auto& cosine){sincos_q22(angle,sine,cosine);};
    lifecycle_bindings.terrain_47df40=[&](auto x,auto y){if(!state.world.terrain)throw MissionTickError("Lifecycle terrain has no owner");std::int32_t sx,sy;std::memcpy(&sx,&x,4);std::memcpy(&sy,&y,4);return std::uint32_t(ground_height_query(*state.world.terrain,sx,sy));};
    lifecycle=std::make_unique<MissionActorLifecycle>(state,animations,audio.registrations_6e0700,lifecycle_bindings);
    dispatch_object=[&](const auto& ref,U callback,U message){
        if(callback==0x45bf80){mission_object_barrel(barrel,mission_actor_source_address(ref.group,ref.index),barrel_source,message);return;}
        if(callback==0x451d40){mission_object_mine_update(mine_update,mission_actor_source_address(ref.group,ref.index),mine_source);return;}
        if(callback==0x450e40){mission_object_helo_update(helo,mission_actor_source_address(ref.group,ref.index),message,helo_source,helo_frame.size());return;}
        if(callback==0x451e70){run_tree(mission_actor_source_address(ref.group,ref.index),message);return;}
        if(callback==0x451fd0){run_seagull(mission_actor_source_address(ref.group,ref.index));return;}
        mission_object_update(state,ref,callback,message,object);
    };
    flag_bindings=mission_event_actor_flag_bindings(*lifecycle,*outbox,[&](const auto& ref,auto callback,auto message){dispatch_object(ref,callback,message);});
    event_host.flags=[&](auto side){mission_event_actor_flags(state,side,shared_damage.last_damage,flag_bindings);};
    event_host.remove=[&](auto side){mission_event_actor_remove(state,side,flag_bindings);};
    peer={cell,cell_store,{}};peer_timers.read=cell;peer_timers.write=cell_store;
    budget={shared_read,shared_store};round.store=shared_store;
     // Defined cold PE cell; full client startup remains separate.

    MissionRoundCheckBindings round_host;round_host.read=[&](auto at,auto width){if(at==0x6e5884&&width==1)return std::uint32_t(status_6e5884);return shared_read(at,width);};
    round_host.write=[&](auto at,auto width,auto value){if(at>=0x6e9a90&&std::uint64_t(at-0x6e9a90)+width<=17ull*mission_client_record_size){const auto delta=at-0x6e9a90;client_store(delta/mission_client_record_size,delta%mission_client_record_size,width,value);return;}shared_store(at,width,value);};
    round_host.enqueue_441750=[&](auto slot,auto type,auto* payload,auto length){outbox->enqueue(slot,std::int32_t(type),payload,length);};
    round_host.call=[&](auto site,const auto& args)->std::uint32_t{if(site==0x444ca0){mission_logic_request_result(result_bindings,args.at(0),result_caller_packet);return 0;}if(site==0x477620)return query_isqrt_477620(args.at(0));if(site==0x4423f0){std::uint8_t payload=std::uint8_t(args.at(1));mission_round_broadcast(round_checks,args.at(0),&payload,args.at(2));return 0;}throw MissionTickError("Periodic nested body is not bound "+std::to_string(site));};
    round_checks=mission_round_check_bindings(state,round_host);

    actor_message.broadcast=[&](auto type,auto& payload){mission_round_broadcast(round_checks,type,payload.data(),payload.size());};
    actor_message.read=[&](auto at,auto width){if(width!=4)throw MissionTickError("Actor phrase width is invalid");for(std::uint8_t group=0;group<4;++group){const auto base=mission_actor_source_address(group,0);if(at<base)continue;const auto delta=at-base,slot=delta/0x114,offset=delta%0x114;if(slot>=state.tables.groups[group].size()||offset+width>0x114)continue;const auto image=snapshot_mission_actor(state,{group,slot});std::uint32_t value;std::memcpy(&value,image.data()+offset,4);return offset==4?(value?definition_base+value*0x13cu:0u):value;}if(mission_extra_actor_record_base(at,width))return c.extra_memory.read(at,width);if(at>=definition_base&&std::uint64_t(at-definition_base)+width<=initial.definitions.size()*sizeof(RuntimeItemDefinition))return sound.asset_bindings().read(at,width);return sound.asset_bindings().read(at,width);};
    actor_message.positional=[&](auto id,Fixed3 position){sound.positional(id,position);};
    message_bindings.message=[this](auto id,const auto* packet,auto length){if(id!=0x15)return false;receive_scoreboard(packet,length);return true;};
    message_bindings.call=[&](const auto& call){if(call.address==0x471010){mission_logic_receive_result(result_bindings,call.payload,call.length);return;}if(call.address==0x43f5a0&&call.array==GameRecordArray::players_7cca80&&call.index>=0&&call.count==1){mission_actor_message_receive(actor_message,mission_actor_source_address(3,std::uint32_t(call.index)),call.args[0]);return;}throw MissionTickError("Message live call has no owner "+std::to_string(call.address));};messages.set_live_bindings(message_bindings);
    periodic.read=[&](U at,U width){
        if(at>=periodic_source&&std::uint64_t(at-periodic_source)+width<=periodic_frame.size()){U n=0;for(U i=0;i<width;++i)n|=U(periodic_frame.at(at-periodic_source+i))<<(i*8);return n;}
        if(at==0x8cfa18&&width==4)return U(state.tables.groups[3].size());
        if(at==0x8cfa1c&&width==4)return U(state.tables.groups[0].size());
        for(std::uint8_t group=0;group<4;++group){const auto base=mission_actor_source_address(group,0);if(at<base+4)continue;const auto offset=at-base;if(offset%0x114!=4||width!=4)continue;const auto slot=offset/0x114;if(slot>=state.tables.groups[group].size())continue;if(group==0&&h.primary_source_records)return c.sound.asset_bindings().read(at,width);const auto image=snapshot_mission_actor(state,{group,slot});U index;std::memcpy(&index,image.data()+4,4);return index?definition_base+index*0x13c:0u;}
        return c.sound.asset_bindings().read(at,width);
    };
    periodic.write=[&](U at,U width,U value){if(at>=periodic_source&&std::uint64_t(at-periodic_source)+width<=periodic_frame.size()){for(U i=0;i<width;++i)periodic_frame.at(at-periodic_source+i)=std::uint8_t(value>>(i*8));return;}c.sound.asset_bindings().write(at,width,value);};
    periodic.call=[&](U site,const auto& args)->U{
        if(site==0x4403c0){if(args.at(0)!=peer_leave_source)throw MissionTickError("Peer result frame has no current owner");mission_logic_build_result(result_bindings,peer_leave_packet);return 0;}
        if(site==0x460e40||site==0x442ae0||site==0x4421d0)return logic.call(site,args);
        if(site==0x490880)return need(h.pump_490880,"Mission leave pump is not bound")()?0u:1u;
        if(site==0x446da0){mission_periodic_team_scores(periodic);return 0;}
        if(site==0x448130)return U(mission_periodic_compare_clients(periodic,args.at(0),args.at(1)));
        if(site==0x49a620){for(U i=0;i<args.at(2);++i)periodic.write(args.at(0)+i,1,args.at(1)&255);return args.at(0);}
        if(site==0x4423f0||site==0x441750){
            const auto pointer=args.at(site==0x4423f0?1:2),length=args.at(site==0x4423f0?2:3);
            std::uint8_t* payload=nullptr;
            if(length){if(pointer>=peer_leave_source&&std::uint64_t(pointer-peer_leave_source)+length<=peer_leave_packet.size())payload=peer_leave_packet.data()+pointer-peer_leave_source;else{if(pointer<periodic_source||std::uint64_t(pointer-periodic_source)+length>periodic_frame.size())throw MissionTickError("Periodic payload has no current owner");payload=periodic_frame.data()+pointer-periodic_source;}}
            if(site==0x4423f0)mission_round_broadcast(round_checks,args.at(0),payload,length);
            else{const U client=args.at(0);if(client<mission_client_source_base||(client-mission_client_source_base)%mission_client_record_size)throw MissionTickError("Periodic client pointer is invalid");const auto slot=(client-mission_client_source_base)/mission_client_record_size;if(slot>=17)throw MissionTickError("Periodic client index exceeds its pool");outbox->enqueue(slot,std::int32_t(args.at(1)),payload,length);}
            return 0;
        }
        if(site==0x404cf0)return c.sound.asset_bindings().call(site,args);
        throw MissionTickError("Periodic required body is not bound "+std::to_string(site));
    };
    parser.read=[&](auto at,auto width){return periodic.read(at,width);};
    parser.write=[&](auto at,auto width,auto value){periodic.write(at,width,value);};
    parser.resolve=[this](U at,std::size_t length){auto& bytes=messages.peer_frame_buffer().bytes_8ec1cc;if(at<0x8ec1cc||std::uint64_t(at-0x8ec1cc)+length>bytes.size())throw MissionTickError("Peer parser source span has no current owner");return bytes.data()+at-0x8ec1cc;};
    parser.client=[&](auto slot){return clients.client(slot);};
    parser.call=[](const auto& call)->U{throw MissionTickError("Peer parser required body is not bound "+std::to_string(call.address));};
    logic.cell=cell;logic.store=cell_store;logic.client=client_read;logic.client_store=client_store;
    logic.actor=[&](auto group,auto slot,auto at,auto width){const auto image=snapshot_mission_actor(state,{std::uint8_t(group),slot});std::uint32_t n=0;for(unsigned i=0;i<width;++i)n|=std::uint32_t(image.at(at+i))<<(8*i);return n;};
    logic.actor_byte_store=[&](auto group,auto slot,auto at,auto n){auto image=snapshot_mission_actor(state,{std::uint8_t(group),slot});image.at(at)=n;apply_mission_actor_image(state,{std::uint8_t(group),slot},image);};
    logic.client_actor_flags=[&](auto slot)->std::optional<std::uint8_t>{const auto raw=client_read(slot,8,4);if(!raw)return std::nullopt;for(std::size_t index=0;index<state.tables.groups[3].size();++index)if(mission_actor_source_address(3,index)==raw)return snapshot_mission_actor(state,{3,index})[0x6c];throw MissionTickError("Client actor has no current physical owner");};
    logic.send=[&](auto id,const auto& packet){outbox->broadcast(std::int32_t(id),packet.data(),std::uint32_t(packet.size()));};
    logic.call=[&](auto site,const auto& args)->std::uint32_t{
        if(site==0x444ca0){mission_logic_request_result(result_bindings,args.at(0),result_caller_packet);return 0;}
        if(site==0x4438c0||site==0x448c30||site==0x445c80||site==0x446200||site==0x4482e0){run_periodic(site);return 0;}
        if(site==0x444630){mission_round_check_end(round_checks);return 0;}
        if(site==0x448590){mission_round_check_clients(round_checks);return 0;}
        if(site==0x447930){mission_round_publish_status(round_checks);return 0;}
        if(site==0x44a200){mission_type_counts(mission_type_count_bindings(state,types));return 0;}
        if(site==0x42eaf0){event_stage.tick();return 0;}
        if(site==0x45fd80){if(!reset_zones)throw MissionTickError("Zone reset has no owner");reset_zones();return 0;}
        if(site==0x442ae0){mission_peer_parser_parse(*outbox,messages.table(),parser);return 0;}
        if(site==0x460e40)return mission_peer_frame_receive(messages.peer_frame_buffer(),peer);
        if(site==0x4477f0){mission_peer_timers(peer_timers);return 0;}
        if(site==0x444e30){mission_peer_budget_update(budget);return 0;}
        if(site==0x444f60){mission_round_clear_frame_flag(round);return 0;}
        if(site==0x4421d0){outbox->tick();return 0;}
        throw MissionTickError("Logic body is not bound "+std::to_string(site));};
    logic_run=[&]{mission_logic_frame(logic);};
    helo.read=[&](U at,U width){
        if(at>=helo_source&&std::uint64_t(at-helo_source)+width<=helo_frame.size()){U n=0;for(U i=0;i<width;++i)n|=U(helo_frame.at(at-helo_source+i))<<(i*8);return n;}
        if(at>=helo_condition_source&&std::uint64_t(at-helo_condition_source)+width<=helo_condition_frame.size()){U n=0;for(U i=0;i<width;++i)n|=U(helo_condition_frame.at(at-helo_condition_source+i))<<(i*8);return n;}
        if(at>=helo_altitude_source&&std::uint64_t(at-helo_altitude_source)+width<=helo_altitude_frame.size()){U n=0;for(U i=0;i<width;++i)n|=U(helo_altitude_frame.at(at-helo_altitude_source+i))<<(i*8);return n;}
        if(at>=0x78f280&&std::uint64_t(at)+width<=0x793680){
            const U slot=(at-0x78f280)/0x88,offset=(at-0x78f280)%0x88;
            if(offset+width>0x88||slot>=state.world.behavior_world.lists.size())throw MissionTickError("Helicopter marker list has no current owner");
            const auto& list=state.world.behavior_world.lists[slot];std::array<U,34> words{};words[0]=list.flags;words[1]=list.count;std::copy(list.indices.begin(),list.indices.end(),words.begin()+2);
            U n=0;const auto* bytes=reinterpret_cast<const std::uint8_t*>(words.data());for(U i=0;i<width;++i)n|=U(bytes[offset+i])<<(i*8);return n;
        }
        if(at>=0x793680&&std::uint64_t(at)+width<=0x794680){U n=0;for(U i=0;i<width;++i)n|=U(types.bytes.at(at-0x793680+i))<<(i*8);return n;}
        if(at>=0x794680&&std::uint64_t(at)+width<=0x7cca80){const auto slot=(at-0x794680)/0xb4,offset=(at-0x794680)%0xb4;
            if(offset+width>0xb4||slot>=state.people.size())throw MissionTickError("Helicopter person read has no current owner");
            auto image=ai_person_image(state.people[slot]);const auto& extra=state.person_extra(slot);
            std::memcpy(image.data()+0x28,&extra.field_28,4);
            std::memcpy(image.data()+0x2c,&extra.field_2c,4);
            std::memcpy(image.data()+0x30,&extra.field_30,4);
            std::memcpy(image.data()+0x34,&extra.field_34,4);
            std::memcpy(image.data()+0x38,&extra.field_38,4);
            std::memcpy(image.data()+0x3c,&extra.field_3c,4);
            std::memcpy(image.data()+0x40,&extra.field_40,4);
            std::memcpy(image.data()+0x44,&extra.field_44,4);
            std::memcpy(image.data()+0x48,&extra.field_48,4);
            std::memcpy(image.data()+0x4c,&extra.field_4c,4);
            std::memcpy(image.data()+0x50,&extra.field_50,4);
            std::memcpy(image.data()+0x6c,&extra.field_6c,4);
            std::memcpy(image.data()+0xa0,&extra.field_a0,4);
            std::memcpy(image.data()+0xa4,&extra.field_a4,4);
            U n=0;for(U i=0;i<width;++i)n|=U(image.at(offset+i))<<(i*8);return n;
        }
        for(std::uint8_t group=0;group<4;++group){const auto base=mission_actor_source_address(group,0);if(at<base+0xc||width!=4)continue;const auto offset=at-base;if(offset%0x114!=0xc)continue;const auto slot=offset/0x114;if(slot>=state.tables.groups[group].size())continue;const auto person=state.tables.groups[group][slot].person_slot;if(person&&*person>=state.people.size())throw MissionTickError("Helicopter actor person has no current owner");return person?0x794680u+U(*person)*0xb4u:0u;}
        return periodic.read(at,width);
    };
    helo.write=[&](U at,U width,U value){
        if(at>=helo_source&&std::uint64_t(at-helo_source)+width<=helo_frame.size()){for(U i=0;i<width;++i)helo_frame.at(at-helo_source+i)=std::uint8_t(value>>(i*8));return;}
        if(at>=helo_condition_source&&std::uint64_t(at-helo_condition_source)+width<=helo_condition_frame.size()){for(U i=0;i<width;++i)helo_condition_frame.at(at-helo_condition_source+i)=std::uint8_t(value>>(i*8));return;}
        if(at>=helo_altitude_source&&std::uint64_t(at-helo_altitude_source)+width<=helo_altitude_frame.size()){for(U i=0;i<width;++i)helo_altitude_frame.at(at-helo_altitude_source+i)=std::uint8_t(value>>(i*8));return;}
        if(at>=0x794680&&std::uint64_t(at)+width<=0x7cca80){const auto slot=(at-0x794680)/0xb4,offset=(at-0x794680)%0xb4;
            if(slot>=state.people.size())throw MissionTickError("Helicopter person store has no current owner");
            U* word=nullptr;U base=0;
            auto& extra=state.person_extra(slot);
            if(offset>=0x28&&offset+width<=0x2c){word=&extra.field_28;base=0x28;}
            else if(offset>=0x2c&&offset+width<=0x30){word=&extra.field_2c;base=0x2c;}
            else if(offset>=0x30&&offset+width<=0x34){word=&extra.field_30;base=0x30;}
            else if(offset>=0x34&&offset+width<=0x38){word=&extra.field_34;base=0x34;}
            else if(offset>=0x38&&offset+width<=0x3c){word=&extra.field_38;base=0x38;}
            else if(offset>=0x3c&&offset+width<=0x40){word=&extra.field_3c;base=0x3c;}
            else if(offset>=0x40&&offset+width<=0x44){word=&extra.field_40;base=0x40;}
            else if(offset>=0x44&&offset+width<=0x48){word=&extra.field_44;base=0x44;}
            else if(offset>=0x48&&offset+width<=0x4c){word=&extra.field_48;base=0x48;}
            else if(offset>=0x4c&&offset+width<=0x50){word=&extra.field_4c;base=0x4c;}
            else if(offset>=0x50&&offset+width<=0x54){word=&extra.field_50;base=0x50;}
            else if(offset>=0x6c&&offset+width<=0x70){word=&extra.field_6c;base=0x6c;}
            else if(offset>=0xa0&&offset+width<=0xa4){word=&extra.field_a0;base=0xa0;}
            else if(offset>=0xa4&&offset+width<=0xa8){word=&extra.field_a4;base=0xa4;}
            else if(offset>=0xac&&offset+width<=0xb0){word=&state.people[slot].field_ac;base=0xac;}
            else if(offset>=0xb0&&offset+width<=0xb4){word=&state.people[slot].field_b0;base=0xb0;}
            else if(offset>=0x54&&offset+width<=0x60&&(offset-0x54)%4+width<=4){base=0x54+(offset-0x54)/4*4;word=&state.people[slot].position[(offset-0x54)/4];}
            else if(offset>=0x90&&offset+width<=0x94){word=&state.people[slot].field_90;base=0x90;}
            if(!word)throw MissionTickError("Helicopter person store needs its current field owner");
            auto* bytes=reinterpret_cast<std::uint8_t*>(word);for(U i=0;i<width;++i)bytes[offset-base+i]=std::uint8_t(value>>(i*8));if(base==0xa4)state.people[slot].tail_flag=*word!=0;return;
        }
        periodic.write(at,width,value);
    };
    helo.call=[&](U site,const auto& args)->U{
        if(site==0x486fd0){
            const auto signed_word=[](U word){std::int32_t n;std::memcpy(&n,&word,4);return n;};
            const std::array<std::int32_t,3> point{signed_word(helo.read(args.at(1),4)),signed_word(helo.read(args.at(1)+4,4)),signed_word(helo.read(args.at(1)+8,4))};
            auto height=ground_height_query(terrain.geometry,point[0],point[1]);
            const auto ref=mission_projectile_owner_source_ref(state,args.at(0));
            state.runtime_for(ref).ground_100=0;
            const auto& actor=*state.actor_at(ref);if(signed_word(actor.field_108)<=0)return U(height);
            const auto start=actor.field_104;
            for(U index=0;signed_word(index)<signed_word(actor.field_108);++index){
                const auto slot=std::uint64_t(start)+index;if(slot>=state.ai.target_list.size())throw MissionTickError("Helicopter ground target has no current owner");
                const auto target=state.ai.target_list[std::size_t(slot)];const auto image=snapshot_mission_actor(state,target);
                geometry.ground_entity(image,point,matrices,[&](std::int32_t surface){
                    if(surface<height||signed_word(U(point[2])+args.at(2))<surface)return;
                    state.runtime_for(ref).ground_100=actor_handle(target.group,target.index);height=surface;
                });
            }
            return U(height);
        }
        if(site==0x4509f0)return mission_object_helo_home(helo,args.at(0));
        if(site==0x450100){
            if(helo_altitude_active)throw MissionTickError("Helicopter altitude frame is already active");
            helo_altitude_active=true;struct Scope{bool& active;~Scope(){active=false;}} scope{helo_altitude_active};
            for(U i=0;i<4;++i)helo.write(helo_altitude_source+0x30+i*4,4,args.at(i));
            mission_object_helo_altitude(helo,helo_altitude_source);return 0;
        }
        if(site==0x477820){const U index=args.at(0)>>21;helo.write(args.at(1),4,U(kSineTableQ22[index]));helo.write(args.at(2),4,U(kSineTableQ22[index+512]));return 0;}
        if(site==0x450ae0){
            if(helo_condition_active)throw MissionTickError("Helicopter condition frame is already active");
            helo_condition_active=true;struct Scope{bool& active;~Scope(){active=false;}} scope{helo_condition_active};
            return mission_object_helo_condition(helo,args.at(0),helo_condition_source,helo_condition_frame.size());
        }
        if(site==0x45c6a0)return mission_object_helo_marker(helo,args.at(0),args.at(1),args.at(2),args.at(3),args.at(4));
        if(site==0x42ee30){event_stage.set_flag_42ee30(args.at(0),args.at(1),args.at(2));return 0;}
        if(site==0x477770)return query_angle_477770(args.at(0),args.at(1));
        if(site==0x477670){std::array<std::int32_t,3> n{};for(U i=0;i<3;++i)std::memcpy(&n[i],&args.at(i),4);return U(collision_approximate_length(n[0],n[1],n[2]));}
        if(site==0x43ef80){sound.attach(args.at(0),args.at(1),args.at(2),args.at(3));return 0;}
        if(site==0x43f020){sound.stop(args.at(0),args.at(1));return 0;}
        if(site==0x477860)return behavior_rng_next(state.behavior);
        if(site==0x47df40){std::int32_t x,y;std::memcpy(&x,&args.at(0),4);std::memcpy(&y,&args.at(1),4);return U(ground_height_query(terrain.geometry,x,y));}
        if(site==0x49a620){for(U i=0;i<args.at(2);++i)helo.write(args.at(0)+i,1,args.at(1)&255);return args.at(0);}
        throw MissionTickError("Helicopter required body is not bound "+std::to_string(site));
    };
    bind_mission_object_helo_move(state,helo);
    bind_mission_object_helo_update(state,helo,helo_source,helo_frame.size());
    if(std::uint64_t(color_base)+terrain.color.pixels.size()>animation_table_base||std::uint64_t(definition_base)+initial.definitions.size()*sizeof(RuntimeItemDefinition)>animation_table_base)
        throw MissionTickError("Animation table reservation overlaps current source memory");
    animation_tables=std::make_unique<MissionAnimationTableLive>(initial,cache,animation_table_base,0x2000,1024);
    tree.read=[&](U at,U width){
        if(at>=tree_source&&std::uint64_t(at-tree_source)+width<=tree_frame.size()){U n=0;for(U i=0;i<width;++i)n|=U(tree_frame.at(at-tree_source+i))<<(i*8);return n;}
        if(animation_tables->claims(at,width))return animation_tables->read(at,width);
        if(at==0xc23814&&width==4)return U(initial.definitions.size());
        if(at==0xc23820&&width==4)return state.config.local_actor?mission_actor_source_address(3,*state.config.local_actor):0u;
        if(at>=0x7cca80&&std::uint64_t(at)+width<=0x7dde80){
            const U slot=(at-0x7cca80)/0x114,offset=(at-0x7cca80)%0x114;
            if(offset+width>0x114)throw MissionTickError("Tree physical read crosses its actor record");
            auto image=slot<state.tables.groups[3].size()?snapshot_mission_actor(state,{3,slot}):group3_tail.at(slot);
            if(slot<state.tables.groups[3].size()){
                U index;std::memcpy(&index,image.data()+4,4);const U definition=index?definition_base+index*0x13c:0;
                std::memcpy(image.data()+4,&definition,4);
                const auto person=state.tables.groups[3][slot].person_slot;const U physical=person?0x794680+U(*person)*0xb4:0;
                std::memcpy(image.data()+0xc,&physical,4);
            }
            U n=0;for(U i=0;i<width;++i)n|=U(image.at(offset+i))<<(i*8);return n;
        }
        if(at>=definition_base&&std::uint64_t(at-definition_base)<initial.definitions.size()*sizeof(RuntimeItemDefinition)){
            const U offset=(at-definition_base)%sizeof(RuntimeItemDefinition);
            if(offset>=0x78&&offset+width<=0x7c){const auto index=(at-definition_base)/sizeof(RuntimeItemDefinition);const U pointer=animation_tables->table_address(initial.definitions.at(index).animation);return (pointer>>((offset-0x78)*8))&(width==4?0xffffffffu:((1u<<(width*8))-1));}
        }
        return periodic.read(at,width);
    };
    tree.write=[&](U at,U width,U value){if(at>=tree_source&&std::uint64_t(at-tree_source)+width<=tree_frame.size()){for(U i=0;i<width;++i)tree_frame.at(at-tree_source+i)=std::uint8_t(value>>(i*8));return;}periodic.write(at,width,value);};
    tree.call=[&](U site,const auto& args)->U{
        if(site==0x44fba0)return mission_object_tree_signal(tree,args.at(0));
        if(site==0x477670){std::array<std::int32_t,3> n{};for(U i=0;i<3;++i)std::memcpy(&n[i],&args.at(i),4);return U(collision_approximate_length(n[0],n[1],n[2]));}
        if(site==0x477860)return behavior_rng_next(state.behavior);
        if(site==0x43e7d0){Fixed3 p;std::memcpy(&p.x,&args.at(1),4);std::memcpy(&p.y,&args.at(2),4);std::memcpy(&p.z,&args.at(3),4);sound.positional(args.at(0),p);return 0;}
        if(site==0x49a620){for(U i=0;i<args.at(2);++i)tree.write(args.at(0)+i,1,args.at(1)&255);return args.at(0);}
        if(site==0x45d970){if(!h.primary_source_records)throw MissionTickError("Tree insertion needs current primary source records");auto raw=insertion;raw.read=tree.read;raw.write=tree.write;return mission_actor_insert(raw,args.at(0),args.at(1));}
        throw MissionTickError("Tree required body is not bound "+std::to_string(site));
    };
    if(state.update_callbacks.count(0x451e70))throw MissionTickError("Tree callback already has an owner");
    state.update_callbacks.emplace(0x451e70,[this](auto& current,const auto& ref,const auto& action){
        if(action.kind!=AiActorActionKind::update||action.target!=0x451e70||ref.group>=4||!current.actor_at(ref))throw MissionTickError("Tree callback actor or action has no current owner");
        const auto actor=mission_actor_source_address(ref.group,ref.index);(void)mission_projectile_owner_source_ref(current,actor);run_tree(actor,0);
    });
    seagull.read=[&](U at,U width){
        if(at>=seagull_source&&std::uint64_t(at-seagull_source)+width<=seagull_frame.size()){U n=0;for(U i=0;i<width;++i)n|=U(seagull_frame.at(at-seagull_source+i))<<(i*8);return n;}
        return periodic.read(at,width);
    };
    seagull.write=[&](U at,U width,U value){
        if(at>=seagull_source&&std::uint64_t(at-seagull_source)+width<=seagull_frame.size()){for(U i=0;i<width;++i)seagull_frame.at(at-seagull_source+i)=std::uint8_t(value>>(i*8));return;}
        periodic.write(at,width,value);
    };
    seagull.call=[&](U site,const auto& args)->U{
        if(site==0x477860)return behavior_rng_next(state.behavior);
        if(site==0x477820){const U index=args.at(0)>>21;seagull.write(args.at(1),4,U(kSineTableQ22.at(index)));seagull.write(args.at(2),4,U(kSineTableQ22.at(index+512)));return 0;}
        if(site==0x477670){std::array<std::int32_t,3> n{};for(U i=0;i<3;++i)std::memcpy(&n[i],&args.at(i),4);return U(collision_approximate_length(n[0],n[1],n[2]));}
        if(site==0x477770)return query_angle_477770(args.at(0),args.at(1));
        if(site==0x47df40){std::int32_t x,y;std::memcpy(&x,&args.at(0),4);std::memcpy(&y,&args.at(1),4);return U(ground_height_query(terrain.geometry,x,y));}
        if(site==0x43e7d0){Fixed3 p;std::memcpy(&p.x,&args.at(1),4);std::memcpy(&p.y,&args.at(2),4);std::memcpy(&p.z,&args.at(3),4);sound.positional(args.at(0),p);return 0;}
        throw MissionTickError("Seagull required body is not bound "+std::to_string(site));
    };
    if(state.update_callbacks.count(0x451fd0)||state.secondary_callbacks.count(0x452310))throw MissionTickError("Seagull callbacks already have an owner");
    state.update_callbacks.emplace(0x451fd0,[this](auto& current,const auto& ref,const auto& action){
        if(action.kind!=AiActorActionKind::update||action.target!=0x451fd0||!current.actor_at(ref))throw MissionTickError("Seagull AI actor or action has no current owner");
        const auto actor=mission_actor_source_address(ref.group,ref.index);(void)mission_projectile_owner_source_ref(current,actor);run_seagull(actor);
    });
    state.secondary_callbacks.emplace(0x452310,[this](auto& current,const auto& ref,const auto& action){
        if(action.kind!=AiActorActionKind::secondary||action.target!=0x452310||!current.actor_at(ref))throw MissionTickError("Seagull move actor or action has no current owner");
        const auto actor=mission_actor_source_address(ref.group,ref.index);(void)mission_projectile_owner_source_ref(current,actor);mission_object_seagull_move(seagull,actor);
    });
    bind_mission_object_mine_move(state,{periodic.read,periodic.write});
    barrel.read=[&](U at,U width){if(at>=barrel_source&&std::uint64_t(at-barrel_source)+width<=barrel_frame.size()){U value=0;for(U i=0;i<width;++i)value|=U(barrel_frame.at(at-barrel_source+i))<<(i*8);return value;}return periodic.read(at,width);};
    barrel.write=[&](U at,U width,U value){if(at>=barrel_source&&std::uint64_t(at-barrel_source)+width<=barrel_frame.size()){for(U i=0;i<width;++i)barrel_frame.at(at-barrel_source+i)=std::uint8_t(value>>(8*i));return;}periodic.write(at,width,value);};
    barrel.call=[&](U site,const auto& args)->U{
        if(site==0x4471d0){for(std::uint8_t group=0;group<4;++group)for(std::size_t slot=0;slot<state.tables.groups[group].size();++slot)if(mission_actor_source_address(group,slot)==args.at(0)){mission_event_actor_remove_one(state,{group,slot},flag_bindings);return 0;}throw MissionTickError("Barrel removal actor has no current physical owner");}
        throw MissionTickError("Barrel required body is not bound "+std::to_string(site));
    };
    bind_mission_object_barrel(state,barrel,barrel_source);
    mine_update.read=[&](U at,U width){if(at>=mine_source&&std::uint64_t(at-mine_source)+width<=mine_frame.size()){U value=0;for(U i=0;i<width;++i)value|=U(mine_frame.at(at-mine_source+i))<<(8*i);return value;}return periodic.read(at,width);};
    mine_update.write=[&](U at,U width,U value){if(at>=mine_source&&std::uint64_t(at-mine_source)+width<=mine_frame.size()){for(U i=0;i<width;++i)mine_frame.at(at-mine_source+i)=std::uint8_t(value>>(8*i));return;}periodic.write(at,width,value);};
    mine_update.call=[&](U site,const auto& args)->U{
        if(site==0x477820){const U index=args.at(0)>>21;mine_update.write(args.at(1),4,U(kSineTableQ22.at(index)));mine_update.write(args.at(2),4,U(kSineTableQ22.at(index+512)));return 0;}
        if(site==0x45da30){for(std::uint8_t group=0;group<4;++group)for(std::size_t slot=0;slot<state.tables.groups[group].size();++slot)if(mission_actor_source_address(group,slot)==args.at(0)){lifecycle->remove(AiActorRef{group,slot});return 0;}throw MissionTickError("Mine removal actor has no current physical owner");}
        throw MissionTickError("Mine update required body is not bound "+std::to_string(site));
    };
    bind_mission_object_mine_update(state,mine_update,mine_source);

    MissionRoundServiceBindings result_round;
    result_round.cell=[this](auto at,auto width){return c.sound.asset_bindings().read(at,width);};
    result_round.store=[this](auto at,auto width,auto value){c.sound.asset_bindings().write(at,width,value);};
    result_round.client=client_read;
    result_round.squad=[&](auto slot,auto at,auto width){return c.sound.asset_bindings().read(0x6e8090+U(slot)*0x68+at,width);};
    result_round.squad_store=[&](auto slot,auto at,auto width,auto value){c.sound.asset_bindings().write(0x6e8090+U(slot)*0x68+at,width,value);};
    MissionResultLiveHost result_host;
    result_host.state.cell=cell;result_host.state.store=cell_store;result_host.state.client=client_read;result_host.state.client_store=client_store;
    result_host.state.call=[this,result_round](auto site,const auto& args)->U{
        if(site==0x446100){mission_round_set_client_updates(result_round,args.at(0));return 0;}
        if(site==0x448d20){mission_round_log(result_round,args.at(0));return 0;}
        if(site==0x446da0){mission_round_collect_squads(result_round);return 0;}
        if(site==0x448130)return U(mission_round_compare_clients(result_round,args.at(0),args.at(1)));
        if(site==0x43e720){c.sound.global(args.at(0));return 0;}
        throw MissionTickError("Result required body is not bound "+std::to_string(site));
    };
    result_host.physical_tail=[this](const auto& ref){if(ref.group!=3||ref.index<c.state.tables.groups[3].size()||ref.index>=group3_tail.size())throw MissionTickError("Result physical tail has no current owner");return group3_tail.at(ref.index);};
    result_host.fallback_name=[this](U){const auto& memory=c.sound.asset_bindings();const auto text=memory.call(0x404cf0,{0x4ccccc,0x4cccc0});std::string name;for(U i=0;;++i){const auto ch=memory.read(text+i,1);if(!ch)return name;name.push_back(char(ch));}};
    result_host.store_name=[this](auto at,const auto& name){for(std::size_t i=0;i<=name.size();++i)c.sound.asset_bindings().write(at+U(i),1,i<name.size()?std::uint8_t(name[i]):0);};
    result_host.publish=[this](auto slot,auto id,auto& packet){outbox->enqueue(slot,std::int32_t(id),packet.data(),packet.size());};
    result_bindings=mission_result_live_bindings(state,std::move(result_host));
    event_host.result=[this](auto mode){mission_logic_request_result(result_bindings,mode,result_caller_packet);};

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
    ai_shot=std::make_unique<MissionAiShot>(state,muzzle,*spawn);
    state.world.weapon_effect=[&](auto&,const WeaponEvent& event){if(event.kind==WeaponEventKind::sound){const auto& a=state.tables.groups[3][*state.config.local_actor];Fixed3 position;std::memcpy(&position.x,&a.position[0],4);std::memcpy(&position.y,&a.position[1],4);std::memcpy(&position.z,&a.position[2],4);sound.positional(std::uint32_t(event.value),position);return true;}if(event.kind==WeaponEventKind::sound2){sound.global(std::uint32_t(event.value));return true;}if(event.kind==WeaponEventKind::message){if(event.value!=0)throw MissionTickError("Weapon text producer is not bound");MissionHudMessageCall call{0,hud.read(0x67db54,4)};call.text=sound.asset_bindings().call(0x404cf0,{0x4cdc38,0x4cdc2c});mission_hud_message(hud,call);return true;}if(event.kind==WeaponEventKind::queue){messages.queue().enqueue(messages.table(),std::uint8_t(event.value),nullptr,0);return true;}throw MissionTickError("Weapon effect has no live owner "+std::to_string(unsigned(event.kind)));};
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
    projectile_live.update=[&](auto&,const auto& ref,auto callback,auto message){dispatch_object(ref,callback,message);};
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
    bind_mission_actor_queries(state,[&](const auto& ref){auto data=mission_actor_query_data(state,ref,geometry,height_bounds.front(),zone_owner.active_records(),(rules_word&0x80)!=0,matrices);data.sight_line=[&,ref](std::size_t slot){return mission_object_sight(state,ref,{3,slot},sight_bindings);};return data;},[&](const auto& ref,const auto& action,auto&){if(action.site==0x45e7a0){if(!ai_shot)throw MissionTickError("AI shot owner is not installed");ai_shot->fire(ref);return true;}if(action.site==behavior_site::kRegionBit){event_stage.set_flag_42edd0(action.args[0],action.args[1]);return true;}if(action.site==behavior_site::kRegionBit2){event_stage.set_flag_42ee00(action.args[0],action.args[1]);return true;}if(action.site==behavior_site::kRegionBit3){event_stage.set_flag_42ee30(action.args[0],action.args[1],action.args[2]);return true;}if(action.site==behavior_site::kMessage){mission_actor_message_send_index(actor_message,action.args[0],action.args[1]);return true;}if(action.site!=behavior_site::kTypeSlotMark)return false;auto binding=mission_type_count_bindings(state,types);mission_type_mark(binding,action.args[0]);return true;});

        input.next_command=[this](auto tick)->std::optional<MissionCommandEvent>{auto record=c.input.pop_due(tick);if(!record)return {};return MissionCommandEvent{record->id,std::int32_t(record->value)};};
        input.dispatch_command=[this](auto id,auto value){return dispatch_game_command(id,value,c.state.globals,c.state.dispatch_actor,c.input.command_table());};
        if(!hud_input_initialized){
            if(c.input.stage().queue().size())throw MissionTickError("Cold input initialization has queued commands");
            mission_hud_message_clear(hud);
            c.input.stage().activate();
            state.globals.tick_base_bd4844=state.globals.command_tick_6e471c;
            hud_input_initialized=true;
        }
    }
};
MissionSession::MissionSession(MissionSessionContext c,MissionSessionHost h):impl_(std::make_unique<Impl>(c,std::move(h))){}
MissionSession::MissionSession(MissionSessionContext c,MissionSessionHost h,MissionResultImage& result):impl_(std::make_unique<Impl>(c,std::move(h),&result)){}
MissionSession::~MissionSession(){uninstall();}
void MissionSession::install(){auto& p=*impl_;if(p.active)throw MissionTickError("Mission session is already installed");p.prior_world=p.c.state.world;p.prior_updates=p.c.state.update_callbacks;p.prior_secondary=p.c.state.secondary_callbacks;p.active=true;const auto remember=[&]{for(std::size_t i=0;i<p.update_ids.size();++i)p.installed_updates[i]=!p.prior_updates.count(p.update_ids[i])&&p.c.state.update_callbacks.count(p.update_ids[i]);for(std::size_t i=0;i<p.secondary_ids.size();++i)p.installed_secondary[i]=!p.prior_secondary.count(p.secondary_ids[i])&&p.c.state.secondary_callbacks.count(p.secondary_ids[i]);};try{p.setup();remember();}catch(...){remember();uninstall();throw;}}
void MissionSession::uninstall() noexcept {auto& p=*impl_;if(!p.active)return;if(p.player)p.player->uninstall();using std::swap;swap(p.c.state.world,p.prior_world);for(std::size_t i=0;i<p.update_ids.size();++i)if(p.installed_updates[i])p.c.state.update_callbacks.erase(p.update_ids[i]);for(std::size_t i=0;i<p.secondary_ids.size();++i)if(p.installed_secondary[i])p.c.state.secondary_callbacks.erase(p.secondary_ids[i]);p.installed_updates.fill(false);p.installed_secondary.fill(false);p.active=false;}
bool MissionSession::installed() const noexcept{return impl_->active;}
MissionTickReport MissionSession::step(){if(!installed())throw MissionTickError("Mission session is not installed");return mission_tick(impl_->c.state,impl_->input);}
void MissionSession::request_result(U mode,MissionLogicResultPacket& packet){if(!installed())throw MissionTickError("Mission result requires an installed session");mission_logic_request_result(impl_->result_bindings,mode,packet);}
std::optional<U> MissionSession::leave_service(U site,const std::vector<U>& args){
    if(site!=0x403a50&&site!=0x4401e0&&site!=0x456e30)return {};
    if(!installed())throw MissionTickError("Leave service requires an installed Session");
    if(site==0x456e30){
        if(args.size()!=1)throw MissionTickError("Leave profiler needs one index");
        MissionStageCall call;call.address=site;call.args[0]=args[0];call.argument_count=1;
        return impl_->frame->stage(call);
    }
    if(!args.empty())throw MissionTickError("Peer leave takes no arguments");
    if(site==0x403a50)return mission_peer_leave_drain(impl_->periodic);
    mission_peer_reset(impl_->periodic,Impl::peer_leave_source);return 0;
}
void MissionSession::profile_leave(const std::function<AssetBytes&()>& current_profile){
    if(!installed())throw MissionTickError("Profile leave requires an installed Session");
    const auto& memory=impl_->c.sound.asset_bindings();
    MissionProfileBindings profile;
    profile.cell=[&](U at){return memory.read(at,4);};
    profile.store=[&](U at,U value){memory.write(at,4,value);};
    profile.profile=current_profile;
    profile.result=[this]{return impl_->result_image;};
    profile.fp_control=[this]{return impl_->fp_control_word;};
    profile.end_events=[this]{end_events();};
    mission_profile_leave(profile);
}
void MissionSession::end_events(){if(!installed())throw MissionTickError("Mission end events require an installed Session");impl_->event_stage.run_bit2_events();}
std::optional<U> MissionSession::hud_service(U site,const std::vector<U>& args){return impl_->hud_service(site,args);}
void MissionSession::script_message(U id,const std::uint8_t* bytes,std::size_t length){if(!installed()||!impl_->outbox)throw MissionTickError("Mission script outbox is not installed");if(length>std::numeric_limits<U>::max())throw MissionTickError("Mission script message is too large");impl_->outbox->broadcast(std::int32_t(id),bytes,U(length));}
U MissionSession::camera_yaw() const{return impl_->camera.yaw_6ba3e8;}

std::optional<CommandBinding> mission_session_input_query_row(const MissionSoundRuntime& sound,std::size_t row){if(row!=0x8c)throw MissionTickError("Loaded input query row is not bound");const U address=0x4fb2fc+U(row)*0x2c;const auto& memory=sound.asset_bindings();return CommandBinding{memory.read(address,4),{},std::uint16_t(memory.read(address+0x24,2)),std::uint16_t(memory.read(address+0x26,2)),std::uint16_t(memory.read(address+0x28,2)),std::uint16_t(memory.read(address+0x2a,2))};}
bool MissionSession::claims(U at,U width) const{return impl_->claims(at,width);}
U MissionSession::read(U at,U width) const{if(!claims(at,width))throw MissionTickError("Mission session read has no cell owner");U result=0;for(U i=0;i<width;++i)result|=U(*impl_->source_byte(at+i))<<(i*8);return result;}
void MissionSession::write(U at,U width,U value){if(!claims(at,width))throw MissionTickError("Mission session write has no cell owner");if(std::uint64_t(at)<std::uint64_t(Impl::color_base)+impl_->c.terrain.color.pixels.size()&&std::uint64_t(at)+width>Impl::color_base)throw MissionTickError("Mission terrain color bytes are read only");for(U i=0;i<width;++i)*impl_->source_byte(at+i)=std::uint8_t(value>>(i*8));}
}
