<template>
  <div class="video-analysis">
    <el-row :gutter="20">
      <!-- 分析控制面板 -->
      <el-col :span="8">
        <el-card shadow="hover">
          <template #header>
            <div class="card-header">
              <span>分析控制</span>
              <el-button
                size="small"
                type="primary"
                @click="goToSourceManager"
              >
                管理视频源
              </el-button>
            </div>
          </template>

          <el-form label-width="100px">
            <el-form-item label="选择视频源">
              <el-select
                v-model="videoStore.selectedSourceId"
                placeholder="请选择视频源"
                style="width: 100%"
              >
                <el-option
                  v-for="source in videoStore.videoSources"
                  :key="source.id"
                  :label="source.name"
                  :value="source.id"
                />
              </el-select>
              <div v-if="!videoStore.videoSources.length" class="no-sources-hint">
                <el-text type="info" size="small">
                  暂无视频源，请先到视频源管理页面添加
                </el-text>
              </div>
            </el-form-item>

            <el-form-item label="当前源信息" v-if="videoStore.selectedSource">
              <el-descriptions :column="1" border size="small">
                <el-descriptions-item label="名称">
                  {{ videoStore.selectedSource.name }}
                </el-descriptions-item>
                <el-descriptions-item label="类型">
                  <el-tag
                    :type="videoStore.selectedSource.type === 'camera' ? 'success' :
                           videoStore.selectedSource.type === 'file' ? 'info' : 'warning'"
                    size="small"
                  >
                    {{ typeLabels[videoStore.selectedSource.type] }}
                  </el-tag>
                </el-descriptions-item>
                <el-descriptions-item label="状态">
                  <el-tag
                    :type="videoStore.selectedSource.status === 'active' ? 'success' :
                           videoStore.selectedSource.status === 'inactive' ? 'info' : 'danger'"
                    size="small"
                  >
                    {{ statusLabels[videoStore.selectedSource.status] }}
                  </el-tag>
                </el-descriptions-item>
              </el-descriptions>
            </el-form-item>

            <el-form-item label="分析类型">
              <el-radio-group v-model="videoStore.selectedAnalysisType" @change="onAnalysisTypeChange">
                <el-radio
                  v-for="type in videoStore.analysisTypes"
                  :key="type.id"
                  :value="type.id"
                  :disabled="!type.enabled"
                >
                  {{ type.name }}
                </el-radio>
              </el-radio-group>
            </el-form-item>

            <el-form-item label="选择模型">
              <el-select
                v-model="videoStore.selectedModelId"
                placeholder="请选择模型"
                style="width: 100%"
                @change="onModelChange"
              >
                <el-option
                  v-for="model in videoStore.filteredModels"
                  :key="model.id"
                  :label="model.name"
                  :value="model.id"
                >
                  <div class="model-option">
                    <div class="model-name">{{ model.name }}</div>
                    <div class="model-info">
                      <el-tag
                        :type="model.status === 'loaded' ? 'success' : 'info'"
                        size="small"
                      >
                        {{ model.status === 'loaded' ? '已加载' : '未加载' }}
                      </el-tag>
                      <span class="model-desc">{{ model.description }}</span>
                    </div>
                  </div>
                </el-option>
              </el-select>
              <div v-if="!videoStore.filteredModels.length" class="no-models-hint">
                <el-text type="info" size="small">
                  当前分析类型暂无可用模型
                </el-text>
              </div>
            </el-form-item>

            <el-form-item>
              <el-button
                type="success"
                @click="startAnalysis"
                :disabled="videoStore.isAnalyzing"
                :loading="startingAnalysis"
              >
                开始分析
              </el-button>
              <el-button
                type="warning"
                @click="stopAnalysis"
                :disabled="!videoStore.isAnalyzing"
                :loading="stoppingAnalysis"
              >
                停止分析
              </el-button>
            </el-form-item>
          </el-form>

          <div class="connection-status">
            <el-alert
              :type="connectionStatusType"
              :title="connectionStatusText"
              :closable="false"
              size="small"
            />
          </div>
        </el-card>
      </el-col>

      <!-- 视频预览面板 -->
      <el-col :span="16">
        <el-card shadow="hover">
          <template #header>
            <span>视频预览 - {{ selectedSourceName }}</span>
          </template>

          <div class="video-container">
            <div v-if="!videoStore.selectedSourceId" class="no-video">
              <el-icon size="80"><Camera /></el-icon>
              <p>请选择一个视频源</p>
            </div>
            <div v-else class="video-preview">
              <!-- JPEG视频播放器 -->
              <JpegVideoPlayer
                ref="jpegPlayerRef"
                :width="640"
                :height="480"
                :show-controls="true"
                :show-detections="false"
                @frame-received="onFrameReceived"
                @error="onVideoError"
              />

              <!-- 备用: WebRTC视频流 (当不使用JPEG时) -->
              <video
                ref="videoElement"
                class="video-stream"
                autoplay
                muted
                playsinline
                style="display: none"
              >
              </video>

              <!-- WebRTC连接状态指示器 -->
              <div class="webrtc-status">
                <el-tag
                  :type="videoStore.webrtcConnected ? 'success' : 'danger'"
                  size="small"
                  effect="dark"
                >
                  {{ videoStore.webrtcConnected ? 'WebRTC已连接' : 'WebRTC未连接' }}
                </el-tag>
              </div>

              <!-- 分析结果由后端绘制到帧上，前端不再叠加自绘框 -->

              <!-- 视频流控制按钮 -->
              <div class="video-controls">
                <el-button
                  v-if="!videoStore.webrtcConnected"
                  type="primary"
                  size="small"
                  @click="requestVideoStream"
                  >
                  <el-icon><CaretRight /></el-icon>
                  开始视频流
                </el-button>
                <el-button
                  v-else
                  type="warning"
                  size="small"
                  @click="stopVideoStream"
                >
                  <el-icon><VideoPause /></el-icon>
                  停止视频流
                </el-button>
              </div>
            </div>
          </div>
        </el-card>

        <!-- 实时统计 -->
        <el-card shadow="hover" style="margin-top: 20px">
          <template #header>
            <span>实时统计</span>
          </template>

          <el-row :gutter="20">
            <el-col :span="6">
              <el-statistic
                title="检测对象数"
                :value="currentDetectionCount"
                :value-style="{ color: '#409EFF' }"
              />
            </el-col>
            <el-col :span="6">
              <el-statistic
                title="分析帧数"
                :value="analysisFrameCount"
                :value-style="{ color: '#67C23A' }"
              />
            </el-col>
            <el-col :span="6">
              <el-statistic
                title="平均置信度"
                :value="averageConfidence"
                suffix="%"
                :value-style="{ color: '#E6A23C' }"
              />
            </el-col>
            <el-col :span="6">
              <el-statistic
                title="处理延迟"
                :value="processingDelay"
                suffix="ms"
                :value-style="{ color: '#F56C6C' }"
              />
            </el-col>
          </el-row>

          <!-- 检测结果历史 -->
          <div v-if="recentResult?.detections.length" style="margin-top: 20px">
            <el-divider content-position="left">最近检测结果</el-divider>
            <el-space wrap>
              <el-tag
                v-for="(detection, index) in recentResult.detections"
                :key="index"
                :type="getDetectionTagType(detection.confidence)"
                size="small"
              >
                {{ detection.class_name }} {{ Math.round(detection.confidence * 100) }}%
              </el-tag>
            </el-space>
          </div>
        </el-card>
      </el-col>
    </el-row>
  </div>
