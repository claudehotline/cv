# 基于机器视觉的智能视频分析系统

## 🎯 项目概述

本项目是一个完整的智能视频分析软件系统，采用现代化分布式架构设计，包含视频源管理、视频分析和Web前端展示三个核心模块。系统支持多视频源接入、实时AI分析处理，并提供直观的Web界面进行操作和结果展示。

## 🏗️ 系统架构

### 分布式服务架构 - HTTP REST API + RTSP + WebRTC

```
┌─────────────────┐    RTSP视频流    ┌─────────────────┐    WebRTC流     ┌─────────────────┐
│   视频源管理    │ ──────────────→ │   视频分析模块  │ ─────────────→ │   Vue3前端      │
│   HTTP服务      │   (8554端口)     │  + AI推理引擎   │   (8083端口)     │  + 状态管理     │
│  (端口8081)     │                 │  (端口8082)     │                 │                 │
└─────────────────┘                 └─────────────────┘                 └─────────────────┘
        ↑                                   ↑                                   ↑
        │                                   │                                   │
        │              HTTP REST API        │                                   │
        │            (JSON数据交换)         │                              WebRTC信令
        │                                   │                             (控制命令)
        └───────────────────────────────────┴─────────────────────────────────┘
                                  前端统一调用界面
```

### 🚀 架构优势

- **🔄 分布式设计**: 每个模块独立运行，可独立扩展和维护
- **⚡ 高性能数据流**: RTSP协议传输视频，HTTP REST API处理控制命令
- **🎯 标准化接口**: RESTful API设计，易于集成和调试
- **📈 可扩展性**: 支持负载均衡和多实例部署
- **🛡️ 稳定可靠**: 独立服务避免单点故障

## 📁 项目结构

```
video-analysis-system/
├── video-source-manager/          # 视频源管理模块 (端口8081)
│   ├── include/
│   │   ├── VideoSourceManager.h   # 视频源管理核心类
│   │   ├── RTSPStreamer.h         # RTSP推流服务器
│   │   ├── RTSPInputHandler.h     # RTSP输入处理器(增强版)
│   │   ├── HTTPServer.h           # HTTP服务器
│   │   └── SourceAPI.h            # 视频源管理API
│   ├── src/                       # 源代码实现
│   ├── config/                    # 配置文件
│   └── CMakeLists.txt            # 构建文件
├── video-analyzer/                # 视频分析模块 (端口8082)
│   ├── include/
│   │   ├── VideoAnalyzer.h        # 视频分析核心类
│   │   ├── SignalingServer.h      # WebRTC信令服务器
│   │   ├── HTTPServer.h           # HTTP服务器
│   │   └── AnalysisAPI.h          # 视频分析API
│   ├── src/                       # 源代码实现
│   ├── models/                    # AI模型文件
│   ├── config/                    # 配置文件
│   └── CMakeLists.txt            # 构建文件
├── web-frontend/                  # Web前端模块
│   ├── src/
│   │   ├── stores/
│   │   │   ├── videoSourceStore.ts  # 视频源管理Store
│   │   │   ├── analysisStore.ts     # 视频分析Store
│   │   │   └── appStore.ts          # 应用级协调Store
│   │   ├── components/            # Vue组件
│   │   └── utils/
│   │       └── webrtc.ts          # WebRTC工具类
│   ├── package.json               # NPM配置
│   └── vite.config.ts            # Vite配置
├── docs/                          # 项目文档
│   ├── API-分工说明.md            # API服务分工文档
│   ├── HTTP服务实现总结.md        # HTTP服务实现说明
│   └── RTSP输入支持说明.md        # RTSP功能说明
├── CMakeLists.txt                # 主构建文件
└── README.md                     # 项目说明
```

## ✨ 功能特性

