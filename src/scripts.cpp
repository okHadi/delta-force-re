#include "dfre/scripts.hpp"
namespace dfre {
std::string decode_game_script(const AssetBytes& bytes) {
    std::string text(bytes.rbegin(), bytes.rend());
    std::uint32_t key = 0x04960552;
    for (auto& byte : text) {
        key += (key << 11) | (key >> 21);
        key = ((key << 4) | (key >> 28)) ^ 1;
        byte = static_cast<char>(static_cast<unsigned char>(byte) ^ (key & 255));
    }
    return text;
}
}
