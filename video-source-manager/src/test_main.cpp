#include <opencv2/opencv.hpp>
#include <json/json.h>
#include <iostream>
#include <string>

int main() {
    std::cout << "Video Source Manager - Test Version" << std::endl;
    std::cout << "OpenCV Version: " << CV_VERSION << std::endl;

    // Test OpenCV
    cv::VideoCapture cap(0);
    if (cap.isOpened()) {
        std::cout << "Camera test: SUCCESS" << std::endl;
        cap.release();
    } else {
        std::cout << "Camera test: FAILED (no camera found)" << std::endl;
    }

    // Test JSON
    Json::Value test_json;
    test_json["test"] = "success";
    test_json["version"] = 1;
    std::cout << "JSON test: " << test_json["test"].asString() << std::endl;

    std::cout << "All tests completed!" << std::endl;
    return 0;
}