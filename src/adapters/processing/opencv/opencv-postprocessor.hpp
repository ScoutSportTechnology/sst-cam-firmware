#pragma once

#include <optional>

#include "app/processing/ports/postprocessor.hpp"
#include "domain/capture/models/frame.hpp"
#include "domain/processing/models/crop-rect.hpp"
#include "domain/processing/models/postprocess-config.hpp"

namespace sst::adapters::processing {

class OpenCvPostprocessor final : public sst::processing::IPostprocessor {
   public:
    explicit OpenCvPostprocessor(sst::processing::PostprocessConfig config = {});
    ~OpenCvPostprocessor() override = default;

    auto Process(const sst::capture::Frame& source, const sst::processing::CropRect& crop)
        -> std::optional<sst::capture::Frame> override;

   private:
    sst::processing::PostprocessConfig config_;
};

}  // namespace sst::adapters::processing
