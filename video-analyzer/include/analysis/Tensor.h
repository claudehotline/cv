#pragma once

#include <vector>
#include <cstdint>

// Minimal float32 tensor abstraction for backend-agnostic inference
struct Tensor {
    std::vector<int64_t> shape;
    std::vector<float> data; // contiguous float32 data
};

