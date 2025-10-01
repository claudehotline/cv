#pragma once

#include "analyzer/interfaces.hpp"

#include <memory>
#include <string>

namespace Ort {
class Env;
class Session;
class SessionOptions;
}

namespace va::analyzer {

class OrtModelSession : public IModelSession {
public:
    OrtModelSession();
    ~OrtModelSession();

    bool loadModel(const std::string& model_path, bool use_gpu);
    bool run(const core::TensorView& input, std::vector<core::TensorView>& outputs) override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    bool loaded_ {false};
};

} // namespace va::analyzer
