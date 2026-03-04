#pragma once

#include <fmt/format.h>

#include "domain/config/models/device.hpp"
#include "fmt-helper.hpp"

template <>
struct fmt::formatter<sst::config::DeviceStaticIpData> {
    static constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const sst::config::DeviceStaticIpData& data, FormatContext& ctx) const {
        using namespace sst::config;

        return fmt::format_to(ctx.out(),
                              "DeviceStaticIpData{{\n"
                              "  enabled={},\n"
                              "  ip_address={},\n"
                              "  subnet_mask={},\n"
                              "  gateway={}\n"
                              "}}",
                              BoolOptToStr(data.enabled), StrOptToStr(data.ip_address),
                              StrOptToStr(data.subnet_mask), StrOptToStr(data.gateway));
    }
};

template <>
struct fmt::formatter<sst::config::DeviceConnectivityWifiClientData> {
    static constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const sst::config::DeviceConnectivityWifiClientData& data,
                FormatContext& ctx) const {
        using namespace sst::config;

        return fmt::format_to(ctx.out(),
                              "DeviceConnectivityWifiClientData{{\n"
                              "  enabled={},\n"
                              "  wifi_ssid={},\n"
                              "  wifi_password={},\n"
                              "  static_ip={}\n"
                              "}}",
                              BoolOptToStr(data.enabled), StrOptToStr(data.wifi_ssid),
                              StrOptToStr(data.wifi_password), ObjOptToStr(data.static_ip));
    }
};

template <>
struct fmt::formatter<sst::config::DeviceConnectivityWifiAccessPointData> {
    static constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const sst::config::DeviceConnectivityWifiAccessPointData& data,
                FormatContext& ctx) const {
        using namespace sst::config;

        return fmt::format_to(ctx.out(),
                              "DeviceConnectivityWifiAccessPointData{{\n"
                              "  enabled={},\n"
                              "  ssid={},\n"
                              "  password={}\n"
                              "}}",
                              BoolOptToStr(data.enabled), StrOptToStr(data.ssid),
                              StrOptToStr(data.password));
    }
};

template <>
struct fmt::formatter<sst::config::DeviceConnectivityWifiData> {
    static constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const sst::config::DeviceConnectivityWifiData& data, FormatContext& ctx) const {
        using namespace sst::config;

        return fmt::format_to(ctx.out(),
                              "DeviceConnectivityWifiData{{\n"
                              "  client={},\n"
                              "  access_point={}\n"
                              "}}",
                              ObjOptToStr(data.client), ObjOptToStr(data.access_point));
    }
};

template <>
struct fmt::formatter<sst::config::DeviceConnectivityEthernetData> {
    static constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const sst::config::DeviceConnectivityEthernetData& data, FormatContext& ctx) const {
        using namespace sst::config;

        return fmt::format_to(ctx.out(),
                              "DeviceConnectivityEthernetData{{\n"
                              "  enabled={},\n"
                              "  static_ip={}\n"
                              "}}",
                              BoolOptToStr(data.enabled), ObjOptToStr(data.static_ip));
    }
};

template <>
struct fmt::formatter<sst::config::DeviceConnectivityBluetoothData> {
    static constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const sst::config::DeviceConnectivityBluetoothData& data,
                FormatContext& ctx) const {
        using namespace sst::config;

        return fmt::format_to(ctx.out(),
                              "DeviceConnectivityBluetoothData{{\n"
                              "  enabled={},\n"
                              "  name={},\n"
                              "  password={}\n"
                              "}}",
                              BoolOptToStr(data.enabled), StrOptToStr(data.name),
                              StrOptToStr(data.password));
    }
};

template <>
struct fmt::formatter<sst::config::DeviceConnectivityData> {
    static constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const sst::config::DeviceConnectivityData& data, FormatContext& ctx) const {
        using namespace sst::config;

        return fmt::format_to(ctx.out(),
                              "DeviceConnectivityData{{\n"
                              "  wifi={},\n"
                              "  ethernet={},\n"
                              "  bluetooth={}\n"
                              "}}",
                              ObjOptToStr(data.wifi), ObjOptToStr(data.ethernet),
                              ObjOptToStr(data.bluetooth));
    }
};

template <>
struct fmt::formatter<sst::config::DeviceData> {
    static constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const sst::config::DeviceData& data, FormatContext& ctx) const {
        using namespace sst::config;

        return fmt::format_to(ctx.out(),
                              "DeviceData{{\n"
                              "  name={},\n"
                              "  model={},\n"
                              "  version={},\n"
                              "  serial_number={},\n"
                              "  manufacturer={},\n"
                              "  timezone={},\n"
                              "  timestamp={},\n"
                              "  connectivity={}\n"
                              "}}",
                              StrOptToStr(data.name), StrOptToStr(data.model),
                              StrOptToStr(data.version), StrOptToStr(data.serial_number),
                              StrOptToStr(data.manufacturer), StrOptToStr(data.timezone),
                              StrOptToStr(data.timestamp), ObjOptToStr(data.connectivity));
    }
};
