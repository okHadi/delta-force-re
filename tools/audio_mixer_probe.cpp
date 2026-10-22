#include "dfre/audio_mixer.hpp"

#include <array>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

constexpr char kDigits[] = "0123456789abcdef";

std::int32_t parse_int(const std::string& text) {
    return static_cast<std::int32_t>(std::stoul(text, nullptr, 0));
}

std::uint32_t parse_u32(const std::string& text) {
    return static_cast<std::uint32_t>(std::stoul(text, nullptr, 0));
}

void print_hex32(std::uint32_t value) {
    for (int shift = 28; shift >= 0; shift -= 4) {
        std::cout << kDigits[(value >> shift) & 15];
    }
}

std::string to_hex(const std::uint8_t* data, std::size_t size) {
    std::string text;
    for (std::size_t at = 0; at < size; ++at) {
        text.push_back(kDigits[data[at] >> 4]);
        text.push_back(kDigits[data[at] & 15]);
    }
    return text;
}

void set_active(std::array<dfre::MixVoice, dfre::kMixVoiceCount>& voices,
                std::uint32_t mask) {
    for (std::size_t index = 0; index < dfre::kMixVoiceCount; ++index) {
        if ((mask >> index) & 1u) {
            voices[index].active = 1;
        }
    }
}

void set_stop(std::array<dfre::MixVoice, dfre::kMixVoiceCount>& voices,
              std::uint32_t mask) {
    for (std::size_t index = 0; index < dfre::kMixVoiceCount; ++index) {
        if ((mask >> index) & 1u) {
            voices[index].stop = 1;
        }
    }
}

void run_select(const std::string& volume, const std::string& a30,
                const std::string& a31, const std::string& b30,
                const std::string& volumes) {
    std::array<dfre::MixVoice, dfre::kMixVoiceCount> live{};
    std::array<dfre::MixVoice, dfre::kMixVoiceCount> pending{};
    set_active(live, parse_u32(a30));
    set_stop(live, parse_u32(a31));
    set_active(pending, parse_u32(b30));
    if (volumes.size() >= 64) {
        for (std::size_t index = 0; index < dfre::kMixVoiceCount; ++index) {
            live[index].volume = static_cast<std::int32_t>(
                std::stoul(volumes.substr(index * 4, 4), nullptr, 16));
            if (live[index].volume >= 0x8000) {
                live[index].volume -= 0x10000;
            }
        }
    }
    std::cout << dfre::select_voice(live, pending, parse_int(volume)) << '\n';
}

void run_start(const std::string& pan, const std::string& volume,
               const std::string& master, const std::string& rate_scale,
               const std::string& length, const std::string& address,
               const std::string& sv) {
    dfre::SampleRecord record;
    record.length = parse_u32(length);
    record.step = 0x10000000u;
    record.volume = static_cast<std::uint16_t>(parse_u32(sv));
    record.source_bits = 8;
    record.pcm_offset = 0x10;
    const dfre::MixVoice voice = dfre::start_voice(
        record, parse_u32(address), parse_int(pan), parse_int(volume),
        parse_int(master), parse_u32(rate_scale));
    print_hex32(voice.end);
    std::cout << ' ' << voice.length << ' ';
    print_hex32(voice.frequency);
    std::cout << ' ' << voice.position << ' ';
    print_hex32(voice.step);
    std::cout << ' ' << voice.pan << ' ' << voice.volume << ' ' << static_cast<int>(voice.sample_volume)
              << ' ' << voice.right_gain << ' ' << voice.left_gain << ' '
              << static_cast<int>(voice.active) << static_cast<int>(voice.stop)
              << static_cast<int>(voice.dirty) << '\n';
}

void run_step(const std::string& position, const std::string& frequency) {
    dfre::MixVoice voice;
    voice.position = parse_int(position);
    voice.frequency = parse_u32(frequency);
    const dfre::VoiceStep step = dfre::advance_voice(voice);
    std::cout << (step.ended ? 1 : 0) << ' ' << step.position << ' ' << step.index
              << ' ';
    print_hex32(step.address);
    std::cout << ' ' << voice.position << '\n';
}

void run_sample(const std::string& byte, const std::string& gain) {
    const auto value = static_cast<std::uint8_t>(parse_u32(byte));
    const std::int32_t gain_value = parse_int(gain);
    std::cout << dfre::sample_value(value) << ' '
              << dfre::accumulate(1000, value, gain_value) << '\n';
}

void run_kernel(const std::string& gain) {
    const dfre::GainKernel kernel = dfre::gain_kernel(parse_int(gain));
    std::cout << static_cast<int>(kernel.shift) << ' ';
    print_hex32(static_cast<std::uint32_t>(kernel.index));
    std::cout << ' ' << to_hex(kernel.bytes.data(), kernel.bytes.size()) << ' '
              << (kernel.in_window ? 1 : 0) << '\n';
}

} // namespace

int main() {
    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream parts(line);
        std::string command;
        parts >> command;
        if (command == "select") {
            std::string volume, a30, a31, b30, volumes;
            parts >> volume >> a30 >> a31 >> b30 >> volumes;
            run_select(volume, a30, a31, b30, volumes);
        } else if (command == "start") {
            std::string pan, volume, master, rate_scale, length, address, sv;
            parts >> pan >> volume >> master >> rate_scale >> length >> address >> sv;
            run_start(pan, volume, master, rate_scale, length, address, sv);
        } else if (command == "step") {
            std::string position, frequency;
            parts >> position >> frequency;
            run_step(position, frequency);
        } else if (command == "sample") {
            std::string byte, gain;
            parts >> byte >> gain;
            run_sample(byte, gain);
        } else if (command == "kernel") {
            std::string gain;
            parts >> gain;
            run_kernel(gain);
        } else {
            std::cerr << "Unknown probe command: " << command << '\n';
            return 1;
        }
        if (!std::cout) {
            return 1;
        }
    }
    return std::cin.bad() ? 1 : 0;
}
