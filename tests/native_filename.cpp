#include "dfre/filename.hpp"

#include <iostream>
#include <string>
#include <string_view>

int main() {
    struct Case {
        std::string_view name;
        std::string_view extension;
        std::string_view expected;
    };
    const Case cases[] = {
        {"MAP", "PCX", "MAP.PCX"},
        {"Map.trn", "PCX", "Map.PCX"},
        {"Map.old.data", "TrN", "Map.TrN"},
        {".hidden", "dat", ".dat"},
        {"..", "x", ".x"},
        {"MAP.", "pcx", "MAP.pcx"},
        {"", "PCX", ".PCX"},
        {"", "", "."},
        {"MAP.old", "", "MAP."},
        {"MAP", "", "MAP."},
        {"dir.name/map.pcx", "trn", "dir.trn"},
        {"dir\\map", "PCX", "dir\\map.PCX"},
        {"A", "long.extension", "A.long.extension"},
        {" map name ", " PCX ", " map name . PCX "},
    };
    for (const auto& test : cases) {
        const auto actual = dfre::set_asset_extension(test.name, test.extension);
        if (actual != test.expected) {
            std::cerr << "Unexpected result for name: " << test.name << '\n';
            return 1;
        }
    }
    std::string name = "asset.old";
    std::string extension = "pcx";
    const auto owned = dfre::set_asset_extension(name, extension);
    name.assign("changed");
    extension.assign("changed");
    if (owned != "asset.pcx") {
        std::cerr << "Result must own its bytes.\n";
        return 1;
    }
    return 0;
}
