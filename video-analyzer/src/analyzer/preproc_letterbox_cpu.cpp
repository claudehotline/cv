#include "analyzer/preproc_letterbox_cpu.hpp"

namespace va::analyzer {

LetterboxPreprocessorCPU::LetterboxPreprocessorCPU(int input_width, int input_height)
    : input_width_(input_width), input_height_(input_height) {}

bool LetterboxPreprocessorCPU::run(const core::Frame& in, core::TensorView& out, core::LetterboxMeta& meta) {
    meta.scale = 1.0f;
    meta.pad_x = 0;
    meta.pad_y = 0;
    meta.input_width = input_width_;
    meta.input_height = input_height_;

    out.data = const_cast<uint8_t*>(in.bgr.data());
    out.shape = {1, 3, in.height, in.width};
    out.dtype = core::DType::U8;
    out.on_gpu = false;

    return out.data != nullptr;
}

} // namespace va::analyzer
