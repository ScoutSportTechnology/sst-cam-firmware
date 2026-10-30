#pragma once

#include <fmt/format.h>

#include "domain/network/models/interface-reachability.hpp"

template <>
struct fmt::formatter<sst::network::InterfaceReachability> {
    static constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const sst::network::InterfaceReachability& reachability, FormatContext& ctx) const {
        return fmt::format_to(ctx.out(), "InterfaceReachability{{ethernet_wan={}, wifi_sta={}}}",
                              reachability.ethernet_wan, reachability.wifi_sta);
    }
};

template <>
struct fmt::formatter<sst::network::EgressInterface> {
    static constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(sst::network::EgressInterface egress, FormatContext& ctx) const {
        return fmt::format_to(ctx.out(), "{}", sst::network::ToString(egress));
    }
};
