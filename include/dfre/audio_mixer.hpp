#pragma once

#include <array>
#include <cstdint>

#include "dfre/audio.hpp"

namespace dfre {

// Voice stepping rules rebuilt from Df.exe (SHA-256
// abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff).
// Data only: nothing here opens a device or plays a sample.

// Logical inspection view of a mixer voice; this is not a packed record.
// Mixer voice record. The original keeps two arrays of 16 records with a
// 0x34-byte stride: pending voices at 0x4a1a12 and live voices at 0x4a16d2.
// Field offsets are the original ones.
struct MixVoice {
    std::uint32_t end = 0;            // +0x00 end address (pcm + length)
    std::uint32_t length = 0;         // +0x04 length in bytes
    std::uint32_t frequency = 0;      // +0x08 fixed point step per frame
    std::int32_t position = 0;        // +0x0c biased position, 1/1024 byte
    std::uint32_t step = 0;           // +0x14 sample record step
    std::int32_t pan = 0;             // +0x18
    std::int32_t volume = 0;          // +0x1c
    std::uint8_t sample_volume = 0;   // +0x20
    std::int32_t right_gain = 0;      // +0x24
    std::int32_t left_gain = 0;       // +0x28
    std::uint8_t active = 0;          // +0x30
    std::uint8_t stop = 0;            // +0x31 (gates voice reuse)
    std::uint8_t dirty = 0;           // +0x32
};

inline constexpr std::size_t kMixVoiceCount = 16;

// 0x4a364b: position + 0x12345678, carry means the sample ended.
inline constexpr std::uint32_t kMixPositionBias = 0x12345678u;

// 0x4a3665: byte address = (position + bias) >> 10 - 0x01234568.
inline constexpr std::uint32_t kMixFetchBias = 0x01234568u;
inline constexpr int kMixPositionShift = 10;

// FUN_004b4f5e first and second pass. Returns the slot to fill or -1.
int select_voice(const std::array<MixVoice, kMixVoiceCount>& live,
                 const std::array<MixVoice, kMixVoiceCount>& pending,
                 int volume);

// FUN_004b4f5e core for one record. `pcm_address` is the 32-bit address of
// the first PCM byte of the converted sample record.
MixVoice start_voice(const SampleRecord& record, std::uint32_t pcm_address,
                     int pan, int volume, int master_volume,
                     std::uint32_t rate_scale);

struct VoiceStep {
    bool ended = false;
    std::int32_t position = 0;
    std::int32_t index = 0;
    std::uint32_t address = 0;
};

// Raw unpatched 0x4a364b inspection only. Live playback uses current patched
// operands through MissionAudioPlayback.
VoiceStep advance_voice(MixVoice& voice);

// 0x4a362d: unsigned byte to signed sample value.
std::int32_t sample_value(std::uint8_t byte);

// 0x4a3639: one sample accumulation (cursor block rule).
std::int32_t accumulate(std::int32_t accumulator, std::uint8_t byte,
                        std::int32_t gain);

// FUN_004a48fc kernel selection for a gain value.
// shift comes from the byte table at 0x4a29e0 and the index is
// (gain << shift) >> 3 with an arithmetic shift.
struct GainKernel {
    std::uint8_t shift = 0;
    std::int32_t index = 0;
    std::array<std::uint8_t, 8> bytes{};
    bool in_window = true;
};

GainKernel gain_kernel(std::int32_t gain);

} // namespace dfre
