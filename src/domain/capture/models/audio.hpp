#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

#include "domain/common/models/timestamp.hpp"

namespace sst::capture {
using sst::common::Timestamp;

enum class AudioFormat {
    UNKNOWN,
    S16,
    S32,
    F32,
};

struct AudioSpec {
    std::uint32_t sample_rate{0};
    std::uint32_t channels{0};
    AudioFormat format{AudioFormat::UNKNOWN};
};

struct Audio {
    std::uint64_t audio_id{0};
    AudioSpec spec{};
    std::uint32_t sample_count{0};
    const uint8_t* data{nullptr};
    std::size_t size{0};
    Timestamp captured_at;
    std::shared_ptr<void> owner{nullptr};
};

}  // namespace sst::capture