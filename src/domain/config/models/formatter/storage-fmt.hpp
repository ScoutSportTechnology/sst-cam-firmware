#pragma once

#include <fmt/format.h>

#include "domain/config/models/storage.hpp"
#include "fmt-helper.hpp"

template <>
struct fmt::formatter<sst::config::StorageData> {
    static constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const sst::config::StorageData& data, FormatContext& ctx) const {
        using namespace sst::config;

        return fmt::format_to(ctx.out(),
                              "StorageData{{\n"
                              "  log={},\n"
                              "  video={},\n"
                              "  snapshots={},\n"
                              "  thumbnails={}\n"
                              "}}",
                              StrOptToStr(data.log), StrOptToStr(data.video),
                              StrOptToStr(data.snapshots), StrOptToStr(data.thumbnails));
    }
};
