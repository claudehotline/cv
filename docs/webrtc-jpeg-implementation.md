# WebRTC JPEG视频流实现说明

## 概述

本项目实现了基于WebRTC Data Channel的JPEG视频流传输功能，通过将视频帧编码为JPEG格式并通过数据通道传输，实现了低延迟的视频流播放。

## 架构设计

### 整体架构

```
video-analyzer (C++)          WebRTC           web-frontend (Vue 3)
┌─────────────────┐         Data Channel       ┌─────────────────┐
│ WebRTCStreamer  │ ────────────────────────→ │ WebRTCClient    │
│   - JPEG编码    │                           │   - JPEG解码    │
│   - 数据通道    │                           │   - Canvas显示  │
│   - 帧率控制    │                           │   - 检测叠加    │
└─────────────────┘                           └─────────────────┘
```

### 关键组件

#### 后端 (video-analyzer)

1. **WebRTCStreamer** (`src/WebRTCStreamer.cpp`)
   - 基于libdatachannel的WebRTC实现
   - JPEG视频编码和数据通道传输
   - 客户端连接管理

2. **WebRTCVideoSource** (`src/WebRTCStreamer.cpp`)
   - OpenCV JPEG编码
   - 帧率控制 (~30 FPS)
   - 自动分辨率调整 (最大640x480)

#### 前端 (web-frontend)

1. **WebRTCClient** (`src/utils/webrtc.ts`)
   - WebRTC信令处理
   - JPEG数据通道接收
   - 数据重组和验证

2. **JpegVideoPlayer** (`src/components/JpegVideoPlayer.vue`)
   - Canvas-based JPEG显示
   - 检测结果叠加
   - 性能统计和控制

## 数据传输协议

### JPEG帧格式

每个JPEG帧通过数据通道传输时的格式：

```
+---+---+---+---+---------------------------+
| Size (4 bytes)  |    JPEG Data            |
| (Big Endian)    |    (Variable length)    |
+---+---+---+---+---------------------------+
```

- **Size**: 32位大端序整数，表示JPEG数据大小
- **JPEG Data**: 完整的JPEG图像数据

### 分块传输

对于大于16KB的JPEG帧，采用分块传输：

1. 首先发送4字节大小头部
2. 然后分16KB块发送JPEG数据
3. 前端自动重组完整帧

## 功能特性

### 视频编码 (后端)

- **格式**: JPEG压缩，质量75%
- **分辨率**: 自动调整，最大640x480
- **帧率**: 可配置，默认30 FPS
- **优化**: 启用JPEG优化标志

### 视频解码 (前端)

- **接收**: WebRTC数据通道
- **验证**: JPEG文件头验证 (0xFF 0xD8)
- **显示**: Canvas绘制，保持宽高比
- **缓冲**: 自动处理分块数据重组

### 检测结果叠加

- **实时显示**: 在视频帧上叠加检测框
- **自适应缩放**: 检测框坐标自动适配显示尺寸
- **样式定制**: 可配置颜色、透明度等

## 使用方法

### 启动后端服务

```bash
cd video-analyzer/build
./VideoAnalyzer
```

服务监听端口：
- HTTP API: 8082
- WebRTC信令: 8083

### 启动前端应用

```bash
cd web-frontend
npm run dev
```

访问测试页面：
- 主界面: http://localhost:30000/video-analysis
- JPEG 测试: http://localhost:30000/jpeg-test

### 测试流程

1. **连接WebRTC**: 点击"连接WebRTC"按钮
2. **请求视频流**: 连接成功后点击"请求视频流"
3. **查看视频**: 在JPEG播放器中观看实时视频
4. **监控统计**: 查看FPS、延迟等性能指标

## 性能优化

### 编码优化

- **分辨率限制**: 640x480最大分辨率减少数据量
- **质量平衡**: JPEG质量75%平衡文件大小和质量
- **帧率控制**: 30 FPS避免过高CPU使用

### 传输优化

- **分块传输**: 16KB分块提高可靠性
- **大端序**: 统一字节序避免解析错误
- **数据验证**: JPEG头部验证确保数据完整性

### 显示优化

- **Canvas渲染**: 高效的图像显示
- **帧缓冲**: 避免重复创建Image对象
- **内存管理**: 及时释放Blob URL

## 故障排除

### 常见问题

1. **连接失败**
   ```
   检查后端服务是否启动
   确认端口8083可访问
   查看浏览器控制台错误
   ```

2. **无视频显示**
   ```
   检查WebRTC连接状态
   验证数据通道是否打开
   查看JPEG数据是否接收
   ```

3. **视频卡顿**
   ```
   检查网络延迟
   调整JPEG质量设置
   验证帧率配置
   ```

### 调试信息

前端提供详细的调试日志：
- WebRTC连接状态
- 数据通道状态
- JPEG帧接收统计
- 性能指标监控

## 扩展功能

### 已实现功能

- ✅ JPEG视频流传输
- ✅ 实时检测结果叠加
- ✅ 性能统计和监控
- ✅ 帧保存功能
- ✅ 连接状态监控

### 可扩展功能

- 🔄 多客户端支持
- 🔄 视频录制功能
- 🔄 音频传输支持
- 🔄 自适应码率控制
- 🔄 P2P直连优化

## 技术细节

### 依赖库

**后端 (C++)**:
- libdatachannel: WebRTC实现
- OpenCV: 图像处理和JPEG编码
- jsoncpp: JSON数据处理

**前端 (TypeScript/Vue 3)**:
- WebRTC API: 浏览器原生WebRTC支持
- Canvas API: 图像显示
- Element Plus: UI组件

### 配置参数

**WebRTCStreamer配置**:
```cpp
// JPEG质量 (0-100)
const int JPEG_QUALITY = 75;

// 最大分辨率
const int MAX_WIDTH = 640;
const int MAX_HEIGHT = 480;

// 分块大小
const size_t MAX_CHUNK_SIZE = 16384;

// 目标帧率
const int TARGET_FPS = 30;
```

**WebRTCClient配置（开发模式经由代理）**:
```typescript
const config: WebRTCConfig = {
  signalingServerUrl: `${window.location.protocol === 'https:' ? 'wss://' : 'ws://'}${window.location.host}/signaling`,
  stunServers: [] // 本地连接不使用STUN
}
```

## 总结

本实现提供了完整的WebRTC JPEG视频流解决方案，具有以下优势：

1. **低延迟**: 基于WebRTC的实时传输
2. **高兼容性**: 标准WebRTC API支持
3. **易扩展**: 模块化设计便于功能扩展
4. **高性能**: 优化的编码和传输策略
5. **易调试**: 完善的日志和监控功能

该实现适用于需要实时视频传输和分析的应用场景，如视频监控、实时分析、远程诊断等。
