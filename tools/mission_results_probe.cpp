#include "dfre/assets.hpp"
#include "dfre/mission_results.hpp"

#include <cstdio>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {
std::uint32_t word(const dfre::AssetBytes& data, std::size_t at) {
    if (at + 4 > data.size()) throw dfre::AssetError("Word outside data");
    return std::uint32_t(data[at]) | (std::uint32_t(data[at + 1]) << 8) |
           (std::uint32_t(data[at + 2]) << 16) | (std::uint32_t(data[at + 3]) << 24);
}

std::string hex(std::uint32_t value) {
    std::ostringstream out;
    out << "0x" << std::hex << std::setw(4) << std::setfill('0') << value;
    return out.str();
}

dfre::AssetBytes read_file(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw dfre::AssetError("Cannot open " + path);
    return dfre::AssetBytes(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

struct MissionFile {
    std::string name;
    std::uint32_t prerequisites = 0;
    std::uint32_t equipment_flags = 0;
};

std::vector<MissionFile> campaign_missions(const dfre::AssetStore& assets, std::uint32_t campaign) {
    static const std::uint32_t counts[5] = {6, 6, 8, 10, 10};
    if (campaign < 1 || campaign > 5) throw dfre::AssetError("Campaign index is outside the original table");
    std::vector<MissionFile> out;
    char name[32];
    for (std::uint32_t mission = 0; mission < counts[campaign - 1]; ++mission) {
        std::snprintf(name, sizeof(name), "C%02uM%02u.BMS", campaign + 1, mission + 1);
        const auto bytes = assets.read(name).bytes;
        if (bytes.size() < 0x80 || bytes[0] != 'B' || bytes[1] != 'M' || bytes[2] != 'S')
            throw dfre::AssetError("Invalid BMS header in " + std::string(name));
        MissionFile file;
        file.name = name;
        file.equipment_flags = word(bytes, 0x78);
        file.prerequisites = word(bytes, 0x7c);
        out.push_back(std::move(file));
    }
    return out;
}

void write_profile(const std::string& path, const dfre::AssetBytes& profile) {
    std::ofstream output(path, std::ios::binary);
    if (!output) throw dfre::AssetError("Cannot write " + path);
    output.write(reinterpret_cast<const char*>(profile.data()), std::streamsize(profile.size()));
    if (!output) throw dfre::AssetError("Cannot write " + path);
}
}

int main(int argc, char** argv) {
    try {
        if (argc < 3) {
            std::cerr << "usage: mission_results_probe GAME_FOLDER PROFILE [--apply CAMPAIGN MISSION COMPLETED MASK [--out FILE]]\n"
                         "       completed: 0 or 1; mask: decimal or 0x hex; requires the whole campaign record area\n";
            return 2;
        }
        const dfre::AssetStore assets(argv[1]);
        auto profile = read_file(argv[2]);
        if (profile.size() != 0xa90 || profile[0] != 'P' || profile[1] != 'L' || profile[2] != 'Y')
            throw dfre::AssetError("Expected a 2704-byte PLY profile");

        std::uint32_t apply_campaign = 0, apply_mission = 0, apply_mask = 0;
        bool apply = false, completed = false;
        std::string out_path;
        for (int i = 3; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--apply" && i + 4 < argc) {
                apply = true;
                apply_campaign = std::stoul(argv[++i]);
                apply_mission = std::stoul(argv[++i]);
                completed = std::stoul(argv[++i]) != 0;
                apply_mask = std::stoul(argv[++i], nullptr, 0);
            } else if (arg == "--out" && i + 1 < argc) {
                out_path = argv[++i];
            } else {
                throw dfre::AssetError("Unknown argument " + arg);
            }
        }

        std::ostringstream json;
        json << "{";
        bool first = true;
        if (apply) {
            const auto update = dfre::apply_mission_result(profile, {apply_campaign, apply_mission, completed, apply_mask});
            json << "\"apply\":{\"campaign\":" << apply_campaign
                 << ",\"mission\":" << apply_mission
                 << ",\"completed\":" << (completed ? 1 : 0)
                 << ",\"objective_mask\":" << apply_mask
                 << ",\"completion_offset\":\"" << hex(dfre::CampaignProfile::completion_offset(apply_campaign, apply_mission)) << "\""
                 << ",\"completion_word\":" << update.completion_word
                 << ",\"completion_written\":" << (update.completion_written ? 1 : 0)
                 << ",\"mask_offset\":\"" << hex(dfre::CampaignProfile::mask_offset(apply_campaign)) << "\""
                 << ",\"campaign_mask\":" << update.campaign_mask
                 << ",\"mask_written\":" << (update.mask_written ? 1 : 0)
                 << ",\"debrief_won\":" << (dfre::debrief_won(profile, apply_campaign, apply_mission) ? 1 : 0)
                 << "}";
            first = false;
            if (!out_path.empty()) write_profile(out_path, profile);
        }

        const auto selected_campaign = word(profile, dfre::CampaignProfile::campaign_field);
        if (!first) json << ",";
        json << "\"selected_campaign\":" << selected_campaign;
        json << ",\"selected_mission\":" << (selected_campaign >= 1 && selected_campaign <= 5
                                                 ? word(profile, dfre::CampaignProfile::selected_mission + selected_campaign * 4)
                                                 : 0);
        json << ",\"campaigns\":[";
        for (std::uint32_t campaign = 1; campaign <= 5; ++campaign) {
            if (campaign > 1) json << ",";
            const auto missions = campaign_missions(assets, campaign);
            json << "{\"index\":" << campaign
                 << ",\"completion_base\":\"" << hex(dfre::CampaignProfile::completion_offset(campaign, 0)) << "\""
                 << ",\"mask_offset\":\"" << hex(dfre::CampaignProfile::mask_offset(campaign)) << "\""
                 << ",\"mask\":" << word(profile, dfre::CampaignProfile::mask_offset(campaign))
                 << ",\"complete\":" << (dfre::campaign_complete(profile, campaign, std::uint32_t(missions.size())) ? 1 : 0)
                 << ",\"missions\":[";
            for (std::size_t i = 0; i < missions.size(); ++i) {
                if (i) json << ",";
                const auto& mission = missions[i];
                const bool won = dfre::debrief_won(profile, campaign, std::uint32_t(i));
                const auto request = dfre::debrief_text_request(campaign, std::uint32_t(i), won);
                json << "{\"name\":\"" << mission.name << "\""
                     << ",\"prerequisites\":" << mission.prerequisites
                     << ",\"equipment_flags\":" << mission.equipment_flags
                     << ",\"completed_word\":" << word(profile, dfre::CampaignProfile::completion_offset(campaign, std::uint32_t(i)))
                     << ",\"available\":" << (dfre::mission_available(profile, campaign, std::uint32_t(i), mission.prerequisites) ? 1 : 0)
                     << ",\"debrief_file\":\"" << request.file << "\""
                     << ",\"debrief_group\":\"" << request.group << "\""
                     << ",\"debrief_key\":\"" << request.key << "\"}";
            }
            json << "]}";
        }
        json << "]}";
        std::cout << json.str() << "\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
