#include "dfre/assets.hpp"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <utility>

namespace {
using dfre::AssetBytes;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

template<class Function> void rejects(Function function, const char* message) {
    try { function(); }
    catch (const dfre::AssetError&) { return; }
    throw std::runtime_error(message);
}

void put(AssetBytes& data, std::size_t at, std::uint32_t value, unsigned count = 4) {
    for (unsigned byte = 0; byte < count; ++byte) data.at(at + byte) = static_cast<std::uint8_t>(value >> (byte * 8));
}

AssetBytes bytes(std::string_view text) { return {text.begin(), text.end()}; }

void write(const std::filesystem::path& path, const AssetBytes& data) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    if (!file) throw std::runtime_error("Cannot write synthetic fixture");
}

struct Member {
    std::string name;
    AssetBytes data;
    std::uint32_t flags = 0;
};

AssetBytes pff(const std::vector<Member>& members) {
    AssetBytes data(20, 0);
    put(data, 0, 20);
    std::copy_n("PFF3", 4, data.begin() + 4);
    put(data, 8, static_cast<std::uint32_t>(members.size()));
    put(data, 12, 32);
    AssetBytes records;
    for (const auto& member : members) {
        const auto at = records.size();
        records.resize(at + 32, 0);
        put(records, at, member.flags);
        put(records, at + 4, static_cast<std::uint32_t>(data.size()));
        put(records, at + 8, static_cast<std::uint32_t>(member.data.size()));
        put(records, at + 12, 123);
        std::copy_n(member.name.begin(), std::min<std::size_t>(member.name.size(), 16), records.begin() + at + 16);
        data.insert(data.end(), member.data.begin(), member.data.end());
    }
    put(data, 16, static_cast<std::uint32_t>(data.size()));
    data.insert(data.end(), records.begin(), records.end());
    return data;
}

AssetBytes pcx(unsigned width = 2, unsigned height = 2, unsigned stride = 2,
               AssetBytes body = {0xc2, 7, 8, 9}) {
    AssetBytes data(128, 0);
    data[0] = 10; data[1] = 5; data[2] = 1; data[3] = 8; data[65] = 1;
    put(data, 8, width - 1, 2); put(data, 10, height - 1, 2); put(data, 66, stride, 2);
    data.insert(data.end(), body.begin(), body.end());
    return data;
}

AssetBytes bms(std::string_view terrain = "TEST.TRN") {
    AssetBytes data(100, 0);
    std::copy_n("BMS\r", 4, data.begin());
    std::copy(terrain.begin(), terrain.end(), data.begin() + 84);
    return data;
}

const std::string config = "terrain_name \"Synthetic terrain\"\ncolor_map TEST_C\nelev_map TEST_D\ndetail_map TEST_M\n"
                           "type 1 2 3 grass\ntype 4 5 6 dirt\ncustom \"text // inside\" // comment\n";

