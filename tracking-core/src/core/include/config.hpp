#pragma once

#include <stdexcept>
#include <string>

namespace tracking {

// Thrown by Config::load on a missing required field or a type mismatch.
class ConfigError : public std::runtime_error {
public:
    explicit ConfigError(const std::string& message);
};

struct CameraConfig {
    int device_id = 0;
    int target_fps = 0;
    int width = 0;
    int height = 0;
    int exposure_us = 0;  // Manual exposure locked at calibration time (ADR-004).
};

struct LaserConfig {
    double modulation_frequency_hz = 0.0;
    double modulation_duty_cycle = 0.0;
};

struct SafeForControlConfig {
    double age_max_ms = 0.0;
    double laser_settled_speed_m_per_s = 0.0;
    double alignment_tolerance_m = 0.0;
};

struct BallConfig {
    double radius_m = 0.0;                // Physical radius (ADR-010 Z compensation).
    int expected_radius_px_min = 0;       // TRK-010 size gate, lower bound.
    int expected_radius_px_max = 0;       // TRK-010 size gate, upper bound.
    double min_circularity = 0.0;         // 4*pi*area/perimeter^2 must exceed this.
    int detection_blur_kernel = 0;        // Odd Gaussian kernel size for denoising.
    int brightness_threshold = 0;         // Fixed prior: pixels above this are ball-candidate (NoIR).
};

struct ZmqConfig {
    std::string bind_address;
};

struct CalibrationConfig {
    std::string intrinsics_path;
    std::string extrinsics_path;
};

struct PipelineConfig {
    int ring_buffer_capacity = 0;      // Frame slots pre-allocated between capture and processing.
    int capture_cpu_core = 0;          // Core the ingestion thread is pinned to (§7.1 affinity).
    int capture_thread_priority = 0;   // SCHED_FIFO priority for the ingestion thread.
};

struct FrameQualityConfig {
    double underexposed_threshold = 0.0;  // Histogram mean below this -> REJECT.
    double overexposed_threshold = 0.0;   // Histogram mean above this -> REJECT.
    double blur_threshold = 0.0;          // Laplacian variance below this -> REJECT.
};

struct LoggingConfig {
    std::string level;        // One of: trace, debug, info, warn, error, critical.
    std::string output_dir;   // Must be tmpfs on the Pi — never the SD card (§7.1).
    int max_file_size_mb = 0; // Rotating-sink file size limit.
};

// Runtime configuration, loaded once at startup from tracking_core.yaml. Treated
// as read-only after Config::load returns — pass by const reference. (Members are
// public and non-const, so immutability is a convention, not a type guarantee.)
// Not hot-reloaded in v0.3.
struct Config {
    CameraConfig camera;
    LaserConfig laser;
    SafeForControlConfig safe_for_control;
    BallConfig ball;
    ZmqConfig zmq;
    CalibrationConfig calibration;
    LoggingConfig logging;
    PipelineConfig pipeline;
    FrameQualityConfig frame_quality;

    // Loads and validates the YAML at `path`. Throws ConfigError on a missing
    // required field or a type mismatch. Startup-only — may throw (config and
    // startup code are exempt from the hot-path no-exceptions rule).
    static Config load(const std::string& path);
};

}  // namespace tracking
