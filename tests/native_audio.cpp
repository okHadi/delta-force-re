#include "dfre/audio.hpp"

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

// Silent test: only data rules run here. No device is opened, no sample is
// played and the system volume is not touched.
// Golden values come from the original Df.exe routines emulated by
// tools/check_audio_original.py.

namespace {

bool require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
    }
    return condition;
}

int hex_digit(char value) {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    return -1;
}

std::vector<std::uint8_t> from_hex(const std::string& text) {
    std::vector<std::uint8_t> bytes;
    for (std::size_t at = 0; at + 1 < text.size(); at += 2) {
        const int high = hex_digit(text[at]);
        const int low = hex_digit(text[at + 1]);
        bytes.push_back(static_cast<std::uint8_t>((high << 4) | low));
    }
    return bytes;
}

std::string to_hex(const std::uint8_t* data, std::size_t size) {
    constexpr char digits[] = "0123456789abcdef";
    std::string text;
    text.reserve(size * 2);
    for (std::size_t at = 0; at < size; ++at) {
        text.push_back(digits[data[at] >> 4]);
        text.push_back(digits[data[at] & 15]);
    }
    return text;
}

std::uint32_t fnv1a32(const std::vector<std::uint8_t>& bytes) {
    std::uint32_t hash = 0x811c9dc5u;
    for (const auto byte : bytes) {
        hash = (hash ^ byte) * 0x01000193u;
    }
    return hash;
}

// Build a canonical 44-byte-header PCM WAV image.
std::vector<std::uint8_t> make_wav(const std::vector<std::int32_t>& samples,
                                   std::uint32_t rate, std::uint16_t bits) {
    std::vector<std::uint8_t> payload;
    for (const auto sample : samples) {
        if (bits == 16) {
            payload.push_back(static_cast<std::uint8_t>(sample & 0xff));
            payload.push_back(static_cast<std::uint8_t>((sample >> 8) & 0xff));
        } else {
            payload.push_back(static_cast<std::uint8_t>(sample & 0xff));
        }
    }
    const std::uint32_t block = bits == 16 ? 2 : 1;
    std::vector<std::uint8_t> image;
    const auto push32 = [&image](std::uint32_t value) {
        image.push_back(static_cast<std::uint8_t>(value));
        image.push_back(static_cast<std::uint8_t>(value >> 8));
        image.push_back(static_cast<std::uint8_t>(value >> 16));
        image.push_back(static_cast<std::uint8_t>(value >> 24));
    };
    const auto push16 = [&image](std::uint16_t value) {
        image.push_back(static_cast<std::uint8_t>(value));
        image.push_back(static_cast<std::uint8_t>(value >> 8));
    };
    image.insert(image.end(), {'R', 'I', 'F', 'F'});
    push32(36 + static_cast<std::uint32_t>(payload.size()));
    image.insert(image.end(), {'W', 'A', 'V', 'E'});
    image.insert(image.end(), {'f', 'm', 't', ' '});
    push32(16);
    push16(1);
    push16(1);
    push32(rate);
    push32(rate * block);
    push16(static_cast<std::uint16_t>(block));
    push16(bits);
    image.insert(image.end(), {'d', 'a', 't', 'a'});
    push32(static_cast<std::uint32_t>(payload.size()));
    image.insert(image.end(), payload.begin(), payload.end());
    return image;
}

bool check_record(const std::vector<std::uint8_t>& wav, std::uint32_t tag,
                  std::uint32_t length, std::uint32_t step, std::uint16_t volume,
                  const std::string& pcm, const char* name) {
    std::vector<std::uint8_t> image = wav;
    const dfre::SampleRecord record = dfre::convert_wav_record(image, tag);
    if (!require(record.tag == tag, name)) return false;
    if (!require(record.length == length, name)) return false;
    if (!require(record.step == step, name)) return false;
    if (!require(record.volume == volume, name)) return false;
    if (!require(to_hex(image.data() + record.pcm_offset, record.length) == pcm, name)) return false;
    if (record.pcm_offset != 0x10) return false;
    // A second pass over the converted image must not change it.
    const std::vector<std::uint8_t> once = image;
    const dfre::SampleRecord again = dfre::convert_wav_record(image, tag);
    if (!require(image == once, "convert must be idempotent")) return false;
    if (!require(again.length == length && again.step == step && again.volume == volume,
                 "converted record fields must stay")) return false;
    return true;
}

} // namespace

