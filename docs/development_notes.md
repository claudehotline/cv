# 开发说明

本文档汇总视频分析后端在依赖管理、日志策略与回归检查方面的实践，方便团队成员快速对齐开发方式。

## 构建与依赖

- **FFmpeg**：仓库不再携带官方二进制，请在本地准备与运行环境一致的共享库，并通过 `PATH` 或 `config/app.yaml` 中的 `defaults.encoder` 保证可被动态加载。`third_party/ffmpeg/` 仅保留获取脚本，不放置可执行文件。
- **yaml-cpp**：优先使用仓库 `third_party/yaml-cpp` 下的源码构建；若切换到系统库，请同步更新 `video-analyzer/CMakeLists.txt` 内的 `YAML_LOCAL_ROOT` 配置。
- **ONNX Runtime / TensorRT**：自编译的 GPU 版建议放置在 `D:\Projects\ai\cv\build\onnxruntime\build\Windows\Release\Release`，并设置 `ONNXRUNTIME_ROOT` 及 TensorRT `lib` 目录到系统 `PATH`。需要 CUDA 12.9 及匹配的 NVIDIA 驱动。

## 日志与观测配置

`config/app.yaml` 的 `observability` 段用于统一日志输出与指标采样：

```yaml
observability:
  log_level: info        # trace/debug/info/warn/error
  console: true          # 是否输出到控制台
  file:
    path: logs/video-analyzer.log
    max_size_kb: 10240   # 单文件最大 10MB
    max_files: 5         # 轮转文件数
  pipeline_metrics:
    enabled: true
    interval_ms: 5000    # 聚合指标刷新周期
```

后端代码已经切换为 `VA_LOG_*` 宏；即使未启用文件输出，也会统一走 Logger 管线，便于部署时快速接入。

## 检测日志分析

运行时可通过 `VideoAnalyzer.exe` 的标准输出收集检测统计（示例 `va-output.log`）。为了快速定位误报/漏报，可使用 `scripts/analyze_detection_log.py`：

```powershell
python scripts/analyze_detection_log.py --log va-output.log --source-prefix camera_
```

脚本会统计每路流的帧数、平均检测框数量、0 检测占比及高于阈值的帧比例，可以配合 `--threshold 15`、`--top 10` 等参数聚焦异常时段。

## REST API 回归检查

`scripts/check_analysis_api.py` 用于快速验证后端 REST 接口是否返回成功响应（需要先启动 `VideoAnalyzer.exe`）：

```powershell
python scripts/check_analysis_api.py --base http://127.0.0.1:8082
```

脚本会访问 `/api/system/info`、`/api/system/stats`、`/api/models`、`/api/profiles`、`/api/pipelines` 等端点，确保状态码为 200 且返回 `{ "success": true, ... }`。如需自定义地址或超时时间，可使用 `--base`、`--timeout` 参数。

对于关键的订阅流程，可执行：

```powershell
python scripts/check_subscription_flow.py --base http://127.0.0.1:8082 --url rtsp://127.0.0.1:8554/camera_01
```

该脚本会自动挑选 profile，依次完成 `/api/subscribe`、`/api/pipelines` 校验与 `/api/unsubscribe`，验证新增管线和清理是否生效。若实际环境使用不同的 RTSP/模型参数，可通过 `--profile`、`--model`、`--url` 自行指定。

## 参考资料

- `docs/model_configuration.md`：模型与 Profile 配置说明。
- `docs/api_endpoints.md`：REST API 文档（旧的 `/api/analysis/*` 已废弃）。
- `深度重构计划.txt`：阶段性重构目标与 TODO 列表。

如需扩展功能或调整流程，请同步更新本文档，保持依赖与观测策略一致。
