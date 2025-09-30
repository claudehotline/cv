export interface WebRTCConfig {
  signalingServerUrl: string
  stunServers: string[]
  turnServers?: RTCIceServer[]
}

export interface SignalingMessage {
  type: string
  client_id?: string
  data?: any
  timestamp?: number
}

export class WebRTCClient {
  private peerConnection: RTCPeerConnection | null = null
  private signalingSocket: WebSocket | null = null
  private localVideo: HTMLVideoElement | null = null
  private remoteVideo: HTMLVideoElement | null = null
  private clientId: string | null = null
  private pendingIceCandidates: any[] = []
  private pendingStream: MediaStream | null = null
  private dataChannel: RTCDataChannel | null = null
  private jpegBuffer: ArrayBuffer[] = []
  private currentFrameSize: number = 0
  private frameReceiving: boolean = false

  private config: WebRTCConfig
  private onConnected?: () => void
  private onDisconnected?: () => void
  private onVideoStream?: (stream: MediaStream) => void
  private onJpegFrame?: (jpegData: ArrayBuffer) => void
  private onError?: (error: string) => void

  constructor(config: WebRTCConfig) {
    this.config = config
  }

  async connect(): Promise<boolean> {
    try {
      console.log('🔌 开始WebRTC连接...')

      // 连接信令服务器
      console.log('🌐 连接信令服务器:', this.config.signalingServerUrl)
      await this.connectSignalingServer()

      // 创建PeerConnection
      console.log('🔗 创建PeerConnection')
      this.createPeerConnection()

      console.log('✅ WebRTC连接初始化成功')
      return true
    } catch (error) {
      console.error('❌ WebRTC连接失败:', error)
      this.onError?.(error instanceof Error ? error.message : 'Unknown error')
      return false
    }
  }

  disconnect(): void {
    if (this.peerConnection) {
      this.peerConnection.close()
      this.peerConnection = null
    }

    if (this.signalingSocket) {
      this.signalingSocket.close()
      this.signalingSocket = null
    }

    this.onDisconnected?.()
  }

  setVideoElement(videoElement: HTMLVideoElement): void {
    console.log('🎥 设置视频元素')
    this.remoteVideo = videoElement

    // 如果有待处理的流，立即设置
    if (this.pendingStream && this.remoteVideo) {
      console.log('📹 应用待处理的视频流到元素')
      this.remoteVideo.srcObject = this.pendingStream
      this.remoteVideo.muted = true
      this.remoteVideo.autoplay = true

      this.remoteVideo.play().then(() => {
        console.log('▶️ 延迟播放成功')
      }).catch((err) => {
        console.error('❌ 延迟播放失败:', err)
      })
    }
  }

  setEventHandlers(handlers: {
    onConnected?: () => void
    onDisconnected?: () => void
    onVideoStream?: (stream: MediaStream) => void
    onJpegFrame?: (jpegData: ArrayBuffer) => void
    onError?: (error: string) => void
  }): void {
    this.onConnected = handlers.onConnected
    this.onDisconnected = handlers.onDisconnected
    this.onVideoStream = handlers.onVideoStream
    this.onJpegFrame = handlers.onJpegFrame
    this.onError = handlers.onError
  }

  requestVideoStream(): void {
    if (this.signalingSocket && this.signalingSocket.readyState === WebSocket.OPEN) {
      const message: SignalingMessage = {
        type: 'request_offer',
        timestamp: Date.now()
      }
      this.signalingSocket.send(JSON.stringify(message))
    }
  }

  sendControlMessage(controlData: any): void {
    if (this.signalingSocket && this.signalingSocket.readyState === WebSocket.OPEN) {
      const message = {
        ...controlData,
        timestamp: Date.now()
      }
      console.log('📤 发送控制消息:', message)
      this.signalingSocket.send(JSON.stringify(message))
    } else {
      console.warn('⚠️ WebSocket未连接，无法发送控制消息')
    }
  }

