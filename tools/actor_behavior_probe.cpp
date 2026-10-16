#include "actor_record_probe.hpp"
namespace {
using namespace dfre_probe;
// Row: callback rng host param2 raw actor_hex person_hex markers lists targets mres inputs
int run_case(const std::vector<std::string>& fields,bool live) {
    if (fields.size() != 12) throw std::runtime_error("case needs 12 fields");
    const std::string callback = fields[0];
    const std::uint32_t rng = number(fields[1]);
    const bool host = number(fields[2]) != 0;
    const std::uint32_t param2 = number(fields[3]);
    const std::uint32_t raw = hex_u32(fields[4]);
    Parsed state = parse_state(fields[5], fields[6]);
    dfre::BehaviorWorld world;
    if (fields[7] != "-") {
        for (const auto& entry : split(fields[7], ';')) {
            if (entry.empty()) continue;
            const auto v = split(entry, ',');
            if (v.size() != 6) throw std::runtime_error("bad marker");
            dfre::BehaviorMarker marker;
            marker.position[0] = static_cast<std::uint32_t>(signed_number(v[0]));
            marker.position[1] = static_cast<std::uint32_t>(signed_number(v[1]));
            marker.position[2] = static_cast<std::uint32_t>(signed_number(v[2]));
            marker.yaw = number(v[3]);
            marker.radius = number(v[4]);
            marker.object_id = number(v[5]);
            world.markers.push_back(marker);
        }
    }
    if (fields[8] != "-") {
        for (const auto& entry : split(fields[8], ';')) {
            if (entry.empty()) continue;
            const auto v = split(entry, ',');
            if (v.size() < 2) throw std::runtime_error("bad list");
            dfre::BehaviorMarkerList list;
            list.flags = number(v[0]);
            list.count = number(v[1]);
            for (std::size_t i = 2; i < v.size(); ++i) list.indices[i - 2] = number(v[i]);
            world.lists.push_back(list);
        }
    }
    std::map<std::uint32_t, dfre::BehaviorTargetView> targets;
    if (fields[9] != "-") {
        for (const auto& entry : split(fields[9], ';')) {
            if (entry.empty()) continue;
            const auto v = split(entry, ',');
            if (v.size() != 7) throw std::runtime_error("bad target");
            dfre::BehaviorTargetView target;
            target.field_70 = hex_u32(v[1]);
            target.field_6c = static_cast<std::uint8_t>(number(v[2]));
            target.field_f8 = hex_u32(v[3]);
            target.position[0] = static_cast<std::uint32_t>(signed_number(v[4]));
            target.position[1] = static_cast<std::uint32_t>(signed_number(v[5]));
            target.position[2] = static_cast<std::uint32_t>(signed_number(v[6]));
            targets[hex_u32(v[0])] = target;
        }
        if (!targets.empty()) {
            world.resolve_target = [targets](std::uint32_t id) {
                const auto found = targets.find(id);
                if (found == targets.end()) return std::optional<dfre::BehaviorTargetView>{};
                return std::optional<dfre::BehaviorTargetView>(found->second);
            };
        }
    }
    std::map<std::uint32_t, std::size_t> marker_resolution;
    if (fields[10] != "-") {
        for (const auto& entry : split(fields[10], ';')) {
            if (entry.empty()) continue;
            const auto v = split(entry, ',');
            if (v.size() != 2) throw std::runtime_error("bad mres");
            marker_resolution[hex_u32(v[0])] = static_cast<std::size_t>(number(v[1]));
        }
        if (!marker_resolution.empty()) {
            world.resolve_marker = [marker_resolution](std::uint32_t id) {
                const auto found = marker_resolution.find(id);
                if (found == marker_resolution.end()) return std::optional<std::size_t>{};
                return std::optional<std::size_t>(found->second);
            };
        }
    }
    dfre::BehaviorState behavior;
    behavior.rng = rng;
    behavior.host = host;
    if (fields[11] != "-") {
        for (const auto& entry : split(fields[11], ';')) {
            if (entry.empty()) continue;
            const auto v = split(entry, ',');
            if (v.size() != 3) throw std::runtime_error("bad input");
            behavior.inputs.push_back({hex_u32(v[0]), hex_u32(v[1]), hex_u32(v[2])});
        }
    }
    if(live) {
        const auto answers=behavior.inputs;
        behavior.inputs.clear();
        behavior.on_query=[&behavior,answers](const dfre::BehaviorAction& request) {
            if(behavior.cursor>=answers.size() || answers[behavior.cursor].site!=request.site)
                throw std::runtime_error("live query order differs");
            return answers[behavior.cursor];
        };
    }
    dfre::BehaviorActorRef ref{3, 0};
    dfre::BehaviorActor view{ref, raw, state.actor, state.person, state.actor_extra, state.person_extra};
    if (callback == "null") dfre::behavior_null(view);
    else if (callback == "org0") dfre::behavior_org0(view);
    else if (callback == "plyr") dfre::behavior_plyr(view, world, behavior);
    else if (callback == "org1") dfre::behavior_org1(view, world, behavior, param2);
    else throw std::runtime_error("unknown callback");

    const auto actor_image = dfre::behavior_actor_image(view);
    const auto person_image = dfre::behavior_person_image(view);
    std::string actions;
    for (const auto& action : behavior.actions) {
        if (!actions.empty()) actions += ';';
        actions += action.kind == dfre::BehaviorCallKind::query ? "q" : "e";
        actions += ',';
        char site[16];
        std::snprintf(site, sizeof(site), "%x", action.site);
        actions += site;
        for (auto arg : action.args) {
            actions += ',';
            actions += std::to_string(arg);
        }
    }
    std::cout << to_hex(actor_image.data(), actor_image.size()) << ' '
              << to_hex(person_image.data(), person_image.size()) << ' '
              << (actions.empty() ? "-" : actions) << ' '
              << behavior.cursor << ' ' << behavior.rng << '\n';
    return 0;
}
} // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 2 || (std::string(argv[1]) != "case" && std::string(argv[1]) != "live"))
            throw std::runtime_error("actor_behavior_probe case < cases.txt");
        std::string line;
        while (std::getline(std::cin, line)) {
            if (line.empty()) continue;
            std::vector<std::string> fields;
            std::stringstream stream(line);
            std::string field;
            while (stream >> field) fields.push_back(field);
            if (fields.empty()) continue;
            run_case(fields,std::string(argv[1])=="live");
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
