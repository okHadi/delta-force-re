#include "dfre/mission_tick.hpp"
#include "dfre/models.hpp"
#include "dfre/movement.hpp"
#include "dfre/world_runtime.hpp"

#include <cstring>
#include <algorithm>

namespace dfre {
namespace {

// ---------------------------------------------------------------------------
// 32-bit wrap helpers (all original arithmetic is 32-bit).
// ---------------------------------------------------------------------------
std::int32_t signed_bits(std::uint32_t value) noexcept {
    std::int32_t result;
    std::memcpy(&result, &value, sizeof(result));
    return result;
}
std::uint32_t unsigned_bits(std::int32_t value) noexcept {
    return static_cast<std::uint32_t>(value);
}
std::int32_t wrap_add(std::int32_t value, std::int32_t delta) noexcept {
    return signed_bits(unsigned_bits(value) + unsigned_bits(delta));
}
std::int32_t product_shift(std::int32_t left, std::int32_t right, unsigned shift) noexcept {
    return signed_bits(static_cast<std::uint32_t>(
        (static_cast<std::int64_t>(left) * static_cast<std::int64_t>(right)) >> shift));
}

// Original table bases used by the message payload index (pointer - base)/0x114.
constexpr std::uint32_t kExtraBase = 0x822E80;
constexpr std::uint32_t kActorStride = 0x114;

constexpr std::uint32_t kActorUpdate = 0x487370;
constexpr std::uint32_t kLocalControl = 0x488200;
constexpr std::uint32_t kFixed45E050 = 0x45E050;
constexpr std::uint32_t kProjectilePass = 0x464EB0;
constexpr std::uint32_t kPendingSpawnPass = 0x465BD0;
constexpr std::uint32_t kGroundEntityQuery = 0x486FD0;
constexpr std::uint32_t kClearanceScan = 0x4876E0;
constexpr std::uint32_t kGroundHeightQuery = 0x47DF40;
constexpr std::uint32_t kSlopeQuery = 0x47E5D0;
constexpr std::uint32_t kWeaponFire = 0x483C70;
constexpr std::uint32_t kWeaponReload = 0x483BD0;
constexpr std::uint32_t kWeaponSelect = 0x483DD0;
constexpr std::uint32_t kWeaponCycle = 0x484B70;
constexpr std::uint32_t kWeaponAim = 0x484C20;
constexpr std::uint32_t kWeaponResupply = 0x4848C0;
constexpr std::uint32_t kWeaponPart = 0x482B00;
constexpr std::uint32_t kMessageQueueCheck = 0x461050;
constexpr std::uint32_t kMessageHandlers = 0x46D9F0;
constexpr std::uint32_t kDetonatorCheck = 0x470FB0;
constexpr std::uint32_t kMessagePush = 0x46F3E0;
constexpr std::uint32_t kMessageFlush = 0x46F4B0;
constexpr std::uint32_t kTagEffect = 0x460D20;
constexpr std::uint32_t kTimer43F560 = 0x43F560;
constexpr std::uint32_t kCommandPump = 0x479450;
constexpr std::uint32_t kAiTick = 0x487EA0;
constexpr std::uint32_t kProfilerMark = 0x456E30;
constexpr std::uint32_t kMissionEventProcessor = 0x443C50;
constexpr std::uint32_t kCameraUpdate = 0x42AB70;
constexpr std::uint32_t kHudFrame = 0x479750;
constexpr std::uint32_t kHudTimers = 0x479ED0;
constexpr std::uint32_t kEffectPump = 0x43F0B0;
constexpr std::uint32_t kFrameTiming = 0x490880;
constexpr std::uint32_t kFrameEventRelease = 0x4562A0;
constexpr std::uint32_t kEarlyStop = 0x461350;
constexpr std::uint32_t kRequestState = 0x461340;
constexpr std::uint32_t kWeaponStage = 0x46F5C0;
constexpr std::uint32_t kSecondCounterBlock = 0x4702A0;
constexpr std::uint32_t kMenuHandler = 0x404E30;

// The 0x487EA0 +0x110 callbacks this module owns bodies for.
constexpr std::uint32_t kNullCallback = 0x45B590;
constexpr std::uint32_t kOrg0Callback = 0x45B5A0;
constexpr std::uint32_t kOrg1Callback = 0x45B5B0;
constexpr std::uint32_t kPlyrCallback = 0x45BDF0;

std::uint32_t raw_pointer(const AiActorRef& ref) {
    if(ref.group<4)return mission_actor_source_address(ref.group,ref.index);
    return kExtraBase + static_cast<std::uint32_t>(ref.index) * kActorStride;
}

void trace(MissionTickReport& report, MissionTraceKind kind, std::uint32_t address,
           const AiActorRef& ref, std::uint32_t a1 = 0, std::uint32_t a2 = 0,
           std::uint32_t a3 = 0, std::uint32_t a4 = 0) {
    MissionTraceEntry entry;
    entry.kind = kind;
    entry.address = address;
    entry.group = ref.group;
    entry.index = ref.index;
    entry.args = {a1, a2, a3, a4};
    report.trace.push_back(entry);
}
void trace_plain(MissionTickReport& report, MissionTraceKind kind, std::uint32_t address,
                 std::uint32_t a1 = 0, std::uint32_t a2 = 0) {
    trace(report, kind, address, AiActorRef{}, a1, a2);
}

std::uint32_t external_stage(MissionTickState& state,MissionTickReport& report,
    const MissionStageCall& call) {
    trace_plain(report,MissionTraceKind::external_stage,call.address,call.args[0],call.args[1]);
    if(!state.world.external_stage)throw MissionTickError("mission tick: external stage provider missing");
    const auto result=state.world.external_stage(state,call);
    if(!result)throw MissionTickError("mission tick: unbound external stage "+std::to_string(call.address));
    return *result;
}
std::uint32_t external_stage(MissionTickState& state,MissionTickReport& report,
    std::uint32_t address,std::initializer_list<std::uint32_t> args={}) {
    MissionStageCall call;call.address=address;call.argument_count=std::uint8_t(args.size());
    std::copy(args.begin(),args.end(),call.args.begin());
    return external_stage(state,report,call);
}

const AiItemDefinition& definition_for(const MissionTickState& state, const AiActorState& actor) {
    if (actor.definition_index >= state.definitions.entries().size())
        throw MissionTickError("mission tick: actor definition index outside the table");
    return state.definitions.entries()[actor.definition_index];
}

// ---------------------------------------------------------------------------
// Actor flags: the original dword at +0x6C spans +0x6C..+0x6E.
// ---------------------------------------------------------------------------
std::uint32_t actor_flags(const AiActorState& actor, const BehaviorActorExtra& extra,
                          const MissionActorRuntime& runtime) {
    return std::uint32_t(actor.field_6c) | (std::uint32_t(extra.field_6d) << 8) |
           (std::uint32_t(runtime.byte_6e) << 16);
}
void store_actor_flags(AiActorState& actor, BehaviorActorExtra& extra, MissionActorRuntime& runtime,
                       std::uint32_t flags) {
    actor.field_6c = static_cast<std::uint8_t>(flags & 0xFF);
    extra.field_6d = static_cast<std::uint8_t>((flags >> 8) & 0xFF);
    runtime.byte_6e = static_cast<std::uint8_t>((flags >> 16) & 0xFF);
}

bool is_local_actor(const MissionTickState& state, const AiActorRef& ref) {
    return ref.group == 3 && state.config.local_actor &&
           *state.config.local_actor == ref.index;
}

struct EntityCandidate {
    GroundEntity entity;
    std::uint32_t handle = 0;
};

// Builds the 0x486FD0 candidate list from the actor's 0x487C50 target list.
std::vector<AiActorRef> actor_targets(const MissionTickState& state,const AiActorState& actor) {
    const auto start=std::size_t(actor.field_104),count=std::size_t(actor.field_108);
    if(start>state.ai.target_list.size() || count>state.ai.target_list.size()-start)
        throw MissionTickError("mission tick: actor target span exceeds the shared list");
    return {state.ai.target_list.begin()+start,state.ai.target_list.begin()+start+count};
}

std::vector<EntityCandidate> target_entities(MissionTickState& state,const AiActorState& actor,
    const std::array<std::int32_t,3>& point) {
    std::vector<EntityCandidate> out;
    for (const AiActorRef& target : actor_targets(state,actor)) {
        const AiActorState* candidate = state.actor_at(target);
        if (candidate == nullptr) continue;
        std::uint32_t handle = candidate->field_10;
        if ((candidate->field_6c & 8) != 0 && candidate->field_14 != 0)
            handle = candidate->field_14;
        if (handle == 0) continue;
        if(!state.world.ground_entity)throw MissionTickError("mission tick: ground entity binding missing (0x486FD0)");
        const auto entity = state.world.ground_entity(target,point);
        if (!entity) continue;
        out.push_back({*entity, actor_handle(target.group,target.index)});
    }
    return out;
}

std::uint32_t entity_handle(const std::vector<EntityCandidate>& list,
                            const std::vector<GroundEntity>& view, const GroundEntity* pointer) {
    if (pointer == nullptr) return 0;
    for (std::size_t i=0;i<list.size();++i)
        if (&view[i] == pointer) return list[i].handle;
    return 0;
}

class WeaponEffectScope {
    MissionTickState& state_;
    std::function<void(const WeaponEvent&)> previous_;
    std::function<bool()> previous_query_;
    MissionTickReport* previous_report_;
public:
    WeaponEffectScope(MissionTickState& state,MissionTickReport* report,bool bind_query=true):state_(state),
        previous_(std::move(state.weapons.on_event)),previous_query_(std::move(state.weapons.trigger_query)),
        previous_report_(state.current_report) {
        if(report)state.current_report=report;
        if(bind_query)state.weapons.trigger_query=[&state]{
            if(!state.world.trigger_held_47a290)throw MissionTickError("mission tick: trigger input binding missing (0x47A290)");
            return state.world.trigger_held_47a290(0x8c);
        };
        else state.weapons.trigger_query=previous_query_;
        state.weapons.on_event=[&state](const WeaponEvent& event){
            mission_weapon_effect(state,event);
        };
    }
    ~WeaponEffectScope(){state_.weapons.on_event=std::move(previous_);state_.weapons.trigger_query=std::move(previous_query_);state_.current_report=previous_report_;}
};
void drain_weapon_events(MissionTickState& state, MissionTickReport&) {
    state.weapons.take_events(); // Effects ran synchronously at emit; discard observations.
}

// ---------------------------------------------------------------------------
// Behavior callback providers
// ---------------------------------------------------------------------------
std::optional<BehaviorInput> behavior_answer(MissionTickState& state, const AiActorRef& ref,
                                             const BehaviorAction& request,
                                             const BehaviorActor& view) {
    std::optional<BehaviorInput> answer;
    if (state.world.behavior_query.answer)
        answer = state.world.behavior_query.answer(ref, request.site, request, view);
    if (answer) return answer;
    // Weapon table AD3ED0: +0xA4 flags or +0x9C fire delay at 45B958.
    if (request.site == behavior_site::kWeaponFlags810 ||
        request.site == behavior_site::kWeaponFlags880 ||
        request.site == behavior_site::kWeaponStep) {
        const std::uint32_t weapon_index = request.args[0];
        if (weapon_index < kWeaponCount) {
            BehaviorInput input;
            input.site = request.site;
            input.value = request.site==behavior_site::kWeaponStep?
                std::uint32_t(weapon_definitions()[weapon_index].fire_delay):weapon_definitions()[weapon_index].flags;
            return input;
        }
    }
    return std::nullopt;
}

// Providers run once at the original call point against current actor state.
template <typename Body>
void run_behavior_callback(MissionTickState& state,const AiActorRef& ref,MissionTickReport& report,
                           Body&& body) {
    auto* actor=state.actor_at(ref);
    if(!actor || !actor->person_slot || *actor->person_slot>=state.people.size())
        throw MissionTickError("mission tick: human callback needs a live person record");
    BehaviorActor view{BehaviorActorRef{ref.group,ref.index},raw_pointer(ref),*actor,
        state.people[*actor->person_slot],state.extra_for(ref),state.person_extra(*actor->person_slot)};
    auto& live=state.behavior;
    struct ProviderScope {
        BehaviorState& state;
        decltype(BehaviorState::on_query) query;
        decltype(BehaviorState::on_effect) effect;
        ~ProviderScope(){state.on_query=std::move(query);state.on_effect=std::move(effect);}
    } scope{live,std::move(live.on_query),std::move(live.on_effect)};
    live.host=state.globals.mission_67f528!=0;
    live.inputs.clear();live.actions.clear();live.cursor=0;
    live.on_query=[&](const BehaviorAction& request) {
        ++report.behavior_queries;
        trace(report,MissionTraceKind::behavior_query,request.site,ref,
            request.args[0],request.args[1],request.args[2]);
        auto answer=behavior_answer(state,ref,request,view);
        if(!answer)throw MissionTickError("mission tick: missing behavior query "+std::to_string(request.site));
        answer->site=request.site;
        return *answer;
    };
    live.on_effect=[&](const BehaviorAction& action) {
        trace(report,MissionTraceKind::behavior_effect,action.site,ref,
            action.args[0],action.args[1],action.args[2]);
        if(!state.world.behavior_query.effect || !state.world.behavior_query.effect(ref,action,view))
            throw MissionTickError("mission tick: missing behavior effect "+std::to_string(action.site));
    };
    body(view,live);
}

} // namespace

void mission_weapon_effect(MissionTickState& state,const WeaponEvent& event){
    if(state.current_report)trace_plain(*state.current_report,MissionTraceKind::weapon_event,
        static_cast<std::uint32_t>(event.kind),unsigned_bits(event.value));
    if(!state.world.weapon_effect || !state.world.weapon_effect(state,event))
        throw MissionTickError("mission tick: weapon effect binding missing");
}

// ---------------------------------------------------------------------------
// State accessors
// ---------------------------------------------------------------------------
AiActorState* MissionTickState::actor_at(const AiActorRef& ref) {
    if (ref.group > 3) {
        if (ref.index >= tables.extra.size()) return nullptr;
        return &tables.extra[ref.index];
    }
    if (ref.index >= tables.groups[ref.group].size()) return nullptr;
    return &tables.groups[ref.group][ref.index];
}
const AiActorState* MissionTickState::actor_at(const AiActorRef& ref) const {
    return const_cast<MissionTickState*>(this)->actor_at(ref);
}

MissionActorRuntime& MissionTickState::runtime_for(const AiActorRef& ref) {
    const std::size_t group = ref.group > 3 ? 4 : ref.group;
    auto& slots = runtimes[group];
    if (ref.index >= slots.size()) slots.resize(ref.index + 1);
    if (!slots[ref.index]) slots[ref.index].emplace();
    return *slots[ref.index];
}

BehaviorActorExtra& MissionTickState::extra_for(const AiActorRef& ref) {
    const std::size_t group = ref.group > 3 ? 4 : ref.group;
    auto& slots = actor_extras[group];
    if (ref.index >= slots.size()) slots.resize(ref.index + 1);
    return slots[ref.index];
}

BehaviorPersonExtra& MissionTickState::person_extra(std::size_t person_slot) {
    if (person_slot >= person_extras.size()) person_extras.resize(person_slot + 1);
    return person_extras[person_slot];
}

void MissionTickState::bind_animation(const AiActorRef& ref, AnimationController& controller,
                                      std::vector<AnimationDriverAction> actions) {
    MissionActorRuntime& runtime = runtime_for(ref);
    runtime.animation = &controller;
    runtime.driver.emplace(controller, std::move(actions));
}

void init_mission_tick(MissionTickState& state, MissionTickConfig config, AiActorTables tables,
                       AiItemDefinitionTable definitions, std::vector<AiPersonState> people,
                       std::uint32_t behavior_rng) {
    state.config = config;
    state.globals.mission_67f528=config.host?1u:0u;
    state.globals.pause_6e4714=config.paused?1u:0u;
    state.globals.pause2_6e4718=config.demo?1u:0u;
    state.tables = std::move(tables);
    state.definitions = std::move(definitions);
    state.people = std::move(people);
    state.ai = AiUpdateState{};
    state.ai.paused = config.paused;
    state.ai.demo = config.demo;
    state.ai.local_actor = config.local_actor;
    state.behavior.rng = behavior_rng;
    state.behavior.host = config.host;
    for (std::size_t group = 0; group < 4; ++group) {
        state.actor_extras[group].assign(state.tables.groups[group].size(), BehaviorActorExtra{});
        state.runtimes[group].assign(state.tables.groups[group].size(), std::nullopt);
    }
    if(state.tables.extra.size()>0x400)throw MissionTickError("mission tick: extra actor pool exceeds1024 physical records");
    state.stored_extra_actor_images.clear();
    state.extra_actor_encoding.assign(state.tables.extra.size(),MissionExtraActorEncoding{});
    for(const auto& actor:state.tables.extra)state.stored_extra_actor_images.push_back(ai_actor_image(actor));
    state.actor_extras[4].assign(state.tables.extra.size(), BehaviorActorExtra{});
    state.runtimes[4].assign(state.tables.extra.size(), std::nullopt);
    for(std::size_t group=0;group<4;++group){
        state.stored_actor_images[group].clear();
        state.person_encoding[group].assign(state.tables.groups[group].size(),ActorPersonEncoding::slot);
        for(const auto& actor:state.tables.groups[group])state.stored_actor_images[group].push_back(ai_actor_image(actor));
    }
    state.person_extras.assign(state.people.size(), BehaviorPersonExtra{});
    for (std::size_t slot = 0; slot < state.people.size(); ++slot)
        if (state.people[slot].tail_flag) state.person_extras[slot].field_a4 = 1;
    install_human_behavior_callbacks(state);
}

void init_mission_tick(MissionTickState& state,MissionTickConfig config,
    const MissionRuntimeState& initial,std::uint32_t behavior_rng) {
    auto seed=seed_mission_actors(initial);
    std::vector<AiItemDefinition> definitions;
    for(const auto& item:initial.definitions)definitions.push_back({item.id,item.flags,item.model,
        item.ai_data,item.move_data,std::uint16_t(item.hp),item.type,item.effect_count});
    if(!config.local_actor && initial.local_player)config.local_actor=initial.local_player->actor;
    init_mission_tick(state,config,std::move(seed.tables),AiItemDefinitionTable(std::move(definitions)),initial.people,behavior_rng);
    state.stored_actor_images=std::move(seed.images);state.person_encoding=std::move(seed.person_encoding);
    const auto word=[](const AiActorImage& image,std::size_t at){std::uint32_t value;std::memcpy(&value,image.data()+at,4);return value;};
    for(std::size_t group=0;group<4;++group){
        state.actor_extras[group]=std::move(seed.extras[group]);
        for(std::size_t slot=0;slot<state.stored_actor_images[group].size();++slot){
            const auto& image=state.stored_actor_images[group][slot];
            auto& runtime=state.runtime_for({std::uint8_t(group),slot});
            runtime.byte_6e=image[0x6e];runtime.roll_38=word(image,0x38);runtime.vertical_44=signed_bits(word(image,0x44));
            runtime.ground_100=word(image,0x100);runtime.link_90=word(image,0x90);
        }
    }
}
AiActorImage snapshot_mission_actor(const MissionTickState& state,const AiActorRef& ref) {
    const bool extra_group=ref.group==kAiExtraTableGroup;
    if(ref.group>kAiExtraTableGroup || (extra_group?(ref.index>=state.tables.extra.size()||ref.index>=0x400):ref.index>=state.tables.groups[ref.group].size()))
        throw MissionTickError("mission tick: snapshot actor outside the mission tables");
    const auto group=ref.group;const auto slot=ref.index;
    const auto& stored_rows=extra_group?state.stored_extra_actor_images:state.stored_actor_images[group];
    const auto stored=slot<stored_rows.size()?stored_rows[slot]:AiActorImage{};
    const auto extra=slot<state.actor_extras[group].size()?state.actor_extras[group][slot]:BehaviorActorExtra{};
    const auto encoding=extra_group?(slot<state.extra_actor_encoding.size()?state.extra_actor_encoding[slot].person:ActorPersonEncoding::slot):(slot<state.person_encoding[group].size()?state.person_encoding[group][slot]:ActorPersonEncoding::handle);
    const auto& actor=extra_group?state.tables.extra[slot]:state.tables.groups[group][slot];
    auto image=merge_live_actor_image(stored,actor,extra,encoding);
    if(extra_group)std::memcpy(image.data()+4,stored.data()+4,4);
    const auto put=[&](std::size_t at,const auto& value){std::memcpy(image.data()+at,&value,sizeof(value));};
    if(slot<state.runtimes[group].size() && state.runtimes[group][slot]){
        const auto& runtime=*state.runtimes[group][slot];
        put(0x6e,runtime.byte_6e);put(0x38,runtime.roll_38);put(0x44,runtime.vertical_44);
        put(0x100,runtime.ground_100);put(0x90,runtime.link_90);
        if(runtime.driver){const auto action=std::uint32_t(runtime.driver->current_action());put(0x80,action);}
    }
    return image;
}
void clear_mission_extra_actor(MissionTickState& state,std::size_t index){
    if(index>=0x400||index>=state.tables.extra.size())throw MissionTickError("mission tick: extra actor clear exceeds its physical pool");
    state.tables.extra[index]=AiActorState{};
    state.stored_extra_actor_images.at(index).fill(0);
    state.extra_actor_encoding.at(index)=MissionExtraActorEncoding{};
    state.actor_extras[4].at(index)=BehaviorActorExtra{};
    state.runtimes[4].at(index).reset();
}
MissionActorImages snapshot_mission_actors(const MissionTickState& state) {
    MissionActorImages images;
    for(std::size_t group=0;group<4;++group)for(std::size_t slot=0;slot<state.tables.groups[group].size();++slot)
        images[group].push_back(snapshot_mission_actor(state,{std::uint8_t(group),slot}));
    return images;
}

void bind_mission_actor_queries(MissionTickState& state,
    std::function<ActorQueryData(const AiActorRef&)> source,
    std::function<bool(const AiActorRef&,const BehaviorAction&,BehaviorActor&)> sink) {
    if(!source)throw MissionTickError("mission tick: NPC query data source missing");
    auto provider=[&state,source](const AiActorRef& ref) {
        ActorQueryTable table{&state.tables.groups[3],&state.people,&state.actor_extras[3],&state.person_extras};
        auto data=source(ref);
        data.water_level=state.world.water_level;
        if(!data.raw_pointer)data.raw_pointer=[](std::size_t slot){return raw_pointer({3,slot});};
        return ActorQueryProvider(table,std::move(data));
    };
    state.world.behavior_query.answer=[&state,provider](const AiActorRef& ref,std::uint32_t site,
        const BehaviorAction& request,const BehaviorActor& view) {
        auto live_view=view;
        return provider(ref).answer(ref,site,request,live_view,state.behavior);
    };
    state.world.behavior_query.effect=[&state,provider,sink](const AiActorRef& ref,
        const BehaviorAction& action,BehaviorActor& view) {
        if(provider(ref).perform_effect(ref,action,state.behavior))return true;
        return sink?sink(ref,action,view):false;
    };
}

// ---------------------------------------------------------------------------
// Callback registry + dispatch
// ---------------------------------------------------------------------------
void register_actor_callback(MissionTickState& state, std::uint32_t address, bool secondary,
                             std::function<void(MissionTickState&, const AiActorRef&,
                                                const AiActorAction&)> body) {
    if (secondary) state.secondary_callbacks[address] = std::move(body);
    else state.update_callbacks[address] = std::move(body);
}

void install_human_behavior_callbacks(MissionTickState& state) {
    state.update_callbacks[kNullCallback] =
        [](MissionTickState& s, const AiActorRef& ref, const AiActorAction&) {
            AiActorState* actor = s.actor_at(ref);
            if (actor == nullptr) throw MissionTickError("null callback actor missing");
            BehaviorActorExtra& extra = s.extra_for(ref);
            AiPersonState dummy;
            BehaviorActor view{BehaviorActorRef{ref.group, ref.index}, 0, *actor, dummy, extra,
                               s.person_extra(0)};
            behavior_null(view);
        };
    state.update_callbacks[kOrg0Callback] =
        [](MissionTickState& s, const AiActorRef& ref, const AiActorAction&) {
            AiActorState* actor = s.actor_at(ref);
            if (actor == nullptr) throw MissionTickError("org0 callback actor missing");
            BehaviorActorExtra& extra = s.extra_for(ref);
            AiPersonState dummy;
            BehaviorActor view{BehaviorActorRef{ref.group, ref.index}, 0, *actor, dummy, extra,
                               s.person_extra(0)};
            behavior_org0(view);
        };
    state.update_callbacks[kOrg1Callback] =
        [](MissionTickState& s, const AiActorRef& ref, const AiActorAction&) {
            const BehaviorWorld* world = &s.world.behavior_world;
            run_behavior_callback(
                s, ref, *s.current_report,
                [world](BehaviorActor& view, BehaviorState& scratch) {
                    behavior_org1(view, *world, scratch, 0);
                });
        };
    state.update_callbacks[kPlyrCallback] =
        [](MissionTickState& s, const AiActorRef& ref, const AiActorAction&) {
            const BehaviorWorld* world = &s.world.behavior_world;
            run_behavior_callback(
                s, ref, *s.current_report,
                [world](BehaviorActor& view, BehaviorState& scratch) {
                    behavior_plyr(view, *world, scratch);
                });
        };
}

void run_actor_action(MissionTickState& state, const AiActorAction& action,
                      MissionTickReport& report) {
    WeaponEffectScope effects(state,&report);
    const bool is_update = action.kind == AiActorActionKind::update;
    const auto& registry = is_update ? state.update_callbacks : state.secondary_callbacks;
    const auto body = registry.find(action.target);

    if(state.actor_at(action.actor)==nullptr)
        throw MissionTickError("mission tick: action outside the actor tables");

    if (body == registry.end()) {
        ++report.callbacks_missing;
        trace(report, MissionTraceKind::callback_missing, action.target, action.actor,
              std::uint32_t(is_update ? 0x110 : 0x10C));
        throw MissionTickError("mission tick: actor callback body missing "+std::to_string(action.target));
    } else {
        struct ReportScope {
            MissionTickState& state;
            MissionTickReport* previous;
            ~ReportScope(){state.current_report=previous;}
        } scope{state,state.current_report};
        state.current_report = &report;
        body->second(state, action.actor, action);
        ++report.callbacks_run;
        trace(report, MissionTraceKind::actor_callback, action.target, action.actor,
              std::uint32_t(is_update ? 0x110 : 0x10C));
    }
}

// ---------------------------------------------------------------------------
// 0x487370 per-actor update join
// ---------------------------------------------------------------------------
void mission_actor_update(MissionTickState& state, const AiActorRef& ref, std::uint32_t tick,
                          MissionTickReport& report) {
    WeaponEffectScope effects(state,&report);
    AiActorState* actor_pointer = state.actor_at(ref);
    if (actor_pointer == nullptr)
        throw MissionTickError("mission tick: actor update outside the tables");
    AiActorState& actor = *actor_pointer;
    MissionActorRuntime& runtime = state.runtime_for(ref);
    BehaviorActorExtra& extra = state.extra_for(ref);
    const bool local = is_local_actor(state, ref);
    std::uint32_t flags = actor_flags(actor, extra, runtime);

    trace(report, MissionTraceKind::stage, kActorUpdate, ref, unsigned_bits(actor.position[0]),
          unsigned_bits(actor.position[1]), unsigned_bits(actor.position[2]));

    if (runtime.animation == nullptr || !runtime.driver)
        throw MissionTickError("mission tick: actor update needs the bound animation");

    const auto clearance_root_count=runtime.animation->root_count();
    // Frame mask is read before the animation tick (0x487382..0x4873AE).
    std::uint32_t frame_mask = 0;
    const bool have_mask = state.world.frame_mask != nullptr;
    if (have_mask) frame_mask = state.world.frame_mask(ref);

    // 0x4877F4 / 0x4873B4 / 0x4873D3 through the animation driver.
    const auto driver_inputs=[&] {
        AnimationDriverState driver_state;
        driver_state.update_flags = (tick & 1) != 0 ? 1 : 0;
        driver_state.pending_action = signed_bits(actor.field_7c);
        driver_state.heading_raw = signed_bits(actor.angle_yaw);
        driver_state.object_angles[0] = signed_bits(actor.position[0]);
        driver_state.object_angles[1] = signed_bits(actor.position[1]);
        driver_state.object_angles[2] = signed_bits(actor.position[2]);
        driver_state.anim_flags = runtime.anim_flags_48;
        driver_state.stance = runtime.stance_64;
        driver_state.pitch = runtime.pitch_65;
        driver_state.byte78 = runtime.byte78;
        driver_state.byte79 = runtime.byte79;
        driver_state.aim_a = runtime.aim_a_7a;
        driver_state.aim_b = runtime.aim_b_7b;
        driver_state.switch_flag = runtime.switch_66;
        driver_state.extra_4c = runtime.extra_4c;
        return driver_state;
    };
    const AnimationDriverUpdate update=runtime.driver->update(driver_inputs(),[&](AnimationDriverState& live){
        trace(report,MissionTraceKind::effect,kTagEffect,ref);
        external_stage(state,report,kTagEffect,{raw_pointer(ref),0,0,0});
        live=driver_inputs();
    });

    if(update.dispatch.target==AnimationEventTarget::call_43f5a0)
        external_stage(state,report,0x43f5a0,{raw_pointer(ref),update.dispatch.value});
    else if(update.dispatch.target==AnimationEventTarget::call_45e0f0)
        external_stage(state,report,0x45e0f0,{update.dispatch.value,update.dispatch.value2,raw_pointer(ref)});
    flags=actor_flags(actor,extra,runtime);

    const AnimationDriverAction* current =
        runtime.driver->action(static_cast<std::uint32_t>(update.action_index));
    const std::int32_t frame_speed = current ? current->velocity_per_tick : 0;
    const bool frame_vertical = current != nullptr && current->override_flag;

    StepVector step;
    const bool odd = (tick & 1) != 0;
    if (odd) {
        FrameStepInput input;
        input.frame_angle = update.action_direction;
        input.frame_speed = frame_speed;
        input.frame_vertical = frame_vertical;
        input.walk_3d = (flags & actor_in_water) != 0;
        input.definition_side = runtime.animation->motion()[0];
        input.definition_vertical = runtime.animation->motion()[1];
        input.definition_up = runtime.animation->motion()[2];
        input.position_z = signed_bits(actor.position[2]);
        input.water_level = state.world.water_level;
        input.base = frame_base_sincos(actor.angle_yaw, update.action_direction);
        input.pitch_pair = frame_pitch_sincos(actor.angle_pitch);
        step = frame_step(input);

        const bool slope_eligible =
            (flags & actor_active) != 0 && (flags & (actor_airborne | actor_in_water)) == 0 &&
            runtime.ground_100 == 0;
        if (!slope_eligible) {
            step = cached_step(step, (flags & actor_airborne) != 0, runtime.step_cache);
        } else {
            if (state.world.terrain == nullptr)
                throw MissionTickError("mission tick: slope query needs the terrain map");
            const GroundSlope slope =
                ground_slope_query(*state.world.terrain, signed_bits(actor.position[0]),
                                   signed_bits(actor.position[1]));
            const SlopeResponse response =
                terrain_slope_response(step, flags, slope.x, slope.y, input.base);
            step = response.step;
            if (response.applied) {
                flags &= ~(actor_slope_blocked | actor_slope_step);
                if (response.slope_blocked) flags |= actor_slope_blocked;
                if (response.wall) flags |= actor_slope_step;
                store_actor_flags(actor, extra, runtime, flags);
            }
            runtime.step_cache = {step.x, step.y, step.z};
        }
        trace(report, MissionTraceKind::effect, kSlopeQuery, ref, unsigned_bits(step.x),
              unsigned_bits(step.y), unsigned_bits(step.z));
    } else {
        // 0x4879D1 reads the per-slot cache.
        step = {runtime.step_cache.x, runtime.step_cache.y, runtime.step_cache.z};
    }

    // 0x487565 ground object transform + position += step. full_pass and
    // is_local are false here so the proven ground_update only moves the actor.
    MotionActor motion;
    motion.position[0] = signed_bits(actor.position[0]);
    motion.position[1] = signed_bits(actor.position[1]);
    motion.position[2] = signed_bits(actor.position[2]);
    motion.yaw = actor.angle_yaw;
    motion.vertical = runtime.vertical_44;
    motion.flags = flags;
    motion.on_ground_object = runtime.ground_100 != 0;

    GroundObject ground_object;
    const GroundObject* ground_pointer = nullptr;
    if (runtime.ground_100 != 0) {
        std::optional<GroundObject> object;
        if (state.world.ground_object) object = state.world.ground_object(runtime.ground_100);
        if (object) {
            ground_object = *object;
            ground_pointer = &ground_object;
        } else {
            throw MissionTickError("mission tick: ground object binding missing");
        }
    }
    const GroundQuery move_only;
    ground_update(motion, ground_pointer, move_only, step, false);

    // 0x4876A0..0x4876D0 target collision (odd ticks or the local player).
    if ((odd || local) && actor.field_108 != 0) {
        std::uint32_t result = 0;
        int passes = 0;
        do {
            result = 0;
            for (const AiActorRef& target : actor_targets(state,actor)) {
                const AiActorState* candidate = state.actor_at(target);
                if (candidate == nullptr) continue;
                if(step.x==0 && step.y==0 && step.z==0 &&
                    (candidate->definition_index==0 || definition_for(state,*candidate).id==2))continue;
                if (state.world.target_collision) {
                    result |= state.world.target_collision(ref, target);
                } else {
                    throw MissionTickError("mission tick: target collision binding missing (0x4889C0)");
                }
            }
            passes += 1;
        } while (result != 0 && passes < 2);
    }

    // 0x4876E0..0x4877DE clearance, ground height and snap (tick & 3 == 3 or
    // the local player).
    if (((tick & 3) == 3) || local) {
        std::int32_t clearance = clearance_none;
        if (runtime.animation != nullptr && have_mask) {
            const auto& roots=runtime.animation->root_positions();
            if(clearance_root_count>roots.size())throw MissionTickError("mission tick: changed animation root storage");
            const std::vector<PosePosition> active_roots(roots.begin(),roots.begin()+clearance_root_count);
            clearance = ground_clearance(active_roots, frame_mask);
            trace(report, MissionTraceKind::effect, kClearanceScan, ref,
                  unsigned_bits(clearance));
        } else if (runtime.animation != nullptr) {
            throw MissionTickError("mission tick: frame mask binding missing");
        }
        const std::int32_t px = motion.position[0];
        const std::int32_t py = motion.position[1];
        std::int32_t height = 0;
        if (state.world.terrain == nullptr)
            throw MissionTickError("mission tick: ground query needs the terrain map");
        if (actor.field_108 == 0) {
            height = ground_height_query(*state.world.terrain, px, py);
            runtime.ground_100 = 0;
            trace(report, MissionTraceKind::effect, kGroundHeightQuery, ref, unsigned_bits(px),
                  unsigned_bits(py), unsigned_bits(height));
        } else {
            const std::int32_t terrain_height = ground_height_query(*state.world.terrain, px, py);
            const std::array<std::int32_t, 3> point{px, py, wrap_add(motion.position[2], clearance)};
            std::vector<EntityCandidate> entities = target_entities(state,actor,point);
            std::vector<GroundEntity> view;
            view.reserve(entities.size());
            for (const auto& candidate : entities) view.push_back(candidate.entity);
            const GroundEntityResult result =
                ground_entity_height(terrain_height, point, 0x8000, view);
            height = result.height;
            runtime.ground_100 = entity_handle(entities,view,result.entity);
            trace(report, MissionTraceKind::effect, kGroundEntityQuery, ref, unsigned_bits(px),
                  unsigned_bits(py), unsigned_bits(height), runtime.ground_100);
        }
        GroundQuery query;
        query.height = height;
        query.water_level = state.world.water_level;
        query.clearance = clearance;
        query.full_pass = (tick & 3) == 3;
        MotionActor snap = motion;
        snap.on_ground_object = false;
        ground_update(snap, nullptr, query, StepVector{}, local);
        motion = snap;
    }

    // Store the actor record back.
    actor.position[0] = unsigned_bits(motion.position[0]);
    actor.position[1] = unsigned_bits(motion.position[1]);
    actor.position[2] = unsigned_bits(motion.position[2]);
    actor.angle_yaw = motion.yaw;
    runtime.vertical_44 = motion.vertical;
    store_actor_flags(actor, extra, runtime, motion.flags);

    // 0x4877DE: position copy to the record at actor +0x90.
    if (runtime.link_90 != 0) {
        if (state.world.linked_position) {
            state.world.linked_position(
                runtime.link_90,
                {signed_bits(actor.position[0]), signed_bits(actor.position[1]),
                 signed_bits(actor.position[2])});
        } else {
            throw MissionTickError("mission tick: linked position binding missing");
        }
    }
}

std::uint32_t mission_steady(const MissionTickState& state) {
    const auto weapon=state.weapons.slot_map()[std::size_t(state.weapons.slot())];
    if(weapon>=kWeaponCount)throw MissionTickError("mission tick: steady weapon outside the table");
    return (weapon_definitions()[weapon].flags&1)!=0 &&
        (state.globals.control_c23a00&0x41e)==0 && !state.weapons.aim_blocked()
        ?state.scope_c23a24:0;
}

// ---------------------------------------------------------------------------
// 0x488200 local player control join
// ---------------------------------------------------------------------------
void mission_local_control(MissionTickState& state, MissionTickReport& report) {
    if (!state.config.local_actor)
        throw MissionTickError("mission tick: 0x488200 needs the local actor (0xC23820)");
    const AiActorRef ref{3, *state.config.local_actor};
    WeaponEffectScope effects(state,&report);
    AiActorState* actor_pointer = state.actor_at(ref);
    if (actor_pointer == nullptr)
        throw MissionTickError("mission tick: local actor outside the group 3 table");
    AiActorState& actor = *actor_pointer;
    MissionActorRuntime& runtime = state.runtime_for(ref);

    trace(report, MissionTraceKind::stage, kLocalControl, ref);

    // 0x48820C: clear the live controller before weapon and pose work.
    runtime.condition_state_38 = 0;
    if(runtime.animation)runtime.animation->set_condition_state(0);

    std::uint32_t control = state.globals.control_c23a00;
    const std::uint32_t weapon_index =
        state.weapons.slot_map()[std::size_t(state.weapons.slot())];
    if (weapon_index >= kWeaponCount)
        throw MissionTickError("mission tick: weapon slot outside the weapon table");
    const std::uint32_t weapon_flags = weapon_definitions()[weapon_index].flags;

    // 0x48825A/0x48825F: control word fire requests.
    if ((control & 0x800) != 0) {
        const bool fired = state.weapons.fire();
        trace(report, MissionTraceKind::effect, kWeaponFire, ref, fired ? 1u : 0u);
        drain_weapon_events(state, report);
    }
    if ((control & 0x1000) != 0) {
        const bool fired = state.weapons.fire();
        trace(report, MissionTraceKind::effect, kWeaponFire, ref, fired ? 1u : 0u);
        drain_weapon_events(state, report);
    }
    if (state.config.input_disabled_67dbc8_bit1) {
        // 0x48826D: with 0x67DBC8 bit 2 set the body is skipped; the +0x7C
        // write still happens unless +0x6C bit 4 is set.
        if ((actor.field_6c & 4) == 0) actor.field_7c = 0;
        return;
    }

    if (runtime.animation == nullptr)
        throw MissionTickError("mission tick: 0x488200 needs the bound animation");

    // Clearance scan 0x4882E7..0x48843F: min transposed z over the clear bone
    // bits; x/y of the last processed bone feed the 0x486FD0 point.
    std::int32_t min_z = 0x7FFFFFF;
    std::int32_t point_x = 0;
    std::int32_t point_y = 0;
    if (!state.world.frame_mask) {
        throw MissionTickError("mission tick: frame mask binding missing");
    } else {
        const std::uint32_t mask = state.world.frame_mask(ref);
        const Fixed3 position{static_cast<std::int32_t>(actor.position[0]),
                              static_cast<std::int32_t>(actor.position[1]),
                              static_cast<std::int32_t>(actor.position[2])};
        // 0x4882A2: the actor pitch is zeroed for the pose matrix and restored.
        const Angle3 angles{actor.angle_yaw, 0, runtime.roll_38};
        const Matrix4 pose = compose_pose(position, angles);
        const auto& pose_list = runtime.animation->root_positions();
        for (std::size_t index = 0; index < runtime.animation->root_count(); ++index) {
            const unsigned bit = unsigned(clearance_bit_base + index) & 31u;
            if ((mask & (std::uint32_t(1) << bit)) != 0) continue;
            const std::int32_t candidate = product_shift(pose_list[index][1], clearance_scale, 16);
            const Fixed3 transformed = transform_point(pose, Fixed3{0, 0, candidate});
            if (transformed.z < min_z) min_z = transformed.z;
            point_x = transformed.x;
            point_y = transformed.y;
        }
    }

    // 0x48830D: 486FD0 with the transformed x/y and the min as z. The result
    // height is discarded by the original; the entity handle (+0x100) is not.
    if (state.world.terrain != nullptr) {
        const std::int32_t terrain_height =
            ground_height_query(*state.world.terrain, point_x, point_y);
        const std::array<std::int32_t, 3> point{point_x, point_y, min_z};
        std::vector<EntityCandidate> entities = target_entities(state,actor,point);
        std::vector<GroundEntity> view;
        view.reserve(entities.size());
        for (const auto& candidate : entities) view.push_back(candidate.entity);
        const GroundEntityResult result = ground_entity_height(terrain_height, point, 0x8000, view);
        runtime.ground_100 = entity_handle(entities,view,result.entity);
        trace(report, MissionTraceKind::effect, kGroundEntityQuery, ref, unsigned_bits(point_x),
              unsigned_bits(point_y), result.height, runtime.ground_100);
    } else {
        throw MissionTickError("mission tick: local ground binding missing");
    }

    // 0x48832A: apply the live foreign result before movement input reads.
    if(signed_bits(external_stage(state,report,0x4718D0))>2)
        state.globals.control_c23a00 &= 0xFFE1u;
    control=state.globals.control_c23a00;

    // 0x48833B: movement input clears the aim blocker.
    if ((control & 0x41E) != 0 && state.weapons.aim_blocked()) {
        state.weapons.toggle_aim_block();
        trace(report, MissionTraceKind::effect, kWeaponAim, ref, 0);
    }

    const bool steady=state.world.steady?state.world.steady():mission_steady(state)!=0;

    MovementActorState movement;
    movement.control = static_cast<std::uint16_t>(control & 0xFFFF);
    movement.z = static_cast<std::int32_t>(actor.position[2]);
    movement.vertical = runtime.vertical_44;
    movement.flags = actor_flags(actor, state.extra_for(ref), runtime);
    movement.has_ground_object = runtime.ground_100 != 0;
    movement.mode = state.movement.mode_c23824;
    movement.prone = state.movement.prone_c23828;
    movement.stance = state.weapons.fire_timer(); // Both read original C23830.
    movement.pulse_jump = state.movement.jump_pulse_c23a10;
    movement.pulse_prone = static_cast<std::uint8_t>(state.globals.pulse_c23a08);
    movement.pulse_crouch = static_cast<std::uint8_t>(state.globals.pulse_c23a04);
    movement.pulse_stand = static_cast<std::uint8_t>(state.globals.pulse_c23a0c);

    const std::int32_t animation =
        movement_animation(movement, static_cast<std::uint8_t>(weapon_flags), steady,
                           state.movement.jump_impulse_c2382c);

    state.movement.mode_c23824 = movement.mode;
    state.movement.prone_c23828 = movement.prone;
    state.movement.jump_pulse_c23a10 = movement.pulse_jump;
    state.globals.pulse_c23a08 = movement.pulse_prone;
    state.globals.pulse_c23a04 = movement.pulse_crouch;
    state.globals.pulse_c23a0c = movement.pulse_stand;
    actor.position[2] = unsigned_bits(movement.z);
    runtime.vertical_44 = movement.vertical;

    // 0x488379: look steps plus the pitch clamp (movement.hpp leaves).
    PlayerPose pose;
    pose.angles[0] = actor.angle_yaw;
    pose.angles[1] = actor.angle_pitch;
    const PlayerPose stepped =
        apply_look_step(pose, static_cast<std::uint8_t>(control & 0xFF),
                        static_cast<std::uint8_t>((control >> 8) & 0xFF));
    actor.angle_yaw = stepped.angles[0];
    actor.angle_pitch = stepped.angles[1];

    // 0x4883EA/0x48895B: +0x6C bit 4 gates the animation write only.
    if ((actor.field_6c & 4) == 0) actor.field_7c = unsigned_bits(animation);
    trace(report, MissionTraceKind::effect, kLocalControl, ref, unsigned_bits(animation));
}

// ---------------------------------------------------------------------------
// Weapon stages
// ---------------------------------------------------------------------------
namespace {

std::uint32_t apply_dispatch_call(MissionTickState& state, const DispatchCall& call,
                         MissionTickReport& report) {
    const AiActorRef ref{3, state.config.local_actor.value_or(0)};
    if(call.address==0x4847c0){state.scope_c23a24^=1;return 0;}
    switch (call.kind) {
        case DispatchCallKind::weapon_fire: {
            const bool fired = state.weapons.fire();
            trace(report, MissionTraceKind::effect, kWeaponFire, ref, fired ? 1u : 0u);
            drain_weapon_events(state, report);
            break;
        }
        case DispatchCallKind::weapon_reload: {
            const bool reloaded = state.weapons.reload_press();
            trace(report, MissionTraceKind::effect, kWeaponReload, ref, reloaded ? 1u : 0u);
            drain_weapon_events(state, report);
            break;
        }
        case DispatchCallKind::weapon_select: {
            const std::int32_t slot = call.arguments.empty() ? 0 : signed_bits(call.arguments[0]);
            const bool selected = state.weapons.select_slot(slot);
            trace(report, MissionTraceKind::effect, kWeaponSelect, ref, unsigned_bits(slot),
                  selected ? 1u : 0u);
            drain_weapon_events(state, report);
            break;
        }
        case DispatchCallKind::weapon_cycle: {
            const std::int32_t direction =
                call.arguments.empty() ? 0 : signed_bits(call.arguments[0]);
            const bool cycled = state.weapons.cycle_slot(direction);
            trace(report, MissionTraceKind::effect, kWeaponCycle, ref, unsigned_bits(direction),
                  cycled ? 1u : 0u);
            drain_weapon_events(state, report);
            break;
        }
        case DispatchCallKind::weapon_aim:
            external_stage(state,report,kWeaponAim);
            break;
        case DispatchCallKind::weapon_supply:
            if (call.address == kWeaponResupply) {
                state.weapons.resupply(state.config.double_ammo_c23a4c);
                trace(report, MissionTraceKind::effect, kWeaponResupply, ref);
                drain_weapon_events(state, report);
                return 0;
            }
            [[fallthrough]];
        default: {
            if(!state.world.dispatch_effect)throw MissionTickError("mission tick: dispatch effect binding missing");
            const auto result=state.world.dispatch_effect(state,call);
            if(!result)throw MissionTickError("mission tick: unbound dispatch effect "+std::to_string(call.address));
            return *result;
        }
    }
    return 0;
}

void sync_record_to_dispatch(MissionTickState& state) {
    if (!state.config.local_actor) return;
    const AiActorRef ref{3, *state.config.local_actor};
    const AiActorState* actor = state.actor_at(ref);
    if (actor == nullptr) return;
    const BehaviorActorExtra& extra = state.extra_for(ref);
    const MissionActorRuntime& runtime = state.runtime_for(ref);
    state.dispatch_actor.flags_6c = actor_flags(*actor, extra, runtime);
    state.dispatch_actor.look_yaw_30 = actor->angle_yaw;
    state.dispatch_actor.look_pitch_34 = actor->angle_pitch;
    state.dispatch_actor.stance_74 = extra.field_74;
}

void sync_dispatch_to_record(MissionTickState& state) {
    if (!state.config.local_actor) return;
    const AiActorRef ref{3, *state.config.local_actor};
    AiActorState* actor = state.actor_at(ref);
    if (actor == nullptr) return;
    actor->angle_yaw = state.dispatch_actor.look_yaw_30;
    actor->angle_pitch = state.dispatch_actor.look_pitch_34;
    state.extra_for(ref).field_74 = static_cast<std::uint16_t>(state.dispatch_actor.stance_74);
}

void run_command_pump(MissionTickState& state, const MissionTickInputs& inputs,
                      MissionTickReport& report) {
    trace_plain(report, MissionTraceKind::external_stage, kCommandPump);
    const auto apply_result=[&](const DispatchResult& result) {
        if(result.gated)return;
        sync_dispatch_to_record(state);
        std::optional<std::uint32_t> previous;
        for(DispatchCall call:result.calls) {
            if(call.threaded_result){
                if(!previous || call.arguments.empty())throw MissionTickError("mission tick: missing prior dispatch result");
                call.arguments[0]=*previous;
            }
            trace_plain(report,MissionTraceKind::dispatch_call,call.address,
                call.arguments.empty()?0:call.arguments[0],call.arguments.size()>1?call.arguments[1]:0);
            previous=apply_dispatch_call(state,call,report);
            sync_record_to_dispatch(state);
        }
    };
    if(inputs.next_command) {
        if(!inputs.dispatch_command)throw MissionTickError("mission tick: live input dispatcher missing");
        const auto tick=state.tick_67dbf4;
        while(const auto command=inputs.next_command(tick)){
            sync_record_to_dispatch(state);
            apply_result(inputs.dispatch_command(command->id,command->value));
        }
    } else if(inputs.dispatch_command) {
        for(const MissionCommandEvent& command:inputs.commands){
            sync_record_to_dispatch(state);
            apply_result(inputs.dispatch_command(command.id,command.value));
        }
    } else for(const auto& result:inputs.dispatch)apply_result(result);

    sync_dispatch_to_record(state);
}

void run_weapon_stage(MissionTickState& state, MissionTickReport& report) {
    // The network packet and throttle state require the full multiplayer body.
    if(state.globals.multiplayer_67f520){external_stage(state,report,kWeaponStage);return;}
    ++state.message_idle_bc5d20;
    while(external_stage(state,report,kMessageQueueCheck)!=0){
        external_stage(state,report,kMessageHandlers);
        state.message_idle_bc5d20=0;
    }
    external_stage(state,report,kWeaponPart);
    drain_weapon_events(state,report);
    if(state.globals.multiplayer_67f520)throw MissionTickError("mission tick: multiplayer entered during the single-player weapon stage");
    const std::uint32_t weapon_index =
        state.weapons.slot_map()[std::size_t(state.weapons.slot())];
    if (weapon_index == 0x1B)
        external_stage(state,report,kDetonatorCheck);
    if (state.globals.mission_67f528 == 0 && state.globals.menu_flag_67dbc0 != 0)
        external_stage(state,report,kMessagePush,{0x32,0,0});
    external_stage(state,report,kMessageFlush);
}

void run_projectile_pass(MissionTickState& state, MissionTickReport& report) {
    trace_plain(report, MissionTraceKind::stage, kProjectilePass);
    std::vector<ProjectileEvent> events;
    auto query=state.world.projectile_query;
    const auto sink=query.on_event;
    query.on_event=[&](ProjectileEntity& entity,const ProjectileEvent& event){
        if(!sink)throw MissionTickError("mission tick: projectile effect binding missing");
        sink(entity,event);
    };
    for (ProjectileEntity& entity : state.projectiles) {
        events.clear();
        tick_projectile(entity, state.world.world_center, query, events);
        ++report.projectile_ticks;
        for (const ProjectileEvent& event : events) {
            const std::uint32_t kind = static_cast<std::uint32_t>(event.kind);
            trace_plain(report, MissionTraceKind::projectile_event, kind, event.hit_type,
                        unsigned_bits(event.position.x));
            trace(report, MissionTraceKind::projectile_event, kind | 0x8000u, AiActorRef{},
                  unsigned_bits(event.position.y), unsigned_bits(event.position.z),
                  unsigned_bits(event.from.x), unsigned_bits(event.from.y));
        }
    }
}

void run_ai_stage(MissionTickState& state, MissionTickReport& report) {
    const std::uint32_t tick=state.tick_67dbf4;
    state.ai.paused=state.globals.pause_6e4714!=0;
    state.ai.demo=state.globals.pause2_6e4718!=0;
    state.ai.local_actor=state.config.local_actor;
    if(!state.world.bounds)
        throw MissionTickError("mission tick: AI bounds source missing (0x487C50)");
    trace_plain(report,MissionTraceKind::stage,kAiTick,tick);
    AiTickHooks hooks;
    hooks.on_mark=[&](std::uint32_t mark) {
        trace_plain(report,MissionTraceKind::mark,kProfilerMark,mark);
    };
    hooks.on_action=[&](const AiActorAction& action){run_actor_action(state,action,report);};
    hooks.on_clear=[&](const AiActorRef& ref){if(ref.group!=kAiExtraTableGroup)throw MissionTickError("mission tick: actor clear has an unowned table");clear_mission_extra_actor(state,ref.index);};
    hooks.on_call=[&](const AiTickCall& call) {
        switch(call.kind) {
        case AiTickCall::Kind::local_player_control:
            mission_local_control(state,report);break;
        case AiTickCall::Kind::per_actor_update:
            mission_actor_update(state,call.actor,tick,report);
            ++report.per_actor_updates;break;
        case AiTickCall::Kind::fixed_0045e050:
            external_stage(state,report,kFixed45E050);break;
        case AiTickCall::Kind::fixed_00464eb0:
            run_projectile_pass(state,report);break;
        case AiTickCall::Kind::fixed_00465bd0:
            external_stage(state,report,kPendingSpawnPass);break;
        }
    };
    const auto ai=tick_ai(state.tables,state.ai,state.definitions,state.world.bounds,tick,hooks);
    report.ai_actions=ai.actions;
    report.ai_marks=ai.profiler_marks;
}

void run_frame(MissionTickState& state,const MissionStageProvider& stages,MissionTickReport& report) {
    report.paused=state.globals.pause_6e4714!=0;
    report.demo=state.globals.pause2_6e4718!=0;
    report.tick=state.tick_67dbf4;
    const auto call=[&](std::uint32_t address,std::initializer_list<std::uint32_t> args={}) {
        MissionStageCall entry;entry.address=address;entry.argument_count=std::uint8_t(args.size());
        std::copy(args.begin(),args.end(),entry.args.begin());report.frame_calls.push_back(entry);
        if(address==kTimer43F560){
            ++state.globals.command_tick_6e471c;
            if(state.globals.pause_6e4714==0){++state.tick_6e4720;if(state.globals.pause2_6e4718==0)++state.tick_6e4724;}
            trace_plain(report,MissionTraceKind::effect,address,state.globals.command_tick_6e471c,state.tick_6e4720);
            return std::uint32_t(0);
        }
        if(address==kProfilerMark)trace_plain(report,MissionTraceKind::mark,address,entry.args[0]);
        if(!stages)throw MissionTickError("mission frame: stage provider missing");
        const auto result=stages(state,entry);
        if(!result)throw MissionTickError("mission frame: unbound stage "+std::to_string(address));
        return *result;
    };
    if(call(kEarlyStop)!=0){report.result=1;report.exit_reason=MissionFrameExit::early_stop;return;}
    call(kFrameEventRelease,{state.frame_event_67dbf0});state.frame_event_67dbf0=0;
    call(kTimer43F560);
    if(state.globals.mission_67f528 && !state.globals.pause_6e4714 && !state.globals.pause2_6e4718 && signed_bits(state.mission_timer_67dbe8)>0)--state.mission_timer_67dbe8;
    call(kProfilerMark,{6});++state.tick_67dbf4;report.tick=state.tick_67dbf4;
    call(kFrameTiming);call(kHudFrame,{state.tick_67dbf4});call(kHudTimers);call(kEffectPump);
    call(kProfilerMark,{7});call(kCommandPump,{state.tick_67dbf4});call(kProfilerMark,{8});
    call(kWeaponStage);call(kProfilerMark,{9});call(kAiTick,{state.tick_67dbf4});call(kProfilerMark,{0xd});
    call(kMissionEventProcessor);call(kCameraUpdate);++state.frame_counter_4ced10;call(kProfilerMark,{0xe});
    if(signed_bits(state.frame_counter_4ced10)>=0x3e){
        state.previous_work_67dbd0=state.frame_work_67dbcc;state.frame_work_67dbcc=0;
        state.clock_delta_67dbd8=state.clock_491014-state.last_clock_67dbd4;
        state.frame_counter_4ced10-=0x3e;state.last_clock_67dbd4=state.clock_491014;
        call(kSecondCounterBlock,{0x67dbdc});
        if(state.globals.multiplayer_67f520){call(0x48fbd0);call(0x489220);call(0x467260);}
    }
    if(state.globals.menu_state_67dbbc==4){
        if(!state.globals.multiplayer_67f520){call(kMenuHandler);return;}
        if(!state.globals.mission_67f528 && state.game_type_67f53c){
            call(kRequestState,{0x4cece0});report.result=1;report.exit_reason=MissionFrameExit::game_loop;return;
        }
    }
    if(state.shutdown_503424){call(kRequestState,{0});report.result=1;report.exit_reason=MissionFrameExit::shutdown;return;}
    if(!state.globals.menu_state_67dbbc)return;
    call(kRequestState,{0x4eb834});report.result=1;report.exit_reason=MissionFrameExit::post_menu;return;
}

} // namespace

void mission_weapon_part(MissionTickState& state,bool trigger_held_fixture) {
    WeaponEffectScope effects(state,nullptr,false);
    state.weapons.set_unlimited_ammo(state.config.pansy_c23dbc9_bit0);
    state.weapons.tick(trigger_held_fixture);
}

MissionTickReport mission_frame(MissionTickState& state,const MissionStageProvider& stages) {
    MissionTickReport report;run_frame(state,stages,report);return report;
}

MissionTickReport mission_tick(MissionTickState& state,const MissionTickInputs& inputs) {
    MissionTickReport report;
    WeaponEffectScope effects(state,&report);
    const auto stages=[&](MissionTickState& current,const MissionStageCall& call)->std::optional<std::uint32_t>{
        switch(call.address){
        case kCommandPump:sync_record_to_dispatch(current);run_command_pump(current,inputs,report);return 0;
        case kWeaponStage:run_weapon_stage(current,report);return 0;
        case kAiTick:run_ai_stage(current,report);return 0;
        default:return external_stage(current,report,call);
        }
    };
    run_frame(state,stages,report);
    return report;
}

} // namespace dfre
