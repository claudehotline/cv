# WebRTC 测试指南

## 测试环境配置

### 1. 启动 video-analyzer 后端服务

```bash
cd D:\Projects\ai\cv\video-analyzer\build\bin\Release
VideoAnalyzer.exe
```

确保看到以下输出：
- WebRTC Streamer initialized with libdatachannel on port 8080
- WebRTC Signaling Server started on port: 8083
- HTTP服务器已启动，端口: 8082

### 2. 启动 web-frontend 前端

```bash
cd D:\Projects\ai\cv\web-frontend
npm install  # 首次运行需要安装依赖
npm run dev
```

前端会在 http://localhost:30000 启动（见 vite.config.ts）

## WebRTC 功能测试步骤

### 1. 基础连接测试

1. 打开浏览器访问 http://localhost:30000
2. 点击 "VideoStreams" 页面
3. 检查页面底部的连接状态：
   - WebSocket连接状态应显示 "已连接"
   - WebRTC状态应显示 "WebRTC已连接"

### 2. 视频流测试

1. 在左侧面板选择一个视频源（如 "摄像头 1" 或 "测试视频"）
2. 点击 "开始分析" 按钮
3. 在右侧视频预览区域应该能看到：
   - WebRTC连接指示器显示绿色 "WebRTC已连接"
   - 视频画面开始播放（模拟视频流）

### 3. 信令协商测试

通过浏览器开发者工具 (F12) 观察网络活动：

1. **WebRTC 信令连接**（经由 Vite 代理）: ws://localhost:30000/signaling
   - 认证消息: `{"type":"auth","data":{"client_type":"web_client"}}`
   - 认证成功: `{"type":"auth_success","client_id":"client_123456"}`
   - 请求视频流: `{"type":"request_offer"}`
   - SDP Offer/Answer 交换
   - ICE 候选者交换

## 预期行为

### 成功的连接序列：

1. 前端连接到信令服务器 (端口8083)
2. 发送认证消息并收到成功响应
3. 请求视频流 (`request_offer`)
4. 后端创建 WebRTC offer 并发送给前端
5. 前端处理 offer 并发送 answer
6. ICE 候选者交换完成连接建立
7. 视频流开始传输

### 故障排除

如果连接失败，检查：

1. **端口占用**: 确保 8082, 8083, 8080 端口未被其他程序占用
2. **防火墙**: Windows防火墙可能阻止连接
3. **浏览器控制台**: 查看错误信息
4. **后端日志**: 观察 VideoAnalyzer.exe 的控制台输出

## 技术实现说明

### 后端 (C++ libdatachannel)
- **WebRTCStreamer**: 处理 P2P 视频流传输
- **SignalingServer**: 基于 ixwebsocket 的信令服务器
- **端口分配**:
  - 8080: WebRTC P2P 连接
  - 8082: HTTP API 服务器
  - 8083: WebRTC 信令服务器

### 前端 (Vue.js + WebRTC API)
- **WebRTCClient**: 封装 WebRTC 连接逻辑
- **VideoStore**: Pinia 状态管理
- **VideoStreams.vue**: 视频显示组件

## 下一步

测试成功后，可以：
1. 连接真实摄像头或视频文件
2. 集成 AI 分析结果显示
3. 添加多客户端支持测试
4. 性能优化和错误处理改进
