import { defineStore } from 'pinia'
import { ref, computed } from 'vue'
import type { VideoSource, AnalysisResult, AnalysisType } from '@/types'
import { WebRTCClient, type WebRTCConfig } from '@/utils/webrtc'

// 模型信息接口
interface ModelInfo {
  id: string
  name: string
  type: string
  status: 'loaded' | 'available' | 'loading' | 'error'
  description?: string
}

export const useVideoStore = defineStore('video', () => {
  // 状态
  const videoSources = ref<VideoSource[]>([])
  const analysisResults = ref<AnalysisResult[]>([])
  const analysisTypes = ref<AnalysisType[]>([
    { id: 'object_detection', name: '目标检测', enabled: true },
    { id: 'instance_segmentation', name: '实例分割', enabled: false }
  ])

  const selectedSourceId = ref<string>('')
  const selectedAnalysisType = ref<string>('object_detection')

  // 模型相关状态
  const availableModels = ref<ModelInfo[]>([])
  const selectedModelId = ref<string>('')
  const isAnalyzing = ref(true)

  const wsConnection = ref<WebSocket | null>(null)
  const connectionStatus = ref<'connecting' | 'connected' | 'disconnected'>('disconnected')

  // WebRTC相关状态
  const webrtcClient = ref<WebRTCClient | null>(null)
  const webrtcConnected = ref(false)
  const videoStream = ref<MediaStream | null>(null)
  const currentVideoElement = ref<HTMLVideoElement | null>(null)

  // JPEG视频播放器状态
  const jpegVideoPlayer = ref<any>(null) // JpegVideoPlayer组件引用
  const lastFrameTimestamp = ref(0)
  const frameLatency = ref(0)

  // 计算属性
  const activeVideoSources = computed(() =>
    videoSources.value.filter(source => source.status === 'active')
  )

  const selectedSource = computed(() =>
    videoSources.value.find(source => source.id === selectedSourceId.value)
  )

  const recentAnalysisResults = computed(() =>
    analysisResults.value
      .filter(result => result.source_id === selectedSourceId.value)
      .sort((a, b) => b.timestamp - a.timestamp)
      .slice(0, 10)
  )

  // 根据选中的分析类型过滤模型
  const filteredModels = computed(() =>
    availableModels.value.filter(model => model.type === selectedAnalysisType.value)
  )

  // 方法
  const connectWebSocket = () => {
    // 简化实现：直接设置为已连接，因为后端没有实现控制WebSocket
    connectionStatus.value = 'connected'
    console.log('模拟WebSocket连接已建立')
  }

  const handleWebSocketMessage = (message: any) => {
    switch (message.type) {
      case 'video_sources_update':
        videoSources.value = message.data
        break

      case 'analysis_result':
        analysisResults.value.unshift(message.data)
        // 保持最多1000个结果
        if (analysisResults.value.length > 1000) {
          analysisResults.value = analysisResults.value.slice(0, 1000)
        }
        break

      case 'status_update':
        updateVideoSourceStatus(message.source_id, message.data.status)
        break

      case 'error':
        console.error('系统错误:', message.data)
        break

      default:
        console.log('未知消息类型:', message)
    }
  }

  const updateVideoSourceStatus = (sourceId: string, status: string) => {
    const source = videoSources.value.find(s => s.id === sourceId)
    if (source) {
      source.status = status as VideoSource['status']
    }
  }

  const addVideoSource = (source: Omit<VideoSource, 'id' | 'status'>) => {
    const newSource: VideoSource = {
      ...source,
      id: `source_${Date.now()}`,
      status: 'active'
    }
    videoSources.value.push(newSource)
    console.log('添加视频源:', newSource.name)
  }

  const updateVideoSource = (source: VideoSource) => {
    const index = videoSources.value.findIndex(s => s.id === source.id)
    if (index > -1) {
      videoSources.value[index] = source
      console.log('更新视频源:', source.name)
    }
  }

  const removeVideoSource = (sourceId: string) => {
    const index = videoSources.value.findIndex(s => s.id === sourceId)
    if (index > -1) {
      videoSources.value.splice(index, 1)

      // 发送到后端
      if (wsConnection.value && wsConnection.value.readyState === WebSocket.OPEN) {
        wsConnection.value.send(JSON.stringify({
          type: 'remove_video_source',
          data: { source_id: sourceId }
        }))
      }
    }
  }

  const setSelectedModel = (modelId: string) => {
    selectedModelId.value = modelId
  }

  const getAnalysisStatus = async () => {
    // 简化实现：返回当前状态
    return {
      isAnalyzing: isAnalyzing.value,
      selectedSourceId: selectedSourceId.value,
      selectedModelId: selectedModelId.value
    }
  }

  const startAnalysis = async (sourceId: string, analysisType: string) => {
    console.log('开始分析:', sourceId, analysisType, 'model_id:', selectedModelId.value)

    try {
      // 确保有选中的模型ID
      if (!selectedModelId.value) {
        throw new Error('请先选择一个模型')
      }

      // 通过HTTP API发送启用检测框绘制的请求
      const response = await fetch('/api/analyzer/analysis/start', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json'
        },
        body: JSON.stringify({
          source_id: sourceId,
          model_id: selectedModelId.value,
          analysis_type: analysisType
        })
      })

      console.log('API响应状态:', response.status, response.statusText)

      // 检查响应是否为空
      const text = await response.text()
      console.log('API响应内容:', text)

      if (!text) {
        throw new Error('服务器返回空响应')
      }

      const data = JSON.parse(text)
      if (data.success) {
        isAnalyzing.value = true
        console.log('✅ 启用分析成功')
        // 如果WebRTC未连接，请求视频流
        if (!webrtcConnected.value) {
          requestVideoStream()
        }
      } else {
        console.error('❌ 启用分析失败:', data.message)
        throw new Error(data.message || '启用分析失败')
      }
    } catch (error) {
      console.error('❌ 启用分析请求失败:', error)
      throw error
    }
  }

  const stopAnalysis = async (sourceId: string) => {
    console.log('停止分析:', sourceId)

    try {
      // 通过HTTP API发送禁用检测框绘制的请求
      const response = await fetch('/api/analyzer/analysis/stop', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json'
        },
        body: JSON.stringify({
          source_id: sourceId
        })
      })

      console.log('API响应状态:', response.status, response.statusText)

      // 检查响应是否为空
      const text = await response.text()
      console.log('API响应内容:', text)

      if (!text) {
        throw new Error('服务器返回空响应')
      }

      const data = JSON.parse(text)
      if (data.success) {
        isAnalyzing.value = false
        console.log('✅ 停止分析成功')
      } else {
        console.error('❌ 停止分析失败:', data.message)
        throw new Error(data.message || '停止分析失败')
      }
    } catch (error) {
      console.error('❌ 停止分析请求失败:', error)
      throw error
    }
  }

  const setSelectedSource = (sourceId: string) => {
    selectedSourceId.value = sourceId
  }

  const setSelectedAnalysisType = (analysisType: string) => {
    selectedAnalysisType.value = analysisType
  }

  // WebRTC相关方法
  const initWebRTC = () => {
    const config: WebRTCConfig = {
      signalingServerUrl: 'ws://localhost:8083',
      stunServers: []  // 纯本地连接，不使用STUN/TURN
    }

    webrtcClient.value = new WebRTCClient(config)

    webrtcClient.value.setEventHandlers({
      onConnected: () => {
        webrtcConnected.value = true
        console.log('WebRTC连接建立')
      },
      onDisconnected: () => {
        webrtcConnected.value = false
        videoStream.value = null
        console.log('WebRTC连接断开')
      },
      onVideoStream: (stream: MediaStream) => {
        videoStream.value = stream
        if (currentVideoElement.value) {
          currentVideoElement.value.srcObject = stream
        }
        console.log('接收到视频流')
      },
      onJpegFrame: (jpegData: ArrayBuffer) => {
        handleJpegFrame(jpegData)
      },
      onError: (error: string) => {
        console.error('WebRTC错误:', error)
      }
    })
  }

  const connectWebRTC = async () => {
    if (!webrtcClient.value) {
      initWebRTC()
    }

    try {
      const success = await webrtcClient.value?.connect()
      if (success) {
        console.log('WebRTC信令连接成功')
      }
    } catch (error) {
      console.error('WebRTC连接失败:', error)
    }
  }

  const disconnectWebRTC = () => {
    webrtcClient.value?.disconnect()
    webrtcClient.value = null
    webrtcConnected.value = false
    videoStream.value = null
  }

  const setVideoElement = (element: HTMLVideoElement) => {
    console.log('📹 videoStore.setVideoElement被调用, element:', element)
    currentVideoElement.value = element
    if (webrtcClient.value) {
      console.log('✅ webrtcClient存在，正在设置视频元素')
      webrtcClient.value.setVideoElement(element)
    } else {
      console.log('⚠️ webrtcClient不存在，无法设置视频元素')
    }
  }

  const requestVideoStream = () => {
    console.log('🎬 请求视频流 - webrtcClient:', !!webrtcClient.value, 'selectedSourceId:', selectedSourceId.value)
    if (webrtcClient.value && selectedSourceId.value) {
      console.log('✅ 条件满足，发送request_offer')
      webrtcClient.value.requestVideoStream()
    } else {
      console.log('❌ 条件不满足 - webrtcClient存在:', !!webrtcClient.value, 'selectedSourceId存在:', !!selectedSourceId.value)
    }
  }

  // JPEG视频播放器相关方法
  const setJpegVideoPlayer = (player: any) => {
    jpegVideoPlayer.value = player
    console.log('🎬 设置JPEG视频播放器:', !!player)
  }

  const handleJpegFrame = (jpegData: ArrayBuffer) => {
    const currentTime = performance.now()

    // 计算延迟
    if (lastFrameTimestamp.value > 0) {
      frameLatency.value = currentTime - lastFrameTimestamp.value
    }
    lastFrameTimestamp.value = currentTime

    // 发送到JPEG播放器
    if (jpegVideoPlayer.value) {
      jpegVideoPlayer.value.displayJpegFrame(jpegData)
      jpegVideoPlayer.value.updateLatency(frameLatency.value)

      // 更新分析结果（如果有）
      const recentResult = recentAnalysisResults.value[0]
      if (recentResult) {
        jpegVideoPlayer.value.updateDetections(recentResult.detections || [])
      }
    } else {
      console.warn('⚠️ JPEG视频播放器未设置')
    }
  }

  // 从后端获取模型列表
  const fetchModels = async () => {
    try {
      const response = await fetch('/api/analyzer/models')
      const data = await response.json()
      if (data.success && data.data) {
        availableModels.value = data.data
        if (availableModels.value.length > 0 && !selectedModelId.value) {
          selectedModelId.value = availableModels.value[0].id
        }
      }
    } catch (error) {
      console.error('获取模型列表失败:', error)
    }
  }

  // 从后端获取视频源列表
  const fetchVideoSources = async () => {
    try {
      const response = await fetch('/api/analyzer/sources')
      const data = await response.json()
      if (data.success && data.data) {
        videoSources.value = data.data
        if (videoSources.value.length > 0 && !selectedSourceId.value) {
          selectedSourceId.value = videoSources.value[0].id
        }
      }
    } catch (error) {
      console.error('获取视频源列表失败:', error)
    }
  }

  // 初始化
  const init = async () => {
    // 初始化WebSocket用于控制命令，WebRTC用于视频流
    console.log('🚀 videoStore.init() - 开始初始化')
    connectWebSocket()
    console.log('🎯 正在连接WebRTC...')
    connectWebRTC()

    // 从后端获取视频源列表
    console.log('📹 正在获取视频源列表...')
    await fetchVideoSources()

    // 从后端获取模型列表
    console.log('📦 正在获取模型列表...')
    await fetchModels()

    console.log('✅ videoStore.init() - 初始化完成')
  }

  return {
    // 状态
    videoSources,
    analysisResults,
    analysisTypes,
    selectedSourceId,
    selectedAnalysisType,
    connectionStatus,

    // 模型相关状态
    availableModels,
    selectedModelId,
    isAnalyzing,

    // WebRTC状态
    webrtcConnected,
    videoStream,
    currentVideoElement,

    // 计算属性
    activeVideoSources,
    selectedSource,
    recentAnalysisResults,
    filteredModels,

    // 方法
    connectWebSocket,
    addVideoSource,
    updateVideoSource,
    removeVideoSource,
    startAnalysis,
    stopAnalysis,
    setSelectedSource,
    setSelectedAnalysisType,
    setSelectedModel,
    getAnalysisStatus,
    fetchModels,
    fetchVideoSources,

    // WebRTC方法
    connectWebRTC,
    disconnectWebRTC,
    setVideoElement,
    requestVideoStream,

    // JPEG播放器方法
    setJpegVideoPlayer,
    handleJpegFrame,

    init
  }
})