### 🎥 视频源管理模块 (端口8081)
- ✅ **多源支持**: 摄像头、视频文件、RTSP流、网络流
- ✅ **RTSP输入**: 支持IP摄像头、NVR/DVR系统接入
- ✅ **RTSP推流**: FFmpeg集成，高质量视频流推送
- ✅ **HTTP API**: RESTful接口，支持视频源CRUD操作
- ✅ **状态监控**: 实时视频源状态和性能监控
- ✅ **配置管理**: JSON配置文件和动态配置支持
- 🆕 **增强RTSP处理**: 自动重连、错误恢复、性能统计

**主要API端点:**
```
GET    /api/sources              - 获取所有视频源
POST   /api/sources              - 添加新视频源
PUT    /api/sources/:id          - 更新视频源
DELETE /api/sources/:id          - 删除视频源
POST   /api/sources/:id/rtsp/start - 启动RTSP推流
POST   /api/sources/:id/rtsp/stop  - 停止RTSP推流
GET    /api/system/info          - 获取系统信息
```

### 🧠 视频分析模块 (端口8082)
- ✅ **AI模型管理**: ONNX Runtime支持，动态加载/卸载
- ✅ **实时分析**: 目标检测、实例分割
- ✅ **RTSP接收**: OpenCV VideoCapture接收视频流
- ✅ **多任务处理**: 并发分析多个视频源
- ✅ **HTTP API**: RESTful接口，支持分析控制
- ✅ **WebRTC推流**: 低延迟视频流传输(端口8083)
- ✅ **结果管理**: 分析结果存储和查询
- 🆕 **性能监控**: GPU使用率、内存监控、FPS统计

**主要API端点:**
```
GET    /api/models               - 获取可用模型
POST   /api/models/load          - 加载AI模型
POST   /api/analysis/start       - 启动分析任务
POST   /api/analysis/stop        - 停止分析任务
GET    /api/analysis/results     - 获取分析结果
GET    /api/system/stats         - 获取性能统计
```

### 🌐 Web前端模块
- ✅ **现代化架构**: Vue 3 + TypeScript + Vite
- ✅ **分布式调用**: 同时调用多个后端API服务
- ✅ **状态管理**: Pinia-based响应式状态管理
- ✅ **WebRTC集成**: 实时视频流接收和显示
- ✅ **统一工作流**: 自动化视频源到分析的完整流程
- ✅ **实时监控**: 系统状态、任务进度实时更新
- 🆕 **分离式Store**: videoSourceStore + analysisStore + appStore
- 🆕 **错误处理**: 统一错误处理和用户反馈

## 🛠️ 技术栈

### 后端服务 (C++)
- **OpenCV 4.5+**: 图像视频处理和RTSP支持
- **ONNX Runtime**: 深度学习模型推理
- **jsoncpp**: JSON数据处理
- **WebSocket++**: WebRTC信令通信
- **FFmpeg**: RTSP视频流处理
- 🆕 **自研HTTP服务器**: 轻量级HTTP REST API服务器
- **CMake**: 跨平台构建系统

### 前端应用
- **Vue 3**: 渐进式前端框架
- **TypeScript**: 类型安全的JavaScript
- **Vite**: 极速构建工具
- **Element Plus**: 企业级UI组件库
- **Pinia**: 新一代状态管理库
- **WebRTC API**: 浏览器原生实时通信
- 🆕 **Fetch API**: 现代HTTP请求处理

## 🚀 快速开始

### 💻 环境要求

**系统环境:**
- CMake 3.16+
- C++17编译器 (GCC 8+ / MSVC 2019+ / Clang 8+)
- Node.js 16+
- FFmpeg (RTSP功能需要)

**第三方依赖:**
- OpenCV 4.0+ (必需)
- ONNX Runtime (可选，AI推理)
- jsoncpp (必需)
- WebSocket++ (必需)

### 📦 安装步骤

1. **克隆项目**
```bash
git clone <repository-url>
cd video-analysis-system
```

2. **安装系统依赖**
```bash
# Ubuntu/Debian
sudo apt-get update
sudo apt-get install libopencv-dev libjsoncpp-dev ffmpeg

# Windows (使用 vcpkg)
vcpkg install opencv jsoncpp ixwebsocket libdatachannel boost-system

# macOS
brew install opencv jsoncpp ffmpeg
```

