# Development Notes

This document gathers everyday guidance for the `video-analyzer` backend:
dependencies, logging knobs, and helper scripts that keep regressions in check.

## Runtime dependencies

- **FFmpeg** – Only helper scripts live under
  `third_party/ffmpeg-prebuilt`. Ensure the FFmpeg runtime DLLs (e.g.
  `avcodec-62.dll`, `avformat-62.dll`) are on `PATH` or co-located with
  `VideoAnalyzer.exe`.
- **yaml-cpp** – CMake prefers the local checkout at
  `third_party/yaml-cpp/yaml-cpp-0.8.0`. Switch to the system package by setting
  `YAML_LOCAL_ROOT` accordingly.
- **ONNX Runtime + TensorRT** – GPU builds rely on the custom ORT 1.23.0 output
  in `build/onnxruntime/build/Windows/Release/Release` and TensorRT 10.x (`lib`
  folder on `PATH`). Tested against CUDA 12.9 drivers.

### Engine configuration snippets

```yaml
engine:
  type: ort-trt        # ort-cpu | ort-cuda | ort-trt
  device: 0
  options:
    trt_fp16: true
    trt_int8: false
    trt_workspace_mb: 4096
    trt_max_partition_iterations: 100
    trt_min_subgraph_size: 1
    use_io_binding: true
    prefer_pinned_memory: true
```

Leave the TensorRT keys unset (or zero) when running pure CUDA/CPU.

## Logging & observability

The logging policy lives in `config/app.yaml`:

```yaml
observability:
  log_level: info
  console: true
  file:
    path: logs/video-analyzer.log
    max_size_kb: 10240
    max_files: 5
  pipeline_metrics:
    enabled: true
    interval_ms: 5000
```

All console output is redirected through the `VA_LOG_*` helpers, so flipping
`console` to `false` silences the binary without code changes.

## Regression helpers

- `python scripts/check_analysis_api.py --base http://127.0.0.1:8082`
  – Smoke test the REST surface (`/api/system/info`, `/api/models`, …).
- `python scripts/check_subscription_flow.py --base http://127.0.0.1:8082 --url rtsp://127.0.0.1:8554/camera_01`
  – Creates/destroys a pipeline and verifies `/api/pipelines` updates.
- `python scripts/check_gpu_inference.py --base http://127.0.0.1:8082`
  – Validates the `engine_runtime` block (`provider`, `gpu_active`, `io_binding`,
    `device_binding`, `cpu_fallback`).
    - `--expect-provider tensorrt` 断言当前执行 EP；默认输出 `cuda` 或
      `tensorrt`。
    - `--url ... --require-gpu --require-device-binding` 会执行短暂订阅，确认
      GPU IoBinding/设备缓冲状态在运行中被刷新。
- `python scripts/analyze_detection_log.py --log logs/video-analyzer.log`
  – Parses the main log for per-stream FPS/latency anomalies.

## Reference docs

- `docs/model_configuration.md` – Model/profile schema.
- `docs/api_endpoints.md` – Supported REST endpoints and response schema.
- `docs/深度重构计划.txt` – Stage-by-stage refactor plan (Chinese).

Update this note whenever we add new tooling or change the recommended
debugging workflow. Keep it short and actionable.
