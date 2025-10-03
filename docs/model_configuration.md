# 模型配置说明

本文档记录当前 `config/models.yaml` 结构及占位策略，便于后续扩展分割能力。

## 检测模型

- `det:yolo:*` 条目对应目标检测模型，当前部署的 `yolov12l`、`yolov12x`、`yolov8n` 均已实装。
- `Application::loadModel` 与 `/api/models/load` 会将相应模型标记为当前任务默认模型，并驱动正在运行的管线热切换。

## 分割模型占位

- `seg:yolo:*` 保留为分割任务的配置占位，当前仓库未提供对应 ONNX 模型文件。
- 当配置缺少实际模型文件时，管线创建会失败并向 REST 层返回 `model not found`，以免误以为分割模型已工作。
- `YoloSegmentationPostprocessor` 与 `PassthroughRenderer` 仍保留，可在未来提供分割模型后直接补齐。当前逻辑对缺失模型做防护，不会影响检测通道。

## 建议

1. **保持占位条目**：前端需要这些条目来展示分割选项；无模型时返回的错误可作为提示。
2. **补齐模型后**：只需将实际的 `yolov12s-seg.onnx` 等文件放入 `video-analyzer/model/`，无需改动代码即可启用。
3. **配置扩展**：新增任务时沿用 `task: family: variant:` 的层级结构，`Application::resolveModel` 会自动解析。

如需调整或移除占位，请同步更新前端下拉列表及 `/api/models` 展示逻辑，避免出现空模型项。
