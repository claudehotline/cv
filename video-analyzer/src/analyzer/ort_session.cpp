#include "analyzer/ort_session.hpp"

#include <vector>

namespace va::analyzer {

struct OrtModelSession::Impl {
    // TODO: hold Ort::Session and resources
};

OrtModelSession::OrtModelSession() = default;
OrtModelSession::~OrtModelSession() = default;

bool OrtModelSession::loadModel(const std::string& /*model_path*/, bool /*use_gpu*/) {
    impl_ = std::make_unique<Impl>();
    loaded_ = true;
    return true;
}

bool OrtModelSession::run(const core::TensorView& /*input*/, std::vector<core::TensorView>& outputs) {
    if (!loaded_) {
        return false;
    }
    outputs.clear();
    return true;
}

} // namespace va::analyzer