3. **构建C++服务**
```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

4. **安装前端依赖**
```bash
cd web-frontend
npm install
```

### 🏃‍♂️ 运行系统

1. **启动视频源管理服务** (端口8081)
```bash
cd build/bin
./VideoSourceManager
```
服务功能：
- 视频源管理和RTSP推流
- HTTP REST API服务
- RTSP输入流处理

2. **启动视频分析服务** (端口8082/8083)
```bash
cd build/bin
./VideoAnalyzer
```
服务功能：
- AI模型管理和推理
- HTTP REST API服务 (8082)
- WebRTC信令服务 (8083)

3. **启动Web前端** (端口30000)
```bash
cd web-frontend
npm run dev
```

4. **访问系统界面**
```
Web界面: http://localhost:30000
视频源API: http://localhost:8081/api
分析API: http://localhost:8082/api
```

### 前端开发代理说明

- 已在 `web-frontend/vite.config.ts` 配置以下代理：
  - `/api/analyzer` → `http://localhost:8082/api`
  - `/api/source-manager` → `http://localhost:8081/api`
  - `/signaling`（WS）→ `ws://localhost:8083`
- 建议前端使用上述代理路径访问后端与信令服务，避免跨域与端口暴露。
- WebRTC 信令推荐使用绝对 WS 地址，基于当前页面协议与主机构造：
  - `const ws = new WebSocket(`${location.protocol === 'https:' ? 'wss://' : 'ws://'}${location.host}/signaling`)`
```

### 🎯 典型使用流程

1. **配置视频源**: 添加摄像头或RTSP流
2. **启动推流**: 开启RTSP推流服务
3. **选择模型**: 加载AI分析模型
4. **开始分析**: 启动实时视频分析
5. **查看结果**: WebRTC实时视频 + 分析结果

## ⚙️ 配置说明

### 📹 视频源配置示例
```json
{
  "video_sources": [
    {
      "id": "local_camera",
      "source_path": "0",
      "type": "camera",
      "enabled": true,
      "fps": 30,
      "enable_rtsp": true,
      "rtsp_port": 8554
    },
    {
      "id": "ip_camera_01",
      "source_path": "rtsp://192.168.1.100:554/stream1",
      "type": "stream",
      "enabled": true,
      "fps": 25,
      "enable_rtsp": true
    }
  ]
}
```

### 🤖 AI模型配置示例
```json
{
  "models": {
    "yolov5": {
      "type": "onnx",
      "path": "models/yolov5s.onnx",
      "confidence_threshold": 0.5
    }
  },
  "analysis": {
    "num_workers": 4,
    "gpu_enabled": true
  }
}
```

## 🔧 API使用示例

### 视频源管理API
```bash
# 添加RTSP视频源
curl -X POST http://localhost:8081/api/sources \
  -H "Content-Type: application/json" \
  -d '{
    "source_path": "rtsp://192.168.1.100:554/stream1",
    "type": "stream",
    "name": "IP摄像头01",
    "enable_rtsp": true
  }'

# 启动RTSP推流
curl -X POST http://localhost:8081/api/sources/camera_01/rtsp/start

# 获取系统状态
curl http://localhost:8081/api/system/info
```

### 视频分析API
```bash
# 加载AI模型
curl -X POST http://localhost:8082/api/models/load \
  -H "Content-Type: application/json" \
  -d '{"model_id": "yolov5"}'

# 启动分析任务
curl -X POST http://localhost:8082/api/analysis/start \
  -H "Content-Type: application/json" \
  -d '{
    "source_id": "camera_01",
    "model_id": "yolov5",
    "analysis_type": "object_detection"
  }'

