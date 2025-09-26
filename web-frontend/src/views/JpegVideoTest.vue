<template>
  <div class="jpeg-video-test">
    <el-row :gutter="20">
      <!-- 控制面板 -->
      <el-col :span="8">
        <el-card shadow="hover">
          <template #header>
            <span>JPEG视频流测试</span>
          </template>

          <!-- WebRTC连接状态 -->
          <div class="status-section">
            <h4>连接状态</h4>
            <el-descriptions :column="1" border size="small">
              <el-descriptions-item label="信令服务器">
                <el-tag :type="webrtcConnected ? 'success' : 'danger'">
                  {{ webrtcConnected ? '已连接' : '未连接' }}
                </el-tag>
              </el-descriptions-item>
              <el-descriptions-item label="WebRTC状态">
                <el-tag :type="getConnectionStateType(connectionState)">
                  {{ connectionState }}
                </el-tag>
              </el-descriptions-item>
              <el-descriptions-item label="数据通道">
                <el-tag :type="dataChannelOpen ? 'success' : 'info'">
                  {{ dataChannelOpen ? '已打开' : '未打开' }}
                </el-tag>
              </el-descriptions-item>
            </el-descriptions>
          </div>

          <!-- 控制按钮 -->
          <div class="control-section">
            <h4>控制</h4>
            <el-button-group>
              <el-button
                type="primary"
                @click="connectWebRTC"
                :disabled="webrtcConnected"
                :loading="connecting"
              >
                连接WebRTC
              </el-button>
              <el-button
                type="warning"
                @click="disconnectWebRTC"
                :disabled="!webrtcConnected"
              >
                断开连接
              </el-button>
            </el-button-group>

            <div style="margin-top: 10px">
              <el-button
                type="success"
                @click="requestVideoStream"
                :disabled="!webrtcConnected"
              >
                请求视频流
              </el-button>
            </div>
          </div>

          <!-- 统计信息 -->
          <div class="stats-section">
            <h4>统计信息</h4>
            <el-descriptions :column="1" border size="small">
              <el-descriptions-item label="已接收帧数">
                {{ frameCount }}
              </el-descriptions-item>
              <el-descriptions-item label="当前FPS">
                {{ currentFPS.toFixed(1) }}
              </el-descriptions-item>
              <el-descriptions-item label="平均延迟">
                {{ avgLatency.toFixed(0) }}ms
              </el-descriptions-item>
              <el-descriptions-item label="总接收数据">
                {{ formatBytes(totalBytes) }}
              </el-descriptions-item>
            </el-descriptions>
          </div>

          <!-- 日志 -->
          <div class="log-section">
            <h4>连接日志</h4>
            <el-input
              v-model="logText"
              type="textarea"
              :rows="8"
              readonly
              resize="none"
            />
            <div style="margin-top: 10px; text-align: center">
              <el-button size="small" @click="clearLog">清除日志</el-button>
            </div>
          </div>
        </el-card>
      </el-col>

      <!-- 视频显示 -->
      <el-col :span="16">
        <el-card shadow="hover">
          <template #header>
            <div class="video-header">
              <span>JPEG视频流 - {{ currentResolution }}</span>
              <div>
                <el-button
                  size="small"
                  @click="saveCurrentFrame"
                  :disabled="!hasFrame"
                >
                  <el-icon><Download /></el-icon>
                  保存帧
                </el-button>
                <el-button
                  size="small"
                  @click="toggleDetections"
                >
                  <el-icon><View /></el-icon>
                  {{ showDetections ? '隐藏' : '显示' }}检测
                </el-button>
              </div>
            </div>
          </template>

          <!-- JPEG视频播放器 -->
          <div class="video-display">
            <JpegVideoPlayer
              ref="jpegPlayerRef"
              :width="800"
              :height="600"
              :show-controls="true"
              :show-detections="showDetections"
              :detections="mockDetections"
              @frame-received="onFrameReceived"
              @error="onVideoError"
            />
          </div>

          <!-- 调试信息 -->
          <div class="debug-info" v-if="debugMode">
            <el-alert
              title="调试信息"
              type="info"
              :closable="false"
              style="margin-top: 20px"
            >
              <p>最后帧时间: {{ lastFrameTime }}</p>
              <p>帧间隔: {{ frameInterval }}ms</p>
              <p>数据通道状态: {{ dataChannelState }}</p>
            </el-alert>
          </div>
        </el-card>
      </el-col>
    </el-row>
  </div>
