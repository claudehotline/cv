# REST API 概览

本节列出当前可用的 HTTP 接口。早期版本中的 `/api/analysis/*`、`/api/sources*` 等旧端点已移除或不再注册，请以此文档为准。

## 模型

- `GET /api/models`：返回所有检测/分割配置条目、当前激活状态及正在使用的管线数量。
- `POST /api/models/load`：激活指定模型，负载格式：`{"model_id": "det:yolo:v12l"}`。
  - 若模型不存在或切换失败，返回详细错误信息和对应的 4xx/5xx 状态码。

## 管线

- `POST /api/subscribe`：创建或复用管线，负载示例：
  ```json
  { "stream": "camera_01", "profile": "det_720p", "url": "rtsp://127.0.0.1:8554/camera_01" }
  ```
  可选字段 `model_id` 用于强制指定模型。
- `POST /api/unsubscribe`：停止指定管线，负载：`{"stream": "camera_01", "profile": "det_720p"}`。
- `GET /api/pipelines`：列出所有活跃管线（含推理 FPS、延迟、编码参数和传输统计）。

## 配置与状态

- `GET /api/profiles`：查看所有 profile（任务、模型默认值、编码/发布参数）。
- `GET /api/system/info`：返回整体配置概览（引擎类型、FFmpeg 状态、模型/管线数量等）。
- `GET /api/system/stats`：聚合运行指标（FPS 总和、累计帧数、网络流量等）。

> ⚠️ 任何未在上文列出的旧端点都已取消注册；HTTPServer 会以 `404` 返回“路径未找到”。
