#pragma once

#include <opencv2/core.hpp>

namespace tracking {

// The stub Detector was replaced by BallDetector (TRK-010, ball_detector.hpp).
// The Tracker stub remains until TRK-014 supplies the real track lifecycle.
class Tracker {
public:
    cv::Rect update(const cv::Rect& detection);

private:
    cv::Rect last_box_{};
};

}  // namespace tracking
