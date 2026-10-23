#pragma once
#include "dfre/crt_random.hpp"
#include "dfre/mission_sound_runtime.hpp"
namespace dfre {
struct MissionCrtThreadCells {
    std::uint32_t seed=crt_random_initial_seed;
    std::uint32_t tokenizer_cursor=0;
};
// Keep one current-thread owner alive. Map its+0C/+10 bytes in shared memory.
MissionSoundRuntimeBindings mission_crt_thread_bindings(MissionSoundRuntimeBindings,
    MissionCrtThreadCells&,std::uint32_t source_thread);
}
