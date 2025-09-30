#!/usr/bin/env python3
import websocket
import json
import time

def on_message(ws, message):
    print(f"📩 收到消息: {message}")
    try:
        msg = json.loads(message)

        if msg.get('type') == 'auth_success':
            print("✅ 认证成功！")

            # 发送request_offer
            request_offer_msg = {
                'type': 'request_offer',
                'data': {
                    'source_id': 'camera_01'
                },
                'timestamp': int(time.time() * 1000)
            }

            print(f"📤 发送request_offer消息: {json.dumps(request_offer_msg)}")
            ws.send(json.dumps(request_offer_msg))

        elif msg.get('type') == 'offer':
            print("✅ 收到Offer！")
            print(f"SDP长度: {len(msg.get('data', {}).get('sdp', ''))}")
            ws.close()

    except Exception as e:
        print(f"❌ 解析消息失败: {e}")

def on_error(ws, error):
    print(f"❌ WebSocket错误: {error}")

def on_close(ws, close_status_code, close_msg):
    print("🔌 WebSocket连接已关闭")

def on_open(ws):
    print("✅ WebSocket连接已打开")

    # 发送认证消息
    auth_msg = {
        'type': 'auth',
        'data': {
            'client_type': 'web_client',
            'client_id': f'test_{int(time.time() * 1000)}'
        },
        'timestamp': int(time.time() * 1000)
    }

    print(f"📤 发送认证消息: {json.dumps(auth_msg)}")
    ws.send(json.dumps(auth_msg))

if __name__ == "__main__":
    ws = websocket.WebSocketApp("ws://localhost:8083",
                              on_open=on_open,
                              on_message=on_message,
                              on_error=on_error,
                              on_close=on_close)

    ws.run_forever()