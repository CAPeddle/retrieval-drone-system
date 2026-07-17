#include "camera_source.hpp"

#include "logging.hpp"

namespace tracking {

CameraSource::CameraSource(const CameraConfig& config) {
    // V4L2 first (the Pi CSI path); default backend as the dev-machine
    // fallback (TRK-006 integration note).
    if (!capture_.open(config.device_id, cv::CAP_V4L2)) {
        capture_.open(config.device_id, cv::CAP_ANY);
    }
    if (!capture_.isOpened()) {
        return;  // caller checks is_open()
    }
    capture_.set(cv::CAP_PROP_FRAME_WIDTH, config.width);
    capture_.set(cv::CAP_PROP_FRAME_HEIGHT, config.height);
    capture_.set(cv::CAP_PROP_FPS, config.target_fps);
    // Manual exposure: V4L2 semantics are 1 = manual mode, then the absolute
    // exposure value. Some backends ignore these — log the outcome so a
    // calibration-time exposure drift is diagnosable (R-05).
    if (!capture_.set(cv::CAP_PROP_AUTO_EXPOSURE, 1) ||
        !capture_.set(cv::CAP_PROP_EXPOSURE, static_cast<double>(config.exposure_us))) {
        LOG_WARN("camera: backend ignored manual-exposure setup (auto_exposure=1, "
                 "exposure={} us) — exposure may drift from calibration",
                 config.exposure_us);
    }
}

}  // namespace tracking
