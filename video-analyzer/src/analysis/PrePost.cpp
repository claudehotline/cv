#include "analysis/PrePost.h"

#include <algorithm>
#include <cmath>
#include <iostream>

cv::Mat LetterboxPreprocessor::apply(const cv::Mat& bgr, int in_w, int in_h, LetterboxInfo& info) {
    const int src_w = bgr.cols, src_h = bgr.rows;
    const float r = std::min(static_cast<float>(in_w) / src_w, static_cast<float>(in_h) / src_h);
    const int new_w = static_cast<int>(std::round(src_w * r));
    const int new_h = static_cast<int>(std::round(src_h * r));
    info.pad_w = (in_w - new_w) / 2;
    info.pad_h = (in_h - new_h) / 2;
    info.scale = r; info.in_w = in_w; info.in_h = in_h;

    cv::Mat resized; cv::resize(bgr, resized, cv::Size(new_w, new_h));
    cv::Mat canvas(in_h, in_w, bgr.type(), cv::Scalar(114,114,114));
    resized.copyTo(canvas(cv::Rect(info.pad_w, info.pad_h, new_w, new_h)));

    cv::Mat rgb; cv::cvtColor(canvas, rgb, cv::COLOR_BGR2RGB);
    cv::Mat f32; rgb.convertTo(f32, CV_32F, 1.0f/255.0f);
    std::vector<cv::Mat> ch; cv::split(f32, ch);
    cv::Mat chw(1, 3*in_w*in_h, CV_32F);
    float* dst = chw.ptr<float>();
    for (int c=0;c<3;++c) { std::memcpy(dst + c*in_w*in_h, ch[c].ptr<float>(), in_w*in_h*sizeof(float)); }
    return chw;
}

std::vector<DetectionResult> YoloDetectPost::run(const float* data,
                                                 const std::vector<int64_t>& dims,
                                                 const LetterboxInfo& info,
                                                 const cv::Size& original_size,
                                                 const std::vector<std::string>& class_names,
                                                 float confThresh,
                                                 float iouThresh) {
    std::vector<DetectionResult> detections;
    if (dims.size() < 3 || data == nullptr) return detections;

    int64_t N = 0, C = 0; bool det_major = false;
    if (dims.size() >= 3) {
        const int64_t d1 = dims[1];
        const int64_t d2 = dims[2];
        if (d1 <= d2) {
            C = d1; N = d2; det_major = false; // [1, C, N]
        } else {
            C = d2; N = d1; det_major = true;  // [1, N, C]
        }
    }
    if (C < 6) return detections; // cx,cy,w,h,obj,cls...

    auto getVal = [&](int i, int a) -> float {
        if (det_major) return data[i * C + a]; // [N,C]
        else return data[a * N + i];           // [C,N]
    };

    const int expected_classes = static_cast<int>(class_names.size());
    const int class_offset = 4; // YOLOv12 输出为 [cx, cy, w, h, class scores...]
    const int num_classes = std::max(0, static_cast<int>(C) - class_offset);
    if (num_classes <= 0) return detections;

    std::vector<cv::Rect2f> boxes_f; std::vector<float> scores; std::vector<int> class_ids;
    std::vector<cv::Rect> boxes_int;
    boxes_f.reserve(static_cast<size_t>(N));
    boxes_int.reserve(static_cast<size_t>(N));
    scores.reserve(static_cast<size_t>(N)); class_ids.reserve(static_cast<size_t>(N));

    auto sigmoid = [](float x) -> float { return 1.0f / (1.0f + std::exp(-x)); };

    static int debug_count = 0;
    if (debug_count < 5) {
        std::cout << "[Post] dims=";
        for (auto d : dims) std::cout << d << " ";
        std::cout << " confThr=" << confThresh << " iouThr=" << iouThresh
                  << " numClasses=" << num_classes
                  << " expectedClasses=" << expected_classes << std::endl;
    }

    int detections_before_filter = 0;

    for (int i=0; i<static_cast<int>(N); ++i) {
        int best_id = -1;
        float best = 0.0f;
        for (int c=0; c<num_classes; ++c) {
            float s = sigmoid(getVal(i, class_offset + c));
            if (s > best) { best = s; best_id = c; }
        }
        float conf = best;
        if (best_id < 0 || conf < confThresh) continue;
        ++detections_before_filter;

        float cx = getVal(i, 0), cy = getVal(i, 1), w = std::max(getVal(i, 2), 0.0f), h = std::max(getVal(i, 3), 0.0f);
        if (cx <= 1.5f && cy <= 1.5f && w <= 1.5f && h <= 1.5f) {
            cx *= static_cast<float>(info.in_w);
            cy *= static_cast<float>(info.in_h);
            w  *= static_cast<float>(info.in_w);
            h  *= static_cast<float>(info.in_h);
        }

        float x0_in = (cx - 0.5f*w) - static_cast<float>(info.pad_w);
        float y0_in = (cy - 0.5f*h) - static_cast<float>(info.pad_h);
        float x1_in = (cx + 0.5f*w) - static_cast<float>(info.pad_w);
        float y1_in = (cy + 0.5f*h) - static_cast<float>(info.pad_h);
        float x0 = x0_in / info.scale, y0 = y0_in / info.scale, x1 = x1_in / info.scale, y1 = y1_in / info.scale;
        float bw = std::max(0.0f, x1 - x0), bh = std::max(0.0f, y1 - y0);

        x0 = std::clamp(x0, 0.0f, static_cast<float>(original_size.width - 1));
        y0 = std::clamp(y0, 0.0f, static_cast<float>(original_size.height - 1));
        bw = std::clamp(bw, 0.0f, static_cast<float>(original_size.width) - x0);
        bh = std::clamp(bh, 0.0f, static_cast<float>(original_size.height) - y0);

        boxes_f.emplace_back(cv::Rect2f(x0, y0, bw, bh));
        scores.push_back(conf); class_ids.push_back(best_id);
    }

    if (boxes_f.empty()) return detections;

    for (const auto& r : boxes_f) {
        cv::Rect ri;
        ri.x = static_cast<int>(std::round(r.x));
        ri.y = static_cast<int>(std::round(r.y));
        ri.width = static_cast<int>(std::round(r.width));
        ri.height = static_cast<int>(std::round(r.height));
        boxes_int.emplace_back(ri);
    }

    std::vector<int> keep;
    cv::dnn::NMSBoxes(boxes_int, scores, confThresh, iouThresh, keep);
    for (int idx : keep) {
        DetectionResult d; d.bbox = boxes_int[idx]; d.confidence = scores[idx]; d.class_id = class_ids[idx];
        if (d.class_id >= 0 && d.class_id < static_cast<int>(class_names.size())) d.class_name = class_names[d.class_id]; else d.class_name = "object";
        detections.push_back(std::move(d));
    }

    if (debug_count < 5) {
        std::cout << "[Post] candidates=" << detections_before_filter
                  << " kept=" << detections.size() << std::endl;
        if (!detections.empty()) {
            const auto& d = detections.front();
            std::cout << "[Post] first bbox: (" << d.bbox.x << ", " << d.bbox.y
                      << ", " << d.bbox.width << ", " << d.bbox.height << ") conf="
                      << d.confidence << " class=" << d.class_name << std::endl;
        }
        ++debug_count;
    }
    return detections;
}
