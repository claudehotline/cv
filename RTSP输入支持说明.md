# RTSP输入支持说明

## 📋 当前支持状态

视频源管理模块**完全支持**RTSP视频流作为输入源。

## ✅ 基础RTSP输入支持

### 配置方式
在配置文件中设置RTSP输入源：

```json
{
  "video_sources": [
    {
      "id": "ip_camera_01",
      "source_path": "rtsp://192.168.1.100:554/stream1",
      "type": "stream",
      "enabled": true,
      "fps": 25,
      "enable_rtsp": true,
      "rtsp_url": "rtsp://localhost:8554/ip_camera_01",
      "rtsp_port": 8554
    },
    {
      "id": "nvr_channel_02",
      "source_path": "rtsp://admin:password@192.168.1.200:554/cam2",
      "type": "stream",
      "enabled": true,
      "fps": 30,
      "enable_rtsp": true,
      "rtsp_url": "rtsp://localhost:8554/nvr_channel_02",
      "rtsp_port": 8554
    }
  ]
}
```

### HTTP API支持
通过新的HTTP API添加RTSP输入源：

```bash
# 添加RTSP视频源
curl -X POST http://localhost:8081/api/sources \
  -H "Content-Type: application/json" \
  -d '{
    "source_path": "rtsp://192.168.1.100:554/stream1",
    "type": "stream",
    "name": "IP摄像头01",
    "fps": 25,
    "enable_rtsp": true
  }'
```

## 🔧 工作原理

### 数据流路径
```
RTSP输入源 → OpenCV VideoCapture → 帧处理 → RTSP推流 → 视频分析模块
```

### 典型使用场景

1. **IP摄像头接入**
   - 网络摄像头RTSP流接入
   - NVR/DVR系统通道接入
   - 第三方流媒体服务器接入

2. **流媒体转发**
   - 接收外部RTSP流
   - 转码并推流给分析模块
   - 实现流媒体代理功能

## 🚀 增强版RTSP输入处理器

我刚创建了增强版的`RTSPInputHandler`类，提供更强大的功能：

### 主要特性
- **自动重连**: 网络中断自动恢复
- **连接监控**: 实时连接状态监控
- **性能统计**: FPS、延迟、丢帧统计
- **错误处理**: 详细错误信息和回调
- **缓冲控制**: 可配置的缓冲策略

### 使用示例
```cpp
// 配置RTSP输入
RTSPInputHandler::RTSPConfig config;
config.rtsp_url = "rtsp://192.168.1.100:554/stream1";
config.timeout_ms = 5000;
config.max_reconnect_attempts = 3;
config.enable_buffering = false;  // 低延迟模式

// 创建处理器
RTSPInputHandler rtsp_handler(config);

// 设置帧回调
rtsp_handler.setFrameCallback([](const cv::Mat& frame) {
    // 处理接收到的帧
    std::cout << "接收到帧: " << frame.cols << "x" << frame.rows << std::endl;
});

// 设置状态回调
rtsp_handler.setStatusCallback([](bool connected, const std::string& error) {
    if (connected) {
        std::cout << "RTSP连接成功" << std::endl;
    } else {
        std::cout << "RTSP连接失败: " << error << std::endl;
    }
});

// 启动处理
rtsp_handler.initialize();
rtsp_handler.start();
```

## 📊 支持的RTSP特性

### URL格式支持
- `rtsp://ip:port/path`
- `rtsp://username:password@ip:port/path`
- `rtsp://ip:port/path?param=value`

### 编码格式支持
通过OpenCV VideoCapture支持的所有格式：
- H.264/AVC
- H.265/HEVC
- MJPEG
- MPEG-4

### 分辨率支持
- 标清: 640x480, 720x576
- 高清: 1280x720, 1920x1080
- 4K: 3840x2160 (性能允许的情况下)

## ⚡ 性能考虑

### 延迟优化
- 禁用VideoCapture缓冲: `CAP_PROP_BUFFERSIZE = 1`
- 设置超时参数: `CAP_PROP_READ_TIMEOUT_MSEC`
- 使用低延迟模式

### 资源管理
- 每个RTSP流独立线程处理
- 自动释放断开的连接
- 内存使用监控

### 网络优化
- TCP/UDP传输协议自适应
- 网络中断自动重连
- 连接超时配置

## 🔍 监控和调试

### 状态监控
```cpp
auto stats = rtsp_handler.getStats();
std::cout << "连接状态: " << (stats.is_connected ? "已连接" : "断开") << std::endl;
std::cout << "接收帧数: " << stats.frames_received << std::endl;
std::cout << "当前FPS: " << stats.current_fps << std::endl;
std::cout << "丢帧数: " << stats.frames_dropped << std::endl;
std::cout << "重连次数: " << stats.reconnect_count << std::endl;
```

### HTTP API监控
```bash
# 获取RTSP状态
curl http://localhost:8081/api/sources/ip_camera_01/stats

# 获取系统信息
curl http://localhost:8081/api/system/info
```

## 🛠️ 故障排除

### 常见问题

1. **连接超时**
   - 检查网络连通性
   - 验证RTSP URL正确性
   - 增加超时时间

2. **认证失败**
   - 确认用户名密码正确
   - 检查摄像头认证设置

3. **编码不支持**
   - 检查OpenCV编译参数
   - 尝试不同的编码格式

4. **性能问题**
   - 降低分辨率或帧率
   - 检查网络带宽
   - 启用硬件加速

## 📝 配置建议

### 生产环境配置
```json
{
  "timeout_ms": 10000,
  "max_reconnect_attempts": 5,
  "reconnect_delay_ms": 3000,
  "enable_buffering": false,
  "buffer_size": 1
}
```

### 开发测试配置
```json
{
  "timeout_ms": 3000,
  "max_reconnect_attempts": 2,
  "reconnect_delay_ms": 1000,
  "enable_buffering": true,
  "buffer_size": 3
}
```

---

**总结**: 视频源管理模块完全支持RTSP输入，具备基础功能和增强功能，可以满足各种RTSP视频流接入需求。