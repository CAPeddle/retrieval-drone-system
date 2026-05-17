#include "include/tracking_pipeline.hpp"

namespace tracking {

cv::Rect Detector::detect(const cv::Mat& frame) const {
    if (frame.empty()) {
        return {};
    }

    const int width = frame.cols / 6;
    const int height = frame.rows / 6;
    const int x = (frame.cols - width) / 2;
    const int y = (frame.rows - height) / 2;
    return cv::Rect(x, y, width, height);
}

}  // namespace tracking
