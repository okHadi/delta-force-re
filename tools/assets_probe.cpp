#include "dfre/assets.hpp"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <set>
#include <sstream>

namespace {
std::string json(std::string_view value) {
    std::ostringstream out;
    out << '"';
    for (const unsigned char c : value) {
        if (c == '"' || c == '\\') out << '\\' << c;
        else if (c < 32) out << "\\u" << std::hex << std::setfill('0') << std::setw(4) << unsigned(c);
        else out << c;
    }
    out << '"';
    return out.str();
}

std::string fingerprint(const dfre::AssetBytes& bytes) {
    std::uint64_t hash = 0xcbf29ce484222325ull;
    for (auto byte : bytes) hash = (hash ^ byte) * 0x100000001b3ull;
    std::ostringstream out;
    out << std::hex << std::setfill('0') << std::setw(16) << hash;
    return out.str();
}

void location(const dfre::AssetLocation& source) {
    std::cout << "{\"archive\":" << json(source.archive) << ",\"member\":" << json(source.member)
              << ",\"offset\":" << source.offset << ",\"bytes\":" << source.size << '}';
}

void image(const dfre::TerrainImage& asset) {
    const auto& data = asset.image;
    std::cout << "{\"source\":";
    location(asset.source);
    std::cout << ",\"width\":" << data.width << ",\"height\":" << data.height
              << ",\"pixels\":" << data.pixels.size()
              << ",\"minimum\":" << unsigned(*std::min_element(data.pixels.begin(), data.pixels.end()))
              << ",\"maximum\":" << unsigned(*std::max_element(data.pixels.begin(), data.pixels.end()))
              << ",\"palette_bytes\":" << data.palette.size()
              << ",\"ancillary_bytes\":" << data.ancillary.size()
              << ",\"pixels_fnv1a64\":" << json(fingerprint(data.pixels))
              << ",\"palette_fnv1a64\":" << json(fingerprint(data.palette))
              << ",\"ancillary_fnv1a64\":" << json(fingerprint(data.ancillary)) << '}';
}

void mission(const dfre::MissionTerrain& data) {
    std::cout << "{\"mission\":";
    location(data.mission);
    std::cout << ",\"terrain\":";
    location(data.terrain);
    std::cout << ",\"config_fields\":" << data.config.fields.size() << ",\"maps\":{\"color_map\":";
    image(data.color);
    std::cout << ",\"elev_map\":";
    image(data.elevation);
    std::cout << ",\"detail_map\":";
    image(data.detail);
    std::cout << "}}";
}

int usage() {
    std::cerr << "assets_probe archive PFF | mission GAME_FOLDER BMS | all GAME_FOLDER | image GAME_FOLDER MEMBER | read GAME_FOLDER MEMBER\n";
    return 1;
}
} // namespace

int main(int argc, char** argv) {
    if (argc < 3) return usage();
    try {
        const std::string command = argv[1];
        if (command == "archive" && argc == 3) {
            const dfre::PffArchive archive(argv[2]);
            std::cout << "{\"archive\":" << json(archive.path().filename().string())
                      << ",\"size\":" << archive.size() << ",\"footer_bytes\":" << archive.footer_bytes()
                      << ",\"entries\":[";
            bool first = true;
            for (const auto& entry : archive.entries()) {
                if (!first) std::cout << ',';
                first = false;
                std::cout << "{\"name\":" << json(entry.name) << ",\"offset\":" << entry.offset
                          << ",\"size\":" << entry.size << ",\"timestamp\":" << entry.timestamp
                          << ",\"deleted\":" << (entry.deleted ? "true" : "false") << '}';
            }
            std::cout << "]}\n";
        } else if (command == "all" && argc == 3) {
            const dfre::AssetStore store(argv[2]);
            std::size_t resolved = 0;
            std::set<std::string> maps;
            std::vector<std::pair<std::string, std::string>> failures;
            std::cout << "{\"missions\":[";
            for (const auto& name : store.missions()) {
                try {
                    const auto data = store.load_mission(name);
                    if (resolved) std::cout << ',';
                    mission(data);
                    ++resolved;
                    for (const auto* asset : {&data.color, &data.elevation, &data.detail}) {
                        maps.insert(asset->source.archive + ":" + asset->source.member);
                    }
                } catch (const dfre::AssetError& error) {
                    failures.emplace_back(name, error.what());
                }
            }
            std::cout << "],\"resolved\":" << resolved << ",\"unique_maps\":" << maps.size()
                      << ",\"unresolved\":[";
            for (std::size_t at = 0; at < failures.size(); ++at) {
                if (at) std::cout << ',';
                std::cout << "{\"mission\":" << json(failures[at].first)
                          << ",\"error\":" << json(failures[at].second) << '}';
            }
            std::cout << "]}\n";
            if (!failures.empty()) return 1;
        } else if ((command == "mission" || command == "image" || command == "read") && argc == 4) {
            const dfre::AssetStore store(argv[2]);
            if (command == "mission") mission(store.load_mission(argv[3]));
            else if (command == "image") image(store.load_image(argv[3]));
            else {
                const auto asset = store.read(argv[3]);
                std::cout << "{\"source\":";
                location(asset.source);
                std::cout << ",\"fnv1a64\":" << json(fingerprint(asset.bytes)) << '}';
            }
            std::cout << '\n';
        } else return usage();
        return std::cout ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "assets: " << error.what() << '\n';
        return 1;
    }
}