int main() {
    const std::uint32_t tag = 0x5a5a5a5au;

    // Pan table copied from Df.exe 0x4b5639.
    const auto& table = dfre::audio_pan_table();
    if (!require(table.size() == 256, "Pan table must have 256 entries.")) return 1;
    if (!require(table[0].left == 64 && table[0].right == 63, "Pan 0 must be centered.")) return 1;
    if (!require(table[1].left == 63 && table[1].right == 64, "Pan 1 must lean right.")) return 1;
    if (!require(table[64].left == 0 && table[64].right == 127, "Pan 64 must be hard right.")) return 1;
    if (!require(table[128].left == -63 && table[128].right == 64, "Pan 128 must invert left.")) return 1;
    if (!require(table[192].left == 127 && table[192].right == 0, "Pan 192 must be hard left.")) return 1;
    if (!require(table[255].left == 64 && table[255].right == 63, "Pan 255 must close the circle.")) return 1;
    std::vector<std::uint8_t> raw;
    for (const auto& pair : table) {
        raw.push_back(static_cast<std::uint8_t>(pair.left));
        raw.push_back(static_cast<std::uint8_t>(pair.right));
    }
    if (!require(fnv1a32(raw) == 0x521b0e8fu, "Pan table bytes must match Df.exe 0x4b5639.")) return 1;

    // Mixer step: (rate * 16 / 22050) << 24.
    if (!require(dfre::audio_mixer_step(8000) == 0x05000000u, "Step for 8000 Hz failed.")) return 1;
    if (!require(dfre::audio_mixer_step(11025) == 0x08000000u, "Step for 11025 Hz failed.")) return 1;
    if (!require(dfre::audio_mixer_step(22050) == dfre::kAudioStepOne, "Step for 22050 Hz failed.")) return 1;
    if (!require(dfre::audio_mixer_step(44100) == 0x20000000u, "Step for 44100 Hz failed.")) return 1;

    // WAV images converted by FUN_00494c50.
    if (!check_record(from_hex("524946464400000057415645666d74201000000001000100112b0000112b00"
                               "000100080064617461200000000b30557a9fc4e90e33587da2c7ec11365b80"
                               "a5caef14395e83a8cdf2173c6186"),
                      tag, 32, 0x08000000u, 0x0100,
                      "0b30557a9fc4e90e33587da2c7ec11365b80a5caef14395e83a8cdf2173c6186",
                      "8-bit ramp at 11025 Hz failed.")) return 1;
    if (!check_record(from_hex("524946462c00000057415645666d7420100000000100010022560000225600"
                               "0001000800646174610800000080c850ff005aaa1e"),
                      tag, 8, 0x10000000u, 0x0100, "80c850ff005aaa1e",
                      "8-bit beep at 22050 Hz failed.")) return 1;
    if (!check_record(from_hex("524946463400000057415645666d742010000000010001002256000044ac00"
                               "00020010006461746110000000e80318fcff7f008000000700f9ff3930"),
                      tag, 8, 0x10000000u, 0x0100, "8080808080808080",
                      "16-bit loud image failed.")) return 1;
    if (!check_record(from_hex("524946463400000057415645666d742010000000010001002256000044ac00"
                               "0002001000646174611000000064009cfffe0002ff00000700f9ff7b00"),
                      tag, 8, 0x10000000u, 0x0100, "b24e7f8180837dbd",
                      "16-bit quiet image failed.")) return 1;

    // Loud 16-bit audio collapses to 0x80: the original scales the low byte
    // by the full 16-bit peak (see docs/audio-runtime.md).
    {
        std::vector<std::uint8_t> loud =
            make_wav({32767, -32768, 32767, -32768, 32767, -32768, 32767, -32768}, 22050, 16);
        const dfre::SampleRecord record = dfre::convert_wav_record(loud, tag);
        if (!require(record.length == 8, "Loud 16-bit length failed.")) return 1;
        if (!require(to_hex(loud.data() + 0x10, 8) == "8080808080808080",
                     "Loud 16-bit samples must collapse to 0x80.")) return 1;
    }
    // All-zero 16-bit input divides by zero in the original; the port writes
    // 0x80 silence instead of faulting.
    {
        std::vector<std::uint8_t> silent = make_wav({0, 0, 0, 0, 0, 0, 0, 0}, 22050, 16);
        const dfre::SampleRecord record = dfre::convert_wav_record(silent, tag);
        if (!require(record.length == 8, "Silent 16-bit length failed.")) return 1;
        if (!require(to_hex(silent.data() + 0x10, 8) == "8080808080808080",
                     "Silent 16-bit samples must become 0x80.")) return 1;
    }

    // Gain values from FUN_004b4d3a.
    struct GainCase {
        int master;
        int pan;
        int volume;
        int shift;
        std::int32_t right;
        std::int32_t left;
    };
    const GainCase gain_cases[] = {
        {255, 0, 255, 0, 62, 63},
        {255, 64, 255, 0, 126, 0},
        {255, 192, 255, 0, 0, 126},
        {255, 128, 255, 0, 63, -63},
        {200, 0, 200, 0, 38, 39},
        {0, 0, 255, 0, 0, 0},
        {255, 0, 255, 16, 4096575, 4161600},
        {0x10000, 0, 255, 0, 16065, 16320},
        {255, 100, 100, 3, 4, -2},
        {255, 5, 250, 2, 16, 14},
    };
    for (const auto& item : gain_cases) {
        const dfre::StereoGain gain = dfre::compute_voice_gain(
            item.pan, item.volume, item.master, item.shift);
        if (!require(gain.right == item.right && gain.left == item.left,
                     "Voice gain case failed.")) return 1;
    }
    // Pan is a byte; the shift count wraps at 32 like the original.
    if (!require(dfre::compute_voice_gain(256, 255, 255, 0).left ==
                 dfre::compute_voice_gain(0, 255, 255, 0).left,
                 "Pan must be masked to one byte.")) return 1;
    if (!require(dfre::compute_voice_gain(0, 255, 255, 16).left ==
                 dfre::compute_voice_gain(0, 255, 255, 48).left,
                 "Shift count must wrap at 32.")) return 1;

    // Volume clamps from 0x422bd0 and 0x4947e0.
    if (!require(dfre::clamp_play_volume(-5) == 0, "Effect volume below zero failed.")) return 1;
    if (!require(dfre::clamp_play_volume(255) == 255, "Effect volume 255 failed.")) return 1;
    if (!require(dfre::clamp_play_volume(256) == 255, "Effect volume above 255 failed.")) return 1;
    if (!require(dfre::clamp_play_volume(70000) == 255, "Large effect volume failed.")) return 1;
    if (!require(dfre::clamp_master_volume(255) == 255, "Master volume 255 failed.")) return 1;
    if (!require(dfre::clamp_master_volume(256) == 255, "Master volume 256 failed.")) return 1;
    if (!require(dfre::clamp_master_volume(0xffffffffu) == 255,
                 "Master volume 0xffffffff must clamp to 255.")) return 1;

    return 0;
}