</template>

<script setup lang="ts">
import { ref, computed, watch, onMounted, onUnmounted } from 'vue'
import { useRouter } from 'vue-router'
import { useVideoStore } from '@/stores/videoStore'
import type { DetectionResult } from '@/types'
import { CaretRight, VideoPause, Camera } from '@element-plus/icons-vue'
import { ElMessage } from 'element-plus'
import JpegVideoPlayer from '@/components/JpegVideoPlayer.vue'

const router = useRouter()
const videoStore = useVideoStore()

// 数据
const videoElement = ref<HTMLVideoElement | null>(null)
const jpegPlayerRef = ref<InstanceType<typeof JpegVideoPlayer> | null>(null)
const startingAnalysis = ref(false)
const stoppingAnalysis = ref(false)

// 标签映射
const typeLabels = {
  camera: '摄像头',
  file: '文件',
  stream: '流'
}

const statusLabels = {
  active: '运行中',
  inactive: '未激活',
  error: '错误'
}

// 计算属性
const selectedSourceName = computed(() => {
  return videoStore.selectedSource?.name || '未选择'
})

const connectionStatusType = computed(() => {
  switch (videoStore.connectionStatus) {
    case 'connected': return 'success'
    case 'connecting': return 'warning'
    default: return 'error'
  }
})

