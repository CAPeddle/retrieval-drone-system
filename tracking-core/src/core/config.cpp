#include "config.hpp"

#include <yaml-cpp/yaml.h>

#include <cmath>
#include <initializer_list>
#include <string>

namespace tracking {

ConfigError::ConfigError(const std::string& message)
    : std::runtime_error(message) {}

namespace {

// Returns the required map `section` under `root`, throwing ConfigError if it is
// absent or is not a map.
YAML::Node require_section(const YAML::Node& root, const char* section) {
    const YAML::Node node = root[section];
    if (!node || !node.IsMap()) {
        throw ConfigError(std::string("missing required section: ") + section);
    }
    return node;
}

// Returns the required scalar `section.key`, throwing ConfigError with the
// dotted path on absence or on a type that cannot convert to T.
template <typename T>
T require(const YAML::Node& section_node, const char* section, const char* key) {
    const YAML::Node node = section_node[key];
    if (!node || node.IsNull()) {
        throw ConfigError(std::string("missing required field: ") + section + "." + key);
    }
    try {
        return node.as<T>();
    } catch (const YAML::BadConversion&) {
        throw ConfigError(std::string("type mismatch for field: ") + section + "." + key);
    }
}

// Rejects non-finite (NaN/inf) doubles. A NaN threshold silently disables the
// ADR-007 predicate, because every comparison against NaN is false.
void require_finite(double value, const char* field) {
    if (!std::isfinite(value)) {
        throw ConfigError(std::string("field must be finite: ") + field);
    }
}

void require_gt(double value, double bound, const char* field) {
    require_finite(value, field);
    if (value <= bound) {
        throw ConfigError(std::string("field must be > ") + std::to_string(bound) + ": " + field);
    }
}

void require_in(double value, double lo, double hi, const char* field) {
    require_finite(value, field);
    if (value < lo || value > hi) {
        throw ConfigError(std::string("field out of range [") + std::to_string(lo) + ", " +
                          std::to_string(hi) + "]: " + field);
    }
}

void require_nonempty(const std::string& value, const char* field) {
    if (value.empty()) {
        throw ConfigError(std::string("field must not be empty: ") + field);
    }
}

// Rejects a string outside `allowed`, listing the allowed values in the error so
// the operator can fix the config without reading source.
void require_one_of(const std::string& value,
                    std::initializer_list<const char*> allowed, const char* field) {
    for (const char* candidate : allowed) {
        if (value == candidate) {
            return;
        }
    }
    std::string message = std::string("invalid value '") + value + "' for " + field +
                          " (allowed:";
    for (const char* candidate : allowed) {
        message += std::string(" ") + candidate;
    }
    message += ")";
    throw ConfigError(message);
}

}  // namespace

Config Config::load(const std::string& path) {
    YAML::Node root;
    try {
        root = YAML::LoadFile(path);
    } catch (const YAML::Exception& e) {
        throw ConfigError(std::string("failed to load config '") + path + "': " + e.what());
    }
    if (!root.IsMap()) {
        throw ConfigError(std::string("config root is not a mapping: ") + path);
    }

    Config cfg;
    // Defence in depth: any yaml-cpp error inside the field walk (e.g. a
    // non-map node reached via operator[]) becomes an actionable ConfigError
    // rather than an uncaught exception that aborts the process.
    try {
        const YAML::Node camera = require_section(root, "camera");
        cfg.camera.device_id = require<int>(camera, "camera", "device_id");
        cfg.camera.target_fps = require<int>(camera, "camera", "target_fps");
        cfg.camera.width = require<int>(camera, "camera", "width");
        cfg.camera.height = require<int>(camera, "camera", "height");
        cfg.camera.exposure_us = require<int>(camera, "camera", "exposure_us");

        const YAML::Node laser = require_section(root, "laser");
        cfg.laser.modulation_frequency_hz =
            require<double>(laser, "laser", "modulation_frequency_hz");
        cfg.laser.modulation_duty_cycle =
            require<double>(laser, "laser", "modulation_duty_cycle");

        const YAML::Node sfc = require_section(root, "safe_for_control");
        cfg.safe_for_control.age_max_ms =
            require<double>(sfc, "safe_for_control", "age_max_ms");
        cfg.safe_for_control.laser_settled_speed_m_per_s =
            require<double>(sfc, "safe_for_control", "laser_settled_speed_m_per_s");
        cfg.safe_for_control.alignment_tolerance_m =
            require<double>(sfc, "safe_for_control", "alignment_tolerance_m");

        const YAML::Node ball = require_section(root, "ball");
        cfg.ball.radius_m = require<double>(ball, "ball", "radius_m");
        cfg.ball.expected_radius_px_min = require<int>(ball, "ball", "expected_radius_px_min");
        cfg.ball.expected_radius_px_max = require<int>(ball, "ball", "expected_radius_px_max");
        cfg.ball.min_circularity = require<double>(ball, "ball", "min_circularity");
        cfg.ball.detection_blur_kernel = require<int>(ball, "ball", "detection_blur_kernel");
        cfg.ball.brightness_threshold = require<int>(ball, "ball", "brightness_threshold");

        const YAML::Node zmq = require_section(root, "zmq");
        cfg.zmq.bind_address = require<std::string>(zmq, "zmq", "bind_address");

        const YAML::Node track = require_section(root, "track");
        cfg.track.confirm_threshold = require<int>(track, "track", "confirm_threshold");
        cfg.track.predict_timeout_ms = require<double>(track, "track", "predict_timeout_ms");
        cfg.track.occlude_timeout_ms = require<double>(track, "track", "occlude_timeout_ms");
        cfg.track.retire_timeout_ms = require<double>(track, "track", "retire_timeout_ms");
        cfg.track.max_predict_duration_ms =
            require<double>(track, "track", "max_predict_duration_ms");

        const YAML::Node gating = require_section(root, "gating");
        cfg.gating.max_distance_px = require<double>(gating, "gating", "max_distance_px");

        const YAML::Node coordinate = require_section(root, "coordinate");
        cfg.coordinate.pixel_uncertainty_stddev_px =
            require<double>(coordinate, "coordinate", "pixel_uncertainty_stddev_px");
        cfg.coordinate.condition_number_max =
            require<double>(coordinate, "coordinate", "condition_number_max");
        cfg.coordinate.floor_aoi_x_min_m =
            require<double>(coordinate, "coordinate", "floor_aoi_x_min_m");
        cfg.coordinate.floor_aoi_x_max_m =
            require<double>(coordinate, "coordinate", "floor_aoi_x_max_m");
        cfg.coordinate.floor_aoi_y_min_m =
            require<double>(coordinate, "coordinate", "floor_aoi_y_min_m");
        cfg.coordinate.floor_aoi_y_max_m =
            require<double>(coordinate, "coordinate", "floor_aoi_y_max_m");

        const YAML::Node calibration = require_section(root, "calibration");
        cfg.calibration.intrinsics_path =
            require<std::string>(calibration, "calibration", "intrinsics_path");
        cfg.calibration.extrinsics_path =
            require<std::string>(calibration, "calibration", "extrinsics_path");
        cfg.calibration.aruco_dictionary =
            require<std::string>(calibration, "calibration", "aruco_dictionary");
        // The generic require<T> handles sequences via yaml-cpp's vector convert.
        cfg.calibration.marker_ids =
            require<std::vector<int>>(calibration, "calibration", "marker_ids");
        const YAML::Node charuco = require_section(calibration, "charuco");
        cfg.calibration.charuco.squares_x = require<int>(charuco, "calibration.charuco", "squares_x");
        cfg.calibration.charuco.squares_y = require<int>(charuco, "calibration.charuco", "squares_y");
        cfg.calibration.charuco.square_length_m =
            require<double>(charuco, "calibration.charuco", "square_length_m");
        cfg.calibration.charuco.marker_length_m =
            require<double>(charuco, "calibration.charuco", "marker_length_m");

        const YAML::Node pipeline = require_section(root, "pipeline");
        cfg.pipeline.ring_buffer_capacity =
            require<int>(pipeline, "pipeline", "ring_buffer_capacity");
        cfg.pipeline.capture_cpu_core =
            require<int>(pipeline, "pipeline", "capture_cpu_core");
        cfg.pipeline.capture_thread_priority =
            require<int>(pipeline, "pipeline", "capture_thread_priority");

        const YAML::Node fqa = require_section(root, "frame_quality");
        cfg.frame_quality.underexposed_threshold =
            require<double>(fqa, "frame_quality", "underexposed_threshold");
        cfg.frame_quality.overexposed_threshold =
            require<double>(fqa, "frame_quality", "overexposed_threshold");
        cfg.frame_quality.blur_threshold =
            require<double>(fqa, "frame_quality", "blur_threshold");

        const YAML::Node logging = require_section(root, "logging");
        cfg.logging.level = require<std::string>(logging, "logging", "level");
        cfg.logging.output_dir = require<std::string>(logging, "logging", "output_dir");
        cfg.logging.max_file_size_mb = require<int>(logging, "logging", "max_file_size_mb");
    } catch (const ConfigError&) {
        throw;  // already actionable
    } catch (const YAML::Exception& e) {
        throw ConfigError(std::string("invalid config '") + path + "': " + e.what());
    }

    // Semantic validation. Presence + type is not enough: nonsensical values
    // (negative, out-of-range, NaN/inf) would otherwise flow into the
    // ADR-005/ADR-007 pipeline and fail far from the cause.
    if (cfg.camera.device_id < 0) {
        throw ConfigError("field must be >= 0: camera.device_id");
    }
    if (cfg.camera.target_fps <= 0) {
        throw ConfigError("field must be > 0: camera.target_fps");
    }
    if (cfg.camera.width <= 0) {
        throw ConfigError("field must be > 0: camera.width");
    }
    if (cfg.camera.height <= 0) {
        throw ConfigError("field must be > 0: camera.height");
    }
    // Slots hold full pre-allocated frames (~1 MB each at 640x480 BGR); an
    // unbounded capacity would commit large amounts of Pi RAM at startup.
    if (cfg.pipeline.ring_buffer_capacity <= 0 || cfg.pipeline.ring_buffer_capacity > 64) {
        throw ConfigError("field must be in [1, 64]: pipeline.ring_buffer_capacity");
    }
    if (cfg.camera.exposure_us <= 0) {
        throw ConfigError("field must be > 0: camera.exposure_us");
    }
    if (cfg.pipeline.capture_cpu_core < 0 || cfg.pipeline.capture_cpu_core > 63) {
        throw ConfigError("field must be in [0, 63]: pipeline.capture_cpu_core");
    }
    // SCHED_FIFO valid priority range on Linux.
    if (cfg.pipeline.capture_thread_priority < 1 || cfg.pipeline.capture_thread_priority > 99) {
        throw ConfigError("field must be in [1, 99]: pipeline.capture_thread_priority");
    }
    require_in(cfg.frame_quality.underexposed_threshold, 0.0, 255.0,
               "frame_quality.underexposed_threshold");
    require_in(cfg.frame_quality.overexposed_threshold, 0.0, 255.0,
               "frame_quality.overexposed_threshold");
    if (cfg.frame_quality.overexposed_threshold <= cfg.frame_quality.underexposed_threshold) {
        throw ConfigError(
            "frame_quality.overexposed_threshold must be > frame_quality.underexposed_threshold");
    }
    require_gt(cfg.frame_quality.blur_threshold, 0.0, "frame_quality.blur_threshold");
    require_gt(cfg.laser.modulation_frequency_hz, 0.0, "laser.modulation_frequency_hz");
    require_in(cfg.laser.modulation_duty_cycle, 0.0, 1.0, "laser.modulation_duty_cycle");
    require_gt(cfg.safe_for_control.age_max_ms, 0.0, "safe_for_control.age_max_ms");
    require_finite(cfg.safe_for_control.laser_settled_speed_m_per_s,
                   "safe_for_control.laser_settled_speed_m_per_s");
    if (cfg.safe_for_control.laser_settled_speed_m_per_s < 0.0) {
        throw ConfigError("field must be >= 0: safe_for_control.laser_settled_speed_m_per_s");
    }
    require_gt(cfg.safe_for_control.alignment_tolerance_m, 0.0,
              "safe_for_control.alignment_tolerance_m");
    require_gt(cfg.ball.radius_m, 0.0, "ball.radius_m");
    // TRK-010 ball detector gates. All defaults are provisional (guessed,
    // pending real-footage validation) — see the yaml provenance comments.
    if (cfg.ball.expected_radius_px_min <= 0) {
        throw ConfigError("field must be > 0: ball.expected_radius_px_min");
    }
    if (cfg.ball.expected_radius_px_max <= cfg.ball.expected_radius_px_min) {
        throw ConfigError(
            "ball.expected_radius_px_max must be > ball.expected_radius_px_min");
    }
    require_in(cfg.ball.min_circularity, 0.0, 1.0, "ball.min_circularity");
    if (cfg.ball.min_circularity <= 0.0) {
        throw ConfigError("field must be > 0: ball.min_circularity");
    }
    // Odd kernel required by cv::GaussianBlur; bounded to keep denoising sane.
    if (cfg.ball.detection_blur_kernel < 3 || cfg.ball.detection_blur_kernel > 15 ||
        cfg.ball.detection_blur_kernel % 2 == 0) {
        throw ConfigError("field must be an odd int in [3, 15]: ball.detection_blur_kernel");
    }
    if (cfg.ball.brightness_threshold < 1 || cfg.ball.brightness_threshold > 254) {
        throw ConfigError("field must be in [1, 254]: ball.brightness_threshold");
    }
    // TRK-014 track lifecycle. Timeouts feed the ADR-007 clause 3/4 inputs via
    // track state, so every value gets the finite check.
    if (cfg.track.confirm_threshold < 1) {
        throw ConfigError("field must be >= 1: track.confirm_threshold");
    }
    require_gt(cfg.track.predict_timeout_ms, 0.0, "track.predict_timeout_ms");
    require_gt(cfg.track.occlude_timeout_ms, 0.0, "track.occlude_timeout_ms");
    require_gt(cfg.track.retire_timeout_ms, 0.0, "track.retire_timeout_ms");
    require_gt(cfg.track.max_predict_duration_ms, 0.0, "track.max_predict_duration_ms");
    // The decay chain only makes sense strictly ordered (Predicted before
    // Occluded before Lost); equal values would collapse states.
    if (cfg.track.occlude_timeout_ms <= cfg.track.predict_timeout_ms) {
        throw ConfigError("track.occlude_timeout_ms must be > track.predict_timeout_ms");
    }
    if (cfg.track.retire_timeout_ms <= cfg.track.occlude_timeout_ms) {
        throw ConfigError("track.retire_timeout_ms must be > track.occlude_timeout_ms");
    }
    require_gt(cfg.gating.max_distance_px, 0.0, "gating.max_distance_px");
    // TRK-017..019 coordinate mapping. AOI bounds feed the degenerate-mapping
    // rejection, so NaN here would silently disable it (same rationale as the
    // ADR-007 finite checks).
    require_gt(cfg.coordinate.pixel_uncertainty_stddev_px, 0.0,
               "coordinate.pixel_uncertainty_stddev_px");
    require_gt(cfg.coordinate.condition_number_max, 1.0, "coordinate.condition_number_max");
    require_finite(cfg.coordinate.floor_aoi_x_min_m, "coordinate.floor_aoi_x_min_m");
    require_finite(cfg.coordinate.floor_aoi_x_max_m, "coordinate.floor_aoi_x_max_m");
    require_finite(cfg.coordinate.floor_aoi_y_min_m, "coordinate.floor_aoi_y_min_m");
    require_finite(cfg.coordinate.floor_aoi_y_max_m, "coordinate.floor_aoi_y_max_m");
    if (cfg.coordinate.floor_aoi_x_max_m <= cfg.coordinate.floor_aoi_x_min_m) {
        throw ConfigError(
            "coordinate.floor_aoi_x_max_m must be > coordinate.floor_aoi_x_min_m");
    }
    if (cfg.coordinate.floor_aoi_y_max_m <= cfg.coordinate.floor_aoi_y_min_m) {
        throw ConfigError(
            "coordinate.floor_aoi_y_max_m must be > coordinate.floor_aoi_y_min_m");
    }
    require_nonempty(cfg.zmq.bind_address, "zmq.bind_address");
    require_nonempty(cfg.calibration.intrinsics_path, "calibration.intrinsics_path");
    require_nonempty(cfg.calibration.extrinsics_path, "calibration.extrinsics_path");
    // TRK-011 marker detector.
    require_one_of(cfg.calibration.aruco_dictionary, {"4X4_50", "4X4_100", "5X5_50"},
                   "calibration.aruco_dictionary");
    // ADR-004 Phase 2 health monitoring needs at least one static marker.
    if (cfg.calibration.marker_ids.empty()) {
        throw ConfigError("field must be a non-empty int list: calibration.marker_ids");
    }
    for (const int id : cfg.calibration.marker_ids) {
        if (id < 0) {
            throw ConfigError("field must contain only non-negative ids: calibration.marker_ids");
        }
    }
    // Odd square counts per KTD-4 (future-proofing against legacy Charuco
    // patterns and rows-vs-columns axis confusion; all our OpenCV versions
    // already agree on the post-4.6.0 pattern).
    if (cfg.calibration.charuco.squares_x < 3 || cfg.calibration.charuco.squares_x % 2 == 0) {
        throw ConfigError("field must be an odd int >= 3: calibration.charuco.squares_x");
    }
    if (cfg.calibration.charuco.squares_y < 3 || cfg.calibration.charuco.squares_y % 2 == 0) {
        throw ConfigError("field must be an odd int >= 3: calibration.charuco.squares_y");
    }
    require_gt(cfg.calibration.charuco.square_length_m, 0.0, "calibration.charuco.square_length_m");
    require_gt(cfg.calibration.charuco.marker_length_m, 0.0, "calibration.charuco.marker_length_m");
    if (cfg.calibration.charuco.marker_length_m >= cfg.calibration.charuco.square_length_m) {
        throw ConfigError(
            "calibration.charuco.marker_length_m must be < calibration.charuco.square_length_m");
    }
    require_one_of(cfg.logging.level,
                   {"trace", "debug", "info", "warn", "error", "critical"},
                   "logging.level");
    require_nonempty(cfg.logging.output_dir, "logging.output_dir");
    if (cfg.logging.max_file_size_mb <= 0) {
        throw ConfigError("field must be > 0: logging.max_file_size_mb");
    }
    // The sink keeps the active file plus 3 rotated files on RAM-backed tmpfs;
    // an unbounded size could commit several GB of the Pi's memory.
    if (cfg.logging.max_file_size_mb > 256) {
        throw ConfigError("field must be <= 256: logging.max_file_size_mb");
    }

    return cfg;
}

}  // namespace tracking
