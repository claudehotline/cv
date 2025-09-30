#pragma once
#include <string>

// 从 JSON 配置中读取模型路径（如果缺失则给出合理默认）
// 返回 true 表示成功解析，false 表示使用了默认值或解析失败
bool LoadModelPaths(const std::string& config_file, std::string& detection_path, std::string& segmentation_path);

