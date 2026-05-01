#pragma once

#include <fmt/format.h>

#include "domain/db/models/formatter/fmt-helper.hpp"
#include "domain/db/models/network.hpp"

template <>
struct fmt::formatter<sst::db::NetworkWifiDirect> {
    static constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const sst::db::NetworkWifiDirect& data, FormatContext& ctx) const {
        return fmt::format_to(ctx.out(),
                              "NetworkWifiDirect{{\n"
                              "  user_id={},\n"
                              "  enabled={},\n"
                              "  ssid=\"{}\",\n"
                              "  passphrase=\"{}\",\n"
                              "  channel={},\n"
                              "  ip_address={}\n"
                              "}}",
                              data.user_id, sst::db::BoolToStr(data.enabled), data.ssid,
                              data.passphrase, data.channel,
                              sst::db::StrOptToStr(data.ip_address));
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
