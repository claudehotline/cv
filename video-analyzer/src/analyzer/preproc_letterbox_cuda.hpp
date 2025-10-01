#pragma once

#include "analyzer/interfaces.hpp"

namespace va::analyzer {

class LetterboxPreprocessorCUDA : public IPreprocessor {
public:
    LetterboxPreprocessorCUDA(int input_width, int input_height);

    bool run(const core::Frame& in, core::TensorView& out, core::LetterboxMeta& meta) override;

private:
    int input_width_;
    int input_height_;
};

} // namespace va::analyzer