  private async connectSignalingServer(): Promise<void> {
    return new Promise((resolve, reject) => {
      this.signalingSocket = new WebSocket(this.config.signalingServerUrl)

      this.signalingSocket.onopen = () => {
        console.log('✅ WebSocket已打开, readyState:', this.signalingSocket?.readyState)
        // 确保WebSocket完全打开后再发送认证消息
        setTimeout(() => {
          this.authenticate()
        }, 100)
        resolve()
      }

      this.signalingSocket.onclose = () => {
        this.onDisconnected?.()
      }

      this.signalingSocket.onerror = (error) => {
        reject(new Error('Signaling server connection failed'))
      }

      this.signalingSocket.onmessage = (event) => {
        try {
          const rawMessage = JSON.parse(event.data)
          // 只显示offer消息来调试offer问题
          if (rawMessage.type === 'offer') {
            console.log('📩 收到OFFER原始数据:', event.data)
            console.log('🔍 OFFER解析后:', rawMessage.type, rawMessage)
          } else if (rawMessage.type && !['analysis_result', 'ice_candidate'].includes(rawMessage.type)) {
            console.log('📩 原始数据:', event.data)
            console.log('🔍 解析后:', rawMessage.type, rawMessage)
          }
          this.handleSignalingMessage(rawMessage)
        } catch (error) {
          console.error('❌ 解析WebSocket消息失败:', error, event.data)
        }
      }

      // 超时处理
      setTimeout(() => {
        if (this.signalingSocket?.readyState !== WebSocket.OPEN) {
          reject(new Error('Signaling server connection timeout'))
        }
      }, 10000)
    })
  }

  private authenticate(): void {
    const authMessage: SignalingMessage = {
      type: 'auth',
      data: {
        client_type: 'web_client',
        client_id: `web_${Date.now()}`
      },
      timestamp: Date.now()
    }

    const authJson = JSON.stringify(authMessage)
    console.log('🔐 发送认证消息:', authJson)
    console.log('📡 WebSocket状态:', this.signalingSocket?.readyState)
    this.signalingSocket?.send(authJson)
    console.log('✅ 认证消息已发送')
  }

