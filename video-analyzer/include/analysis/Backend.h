#pragma once

#include <string>
#include <vector>
#include <memory>

#include "analysis/Tensor.h"

class IInferenceBackend {
public:
    virtual ~IInferenceBackend() = default;

    virtual bool loadModel(const std::string& model_path) = 0;
    virtual bool infer(const std::vector<Tensor>& inputs, std::vector<Tensor>& outputs) = 0;

    virtual std::vector<std::string> getInputNames() const = 0;
    virtual std::vector<std::string> getOutputNames() const = 0;
};