</template>

<script setup lang="ts">
import { ref, onMounted, onUnmounted, computed } from 'vue'
import { ElMessage } from 'element-plus'
import { Download, View } from '@element-plus/icons-vue'
import JpegVideoPlayer from '@/components/JpegVideoPlayer.vue'
import { WebRTCClient, type WebRTCConfig } from '@/utils/webrtc'
import type { DetectionResult } from '@/types'

// 响应式数据
const jpegPlayerRef = ref<InstanceType<typeof JpegVideoPlayer> | null>(null)
const webrtcClient = ref<WebRTCClient | null>(null)

// 连接状态
const webrtcConnected = ref(false)
const connecting = ref(false)
const connectionState = ref('disconnected')
const dataChannelOpen = ref(false)
const dataChannelState = ref('closed')

// 视频统计
const frameCount = ref(0)
const currentFPS = ref(0)
const avgLatency = ref(0)
const totalBytes = ref(0)
const hasFrame = ref(false)
const currentResolution = ref('等待视频...')

// 显示控制
const showDetections = ref(true)
const debugMode = ref(true)

// 日志
const logText = ref('')
const logEntries = ref<string[]>([])

// 时间统计
const lastFrameTime = ref('')
const frameInterval = ref(0)
const frameTimestamps = ref<number[]>([])

// 模拟检测结果
const mockDetections = ref<DetectionResult[]>([
  {
    bbox: { x: 100, y: 100, width: 150, height: 200 },
    confidence: 0.85,
    class_id: 0,
    class_name: 'person'
  },
  {
    bbox: { x: 300, y: 150, width: 120, height: 100 },
    confidence: 0.72,
    class_id: 1,
    class_name: 'car'
  }
])

// 计算属性
const getConnectionStateType = (state: string) => {
  switch (state) {
    case 'connected': return 'success'
    case 'connecting': return 'warning'
    case 'disconnected': return 'info'
    case 'failed': return 'danger'
    default: return 'info'
  }
}

// 方法
const addLog = (message: string) => {
  const timestamp = new Date().toLocaleTimeString()
  const logEntry = `[${timestamp}] ${message}`
  logEntries.value.unshift(logEntry)

  // 保持最多100条日志
  if (logEntries.value.length > 100) {
    logEntries.value = logEntries.value.slice(0, 100)
  }

  logText.value = logEntries.value.join('\n')
}

const clearLog = () => {
  logEntries.value = []
  logText.value = ''
}

const formatBytes = (bytes: number) => {
  if (bytes === 0) return '0 B'
  const k = 1024
  const sizes = ['B', 'KB', 'MB', 'GB']
  const i = Math.floor(Math.log(bytes) / Math.log(k))
  return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i]
}

const calculateFPS = () => {
  const now = performance.now()
  frameTimestamps.value.push(now)

  // 保留最近1秒的时间戳
  frameTimestamps.value = frameTimestamps.value.filter(
    timestamp => now - timestamp <= 1000
  )

  currentFPS.value = frameTimestamps.value.length
}

const initWebRTC = () => {
  addLog('初始化WebRTC客户端...')

  const config: WebRTCConfig = {
    signalingServerUrl: 'ws://localhost:8083',
    stunServers: [] // 本地测试不使用STUN服务器
  }

  webrtcClient.value = new WebRTCClient(config)

  webrtcClient.value.setEventHandlers({
    onConnected: () => {
      webrtcConnected.value = true
      addLog('✅ WebRTC信令连接建立')
      ElMessage.success('WebRTC连接成功')
    },
    onDisconnected: () => {
      webrtcConnected.value = false
      connectionState.value = 'disconnected'
      dataChannelOpen.value = false
      addLog('❌ WebRTC连接断开')
      ElMessage.warning('WebRTC连接断开')
    },
    onVideoStream: (stream: MediaStream) => {
      addLog('📹 收到MediaStream视频流')
    },
    onJpegFrame: (jpegData: ArrayBuffer) => {
      handleJpegFrame(jpegData)
    },
    onError: (error: string) => {
      addLog(`❌ WebRTC错误: ${error}`)
      ElMessage.error(`WebRTC错误: ${error}`)
    }
  })
}

