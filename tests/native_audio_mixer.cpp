#include "dfre/audio_mixer.hpp"

#include <array>
#include <cstdint>
#include <iostream>
#include <string>

// Silent test: only data rules run here. No device is opened, no sample is
// played and the system volume is not touched.
// Golden values come from the original Df.exe routines emulated by
// tools/check_audio_mixer_original.py.

namespace {

bool require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
    }
    return condition;
}

std::string to_hex(const std::array<std::uint8_t, 8>& bytes) {
    constexpr char digits[] = "0123456789abcdef";
    std::string text;
    for (const auto byte : bytes) {
        text.push_back(digits[byte >> 4]);
        text.push_back(digits[byte & 15]);
    }
    return text;
}

struct StartCase {
    int pan;
    int volume;
    int master;
    std::uint32_t rate_scale;
    std::uint32_t length;
    std::uint32_t address;
    int sample_volume;
    std::uint32_t end;
    std::uint32_t frequency;
    std::int32_t position;
    std::int32_t right;
    std::int32_t left;
};

struct StepCase {
    int voice;
    std::uint32_t position;
    int ended;
    std::int32_t new_position;
    std::int32_t index;
    std::uint32_t address;
};

} // namespace

int main() {
    // Voice selection (FUN_004b4f5e). 0x4a16d2 is the live table.
    struct SelectCase {
        int volume;
        std::uint32_t live_active;
        std::uint32_t live_stop;
        std::uint32_t pending;
        std::int32_t volume0;
        std::int32_t volume1;
        int slot;
    };
    const SelectCase select_cases[] = {
        {200, 0x0000, 0x0000, 0x0000, 0, 0, 0},
        {200, 0x0001, 0x0000, 0x0000, 0, 0, 1},
        {90, 0x0011, 0x0000, 0x0000, 0, 1, 1},
        {50, 0x0001, 0x0000, 0x0000, 0, 100, 1},
        {200, 0x0000, 0x0000, 0x0001, 0, 0, 1},
    };
    for (const auto& item : select_cases) {
        std::array<dfre::MixVoice, dfre::kMixVoiceCount> live{};
        std::array<dfre::MixVoice, dfre::kMixVoiceCount> pending{};
        for (std::size_t index = 0; index < dfre::kMixVoiceCount; ++index) {
            live[index].active = (item.live_active >> index) & 1u;
            live[index].stop = (item.live_stop >> index) & 1u;
            pending[index].active = (item.pending >> index) & 1u;
        }
        live[0].volume = item.volume0;
        live[1].volume = item.volume1;
        const int slot = dfre::select_voice(live, pending, item.volume);
        if (!require(slot == item.slot, "Voice selection case failed.")) return 1;
    }
    // Every slot busy and louder than the new sound: no voice is returned.
    {
        std::array<dfre::MixVoice, dfre::kMixVoiceCount> live{};
        std::array<dfre::MixVoice, dfre::kMixVoiceCount> pending{};
        for (auto& voice : live) {
            voice.active = 1;
            voice.volume = 500;
        }
        if (!require(dfre::select_voice(live, pending, 100) == -1,
                     "Full voice table must return -1.")) return 1;
    }

    // Voice start (FUN_004b4f5e core).
    const StartCase start_cases[] = {
        {0, 255, 255, 0x10000, 32, 0xfedf3000, 0, 0xfedf3020, 0x00000400, -31744, 62, 63},
        {128, 255, 255, 0x10000, 32, 0xfedf3000, 0, 0xfedf3020, 0x00000400, -31744, 63, -63},
        {-64, 200, 200, 0x8000, 100, 0xfee01000, 0, 0xfee01064, 0x00000200, -101376, 0, 77},
        {7, 0, 255, 0x0, 1, 0xfee01200, 1, 0xfee01201, 0x00000400, 0, 0, 0},
    };
    for (const auto& item : start_cases) {
        dfre::SampleRecord record;
        record.length = item.length;
        record.step = 0x10000000u;
        record.volume = static_cast<std::uint16_t>(item.sample_volume);
        record.source_bits = 8;
        record.pcm_offset = 0x10;
        const dfre::MixVoice voice = dfre::start_voice(
            record, item.address, item.pan, item.volume, item.master, item.rate_scale);
        if (!require(voice.end == item.end, "Voice end failed.")) return 1;
        if (!require(voice.length == item.length, "Voice length failed.")) return 1;
        if (!require(voice.frequency == item.frequency, "Voice frequency failed.")) return 1;
        if (!require(voice.position == item.position, "Voice position failed.")) return 1;
        if (!require(voice.step == record.step, "Voice step failed.")) return 1;
        if (!require(voice.right_gain == item.right && voice.left_gain == item.left,
                     "Voice gains failed.")) return 1;
        if (!require(voice.active == 1 && voice.stop == 0 && voice.dirty == 0,
                     "Voice flags failed.")) return 1;
    }

    // Position stepping (0x4a364b). Carry means the voice ended.
    const StepCase step_cases[] = {
        {0, 0x00000000, 0, 305419896, 298261, 0xfee147ad},
        {0, 0x00000400, 0, 305420920, 298262, 0xfee147ae},
        {3, 0x33000000, 0, 1161057912, 1133845, 0xfeee07ad},
        {0, 0xffffffff, 1, -1, 0, 0x00000000},
        {7, 0xedcbb000, 1, -305418240, 0, 0x00000000},
    };
    for (const auto& item : step_cases) {
        dfre::MixVoice voice;
        voice.position = static_cast<std::int32_t>(item.position);
        const dfre::VoiceStep step = dfre::advance_voice(voice);
        if (!require(step.ended == (item.ended != 0), "Voice end flag failed.")) return 1;
        if (step.ended) {
            if (!require(voice.position == item.new_position,
                         "Ended voice must keep its position.")) return 1;
            continue;
        }
        if (!require(voice.position == item.new_position, "Stepped position failed.")) return 1;
        if (!require(step.index == item.index, "Fetch index failed.")) return 1;
        if (!require(step.address == item.address, "Fetch address failed.")) return 1;
    }

    // Sample conversion and accumulation (0x4a362d).
    struct SampleCase {
        std::uint8_t byte;
        std::int32_t gain;
        std::int32_t value;
        std::int32_t accumulate;
    };
    const SampleCase sample_cases[] = {
        {0x00, 64, -128, -7192},
        {0xff, -64, 127, -7128},
        {0x80, 1, 0, 1000},
        {0x40, 33, -64, -1112},
    };
    for (const auto& item : sample_cases) {
        if (!require(dfre::sample_value(item.byte) == item.value,
                     "Sample value failed.")) return 1;
        if (!require(dfre::accumulate(1000, item.byte, item.gain) == item.accumulate,
                     "Sample accumulation failed.")) return 1;
    }

    // Gain kernel selection (FUN_004a48fc).
    struct KernelCase {
        std::int32_t gain;
        std::uint8_t shift;
        std::int32_t index;
        const char* bytes;
    };
    const KernelCase kernel_cases[] = {
        {0, 6, 0, "8d041bb800000000"},
        {1, 6, 8, "8d04dd0000000090"},
        {63, 2, 31, "8d041bc1e0042bc3"},
        {126, 1, 31, "8d041bc1e0042bc3"},
        {-1, 6, -8, "8d041bc1e002f7d8"},
    };
    for (const auto& item : kernel_cases) {
        const dfre::GainKernel kernel = dfre::gain_kernel(item.gain);
        if (!require(kernel.in_window, "Kernel must be in the table window.")) return 1;
        if (!require(kernel.shift == item.shift, "Kernel shift failed.")) return 1;
        if (!require(kernel.index == item.index, "Kernel index failed.")) return 1;
        if (!require(to_hex(kernel.bytes) == item.bytes, "Kernel bytes failed.")) return 1;
    }
    // Far outside the table window the lookup is reported as unsupported.
    if (!require(!dfre::gain_kernel(100000).in_window,
                 "Out-of-window kernel must be flagged.")) return 1;

    return 0;
}
