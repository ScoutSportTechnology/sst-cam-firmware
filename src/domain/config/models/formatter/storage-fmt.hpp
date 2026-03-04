#pragma once

#include <fmt/format.h>

#include "domain/config/models/storage.hpp"

template <>
struct fmt::formatter<sst::config::StorageSectionData> {
    static constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const sst::config::StorageSectionData& data, FormatContext& ctx) const {
        const char* enabled;
        if (data.enabled) {
            enabled = *data.enabled ? "true" : "false";
        } else {
            enabled = "null";
        }
        const auto path = data.path ? fmt::format("\"{}\"", *data.path) : "null";
        const auto format_str = data.format ? fmt::format("\"{}\"", *data.format) : "null";

        return fmt::format_to(ctx.out(),
                              "StorageSectionData{{\n"
                              "  enabled={},\n"
                              "  path={},\n"
                              "  format={}\n"
                              "}}",
                              enabled, path, format_str);
    }
};

template <>
struct fmt::formatter<sst::config::StorageData> {
    static constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const sst::config::StorageData& data, FormatContext& ctx) const {
        const auto recording = data.recording ? fmt::format("{}", *data.recording) : "null";
        const auto snapshots = data.snapshots ? fmt::format("{}", *data.snapshots) : "null";
        const auto logs = data.logs ? fmt::format("{}", *data.logs) : "null";
        const auto thumbnails = data.thumbnails ? fmt::format("{}", *data.thumbnails) : "null";

        return fmt::format_to(ctx.out(),
                              "StorageData{{\n"
                              "  recording={},\n"
                              "  snapshots={},\n"
                              "  logs={},\n"
                              "  thumbnails={}\n"
                              "}}",
                              recording, snapshots, logs, thumbnails);
    }
};
