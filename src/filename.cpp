#include "dfre/filename.hpp"

namespace dfre {

std::string set_asset_extension(std::string_view name, std::string_view extension) {
    std::string result(name.substr(0, name.find('.')));
    result.push_back('.');
    result.append(extension);
    return result;
}

} // namespace dfre
