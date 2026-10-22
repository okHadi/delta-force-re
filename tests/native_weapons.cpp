// Native weapon state checks. The expected numbers come from
// tools/check_weapons_original.py, which replays the same scenarios on the
// original Df.exe code (SHA-256
// abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff).
#include "dfre/weapons.hpp"
#include <iostream>
#include <stdexcept>
namespace {
void require(bool yes, const char* message) { if (!yes) throw std::runtime_error(message); }
const std::array<std::uint8_t, 10> kMainMap{1, 4, 6, 10, 12, 15, 28, 29, 33, 37}; // M4 burst at slot 2
const std::array<std::uint8_t, 10> kSingleMap{1, 4, 7, 10, 12, 15, 28, 29, 33, 37}; // M4 single at slot 2
dfre::WeaponState make(const std::array<std::uint8_t, 10>& map, bool doubled = false) {
    dfre::WeaponState state;
    state.set_slot_map(map);
    state.resupply(doubled);
    return state;
}
}
int main() { try {
    // Static tables and table-driven definitions.
    const auto& weapons = dfre::weapon_definitions();
    const auto& classes = dfre::weapon_classes();
    require(weapons.size() == 39 && classes.size() == 28, "table sizes");
    require(weapons[6].primary_item == 4057 && weapons[6].use_class == 1 && weapons[6].weapon_class == 7, "M4 record");
    require(weapons[6].fire_delay == 6 && weapons[7].fire_delay == 10, "M4 burst/single delays");
    require(weapons[12].weapon_class == 14 && (weapons[12].flags & 0x20) != 0, "M249 auto flag");
    require(classes[7].magazine_size == 30 && classes[14].magazine_size == 200, "magazine sizes");

    {
        auto live=make(kMainMap);live.select_slot(2);live.take_events();
        unsigned shots=0;
        live.on_event=[&](const dfre::WeaponEvent& event){
            require(event.kind==dfre::WeaponEventKind::shot && live.pending(7)==1 && live.fire_timer()==0,
                "shot effect runs before pending and timer writes");
            ++shots;live.set_unlimited_ammo(true);
        };
        require(live.fire() && shots==1 && live.pending(7)==0 && live.fire_timer()==6,"live shot callback");
        live.on_event={};
        for(unsigned tick=0;tick<6;++tick)live.tick(false);
        require(live.magazine(7)==30,"later weapon logic reads effect state");
    }

    {
        dfre::WeaponState live;std::array<std::uint8_t,10> slots{};slots.fill(27);
        live.set_slot_map(slots);live.resupply(false);live.restore_timing({1,0,0});
        live.on_event=[&](const dfre::WeaponEvent& event){
            require(event.kind==dfre::WeaponEventKind::queue && event.value==0x2c,"laser expiry queue");
            slots.fill(28);live.set_slot_map(slots);
        };
        live.tick(false);
        require(live.current_weapon()==28 && live.pending(0)==1 && live.pending(27)==0,
            "laser expiry writes the class cached before the queue effect");
    }

    {
        dfre::WeaponState selected;
        selected.loadout({6,9,21});
        require(selected.slot_map()==std::array<std::uint8_t,10>{27,1,5,6,10,7,29,30,0,33},
            "loadout recipe slots");
        selected.select_slot(3);
        require(selected.current_weapon()==6,"loadout selected weapon");
        selected.loadout({6,9,1});
        require(selected.reserve(7)==std::int32_t(classes[7].magazine_count)*2-1,"ammo box doubles primary reserve");
        selected.loadout({6,9,2});
        require(selected.reserve(7)==std::int32_t(classes[7].magazine_count)-1,"secondary id must equal one to double reserve");
        try { selected.loadout({6,28,1});require(false,"invalid loadout id accepted"); }
        catch(const std::out_of_range&) {}
    }

    // 4848C0 resupply: reserve = spare magazines - 1, magazine = size, pending = 1.
    auto state = make(kMainMap);
    require(state.magazine(7) == 30 && state.reserve(7) == 11 && state.pending(7) == 1, "M4 resupply");
    require(state.magazine(14) == 200 && state.reserve(14) == 2, "M249 resupply");
    require(state.reserve(0) == -1, "melee reserve is -1");
    auto doubled = make(kMainMap, true);
    require(doubled.reserve(14) == 5 && doubled.reserve(15) == 7, "double ammo slots 3..5");
    require(doubled.reserve(7) == 11, "double ammo leaves other slots");

    // 483DD0 slot select resets timers and arms empty weapons.
    state = make(kMainMap);
    require(state.select_slot(2) && state.slot() == 2 && state.current_weapon() == 6, "slot select");
    require(!state.select_slot(10) && state.slot() == 2, "invalid slot ignored");

    // 483C70 fire: burst weapon 6 fires once per press and schedules the
    // 45EA00 burst counter; the 482B00 burst gate fires the two follow-ups.
    state.select_slot(2);
    require(state.fire() && state.fire_timer() == 6 && state.burst_counter() == 2, "burst press");
    require(state.take_events().size() == 1, "burst press event");
    for (int i = 0; i < 5; ++i) state.tick(false);
    require(state.fire_timer() == 1 && state.magazine(7) == 30, "burst cooldown");
    state.tick(false); // timer expiry: -1 round, follow-up shot
    require(state.fire_timer() == 6 && state.magazine(7) == 29 && state.burst_counter() == 1, "burst follow-up one");
    auto events = state.take_events();
    require(events.size() == 1 && events[0].kind == dfre::WeaponEventKind::shot && events[0].value == 6, "burst follow-up event");
    for (int i = 0; i < 6; ++i) state.tick(false);
    require(state.magazine(7) == 28 && state.burst_counter() == 0, "burst follow-up two");
    state.take_events();
    for (int i = 0; i < 6; ++i) state.tick(false);
    require(state.magazine(7) == 27 && state.burst_counter() == 0, "burst ends");
    require(state.take_events().empty(), "no fourth shot");

    // M249 keeps firing while the trigger stays held (+0xA4 bit 0x20).
    state = make(kMainMap);
    state.select_slot(4);
    require(state.fire() && state.fire_timer() == 6, "M249 press");
    state.take_events();
    for (int i = 0; i < 6; ++i) state.tick(true);
    require(state.magazine(14) == 199 && state.fire_timer() == 6, "M249 auto refire");
    events = state.take_events();
    require(events.size() == 1 && events[0].kind == dfre::WeaponEventKind::shot, "M249 refire event");
    for (int i = 0; i < 6; ++i) state.tick(false);
    require(state.magazine(14) == 198 && state.fire_timer() == 0, "M249 released stops");

    {
        auto live=make(kMainMap);live.select_slot(4);live.fire();live.take_events();
        unsigned queries=0;
        live.trigger_query=[&]{++queries;live.toggle_aim_block();return true;};
        for(unsigned tick=0;tick<6;++tick)live.tick(true);
        require(queries==1 && live.aim_blocked() && live.fire_timer()==0,
            "gun expiry reads current aim state after its input query");
        require(live.take_events().empty(),"input query blocked the automatic shot");
    }

    // 4832F0 / 483390 reload: 120 ticks, one spare magazine, action sound.
    state = make(kSingleMap);
    state.select_slot(2);
    state.fire();
    state.take_events();
    for (int i = 0; i < 10; ++i) state.tick(false); // one shot consumed at expiry
    require(state.magazine(7) == 29, "single click consumes one");
    state.fire();
    state.take_events();
    for (int i = 0; i < 10; ++i) state.tick(false);
    require(state.magazine(7) == 28, "second single click consumes");
    require(state.reload_press() && state.reload_timer() == 120 && state.magazine(7) == 0, "reload start");
    events = state.take_events();
    require(events.size() == 1 && events[0].kind == dfre::WeaponEventKind::sound && events[0].value == 83, "reload sound");
    for (int i = 0; i < 120; ++i) state.tick(false);
    require(state.reload_timer() == 0 && state.magazine(7) == 30 && state.reserve(7) == 10, "reload complete");

    // Reload with a full magazine does nothing.
    state = make(kMainMap);
    state.select_slot(2);
    require(state.reload_press() && state.reload_timer() == 0, "full magazine reload ignored");

    // Empty magazine with no spare: dry fire sound only.
    state = make(kMainMap);
    state.select_slot(2);
    state.fire();
    for (int i = 0; i < 6; ++i) state.tick(false); // shoots 2nd burst round
    state.take_events();
    for (int i = 0; i < 6; ++i) state.tick(false); // shoots 3rd burst round
    state.take_events();

    // Aim blocker (484C20) stops 483C70 until toggled back.
    state = make(kMainMap);
    state.select_slot(2);
    state.toggle_aim_block();
    require(state.aim_blocked() && !state.fire() && state.fire_timer() == 0, "aim blocks fire");
    state.toggle_aim_block();
    require(!state.aim_blocked() && state.fire(), "aim toggle releases");

    // Cycle skips empty slots and wraps.
    dfre::WeaponState gaps;
    gaps.set_slot_map({0, 0, 6, 0, 0, 0, 0, 0, 0, 0});
    gaps.resupply(false);
    gaps.select_slot(2);
    require(gaps.cycle_slot(1) && gaps.slot() == 2, "cycle reselects the only slot");
    require(gaps.cycle_slot(-1) && gaps.slot() == 2, "cycle backward wraps");

    // Gun switch to an empty magazine starts the reload (483DD0 -> 4832F0).
    state = make(kSingleMap);
    state.select_slot(2);
    while (state.magazine(7) > 1) { // drain until the auto reload at empty
        require(state.fire(), "drain press");
        state.take_events();
        for (int i = 0; i < 10; ++i) state.tick(false);
        state.take_events();
    }
    require(state.fire(), "last round press");
    state.take_events();
    for (int i = 0; i < 10; ++i) state.tick(false); // expiry empties and reloads
    state.take_events();
    require(state.magazine(7) == 0 && state.reload_timer() > 0, "auto reload at empty");
    state.select_slot(1); // switching cancels the reload timers
    require(state.reload_timer() == 0, "switch cancels reload");
    state.select_slot(2); // empty magazine starts the reload again
    require(state.magazine(7) == 0 && state.reload_timer() == 120, "switch reload");
    state.take_events();

    // Unlimited ammo flag skips magazine cost but keeps pending bookkeeping.
    state = make(kMainMap);
    state.set_unlimited_ammo(true);
    state.select_slot(2);
    state.fire();
    for (int i = 0; i < 6; ++i) state.tick(true);
    require(state.magazine(7) == 30, "unlimited keeps magazine");

    // Grenade: 6 throws including the last one fired at zero reserve.
    state = make(kMainMap);
    state.select_slot(8);
    require(state.reserve(12) == 5, "grenade reserve");
    for (int throw_index = 0; throw_index < 6; ++throw_index) {
        require(state.fire(), "grenade throw");
        state.take_events();
        for (int i = 0; i < 90; ++i) state.tick(false);
        events = state.take_events();
    }
    require(state.reserve(12) == 0, "grenade reserve drained");
    state.fire(); // dry click, no spawn
    events = state.take_events();
    require(events.size() == 1 && events[0].kind == dfre::WeaponEventKind::sound && events[0].value == 0, "grenade empty click");
    state.fire();
    events = state.take_events();
    require(events.size() == 1 && events[0].kind == dfre::WeaponEventKind::sound && events[0].value == 0, "grenade stays dry");

    // Claymore placement uses item 0xFAE for weapon 37 and 0xFC8 for
    // weapon 38 (484150 in 4840B0).
    state = make(kMainMap);
    state.select_slot(9);
    require(state.fire(), "claymore place");
    events = state.take_events();
    require(events.size() == 2 && events[0].kind == dfre::WeaponEventKind::place && events[0].value == 0xFAE, "claymore motion item");
    require(events[1].kind == dfre::WeaponEventKind::sound, "claymore sound");
    dfre::WeaponState claymore38;
    claymore38.set_slot_map({1, 4, 6, 10, 12, 15, 28, 29, 33, 38});
    claymore38.resupply(false);
    claymore38.select_slot(9);
    claymore38.take_events();
    claymore38.fire();
    events = claymore38.take_events();
    require(events.size() == 2 && events[0].kind == dfre::WeaponEventKind::place && events[0].value == 0xFC8, "claymore entity item");

    // Detonator (470F40) queues message 0x28 with no other state change.
    dfre::WeaponState queue_state;
    queue_state.set_slot_map({1, 4, 6, 10, 12, 15, 28, 29, 30, 37});
    queue_state.resupply(false);
    queue_state.select_slot(8);
    queue_state.take_events();
    require(queue_state.fire(), "detonator use");
    events = queue_state.take_events();
    require(events.size() == 1 && events[0].kind == dfre::WeaponEventKind::queue && events[0].value == 0x28, "detonator queue");

    // Laser: asset + message + delay, and the 483AA0 expiry queues 0x2C.
    dfre::WeaponState laser;
    laser.set_slot_map({1, 27, 6, 10, 12, 15, 28, 29, 33, 37});
    laser.resupply(false);
    laser.select_slot(1);
    laser.take_events();
    require(laser.fire(), "laser use");
    events = laser.take_events();
    require(events.size() == 2 && events[0].kind == dfre::WeaponEventKind::sound2 && events[0].value == 0x1AE, "laser asset");
    require(events[1].kind == dfre::WeaponEventKind::message, "laser message");
    require(laser.fire_timer() == 180, "laser delay");

    // Knife: 70-tick cadence, no ammo use, spawn event only.
    state = make(kMainMap);
    state.select_slot(0);
    require(state.fire() && state.fire_timer() == 70 && state.burst_counter() == 0, "knife swing");
    events = state.take_events();
    require(events.size() == 1 && events[0].kind == dfre::WeaponEventKind::shot, "knife event");
    for (int i = 0; i < 70; ++i) state.tick(false);
    require(state.fire_timer() == 0, "knife cadence");

    // Raise counter decays by two per tick and floors at zero (C23A3C).
    require(state.raise_counter() == 0, "raise idle");

    std::cout << "weapons checks passed\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
} }
