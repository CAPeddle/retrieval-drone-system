#pragma once

// TRK-006: FrameSource backed by the physical camera via OpenCV VideoCapture.
// Prefers the V4L2 backend (Pi 5 CSI path) and falls back to OpenCV's default
// backend on dev machines without V4L2. Manual exposure is locked at open
// (auto-exposure disabled) so calibration-time exposure holds (ADR-004).

#include "capture_thread.hpp"
#include "config.hpp"

#include <opencv2/videoio.hpp>

namespace tracking {

class CameraSource final : public FrameSource {
public:
    // Opens and configures the device; startup-only (may allocate/log).
    explicit CameraSource(const CameraConfig& config);

    bool is_open() const { return capture_.isOpened(); }
    bool grab(cv::Mat& out) override { return capture_.read(out); }

private:
    cv::VideoCapture capture_;
};

}  // namespace tracking