const connectionStatusText = computed(() => {
  switch (videoStore.connectionStatus) {
    case 'connected': return '已连接到后端服务'
    case 'connecting': return '正在连接后端服务...'
    default: return '后端服务连接失败'
  }
})

const recentResult = computed(() => {
  return videoStore.recentAnalysisResults[0]
})

const currentDetectionCount = computed(() => {
  return recentResult.value?.detections.length || 0
})

const analysisFrameCount = computed(() => {
  return videoStore.analysisResults.length
})

const averageConfidence = computed(() => {
  if (!recentResult.value?.detections.length) return 0
  const sum = recentResult.value.detections.reduce((acc, det) => acc + det.confidence, 0)
  return Math.round((sum / recentResult.value.detections.length) * 100)
})

const processingDelay = computed(() => {
  // 模拟处理延迟
  return Math.round(Math.random() * 100 + 50)
})

// 方法
const startAnalysis = async () => {
  startingAnalysis.value = true
  try {
    await videoStore.startAnalysis(videoStore.selectedSourceId, videoStore.selectedAnalysisType)
    console.log('✅ 开始分析成功')
  } catch (error) {
    console.error('❌ 开始分析失败:', error)
    // 显示错误提示
    ElMessage.error('开始分析失败: ' + (error as Error).message)
  } finally {
    startingAnalysis.value = false
  }
}

const stopAnalysis = async () => {
  stoppingAnalysis.value = true
  try {
    await videoStore.stopAnalysis(videoStore.selectedSourceId)
    console.log('✅ 停止分析成功')
  } catch (error) {
    console.error('❌ 停止分析失败:', error)
    // 显示错误提示
    ElMessage.error('停止分析失败: ' + (error as Error).message)
  } finally {
    stoppingAnalysis.value = false
  }
}

const getDetectionBoxStyle = (detection: DetectionResult) => {
  return {
    left: `${detection.bbox.x}px`,
    top: `${detection.bbox.y}px`,
    width: `${detection.bbox.width}px`,
    height: `${detection.bbox.height}px`,
  }
}

const getDetectionTagType = (confidence: number) => {
  if (confidence >= 0.8) return 'success'
  if (confidence >= 0.6) return 'warning'
  return 'danger'
}

const goToSourceManager = () => {
  router.push('/video-source-manager')
}

// 模型选择相关方法
const onAnalysisTypeChange = (analysisType: string) => {
  videoStore.setSelectedAnalysisType(analysisType)
}

const onModelChange = async (modelId: string) => {
  try {
    await videoStore.setSelectedModel(modelId)
  } catch (error) {
    console.error('切换模型失败:', error)
    ElMessage.error('切换模型失败')
  }
}

// WebRTC相关方法
const requestVideoStream = async () => {
  // 如果WebRTC未连接，先重新连接
  if (!videoStore.webrtcConnected) {
    console.log('🔗 WebRTC未连接，正在重新连接...')
    await videoStore.connectWebRTC()
    // 等待连接建立后再请求视频流
    setTimeout(() => {
      videoStore.requestVideoStream()
    }, 1000)
  } else {
    videoStore.requestVideoStream()
  }
}

const stopVideoStream = () => {
  // 断开WebRTC连接
  videoStore.disconnectWebRTC()

  // 清理本地视频元素
  if (videoElement.value) {
    videoElement.value.srcObject = null
  }

  // 清理JPEG播放器
  if (jpegPlayerRef.value) {
    jpegPlayerRef.value.clearCanvas()
  }

  console.log('🛑 视频流已停止')
}

// JPEG播放器事件处理
const onFrameReceived = (width: number, height: number) => {
  // 帧接收处理（不输出日志）
}

const onVideoError = (message: string) => {
  console.error('❌ JPEG播放器错误:', message)
}

