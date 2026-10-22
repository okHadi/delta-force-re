#pragma once
#include "dfre/game_messages.hpp"
namespace dfre {
inline constexpr std::size_t mission_client_record_size=0x9b84;
struct MissionClientRecordView {std::uint8_t* bytes=nullptr;std::size_t length=0;};
struct MissionServerMessageBuffer {
    std::array<std::uint8_t,kGameMessageFrameCapacity> bytes_8e61c0{};
    std::uint32_t count_8e81c0=0;
};
struct MissionClientOutboxBindings {
    std::function<MissionClientRecordView(std::size_t)> client;
    std::function<std::uint32_t(std::uint32_t)> cell;
    std::function<void(std::uint8_t*,std::uint32_t,std::uint32_t)> transport_4977e0;
    std::function<void(std::int32_t)> diagnostic_4564b0;
};
// Client spans, stage, buffer and callback bindings stay valid during calls.
class MissionClientOutbox {
public:
    MissionClientOutbox(GameMessageStage&,MissionServerMessageBuffer&,MissionClientOutboxBindings);
    bool enqueue(std::size_t,std::int32_t,const std::uint8_t*,std::uint32_t); //441750
    void broadcast(std::int32_t,const std::uint8_t*,std::uint32_t); //442630
    void begin() noexcept; //461040; count only
    void append(std::int32_t,std::uint16_t,const std::uint8_t*,std::int32_t); //460ED0
    std::uint32_t flush(std::uint32_t peer); //460FB0
    void tick(); //4421D0
private:
    MissionClientRecordView client(std::size_t) const;
    std::uint32_t cell(std::uint32_t) const;
    const GameMessageDescriptor& descriptor(std::int32_t) const;
    GameMessageStage& messages_;
    MissionServerMessageBuffer& buffer_;
    MissionClientOutboxBindings bindings_;
};
} // namespace dfre
