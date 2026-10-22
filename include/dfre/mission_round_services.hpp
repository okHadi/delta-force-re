#pragma once
#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace dfre {
class MissionRoundServiceError : public std::runtime_error {
public: using std::runtime_error::runtime_error;
};
using MissionRoundLogValue=std::variant<std::uint32_t,std::string>;
struct MissionRoundServiceBindings {
    std::function<std::uint32_t(std::uint32_t address,std::uint32_t width)> cell;
    std::function<void(std::uint32_t address,std::uint32_t width,std::uint32_t value)> store;
    std::function<std::uint32_t(std::uint32_t slot,std::uint32_t offset,std::uint32_t width)> client;
    // Squad base6E8090, stride68. Preserve wrapped source address mapping.
    std::function<std::uint32_t(std::uint32_t slot,std::uint32_t offset,std::uint32_t width)> squad;
    std::function<void(std::uint32_t slot,std::uint32_t offset,std::uint32_t width,std::uint32_t value)> squad_store;
    std::function<std::string(std::uint32_t address)> text;
    std::function<std::string(std::uint32_t slot)> client_name; // current client+C, through its end byte
    std::function<std::uint32_t(const std::string& path,const std::string& mode)> file_open; // 49A41C
    std::function<void(std::uint32_t file,std::uint32_t format,const std::vector<MissionRoundLogValue>&)> file_print; // 49BAC0
    std::function<void(std::uint32_t file)> file_close; // 49A840
    // SYSTEMTIME: year,month,weekday,day,hour,minute,second,millisecond.
    std::function<std::array<std::uint16_t,8>()> local_time;
};
void mission_round_set_client_updates(const MissionRoundServiceBindings&,std::uint32_t enabled); // 446100
void mission_round_clear_frame_flag(const MissionRoundServiceBindings&); // 444F60,6E5570=0
void mission_round_collect_squads(const MissionRoundServiceBindings&); // 446DA0
std::int32_t mission_round_compare_clients(const MissionRoundServiceBindings&,std::uint32_t first,std::uint32_t second); // 448130
// 448D20 uses incoming EDI for team deductions before it sets EDI itself.
// Supply that exact input when the team branch is reached; stock SP exits first.
void mission_round_log(const MissionRoundServiceBindings&,std::uint32_t mode,std::optional<std::uint32_t> inherited_edi=std::nullopt);
const char* mission_round_log_format(std::uint32_t address);
} // namespace dfre
