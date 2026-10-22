// projectiles_probe: scripted probe over the native projectile module.
// Reads a line script from stdin (or argv[1]) and prints one JSON line per
// step. tools/check_projectiles_original.py replays the same script against
// the original Df.exe code and compares the output.
//
// Commands (numbers accept 0x prefixes):
//   sincos ANGLE
//   rot AXIS FIRST SECOND
//   diag X Y Z
//   mul 12+12 matrix values
//   point 12 matrix values X Y Z
//   pose X Y Z YAW PITCH ROLL
//   muzzle ACTOR xyz + ypr + VIEW xyz + WEAPON xyz + AIM ypr + EYE
//   barrel MUZZLE xyz + AIM ypr + OFFSET xyz
//   plan WEAPON CLASS BURST SPREAD SEED YAW PITCH
//   record X Y Z YAW PITCH SLOT KIND GROUND
//   decode PACKEDX PACKEDY SLOT YAWWORD PITCHWORD KIND GROUND
//   spawn X Y Z YAW PITCH ROLL SPEED F10 F14 F18 FLAGS ITEM0 ITEM1 ITEMFIELD SLOT TEAMCOLOR GBIT
//   entity X Y Z VX VY VZ TTL FLAGS SPECIALB ALTMOVE
//   world X Y Z
//   tick N            (followed by N lines: q TERRAIN A B S X)
#include "dfre/projectiles.hpp"
#include <array>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
namespace {
std::istringstream* current_input = nullptr;
std::int64_t num() {
    std::string token;
    (*current_input) >> token;
    return std::stoll(token, nullptr, 0);
}
void dump_matrix(const dfre::Matrix4& matrix) {
    std::cout << "\"m\":[";
    for (std::size_t i = 0; i < matrix.m.size(); ++i) std::cout << (i ? "," : "") << matrix.m[i];
    std::cout << "]";
}
dfre::Matrix4 read_matrix() {
    dfre::Matrix4 matrix;
    for (auto& value : matrix.m) value = std::int32_t(num());
    return matrix;
}
std::string event_list(std::vector<dfre::ProjectileEvent> events) {
    std::ostringstream out;
    out << "[";
    bool first = true;
    for (const auto& event : events) {
        if (!first) out << ",";
        first = false;
        const char* kind = "?";
        switch (event.kind) {
            case dfre::ProjectileEventKind::move: kind = "move"; break;
            case dfre::ProjectileEventKind::world_edge: kind = "edge"; break;
            case dfre::ProjectileEventKind::impact: kind = "impact"; break;
        }
        out << "[\"" << kind << "\"," << int(event.hit_type) << "," << event.position.x << ","
            << event.position.y << "," << event.position.z << "," << event.from.x << "," << event.from.y
            << "," << event.from.z << "]";
    }
    out << "]";
    return out.str();
}
void dump_entity(const char* op, const dfre::ProjectileEntity& entity,
                 std::vector<dfre::ProjectileEvent> events) {
    std::cout << "{\"op\":\"" << op << "\",\"x\":" << entity.position.x << ",\"y\":" << entity.position.y
              << ",\"z\":" << entity.position.z << ",\"vx\":" << entity.velocity.x
              << ",\"vy\":" << entity.velocity.y << ",\"vz\":" << entity.velocity.z
              << ",\"ttl\":" << entity.ttl << ",\"item\":" << entity.item_main
              << ",\"color\":" << std::int32_t(entity.color) << ",\"events\":" << event_list(events)
              << "}\n";
}
}
int main(int argc, char** argv) {
    std::istream* input = &std::cin;
    std::ifstream file;
    if (argc > 1) {
        file.open(argv[1]);
        if (!file) { std::cerr << "cannot open script\n"; return 1; }
        input = &file;
    }
    dfre::ProjectileEntity entity;
    dfre::Fixed3 world{};
    dfre::ProjectileQuery query;
    std::string line;
    while (std::getline(*input, line)) {
        std::istringstream words(line);
        current_input = &words;
        std::string command;
        if (!(words >> command) || command.empty() || command[0] == '#') continue;
        if (command == "sine_table") {
            std::cout<<"{\"op\":\"sine_table\",\"values\":[";
            for(std::size_t i=0;i<dfre::kSineTableQ22.size();++i){if(i)std::cout<<',';std::cout<<dfre::kSineTableQ22[i];}
            std::cout<<"]}\n";
        } else if (command == "sincos") {
            const std::uint32_t angle = std::uint32_t(num());
            std::int32_t sin = 0, cos = 0;
            dfre::sincos_q22(angle, sin, cos);
            std::cout << "{\"op\":\"sincos\",\"sin\":" << sin << ",\"cos\":" << cos << "}\n";
        } else if (command == "rot") {
            const std::uint32_t axis = std::uint32_t(num());
            const std::int32_t first = std::int32_t(num()), second = std::int32_t(num());
            std::cout << "{\"op\":\"rot\",";
            dump_matrix(dfre::matrix_rotation(axis, first, second));
            std::cout << "}\n";
        } else if (command == "diag") {
            dfre::Fixed3 value;
            value.x = std::int32_t(num()); value.y = std::int32_t(num()); value.z = std::int32_t(num());
            std::cout << "{\"op\":\"diag\",";
            dump_matrix(dfre::matrix_translation(value));
            std::cout << "}\n";
        } else if (command == "mul") {
            const dfre::Matrix4 left = read_matrix(), right = read_matrix();
            std::cout << "{\"op\":\"mul\",";
            dump_matrix(dfre::matrix_multiply(left, right));
            std::cout << "}\n";
        } else if (command == "point") {
            const dfre::Matrix4 matrix = read_matrix();
            dfre::Fixed3 point;
            point.x = std::int32_t(num()); point.y = std::int32_t(num()); point.z = std::int32_t(num());
            const dfre::Fixed3 out = dfre::transform_point(matrix, point);
            std::cout << "{\"op\":\"point\",\"x\":" << out.x << ",\"y\":" << out.y << ",\"z\":" << out.z << "}\n";
        } else if (command == "pose") {
            dfre::Fixed3 position; dfre::Angle3 angles;
            position.x = std::int32_t(num()); position.y = std::int32_t(num()); position.z = std::int32_t(num());
            angles.yaw = std::uint32_t(num()); angles.pitch = std::uint32_t(num()); angles.roll = std::uint32_t(num());
            std::cout << "{\"op\":\"pose\",";
            dump_matrix(dfre::compose_pose(position, angles));
            std::cout << "}\n";
        } else if (command == "muzzle") {
            dfre::MuzzleInputs in;
            in.actor_position.x = std::int32_t(num()); in.actor_position.y = std::int32_t(num());
            in.actor_position.z = std::int32_t(num());
            in.actor_angles.yaw = std::uint32_t(num()); in.actor_angles.pitch = std::uint32_t(num());
            in.actor_angles.roll = std::uint32_t(num());
            in.view_delta.x = std::int32_t(num()); in.view_delta.y = std::int32_t(num());
            in.view_delta.z = std::int32_t(num());
            in.weapon_muzzle.x = std::int32_t(num()); in.weapon_muzzle.y = std::int32_t(num());
            in.weapon_muzzle.z = std::int32_t(num());
            in.weapon_aim.yaw = std::uint32_t(num()); in.weapon_aim.pitch = std::uint32_t(num());
            in.weapon_aim.roll = std::uint32_t(num());
            in.eye_height = std::int32_t(num());
            const auto out = dfre::compute_muzzle(in);
            std::cout << "{\"op\":\"muzzle\",\"x\":" << out.origin.x << ",\"y\":" << out.origin.y
                      << ",\"z\":" << out.origin.z << ",\"yaw\":" << out.aim.yaw << ",\"pitch\":"
                      << out.aim.pitch << ",\"roll\":" << out.aim.roll << "}\n";
        } else if (command == "barrel") {
            dfre::Fixed3 muzzle, offset; dfre::Angle3 aim;
            muzzle.x = std::int32_t(num()); muzzle.y = std::int32_t(num()); muzzle.z = std::int32_t(num());
            aim.yaw = std::uint32_t(num()); aim.pitch = std::uint32_t(num()); aim.roll = std::uint32_t(num());
            offset.x = std::int32_t(num()); offset.y = std::int32_t(num()); offset.z = std::int32_t(num());
            const auto out = dfre::compute_barrel_tip(muzzle, aim, offset);
            std::cout << "{\"op\":\"barrel\",\"x\":" << out.x << ",\"y\":" << out.y << ",\"z\":" << out.z << "}\n";
        } else if (command == "plan") {
            const std::uint32_t weapon = std::uint32_t(num());
            const int weapon_class = std::int32_t(num());
            const std::int32_t burst = std::int32_t(num()), spread = std::int32_t(num());
            const std::uint32_t seed = std::uint32_t(num());
            dfre::Angle3 aim;
            aim.yaw = std::uint32_t(num()); aim.pitch = std::uint32_t(num());
            dfre::WeaponRandom rng; rng.seed(seed);
            const auto plan = dfre::plan_spawn(weapon, std::uint8_t(weapon_class), burst, spread, rng, aim);
            std::cout << "{\"op\":\"plan\",\"kind\":" << int(plan.kind) << ",\"spawns\":"
                      << (plan.spawns ? "true" : "false") << ",\"melee\":" << (plan.melee ? "true" : "false")
                      << ",\"burst\":" << plan.burst_after << ",\"yaw\":" << aim.yaw << ",\"pitch\":"
                      << aim.pitch << ",\"rng\":" << rng.state << "}\n";
        } else if (command == "record") {
            dfre::Fixed3 origin; dfre::Angle3 aim;
            origin.x = std::int32_t(num()); origin.y = std::int32_t(num()); origin.z = std::int32_t(num());
            aim.yaw = std::uint32_t(num()); aim.pitch = std::uint32_t(num());
            const int slot = std::int32_t(num()), kind = std::int32_t(num()), ground = std::int32_t(num());
            const auto record = dfre::build_spawn_record(
                origin, aim, std::uint16_t(slot), std::uint8_t(kind),
                [ground](std::int32_t, std::int32_t) { return ground; });
            std::cout << "{\"op\":\"record\",\"px\":" << std::int32_t(record.packed_x) << ",\"py\":"
                      << std::int32_t(record.packed_y) << ",\"slot\":" << record.actor_slot << ",\"yaw\":"
                      << record.aim_yaw_word << ",\"pitch\":" << record.aim_pitch_word << ",\"kind\":"
                      << int(record.kind) << "}\n";
        } else if (command == "decode") {
            dfre::SpawnRecord record;
            record.packed_x = std::uint32_t(num()); record.packed_y = std::uint32_t(num());
            const int slot = std::int32_t(num()), yaw = std::int32_t(num()), pitch = std::int32_t(num());
            const int kind = std::int32_t(num()), ground = std::int32_t(num());
            record.actor_slot = std::uint16_t(slot);
            record.aim_yaw_word = std::uint16_t(yaw);
            record.aim_pitch_word = std::uint16_t(pitch);
            record.kind = std::uint8_t(kind);
            const auto out = dfre::decode_spawn_record(
                record, [ground](std::int32_t, std::int32_t) { return ground; });
            std::cout << "{\"op\":\"decode\",\"x\":" << out.origin.x << ",\"y\":" << out.origin.y
                      << ",\"z\":" << out.origin.z << ",\"yaw\":" << out.aim.yaw << ",\"pitch\":"
                      << out.aim.pitch << "}\n";
        } else if (command == "spawn") {
            dfre::Fixed3 origin; dfre::Angle3 aim; dfre::ClassSpawnFields fields;
            std::array<std::int32_t, 2> items{};
            origin.x = std::int32_t(num()); origin.y = std::int32_t(num()); origin.z = std::int32_t(num());
            aim.yaw = std::uint32_t(num()); aim.pitch = std::uint32_t(num()); aim.roll = std::uint32_t(num());
            fields.speed = std::uint32_t(num()); fields.field_10 = std::uint32_t(num());
            fields.field_14 = std::uint32_t(num()); fields.field_18 = std::uint32_t(num());
            fields.flags = std::uint32_t(num());
            items[0] = std::int32_t(num()); items[1] = std::int32_t(num());
            const int item_field = std::int32_t(num()), slot = std::int32_t(num());
            const std::uint32_t team_color = std::uint32_t(num());
            const bool gbit = num() != 0;
            const auto spawned = dfre::spawn_projectile(origin, aim, fields, items, item_field, slot,
                                                        team_color, gbit);
            std::cout << "{\"op\":\"spawn\",\"x\":" << spawned.position.x << ",\"y\":" << spawned.position.y
                      << ",\"z\":" << spawned.position.z << ",\"yaw\":" << spawned.aim_yaw << ",\"pitch\":"
                      << spawned.aim_pitch << ",\"vx\":" << spawned.velocity.x << ",\"vy\":"
                      << spawned.velocity.y << ",\"vz\":" << spawned.velocity.z << ",\"ttl\":"
                      << spawned.ttl << ",\"item0\":" << spawned.item_main << ",\"item1\":"
                      << spawned.item_alt << ",\"itemfield\":" << spawned.item_field << ",\"color\":"
                      << std::int32_t(spawned.color) << ",\"flags\":" << std::int32_t(spawned.class_flags)
                      << ",\"f7c\":" << std::int32_t(spawned.field_7c) << ",\"f80\":"
                      << std::int32_t(spawned.field_80) << ",\"f84\":" << std::int32_t(spawned.field_84)
                      << "}\n";
        } else if (command == "entity") {
            entity.position.x = std::int32_t(num()); entity.position.y = std::int32_t(num());
            entity.position.z = std::int32_t(num());
            entity.velocity.x = std::int32_t(num()); entity.velocity.y = std::int32_t(num());
            entity.velocity.z = std::int32_t(num());
            entity.ttl = std::int32_t(num()); entity.class_flags = std::uint32_t(num());
            query.special_b_enabled = num() != 0;
            query.alternate_move_filter = num() != 0;
            entity.item_main = 0x1000;
        } else if (command == "world") {
            world.x = std::int32_t(num()); world.y = std::int32_t(num()); world.z = std::int32_t(num());
        } else if (command == "tick") {
            const int count = std::int32_t(num());
            for (int step = 0; step < count; ++step) {
                std::string qline;
                if (!std::getline(*input, qline)) break;
                std::istringstream q(qline);
                current_input = &q;
                std::string qop, terrain, a, b, s, x;
                q >> qop >> terrain >> a >> b >> s >> x;
                unsigned action=0;q>>action;
                bool late_filter=query.alternate_move_filter;
                query.use_alternate_move_filter=[&]{return late_filter;};
                query.current_world_center=[&]{return world;};
                query.on_event=[&](auto& p,const auto& e){
                    if(e.kind==dfre::ProjectileEventKind::impact){
                        if(action==1)late_filter=true;
                        if(action==2)p.position={123,456,789};
                    }
                    if(e.kind==dfre::ProjectileEventKind::move && action==3){world={0,0,0};p.position={0,0,0};}
                };
                const auto parse = [](const std::string& token, std::int32_t&,
                                      bool& present) noexcept {
                    if (token == "none") { present = false; return std::int32_t(0); }
                    present = true;
                    return std::int32_t(std::stoll(token, nullptr, 0));
                };
                std::int32_t distance = 0; bool present = false;
                const std::int32_t terrain_raw = std::int32_t(std::stoll(terrain, nullptr, 0));
                query.terrain = [&,terrain_raw](const dfre::ProjectileEntity&, const dfre::Fixed3&,
                                              std::int32_t, std::int32_t&) {
                    if(action==4){entity.position={123,456,789};entity.velocity={1000,2000,3000};}
                    return terrain_raw;
                };
                const std::int32_t araw = parse(a, distance, present);
                query.actors_a = [araw, present](const dfre::ProjectileEntity&, const dfre::Fixed3&,
                                                           std::int32_t, std::int32_t& out) {
                    if (!present) return std::int32_t(1);
                    out = araw;
                    return std::int32_t(0);
                };
                const std::int32_t braw = parse(b, distance, present);
                query.actors_b = [braw, present](const dfre::ProjectileEntity&, const dfre::Fixed3&,
                                                           std::int32_t, std::int32_t& out) {
                    if (!present) return std::int32_t(1);
                    out = braw;
                    return std::int32_t(0);
                };
                const std::int32_t sraw = parse(s, distance, present);
                query.special_a = [sraw, present](const dfre::ProjectileEntity&, const dfre::Fixed3&,
                                                            std::int32_t, std::int32_t& out) {
                    if (!present) return std::int32_t(1);
                    out = sraw;
                    return std::int32_t(0);
                };
                const std::int32_t xraw = parse(x, distance, present);
                query.special_b = [xraw, present](const dfre::ProjectileEntity&, const dfre::Fixed3&,
                                                            std::int32_t, std::int32_t& out) {
                    if (!present) return std::int32_t(1);
                    out = xraw;
                    return std::int32_t(0);
                };
                std::vector<dfre::ProjectileEvent> events;
                dfre::tick_projectile(entity, world, query, events);
                std::ostringstream name;
                name << "tick" << step;
                dump_entity(name.str().c_str(), entity, events);
            }
            current_input = &words;
        } else {
            std::cerr << "unknown command: " << command << "\n";
            return 1;
        }
    }
    return 0;
}
