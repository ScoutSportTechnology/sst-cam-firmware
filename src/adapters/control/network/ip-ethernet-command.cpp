#include "adapters/control/network/ip-ethernet-command.hpp"

#include <algorithm>
#include <bitset>
#include <cctype>
#include <cstdint>
#include <sstream>
#include <string>

namespace sst::adapters::control {

namespace {

constexpr int kMaxOctet = 255;
constexpr int kMaxPrefix = 32;
constexpr int kOctetsPerIpv4 = 4;
constexpr int kBitsPerOctet = 8;

// A single decimal octet in [0,255]: 1-3 digits, all decimal, value in range,
// and no leading-zero padding ("01", "00" rejected — a strict literal only).
auto IsOctet(const std::string& tok) -> bool {
    if (tok.empty() || tok.size() > 3) {
        return false;
    }
    if (!std::all_of(tok.begin(), tok.end(),
                     [](unsigned char chr) { return std::isdigit(chr) != 0; })) {
        return false;
    }
    if (tok.size() > 1 && tok[0] == '0') {
        return false;  // no leading zeros
    }
    return std::stoi(tok) <= kMaxOctet;
}

// Split `text` on `delim` into exactly the produced tokens (empty tokens kept, so
// "1..2" yields an empty middle token that IsOctet then rejects).
auto Split(const std::string& text, char delim) -> std::vector<std::string> {
    std::vector<std::string> out;
    std::string tok;
    std::istringstream stream(text);
    while (std::getline(stream, tok, delim)) {
        out.push_back(tok);
    }
    // getline drops a trailing empty field ("1." → {"1"}); add it back so a
    // trailing delimiter is caught as malformed rather than silently trimmed.
    if (!text.empty() && text.back() == delim) {
        out.emplace_back();
    }
    return out;
}

}  // namespace

auto IsIpv4Literal(const std::string& text) -> bool {
    // Reject any stray character up front (newline, space, ';', letters) so a
    // smuggled directive can never slip through octet parsing.
    if (text.empty() || text.find_first_not_of("0123456789.") != std::string::npos) {
        return false;
    }
    const auto octets = Split(text, '.');
    if (octets.size() != kOctetsPerIpv4) {
        return false;
    }
    return std::all_of(octets.begin(), octets.end(), IsOctet);
}

auto IsIpv4Cidr(const std::string& text) -> bool {
    if (text.find_first_not_of("0123456789./") != std::string::npos) {
        return false;
    }
    const auto parts = Split(text, '/');
    if (parts.size() != 2 || !IsIpv4Literal(parts[0])) {
        return false;
    }
    const std::string& prefix = parts[1];
    if (prefix.empty() || prefix.size() > 2 ||
        !std::all_of(prefix.begin(), prefix.end(),
                     [](unsigned char chr) { return std::isdigit(chr) != 0; })) {
        return false;
    }
    if (prefix.size() > 1 && prefix[0] == '0') {
        return false;
    }
    return std::stoi(prefix) <= kMaxPrefix;
}

auto MaskToPrefix(const std::string& mask) -> std::optional<int> {
    if (!IsIpv4Literal(mask)) {
        return std::nullopt;
    }
    const auto octets = Split(mask, '.');
    std::uint32_t bits = 0;
    for (const auto& octet : octets) {
        bits = (bits << kBitsPerOctet) | static_cast<std::uint32_t>(std::stoi(octet) & kMaxOctet);
    }
    // A valid netmask is a run of 1s followed by a run of 0s: (~bits + 1) must be
    // a power of two (or bits == all-ones / all-zeros).
    const std::uint32_t inverted = ~bits;
    if ((inverted & (inverted + 1)) != 0) {
        return std::nullopt;  // non-contiguous mask
    }
    return static_cast<int>(std::bitset<kMaxPrefix>(bits).count());
}

auto BuildResolvConf(const std::string& dns) -> std::string {
    if (dns.empty()) {
        return {};
    }
    return "nameserver " + dns + "\n";
}

auto BuildEthAddrReplaceArgv(const std::string& cidr,
                             const std::string& iface) -> std::vector<std::string> {
    if (iface.empty() || !IsIpv4Cidr(cidr)) {
        return {};
    }
    return {"ip", "addr", "replace", cidr, "dev", iface};
}

auto BuildEthAddrDelArgv(const std::string& cidr,
                         const std::string& iface) -> std::vector<std::string> {
    if (iface.empty() || !IsIpv4Cidr(cidr)) {
        return {};
    }
    return {"ip", "addr", "del", cidr, "dev", iface};
}

auto BuildEthRouteReplaceArgv(const std::string& gateway,
                              const std::string& iface) -> std::vector<std::string> {
    if (iface.empty() || !IsIpv4Literal(gateway)) {
        return {};
    }
    return {"ip", "route", "replace", "default", "via", gateway, "dev", iface, "metric", "100"};
}

auto ParseDhcpOfferCidr(const std::string& leases) -> std::optional<std::string> {
    // Scan every lease block, keeping the LAST fixed-address / subnet-mask seen —
    // dhclient appends renewals, so the final block is the freshest offer.
    std::istringstream stream(leases);
    std::string line;
    std::string address;
    std::string mask;
    const auto take_value = [](const std::string& text, const std::string& key) -> std::string {
        const auto pos = text.find(key);
        if (pos == std::string::npos) {
            return {};
        }
        auto start = pos + key.size();
        while (start < text.size() &&
               (std::isspace(static_cast<unsigned char>(text[start])) != 0)) {
            ++start;
        }
        const auto end = text.find_first_of(";\t ", start);
        return text.substr(start, end == std::string::npos ? std::string::npos : end - start);
    };
    while (std::getline(stream, line)) {
        // Both keys may sit on the SAME line (a compact single-line lease block),
        // so parse them independently — never else-if — and let the last-seen
        // value win, which is the freshest offer dhclient appended.
        if (line.find("fixed-address ") != std::string::npos) {
            const std::string value = take_value(line, "fixed-address ");
            if (IsIpv4Literal(value)) {
                address = value;
            }
        }
        if (line.find("option subnet-mask ") != std::string::npos) {
            mask = take_value(line, "option subnet-mask ");
        }
    }
    if (address.empty()) {
        return std::nullopt;
    }
    const auto prefix = MaskToPrefix(mask);
    if (!prefix) {
        return std::nullopt;
    }
    return address + "/" + std::to_string(*prefix);
}

auto DeriveEthernetMode(sst::network::EthernetMode last_applied, bool carrier, bool has_address,
                        bool dhcp_lease_active) -> sst::network::EthernetMode {
    if (!carrier || !has_address) {
        return sst::network::EthernetMode::kDown;
    }
    if (dhcp_lease_active || last_applied == sst::network::EthernetMode::kDhcp) {
        return sst::network::EthernetMode::kDhcp;
    }
    return sst::network::EthernetMode::kStatic;
}

}  // namespace sst::adapters::control
