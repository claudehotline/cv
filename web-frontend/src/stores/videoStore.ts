import { defineStore } from 'pinia'
import { ref, computed } from 'vue'
import type { VideoSource, AnalysisResult, AnalysisType } from '@/types'
import { WebRTCClient, type WebRTCConfig } from '@/utils/webrtc'

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

  const startAnalysis = (sourceId: string, analysisType: string) => {
    console.log('开始分析:', sourceId, analysisType)
    // 简化实现：直接通过WebRTC请求视频流
    requestVideoStream()
  }

  const stopAnalysis = (sourceId: string) => {
    console.log('停止分析:', sourceId)
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

    console.log('🖼️ 收到JPEG帧，大小:', jpegData.byteLength, 'bytes')

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

  // 初始化
  const init = () => {
    // 添加一些示例视频源
    videoSources.value = [
      {
        id: 'camera_01',
        name: '摄像头 1',
        type: 'camera',
        url: '/dev/video0',
        status: 'active',
        fps: 30,
        resolution: '1280x720'
      },
      {
        id: 'file_01',
        name: '测试视频',
        type: 'file',
        url: 'test_video.mp4',
        status: 'inactive',
        fps: 25,
        resolution: '1920x1080'
      }
    ]

    if (videoSources.value.length > 0) {
      selectedSourceId.value = videoSources.value[0].id
    }

    // 初始化WebSocket用于控制命令，WebRTC用于视频流
    console.log('🚀 videoStore.init() - 开始初始化')
    connectWebSocket()
    console.log('🎯 正在连接WebRTC...')
    connectWebRTC()
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

    // WebRTC状态
    webrtcConnected,
    videoStream,
    currentVideoElement,

    // 计算属性
    activeVideoSources,
    selectedSource,
    recentAnalysisResults,

    // 方法
    connectWebSocket,
    addVideoSource,
    removeVideoSource,
    startAnalysis,
    stopAnalysis,
    setSelectedSource,
    setSelectedAnalysisType,

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