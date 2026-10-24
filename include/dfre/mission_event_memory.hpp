#pragma once
#include "dfre/mission_event_resources.hpp"
#include <cstddef>
#include <memory>
#include <stdexcept>
namespace dfre {
class MissionEventMemoryError : public std::runtime_error {public:using std::runtime_error::runtime_error;};
enum class EventTable { events, triggers, actions };
namespace detail {struct MissionEventMemoryState;}
class EventRecordView {
public:
    EventRecordView()=default;
    std::uint32_t source_address() const noexcept{return address_;}
    std::uint32_t byte_size() const noexcept{return bytes_;}
    std::uint32_t read(std::uint32_t offset,unsigned width) const;
    void write(std::uint32_t offset,unsigned width,std::uint32_t value) const;
private:
    friend class MissionEventMemory;
    EventRecordView(const std::shared_ptr<detail::MissionEventMemoryState>&,
        std::uint32_t address,std::uint32_t bytes);
    std::weak_ptr<detail::MissionEventMemoryState> state_;
    std::uint32_t address_=0,bytes_=0;
};
// Keep the binding object and its actual memory owner alive longer than this adapter.
class MissionEventMemory {
public:
    explicit MissionEventMemory(const MissionEventResourceBindings&);
    MissionEventMemory(MissionEventResourceBindings&&)=delete;
    MissionEventMemory(const MissionEventResourceBindings&&)=delete;
    ~MissionEventMemory();
    MissionEventMemory(const MissionEventMemory&)=delete;
    MissionEventMemory& operator=(const MissionEventMemory&)=delete;
    MissionEventMemory(MissionEventMemory&&)=delete;
    MissionEventMemory& operator=(MissionEventMemory&&)=delete;
    std::uint32_t count_bits(EventTable) const;
    std::uint32_t table_base(EventTable) const;
    EventRecordView event_at(std::size_t index) const;
    EventRecordView trigger_at(std::size_t index) const;
    EventRecordView action_at(std::size_t index) const;
    EventRecordView referenced_record(EventTable,std::uint32_t address) const;
    std::size_t current_index(EventTable,std::uint32_t address,std::size_t run_count=1) const;
    std::uint32_t encoded_reference(EventTable,std::size_t index,std::size_t run_count) const;
    std::uint32_t read(std::uint32_t address,unsigned width) const;
    void write(std::uint32_t address,unsigned width,std::uint32_t value) const;
    void load_current_file();
    void reset();
private:
    EventRecordView at(EventTable,std::size_t) const;
    std::shared_ptr<detail::MissionEventMemoryState> state_;
};
}
