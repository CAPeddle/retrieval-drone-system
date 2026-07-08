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
    double radius_m = 0.0;
};

struct ZmqConfig {
    std::string bind_address;
};

struct CalibrationConfig {
    std::string intrinsics_path;
    std::string extrinsics_path;
};

// Runtime configuration, loaded once at startup from tracking_core.yaml and
// immutable thereafter (pass by const reference; not hot-reloaded in v0.3).
struct Config {
    CameraConfig camera;
    LaserConfig laser;
    SafeForControlConfig safe_for_control;
    BallConfig ball;
    ZmqConfig zmq;
    CalibrationConfig calibration;

    // Loads and validates the YAML at `path`. Throws ConfigError on a missing
    // required field or a type mismatch. Startup-only — may throw (config and
    // startup code are exempt from the hot-path no-exceptions rule).
    static Config load(const std::string& path);
};

}  // namespace tracking