// 监听视频源变化，更新分析状态并重新请求视频流
watch(() => videoStore.selectedSourceId, async (newSourceId, oldSourceId) => {
  if (newSourceId && newSourceId !== oldSourceId) {
    console.log('📹 视频源已切换:', oldSourceId, '->', newSourceId)

    // 如果WebRTC已连接，直接请求新源的视频流（不需要断开重连）
    if (videoStore.webrtcConnected) {
      console.log('🔄 保持WebRTC连接，切换到新源:', newSourceId)
      // 稍等一下让selectedSourceId更新完成
      await new Promise(resolve => setTimeout(resolve, 100))
      videoStore.requestVideoStream()
    } else {
      // 如果未连接，先连接再请求
      console.log('🔗 WebRTC未连接，正在连接并请求新源:', newSourceId)
      await videoStore.connectWebRTC()
      setTimeout(() => {
        videoStore.requestVideoStream()
      }, 1000)
    }

    // 更新分析状态
    await videoStore.getAnalysisStatus()
  }
})

// 生命周期
onMounted(async () => {
  console.log('🎬 VideoAnalysis组件已挂载')
  videoStore.init()

  // 等待WebRTC连接建立和DOM更新
  setTimeout(async () => {
    console.log('🎥 准备设置视频元素和JPEG播放器')

    // 设置JPEG视频播放器
    if (jpegPlayerRef.value) {
      console.log('📹 找到JPEG播放器，正在设置到store')
      videoStore.setJpegVideoPlayer(jpegPlayerRef.value)
    } else {
      console.error('❌ JPEG播放器未找到')
    }

    // 设置备用视频元素
    if (videoElement.value) {
      console.log('📹 找到视频元素，正在设置到store')
      videoStore.setVideoElement(videoElement.value)
    }

    // 每个客户端独立：刷新后自动开启分析
    if (videoStore.selectedSourceId) {
      console.log('🎬 自动开启分析（新连接默认开启）')
      try {
        await videoStore.startAnalysis(videoStore.selectedSourceId, videoStore.selectedAnalysisType)
      } catch (error) {
        console.error('自动开启分析失败:', error)
      }
    }

    // 自动请求视频流（如果已选择源）
    if (videoStore.selectedSourceId) {
      console.log('🎬 自动请求视频流, sourceId:', videoStore.selectedSourceId)
      setTimeout(() => {
        videoStore.requestVideoStream()
      }, 500)
    }
  }, 1000) // 增加延迟确保WebRTC客户端已初始化
})

onUnmounted(() => {
  // 清理WebRTC连接
  videoStore.disconnectWebRTC()
  startingAnalysis.value = false
  stoppingAnalysis.value = false
})
</script>

<style scoped>
.video-analysis {
  height: 100%;
}

.card-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
}

.no-sources-hint {
  margin-top: 8px;
}

.video-container {
  height: 400px;
  background-color: #000;
  border-radius: 8px;
  position: relative;
  overflow: hidden;
}

.no-video {
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  height: 100%;
  color: #666;
}

.video-preview {
  width: 100%;
  height: 100%;
  position: relative;
  display: flex;
  align-items: center;
  justify-content: center;
}

.video-stream {
  width: 100%;
  height: 100%;
  object-fit: contain;
  background-color: #000;
  border-radius: 8px;
}

.webrtc-status {
  position: absolute;
  top: 10px;
  left: 10px;
  z-index: 10;
}

.video-controls {
  position: absolute;
  bottom: 10px;
  left: 50%;
  transform: translateX(-50%);
  z-index: 10;
}

.analysis-overlay {
  position: absolute;
  top: 0;
  left: 0;
  width: 100%;
  height: 100%;
  pointer-events: none;
}

.detection-box {
  position: absolute;
  border: 2px solid #00ff00;
  background-color: rgba(0, 255, 0, 0.1);
}

.detection-label {
  position: absolute;
  top: -25px;
  left: 0;
  background-color: #00ff00;
  color: #000;
  padding: 2px 6px;
  font-size: 12px;
  border-radius: 3px;
  white-space: nowrap;
}

.connection-status {
  margin-top: 20px;
}

.model-option {
  padding: 4px 0;
}

.model-name {
  font-weight: 500;
  margin-bottom: 4px;
}

.model-info {
  display: flex;
  align-items: center;
  gap: 8px;
}

.model-desc {
  font-size: 12px;
  color: #666;
  flex: 1;
}

.no-models-hint {
  margin-top: 8px;
}
</style>
