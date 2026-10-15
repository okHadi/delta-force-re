#include "dfre/network_send.hpp"

#include <cstdint>
#include <iostream>
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

const char* format_name(dfre::SendStatus status, dfre::SendWireFormat format, bool sent) {
    if (status != dfre::SendStatus::Ok || !sent) {
        return "none";
    }
    return format == dfre::SendWireFormat::Extended ? "extended" : "simple";
}

std::string hex_field(std::uint32_t value, unsigned digits) {
    constexpr char hex[] = "0123456789abcdef";
    std::string text(digits, '0');
    for (unsigned index = 0; index < digits; ++index) {
        text[digits - 1 - index] = hex[(value >> (4 * index)) & 15u];
    }
    return text;
}

void print_result(std::uint32_t status, const char* format, std::uint32_t ip, std::uint32_t port,
                  std::size_t wire, bool flag, const std::vector<std::uint8_t>& datagram) {
    std::cout << "{\"status\":" << status << ",\"format\":\"" << format << "\""
              << ",\"ip\":\"" << hex_field(ip, 8) << "\""
              << ",\"port\":\"" << hex_field(port, 4) << "\""
              << ",\"wire\":" << wire << ",\"flag\":" << (flag ? "true" : "false")
              << ",\"datagram\":\"" << to_hex(datagram) << "\"}\n";
}

int run_extended() {
    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream fields(line);
        std::string ip_text;
        std::string port_text;
        std::string flag_text;
        std::string payload_text;
        if (!(fields >> ip_text >> port_text >> flag_text)) {
            std::cerr << "Expected: <ip hex8> <port hex4> <flag> <payload hex>.\n";
            return 1;
        }
        fields >> payload_text;
        std::uint32_t ip = 0;
        std::uint32_t port = 0;
        std::vector<std::uint8_t> payload;
        if (!parse_hex_number(ip_text, ip) || !parse_hex_number(port_text, port) || port > 0xffffu ||
            !parse_hex(payload_text, payload) || (flag_text != "0" && flag_text != "1")) {
            std::cerr << "Invalid extended fields.\n";
            return 1;
        }
        dfre::SendTarget target;
        target.ip = ip;
        target.port = static_cast<std::uint16_t>(port);
        const dfre::ExtendedEncodeResult result =
            dfre::encode_extended_datagram(payload, target, flag_text == "1");
        const bool sent = !result.datagram.empty() && result.status == dfre::SendStatus::Ok;
        print_result(static_cast<std::uint32_t>(result.status),
                     sent ? "extended" : "none", ip, port, result.datagram.size(),
                     result.compressed_flag, result.datagram);
    }
    return std::cin.bad() || !std::cout ? 1 : 0;
}

int run_select(bool compression_available) {
    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream fields(line);
        std::string ip_text;
        std::string port_text;
        std::string payload_text;
        if (!(fields >> ip_text >> port_text)) {
            std::cerr << "Expected: <ip hex8> <port hex4> <payload hex>.\n";
            return 1;
        }
        fields >> payload_text;
        std::uint32_t ip = 0;
        std::uint32_t port = 0;
        std::vector<std::uint8_t> payload;
        if (!parse_hex_number(ip_text, ip) || !parse_hex_number(port_text, port) || port > 0xffffu ||
            !parse_hex(payload_text, payload)) {
            std::cerr << "Invalid select fields.\n";
            return 1;
        }
        dfre::SendTarget target;
        target.ip = ip;
        target.port = static_cast<std::uint16_t>(port);
        const dfre::SendSelectionResult result =
            dfre::select_and_encode_send(payload, target, compression_available);
        const bool sent = !result.datagram.empty() && result.status == dfre::SendStatus::Ok;
        print_result(static_cast<std::uint32_t>(result.status),
                     format_name(result.status, result.format, sent), ip, port,
                     result.datagram.size(), result.compressed_flag, result.datagram);
    }
    return std::cin.bad() || !std::cout ? 1 : 0;
}

} // namespace

int main(int argc, char** argv) {
    std::string mode = "select";
    bool compression_available = true;
    int index = 1;
    if (index < argc && argv[index][0] != '-') {
        mode = argv[index++];
    }
    for (; index < argc; ++index) {
        const std::string option = argv[index];
        if (option == "--no-compression") {
            compression_available = false;
        } else {
            std::cerr << "Unknown option: " << option << '\n';
            return 2;
        }
    }
    if (mode == "extended") {
        return run_extended();
    }
    if (mode == "select") {
        return run_select(compression_available);
    }
    std::cerr << "Usage: network_send_probe [extended|select] [--no-compression]\n";
    return 2;
}
