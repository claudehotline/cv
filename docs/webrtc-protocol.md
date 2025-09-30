# WebRTC通信协议文档

## 概述

本文档描述了视频分析系统中 WebRTC 实时视频传输的通信协议。系统采用 WebSocket 进行信令交换，当前实现通过 WebRTC Data Channel 传输 JPEG 帧进行视频预览（不创建媒体 Track）。

## 架构组成

- **视频分析模块**: WebRTC视频流推送端
- **信令服务器**: 协调WebRTC连接建立
- **Vue3前端**: WebRTC视频流接收端

## 信令协议

### 连接建立

#### 1. 客户端认证
```json
{
  "type": "auth",
  "data": {
    "client_type": "web_client",
    "client_id": "web_1234567890"
  },
  "timestamp": 1640995200000
}
```

#### 2. 认证成功响应
```json
{
  "type": "auth_success",
  "client_id": "web_1234567890",
  "message": "认证成功"
}
```

### WebRTC信令交换

#### 3. 请求视频流
```json
{
  "type": "request_offer",
  "timestamp": 1640995200000
}
```

#### 4. SDP Offer
```json
{
  "type": "offer",
  "client_id": "web_1234567890",
  "data": {
    "type": "offer",
    "sdp": "v=0\r\no=- 123456789 123456789 IN IP4 127.0.0.1\r\n..."
  }
}
```

#### 5. SDP Answer
```json
{
  "type": "answer",
  "data": {
    "type": "answer",
    "sdp": "v=0\r\no=- 987654321 987654321 IN IP4 127.0.0.1\r\n..."
  },
  "timestamp": 1640995200000
}
```

#### 6. ICE Candidate
```json
{
  "type": "ice_candidate",
  "data": {
    "candidate": "candidate:1 1 UDP 2113667327 192.168.1.100 54400 typ host",
    "sdpMid": "0",
    "sdpMLineIndex": 0
  },
  "timestamp": 1640995200000
}
```

## 控制消息

### 分析控制

#### 开始分析
```json
{
  "type": "start_analysis",
  "data": {
    "source_id": "camera_01",
    "analysis_type": "object_detection"
  },
  "timestamp": 1640995200000
}
```

#### 停止分析
```json
{
  "type": "stop_analysis",
  "data": {
    "source_id": "camera_01"
  },
  "timestamp": 1640995200000
}
```

### 分析结果

#### 分析结果通知
```json
{
  "type": "analysis_result",
  "source_id": "camera_01",
  "request_id": 12345,
  "analysis_type": "object_detection",
  "timestamp": 1640995200000,
  "detections": [
    {
      "class_name": "person",
      "confidence": 0.92,
      "bbox": {
        "x": 100,
        "y": 150,
        "width": 80,
        "height": 200
      }
    }
  ]
}
```

## WebRTC配置

### STUN服务器
```json
{
  "stun_servers": [
    "stun:stun.l.google.com:19302",
    "stun:stun1.l.google.com:19302"
  ]
}
```

### 视频传输配置（当前实现）
- 视频传输: DataChannel 传输 JPEG 帧（分块 16KB，首 4 字节为帧大小，大端序）
- 媒体 Track: 不使用（ontrack 仅用于兼容日志）
- 音频: 禁用
- 帧率: 约 30 FPS

## 端口分配

| 服务 | 端口 | 协议 | 用途 |
|------|------|------|------|
| WebRTC 推流 | 8080 | UDP/TCP | P2P 连接（DataChannel） |
| Analyzer HTTP API | 8082 | HTTP | 分析/模型 REST 接口 |
| WebRTC 信令 | 8083 | WebSocket | 信令交换与控制消息 |
| Web 前端 (Vite) | 30000 | HTTP | 开发服务器 |

开发建议：前端在开发模式下通过 Vite 代理连接信令服务，使用路径 `/signaling`（Vite 已将其代理到 `ws://localhost:8083`）。前端应根据当前页面协议构造绝对 WS 地址，例如：

```
const wsScheme = location.protocol === 'https:' ? 'wss://' : 'ws://'
const signalingUrl = `${wsScheme}${location.host}/signaling`
```

## 错误处理

### 信令错误
```json
{
  "type": "error",
  "error_code": "SIGNALING_ERROR",
  "message": "信令交换失败",
  "details": "ICE candidate添加失败"
}
```

### WebRTC连接错误
```json
{
  "type": "webrtc_error",
  "error_code": "CONNECTION_FAILED",
  "message": "WebRTC连接建立失败",
  "client_id": "web_1234567890"
}
```

## 连接状态管理

### 状态枚举
- `connecting`: 正在建立连接
- `connected`: 连接已建立
- `disconnected`: 连接已断开
- `failed`: 连接失败

### 状态通知
```json
{
  "type": "connection_status",
  "client_id": "web_1234567890",
  "status": "connected",
  "timestamp": 1640995200000
}
```

## 性能监控

### 统计信息请求
```json
{
  "type": "get_stats",
  "client_id": "web_1234567890"
}
```

### 统计信息响应
```json
{
  "type": "stats",
  "client_id": "web_1234567890",
  "data": {
    "connected_clients": 3,
    "frames_sent": 12345,
    "avg_fps": 29.5,
    "bytes_sent": 1048576,
    "bitrate_kbps": 1850,
    "packet_loss": 0.01
  }
}
```

## 安全考虑

1. **认证**: 所有客户端必须通过认证才能建立WebRTC连接
2. **HTTPS**: 生产环境建议使用HTTPS和WSS
3. **TURN服务器**: 对于NAT穿透，可配置TURN服务器
4. **访问控制**: 限制WebRTC连接的客户端数量

## 故障排除

### 常见问题

1. **WebRTC连接失败**
   - 检查防火墙UDP端口是否开放
   - 验证STUN服务器可访问性
   - 检查NAT配置

2. **视频流不显示**
   - 确认浏览器WebRTC支持
   - 检查video元素配置
   - 验证视频编解码器支持

3. **信令连接失败**
   - 检查WebSocket连接
   - 验证信令服务器状态
   - 确认端口8083可访问

## 示例代码

### 前端WebRTC连接
```typescript
const peerConnection = new RTCPeerConnection({
  iceServers: [
    { urls: 'stun:stun.l.google.com:19302' }
  ]
});

peerConnection.ontrack = (event) => {
  videoElement.srcObject = event.streams[0];
};
```

### 后端WebRTC推流
```cpp
// 创建WebRTC会话
auto peer_connection = peer_connection_factory_->CreatePeerConnection(
    config, nullptr, nullptr, observer.get());

// 添加视频轨道
auto result = peer_connection->AddTrack(video_track_, {"stream_id"});
```
