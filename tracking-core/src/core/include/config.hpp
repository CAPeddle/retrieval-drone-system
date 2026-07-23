#pragma once

#include <stdexcept>
#include <string>
#include <vector>

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
    // TRK-009 modulation detector (ADR-005, KTD-8). All PROVISIONAL pending the
    // operator's modulated-laser recording session (R14). The correlation window
    // length is NOT a field — it derives as 2 * camera.target_fps /
    // modulation_frequency_hz (two modulation periods, KTD-1) and is validated
    // integral and >= 8 at load.
    double psd_power_min = 0.0;    // absolute modulation-bin power floor, |X2|^2 units
    double psd_purity_min = 0.0;   // two-sided spectral purity floor; structural > 0.4 (KTD-2)
    int min_cluster_size_px = 0;   // connected-component area gate, lower bound
    int max_cluster_size_px = 0;   // connected-component area gate, upper bound
    int grace_period_cycles = 0;   // >= 2; detection starts at max(cycles*fpc, window) pushes
};

struct SafeForControlConfig {
    double age_max_ms = 0.0;
    double laser_settled_speed_m_per_s = 0.0;
    double alignment_tolerance_m = 0.0;
    double min_unsafe_dwell_ms = 0.0;  // ADR-007 hysteresis dwell (TRK-020c).
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

struct CharucoConfig {
    int squares_x = 0;             // Board columns (odd per KTD-4 future-proofing).
    int squares_y = 0;             // Board rows (odd).
    double square_length_m = 0.0;  // Physical square edge length.
    double marker_length_m = 0.0;  // Physical marker edge length (< square).
};

struct CalibrationConfig {
    std::string intrinsics_path;
    std::string extrinsics_path;
    std::string aruco_dictionary;      // TRK-011: predefined dictionary name.
    std::vector<int> marker_ids;       // Expected static health-monitoring marker IDs (ADR-004 Phase 2).
    CharucoConfig charuco;
};

struct CoordinateConfig {
    double pixel_uncertainty_stddev_px = 0.0;  // TRK-019 input uncertainty.
    double condition_number_max = 0.0;         // Homography degeneracy gate (ADR-006).
    // Floor area of interest in FloorPlane2D metres: mapped positions outside
    // are rejected as degenerate (plan R8). Also bounds the horizon-safety
    // denominator check at artifact load (plan R5).
    double floor_aoi_x_min_m = 0.0;
    double floor_aoi_x_max_m = 0.0;
    double floor_aoi_y_min_m = 0.0;
    double floor_aoi_y_max_m = 0.0;
};

struct GatingConfig {
    double max_distance_px = 0.0;  // Association gate radius in image pixels (TRK-015).
};

struct TrackConfig {
    int confirm_threshold = 0;            // Observations required for Provisional -> Confirmed (TRK-014).
    double predict_timeout_ms = 0.0;      // Predicted -> Occluded age threshold.
    double occlude_timeout_ms = 0.0;      // Occluded -> Lost age threshold.
    double retire_timeout_ms = 0.0;       // Lost -> Retired age threshold.
    double max_predict_duration_ms = 0.0; // Constant-velocity extrapolation cap (TRK-016).
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
    TrackConfig track;
    GatingConfig gating;
    CoordinateConfig coordinate;
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
