#include "tracking_pipeline.hpp"

namespace tracking {

cv::Rect Tracker::update(const cv::Rect& detection) {
    if (detection.area() > 0) {
        last_box_ = detection;
    }
    return last_box_;
}

}  // namespace tracking
