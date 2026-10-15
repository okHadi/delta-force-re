#include "dfre/audio.hpp"

namespace dfre {
namespace {

// Bytes 0x4b5639..0x4b5839 of Df.exe: 256 pan angles, left/right signed
// 8-bit gains. Index 0 is centered, 64 hard right, 192 hard left.
constexpr std::array<PanGain, kAudioPanSteps> kPanTable{{
    {   64,   63}, {   63,   64}, {   62,   65}, {   61,   66}, {   60,   67}, {   60,   67}, {   59,   68}, {   58,   69},
    {   57,   70}, {   56,   71}, {   56,   71}, {   55,   72}, {   54,   73}, {   53,   74}, {   52,   75}, {   52,   75},
    {   51,   76}, {   50,   77}, {   49,   78}, {   48,   79}, {   48,   79}, {   47,   80}, {   46,   81}, {   45,   82},
    {   44,   83}, {   43,   84}, {   43,   84}, {   42,   85}, {   41,   86}, {   40,   87}, {   39,   88}, {   38,   89},
    {   37,   90}, {   36,   91}, {   35,   92}, {   34,   93}, {   33,   94}, {   33,   94}, {   32,   95}, {   31,   96},
    {   30,   97}, {   29,   98}, {   28,   99}, {   26,  101}, {   25,  102}, {   24,  103}, {   23,  104}, {   22,  105},
    {   21,  106}, {   20,  107}, {   19,  108}, {   18,  109}, {   16,  111}, {   15,  112}, {   14,  113}, {   13,  114},
    {   11,  116}, {   10,  117}, {    9,  118}, {    7,  120}, {    6,  121}, {    5,  122}, {    3,  124}, {    2,  125},
    {    0,  127}, {   -2,  125}, {   -3,  124}, {   -5,  122}, {   -6,  121}, {   -7,  120}, {   -9,  118}, {  -10,  117},
    {  -11,  116}, {  -13,  114}, {  -14,  113}, {  -15,  112}, {  -16,  111}, {  -18,  109}, {  -19,  108}, {  -20,  107},
    {  -21,  106}, {  -22,  105}, {  -23,  104}, {  -24,  103}, {  -25,  102}, {  -26,  101}, {  -28,   99}, {  -29,   98},
    {  -30,   97}, {  -31,   96}, {  -32,   95}, {  -33,   94}, {  -33,   94}, {  -34,   93}, {  -35,   92}, {  -36,   91},
    {  -37,   90}, {  -38,   89}, {  -39,   88}, {  -40,   87}, {  -41,   86}, {  -42,   85}, {  -43,   84}, {  -43,   84},
    {  -44,   83}, {  -45,   82}, {  -46,   81}, {  -47,   80}, {  -48,   79}, {  -48,   79}, {  -49,   78}, {  -50,   77},
    {  -51,   76}, {  -52,   75}, {  -52,   75}, {  -53,   74}, {  -54,   73}, {  -55,   72}, {  -56,   71}, {  -56,   71},
    {  -57,   70}, {  -58,   69}, {  -59,   68}, {  -60,   67}, {  -60,   67}, {  -61,   66}, {  -62,   65}, {  -63,   64},
    {  -63,   64}, {   64,  -63}, {   65,  -62}, {   66,  -61}, {   67,  -60}, {   67,  -60}, {   68,  -59}, {   69,  -58},
    {   70,  -57}, {   71,  -56}, {   71,  -56}, {   72,  -55}, {   73,  -54}, {   74,  -53}, {   75,  -52}, {   75,  -52},
    {   76,  -51}, {   77,  -50}, {   78,  -49}, {   79,  -48}, {   79,  -48}, {   80,  -47}, {   81,  -46}, {   82,  -45},
    {   83,  -44}, {   84,  -43}, {   84,  -43}, {   85,  -42}, {   86,  -41}, {   87,  -40}, {   88,  -39}, {   89,  -38},
    {   90,  -37}, {   91,  -36}, {   92,  -35}, {   93,  -34}, {   94,  -33}, {   94,  -33}, {   95,  -32}, {   96,  -31},
    {   97,  -30}, {   98,  -29}, {   99,  -28}, {  101,  -26}, {  102,  -25}, {  103,  -24}, {  104,  -23}, {  105,  -22},
    {  106,  -21}, {  107,  -20}, {  108,  -19}, {  109,  -18}, {  111,  -16}, {  112,  -15}, {  113,  -14}, {  114,  -13},
    {  116,  -11}, {  117,  -10}, {  118,   -9}, {  120,   -7}, {  121,   -6}, {  122,   -5}, {  124,   -3}, {  125,   -2},
    {  127,    0}, {  125,    2}, {  124,    3}, {  122,    5}, {  121,    6}, {  120,    7}, {  118,    9}, {  117,   10},
    {  116,   11}, {  114,   13}, {  113,   14}, {  112,   15}, {  111,   16}, {  109,   18}, {  108,   19}, {  107,   20},
    {  106,   21}, {  105,   22}, {  104,   23}, {  103,   24}, {  102,   25}, {  101,   26}, {   99,   28}, {   98,   29},
    {   97,   30}, {   96,   31}, {   95,   32}, {   94,   33}, {   94,   33}, {   93,   34}, {   92,   35}, {   91,   36},
    {   90,   37}, {   89,   38}, {   88,   39}, {   87,   40}, {   86,   41}, {   85,   42}, {   84,   43}, {   84,   43},
    {   83,   44}, {   82,   45}, {   81,   46}, {   80,   47}, {   79,   48}, {   79,   48}, {   78,   49}, {   77,   50},
    {   76,   51}, {   75,   52}, {   75,   52}, {   74,   53}, {   73,   54}, {   72,   55}, {   71,   56}, {   71,   56},
    {   70,   57}, {   69,   58}, {   68,   59}, {   67,   60}, {   67,   60}, {   66,   61}, {   65,   62}, {   64,   63},
}};

inline std::uint32_t read_u32(const std::vector<std::uint8_t>& image, std::size_t at) {
    return static_cast<std::uint32_t>(image[at]) |
           (static_cast<std::uint32_t>(image[at + 1]) << 8) |
           (static_cast<std::uint32_t>(image[at + 2]) << 16) |
           (static_cast<std::uint32_t>(image[at + 3]) << 24);
}

inline std::uint16_t read_u16(const std::vector<std::uint8_t>& image, std::size_t at) {
    return static_cast<std::uint16_t>(image[at] | (image[at + 1] << 8));
}

inline void write_u32(std::vector<std::uint8_t>& image, std::size_t at, std::uint32_t value) {
    image[at] = static_cast<std::uint8_t>(value);
    image[at + 1] = static_cast<std::uint8_t>(value >> 8);
    image[at + 2] = static_cast<std::uint8_t>(value >> 16);
    image[at + 3] = static_cast<std::uint8_t>(value >> 24);
}

inline void write_u16(std::vector<std::uint8_t>& image, std::size_t at, std::uint16_t value) {
    image[at] = static_cast<std::uint8_t>(value);
    image[at + 1] = static_cast<std::uint8_t>(value >> 8);
}

} // namespace

const std::array<PanGain, kAudioPanSteps>& audio_pan_table() {
    return kPanTable;
}

std::uint32_t audio_mixer_step(std::uint32_t rate) {
    // Original: eax = rate << 4; idiv 22050; result << 24 (low 32 bits kept).
    const std::uint32_t scaled = rate << 4;
    const std::int64_t signed_scaled = (scaled & 0x80000000u)
        ? static_cast<std::int64_t>(scaled) - 0x100000000LL
        : static_cast<std::int64_t>(scaled);
    const std::int64_t quotient = signed_scaled / static_cast<std::int64_t>(kAudioMixRate);
    return static_cast<std::uint32_t>(
        (static_cast<std::uint64_t>(quotient) * 0x1000000ull) & 0xffffffffull);
}

SampleRecord convert_wav_record(std::vector<std::uint8_t>& image, std::uint32_t tag) {
    SampleRecord record;
    record.tag = tag;
    record.pcm_offset = 0x10;
    if (image.size() < 0x2c) {
        return record;
    }
    if (read_u32(image, 0) == tag) {
        // The original skips a buffer that already carries this tag.
        record.length = read_u32(image, 0x04);
        record.step = read_u32(image, 0x08);
        record.volume = read_u16(image, 0x0c);
        record.source_bits = static_cast<std::uint8_t>(read_u16(image, 0x22));
        return record;
    }
    const std::uint32_t rate = read_u32(image, 0x18);
    const std::uint16_t bits = read_u16(image, 0x22);
    std::uint32_t data_size = read_u32(image, 0x28);
    if (data_size > image.size() - 0x2c) {
        data_size = static_cast<std::uint32_t>(image.size() - 0x2c);
    }
    write_u32(image, 0, tag);
    write_u16(image, 0x0c, 0x0100);
    record.step = audio_mixer_step(rate);
    write_u32(image, 0x08, record.step);
    record.source_bits = static_cast<std::uint8_t>(bits);

    if (bits == 0x10) {
        const std::uint32_t count = data_size >> 1;
        std::uint32_t peak = 0;
        for (std::uint32_t at = 0; at < count; ++at) {
            const std::int16_t sample = static_cast<std::int16_t>(read_u16(image, 0x2c + at * 2));
            const std::uint32_t magnitude = sample < 0
                ? static_cast<std::uint32_t>(-static_cast<std::int32_t>(sample))
                : static_cast<std::uint32_t>(sample);
            if (magnitude > peak) {
                peak = magnitude;
            }
        }
        if (peak == 0) {
            // Original divides by zero here; the port writes silence instead.
            for (std::uint32_t at = 0; at < count; ++at) {
                image[0x10 + at] = 0x80;
            }
        } else {
            const std::uint32_t norm = 0x7eff0000u / peak;
            for (std::uint32_t at = 0; at < count; ++at) {
                const std::uint8_t low = image[0x2c + at * 2];
                const std::uint32_t signed_low = low < 0x80u ? low : (low | 0xffffff00u);
                const std::uint32_t product = signed_low * norm;
                image[0x10 + at] =
                    static_cast<std::uint8_t>(((product + 0x800000u) >> 24) ^ 0x80u);
            }
        }
        record.length = count;
    } else {
        for (std::uint32_t at = 0; at < data_size; ++at) {
            image[0x10 + at] = image[0x2c + at];
        }
        record.length = data_size;
    }
    write_u32(image, 0x04, record.length);
    record.volume = read_u16(image, 0x0c);
    return record;
}

StereoGain compute_voice_gain(int pan, int volume, int master_volume,
                              int sample_volume_shift) {
    const std::uint32_t pan_index = static_cast<std::uint32_t>(pan) & 0xffu;
    const PanGain pair = kPanTable[pan_index];
    const std::int32_t scale =
        static_cast<std::int32_t>(static_cast<std::uint32_t>(volume) *
                                  static_cast<std::uint32_t>(master_volume));
    // x86 masks the shift count to five bits.
    const int shift = (0x10 + (sample_volume_shift & 0xff)) & 31;
    StereoGain gain;
    gain.right = static_cast<std::int32_t>(
        static_cast<std::uint32_t>(pair.right) *
        static_cast<std::uint32_t>(scale)) >> shift;
    gain.left = static_cast<std::int32_t>(
        static_cast<std::uint32_t>(pair.left) *
        static_cast<std::uint32_t>(scale)) >> shift;
    return gain;
}

int clamp_play_volume(int volume) {
    if (volume > 0xff) {
        return 0xff;
    }
    if (volume < 0) {
        return 0;
    }
    return volume;
}

std::uint32_t clamp_master_volume(std::uint32_t volume) {
    return volume > 0xffu ? 0xffu : volume;
}

} // namespace dfre
