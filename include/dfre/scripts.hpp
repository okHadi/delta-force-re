#pragma once
#include "dfre/assets.hpp"
#include <string>
namespace dfre {
// Original 0x456eb0 transform with the DF1 startup key.
std::string decode_game_script(const AssetBytes& bytes);
}