  private createPeerConnection(): void {
    const configuration: RTCConfiguration = {
      iceServers: this.config.stunServers.length > 0 ?
        [
          ...this.config.stunServers.map(url => ({ urls: url })),
          ...(this.config.turnServers || [])
        ] : []  // 如果没有配置STUN，使用空数组（仅本地连接）
    }

    this.peerConnection = new RTCPeerConnection(configuration)

    // 处理数据通道
    this.peerConnection.ondatachannel = (event) => {
      console.log('📡 收到数据通道:', event.channel.label)
      this.dataChannel = event.channel

      this.dataChannel.binaryType = 'arraybuffer'

      this.dataChannel.onopen = () => {
        console.log('✅ 数据通道已打开')
      }

      this.dataChannel.onclose = () => {
        console.log('❌ 数据通道已关闭')
      }

      this.dataChannel.onerror = (error) => {
        console.error('❌ 数据通道错误:', error)
      }

      this.dataChannel.onmessage = (event) => {
        this.handleDataChannelMessage(event.data)
      }
    }

    this.peerConnection.ontrack = (event) => {
      console.log('📺 收到远程track事件:', {
        track: event.track,
        kind: event.track.kind,
        streams: event.streams.length,
        readyState: event.track.readyState,
        enabled: event.track.enabled
      })

      const [stream] = event.streams
      if (stream) {
        console.log('🎬 媒体流信息:', {
          id: stream.id,
          active: stream.active,
          tracks: stream.getTracks().map(t => ({
            kind: t.kind,
            enabled: t.enabled,
            readyState: t.readyState,
            label: t.label
          }))
        })
      }

      if (stream) {
        // 保存流以便稍后设置
        this.pendingStream = stream

        if (this.remoteVideo) {
          console.log('📹 设置视频元素的srcObject')
          console.log('📹 视频元素状态:', {
            currentSrc: this.remoteVideo.currentSrc,
            readyState: this.remoteVideo.readyState,
            paused: this.remoteVideo.paused,
            muted: this.remoteVideo.muted,
            autoplay: this.remoteVideo.autoplay
          })
          // 先清理旧的流，避免播放冲突
          if (this.remoteVideo.srcObject) {
            const oldStream = this.remoteVideo.srcObject as MediaStream
            oldStream.getTracks().forEach(track => track.stop())
            this.remoteVideo.srcObject = null
          }

          // 强制设置属性确保能播放
          this.remoteVideo.muted = true
          this.remoteVideo.autoplay = true
          this.remoteVideo.playsInline = true

          // 直接设置srcObject，不等待requestAnimationFrame
          this.remoteVideo.srcObject = stream

          // 使用更快的canplay事件代替loadedmetadata
          const playVideo = () => {
            console.log('📹 视频准备就绪，开始播放')
            this.remoteVideo.play().then(() => {
              console.log('▶️ 视频播放成功')
            }).catch((err) => {
              console.error('❌ 视频播放失败:', err)
              // 尝试静音播放
              this.remoteVideo.muted = true
              this.remoteVideo.play().catch(e => {
                console.error('❌ 静音播放也失败:', e)
              })
            })
          }

          // 监听canplay事件以更快开始播放
          this.remoteVideo.oncanplay = playVideo

          // 如果readyState已经>=HAVE_FUTURE_DATA，立即播放
          if (this.remoteVideo.readyState >= 3) {
            playVideo()
          }
        } else {
          console.warn('⚠️ 视频元素未设置，流已保存等待设置')
        }

        this.onVideoStream?.(stream)
      }
    }

    // 处理ICE候选
    this.peerConnection.onicecandidate = (event) => {
      if (event.candidate) {
        const message: SignalingMessage = {
          type: 'ice_candidate',
          data: {
            candidate: event.candidate.candidate,
            sdpMid: event.candidate.sdpMid,
            sdpMLineIndex: event.candidate.sdpMLineIndex
          },
          timestamp: Date.now()
        }
        this.signalingSocket?.send(JSON.stringify(message))
      }
    }

    this.peerConnection.onconnectionstatechange = () => {
      const state = this.peerConnection?.connectionState
      const iceState = this.peerConnection?.iceConnectionState
      console.log('🔗 WebRTC连接状态变化:', state, 'ICE状态:', iceState)

      if (state === 'connected') {
        console.log('🎉 WebRTC连接已建立！')
      } else if (state === 'disconnected' || state === 'failed') {
        console.log('❌ WebRTC连接断开:', state)
        this.onDisconnected?.()
      }
    }

    // 添加ICE连接状态监听
    this.peerConnection.oniceconnectionstatechange = () => {
      const iceState = this.peerConnection?.iceConnectionState
      console.log('🧊 ICE连接状态变化:', iceState)

      if (iceState === 'connected' || iceState === 'completed') {
        console.log('✅ ICE连接成功建立')

        // 连接建立后，获取统计信息
        setTimeout(async () => {
          const stats = await this.peerConnection?.getStats()
          if (stats) {
            stats.forEach(report => {
              if (report.type === 'inbound-rtp' && report.kind === 'video') {
                console.log('📊 视频接收统计:', {
                  bytesReceived: report.bytesReceived,
                  packetsReceived: report.packetsReceived,
                  framesDecoded: report.framesDecoded,
                  frameWidth: report.frameWidth,
                  frameHeight: report.frameHeight,
                  framesPerSecond: report.framesPerSecond
                })
              }
            })
          }
        }, 2000)
      } else if (iceState === 'failed') {
        console.log('❌ ICE连接失败')
        this.onError?.('ICE connection failed')
      }
    }
  }

