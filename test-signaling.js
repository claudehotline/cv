// 简单的WebSocket测试脚本
const WebSocket = require('ws');

const ws = new WebSocket('ws://localhost:8083');

ws.on('open', function open() {
    console.log('✅ WebSocket连接已打开');

    // 发送认证消息
    const authMsg = {
        type: 'auth',
        data: {
            client_type: 'web_client',
            client_id: `test_${Date.now()}`
        },
        timestamp: Date.now()
    };

    console.log('📤 发送认证消息:', JSON.stringify(authMsg));
    ws.send(JSON.stringify(authMsg));
});

ws.on('message', function incoming(data) {
    console.log('📩 收到消息:', data.toString());

    try {
        const msg = JSON.parse(data.toString());

        if (msg.type === 'auth_success') {
            console.log('✅ 认证成功！');

            // 发送request_offer
            const requestOfferMsg = {
                type: 'request_offer',
                data: {
                    source_id: 'camera_01'
                },
                timestamp: Date.now()
            };

            console.log('📤 发送request_offer消息:', JSON.stringify(requestOfferMsg));
            ws.send(JSON.stringify(requestOfferMsg));
        } else if (msg.type === 'offer') {
            console.log('✅ 收到Offer！');
            console.log('SDP长度:', msg.data?.sdp?.length || 0);
            process.exit(0);
        }
    } catch (e) {
        console.error('解析消息失败:', e);
    }
});

ws.on('error', function error(err) {
    console.error('❌ WebSocket错误:', err.message);
    process.exit(1);
});

ws.on('close', function close() {
    console.log('🔌 WebSocket连接已关闭');
});

// 30秒超时
setTimeout(() => {
    console.log('⏱️ 测试超时');
    ws.close();
    process.exit(1);
}, 30000);