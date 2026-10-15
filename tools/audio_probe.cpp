#include "dfre/audio.hpp"

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

constexpr char kDigits[] = "0123456789abcdef";

int hex_digit(char value) {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
}

bool from_hex(const std::string& text, std::vector<std::uint8_t>& bytes) {
    if (text.size() % 2) {
        return false;
    }
    bytes.clear();
    bytes.reserve(text.size() / 2);
    for (std::size_t at = 0; at < text.size(); at += 2) {
        const int high = hex_digit(text[at]);
        const int low = hex_digit(text[at + 1]);
        if (high < 0 || low < 0) {
            return false;
        }
        bytes.push_back(static_cast<std::uint8_t>((high << 4) | low));
    }
    return true;
}

void print_hex32(std::uint32_t value) {
    for (int shift = 28; shift >= 0; shift -= 4) {
        std::cout << kDigits[(value >> shift) & 15];
    }
}

void print_hex(const std::uint8_t* data, std::size_t size) {
    for (std::size_t at = 0; at < size; ++at) {
        std::cout << kDigits[data[at] >> 4] << kDigits[data[at] & 15];
    }
}

std::uint32_t parse_u32(const std::string& text) {
    return static_cast<std::uint32_t>(std::stoul(text, nullptr, 0));
}

std::uint32_t parse_hex_u32(const std::string& text) {
    return static_cast<std::uint32_t>(std::stoul(text, nullptr, 16));
}

void run_wav(const std::string& tag_text, const std::string& body) {
    std::vector<std::uint8_t> image;
    if (!from_hex(body, image)) {
        std::cerr << "wav: expected hex bytes.\n";
        std::exit(1);
    }
    const dfre::SampleRecord record = dfre::convert_wav_record(image, parse_hex_u32(tag_text));
    print_hex32(record.tag);
    std::cout << ' ' << record.length << ' ';
    print_hex32(record.step);
    std::cout << ' ';
    std::cout << kDigits[(record.volume >> 12) & 15] << kDigits[(record.volume >> 8) & 15]
              << kDigits[(record.volume >> 4) & 15] << kDigits[record.volume & 15];
    std::cout << ' ' << kDigits[record.source_bits >> 4] << kDigits[record.source_bits & 15] << ' ';
    print_hex(image.data() + record.pcm_offset, record.length);
    std::cout << ' ';
    print_hex(image.data(), record.pcm_offset + record.length);
    std::cout << '\n';
}

} // namespace

int main() {
    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream parts(line);
        std::string command;
        parts >> command;
        if (command == "pan") {
            const auto& table = dfre::audio_pan_table();
            std::vector<std::uint8_t> raw;
            raw.reserve(table.size() * 2);
            for (const auto& pair : table) {
                raw.push_back(static_cast<std::uint8_t>(pair.left));
                raw.push_back(static_cast<std::uint8_t>(pair.right));
            }
            print_hex(raw.data(), raw.size());
            std::cout << '\n';
        } else if (command == "gain") {
            std::string pan, volume, master, shift;
            parts >> pan >> volume >> master >> shift;
            const dfre::StereoGain gain = dfre::compute_voice_gain(
                std::stoi(pan), std::stoi(volume), std::stoi(master), std::stoi(shift));
            std::cout << gain.right << ' ' << gain.left << '\n';
        } else if (command == "clamp") {
            std::string value;
            parts >> value;
            std::cout << dfre::clamp_play_volume(std::stoi(value)) << '\n';
        } else if (command == "master") {
            std::string value;
            parts >> value;
            std::cout << dfre::clamp_master_volume(parse_u32(value)) << '\n';
        } else if (command == "step") {
            std::string value;
            parts >> value;
            print_hex32(dfre::audio_mixer_step(parse_u32(value)));
            std::cout << '\n';
        } else if (command == "wav") {
            std::string tag, body;
            parts >> tag >> body;
            run_wav(tag, body);
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