const connectWebRTC = async () => {
  if (!webrtcClient.value) {
    initWebRTC()
  }

  connecting.value = true
  addLog('🔗 开始连接WebRTC...')

  try {
    const success = await webrtcClient.value?.connect()
    if (success) {
      addLog('✅ WebRTC信令连接成功')
      // 监听连接状态变化
      startConnectionMonitoring()
    } else {
      addLog('❌ WebRTC连接失败')
    }
  } catch (error) {
    addLog(`❌ WebRTC连接异常: ${(error as Error).message}`)
  } finally {
    connecting.value = false
  }
}

const disconnectWebRTC = () => {
  addLog('🔌 断开WebRTC连接...')
  webrtcClient.value?.disconnect()
  webrtcClient.value = null
  webrtcConnected.value = false
  connectionState.value = 'disconnected'
  dataChannelOpen.value = false
}

const requestVideoStream = () => {
  if (!webrtcClient.value) {
    ElMessage.warning('WebRTC客户端未初始化')
    return
  }

  addLog('🎬 请求视频流...')
  webrtcClient.value.requestVideoStream()
}

const startConnectionMonitoring = () => {
  const monitor = setInterval(() => {
    if (!webrtcClient.value) {
      clearInterval(monitor)
      return
    }

    const state = webrtcClient.value.getConnectionState()
    if (state !== connectionState.value) {
      connectionState.value = state
      addLog(`🔄 连接状态变化: ${state}`)

      if (state === 'connected') {
        dataChannelOpen.value = true
        dataChannelState.value = 'open'
        addLog('📡 数据通道已打开')
      } else if (state === 'failed' || state === 'disconnected') {
        dataChannelOpen.value = false
        dataChannelState.value = 'closed'
      }
    }
  }, 1000)
}

const handleJpegFrame = (jpegData: ArrayBuffer) => {
  const now = performance.now()
  const currentTime = new Date().toLocaleTimeString()

  // 更新统计信息
  frameCount.value++
  totalBytes.value += jpegData.byteLength
  calculateFPS()

  // 计算帧间隔
  if (lastFrameTime.value) {
    const lastTime = new Date(lastFrameTime.value).getTime()
    frameInterval.value = now - lastTime
  }
  lastFrameTime.value = currentTime

  // 显示JPEG帧
  if (jpegPlayerRef.value) {
    jpegPlayerRef.value.displayJpegFrame(jpegData)
    hasFrame.value = true
  }

  // 每10帧记录一次日志
  if (frameCount.value % 10 === 0) {
    addLog(`📦 接收JPEG帧 #${frameCount.value}, 大小: ${formatBytes(jpegData.byteLength)}`)
  }
}

// 视频播放器事件
const onFrameReceived = (width: number, height: number) => {
  currentResolution.value = `${width}x${height}`
  if (frameCount.value === 1) {
    addLog(`📹 视频分辨率: ${width}x${height}`)
  }
}

const onVideoError = (message: string) => {
  addLog(`❌ JPEG播放器错误: ${message}`)
  ElMessage.error(message)
}

const saveCurrentFrame = () => {
  if (jpegPlayerRef.value) {
    jpegPlayerRef.value.saveCurrentFrame()
    addLog('💾 保存当前帧')
  }
}

const toggleDetections = () => {
  showDetections.value = !showDetections.value
  addLog(`👁️ ${showDetections.value ? '显示' : '隐藏'}检测结果`)
}

// 生命周期
onMounted(() => {
  addLog('🚀 JPEG视频测试页面已加载')
  initWebRTC()
})

onUnmounted(() => {
  if (webrtcClient.value) {
    webrtcClient.value.disconnect()
  }
  addLog('🛑 测试页面已卸载')
})
</script>

<style scoped>
.jpeg-video-test {
  padding: 20px;
  min-height: 100vh;
  background-color: #f5f5f5;
}

.status-section,
.control-section,
.stats-section,
.log-section {
  margin-bottom: 20px;
}

.status-section h4,
.control-section h4,
.stats-section h4,
.log-section h4 {
  margin-bottom: 10px;
  color: var(--el-text-color-primary);
  font-size: 14px;
  font-weight: 600;
}

.video-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
}

.video-display {
  display: flex;
  justify-content: center;
  background-color: #000;
  border-radius: 8px;
  padding: 10px;
}

.debug-info {
  font-family: monospace;
  font-size: 12px;
}

.el-card {
  margin-bottom: 20px;
}

.el-button-group {
  width: 100%;
}

.el-button-group .el-button {
  flex: 1;
}
</style>