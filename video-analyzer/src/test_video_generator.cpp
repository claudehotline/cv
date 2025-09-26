#include "VideoAnalyzer.h"
#include <opencv2/opencv.hpp>
#include <thread>
#include <chrono>

// 测试视频生成器 - 生成彩色动画帧
class TestVideoGenerator {
public:
    static void generateTestFrames(VideoAnalyzer* analyzer, const std::string& source_id) {
        cv::Mat frame(480, 640, CV_8UC3);
        int frame_count = 0;

        while (analyzer->isRunning()) {
            // 创建渐变背景
            for (int y = 0; y < frame.rows; y++) {
                for (int x = 0; x < frame.cols; x++) {
                    frame.at<cv::Vec3b>(y, x) = cv::Vec3b(
                        (frame_count + x) % 255,      // B
                        (frame_count + y) % 255,      // G
                        (frame_count * 2) % 255        // R
                    );
                }
            }

            // 添加移动的圆形
            int circle_x = (frame_count * 5) % frame.cols;
            int circle_y = frame.rows / 2 + 50 * sin(frame_count * 0.1);
            cv::circle(frame, cv::Point(circle_x, circle_y), 30, cv::Scalar(255, 255, 255), -1);

            // 添加文字
            std::string text = "Test Frame #" + std::to_string(frame_count);
            cv::putText(frame, text, cv::Point(10, 30),
                       cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(255, 255, 255), 2);

            // 添加时间戳
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);
            std::string time_str = std::ctime(&time_t);
            time_str.pop_back(); // 移除换行符
            cv::putText(frame, time_str, cv::Point(10, 60),
                       cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(255, 255, 255), 1);

            // 推送帧到WebRTC
            analyzer->pushFrameToWebRTC(frame, source_id);

            // 同时处理分析（可选）
            analyzer->processFrame(frame, source_id, AnalysisType::OBJECT_DETECTION, frame_count);

            frame_count++;

            // 控制帧率 (30 FPS)
            std::this_thread::sleep_for(std::chrono::milliseconds(33));
        }
    }

    // 生成静态测试图像
    static cv::Mat generateTestImage() {
        cv::Mat image(480, 640, CV_8UC3);

        // 创建彩色条纹
        for (int x = 0; x < image.cols; x++) {
            cv::Scalar color;
            int section = (x * 7) / image.cols;
            switch (section) {
                case 0: color = cv::Scalar(255, 255, 255); break; // 白
                case 1: color = cv::Scalar(0, 255, 255); break;   // 黄
                case 2: color = cv::Scalar(255, 255, 0); break;   // 青
                case 3: color = cv::Scalar(0, 255, 0); break;     // 绿
                case 4: color = cv::Scalar(255, 0, 255); break;   // 品红
                case 5: color = cv::Scalar(0, 0, 255); break;     // 红
                case 6: color = cv::Scalar(255, 0, 0); break;     // 蓝
            }
            cv::line(image, cv::Point(x, 0), cv::Point(x, image.rows), color, 1);
        }

        // 添加测试文字
        cv::putText(image, "WebRTC Test Pattern", cv::Point(150, 240),
                   cv::FONT_HERSHEY_SIMPLEX, 1.5, cv::Scalar(0, 0, 0), 3);

        return image;
    }
};