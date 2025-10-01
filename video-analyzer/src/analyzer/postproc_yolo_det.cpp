#include "analyzer/postproc_yolo_det.hpp"

namespace va::analyzer {

bool YoloDetectionPostprocessor::run(const std::vector<core::TensorView>& /*raw_outputs*/,
                                     const core::LetterboxMeta& /*meta*/,
                                     core::ModelOutput& output) {
    output.boxes.clear();
    output.masks.clear();
    return true;
}

} // namespace va::analyzer
