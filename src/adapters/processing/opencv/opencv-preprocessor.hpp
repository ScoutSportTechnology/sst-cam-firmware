#pragma once

#include <optional>

#include "app/processing/ports/preprocessor.hpp"
#include "domain/capture/models/frame.hpp"
#include "domain/processing/models/frame-bundle.hpp"
#include "domain/processing/models/preprocess-config.hpp"

namespace sst::adapters::processing {

class OpenCvPreprocessor final : public sst::processing::IPreprocessor {
   public:
    explicit OpenCvPreprocessor(sst::processing::PreprocessConfig config = {});
    ~OpenCvPreprocessor() override = default;

    auto Process(const sst::capture::Frame& raw)
        -> std::optional<sst::processing::FrameBundle> override;

   private:
    sst::processing::PreprocessConfig config_;
};

}  // namespace sst::adapters::processing
