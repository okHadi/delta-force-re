#pragma once

#include <string>
#include <string_view>

namespace dfre {

// Inputs contain no NUL bytes. Extension text is copied as supplied.
// Replace the suffix after the first dot; return an owned result.
std::string set_asset_extension(std::string_view name, std::string_view extension);

} // namespace dfre
