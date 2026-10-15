#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace dfre {

// Audio data rules rebuilt from Df.exe (SHA-256
// abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff).
// All functions below are data-only. They never open or play a device.

// The original mixer runs at 22050 Hz (FUN_00494c50 divides by 0x5622).
inline constexpr std::uint32_t kAudioMixRate = 22050;

// Pan is one byte: a full circle with 256 steps (table at 0x4b5639).
inline constexpr std::size_t kAudioPanSteps = 256;

// Original fixed sample step: rate 22050 maps to 16 << 24 (494C50).
inline constexpr std::uint32_t kAudioStepOne = 16u << 24;

struct PanGain {
    std::int8_t left;
    std::int8_t right;
};

// 256-entry circular pan table copied from Df.exe 0x4b5639.
const std::array<PanGain, kAudioPanSteps>& audio_pan_table();

// Converted sample record produced by FUN_00494c50:
//   +0x00 tag     generation tag written by the loader
//   +0x04 length  bytes (8-bit) or samples (16-bit)
//   +0x08 step    (rate * 16 / 22050) << 24
//   +0x0c volume  word 0x0100
//   +0x10 pcm     8-bit unsigned PCM
struct SampleRecord {
    std::uint32_t tag = 0;
    std::uint32_t length = 0;
    std::uint32_t step = 0;
    std::uint16_t volume = 0;
    std::uint8_t source_bits = 0;
    std::size_t pcm_offset = 0;
};

// Mixer step for a source rate (FUN_00494c50, signed idiv by 22050).
std::uint32_t audio_mixer_step(std::uint32_t rate);

// FUN_00494c50. Rewrites a 44-byte-header RIFF WAV image in place:
// moves PCM to offset 0x10, writes the record header and normalizes
// 16-bit PCM to 8-bit unsigned. A buffer whose first word already equals
// `tag` is treated as converted and returned unchanged.
SampleRecord convert_wav_record(std::vector<std::uint8_t>& image, std::uint32_t tag);

// Volume and pan gains for one voice (FUN_004b4d3a).
// gain = pan_gain * volume * master_volume >> (16 + sample_volume_shift)
struct StereoGain {
    std::int32_t right = 0;
    std::int32_t left = 0;
};

StereoGain compute_voice_gain(int pan, int volume, int master_volume,
                              int sample_volume_shift);

// FUN_00422bd0 clamps effect volume to 0..255 with signed compares.
int clamp_play_volume(int volume);

// FUN_004947e0 stores the master volume; values above 0xff (unsigned)
// become 0xff.
std::uint32_t clamp_master_volume(std::uint32_t volume);

} // namespace dfre
