#pragma once

#include <fmt/format.h>

#include <cstdint>

#include "domain/db/models/formatter/fmt-helper.hpp"
#include "domain/db/models/network.hpp"

template <>
struct fmt::formatter<sst::db::NetworkMedium> {
    static constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(sst::db::NetworkMedium data, FormatContext& ctx) const {
        return fmt::format_to(ctx.out(), "{}", static_cast<std::uint8_t>(data));
    }
};

template <typename Tag>
struct fmt::formatter<sst::db::NetworkWifiNode<Tag>> {
    static constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const sst::db::NetworkWifiNode<Tag>& data, FormatContext& ctx) const {
        return fmt::format_to(ctx.out(),
                              "NetworkWifiNode{{\n"
                              "  user_id={},\n"
                              "  enabled={},\n"
                              "  medium={},\n"
                              "  ssid={},\n"
                              "  wifi_password={},\n"
                              "  static_ip={},\n"
                              "  ip_address={},\n"
                              "  subnet_mask={},\n"
                              "  gateway={}\n"
                              "}}",
                              data.user_id, sst::db::BoolToStr(data.enabled), data.medium,
                              sst::db::StrOptToStr(data.ssid),
                              sst::db::StrOptToStr(data.wifi_password),
                              sst::db::BoolToStr(data.static_ip),
                              sst::db::StrOptToStr(data.ip_address),
                              sst::db::StrOptToStr(data.subnet_mask),
                              sst::db::StrOptToStr(data.gateway));
    }
};

template <>
struct fmt::formatter<sst::db::NetworkBluetooth> {
    static constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const sst::db::NetworkBluetooth& data, FormatContext& ctx) const {
        return fmt::format_to(ctx.out(),
                              "NetworkBluetooth{{\n"
                              "  user_id={},\n"
                              "  enabled={},\n"
                              "  name=\"{}\",\n"
                              "  password=\"{}\"\n"
                              "}}",
                              data.user_id, sst::db::BoolToStr(data.enabled), data.name,
                              data.password);
    }
};
