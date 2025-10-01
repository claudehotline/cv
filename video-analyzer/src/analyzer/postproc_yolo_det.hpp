#pragma once

#include "analyzer/interfaces.hpp"

namespace va::analyzer {

class YoloDetectionPostprocessor : public IPostprocessor {
public:
    bool run(const std::vector<core::TensorView>& raw_outputs,
             const core::LetterboxMeta& meta,
             core::ModelOutput& output) override;
};

} // namespace va::analyzer
