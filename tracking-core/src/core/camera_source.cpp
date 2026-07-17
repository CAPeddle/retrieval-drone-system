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
    // The ring buffer's slots are pre-sized to config dims; a backend that
    // silently negotiates a different resolution would make every push a
    // rejected frame (or, unguarded, a hot-path realloc racing the consumer).
    // Fail startup instead. Read-back of 0 means the backend defers
    // negotiation to the first grab — tolerated, the buffer's dims guard is
    // the runtime backstop.
    const int actual_width = static_cast<int>(capture_.get(cv::CAP_PROP_FRAME_WIDTH));
    const int actual_height = static_cast<int>(capture_.get(cv::CAP_PROP_FRAME_HEIGHT));
    if ((actual_width != 0 && actual_width != config.width) ||
        (actual_height != 0 && actual_height != config.height)) {
        LOG_ERROR("camera: backend negotiated {}x{} instead of the configured {}x{} — "
                  "refusing to start (slots are pre-sized to config)",
                  actual_width, actual_height, config.width, config.height);
        capture_.release();
        return;
    }
    // Manual exposure: V4L2 semantics are 1 = manual mode, then the absolute
    // exposure value. Some backends ignore these — log the outcome so a
    // calibration-time exposure drift is diagnosable (R-05). On-target
    // verification item: V4L2's exposure_absolute control is in 100 us units,
    // so this raw pass-through may apply 100x the configured value — confirm
    // with v4l2-ctl on the Pi before trusting calibration exposure.
    if (!capture_.set(cv::CAP_PROP_AUTO_EXPOSURE, 1) ||
        !capture_.set(cv::CAP_PROP_EXPOSURE, static_cast<double>(config.exposure_us))) {
        LOG_WARN("camera: backend ignored manual-exposure setup (auto_exposure=1, "
                 "exposure={} us) — exposure may drift from calibration",
                 config.exposure_us);
    }
}

}  // namespace tracking
