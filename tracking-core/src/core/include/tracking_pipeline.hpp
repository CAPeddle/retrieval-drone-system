#pragma once

#include <opencv2/core.hpp>

namespace tracking {

class Detector {
public:
    cv::Rect detect(const cv::Mat& frame) const;
};

class Tracker {
public:
    cv::Rect update(const cv::Rect& detection);

private:
    cv::Rect last_box_{};
};

}  // namespace tracking
