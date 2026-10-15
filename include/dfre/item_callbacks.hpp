#pragma once
#include "dfre/item_runtime.hpp"
#include <cstdint>
#include <functional>
#include <string_view>
#include <vector>

namespace dfre {
// Original 0x45C510 and 0x4563E0 registries. Addresses are lookup ids.
std::uint32_t resolve_item_ai_callback(std::string_view name) noexcept;
std::uint32_t resolve_item_move_callback(std::string_view name) noexcept;
using ItemSoundResolver = std::function<std::uint32_t(std::string_view)>;
// Original 0x4011C0: bind AI, MOVE, then four ambient names in file order.
// Sound ids occupy raw +0x118..+0x124. Missing resolver writes null ids.
void bind_runtime_item_callbacks(std::vector<RuntimeItemDefinition>& items,
                                 const ItemSoundResolver& sound = {});
}