# 获取分析结果
curl http://localhost:8082/api/analysis/results?limit=10
```

## 📊 监控和调试

### 系统状态监控
- **服务健康状态**: HTTP健康检查端点
- **资源使用情况**: CPU、内存、GPU监控
- **网络连接状态**: RTSP连接、WebRTC状态
- **处理性能指标**: FPS、延迟、错误率

### 日志和调试
- **结构化日志**: JSON格式日志输出
- **多级别日志**: DEBUG、INFO、WARNING、ERROR
- **性能追踪**: 处理时间、资源占用分析
- **错误追踪**: 异常堆栈和恢复机制

## 🚀 部署和优化

### Docker容器化部署
```dockerfile
# 多阶段构建
FROM ubuntu:20.04 as builder
RUN apt-get update && apt-get install -y cmake g++ libopencv-dev
COPY . /app
RUN cd /app && mkdir build && cd build && cmake .. && make

FROM ubuntu:20.04
RUN apt-get update && apt-get install -y libopencv4.2
COPY --from=builder /app/build/bin /usr/local/bin
EXPOSE 8081 8082 8083
```

### 性能优化建议
- **GPU加速**: 启用CUDA/OpenCL加速AI推理
- **并发处理**: 调整worker线程池大小
- **内存优化**: 配置视频缓冲区和模型缓存
- **网络优化**: RTSP缓冲策略和WebRTC配置

## 🔍 故障排除

### 常见问题解决

**🔗 服务连接问题**
```bash
# 检查服务状态
curl http://localhost:8081/api/system/info
curl http://localhost:8082/api/system/info

# 检查端口占用
netstat -tulpn | grep -E "8081|8082|8083"
```

**📹 RTSP流问题**
```bash
# 测试RTSP连接
ffmpeg -i rtsp://192.168.1.100:554/stream1 -f null -

# 检查RTSP推流状态
curl http://localhost:8081/api/sources/camera_01/rtsp/status
```

**🤖 AI模型问题**
- 检查模型文件路径和格式
- 验证ONNX Runtime版本兼容性
- 确认GPU驱动和CUDA版本

## 📈 扩展开发

### 添加新的AI模型
1. 将ONNX模型文件放置到`models/`目录
2. 通过API加载模型：`POST /api/models/load`
3. 配置模型参数和阈值
4. 在分析任务中使用新模型

### 自定义视频处理
1. 继承`VideoSource`类实现自定义视频源
2. 实现`initialize()`和`captureLoop()`方法
3. 注册到`VideoSourceManager`中
4. 通过API管理自定义视频源

### 扩展前端功能
1. 创建新的Vue组件和页面
2. 添加对应的Pinia Store
3. 集成新的API调用
4. 更新路由和导航

## 📄 许可证

本项目采用 MIT 许可证 - 详见 [LICENSE](LICENSE) 文件。

## 🤝 贡献指南

欢迎贡献代码和建议！请遵循以下流程：

1. Fork本项目
2. 创建功能分支 (`git checkout -b feature/AmazingFeature`)
3. 提交更改 (`git commit -m 'Add some AmazingFeature'`)
4. 推送到分支 (`git push origin feature/AmazingFeature`)
5. 创建Pull Request

## 👥 开发团队

本项目由专业的软件开发团队开发，采用现代化的开发流程和质量标准：

- **🔍 系统分析师**: 需求分析与系统架构设计
- **🏗️ 软件架构师**: 技术架构与分布式系统设计
- **🎨 前端工程师**: Vue3+TypeScript现代化前端开发
- **⚙️ 后端工程师**: C++高性能服务端开发

## 📞 联系方式

如有问题或建议，请通过以下方式联系：

- 📧 Email: team@videoanalysis.com
- 🐛 Issues: [GitHub Issues](https://github.com/your-org/video-analysis-system/issues)
- 📖 文档: [项目Wiki](https://github.com/your-org/video-analysis-system/wiki)
- 💬 讨论: [GitHub Discussions](https://github.com/your-org/video-analysis-system/discussions)

---

**⭐ 如果这个项目对您有帮助，请考虑给它一个星标！**
