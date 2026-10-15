#include "dfre/network.hpp"

#include <cstdint>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace {

int hex_digit(char value) {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
}

bool parse_hex(const std::string& text, std::vector<std::uint8_t>& out) {
    if (text.size() % 2) {
        return false;
    }
    for (std::size_t at = 0; at < text.size(); at += 2) {
        const int high = hex_digit(text[at]);
        const int low = hex_digit(text[at + 1]);
        if (high < 0 || low < 0) {
            return false;
        }
        out.push_back(static_cast<std::uint8_t>((high << 4) | low));
    }
    return true;
}

bool parse_hex_number(const std::string& text, std::uint32_t& value) {
    if (text.empty() || text.size() > 8) {
        return false;
    }
    value = 0;
    for (const char digit : text) {
        const int nibble = hex_digit(digit);
        if (nibble < 0) {
            return false;
        }
        value = (value << 4) | static_cast<std::uint32_t>(nibble);
    }
    return true;
}

std::string to_hex(const std::vector<std::uint8_t>& bytes) {
    constexpr char digits[] = "0123456789abcdef";
    std::string text;
    text.reserve(bytes.size() * 2);
    for (const auto byte : bytes) {
        text.push_back(digits[byte >> 4]);
        text.push_back(digits[byte & 15]);
    }
    return text;
}

void print_hex_field(const char* name, std::uint32_t value, unsigned digits) {
    constexpr char hex[] = "0123456789abcdef";
    std::cout << "\"" << name << "\":\"";
    for (int shift = static_cast<int>(digits) * 4 - 4; shift >= 0; shift -= 4) {
        std::cout << hex[(value >> shift) & 15u];
    }
    std::cout << "\"";
}

int run_encode() {
    std::string line;
    while (std::getline(std::cin, line)) {
        std::vector<std::uint8_t> payload;
        if (!parse_hex(line, payload)) {
            std::cerr << "Expected an even number of hex digits.\n";
            return 1;
        }
        std::cout << to_hex(dfre::encode_direct_datagram(payload)) << '\n';
    }
    return std::cin.bad() || !std::cout ? 1 : 0;
}

int run_filter(const std::vector<dfre::IpFilterRange>& ranges) {
    std::string line;
    while (std::getline(std::cin, line)) {
        std::uint32_t ip = 0;
        if (!parse_hex_number(line, ip)) {
            std::cerr << "Expected an 8-digit hex address.\n";
            return 1;
        }
        std::cout << "{\"blocked\":" << (dfre::source_address_blocked(ip, ranges) ? "true" : "false")
                  << "}\n";
    }
    return std::cin.bad() || !std::cout ? 1 : 0;
}

int run_decode(const std::vector<dfre::IpFilterRange>& ranges,
               const std::optional<std::vector<std::uint8_t>>& expander_bytes) {
    dfre::Expander expander;
    if (expander_bytes) {
        const std::vector<std::uint8_t> bytes = *expander_bytes;
        expander = [bytes](const std::uint8_t*, std::size_t, std::size_t capacity)
            -> std::optional<std::vector<std::uint8_t>> {
            if (bytes.size() > capacity) {
                return std::nullopt;
            }
            return bytes;
        };
    }
    std::string line;
    while (std::getline(std::cin, line)) {
        // Format: <ip hex8> <port hex4> <capacity> <packet hex>
        std::istringstream fields(line);
        std::string ip_text;
        std::string port_text;
        std::string capacity_text;
        std::string packet_text;
        if (!(fields >> ip_text >> port_text >> capacity_text)) {
            std::cerr << "Expected: <ip hex8> <port hex4> <capacity> <packet hex>.\n";
            return 1;
        }
        fields >> packet_text;
        std::uint32_t ip = 0;
        std::uint32_t port = 0;
        std::uint32_t capacity = 0;
        std::vector<std::uint8_t> packet;
        if (!parse_hex_number(ip_text, ip) || !parse_hex_number(port_text, port) || port > 0xffffu ||
            !parse_hex_number(capacity_text, capacity) || !parse_hex(packet_text, packet)) {
            std::cerr << "Invalid decode fields.\n";
            return 1;
        }
        dfre::DatagramSource source;
        source.ip = ip;
        source.port = static_cast<std::uint16_t>(port);
        const dfre::ReceiveDecodeResult result =
            dfre::decode_received_datagram(packet, capacity, source, ranges, expander);
        std::cout << "{\"status\":" << static_cast<std::uint32_t>(result.status) << ",";
        print_hex_field("ip", result.source.ip, 8);
        std::cout << ",";
        print_hex_field("port", result.source.port, 4);
        std::cout << ",";
        print_hex_field("host_port", result.source_port_host, 4);
        std::cout << ",\"size\":" << result.reported_size << ",\"extended\":"
                  << (result.extended_header ? "true" : "false") << ",\"compressed\":"
                  << (result.compressed_flag ? "true" : "false") << ",\"decompressed\":"
                  << (result.decompressed ? "true" : "false") << ",\"payload\":\""
                  << to_hex(result.payload) << "\"}\n";
    }
    return std::cin.bad() || !std::cout ? 1 : 0;
}

} // namespace

int main(int argc, char** argv) {
    std::string mode = "encode";
    std::vector<dfre::IpFilterRange> ranges;
    std::optional<std::vector<std::uint8_t>> expander_bytes;
    int index = 1;
    if (index < argc && argv[index][0] != '-') {
        mode = argv[index++];
    }
    for (; index < argc; ++index) {
        const std::string option = argv[index];
        if (option == "--block-range" && index + 1 < argc) {
            const std::string value = argv[++index];
            const std::size_t colon = value.find(':');
            std::uint32_t start = 0;
            std::uint32_t span = 0;
            if (colon == std::string::npos || !parse_hex_number(value.substr(0, colon), start) ||
                !parse_hex_number(value.substr(colon + 1), span)) {
                std::cerr << "Expected --block-range start:span in hex.\n";
                return 2;
            }
            dfre::IpFilterRange range;
            range.start = start;
            range.span = span;
            ranges.push_back(range);
        } else if (option == "--expander" && index + 1 < argc) {
            std::vector<std::uint8_t> bytes;
            if (!parse_hex(argv[++index], bytes)) {
                std::cerr << "Expected hex expander bytes.\n";
                return 2;
            }
            expander_bytes = bytes;
        } else {
            std::cerr << "Unknown option: " << option << '\n';
            return 2;
        }
    }
    if (mode == "encode") {
        return run_encode();
    }
    if (mode == "filter") {
        return run_filter(ranges);
    }
    if (mode == "decode") {
        return run_decode(ranges, expander_bytes);
    }
    std::cerr << "Usage: network_probe [encode|decode|filter] [options]\n";
    return 2;
}
