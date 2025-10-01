#include "analyzer/analyzer.hpp"

#include <utility>

namespace va::analyzer {

Analyzer::Analyzer() = default;

void Analyzer::setPreprocessor(std::shared_ptr<IPreprocessor> preprocessor) {
    preprocessor_ = std::move(preprocessor);
}

void Analyzer::setSession(std::shared_ptr<IModelSession> session) {
    session_ = std::move(session);
}

void Analyzer::setPostprocessor(std::shared_ptr<IPostprocessor> postprocessor) {
    postprocessor_ = std::move(postprocessor);
}

void Analyzer::setRenderer(std::shared_ptr<IRenderer> renderer) {
    renderer_ = std::move(renderer);
}

bool Analyzer::analyze(const core::Frame& in, core::Frame& out) {
    if (!preprocessor_ || !session_ || !postprocessor_ || !renderer_) {
        return false;
    }

    core::TensorView tensor;
    core::LetterboxMeta meta;
    if (!preprocessor_->run(in, tensor, meta)) {
        return false;
    }

    std::vector<core::TensorView> outputs;
    if (!session_->run(tensor, outputs)) {
        return false;
    }

    core::ModelOutput model_output;
    if (!postprocessor_->run(outputs, meta, model_output)) {
        return false;
    }

    return renderer_->draw(in, model_output, out);
}

bool Analyzer::switchModel(const std::string& /*model_id*/) {
    // TODO: implement model switching in subsequent stages
    return true;
}

bool Analyzer::switchTask(const std::string& /*task_id*/) {
    // TODO: implement task switching in subsequent stages
    return true;
}

bool Analyzer::updateParams(std::shared_ptr<AnalyzerParams> params) {
    params_ = std::move(params);
    return static_cast<bool>(params_);
}

} // namespace va::analyzer
