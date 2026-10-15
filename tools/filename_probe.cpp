#include "dfre/filename.hpp"

#include <iostream>
#include <string>
#include <string_view>

int main() {
    std::string line;
    while (std::getline(std::cin, line)) {
        const auto tab = line.find('\t');
        if (tab == std::string::npos || line.find('\t', tab + 1) != std::string::npos ||
            line.find('\0') != std::string::npos) {
            std::cerr << "Expected one tab between name and extension, with no NUL bytes.\n";
            return 1;
        }
        const std::string_view view(line);
        std::cout << dfre::set_asset_extension(view.substr(0, tab), view.substr(tab + 1)) << '\n';
    }
    return std::cin.bad() || !std::cout ? 1 : 0;
}