struct Temporary {
    std::filesystem::path path = std::filesystem::temp_directory_path() /
        ("dfre-assets-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Temporary() { require(std::filesystem::create_directory(path), "Cannot create test folder"); }
    ~Temporary() { std::error_code ignored; std::filesystem::remove_all(path, ignored); }
};

void test_pff(const std::filesystem::path& folder) {
    const auto path = folder / "single.pff";
    const auto good = pff({{"TEST.BIN", {1, 2, 3}}});
    write(path, good);
    const dfre::PffArchive archive(path);
    require(archive.entries().size() == 1 && archive.entries()[0].timestamp == 123, "PFF directory failed");
    require(archive.read("test.bin") == AssetBytes({1, 2, 3}), "Case-insensitive PFF read failed");
    require(archive.footer_bytes() == 0, "PFF footer size failed");
    rejects([&] { archive.read("../TEST.BIN"); }, "Unsafe PFF lookup accepted");
    rejects([&] { archive.read("MISSING.BIN"); }, "Missing member accepted");
    for (auto data : {AssetBytes{}, AssetBytes(19, 0)}) {
        write(path, data);
        rejects([&] { dfre::PffArchive bad(path); }, "Truncated header accepted");
    }
    for (const auto& mutation : std::vector<std::pair<std::size_t, std::uint32_t>>{
             {0, 0}, {4, 0}, {8, 0xffffffff}, {12, 36}, {16, 19}, {16, 0xffffffff},
             {23, 2}, {27, 19}, {27, 24}, {31, 0xffffffff}}) {
        auto data = good;
        put(data, mutation.first, mutation.second);
        write(path, data);
        rejects([&] { dfre::PffArchive bad(path); }, "Malformed PFF accepted");
    }
    for (const auto& name : {"../BAD", "C:BAD", "BAD\\NAME", "", "..", "AAAAAAAAAAAAAAAA"}) {
        write(path, pff({{name, {1}}}));
        rejects([&] { dfre::PffArchive bad(path); }, "Unsafe or unterminated PFF name accepted");
    }
    write(path, pff({{"TEST.BIN", {1}}, {"test.bin", {2}}}));
    rejects([&] { dfre::PffArchive(path).read("TEST.BIN"); }, "Duplicate PFF member accepted");
    write(path, pff({{"<DEAD SPACE>", {1}, 1}, {"EMPTY.BIN", {}}}));
    require(dfre::PffArchive(path).read("EMPTY.BIN").empty(), "Empty asset failed");
    rejects([&] { dfre::PffArchive(path).read("<DEAD SPACE>"); }, "Deleted PFF member accepted");
    write(path, pff({{"BAD.BIN", {1}, 1}}));
    rejects([&] { dfre::PffArchive bad(path); }, "Wrong deleted marker accepted");
    write(path, good);
    const dfre::PffArchive before(path);
    write(path, AssetBytes(20, 0));
    rejects([&] { before.read("TEST.BIN"); }, "Changed archive size accepted");
}

void test_pcx() {
    auto image = dfre::decode_pcx(pcx());
    require(image.width == 2 && image.height == 2 && image.pixels == AssetBytes({7, 7, 8, 9}), "PCX RLE failed");
    require(image.palette.empty(), "Unexpected PCX palette");
    image = dfre::decode_pcx(pcx(1, 2, 2, {1, 2, 3, 4}));
    require(image.pixels == AssetBytes({1, 3}), "PCX row padding failed");
    for (const auto& body : std::vector<AssetBytes>{{}, {0xc2}, {0xc0, 1}, {0xc3, 1}, {1}}) {
        rejects([&] { dfre::decode_pcx(pcx(2, 2, 2, body)); }, "Bad PCX RLE accepted");
    }
    for (auto data : {AssetBytes(127, 0), pcx(3, 2, 2), pcx(2, 2, 3), pcx(32769, 1, 32770),
                      pcx(32768, 32768, 32768)}) {
        rejects([&] { dfre::decode_pcx(data); }, "Bad PCX header accepted");
    }
    auto bounds = pcx(); put(bounds, 4, 3, 2);
    rejects([&] { dfre::decode_pcx(bounds); }, "Inverted PCX bounds accepted");
    auto gradient = pcx(4, 4, 4, {0xc4, 7, 0xc4, 8, 0xc4, 9, 0xc4, 10});
    const auto end = gradient.size();
    gradient.resize(end + 1028 + 769, 0);
    std::copy_n("CerP", 4, gradient.begin() + end);
    gradient[end + 1028] = 12;
    image = dfre::decode_pcx(gradient);
    require(image.ancillary.size() == 1028 && image.palette.size() == 768, "CerP block failed");
    auto bad = gradient; bad[end] = 'X';
    rejects([&] { dfre::decode_pcx(bad); }, "Wrong CerP magic accepted");
    bad = gradient; bad[end + 1028] = 11;
    rejects([&] { dfre::decode_pcx(bad); }, "Wrong palette marker accepted");
    bad = gradient; bad.pop_back();
    rejects([&] { dfre::decode_pcx(bad); }, "Truncated CerP palette accepted");
    bad = gradient; bad.push_back(0);
    rejects([&] { dfre::decode_pcx(bad); }, "Extra CerP tail accepted");
    bad = pcx(); bad.insert(bad.end(), gradient.begin() + end, gradient.end());
    rejects([&] { dfre::decode_pcx(bad); }, "Wrong CerP dimensions accepted");
    auto palette_only = pcx(); palette_only.push_back(12); palette_only.resize(palette_only.size() + 768, 0);
    require(dfre::decode_pcx(palette_only).palette.size() == 768, "Standard PCX palette failed");
}

void test_mission(const std::filesystem::path& folder) {
    const auto parsed = dfre::parse_terrain_config(bytes(config));
    require(parsed.text == config && parsed.fields.size() == 7, "TRN full field preservation failed");
    require(parsed.fields[0].values[0] == "Synthetic terrain", "Quoted TRN value failed");
    require(parsed.fields[6].values[0] == "text // inside", "Quoted comment marker failed");
    require(parsed.fields[4].name == "type" && parsed.fields[5].name == "type", "Repeated TRN fields lost");
    for (auto data : {AssetBytes{}, bytes("color_map A\nelev_map B\n"),
                      bytes(config + "color_map OTHER\n"),
                      bytes("color_map ../BAD\nelev_map A\ndetail_map B\n"),
                      bytes("color_map \"TEST_C\"\nelev_map A\ndetail_map B\n"),
                      bytes(config + "bad \"unclosed\n"), AssetBytes(65537, 'A')}) {
        rejects([&] { dfre::parse_terrain_config(data); }, "Malformed TRN accepted");
    }
    for (auto value : {std::uint8_t(0), std::uint8_t(1), std::uint8_t(0xff)}) {
        auto data = bytes(config); data.push_back(value);
        rejects([&] { dfre::parse_terrain_config(data); }, "Invalid TRN byte accepted");
    }
    require(dfre::mission_terrain_name(bms()) == "TEST.TRN", "BMS link failed");
    for (auto data : {AssetBytes(99, 0), bms("../BAD.TRN"), bms("TEST.PCX"), bms("AAAAAAAAAAAAAAAA")}) {
        rejects([&] { dfre::mission_terrain_name(data); }, "Invalid BMS accepted");
    }
    const auto terrain_members = std::vector<Member>{{"TEST.TRN", bytes(config)}, {"TEST_C.PCX", pcx()},
                                                    {"TEST_D.PCX", pcx()}, {"TEST_M.PCX", pcx()}};
    write(folder / "Df.pff", pff(terrain_members));
    write(folder / "Dfbase.pff", pff({{"TEST.BMS", bms()}}));
    const dfre::AssetStore store(folder);
    require(store.missions() == std::vector<std::string>({"TEST.BMS"}), "Mission listing failed");
    const auto loaded = store.load_mission("test.bms");
    require(loaded.color.source.member == "TEST_C.PCX" && loaded.elevation.image.pixels.size() == 4,
            "Mission maps failed");
    require(loaded.config.text == config && loaded.terrain.member == "TEST.TRN", "Mission config failed");
    rejects([&] { store.load_mission("../TEST.BMS"); }, "Unsafe mission lookup accepted");
    rejects([&] { store.load_mission("MISSING.BMS"); }, "Missing mission accepted");
    write(folder / "Dfbase.pff", pff({{"TEST.BMS", bms()}, {"test_c.pcx", pcx(2,2,2,{0xc2,42,0xc2,42})}}));
    const auto overlap = dfre::AssetStore(folder).load_mission("TEST.BMS");
    require(overlap.color.source.archive == "Dfbase.pff" && overlap.color.image.pixels == AssetBytes(4,42), "Primary archive precedence changed");
    auto duplicate_missions = terrain_members; duplicate_missions.push_back({"test.bms",bms()});
    write(folder / "Df.pff",pff(duplicate_missions));
    require(dfre::AssetStore(folder).missions() == std::vector<std::string>{"TEST.BMS"}, "Duplicate mission listing");
    auto missing = terrain_members; missing.pop_back();
    write(folder / "Df.pff", pff(missing));
    write(folder / "Dfbase.pff", pff({{"TEST.BMS", bms()}}));
    rejects([&] { dfre::AssetStore(folder).load_mission("TEST.BMS"); }, "Missing map accepted");
}
} // namespace

int main() {
    try {
        Temporary temporary;
        test_pff(temporary.path);
        test_pcx();
        test_mission(temporary.path);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