  private async handleSignalingMessage(message: SignalingMessage): Promise<void> {
    try {
      // 只记录重要的信令消息
      if (['offer', 'answer', 'auth_success', 'ice_candidate'].includes(message.type)) {
        console.log('📨 收到信令消息:', message.type, message)
      }

      switch (message.type) {
        case 'welcome':
          console.log('🎉 收到欢迎消息，准备认证')
          break

        case 'auth_success':
          console.log('✅ 认证成功，客户端ID:', message.client_id)
          this.clientId = message.client_id || null
          this.onConnected?.()
          break

        case 'auth_error':
          console.error('❌ 认证失败:', message.message)
          this.onError?.(`认证失败: ${message.message}`)
          break

        case 'offer':
          console.log('📤 收到WebRTC Offer')
          await this.handleOffer(message.data)
          break

        case 'ice_candidate':
          console.log('🧊 收到ICE候选')
          if (this.peerConnection?.remoteDescription) {
            await this.handleIceCandidate(message.data)
          } else {
            console.log('📦 缓存ICE候选 (等待远程描述)')
            this.pendingIceCandidates.push(message.data)
          }
          break

        case 'analysis_result':
          // 静默处理分析结果，不输出日志
          break

        default:
          console.log('❓ 未知消息类型:', message.type)
      }
    } catch (error) {
      console.error('❌ 信令消息处理错误:', error)
      this.onError?.(error instanceof Error ? error.message : 'Signaling error')
    }
  }

  private async handleOffer(offerData: any): Promise<void> {
    console.log('🚀 收到offer，开始处理...', offerData)
    if (!this.peerConnection) {
      console.error('❌ PeerConnection未初始化')
      throw new Error('PeerConnection not initialized')
    }

    try {
      const offer = new RTCSessionDescription({
        type: 'offer',
        sdp: offerData.sdp
      })
      console.log('📝 设置远程描述...')
      await this.peerConnection.setRemoteDescription(offer)
      console.log('✅ 远程描述设置成功')

      // 创建answer (接收视频)
      console.log('🔄 创建answer...')
      const answer = await this.peerConnection.createAnswer({
        offerToReceiveVideo: true,
        offerToReceiveAudio: false
      })

      // 修复inactive为recvonly，以接收视频
      if (answer.sdp) {
        answer.sdp = answer.sdp.replace(/a=inactive/g, 'a=recvonly')
        console.log('🔧 修复SDP: inactive -> recvonly')
      }
      console.log('📋 修复后的Answer SDP:', answer.sdp)
      console.log('📝 设置本地描述...')
      await this.peerConnection.setLocalDescription(answer)
      console.log('✅ answer创建并设置成功')

      // 发送answer
      const answerMessage: SignalingMessage = {
        type: 'answer',
        data: {
          type: 'answer',
          sdp: answer.sdp
        },
        timestamp: Date.now()
      }
      console.log('📤 发送answer到后端...', answerMessage.type)
      console.log('📄 Answer SDP内容:', answer.sdp)
      this.signalingSocket?.send(JSON.stringify(answerMessage))
      console.log('✅ answer消息已发送')

      // 处理缓存的ICE候选
      console.log(`🔄 处理 ${this.pendingIceCandidates.length} 个缓存的ICE候选`)
      for (const candidateData of this.pendingIceCandidates) {
        try {
          await this.handleIceCandidate(candidateData)
        } catch (error) {
          console.error('❌ 处理缓存ICE候选失败:', error)
        }
      }
      this.pendingIceCandidates = []
    } catch (error) {
      console.error('❌ 处理offer失败:', error)
      throw error
    }
  }

  private async handleIceCandidate(candidateData: any): Promise<void> {
    if (!this.peerConnection) {
      return
    }

    try {
      // 检查ICE候选数据
      if (!candidateData.candidate || candidateData.candidate.trim() === '') {
        console.log('🔕 跳过空ICE候选')
        return
      }

      const candidate = new RTCIceCandidate({
        candidate: candidateData.candidate,
        sdpMid: candidateData.sdpMid,
        sdpMLineIndex: candidateData.sdpMLineIndex
      })

      console.log('➕ 添加ICE候选:', candidateData.candidate)
      await this.peerConnection.addIceCandidate(candidate)
      console.log('✅ ICE候选添加成功')
    } catch (error) {
      console.warn('⚠️ ICE候选添加失败（可忽略）:', error)
      // ICE候选失败通常不会影响连接建立，只是记录警告
    }
  }

  // 获取连接统计信息
  async getStats(): Promise<RTCStatsReport | null> {
    if (!this.peerConnection) {
      return null
    }
    return await this.peerConnection.getStats()
  }

