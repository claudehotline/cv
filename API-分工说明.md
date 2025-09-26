# API 服务分工说明

## 服务架构概述

系统采用分布式API架构，每个模块独立运行HTTP服务，提供各自的专业功能：

- **视频源管理模块** (端口 8081): 负责视频源的管理和RTSP推流
- **视频分析模块** (端口 8082): 负责AI模型管理和视频分析任务

## 视频源管理模块 API (端口 8081)

### 基础路径: `http://localhost:8081/api`

#### 视频源管理
- `GET /sources` - 获取所有视频源列表
- `POST /sources` - 添加新的视频源
- `PUT /sources/:id` - 更新视频源配置
- `DELETE /sources/:id` - 删除视频源
- `GET /sources/:id` - 获取特定视频源信息

#### RTSP推流控制
- `POST /sources/:id/rtsp/start` - 启动RTSP推流
- `POST /sources/:id/rtsp/stop` - 停止RTSP推流
- `GET /sources/:id/rtsp/status` - 获取RTSP推流状态

#### 统计监控
- `GET /sources/:id/stats` - 获取视频源统计信息
- `GET /system/info` - 获取系统信息

### 主要功能职责
1. **视频源生命周期管理**: 添加、配置、删除视频源
2. **RTSP服务器管理**: 提供RTSP推流服务
3. **视频采集监控**: 监控视频采集质量和性能
4. **硬件设备管理**: 管理摄像头等视频采集设备

## 视频分析模块 API (端口 8082)

### 基础路径: `http://localhost:8082/api`

#### AI模型管理
- `GET /models` - 获取可用AI模型列表
- `POST /models/load` - 加载AI模型
- `POST /models/unload` - 卸载AI模型
- `GET /models/:id` - 获取模型详细信息

#### 分析任务控制
- `POST /analysis/start` - 启动分析任务
- `POST /analysis/stop` - 停止分析任务
- `GET /analysis/status` - 获取分析状态概览
- `GET /analysis/results` - 获取分析结果
- `GET /analysis/tasks` - 获取所有分析任务

#### 分析视频源管理
- `GET /sources` - 获取分析模块的视频源(RTSP接收端)
- `POST /sources` - 添加分析视频源
- `DELETE /sources/:id` - 移除分析视频源

#### 性能监控
- `GET /system/info` - 获取系统信息
- `GET /system/stats` - 获取性能统计

### 主要功能职责
1. **AI模型生命周期管理**: 加载、卸载、切换AI模型
2. **分析任务调度**: 管理多个并发分析任务
3. **RTSP客户端管理**: 接收来自视频源模块的RTSP流
4. **分析结果管理**: 存储和检索分析结果
5. **GPU资源管理**: 优化GPU使用和性能监控

## 数据流和协作

### 典型工作流程
1. **视频源配置**: 前端调用视频源管理API添加视频源
2. **RTSP推流启动**: 视频源管理模块启动RTSP推流服务
3. **分析源配置**: 前端调用视频分析API添加对应的RTSP接收源
4. **模型加载**: 前端调用视频分析API加载所需的AI模型
5. **分析任务启动**: 前端调用视频分析API启动分析任务
6. **结果获取**: 前端定期查询分析结果

### 模块间通信
- **视频数据传输**: 通过RTSP协议传输视频流
- **状态同步**: 通过HTTP API进行状态查询和控制
- **配置协调**: 前端协调两个模块的配置同步

## 前端集成策略

### 服务发现
前端需要知道两个服务的端点：
- 视频源管理: `http://localhost:8081`
- 视频分析: `http://localhost:8082`

### 状态管理
建议使用Pinia store分别管理：
- `useVideoSourceStore`: 管理视频源状态
- `useAnalysisStore`: 管理分析任务和模型状态

### 错误处理
每个模块的API都遵循统一的响应格式：
```json
{
  "success": true/false,
  "data": {...},
  "message": "错误信息",
  "timestamp": 1234567890
}
```

### 配置文件更新
两个模块的配置文件需要包含HTTP服务端口配置：
- `video-source-manager.json`: 添加 `"api_port": 8081`
- `video-analyzer.json`: 添加 `"api_port": 8082`