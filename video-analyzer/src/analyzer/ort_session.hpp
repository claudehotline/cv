#pragma once

#include "analyzer/interfaces.hpp"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace va::analyzer {

class OrtModelSession : public IModelSession {
public:
#ifdef USE_ONNXRUNTIME
    struct Options {
        std::string provider {"cpu"};
        int device_id {0};
        bool use_io_binding {false};
        bool prefer_pinned_memory {true};
        bool allow_cpu_fallback {true};
        bool enable_profiling {false};
        bool tensorrt_fp16 {false};
        bool tensorrt_int8 {false};
        int tensorrt_workspace_mb {0};
        size_t io_binding_input_bytes {0};
        size_t io_binding_output_bytes {0};
    };

    void setOptions(const Options& options);
#endif

    OrtModelSession();
    ~OrtModelSession() override;

    bool loadModel(const std::string& model_path, bool use_gpu) override;
    bool run(const core::TensorView& input, std::vector<core::TensorView>& outputs) override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    bool loaded_ {false};
};

} // namespace va::analyzer