  // 获取连接状态
  getConnectionState(): string {
    return this.peerConnection?.connectionState || 'disconnected'
  }

  // 处理数据通道消息（JPEG数据）
  private handleDataChannelMessage(data: ArrayBuffer): void {
    // 如果不在接收帧中，检查是否是新帧的开始（前4字节是大小）
    if (!this.frameReceiving) {
      if (data.byteLength >= 4) {
        const dataView = new DataView(data)

        // 读取帧大小（大端序）
        this.currentFrameSize = (dataView.getUint8(0) << 24) |
                               (dataView.getUint8(1) << 16) |
                               (dataView.getUint8(2) << 8) |
                               dataView.getUint8(3)

        console.log('📦 新JPEG帧开始，预期大小:', this.currentFrameSize, 'bytes')

        // 开始接收新帧
        this.frameReceiving = true
        this.jpegBuffer = []

        // 保存除了头部4字节外的数据
        if (data.byteLength > 4) {
          const jpegData = data.slice(4)
          this.jpegBuffer.push(jpegData)
          console.log('📦 首块数据大小:', jpegData.byteLength)

          // 检查是否已完整接收
          this.checkFrameComplete()
        }
      } else {
        console.warn('⚠️ 数据太小，无法读取帧头:', data.byteLength)
      }
    } else {
      // 继续接收帧数据
      this.jpegBuffer.push(data)
      console.log('📦 接收数据块，大小:', data.byteLength)
      this.checkFrameComplete()
    }
  }

  // 检查JPEG帧是否完整
  private checkFrameComplete(): void {
    // 计算已接收的总大小
    let receivedSize = 0
    for (const buffer of this.jpegBuffer) {
      receivedSize += buffer.byteLength
    }

    console.log(`📊 进度: ${receivedSize}/${this.currentFrameSize} bytes (${((receivedSize/this.currentFrameSize)*100).toFixed(1)}%)`)

    // 如果接收完整
    if (receivedSize >= this.currentFrameSize) {
      console.log('✅ JPEG帧接收完成，开始合并数据')

      // 合并所有缓冲区
      const fullBuffer = new ArrayBuffer(this.currentFrameSize)
      const uint8View = new Uint8Array(fullBuffer)

      let offset = 0
      for (const buffer of this.jpegBuffer) {
        const bufferView = new Uint8Array(buffer)
        const copyLength = Math.min(buffer.byteLength, this.currentFrameSize - offset)
        uint8View.set(bufferView.slice(0, copyLength), offset)
        offset += copyLength

        if (offset >= this.currentFrameSize) break
      }

      // 验证JPEG文件头
      if (uint8View[0] === 0xFF && uint8View[1] === 0xD8) {
        console.log('✅ JPEG文件头验证通过')

        // 触发JPEG帧回调
        if (this.onJpegFrame) {
          this.onJpegFrame(fullBuffer)
        }
      } else {
        console.error('❌ JPEG文件头验证失败:',
          Array.from(uint8View.slice(0, 8)).map(b => '0x' + b.toString(16).padStart(2, '0')).join(' '))
      }

      // 重置状态，准备接收下一帧
      this.frameReceiving = false
      this.currentFrameSize = 0
      this.jpegBuffer = []

      // 如果有剩余数据，可能是下一帧的开始
      if (receivedSize > this.currentFrameSize) {
        console.log('📦 处理剩余数据，大小:', receivedSize - this.currentFrameSize)
        // 重新计算剩余数据的正确偏移
        let processedSize = 0
        let remainingBuffer: ArrayBuffer | null = null

        for (const buffer of this.jpegBuffer) {
          if (processedSize + buffer.byteLength > this.currentFrameSize) {
            const overflowStart = this.currentFrameSize - processedSize
            remainingBuffer = buffer.slice(overflowStart)
            break
          }
          processedSize += buffer.byteLength
        }

        if (remainingBuffer && remainingBuffer.byteLength > 0) {
          this.handleDataChannelMessage(remainingBuffer)
        }
      }
    }
  }
}